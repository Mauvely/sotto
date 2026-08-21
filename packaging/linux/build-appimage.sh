#!/usr/bin/env bash
# Builds a single-file <Binary>-<version>-linux-<arch>.AppImage from an existing
# CMake build — the artifact the website's download button serves, and the one
# the updater downloads and swaps in place.
#
# Usage:
#   cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
#   cmake --build build --parallel "$(nproc)"
#   packaging/linux/build-appimage.sh [build-dir]
#
# First run needs network access: it downloads linuxdeploy, linuxdeploy-plugin-qt
# and appimagetool into packaging/linux/.cache/ (gitignored), reused after that.
#
# ── Parameterised, deliberately ─────────────────────────────────────────────
#
# Everything app-specific is read from build/app-info.json, which CMake
# generates from cmake/MauvelyAppInfo.cmake. Nothing here is allowed to name an
# app. Play's copy of this script hardcoded `MauvelyCompose` and a desktop file
# that did not exist in its own repo, so it could not succeed and nobody noticed
# — because no CI job ran it. That is the bug this file's shape prevents.
set -euo pipefail

BUILD_DIR="${1:-build}"
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
CACHE_DIR="$ROOT_DIR/packaging/linux/.cache"
APPDIR="$BUILD_DIR/AppDir"
ARCH="$(uname -m)"

INFO="$BUILD_DIR/app-info.json"
if [[ ! -f "$INFO" ]]; then
    echo "error: $INFO not found — configure the project first (cmake -S . -B $BUILD_DIR)" >&2
    exit 1
fi

# python3 rather than jq: it is on every runner and in every Qt CI image, and
# adding a jq install step to five workflows to parse four fields is not a
# trade worth making.
read_info() { python3 -c "import json,sys;print(json.load(open(sys.argv[1]))[sys.argv[2]])" "$INFO" "$1"; }
BINARY="$(read_info binary)"
APP_ID="$(read_info appId)"
VERSION="$(read_info version)"

if [[ -z "$BINARY" || -z "$APP_ID" || -z "$VERSION" ]]; then
    echo "error: app-info.json is incomplete (binary=$BINARY appId=$APP_ID version=$VERSION)" >&2
    exit 1
fi

# The binary lands in bin/ or bin/Release depending on generator and build type,
# and guessing wrong is how a CI job once uploaded an empty artifact directory
# behind `if-no-files-found: warn`.
# The last candidate is for a project that never set
# CMAKE_RUNTIME_OUTPUT_DIRECTORY and drops the binary at the top of the build
# tree. Sotto did that, and the script's job is to work with the app rather than
# to require the app to be rearranged first.
BIN=""
for candidate in "$BUILD_DIR/bin/Release/$BINARY" "$BUILD_DIR/bin/$BINARY" \
                 "$BUILD_DIR/$BINARY"; do
    [[ -x "$candidate" ]] && { BIN="$candidate"; break; }
done
if [[ -z "$BIN" ]]; then
    echo "error: $BINARY not found under $BUILD_DIR/bin — build it first" >&2
    exit 1
fi

DESKTOP="$BUILD_DIR/packaging/$APP_ID.desktop"
ICON_SVG="$ROOT_DIR/packaging/app.svg"
for f in "$DESKTOP" "$ICON_SVG"; do
    [[ -f "$f" ]] || { echo "error: missing $f" >&2; exit 1; }
done

mkdir -p "$CACHE_DIR"

download() {
    local name="$1" url="$2"
    if [[ ! -x "$CACHE_DIR/$name" ]]; then
        echo "Downloading $name..."
        curl -L --fail --retry 3 -o "$CACHE_DIR/$name" "$url"
        chmod +x "$CACHE_DIR/$name"
    fi
}

download "linuxdeploy-$ARCH.AppImage" \
    "https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-$ARCH.AppImage"
download "linuxdeploy-plugin-qt-$ARCH.AppImage" \
    "https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-$ARCH.AppImage"
download "appimagetool-$ARCH.AppImage" \
    "https://github.com/AppImage/appimagetool/releases/download/continuous/appimagetool-$ARCH.AppImage"

