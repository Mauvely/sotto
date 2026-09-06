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
#       PER_USER                             # optional — see below
#   )
#
# UPGRADE_GUID: generate ONCE per app (`uuidgen` or Python `uuid.uuid4()`) and
# hard-code it at the call site — this is how WiX recognises "installing
# version 0.2.0 should replace the existing 0.1.0", not create a second,
# parallel install. Regenerating it on every release breaks upgrades for
# everyone who already has the app installed. Each app needs its OWN GUID:
# sharing one would make two different apps try to upgrade each other.
#
# PER_USER: install into %LOCALAPPDATA% for the current user instead of
# Program Files for the whole machine. This exists for auto-update. A
# per-machine MSI needs administrator rights, so every silent background update
# raises a UAC prompt — which is exactly the "hands off" behaviour an updater is
# supposed to deliver and doesn't. A per-user install needs no elevation, so
# `msiexec /i <new>.msi /passive` completes without ever interrupting anyone.
# The cost is that each Windows account installs its own copy.
#
# It needs two things working together, and both must be present or the install
# silently reverts to per-machine:
#   · CPACK_WIX_ROOT_FOLDER_ID, which reparents the whole directory tree from
#     ProgramFiles64Folder to LocalAppDataFolder;
#   · a custom WiX template carrying InstallScope="perUser" on <Package>, since
#     CPack exposes no variable for it.
# There is no way to check this from a Linux build. Smoke-install the .msi as a
# non-administrator once and confirm it lands in %LOCALAPPDATA% with no UAC
# prompt before trusting the update path.
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
# CMAKE_CURRENT_LIST_DIR read *inside* mauvely_configure_packaging() is the
# directory of the CMakeLists.txt that called it — the repo root — not this
# module's directory. Deriving the WiX template path that way resolved to
# <repo>/../packaging/windows/..., one level above the repository, and failed
# every Windows build. Read at file scope it is <repo>/cmake, which is what the
# relative paths below are written against.
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
# ── The MSVC runtime ─────────────────────────────────────────────────────────
#
# vcruntime140.dll and msvcp140.dll have to travel inside the package. Without
# them it installs perfectly on a clean Windows machine and the app will not
# start, with a system dialog naming a DLL — the same failure shape as a missing
# Qt DLL, and one nothing downstream can detect. A per-user MSI installs without
# elevation on purpose and installing a redistributable needs elevation, so it
# cannot bootstrap one: self-containment is the only option, and the cost is a
# few hundred kilobytes.
#
# **Not `windeployqt --compiler-runtime`.** That flag needs `VCINSTALLDIR` in the
# environment, which is set by a Visual Studio developer prompt and by nothing
# else — not by CI, not by a plain `cmake --build`. Without it windeployqt prints
#
#     Warning: Cannot find Visual Studio installation directory,
#              VCINSTALLDIR is not set.
#
# and exits 0, so the flag looks applied, the build goes green and the DLLs are
# missing. Measured, not assumed.
#
# `InstallRequiredSystemLibraries` finds the redistributable through CMake's own
# MSVC detection, which already knows where the compiler came from.
function(_mauvely_stage_msvc_runtime bindir)
    if(NOT MSVC)
        return()
    endif()
    # The app targets Windows 10 and later, where the UCRT is part of the OS.
    set(CMAKE_INSTALL_UCRT_LIBRARIES FALSE)
    set(CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS_SKIP TRUE)
    include(InstallRequiredSystemLibraries)
    if(NOT CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS)
        message(FATAL_ERROR
            "The MSVC runtime DLLs were not found, so the package would install "
            "an app that cannot start on a machine without the Visual C++ "
            "redistributable. Install the 'C++ ATL'/redistributable component "
            "of the Visual Studio build tools.")
    endif()
    install(PROGRAMS ${CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS} DESTINATION "${bindir}")
    message(STATUS "MSVC runtime: staging ${CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS}")
