#!/usr/bin/env bash
# The one entry point for producing Linux artifacts.
#
#   scripts/package.sh [--format appimage|all] [--build-dir DIR] [--out DIR]
#
# Every Mauvely app has this script at this path with these flags, so "how do I
# build a package for X" has one answer across the whole suite. It refuses
# formats this host cannot build and formats the app does not target, both read
# from build/app-info.json rather than hardcoded here.
#
# Windows formats live in scripts/package.ps1. There is no cross-building: an
# .msi needs WiX and an .msix needs MakeAppx, and both are Windows-only.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
FORMAT="all"
BUILD_DIR="build"
OUT_DIR=""

usage() {
    cat >&2 <<USAGE
usage: scripts/package.sh [options]

  --format appimage|all           what to build (default: all)
  --build-dir DIR                 CMake build directory (default: build)
  --out DIR                       copy finished artifacts here
  -h, --help                      this

The build directory must already be configured; this script builds it if the
binary is missing but never configures it, because the configure flags are the
app's business (SOTTO_GPU, CE_REQUIRE_WEBENGINE, RELAY_PACKAGING …).
USAGE
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --format)    FORMAT="${2:?--format needs a value}"; shift 2 ;;
        --build-dir) BUILD_DIR="${2:?--build-dir needs a value}"; shift 2 ;;
        --out)       OUT_DIR="${2:?--out needs a value}"; shift 2 ;;
        -h|--help)   usage; exit 0 ;;
        *) echo "error: unknown option '$1'" >&2; usage; exit 2 ;;
    esac
done

case "$FORMAT" in
    appimage|all) ;;
    msi|msix)
        echo "error: '$FORMAT' is a Windows format — use scripts/package.ps1 on Windows." >&2
        exit 2 ;;
    *) echo "error: unknown format '$FORMAT'" >&2; usage; exit 2 ;;
esac

if [[ "$(uname -s)" != "Linux" ]]; then
    echo "error: Linux packaging must run on Linux (this is $(uname -s))." >&2
    exit 2
fi

INFO="$BUILD_DIR/app-info.json"
if [[ ! -f "$INFO" ]]; then
    echo "error: $INFO not found. Configure first, e.g." >&2
    echo "  cmake -S . -B $BUILD_DIR -DCMAKE_BUILD_TYPE=Release" >&2
    exit 1
fi

py() { python3 -c "$1" "$INFO" "${2:-}"; }
BINARY="$(py 'import json,sys;print(json.load(open(sys.argv[1]))["binary"])')"
SUPPORTED="$(py 'import json,sys;print(" ".join(json.load(open(sys.argv[1]))["formats"]["linux"]))')"

# An app that ships for Windows only has an empty Linux format list, and
# "does not target appimage (targets: )" is a worse thing to read than the
# actual answer.
if [[ -z "$SUPPORTED" ]]; then
    echo "This app does not ship for Linux — see PLATFORMS in CMakeLists.txt." >&2
    echo "Windows artifacts are built by scripts/package.ps1 on Windows." >&2
    exit 0
fi

wanted() {
    [[ "$FORMAT" == "all" || "$FORMAT" == "$1" ]] || return 1
    # `all` means "everything this app targets", so an app that drops a format
    # from app-info.json stops building it without editing this script.
    if [[ " $SUPPORTED " != *" $1 "* ]]; then
        if [[ "$FORMAT" == "$1" ]]; then
            echo "error: this app does not target $1 (targets: $SUPPORTED)" >&2
            exit 2
        fi
        return 1
    fi
    return 0
}

# Build if the binary is missing. Not `always`, because a caller who just built
# with specific flags should not have them silently re-run.
if ! compgen -G "$BUILD_DIR/bin/$BINARY" >/dev/null \
   && ! compgen -G "$BUILD_DIR/bin/Release/$BINARY" >/dev/null; then
    echo "==> $BINARY not built yet — building $BUILD_DIR"
    # Bounded on purpose. A bare `-j` means UNLIMITED jobs to Make, and the
    # kernel OOM-killed the compiler mid-link on a 4-core CI runner.
    cmake --build "$BUILD_DIR" --config Release --parallel "$(nproc 2>/dev/null || echo 4)"
fi

built=()

if wanted appimage; then
    echo "==> AppImage"
    "$ROOT_DIR/packaging/linux/build-appimage.sh" "$BUILD_DIR"
    built+=("$(ls -t "$BUILD_DIR"/*.AppImage | head -1)")
fi

if (( ${#built[@]} == 0 )); then
    echo "error: nothing was built — check --format against the app's targets ($SUPPORTED)" >&2
    exit 1
fi

if [[ -n "$OUT_DIR" ]]; then
    mkdir -p "$OUT_DIR"
    for f in "${built[@]}"; do cp "$f" "$OUT_DIR/"; done
fi

echo
echo "Built:"
for f in "${built[@]}"; do
    printf '  %s  (%s)\n' "$f" "$(du -h "$f" | cut -f1)"
done
