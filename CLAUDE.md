# CLAUDE.md — how to build a new animation / game for this board

Repo = ESP32-035 (CYD 3.5", ST7796 480x320, XPT2046 touch) hardware guide + standalone examples.
Every visual project lives in `examples/<name>/` with its own PlatformIO env. Best reference
project: `examples/pokemon-kalos/` (framebuffer engine, art pipeline, simulator, tap controls).

## Read first
- `LESSONS.md`: every problem we already hit (Wi-Fi freezes, .local lookups, NVS wiped by the
  web installer, busy serial ports, hosting) with the evidence and the fix. Add new ones there.
- `BOARDS.md`: other CYD boards (2.8" 2432S028R has backlight 21 and touch on its own pins;
  2.4"/3.5" have backlight 27 and touch on the display bus) and how the screen-mirror envs differ.
- Screen-mirror website = `docs/`, auto-deployed by Cloudflare Pages (project `learnnovice-robo`,
  output dir `docs`) on every push to `main` → https://robo.learnnovice.com/. After firmware
  changes run `examples/screen-mirror/tools/build-web-firmware.sh` (all boards, refuses to ship
  the Wi-Fi password), commit `docs/firmware`, push, then `curl` the live manifest to confirm.
- Debug on hardware with evidence, not guesses: `curl http://<board>/stats`, serial log at
  2 Mbaud, a temporary `[DIAG]` line if needed (remove before commit).

## Toolchain (already installed)
- PlatformIO is in the venv: `.venv/bin/pio` (not on PATH).
- Board port: `/dev/cu.usbserial-11110` (set in `platformio.ini`). `flash_cyd.py` has an old port; prefer pio.
- Host: Python 3 + Pillow (no numpy), `clang++` for the desktop simulator.
- Build: `.venv/bin/pio run -e <env>` · Flash: `.venv/bin/pio run -e <env> -t upload` (no BOOT button needed).

## Hardware gotchas (all examples copy this setup)
- Backlight GPIO 27 HIGH. RGB LED 4/16/17 active LOW. Speaker GPIO 26. LDR GPIO 34.
- Touch shares HSPI with the display: `touchSPI.begin(14, 12, 13, 33)`, `ts.begin(touchSPI)`.
- Landscape = `tft.setRotation(1)` and `ts.setRotation(1)`. Valid touch: `p.z >= 500 && p.z < 3900`.
- Serial line `addApbChangeCallback(): duplicate func` at boot is harmless (shared HSPI begun twice).
- SPI 80 MHz works (HSPI pins 12/13/14/15 are native IO_MUX). Per-env override:
  `build_unflags = -D SPI_FREQUENCY=40000000` + `-D SPI_FREQUENCY=80000000`. Fallback 55 MHz.
- RAM: ~320 KB total, no PSRAM. Two 240x160 RGB565 buffers (2x75 KB) leave ~190 KB free.
- `tone()` (core 2.x) queues notes into a background task (128 deep) → whole jingles are
  non-blocking; can't be cancelled, so gate low-priority sfx with a busy-until timestamp.

## Recipe for a new example
1. `mkdir -p examples/<name>/{src,include,tools,docs}`.
2. Add env to `platformio.ini` (copy `[env:pokemon_kalos]`): `build_src_filter = +<../examples/<name>/src/main.cpp>`,
   `-I examples/<name>/include`, SPI override.
3. Reuse the engine instead of drawing straight to the TFT:
   - Copy `examples/pokemon-kalos/include/gfx.h` (240x160 framebuffer: blit with flip/scale/tint/white-flash,
     dithered gradients, ellipses, lines, panels, 5x8 font text with shadow). Pure C++.
   - Copy hardware layer pieces from `examples/pokemon-kalos/src/main.cpp`: `pushFrame()` (diffs against the
     previous frame, pushes only changed rects at 2x → 30 fps), single/double tap detector (`pollTouch`/`takeTap`),
     `plat_sfx` note tables, `plat_led`, fixed 30 Hz tick loop.
   - Keep game/animation logic in a header with no Arduino deps (like `game.h`) exposing
     `game_init / game_tick(Tap) / game_render`, so the simulator can run it.
4. Art: generate it, don't hand-type arrays.
   - `tools/artlib.py`: build sprites from shapes (ellipse/poly/thick line) at 4x supersample; auto outline,
     separation lines between parts, cel shading, then stamp detail pixels (eyes). Small sprites
     (trainer, icons) use `from_ascii()` with a palette dict.
   - `tools/make_art.py` writes `include/art.h` (8-bit indexed pixels + RGB565 palette per sprite, index 0
     = transparent, font, map). Generation must stay deterministic (fixed seeds).
5. Verify visually BEFORE flashing (this is what made the graphics good):
   - `python3 tools/preview.py out.png` → contact sheet of sprites; Read the PNG, fix, repeat.
   - `tools/sim.cpp` runs the real game code on the Mac, dumps 2x PPM frames and logs state changes
     (`./sim OUT TICKS EVERY 300:s 400:d` injects taps). Paste frames into a sheet with Pillow and Read it.
   - Use the scratchpad dir for frames, never the repo.
6. Build, flash, then read serial for ~20 s (reset via RTS with pyserial from `.venv/bin/python`):
   expect heap print + `[ESP32-035] NN fps`. The user must confirm the screen and touch by eye.
7. Docs: `examples/<name>/README.md` (controls table, features, build, how to edit art) + screenshot in
   `examples/<name>/docs/screenshot.png`; add the example to root `README.md` (tree, env table, flash
   command, Screenshots gallery table).
   - Screenshot ANY example (even ones drawing straight to TFT_eSPI) with the emulator:
     `tools/cyd-emu/shoot.sh examples/<name> examples/<name>/docs/screenshot 6000` → `..._6000.png`
     (rename to `screenshot.png`). Runs real TFT_eSPI against stub Arduino/SPI headers and decodes the
     ST7796 command stream. Read the PNG before publishing — it caught a real colour bug in Mario.
   - Gotcha it revealed: `pushImage()` with normal RGB565 arrays needs `tft.setSwapBytes(true)`,
     otherwise sprite colours come out byte-swapped on the real board.
8. Don't commit unless asked.

## Conventions the user liked
- Display-first: self-playing demo loop; any tap switches to play mode; 30 s idle → back to demo.
- Controls: 1 tap = one action, 2 taps = another (300 ms double-tap window). Nothing more complex.
- Demo mode silent; sound only in play mode. LED flashes match on-screen colours.
- Text/dialogue auto-advances — never require taps to read.
- Copyrighted franchises: hand-made fan-tribute art only (no ripped assets/ROMs/music), note it in README.