# linuxdeploy spawns the qt plugin as a child process, so the
# --appimage-extract-and-run flag on the command line below covers only the
# outer invocation — the plugin is its own AppImage and still tries to mount
# itself with FUSE. Where FUSE is unavailable it cannot start, and linuxdeploy
# reports that as:
#
#   -- Running input plugin: qt --
#   ERROR: Could not find plugin: qt
#
# which reads as a missing file and is not one. This variable is inherited by
# every child, so all three tools extract instead of mounting.
export APPIMAGE_EXTRACT_AND_RUN=1

rm -rf "$APPDIR"

# linuxdeploy wants an already-installed tree, so reuse the install rules rather
# than assembling a second, subtly different layout by hand.
DESTDIR="$APPDIR" cmake --install "$BUILD_DIR" --prefix /usr >/dev/null

# linuxdeploy-plugin-qt finds qmake on PATH, which on a machine with both Qt5
# and Qt6 (common on Arch: unversioned `qmake` is Qt5's) silently picks the
# wrong one and then reports "Could not find Qt modules to deploy" with no
# further explanation. Point it at Qt6's explicitly.
if [[ -z "${QMAKE:-}" ]]; then
    QMAKE="$(command -v qmake6 || command -v qmake-qt6 || true)"
fi
if [[ -z "$QMAKE" ]]; then
    echo "error: no Qt6 qmake found (looked for qmake6/qmake-qt6) — set QMAKE=/path/to/qt6/qmake" >&2
    exit 1
fi
export QMAKE

# linuxdeploy's bundled `strip` fails outright — aborting the whole run, not
# just skipping that one file — on libraries built with newer toolchains that
# emit RELR relocations (`.relr.dyn`). Stripping is a size optimisation; the
# deploy succeeding is not.
export NO_STRIP=1

# ── Does this app embed QtWebEngine? ────────────────────────────────────────
# Asked of the binary rather than configured per app, so an app that gains or
# loses WebEngine needs no edit here. Compose has it; nothing else does yet.
HAS_WEBENGINE=0
if ldd "$BIN" 2>/dev/null | grep -q 'libQt6WebEngine'; then
    HAS_WEBENGINE=1
    export EXTRA_QT_MODULES="${EXTRA_QT_MODULES:-webenginecore;webenginewidgets;webchannel;positioning}"
fi

# ── Qt plugins that would abort the deploy ──────────────────────────────────
#
# An unresolvable dependency is fatal to the qt plugin, not a warning:
#
#   ERROR: Could not find dependency: libheif.so.1
#   ERROR: Failed to run plugin: qt (exit code: 1)
#
# Two plugin families do this in practice:
#
#   kimg_*                    KDE Frameworks' extra image formats. No app here
#                             uses one, and on a machine with kimageformats
#                             installed but (say) libheif absent, one of them
#                             takes the whole deploy down with it.
#   libqtposition_nmea.so     GPS sentences off a serial port. QtWebEngineWidgets
#                             links Qt6Positioning, so the qt plugin deploys
#                             *every* plugin in plugins/position/ — and this one
#                             links libQt6SerialPort.so.6, from a Qt module no
#                             app here uses and CI does not install.
#
# ── Why the env var and not --exclude-library ──────────────────────────────
#
# `--exclude-library` on the linuxdeploy command line does NOT reach the qt
# plugin. linuxdeploy runs it as a separate process and does not forward its own
# flags, so the pattern is applied to linuxdeploy's own deployment and silently
# ignored by the plugin — which is the half that deploys Qt's plugin tree. Every
# sibling app carries that flag today and it has never done anything for these.
#
# LINUXDEPLOY_EXCLUDED_LIBRARIES is read by both, because a child process
# inherits the environment. Semicolon-separated glob patterns.
#
# This replaces an earlier workaround that moved libqtposition_nmea.so out of
# the Qt plugins directory for the duration of the deploy and restored it in an
# EXIT trap. That worked on CI, where aqtinstall puts Qt somewhere writable, and
# failed on any distro Qt under /usr — and mutating someone's Qt install to
# build a package was never a good trade. Exclusion is the same outcome with
# none of that.
EXCLUDED="kimg_*;libqtposition_nmea.so"

