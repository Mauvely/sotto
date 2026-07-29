#!/usr/bin/env bash
# Builds a single-file Sotto-<arch>.AppImage from an existing CMake build —
# the artifact that goes on the website's download button.
#
# Usage:
#   cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DSOTTO_GPU=<backend>
#   cmake --build build -j
#   packaging/linux/build-appimage.sh [build-dir]
#
# First run needs network access: it downloads linuxdeploy, linuxdeploy-
# plugin-qt and appimagetool into packaging/linux/.cache/ (gitignored),
# reused on later runs. Needs `qt6-wayland` installed at build time so
# linuxdeploy's Qt plugin bundles the wayland platform plugin Sotto needs —
# without it the AppImage runs but silently has no Wayland QPA backend.
set -euo pipefail

BUILD_DIR="${1:-build}"
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
CACHE_DIR="$ROOT_DIR/packaging/linux/.cache"
APPDIR="$BUILD_DIR/AppDir"
ARCH="$(uname -m)"

if [[ ! -x "$BUILD_DIR/sotto" ]]; then
    echo "error: $BUILD_DIR/sotto not found — build it first (see usage above)" >&2
    exit 1
fi

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

rm -rf "$APPDIR"

# linuxdeploy wants an already-installed tree so the binary, .desktop file
# and icon are already at their final relative layout under $APPDIR/usr/.
DESTDIR="$APPDIR" cmake --install "$BUILD_DIR" --prefix /usr >/dev/null

# linuxdeploy-plugin-qt finds qmake via PATH, which on a system with both Qt5
# and Qt6 installed (common on Arch: unversioned `qmake` is Qt5's) silently
# picks the wrong one and then reports "Could not find Qt modules to deploy"
# with no further explanation. Point it at Qt6's explicitly.
if [[ -z "${QMAKE:-}" ]]; then
    QMAKE="$(command -v qmake6 || command -v qmake-qt6 || true)"
fi
if [[ -z "$QMAKE" ]]; then
    echo "error: no Qt6 qmake found (looked for qmake6/qmake-qt6) — set QMAKE=/path/to/qt6/qmake" >&2
    exit 1
fi
export QMAKE

# linuxdeploy's bundled `strip` fails outright (aborting the whole run, not
# just skipping that one file) on libraries built with newer toolchains that
# emit RELR relocations (`.relr.dyn`) — e.g. current Arch. Stripping is a
# size optimisation, not a correctness requirement, so just skip it rather
# than have the deploy's success depend on the build machine's binutils age.
export NO_STRIP=1

# --appimage-extract-and-run: these tool AppImages need FUSE to mount
# themselves normally, which most CI runners (and some dev sandboxes) don't
# have; extracting and running works everywhere FUSE does too, just a touch
# slower, so it's the safe default rather than something to special-case.
# --exclude-library="kimg_*": KDE Frameworks' extra QImage format plugins
# (AVIF/HEIF/JPEG-XR/...) — Sotto has no use for any of them, and on a build
# machine with a partially-installed kimageformats package one of them
# (kimg_jxr.so, needing libjxrglue.so.0) can be missing a system dependency
# entirely, which aborts the whole deploy rather than just that one plugin.
"$CACHE_DIR/linuxdeploy-$ARCH.AppImage" --appimage-extract-and-run \
    --appdir "$APPDIR" \
    --executable "$APPDIR/usr/bin/sotto" \
    --desktop-file "$ROOT_DIR/packaging/net.mauvely.sotto.app.desktop" \
    --icon-file "$ROOT_DIR/resources/icons/sotto.svg" \
    --plugin qt \
    --exclude-library="kimg_*"

"$CACHE_DIR/appimagetool-$ARCH.AppImage" --appimage-extract-and-run \
    "$APPDIR" "$BUILD_DIR/Sotto-$ARCH.AppImage"

echo "Wrote $BUILD_DIR/Sotto-$ARCH.AppImage"