endfunction()

function(_mauvely_deploy_qt_runtime target)
    _mauvely_find_windeployqt(_wdq)

    # Only pass --qmldir where there is QML to find. An app with no .qml files
    # gains nothing from the scan and the flag is harmless either way, but
    # pointing it at a tree with no imports makes windeployqt's output confusing
    # to read when something else goes wrong.
    file(GLOB_RECURSE _qml_files "${CMAKE_CURRENT_SOURCE_DIR}/*.qml")
    set(_qml_source_dir "")
    if(_qml_files)
        set(_qml_source_dir "${CMAKE_CURRENT_SOURCE_DIR}")
    endif()
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
# --qmldir matters for a QML app and is inert for a Widgets one. windeployqt
# scans the *source* tree for imports to work out which QML modules to copy;
# without it a Qt Quick app installs with its Qt DLLs and none of its QML, and
# then dies at startup on "module Sotto is not installed" — an .msi that
# installs cleanly and cannot run, which is the exact failure the rest of this
# file exists to prevent. Sotto is the only Qt Quick app in the suite, so this
# would have gone unnoticed until its first Windows release.
set(_qmldir_flag "")
if(NOT "@_qml_source_dir@" STREQUAL "")
    set(_qmldir_flag "--qmldir" "@_qml_source_dir@")
endif()

execute_process(
    COMMAND "${_wdq}" ${_cfg_flag} ${_qmldir_flag} "${_exe}"
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

# ---------------------------------------------------------------------------
# And put it in the BUILD TREE too, which is a different problem.
# ---------------------------------------------------------------------------
# Everything above fixes the installer. It does nothing for the executable a
# developer just built, and `install(TARGETS)` copies one file for the same
# reason: Qt's DLLs are not targets. So `build/bin/Release/<app>.exe` sits alone
# in its directory and Windows answers a double-click with
#
#     The code execution cannot proceed because Qt6Widgets.dll was not found.
#
# — a modal dialog rather than a non-zero exit, so it interrupts whoever is at
# the keyboard instead of appearing in any log. It is worse for the tests, which
# land in the same directory: `ctest` starts each one, each one fails to load,
# and a run produces one dialog per test. That happened to a person, twice.
#
# The comment above the install rule says running windeployqt over the build
# tree "does NOT fix this", and that is true of the thing it is about — CPack
# never reads the build tree. Both are needed, for different failures.
#
# POST_BUILD rather than a separate target: it runs only when the executable is
# actually relinked, and it is the only arrangement where the answer to "why
# will it not start" is never "you forgot a step". The flags match the install
# rule's exactly, so what a developer runs is what ships.
#
# The test executables share `bin/<config>/`, so they are covered by the app's
# run. Building *only* a test target leaves them uncovered, and putting the
# command on each one would mean a windeployqt per test for a directory they all
# share — `cmake --build` builds the app, which is the ordinary case.
function(_mauvely_deploy_qt_beside_binaries target)
    _mauvely_find_windeployqt(_wdq)
    if(NOT _wdq)
        # The install path raises a FATAL_ERROR for this a moment later, with
        # the reason. Saying it twice would only bury it.
        return()
    endif()

    file(GLOB_RECURSE _qml_files "${CMAKE_CURRENT_SOURCE_DIR}/*.qml")
    set(_qmldir_flag "")
    if(_qml_files)
        # See the install rule: without this a Qt Quick app gets its DLLs and
        # none of its QML, and dies on "module ... is not installed".
        set(_qmldir_flag "--qmldir" "${CMAKE_CURRENT_SOURCE_DIR}")
    endif()

    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND "${_wdq}"
                "$<IF:$<CONFIG:Debug>,--debug,--release>"
                ${_qmldir_flag}
                "$<TARGET_FILE:${target}>"
        COMMENT "Staging the Qt runtime beside ${target}.exe so it can be run"
        VERBATIM)

    # windeployqt stages `platforms/qwindows.dll` and no other platform
    # integration, which is right for an app somebody double-clicks and wrong
    # for the way this suite renders its screenshots: every repo's CLAUDE.md
    # documents `QT_QPA_PLATFORM=offscreen`, and without the plugin beside the
    # binary that only works where Qt's own plugin directory is already
    # reachable. `_wdq` is `<qt>/bin/windeployqt.exe`, so the plugin is two
    # directories along from the tool we are already using.
    get_filename_component(_wdq_bin "${_wdq}" DIRECTORY)
    get_filename_component(_qt_prefix "${_wdq_bin}" DIRECTORY)
    set(_offscreen "${_qt_prefix}/plugins/platforms/qoffscreen.dll")
    if(EXISTS "${_offscreen}")
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                    "${_offscreen}" "$<TARGET_FILE_DIR:${target}>/platforms/"
            COMMENT "Staging the offscreen platform plugin for the harness"
            VERBATIM)
    endif()

    # windeployqt knows about Qt and nothing else. See the note on
    # MAUVELY_EXTRA_RUNTIME_DLLS below.
    foreach(_dll IN LISTS MAUVELY_EXTRA_RUNTIME_DLLS)
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                    "${_dll}" "$<TARGET_FILE_DIR:${target}>"
            COMMENT "Staging ${_dll} beside ${target}.exe"
            VERBATIM)
    endforeach()
