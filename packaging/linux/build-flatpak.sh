#!/usr/bin/env bash
# Builds <Binary>-<version>-linux-<arch>.flatpak from the working tree.
#
#   packaging/linux/build-flatpak.sh [out-dir]          default: dist
#
# For Mauvely OS, not for Flathub: Mauvely's apps are never published there.
# The release registers the bundle in the feed as `flatpak`, and os-image's
# publish-flatpak.yml takes the newest one into the signed Mauvely remote,
# re-branding its `master` branch to `stable`.
#
# flatpak-builder compiles the app inside org.kde.Sdk, against the runtime's
# Qt, so no host Qt and no host configure are needed. That is why the id, the
# binary and the version are read from CMakeLists.txt rather than from a
# configure's app-info.json. A wrong reading cannot ship: the build is checked
# against the desktop file and binary the sandboxed CMake actually installed.
#
# ── Things that catch everyone ──────────────────────────────────────────────
#
# · The build sandbox has no network. Anything the configure fetches must be a
#   `sources:` entry in packaging/linux/flatpak/app.yml.in (Sotto's whisper.cpp).
# · bwrap needs a user namespace. The self-hosted runners cannot create one
#   (`bwrap: cannot open /proc/sys/user/max_user_namespaces: Read-only file
#   system`), which is why release.yml builds this on GitHub's runner.
# · Set FLATPAK_USER_DIR to keep the runtime, the SDK and the Flathub remote
#   out of your own installation.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
OUT_DIR="${1:-dist}"
ARCH="$(uname -m)"
TEMPLATE="$ROOT_DIR/packaging/linux/flatpak/app.yml.in"
[[ -f "$TEMPLATE" ]] || { echo "error: no $TEMPLATE — this app has no Flatpak" >&2; exit 1; }

for tool in flatpak flatpak-builder python3; do
    command -v "$tool" >/dev/null || { echo "error: $tool is not installed" >&2; exit 1; }
done

# The mauvely_app_info() fields, one per line, as every app writes them.
field() {
    sed -nE "s/^[[:space:]]*$1[[:space:]]+([^[:space:]#]+).*/\1/p" "$ROOT_DIR/CMakeLists.txt" | head -1
}
KEY="$(field KEY)"
APP_ID="$(field APP_ID)"
BINARY="$(field BINARY)"
# Empty for an app that never shipped under another id.
PREVIOUS_ID="$(field PREVIOUS_APP_ID)"
PROJECT="$(sed -nE 's/^project\(([A-Za-z0-9_]+).*/\1/p' "$ROOT_DIR/CMakeLists.txt" | head -1)"
[[ "$BINARY" == '${PROJECT_NAME}' ]] && BINARY="$PROJECT"
# The same reading release.yml's gate makes.
VERSION="$(sed -n 's/^[[:space:]]*VERSION[[:space:]]\+\([0-9][0-9.]*\).*/\1/p' \
    "$ROOT_DIR/CMakeLists.txt" | head -1)"
for v in KEY APP_ID BINARY VERSION; do
    [[ -n "${!v}" ]] || { echo "error: could not read $v from CMakeLists.txt" >&2; exit 1; }
done
echo "==> $APP_ID $VERSION ($BINARY)"

# Outside the source tree, which the manifest copies in whole.
WORK="${FLATPAK_WORK_DIR:-${XDG_CACHE_HOME:-$HOME/.cache}/mauvely-flatpak-build/$KEY}"
MANIFEST="$WORK/$APP_ID.yml"
mkdir -p "$WORK" "$OUT_DIR"
sed -e "s|@MAUVELY_APP_ID@|$APP_ID|g" \
    -e "s|@MAUVELY_APP_BINARY@|$BINARY|g" \
    -e "s|@MAUVELY_APP_KEY@|$KEY|g" \
    -e "s|@MAUVELY_APP_PREVIOUS_ID@|$PREVIOUS_ID|g" \
    -e "s|@MAUVELY_APP_SOURCE_DIR@|$ROOT_DIR|g" \
    "$TEMPLATE" > "$MANIFEST"

flatpak remote-add --if-not-exists --user flathub https://dl.flathub.org/repo/flathub.flatpakrepo

# --install-deps-from fetches the runtime, the SDK and any `base` the manifest
# names. --ccache is deliberately not passed: it hides a stale-object class of
# failure that is very hard to read once it happens.
flatpak-builder --user --disable-rofiles-fuse --force-clean \
    --install-deps-from=flathub \
    --repo="$WORK/repo" --state-dir="$WORK/state" \
    "$WORK/app" "$MANIFEST"

# What the sandboxed CMake installed must carry the id the manifest declared:
# the desktop file's name, its Icon= and StartupWMClass=, and the command.
FILES="$WORK/app/files"
DESKTOP="$FILES/share/applications/$APP_ID.desktop"
[[ -f "$DESKTOP" ]] || { echo "error: the build installed no $APP_ID.desktop" >&2; ls "$FILES/share/applications" >&2 || true; exit 1; }
for key in Icon StartupWMClass; do
    got="$(sed -n "s/^$key=//p" "$DESKTOP" | head -1)"
    [[ "$got" == "$APP_ID" ]] || { echo "error: $key=$got in $APP_ID.desktop" >&2; exit 1; }
done
[[ -x "$FILES/bin/$BINARY" ]] || { echo "error: the build installed no bin/$BINARY" >&2; exit 1; }

OUT="$OUT_DIR/$BINARY-$VERSION-linux-$ARCH.flatpak"
flatpak build-bundle "$WORK/repo" "$OUT" "$APP_ID" \
    --runtime-repo=https://dl.flathub.org/repo/flathub.flatpakrepo
echo "Wrote $OUT"
