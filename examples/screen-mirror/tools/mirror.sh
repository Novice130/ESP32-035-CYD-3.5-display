#!/bin/sh
# Mirror the Mac's screen to the CYD. Sets up the example's own venv (mss + Pillow) on first run.
# Arguments go to mirror.py, e.g.  ./mirror.sh --follow   ./mirror.sh --display 2   ./mirror.sh --help
set -e
here="$(cd "$(dirname "$0")/.." && pwd)"
venv="$here/.venv"
if [ ! -x "$venv/bin/python" ]; then
    echo "Setting up $venv ..."
    python3 -m venv "$venv"
    "$venv/bin/pip" install --no-cache-dir -q mss Pillow
fi
exec "$venv/bin/python" -I -u "$here/tools/mirror.py" "$@"