endfunction()

# ---------------------------------------------------------------------------
# Third-party DLLs, which nothing else knows about.
# ---------------------------------------------------------------------------
# `MAUVELY_EXTRA_RUNTIME_DLLS` is a list of absolute paths a repository appends
# to before calling `mauvely_configure_packaging()`. Both the build tree and the
# installer get them.
#
# It exists because windeployqt deploys Qt, `InstallRequiredSystemLibraries`
# deploys the MSVC runtime, and between them they cover everything the suite
# needed until Cloud was enabled on Windows. `OpenSSL::Crypto` is linked
# dynamically — deliberately, so there is one OpenSSL in the process and it is
# Qt's — which puts a hard `libcrypto-*-x64.dll` import in the executable that
# neither tool has any reason to know about.
#
# Measured on Suite the day Cloud was first built here: `dumpbin /dependents`
# listed `libcrypto-4-x64.dll`, nothing staged it, and the app started only
# because OpenSSL's own `bin` happened to be on PATH. The .msi would have
# installed cleanly on any machine and died before main() — the exact failure
# every comment in this file is about, arriving through the one door none of
# them was watching.
#
# A repository adds to it beside the dependency that caused it, not here.
function(_mauvely_install_extra_runtime_dlls bindir)
    if(NOT MAUVELY_EXTRA_RUNTIME_DLLS)
        return()
    endif()
    foreach(_dll IN LISTS MAUVELY_EXTRA_RUNTIME_DLLS)
        if(NOT EXISTS "${_dll}")
            message(FATAL_ERROR
                "MAUVELY_EXTRA_RUNTIME_DLLS names ${_dll}, which does not "
                "exist. Failing here rather than shipping a package that "
                "installs cleanly and cannot start.")
        endif()
    endforeach()
    install(PROGRAMS ${MAUVELY_EXTRA_RUNTIME_DLLS} DESTINATION "${bindir}")
    message(STATUS "Extra runtime: staging ${MAUVELY_EXTRA_RUNTIME_DLLS}")
endfunction()

