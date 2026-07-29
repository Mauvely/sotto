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
        set(CPACK_GENERATOR "WIX")
        set(CPACK_WIX_UPGRADE_GUID "${MP_UPGRADE_GUID}")
        set(CPACK_WIX_PRODUCT_ICON "${MP_ICON}")
        # One Start Menu shortcut, launching the installed exe directly.
        set(CPACK_PACKAGE_EXECUTABLES "${MP_TARGET}" "${MP_DISPLAY_NAME}")
        set(CPACK_CREATE_DESKTOP_LINKS "${MP_TARGET}")
    endif()

    include(CPack)
endmacro()