# ── Plus anything else on this machine that cannot resolve ──────────────────
#
# The two families above are excluded because no app here wants them. But the
# *general* failure is "a Qt plugin the deploy touches has a dependency this
# machine does not have", and it turns up somewhere new on every machine:
# kimg_heif wanting libheif on one, libqsqlibase (Firebird) wanting
# libfbclient.so.2 on another — reached because Relay links QtSql and the qt
# plugin deploys every driver in sqldrivers/, not just the one in use.
#
# Enumerating them is a losing game, so scan instead: any plugin whose ldd
# reports an unresolved dependency is one that would abort the deploy, and none
# of them can be doing anything useful for us if their libraries are absent.
QT_PLUGINS_DIR="$($QMAKE -query QT_INSTALL_PLUGINS)"
if [[ -d "$QT_PLUGINS_DIR" ]]; then
    while IFS= read -r plugin; do
        if ldd "$plugin" 2>/dev/null | grep -q 'not found'; then
            name="$(basename "$plugin")"
            EXCLUDED="$EXCLUDED;$name"
            echo "Excluding $name — it has an unresolved dependency on this machine."
        fi
    done < <(find "$QT_PLUGINS_DIR" -name '*.so' -type f 2>/dev/null)
fi

export LINUXDEPLOY_EXCLUDED_LIBRARIES="$EXCLUDED"

# ── Platform plugins beyond xcb ─────────────────────────────────────────────
#
# linuxdeploy-plugin-qt deploys only the platform plugin it can see the app
# using, which on an X11 build machine is xcb and nothing else. Two are worth
# adding by hand:
#
#   wayland    Without it Qt falls back to XWayland on a Wayland desktop. That
#              *works*, so there is no error and no crash — it is just blurry on
#              a HiDPI screen and gets no fractional scaling. A silent downgrade
#              nobody notices until someone complains the app looks soft.
#   offscreen  So the packaged artifact can be smoke-tested the same way the
#              build-tree binary is (APP_SHOT + QT_QPA_PLATFORM=offscreen).
#              Without it the AppImage cannot be verified without a display,
#              which means CI publishes something it never once started.
#
# Built from what is actually present rather than hardcoded: a build machine
# without qt6-wayland should produce an AppImage without Wayland support, not
# fail. Wayland also needs its shell-integration and decoration plugins, which
# the qt plugin brings along once the platform plugin is in the list.
_extra_platforms=()
for _p in libqwayland.so libqwayland-generic.so libqwayland-egl.so libqoffscreen.so; do
    [[ -f "$QT_PLUGINS_DIR/platforms/$_p" ]] && _extra_platforms+=("$_p")
