# MauvelyAppInfo — the single place a forked app states its own identity.
#
# Every Mauvely desktop app is a fork of app-base, and the thing that goes wrong
# when you fork by copying is that the *name* is spread across a dozen files
# that nobody remembers to edit. Play is the worked example: its
# packaging/linux/build-appimage.sh still greps for `build/bin/MauvelyCompose`
# and passes `--desktop-file packaging/net.mauvely.compose.app.desktop`, a file
# that does not exist in that repo. The script cannot succeed and never could.
#
# So the name is stated ONCE, here, and everything else derives from it:
#
#   · the .desktop file, its Icon= key and its StartupWMClass
#   · QGuiApplication::setDesktopFileName(), via the APP_ID compile definition
#   · what core/autostart.cpp writes into ~/.config/autostart/
#   · the Flatpak application id and its manifest filename
#   · the AppStream metainfo id
#   · the MSIX AppxManifest identity and executable path
#   · the artifact basenames the release workflow uploads and registers
#
# Those first four are the five strings that must agree byte-for-byte or Wayland
# cannot associate a window with its launcher and falls back to a placeholder
# icon. Compose got this wrong once and paid for it. Deriving them from one
# argument is the only way to make that class of bug impossible rather than
# merely unlikely.
#
# ── Usage ────────────────────────────────────────────────────────────────────
#
#   include(cmake/MauvelyAppInfo.cmake)
#   mauvely_app_info(
#       KEY          appbase                    # release-feed key, R2 prefix
#       BINARY       MauvelyAppBase             # executable + artifact basename
#       CORE         appbase_core               # the OBJECT library, if there is one
#       DISPLAY_NAME "Mauvely App Base"
#       SHORT_NAME   "App Base"                 # without the company name
#       GENERIC_NAME "Desktop App Template"     # the .desktop GenericName=
#       EXEC_ARGS    "%f"                       # only if argv is really read
#       APP_ID       net.mauvely.appbase.app    # reverse-DNS id
#       DESCRIPTION  "${PROJECT_DESCRIPTION}"
#       CATEGORIES   Utility Development         # freedesktop Categories=, a list
#       PLATFORMS    linux windows               # what this app ships for
#       EXTRA_TARGETS core_smoke                 # only where tests are their own targets
#       SUMMARY      "One sentence for the store listing."
#       UPGRADE_GUID "………"                      # fresh per app, NEVER changes
#       ICON         "${CMAKE_CURRENT_SOURCE_DIR}/resources/icons/appbase.ico"
#       PER_USER
#   )
#
# It calls mauvely_configure_packaging() for you — do not call both.
#
# ── Why `net.` and not `com.` ────────────────────────────────────────────────
#
# Compose, Snap, Play and Sotto all use `net.mauvely.*`; Relay alone used
# `com.mauvely.*`, arguing that mauvely.com is the real domain and the others
# were wrong. That argument is fine in the abstract and lost on the facts:
# Compose and Sotto have installed bases, and changing an installed app's id
# breaks its desktop integration and its autostart entry. The cheap side of the
# rename is Relay's, which had no installed base. `net.` it is, everywhere.

set(_MAUVELY_APPINFO_DIR "${CMAKE_CURRENT_LIST_DIR}")

# The generated app-info.json is read by scripts/package.sh, scripts/package.ps1
# and both GitHub workflows. It exists so a shell script never has to re-derive
# the binary name by grepping CMakeLists.txt — the other half of the Play bug.
function(_mauvely_write_app_info)
    # Built as strings rather than with a JSON library because CMake has none,
    # and because the shape is small enough that a generator is more code than
    # the thing it generates.
    set(_platforms "")
    set(_linux_formats "")
    set(_windows_formats "")
    if("linux" IN_LIST MAUVELY_APP_PLATFORMS)
        set(_platforms "\"linux-x86_64\"")
        set(_linux_formats "\"appimage\", \"flatpak\"")
    endif()
    if("windows" IN_LIST MAUVELY_APP_PLATFORMS)
        if(_platforms)
            string(APPEND _platforms ", ")
        endif()
        string(APPEND _platforms "\"windows-x86_64\"")
        set(_windows_formats "\"msi\", \"msix\"")
    endif()

    set(_json "{
  \"key\": \"${MAUVELY_APP_KEY}\",
  \"binary\": \"${MAUVELY_APP_BINARY}\",
  \"displayName\": \"${MAUVELY_APP_DISPLAY_NAME}\",
  \"appId\": \"${MAUVELY_APP_ID}\",
  \"version\": \"${MAUVELY_APP_VERSION}\",
  \"platforms\": [${_platforms}],
  \"formats\": {
    \"linux\": [${_linux_formats}],
    \"windows\": [${_windows_formats}]
  }
}
")
    file(GENERATE OUTPUT "${CMAKE_BINARY_DIR}/app-info.json" CONTENT "${_json}")
