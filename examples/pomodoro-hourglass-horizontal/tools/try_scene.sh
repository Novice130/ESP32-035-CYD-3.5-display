#!/bin/bash
# tools/try_scene.sh <scene> <outdir> <sim commands...>
# Compiles the simulator with only that scene, runs it, converts PPMs to <outdir>/sheet.png
set -e
scene=$1; out=$2; shift 2
here="$(cd "$(dirname "$0")" && pwd)"
mkdir -p "$out"
rm -f "${out:?}"/*.ppm
clang++ -std=gnu++17 -O2 -DHG_ONLY_ONE -DONLY_$scene -I"$here/../include" "$here/sim.cpp" -o "$out/sim_$scene"
"$out/sim_$scene" "$out" 0 "$@"
python3 "$here/sheet.py" "$out"
