# MauvelyPackaging — shared CPack config for Mauvely's Qt/C++ desktop apps
# (Sotto, Compose, Snap). Meant to be copied verbatim into a sibling app's
# cmake/ directory and included the same way; keep the three copies in sync
# by hand since there's no shared package registry between these repos yet.
#
# Covers only the piece CPack actually does well: the Windows .msi via the
# WiX generator. Linux distribution goes through packaging/linux/build-
# appimage.sh instead — linuxdeploy + appimagetool aren't CPack generators,
# and AppImage's "just an ELF with a squashfs appended" model doesn't map
# onto CPack's installer-generator abstraction cleanly.
#
# Usage, from the app's top-level CMakeLists.txt, after the executable target
# and its install(TARGETS ...) rule already exist:
#
#   include(cmake/MauvelyPackaging.cmake)
#   mauvely_configure_packaging(
#       TARGET       sotto                 # the add_executable()/qt_add_executable() target name
#       DISPLAY_NAME "Sotto"                # shown in Add/Remove Programs, the installer UI
#       DESCRIPTION  "Fully local voice dictation"
#       VERSION      ${PROJECT_VERSION}
#       UPGRADE_GUID "PUT-A-FIXED-GUID-HERE" # see below — do not regenerate per release
#       ICON         "${CMAKE_CURRENT_SOURCE_DIR}/resources/icons/sotto.ico"
#   )
#
# UPGRADE_GUID: generate ONCE per app (`uuidgen` or Python `uuid.uuid4()`) and
# hard-code it at the call site — this is how WiX recognises "installing
# version 0.2.0 should replace the existing 0.1.0", not create a second,
# parallel install. Regenerating it on every release breaks upgrades for
# everyone who already has the app installed.
#
# This is inert everywhere except an actual Windows CPack/WiX run: on Linux/
# macOS the macro is a no-op beyond setting a few harmless CPACK_* variables
# that plain `cpack` (without -G WIX) simply ignores.
#
# CI requirement: the WiX generator shells out to WiX Toolset v3's
# candle.exe/light.exe, which GitHub's windows-latest runners do not ship by
# default — install it first, e.g. `choco install wixtoolset -y`.

# Captured here, at include() time, NOT inside the macro below.
#
# A CMake macro is expanded textually into the caller's scope, so
# CMAKE_CURRENT_LIST_DIR read *inside* the macro is the directory of the
# CMakeLists.txt that called it — the repo root — not this module's directory.
# Nothing here needs it yet; it exists so that module-relative paths added later
# (Compose's copy resolves a WiX template this way) are written against the one
# variable that means what it looks like it means. Getting this wrong resolved
# to <repo>/../..., one level above the repository, and failed every Windows
# build in Compose before it was caught.
set(_MAUVELY_PACKAGING_DIR "${CMAKE_CURRENT_LIST_DIR}")

# ---------------------------------------------------------------------------
# Locate windeployqt.
# ---------------------------------------------------------------------------
# Qt6 exposes it as an imported target; every Qt installation ever has also put
# the binary next to qmake, which covers Qt5 and any layout the target is
# missing from.
function(_mauvely_find_windeployqt out_var)
    if(TARGET Qt6::windeployqt)
        get_target_property(_wdq Qt6::windeployqt IMPORTED_LOCATION)
        if(_wdq)
            set(${out_var} "${_wdq}" PARENT_SCOPE)
            return()
        endif()
    endif()

    set(_hints "")
    foreach(_qmake_target Qt6::qmake Qt5::qmake)
        if(TARGET ${_qmake_target})
            get_target_property(_qmake ${_qmake_target} IMPORTED_LOCATION)
            if(_qmake)
                get_filename_component(_dir "${_qmake}" DIRECTORY)
                list(APPEND _hints "${_dir}")
            endif()
        endif()
    endforeach()
    if(QT_QMAKE_EXECUTABLE)
        get_filename_component(_dir "${QT_QMAKE_EXECUTABLE}" DIRECTORY)
        list(APPEND _hints "${_dir}")
    endif()
    if(DEFINED ENV{QT_ROOT_DIR})
        list(APPEND _hints "$ENV{QT_ROOT_DIR}/bin")
    endif()

    find_program(MAUVELY_WINDEPLOYQT
        NAMES windeployqt6 windeployqt
        HINTS ${_hints}
        DOC "Qt's Windows deployment tool — stages the Qt runtime into the installer")
    set(${out_var} "${MAUVELY_WINDEPLOYQT}" PARENT_SCOPE)
endfunction()

