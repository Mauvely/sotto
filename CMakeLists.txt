cmake_minimum_required(VERSION 3.24)

project(sotto
    VERSION 0.1.0
    DESCRIPTION "Fully local voice dictation, powered by Whisper"
    HOMEPAGE_URL "https://mauvely.com"
    LANGUAGES C CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# ---------------------------------------------------------------------------
# Options
# ---------------------------------------------------------------------------
# GPU backend for whisper.cpp/ggml. One of: cpu | hip | cuda | vulkan
#   hip    -> AMD ROCm (rocm-hip-sdk on Arch)
#   vulkan -> any Vulkan 1.2+ GPU (often the easiest path on AMD)
#   cuda   -> NVIDIA CUDA toolkit
set(SOTTO_GPU "cpu" CACHE STRING "whisper.cpp GPU backend: cpu|hip|cuda|vulkan")
set_property(CACHE SOTTO_GPU PROPERTY STRINGS cpu hip cuda vulkan)

option(SOTTO_LAYER_SHELL "Use LayerShellQt for the Wayland overlay (auto-disabled if not found)" ON)
option(SOTTO_BUILD_TESTS "Build unit tests" ON)

# Compiles out the self-updater for a Microsoft Store build. Sotto has no
# updater yet, so this only defines a macro today — but the release workflow
# builds the MSI with it OFF and the MSIX with it ON, and an option that does
# not exist would make CMake warn about an unused -D on every Windows release.
option(SOTTO_STORE_BUILD "Build for the Microsoft Store (no self-update)" OFF)

# ---------------------------------------------------------------------------
# Qt
# ---------------------------------------------------------------------------
find_package(Qt6 6.4 REQUIRED COMPONENTS
    Core Gui Widgets Qml Quick QuickControls2 Multimedia Network
)
if(NOT WIN32)
    # D-Bus carries the portals (hotkey, injection), notifications and the
    # single-instance IPC; Windows uses native equivalents instead.
    find_package(Qt6 6.4 REQUIRED COMPONENTS DBus)
endif()
qt_standard_project_setup()

# bin/, like every sibling. Sotto dropped its binary at the top of the build
# tree, which is the one thing the shared packaging scripts would otherwise have
# to guess about. They cope either way now, but there is no reason for this app
# to be the exception.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin/Release")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/bin/Debug")

# ---------------------------------------------------------------------------
# LayerShellQt (optional; present on Arch as `layer-shell-qt`)
# ---------------------------------------------------------------------------
set(SOTTO_HAVE_LAYER_SHELL OFF)
if(SOTTO_LAYER_SHELL AND NOT WIN32)
    find_package(LayerShellQt QUIET)
    if(LayerShellQt_FOUND)
        set(SOTTO_HAVE_LAYER_SHELL ON)
        message(STATUS "LayerShellQt found: overlay will use wlr-layer-shell")
    else()
        message(WARNING "LayerShellQt (Qt6) not found: overlay falls back to a plain "
                        "always-on-top window. Install `layer-shell-qt` for proper "
                        "Wayland overlay behaviour.")
    endif()
endif()

# ---------------------------------------------------------------------------
# KWindowSystem (optional): real blur-behind on KWin when the HUD is
# translucent. Present on Arch as `kwindowsystem`.
# ---------------------------------------------------------------------------
# NOT WIN32, like LayerShellQt above. KF6WindowSystem does build on Windows, so
# on a machine that happened to have it the blur path would compile and link —
# and then ask KWin, which is not there, for an effect Windows spells Mica. The
# guard states the platform rather than relying on the package being absent.
set(SOTTO_HAVE_KWINDOWSYSTEM OFF)
find_package(KF6WindowSystem QUIET)
if(KF6WindowSystem_FOUND AND NOT WIN32)
    set(SOTTO_HAVE_KWINDOWSYSTEM ON)
    message(STATUS "KF6WindowSystem found: translucent HUD can request KWin blur-behind")
endif()

# ---------------------------------------------------------------------------
# whisper.cpp (pinned; fetched at configure time)
# ---------------------------------------------------------------------------
include(FetchContent)

set(WHISPER_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(WHISPER_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
set(WHISPER_BUILD_SERVER   OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS      OFF CACHE BOOL "" FORCE)

# These are CACHE variables, so a prior configure with a different
# SOTTO_GPU value leaves its backend switched on unless we explicitly turn
# everything off here before turning the selected one on.
set(GGML_HIP    OFF CACHE BOOL "" FORCE)
set(GGML_CUDA   OFF CACHE BOOL "" FORCE)
set(GGML_VULKAN OFF CACHE BOOL "" FORCE)

if(SOTTO_GPU STREQUAL "hip")
    set(GGML_HIP ON CACHE BOOL "" FORCE)
elseif(SOTTO_GPU STREQUAL "cuda")
    set(GGML_CUDA ON CACHE BOOL "" FORCE)
elseif(SOTTO_GPU STREQUAL "vulkan")
    set(GGML_VULKAN ON CACHE BOOL "" FORCE)
elseif(NOT SOTTO_GPU STREQUAL "cpu")
    message(FATAL_ERROR "Unknown SOTTO_GPU value '${SOTTO_GPU}' (use cpu|hip|cuda|vulkan)")
endif()

# ---------------------------------------------------------------------------
# Why that backend — a sentence for the settings page
# ---------------------------------------------------------------------------
# "backend: cpu" on a machine with a discrete GPU reads as a *runtime* failure
# to find the card. It never is: the backend is chosen here, at configure time,
# and an artifact built against one will not start without its runtime. So the
# build says which toolchain it looked for and what it would take to change the
# answer, and the settings page prints it verbatim (App::gpuBackendReason).
#
# ASCII only, and no semicolons: this ends up inside a -D on an MSVC command
# line, and a `;` in an unquoted CMake expansion is a list separator — it split
# the definition in two and the compiler reported "newline in constant".
if(SOTTO_GPU STREQUAL "cpu")
    # glslc, not just the loader: ggml-vulkan compiles its own shaders, so the
    # Vulkan *SDK* is the requirement, and a machine can have a working Vulkan
    # driver (any recent GPU does) with no SDK at all.
    find_package(Vulkan QUIET COMPONENTS glslc)
    find_package(CUDAToolkit QUIET)

    set(_sotto_gpu_offer "")
    if(Vulkan_FOUND AND Vulkan_glslc_FOUND)
        list(APPEND _sotto_gpu_offer "vulkan")
    endif()
    if(CUDAToolkit_FOUND)
        list(APPEND _sotto_gpu_offer "cuda")
    endif()

    if(_sotto_gpu_offer)
        list(GET _sotto_gpu_offer 0 _sotto_gpu_first)
        string(REPLACE ";" " or " _sotto_gpu_names "${_sotto_gpu_offer}")
        set(SOTTO_GPU_REASON
            "Running on the CPU because this build was configured that way, not because no GPU was found. A ${_sotto_gpu_names} toolchain is installed on the build machine: reconfigure with -DSOTTO_GPU=${_sotto_gpu_first} and rebuild to use it.")
    else()
        set(SOTTO_GPU_REASON
            "Running on the CPU because no GPU toolchain was installed when this was built. The backend is a build-time choice and cannot be picked up at run time. For a GPU build, install the LunarG Vulkan SDK (ggml-vulkan compiles its shaders with the SDK's glslc, so a working driver alone is not enough) and reconfigure with -DSOTTO_GPU=vulkan, which works on AMD and NVIDIA alike. CUDA needs the NVIDIA CUDA Toolkit and -DSOTTO_GPU=cuda. Released builds stay on the CPU because one built against a GPU backend will not start on a machine without that runtime.")
    endif()
    unset(_sotto_gpu_offer)
else()
    set(SOTTO_GPU_REASON
        "Built with the ${SOTTO_GPU} backend. This binary needs that runtime present to start at all, which is why released builds are CPU-only.")
endif()
message(STATUS "whisper.cpp backend: ${SOTTO_GPU}")
message(STATUS "  ${SOTTO_GPU_REASON}")

FetchContent_Declare(whisper
    GIT_REPOSITORY https://github.com/ggml-org/whisper.cpp.git
    GIT_TAG        v1.9.1
    GIT_SHALLOW    TRUE
    # Windows caps a path at 260 characters unless something opts out, and
    # whisper.cpp's own tree is what runs into it: paths like
    # `examples/whisper.android.java/app/src/androidTest/java/com/litongjava/…`
    # are long before the build directory is prepended. The checkout then fails
    # with "Filename too long" and the configure dies at `FetchContent`.
    #
    # Per-clone rather than a note in the README telling somebody to run
    # `git config --system core.longpaths true`, which needs an administrator
    # and changes their machine. GitHub's runners check out at `D:\a\sotto\sotto`
    # and are short enough to have got away with it, which is exactly why this
    # was found on a developer's machine and not in CI.
    GIT_CONFIG     core.longpaths=true
)
FetchContent_MakeAvailable(whisper)

# ---------------------------------------------------------------------------
# Application
# ---------------------------------------------------------------------------
if(WIN32)
    set(SOTTO_PLATFORM_SOURCES
        src/hotkey/WinHotkey.h        src/hotkey/WinHotkey.cpp
    )
else()
    set(SOTTO_PLATFORM_SOURCES
        src/core/DBusService.h        src/core/DBusService.cpp
        src/inject/PortalRemoteDesktop.h src/inject/PortalRemoteDesktop.cpp
        src/portal/PortalRequest.h    src/portal/PortalRequest.cpp
        src/hotkey/GlobalShortcutsPortal.h src/hotkey/GlobalShortcutsPortal.cpp
    )
endif()

qt_add_executable(MauvelySotto
    src/main.cpp
    src/core/App.h                src/core/App.cpp
    src/core/apppaths.h           src/core/apppaths.cpp
    src/core/Settings.h           src/core/Settings.cpp
    src/core/SingleInstance.h     src/core/SingleInstance.cpp
    src/core/Notifier.h           src/core/Notifier.cpp
    src/audio/AudioCapture.h      src/audio/AudioCapture.cpp
    src/stt/SpeechGate.h
    src/stt/WhisperEngine.h       src/stt/WhisperEngine.cpp
    src/stt/TranscriptionSession.h src/stt/TranscriptionSession.cpp
    src/stt/ModelManager.h        src/stt/ModelManager.cpp
    src/format/TextFormatter.h    src/format/TextFormatter.cpp
    src/inject/TextInjector.h     src/inject/TextInjector.cpp
    src/ui/framelesswindow.h      src/ui/framelesswindow.cpp
    src/ui/OverlayController.h    src/ui/OverlayController.cpp
    src/ui/TrayIcon.h             src/ui/TrayIcon.cpp
    ${SOTTO_PLATFORM_SOURCES}
    resources/resources.qrc
)

# `src` is the suite's convention — every include in this repo is repo-rooted
# (`#include "ui/framelesswindow.h"`). `src/ui` is here for one reason:
# qmltyperegistrar records a QML_ELEMENT header by its **bare filename** in the
# metatypes JSON and then generates
#
#     #if __has_include(<framelesswindow.h>)
#     #  include <framelesswindow.h>
#     #endif
#     qmlRegisterTypesAndRevisions<SottoWindow>("Sotto", 1);
#
# — so when the include does not resolve, the guard silently skips it and the
# registration below fails to compile on an undeclared type. Setting
# CMAKE_AUTOMOC_PATH_PREFIX does not help: it changes moc's own #include, not
# the recorded JSON name. The directory has to be findable, so it is.
target_include_directories(MauvelySotto PRIVATE src src/ui)

# pragma Singleton in Brand.qml should be auto-detected, but set the property
# explicitly too: belt-and-braces against Qt versions/policies where the
# QML_FILES scan alone doesn't register it, which silently leaves `Brand`
# unresolved (undefined) at runtime instead of failing the build. Must be set
# *before* qt_add_qml_module, which reads source file properties as it
# processes QML_FILES.
#
# Theme.qml is the second singleton: Brand is the raw ramps, Theme is the
# semantic layer resolved for whichever theme is on.
set_source_files_properties(qml/Brand.qml qml/Theme.qml
                            PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

qt_add_qml_module(MauvelySotto
    URI Sotto
    VERSION 1.0
    RESOURCE_PREFIX /qt/qml # Qt 6.5+ default; pinned so 6.4 builds match
    QML_FILES
        qml/Brand.qml
        qml/Theme.qml
        qml/Overlay.qml
        qml/SettingsWindow.qml
        qml/NotepadWindow.qml
        qml/LogoMark.qml
        qml/VisualizerBars.qml
        qml/LocalBadge.qml
        qml/SPanel.qml
        qml/SSection.qml
        qml/SLabel.qml
        qml/STitleBar.qml
        qml/SWindowButton.qml
        qml/SResizeEdges.qml
        qml/SButton.qml
        qml/SRadio.qml
        qml/SCheckBox.qml
        qml/SSwitch.qml
        qml/SComboBox.qml
        qml/STextField.qml
        qml/SSlider.qml
)

target_link_libraries(MauvelySotto PRIVATE
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Qml Qt6::Quick Qt6::QuickControls2
    Qt6::Multimedia Qt6::Network
    whisper
)

if(WIN32)
    # dwmapi: the window's rounded corners and border colour (src/ui/framelesswindow.cpp).
    # shell32: SHAppBarMessage, which is how the same file finds an auto-hidden
    # taskbar so a maximised window can leave it a pixel to rise from.
    target_link_libraries(MauvelySotto PRIVATE user32 dwmapi shell32)
    target_compile_definitions(MauvelySotto PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
    set_target_properties(MauvelySotto PROPERTIES WIN32_EXECUTABLE TRUE)
else()
    target_link_libraries(MauvelySotto PRIVATE Qt6::DBus)
endif()

target_compile_definitions(MauvelySotto PRIVATE
    SOTTO_VERSION="${PROJECT_VERSION}"
    SOTTO_GPU_BACKEND="${SOTTO_GPU}"
    # Quoted as a whole. Unquoted, CMake splits the expansion on any `;` the
    # sentence contains and defines half a string literal.
    "SOTTO_GPU_REASON=\"${SOTTO_GPU_REASON}\""
)

if(SOTTO_HAVE_LAYER_SHELL)
    target_link_libraries(MauvelySotto PRIVATE LayerShellQt::Interface)
    target_compile_definitions(MauvelySotto PRIVATE SOTTO_HAVE_LAYER_SHELL)
endif()

if(SOTTO_HAVE_KWINDOWSYSTEM)
    target_link_libraries(MauvelySotto PRIVATE KF6::WindowSystem)
    target_compile_definitions(MauvelySotto PRIVATE SOTTO_HAVE_KWINDOWSYSTEM)
endif()

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------
if(SOTTO_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# ---------------------------------------------------------------------------
# Install
# ---------------------------------------------------------------------------
# The brand mark has two consumers: packaging/app.svg, which the shared tooling
# installs and rasterises, and resources/icons/app.svg, which the binary embeds
# for its window and tray icon. Copied at configure time rather than checked in
# twice, so they cannot drift.
configure_file(packaging/app.svg
               "${CMAKE_CURRENT_SOURCE_DIR}/resources/icons/app.svg" COPYONLY)

include(GNUInstallDirs)
install(TARGETS MauvelySotto RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
# Freedesktop integration — the .desktop file, the AppStream metainfo and the
# hicolor icons — is installed by mauvely_app_info() below. All of it derives
# from APP_ID, which is also what src/main.cpp and src/core/Settings.cpp read,
# so the strings that have to agree cannot drift apart.

# ---------------------------------------------------------------------------
# Packaging
# ---------------------------------------------------------------------------
# Linux: packaging/linux/build-appimage.sh (linuxdeploy + appimagetool), not
# CPack — see cmake/MauvelyPackaging.cmake for why.
#
# Windows: `cpack -G WIX` has a real binary to wrap — the experimental port
# (WinHotkey, SendInput injection, QLocalServer single instance, registry
# autostart; see the Portability table in docs/ARCHITECTURE.md) builds and
# runs. No .msi is published yet because the port hasn't had enough
# real-hardware time, not because the packaging is missing. It's the same
# module Compose and Snap use, both of which already ship on Windows.
if(SOTTO_STORE_BUILD)
    target_compile_definitions(MauvelySotto PRIVATE SOTTO_STORE_BUILD)
endif()

include(cmake/MauvelyAppInfo.cmake)
mauvely_app_info(
    KEY          sotto
    BINARY       MauvelySotto
    DISPLAY_NAME "Sotto"
    GENERIC_NAME "Local dictation"
    APP_ID       net.mauvely.sotto.app
    DESCRIPTION  "${PROJECT_DESCRIPTION}"
    SUMMARY      "Fully local transcription and voice dictation"
    # `Utility` is the one Main category; `Accessibility` is an Additional one,
    # so this pairing draws no warning. The original .desktop said
    # `Utility;Accessibility;Audio;` and `Audio` is a Main category that the
    # spec requires `AudioVideo` alongside — desktop-file-validate calls that an
    # error that "will be fatal in the future". Sotto is a dictation tool, not a
    # media player, so the fix is to drop it rather than add AudioVideo.
    CATEGORIES   Utility Accessibility
    # No EXEC_ARGS: main() parses only its own flags and takes no file.
    #
    # Unchanged. It must never change between releases: it is how WiX knows
    # 0.2.0 replaces 0.1.0 rather than installing beside it, and Sotto has
    # shipped installs to protect.
    UPGRADE_GUID "69E2BAFD-4552-4D1C-ADDE-9345FFC482BF"
    ICON         "${CMAKE_CURRENT_SOURCE_DIR}/resources/icons/app.ico"
    # Installs into %LOCALAPPDATA%, so an update applies with `msiexec /passive`
    # and no UAC prompt. It was absent from the call this replaces, which would
    # have made every future Windows update raise an elevation dialog.
    PER_USER
)
