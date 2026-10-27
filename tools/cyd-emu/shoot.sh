#!/usr/bin/env bash
# Build an example against the real TFT_eSPI + stub Arduino core and capture
# screenshots at the given (virtual) times.
#
#   tools/cyd-emu/shoot.sh examples/matrix-rain docs/out 3000,8000 [tap_ms ...]
#   -> docs/out_3000.png, docs/out_8000.png
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
EX="$(cd "$1" && pwd)"
OUT="$2"
SHOTS="$3"
shift 3

TFT="$(ls -d "$ROOT"/.pio/libdeps/*/TFT_eSPI 2>/dev/null | head -1)"
if [ -z "$TFT" ]; then
  echo "TFT_eSPI not found - run '.venv/bin/pio run -e cyd' once to download libraries" >&2
  exit 1
fi

WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT

# TFT_eSPI keeps font-table pointers in uint32_t; widen for 64-bit hosts
sed 's/uint32_t flash_address = 0;/uintptr_t flash_address = 0;/' "$TFT/TFT_eSPI.cpp" > "$WORK/TFT_eSPI_host.cpp"

# copy example headers but never a private config.h (real WiFi creds / IPs)
mkdir -p "$WORK/include"
if [ -d "$EX/include" ]; then
  find "$EX/include" -maxdepth 1 -type f ! -name config.h -exec cp {} "$WORK/include/" \;
fi

DEFS=(
  -DUSER_SETUP_LOADED=1 -DST7796_DRIVER=1 -DTFT_WIDTH=320 -DTFT_HEIGHT=480
  -DTFT_MISO=12 -DTFT_MOSI=13 -DTFT_SCLK=14 -DTFT_CS=15 -DTFT_DC=2 -DTFT_RST=-1
  -DTFT_BL=27 -DTFT_BACKLIGHT_ON=HIGH
  -DLOAD_GLCD=1 -DLOAD_FONT2=1 -DLOAD_FONT4=1 -DLOAD_FONT6=1 -DLOAD_FONT7=1 -DLOAD_FONT8=1 -DLOAD_GFXFF=1
  -DSPI_FREQUENCY=40000000 -DSPI_READ_FREQUENCY=20000000
)
FLAGS=(-std=c++17 -O1 -w -I"$ROOT/tools/cyd-emu/stubs" -I"$WORK/include" -I"$TFT")

# optional example hook: tools/emu_feed.cpp defines emu_tick() (e.g. scripted ESP-NOW packets)
EXTRA=()
[ -f "$EX/tools/emu_feed.cpp" ] && EXTRA=("$EX/tools/emu_feed.cpp")

clang++ "${FLAGS[@]}" "${DEFS[@]}" \
  "$ROOT/tools/cyd-emu/emu.cpp" "$WORK/TFT_eSPI_host.cpp" "$EX/src/main.cpp" ${EXTRA[@]+"${EXTRA[@]}"} -o "$WORK/emu"

mkdir -p "$(dirname "$OUT")"
"$WORK/emu" "$WORK/shot" "$SHOTS" "$@" 2>"$WORK/log.txt" || { tail -20 "$WORK/log.txt" >&2; exit 1; }

python3 - "$WORK" "$OUT" "$SHOTS" <<'EOF'
import sys
from PIL import Image
work, out, shots = sys.argv[1], sys.argv[2], sys.argv[3].split(",")
for s in shots:
    Image.open(f"{work}/shot_{s}.ppm").save(f"{out}_{s}.png", optimize=True)
    print(f"{out}_{s}.png")
EOF