# ---------------------------------------------------------------------------
# Put the Qt runtime in the INSTALLER, not just the build tree.
# ---------------------------------------------------------------------------
# install(TARGETS) copies exactly one file: the .exe. Qt's DLLs, the platform
# plugin, the WebEngine helper process and its Chromium resource paks are not
# targets and not dependencies CMake tracks, so none of them reach the staging
# directory CPack builds the .msi from. The .msi then installs perfectly and
# the app dies before main() with a loader dialog:
#
#     The code execution cannot proceed because Qt6Widgets.dll was not found.
#
# Running windeployqt over build/bin/Release does NOT fix this, which is the
# trap — the workflows did exactly that and it looks like deployment. CPack
# never reads the build tree. The tool has to run over the *staged install*
# tree, which is what the code below does: inside an install script
# CMAKE_INSTALL_PREFIX is CPack's staging root during `cpack`, and the real
# destination during a plain `cmake --install`.
#
# The install(TARGETS ...) rule for the target must already be declared when
# this is called, since install rules run in declaration order and windeployqt
# needs the .exe to be there to read its imports.
function(_mauvely_deploy_qt_runtime target)
    _mauvely_find_windeployqt(_wdq)
    if(NOT _wdq)
        message(FATAL_ERROR
            "mauvely_configure_packaging: windeployqt was not found, so the "
            ".msi would contain a lone ${target}.exe with no Qt runtime beside "
            "it — it installs fine and cannot start. Put Qt's bin directory on "
            "PATH or set MAUVELY_WINDEPLOYQT to the tool's full path.")
    endif()
    message(STATUS "${target}: staging the Qt runtime with ${_wdq}")

    if(CMAKE_INSTALL_BINDIR)
        set(_bindir "${CMAKE_INSTALL_BINDIR}")
    else()
        set(_bindir "bin")
    endif()

    # Bracket argument so ${...} survives to install time; string(CONFIGURE)
    # substitutes only the @...@ placeholders, which are configure-time values.
    set(_code [==[
set(_wdq "@_wdq@")
set(_exe "${CMAKE_INSTALL_PREFIX}/@_bindir@/@target@.exe")
if(NOT EXISTS "${_exe}")
    set(_exe "${CMAKE_INSTALL_PREFIX}/@target@.exe")
endif()
if(NOT EXISTS "${_exe}")
    message(FATAL_ERROR
        "windeployqt has nothing to work on: @target@.exe is not in the install "
        "tree under ${CMAKE_INSTALL_PREFIX}. The install(TARGETS ...) rule for "
        "it has to be declared before mauvely_configure_packaging().")
endif()

# windeployqt infers almost everything from the binary's imports; it only needs
# telling which CRT flavour to match. RelWithDebInfo links the release runtime.
if(CMAKE_INSTALL_CONFIG_NAME STREQUAL "Debug")
    set(_cfg_flag "--debug")
else()
    set(_cfg_flag "--release")
endif()

message(STATUS "Deploying the Qt runtime beside ${_exe}")
execute_process(
    COMMAND "${_wdq}" ${_cfg_flag} "${_exe}"
    RESULT_VARIABLE _wdq_status)
if(NOT _wdq_status EQUAL 0)
    message(FATAL_ERROR
        "windeployqt failed (exit ${_wdq_status}). Failing the install rather "
        "than warning: the .msi it would produce ships an executable that "
        "cannot start, and nothing downstream can detect that.")
endif()
]==])
    string(CONFIGURE "${_code}" _code @ONLY)
    install(CODE "${_code}")
endfunction()

macro(mauvely_configure_packaging)
    set(_mp_options)
    set(_mp_one_value TARGET DISPLAY_NAME DESCRIPTION VERSION UPGRADE_GUID ICON)
    set(_mp_multi_value)
    cmake_parse_arguments(MP "${_mp_options}" "${_mp_one_value}" "${_mp_multi_value}" ${ARGN})

    if(NOT MP_TARGET)
        message(FATAL_ERROR "mauvely_configure_packaging: TARGET is required")
    endif()
    if(NOT MP_UPGRADE_GUID)
        message(FATAL_ERROR
            "mauvely_configure_packaging: UPGRADE_GUID is required. Generate one "
            "ONCE with `uuidgen` and hard-code it at the call site — it must "
            "never change between releases of ${MP_TARGET}.")
    endif()

    set(CPACK_PACKAGE_NAME "${MP_DISPLAY_NAME}")
    set(CPACK_PACKAGE_VENDOR "Mauvely")
    set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "${MP_DESCRIPTION}")
    set(CPACK_PACKAGE_VERSION "${MP_VERSION}")
    set(CPACK_PACKAGE_INSTALL_DIRECTORY "${MP_DISPLAY_NAME}")
    set(CPACK_PACKAGE_HOMEPAGE_URL "https://mauvely.com")

    if(WIN32)
        # Before anything CPack-specific: without this the installer contains
        # one executable and no Qt. See the note above the function.
        _mauvely_deploy_qt_runtime("${MP_TARGET}")

        set(CPACK_GENERATOR "WIX")
        set(CPACK_WIX_UPGRADE_GUID "${MP_UPGRADE_GUID}")
        set(CPACK_WIX_PRODUCT_ICON "${MP_ICON}")
        # One Start Menu shortcut, launching the installed exe directly.
        set(CPACK_PACKAGE_EXECUTABLES "${MP_TARGET}" "${MP_DISPLAY_NAME}")
        set(CPACK_CREATE_DESKTOP_LINKS "${MP_TARGET}")
    endif()

    include(CPack)
endmacro()
