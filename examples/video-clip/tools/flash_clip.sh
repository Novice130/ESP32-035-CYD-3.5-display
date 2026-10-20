#!/usr/bin/env bash
# Write a clip.bin (made by make_clip.py) into the 'clip' flash partition at 0x110000.
# The firmware is not touched, so you can swap clips without rebuilding.
#
#   tools/flash_clip.sh CLIP.bin [PORT]        (default port /dev/cu.usbserial-11120)
set -euo pipefail

OFFSET=0x110000
MAX_BYTES=$((0x2F0000))   # size of the clip partition (partitions.csv)

CLIP="${1:-}"
PORT="${2:-/dev/cu.usbserial-11120}"
if [ -z "$CLIP" ]; then
    echo "usage: $0 CLIP.bin [PORT]" >&2
    exit 1
fi
if [ ! -f "$CLIP" ]; then
    echo "error: no such file: $CLIP" >&2
    exit 1
fi

SIZE=$(wc -c < "$CLIP" | tr -d ' ')
if [ "$SIZE" -gt "$MAX_BYTES" ]; then
    echo "error: $CLIP is $SIZE bytes, the clip partition holds $MAX_BYTES." >&2
    echo "       Make it shorter or smaller with make_clip.py (--dur, --quality, --fps)." >&2
    exit 1
fi
if [ "$(head -c 4 "$CLIP")" != "CLIP" ]; then
    echo "error: $CLIP is not a clip.bin (no CLIP header). Make one with make_clip.py." >&2
    exit 1
fi

REPO="$(cd "$(dirname "$0")/../../.." && pwd)"
PY="$REPO/.venv/bin/python"
if [ ! -x "$PY" ]; then
    echo "error: $PY not found (the repo's PlatformIO venv provides esptool)." >&2
    exit 1
fi

echo "Flashing $CLIP ($SIZE bytes) to $OFFSET on $PORT ..."
"$PY" -m esptool --chip esp32 --port "$PORT" --baud 460800 write_flash "$OFFSET" "$CLIP"
