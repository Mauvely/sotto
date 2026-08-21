#!/usr/bin/env bash
# Renders packaging/linux/icons/{16,24,32,48,64,128,256}.png and
# resources/icons/app.ico from packaging/app.svg.
#
# Run this after replacing app.svg on a fork. Everything downstream — the
# freedesktop hicolor install, the AppImage's .DirIcon, the MSIX tiles and the
# WiX product icon — is generated from these, so this is the only place a raster
# size is decided.
#
# Each size is rendered from the vector rather than downscaled from one large
# raster: a thin stroke turns to mush at 16px if it is resampled twice.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SVG="$ROOT_DIR/packaging/app.svg"
ICON_DIR="$ROOT_DIR/packaging/linux/icons"
ICO="$ROOT_DIR/resources/icons/app.ico"
SIZES=(16 24 32 48 64 128 256)

command -v rsvg-convert >/dev/null || {
    echo "error: rsvg-convert not found (package: librsvg / librsvg2-bin)" >&2
    exit 1
}

mkdir -p "$ICON_DIR" "$(dirname "$ICO")"

for px in "${SIZES[@]}"; do
    rsvg-convert -w "$px" -h "$px" -o "$ICON_DIR/$px.png" "$SVG"
    echo "  ${px}x${px}"
done

# ── The .ico ────────────────────────────────────────────────────────────────
#
# Written directly rather than through a library, for two reasons that have both
# bitten this project:
#
#   · Pillow's ICO writer keeps only one frame when handed `append_images`, so a
#     seven-size icon comes out as one.
#   · ImageMagick writes uncompressed BMP frames. The result is byte-identical in
#     size whatever the artwork — about 372 kB — because it is purely a function
#     of the dimensions. That then gets embedded in the .exe and shipped in every
#     installer. The same seven sizes as PNG frames come to roughly 16 kB.
#
# The container is not complicated: a 6-byte ICONDIR, a 16-byte ICONDIRENTRY per
# image, then the PNG payloads. Windows Vista and later read PNG frames at any
# size, and nothing in the suite targets older than 10.
python3 - "$ICO" "${SIZES[@]}" <<'PY'
import struct, sys, pathlib

out = pathlib.Path(sys.argv[1])
sizes = [int(a) for a in sys.argv[2:]]
icon_dir = out.parent.parent.parent / 'packaging' / 'linux' / 'icons'

frames = [(px, (icon_dir / f'{px}.png').read_bytes()) for px in sizes]

# ICONDIR: reserved, type (1 = icon), count
header = struct.pack('<HHH', 0, 1, len(frames))
offset = len(header) + 16 * len(frames)

entries, payloads = b'', b''
for px, data in frames:
    # 0 means 256 in a single byte — the format's one wart.
    dim = 0 if px >= 256 else px
    entries += struct.pack(
        '<BBBBHHII',
        dim, dim,   # width, height
        0,          # palette size; 0 for truecolour
        0,          # reserved
        1,          # colour planes
        32,         # bits per pixel
        len(data),
        offset,
    )
    payloads += data
    offset += len(data)

out.write_bytes(header + entries + payloads)
print(f"  app.ico ({len(frames)} frames, {len(header) + len(entries) + len(payloads) // 1} bytes)")
PY
