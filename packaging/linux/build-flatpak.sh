#!/usr/bin/env bash
# Builds a single-file <Binary>-<version>-linux-<arch>.flatpak bundle.
#
# Usage:
#   cmake -S . -B build -DCMAKE_BUILD_TYPE=Release   # generates build/app-info.json
#   packaging/linux/build-flatpak.sh [build-dir]
#
# Unlike the AppImage script this does NOT reuse the CMake build: flatpak-builder
# compiles the app itself, inside the runtime's sysroot, against the runtime's
# Qt. That is the point of a Flatpak — the binary links the libraries it will
# actually run against, rather than dragging the host's Qt along.
#
# ── The build sandbox has no network ────────────────────────────────────────
#
# Anything the CMake configure step fetches has to be declared as a `sources:`
# entry in the manifest instead. Two apps in the suite are affected:
#
#   · Sotto fetches whisper.cpp through FetchContent
#   · Relay fetches Corrosion and builds a Rust crate
#
# Both need extra manifest modules; see the notes in
# packaging/linux/flatpak/app.yml.in. A build that "hangs" here is always this.
set -euo pipefail

BUILD_DIR="${1:-build}"
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
ARCH="$(uname -m)"

INFO="$BUILD_DIR/app-info.json"
if [[ ! -f "$INFO" ]]; then
    echo "error: $INFO not found — configure the project first (cmake -S . -B $BUILD_DIR)" >&2
    exit 1
fi
read_info() { python3 -c "import json,sys;print(json.load(open(sys.argv[1]))[sys.argv[2]])" "$INFO" "$1"; }
BINARY="$(read_info binary)"
APP_ID="$(read_info appId)"
VERSION="$(read_info version)"

MANIFEST="$BUILD_DIR/packaging/$APP_ID.yml"
[[ -f "$MANIFEST" ]] || { echo "error: missing generated manifest $MANIFEST" >&2; exit 1; }

for tool in flatpak flatpak-builder; do
    command -v "$tool" >/dev/null || {
        echo "error: $tool is not installed." >&2
        echo "  Fedora/RHEL:  sudo dnf install flatpak flatpak-builder" >&2
        echo "  Debian/Ubuntu: sudo apt-get install flatpak flatpak-builder" >&2
        echo "  Arch:         sudo pacman -S flatpak flatpak-builder" >&2
        exit 1
    }
done

RUNTIME_VERSION="$(sed -n "s/^runtime-version:[[:space:]]*'\{0,1\}\([0-9.]*\)'\{0,1\}.*/\1/p" \
    "$MANIFEST" | head -1)"
: "${RUNTIME_VERSION:=6.7}"

# --user, and Flathub added per-user: a release build must not depend on how the
# machine's system-wide remotes happen to be configured, and CI runners have
# none at all.
flatpak remote-add --if-not-exists --user flathub \
    https://dl.flathub.org/repo/flathub.flatpakrepo
echo "Installing org.kde.Platform//$RUNTIME_VERSION and org.kde.Sdk//$RUNTIME_VERSION..."
flatpak install --user --noninteractive --or-update flathub \
    "org.kde.Platform//$RUNTIME_VERSION" "org.kde.Sdk//$RUNTIME_VERSION"

STATE_DIR="$BUILD_DIR/flatpak-builder"
REPO_DIR="$BUILD_DIR/flatpak-repo"
rm -rf "$STATE_DIR/build" "$REPO_DIR"

# --ccache is deliberately not passed: it caches into the state dir, which CI
# throws away anyway, and locally it hides a stale-object class of failure that
# is very hard to read once it happens.
flatpak-builder \
    --user \
    --disable-rofiles-fuse \
    --force-clean \
    --repo="$REPO_DIR" \
    --state-dir="$STATE_DIR" \
    "$STATE_DIR/build" \
    "$MANIFEST"

OUT="$BUILD_DIR/$BINARY-$VERSION-linux-$ARCH.flatpak"
flatpak build-bundle "$REPO_DIR" "$OUT" "$APP_ID" --runtime-repo=https://flathub.org/repo/flathub.flatpakrepo

echo "Wrote $OUT"
echo
echo "Install and run it with:"
echo "  flatpak install --user $OUT && flatpak run $APP_ID"