macro(mauvely_configure_packaging)
    set(_mp_options PER_USER)
    set(_mp_one_value TARGET DISPLAY_NAME DESCRIPTION VERSION UPGRADE_GUID ICON)
    set(_mp_multi_value)
    cmake_parse_arguments(MP "${_mp_options}" "${_mp_one_value}" "${_mp_multi_value}" ${ARGN})

    if(CMAKE_INSTALL_BINDIR)
        set(_mp_bindir "${CMAKE_INSTALL_BINDIR}")
    else()
        set(_mp_bindir "bin")
    endif()

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
        _mauvely_stage_msvc_runtime("${_mp_bindir}")
        _mauvely_install_extra_runtime_dlls("${_mp_bindir}")
        # The installer is not the only thing that has to start. See the note
        # above the function: the build tree is a separate failure, and it is
        # the one a developer meets.
        _mauvely_deploy_qt_beside_binaries("${MP_TARGET}")

        set(CPACK_GENERATOR "WIX")
        set(CPACK_WIX_UPGRADE_GUID "${MP_UPGRADE_GUID}")
        set(CPACK_WIX_PRODUCT_ICON "${MP_ICON}")
        # One Start Menu shortcut, launching the installed exe directly.
        set(CPACK_PACKAGE_EXECUTABLES "${MP_TARGET}" "${MP_DISPLAY_NAME}")
        set(CPACK_CREATE_DESKTOP_LINKS "${MP_TARGET}")

        if(MP_PER_USER)
            set(_mp_template
                "${_MAUVELY_PACKAGING_DIR}/../packaging/windows/WIX.peruser.template.in")
            if(NOT EXISTS "${_mp_template}")
                message(FATAL_ERROR
                    "mauvely_configure_packaging: PER_USER needs a WiX template at "
                    "${_mp_template}. Without it the <Package> element keeps CPack's "
                    "default per-machine scope and every auto-update prompts for UAC.")
            endif()
            set(CPACK_WIX_TEMPLATE "${_mp_template}")
            # Reparents the generated directory tree. CPack defaults this to
            # ProgramFiles64Folder, which a limited-privilege install cannot
            # write to.
            set(CPACK_WIX_ROOT_FOLDER_ID "LocalAppDataFolder")

            # Silence two ICE validations that a per-user install cannot
            # satisfy and does not need to. Without these, light.exe fails:
            #
            #   error LGHT0204 : ICE38: Component CM_CP_bin.<exe> installs to
            #     user profile. It must use a registry key under HKCU as its
            #     KeyPath, not a file.
            #   error LGHT0204 : ICE64: The directory INSTALL_ROOT is in the
            #     user profile but is not listed in the RemoveFile table.
            #
            # Both check an invariant that only applies to a PER-MACHINE
            # package that happens to drop some components into a user profile.
            # There, MSI needs an HKCU keypath to tell whose copy is whose, and
            # explicit RemoveFile rows because it will not clean another user's
            # profile on uninstall. This package is wholly per-user
            # (InstallScope="perUser"): there is one user, the file keypath
            # resolves inside that user's own LOCALAPPDATA so repair works, and
            # the directories are created by the same components that are
            # removed on uninstall.
            #
            # Suppressing rather than fixing is forced: CPack generates
            # files.wxs and directories.wxs itself, a component may only have
            # one KeyPath, and CPACK_WIX_PATCH_FILE cannot replace the file
            # keypath CPack has already written.
            #
            # NOT -sval (which disables validation wholesale) — every other ICE
            # still runs, so a genuinely malformed package still fails here.
            #
            # Grepping the CPack docs for this variable's literal name finds
            # nothing: it is documented as the templated
            # CPACK_WIX_<TOOL>_EXTRA_FLAGS, where <TOOL> is LIGHT or CANDLE.
            # `cmake --help-manual cpack-generators` has it. It is not new.
            set(CPACK_WIX_LIGHT_EXTRA_FLAGS "-sice:ICE38" "-sice:ICE64")
            message(STATUS "${MP_TARGET}: per-user MSI (%LOCALAPPDATA%, no UAC on update)")
        else()
            message(STATUS "${MP_TARGET}: per-machine MSI (Program Files, UAC on update)")
        endif()
    endif()

    include(CPack)
endmacro()