done
if (( ${#_extra_platforms[@]} )); then
    _joined="$(IFS=';'; echo "${_extra_platforms[*]}")"
    export EXTRA_PLATFORM_PLUGINS="${EXTRA_PLATFORM_PLUGINS:-$_joined}"
    echo "Extra platform plugins: $EXTRA_PLATFORM_PLUGINS"
else
    echo "note: no wayland or offscreen platform plugin found beside xcb." >&2
fi

# Real PNGs alongside the SVG: linuxdeploy derives the AppImage's .DirIcon from
# whichever icon it can use, and given only an SVG it has to rasterise one
# itself. When it cannot, it falls back to a generic placeholder — which is the
# missing-icon square on Relay's build today. The SVG stays first so the
# scalable one still wins where a scalable one is wanted.
ICON_ARGS=(--icon-file "$ICON_SVG")
for px in 256 128 48; do
    png="$ROOT_DIR/packaging/linux/icons/$px.png"
    [[ -f "$png" ]] && ICON_ARGS+=(--icon-file "$png")
done

# --exclude-library repeats the pattern for linuxdeploy's own deployment pass;
# LINUXDEPLOY_EXCLUDED_LIBRARIES above is what covers the qt plugin's.
"$CACHE_DIR/linuxdeploy-$ARCH.AppImage" --appimage-extract-and-run \
    --appdir "$APPDIR" \
    --executable "$BIN" \
    --desktop-file "$DESKTOP" \
    "${ICON_ARGS[@]}" \
    --plugin qt \
    --exclude-library="kimg_*"

if (( HAS_WEBENGINE )); then
    # WebEngine is not a normal shared library: it needs a helper *process*, its
    # own resource packs and ICU data. If any is missing the app still starts and
    # still works — it just shows a placeholder where the web view should be.
    # That is a silent, shippable failure, so it is asserted rather than hoped for.
    QT_LIBEXEC="$($QMAKE -query QT_INSTALL_LIBEXECS)"
    QT_RESOURCES="$($QMAKE -query QT_INSTALL_DATA)/resources"
    QT_TRANSLATIONS="$($QMAKE -query QT_INSTALL_TRANSLATIONS)"

    if [[ ! -x "$APPDIR/usr/libexec/QtWebEngineProcess" ]]; then
        echo "Staging QtWebEngineProcess (the qt plugin did not bring it)..."
        mkdir -p "$APPDIR/usr/libexec"
        cp "$QT_LIBEXEC/QtWebEngineProcess" "$APPDIR/usr/libexec/"
    fi

    mkdir -p "$APPDIR/usr/resources"
    for f in qtwebengine_resources.pak qtwebengine_resources_100p.pak \
             qtwebengine_resources_200p.pak qtwebengine_devtools_resources.pak icudtl.dat; do
        [[ -f "$APPDIR/usr/resources/$f" ]] && continue
        [[ -f "$QT_RESOURCES/$f" ]] && cp "$QT_RESOURCES/$f" "$APPDIR/usr/resources/"
    done

    if [[ -d "$QT_TRANSLATIONS/qtwebengine_locales" \
          && ! -d "$APPDIR/usr/translations/qtwebengine_locales" ]]; then
        mkdir -p "$APPDIR/usr/translations"
        cp -r "$QT_TRANSLATIONS/qtwebengine_locales" "$APPDIR/usr/translations/"
    fi

    missing=()
    [[ -x "$APPDIR/usr/libexec/QtWebEngineProcess" ]] || missing+=("libexec/QtWebEngineProcess")
    [[ -f "$APPDIR/usr/resources/qtwebengine_resources.pak" ]] \
        || missing+=("resources/qtwebengine_resources.pak")
    # icudtl.dat exists only for a WebEngine built against *bundled* ICU. Qt's own
    # binaries ship one; a distribution build often does not — Debian links system
    # ICU and has no icudtl.dat anywhere, where the file is not missing but
    # inapplicable. Demanding it unconditionally fails a perfectly good AppDir.
    if [[ -f "$QT_RESOURCES/icudtl.dat" ]]; then
        [[ -f "$APPDIR/usr/resources/icudtl.dat" ]] || missing+=("resources/icudtl.dat")
    fi
    compgen -G "$APPDIR/usr/lib/libQt6WebEngineCore.so*" >/dev/null \
        || missing+=("lib/libQt6WebEngineCore.so")
    if (( ${#missing[@]} )); then
        echo "error: QtWebEngine is incomplete in the AppDir — the web view would be dead." >&2
        printf '  missing: %s\n' "${missing[@]}" >&2
        echo "  qmake reported libexec=$QT_LIBEXEC resources=$QT_RESOURCES" >&2
        exit 1
    fi

    # WebEngine locates its helper process and resources relative to paths
    # compiled into it, which point at the build machine's Qt prefix — wrong
    # inside an AppImage. linuxdeploy's AppRun runs this hook before launching.
    mkdir -p "$APPDIR/apprun-hooks"
    cat > "$APPDIR/apprun-hooks/webengine.sh" <<'HOOK'
#!/bin/bash
export QTWEBENGINEPROCESS_PATH="${APPDIR}/usr/libexec/QtWebEngineProcess"
export QTWEBENGINE_RESOURCES_PATH="${APPDIR}/usr/resources"
export QTWEBENGINE_LOCALES_PATH="${APPDIR}/usr/translations/qtwebengine_locales"
# Chromium's sandbox needs either a setuid helper or unprivileged user
# namespaces, neither of which an AppImage can rely on, and it refuses to start
# at all without one. This trades the renderer sandbox for running — the same
# trade every Qt AppImage shipping WebEngine makes.
export QTWEBENGINE_DISABLE_SANDBOX=1
HOOK
    chmod +x "$APPDIR/apprun-hooks/webengine.sh"
fi

# Name matches the `platform` key the release feed indexes by (linux-x86_64), so
# a locally built AppImage and a published one are called the same thing.
OUT="$BUILD_DIR/$BINARY-$VERSION-linux-$ARCH.AppImage"
"$CACHE_DIR/appimagetool-$ARCH.AppImage" --appimage-extract-and-run "$APPDIR" "$OUT"

echo "Wrote $OUT"