endfunction()

macro(mauvely_app_info)
    set(_ai_options PER_USER)
    set(_ai_one_value KEY BINARY CORE DISPLAY_NAME SHORT_NAME GENERIC_NAME
                      APP_ID DESCRIPTION SUMMARY EXEC_ARGS UPGRADE_GUID ICON)
    # CATEGORIES is multi-value, and it has to be. cmake_parse_arguments reads
    # ${ARGN}, which is an unquoted expansion — so a single argument written as
    # "Utility;Development;" arrives as *two* arguments, a one-value keyword
    # keeps the first, and the rest are silently dropped into UNPARSED_ARGUMENTS.
    # That produced `Categories=Utility` in the .desktop file with no warning.
    # PLATFORMS is `linux`, `windows`, or both. It is what makes the packaging
    # scripts and both workflows refuse a format the app does not target, rather
    # than every app pretending to want all four. Snap is Windows-only: Wayland
    # has no workable story for a third-party screenshot tool, since every
    # capture raises the compositor's own source-selection prompt.
    set(_ai_multi_value CATEGORIES PLATFORMS EXTRA_TARGETS)
    cmake_parse_arguments(AI "${_ai_options}" "${_ai_one_value}"
                          "${_ai_multi_value}" ${ARGN})

    foreach(_required KEY BINARY DISPLAY_NAME APP_ID)
        if(NOT AI_${_required})
            message(FATAL_ERROR "mauvely_app_info: ${_required} is required")
        endif()
    endforeach()

    # A fork that forgets to change the id would silently collide with app-base
    # on the .desktop name, the Flatpak id and the autostart entry. Catch it at
    # configure time rather than at "why does my app steal Snap's icon".
    if(AI_APP_ID STREQUAL "net.mauvely.appbase.app" AND NOT AI_KEY STREQUAL "appbase")
        message(FATAL_ERROR
            "mauvely_app_info: APP_ID is still app-base's. Give this fork its own "
            "reverse-DNS id (net.mauvely.<key>.app) before building.")
    endif()

    if(NOT AI_PLATFORMS)
        set(AI_PLATFORMS linux windows)
    endif()
    foreach(_p IN LISTS AI_PLATFORMS)
        if(NOT _p STREQUAL "linux" AND NOT _p STREQUAL "windows")
            message(FATAL_ERROR "mauvely_app_info: unknown PLATFORMS value '${_p}' "
                                "(expected linux and/or windows)")
        endif()
    endforeach()

    if(NOT AI_CATEGORIES)
        set(AI_CATEGORIES Utility)
    endif()

    # freedesktop wants a semicolon-separated list with a trailing semicolon.
    # A CMake list already joins on ';', so only the terminator is added — and
    # it is added here rather than asked for at the call site, where forgetting
    # it produces a .desktop file that some launchers accept and others ignore.
    set(MAUVELY_APP_CATEGORIES "${AI_CATEGORIES};")
    set(MAUVELY_APP_PLATFORMS "${AI_PLATFORMS}")
    if(NOT AI_SUMMARY)
        set(AI_SUMMARY "${AI_DESCRIPTION}")
    endif()
    if(NOT AI_GENERIC_NAME)
        set(AI_GENERIC_NAME "${AI_DISPLAY_NAME}")
    endif()
    # The product's name without the company's — "Compose", not "Mauvely
    # Compose". Mirrors the `short` field `website-main/shared/apps.ts` already
    # carries, and it is what `branding::productName` builds an organisation's
    # name from.
    #
    # Stated rather than derived by stripping "Mauvely " off DISPLAY_NAME,
    # because "Sotto by Mauvely" is the entry a prefix-strip gets wrong — it
    # would produce "Sotto by Northgate", which names two companies and belongs
    # to neither.
    if(NOT AI_SHORT_NAME)
        set(AI_SHORT_NAME "${AI_DISPLAY_NAME}")
    endif()

    # `%f` and friends. Only pass this if the app really does accept a file
    # argument — Compose's .desktop claimed `%f` for a long time while its
    # main() ignored argv entirely, so a launcher would open the app and drop
    # the file on the floor.
    set(MAUVELY_APP_EXEC_ARGS "")
    if(AI_EXEC_ARGS)
        set(MAUVELY_APP_EXEC_ARGS " ${AI_EXEC_ARGS}")
    endif()

    # Anything freedesktop allows that this macro does not model — MimeType,
    # Keywords, Actions and their [Desktop Action] groups. Appended verbatim
    # from packaging/desktop-extra.ini when it exists.
    #
    # A file rather than more CMake arguments: Desktop Actions are multi-line
    # groups, and expressing them as a CMake list would mean inventing an
    # encoding for something that already has a perfectly good one.
    set(MAUVELY_APP_DESKTOP_EXTRA "")
    set(_ai_extra "${CMAKE_CURRENT_SOURCE_DIR}/packaging/desktop-extra.ini")
    if(EXISTS "${_ai_extra}")
        file(READ "${_ai_extra}" MAUVELY_APP_DESKTOP_EXTRA)
    endif()

    set(MAUVELY_APP_KEY          "${AI_KEY}")
    set(MAUVELY_APP_BINARY       "${AI_BINARY}")
    set(MAUVELY_APP_DISPLAY_NAME "${AI_DISPLAY_NAME}")
    set(MAUVELY_APP_GENERIC_NAME "${AI_GENERIC_NAME}")
    set(MAUVELY_APP_ID           "${AI_APP_ID}")
    set(MAUVELY_APP_DESCRIPTION  "${AI_DESCRIPTION}")
    set(MAUVELY_APP_SUMMARY      "${AI_SUMMARY}")
    set(MAUVELY_APP_VERSION      "${PROJECT_VERSION}")
    # Absolute: the generated Flatpak manifest lives under the build tree, and a
    # relative `path:` there would depend on how deep the build directory is.
    set(MAUVELY_APP_SOURCE_DIR   "${CMAKE_SOURCE_DIR}")

    # AppStream requires a date on every <release>. Taken from the HEAD commit
    # rather than the clock so that rebuilding a commit produces a byte-identical
    # metainfo file; falls back to today where there is no git (a source tarball,
    # or the Flatpak build sandbox, which gets a copy of the tree and no .git).
    execute_process(
        COMMAND git -C "${CMAKE_SOURCE_DIR}" log -1 --format=%cs
        OUTPUT_VARIABLE MAUVELY_APP_RELEASE_DATE
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET)
    if(NOT MAUVELY_APP_RELEASE_DATE MATCHES "^[0-9]{4}-[0-9]{2}-[0-9]{2}$")
        string(TIMESTAMP MAUVELY_APP_RELEASE_DATE "%Y-%m-%d" UTC)
    endif()

    # ── The compile definitions the C++ reads ───────────────────────────────
    #
    # String literals, not QStringLiteral wrappers, so they can be pasted into
    # a wider literal at compile time — core/autostart.cpp builds its whole
    # .desktop entry that way, which keeps the concatenation free.
    # CORE, when given, is the OBJECT library holding core/ and ui/. It needs
    # these too — core/autostart.cpp builds its whole .desktop entry out of them
    # and core/notifier.cpp names the app to the notification daemon — and PUBLIC
    # on the core target is what carries them to the executable and to the tests,
    # so a test that links the core does not fail to compile on a missing macro.
    set(_ai_targets "${AI_BINARY}")
    if(AI_CORE)
        list(APPEND _ai_targets "${AI_CORE}")
    endif()
    # EXTRA_TARGETS is for the repos with no OBJECT library — Compose and Play
    # build each test binary from its own explicit source list, so nothing
    # carries these macros to them and `APP_SHORT_NAME` silently falls back to
    # the "Mauvely" default in branding.h. The symptom is a test asserting on
    # "Northgate Mauvely Play", which is what caught it.
    if(AI_EXTRA_TARGETS)
        list(APPEND _ai_targets ${AI_EXTRA_TARGETS})
    endif()
    foreach(_t IN LISTS _ai_targets)
        if(_t STREQUAL AI_CORE)
            set(_scope PUBLIC)
        else()
            set(_scope PRIVATE)
        endif()
        target_compile_definitions(${_t} ${_scope}
            # The QSettings organisation, and therefore the parent directory of
            # every app's config and data. Not an argument: it is the same for
            # every app by definition, and it is a coupling point — the
            # org-level Handoff/ directory is how Snap passes a capture to
            # Compose, so an app that disagreed about this string would silently
            # stop talking to its siblings.
            APP_ORGANISATION="Mauvely"
            APP_KEY="${AI_KEY}"
            APP_ID="${AI_APP_ID}"
            APP_DISPLAY_NAME="${AI_DISPLAY_NAME}"
            APP_SHORT_NAME="${AI_SHORT_NAME}"
            APP_DESCRIPTION="${AI_DESCRIPTION}"
            APP_VERSION="${PROJECT_VERSION}"
        )
    endforeach()

    # ── The generated packaging files ───────────────────────────────────────
    #
    # Every one of these is a .in template carrying @MAUVELY_APP_*@ tokens. The
    # alternative — checking in a per-app copy of each and editing five of them
    # on a fork — is what produced Play's broken scripts.
    set(_ai_gen "${CMAKE_BINARY_DIR}/packaging")
    file(MAKE_DIRECTORY "${_ai_gen}")

    configure_file(
        "${_MAUVELY_APPINFO_DIR}/../packaging/app.desktop.in"
        "${_ai_gen}/${AI_APP_ID}.desktop" @ONLY)
    configure_file(
        "${_MAUVELY_APPINFO_DIR}/../packaging/app.metainfo.xml.in"
        "${_ai_gen}/${AI_APP_ID}.metainfo.xml" @ONLY)
    configure_file(
        "${_MAUVELY_APPINFO_DIR}/../packaging/linux/flatpak/app.yml.in"
        "${_ai_gen}/${AI_APP_ID}.yml" @ONLY)

    _mauvely_write_app_info()

    # ── Freedesktop install rules ───────────────────────────────────────────
    #
    # AppStream metainfo is not optional decoration: flatpak-builder validates
    # it, and Flathub refuses a build without one. Nothing in the suite had one
    # before this file existed.
    # NOT ANDROID as well as NOT APPLE: Android is a UNIX by CMake's reckoning,
    # and installing freedesktop files into an APK's staging tree puts a
    # .desktop entry inside an Android package.
    if(UNIX AND NOT APPLE AND NOT ANDROID)
        include(GNUInstallDirs)
        install(FILES "${_ai_gen}/${AI_APP_ID}.desktop"
                DESTINATION "${CMAKE_INSTALL_DATADIR}/applications")
        install(FILES "${_ai_gen}/${AI_APP_ID}.metainfo.xml"
                DESTINATION "${CMAKE_INSTALL_DATADIR}/metainfo")
        install(FILES "${_MAUVELY_APPINFO_DIR}/../packaging/app.svg"
                RENAME "${AI_APP_ID}.svg"
                DESTINATION "${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps")
        foreach(_px 16 24 32 48 64 128 256)
            set(_png "${_MAUVELY_APPINFO_DIR}/../packaging/linux/icons/${_px}.png")
            if(EXISTS "${_png}")
                install(FILES "${_png}" RENAME "${AI_APP_ID}.png"
                    DESTINATION
                    "${CMAKE_INSTALL_DATADIR}/icons/hicolor/${_px}x${_px}/apps")
            endif()
        endforeach()
    endif()

    # ── Windows: the executable's own icon ──────────────────────────────────
    if(WIN32 AND AI_ICON)
        set(MAUVELY_ICON_PATH "${AI_ICON}")
        string(REPLACE "/" "\\\\" MAUVELY_ICON_PATH "${MAUVELY_ICON_PATH}")
        configure_file(
            "${_MAUVELY_APPINFO_DIR}/../packaging/windows/app.rc.in"
            "${CMAKE_CURRENT_BINARY_DIR}/app.rc" @ONLY)
        target_sources(${AI_BINARY} PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/app.rc")
    endif()

    # ── Hand off to CPack/WiX ───────────────────────────────────────────────
    include("${_MAUVELY_APPINFO_DIR}/MauvelyPackaging.cmake")
    if(AI_PER_USER)
        mauvely_configure_packaging(
            TARGET       "${AI_BINARY}"
            DISPLAY_NAME "${AI_DISPLAY_NAME}"
            DESCRIPTION  "${AI_DESCRIPTION}"
            VERSION      "${PROJECT_VERSION}"
            UPGRADE_GUID "${AI_UPGRADE_GUID}"
            ICON         "${AI_ICON}"
            PER_USER)
    else()
        mauvely_configure_packaging(
            TARGET       "${AI_BINARY}"
            DISPLAY_NAME "${AI_DISPLAY_NAME}"
            DESCRIPTION  "${AI_DESCRIPTION}"
            VERSION      "${PROJECT_VERSION}"
            UPGRADE_GUID "${AI_UPGRADE_GUID}"
            ICON         "${AI_ICON}")
    endif()

    message(STATUS "Mauvely app: ${AI_DISPLAY_NAME} (${AI_APP_ID}) "
                   "-> ${AI_BINARY} ${PROJECT_VERSION}")
endmacro()
