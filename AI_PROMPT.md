# 🤖 AI prompt for this repository

Copy everything below the line into your AI assistant (Claude, ChatGPT, Gemini, Copilot, ...), then
ask your question or describe what you want to build. If the assistant can read files, also let it
open `README.md`, `CLAUDE.md` and `platformio.ini`.

---

You are helping me work on the repository **ESP32-035-CYD-3.5-display**
(https://github.com/Novice130/ESP32-035-CYD-3.5-display): a hardware guide plus standalone example
projects for the **ESP32-035** board, a 3.5" "Cheap Yellow Display" (also sold as Sunton
ESP32-3248S035R, TZT ESP32 3.5 Inch LCD, MSP3520). Read these facts before suggesting any code.

## Hardware (this is NOT the classic 2.8" CYD)

- MCU: ESP32-WROOM-32 (`board = esp32dev`, Arduino framework), 4 MB flash, ~320 KB RAM, **no PSRAM**.
  USB-C through a CH340C USB-serial chip; uploads need no BOOT button.
- Display: **ST7796**, 480x320 (some panel batches are ILI9488: then use `-D ILI9488_DRIVER=1` and
  27 MHz SPI). TFT_eSPI is configured entirely by `build_flags` in `platformio.ini`
  (`USER_SETUP_LOADED`, `USE_HSPI_PORT`, `TFT_WIDTH=320`, `TFT_HEIGHT=480`).
  - HSPI pins: MOSI 13, MISO 12, SCLK 14, CS 15, DC 2, RST -1 (tied to EN).
  - **Backlight GPIO 27, active HIGH.** Code written for the 2.8" CYD (backlight 21) leaves the screen black.
  - Landscape = `tft.setRotation(1)`; portrait = `setRotation(0)` (used by noc-dashboard).
  - SPI 40 MHz by default; 80 MHz works (HSPI pins are native IO_MUX), fallback 55 MHz.
  - `pushImage()` with normal RGB565 arrays needs `tft.setSwapBytes(true)`, or colours come out byte-swapped.
- Touch: **XPT2046, resistive, on the same HSPI bus**, CS 33, IRQ 36 (examples poll, IRQ unused).
  - `SPIClass touchSPI(HSPI); touchSPI.begin(14, 12, 13, 33); ts.begin(touchSPI); ts.setRotation(1);`
    (or read it through TFT_eSPI with `TOUCH_CS=33`, as cloud-game does).
  - Valid press: `p.z >= 500 && p.z < 3900`. Raw range ~200..3800.
  - **The owner's touch glass is cracked**: it sometimes reads a permanent fake press near raw
    ~1740,3520 with z ≈ 4099. Never trust touch alone; give important actions a light-sensor fallback.
  - Serial line `addApbChangeCallback(): duplicate func` at boot is harmless (HSPI begun twice).
- RGB LED: R 4, G 16, B 17, **active LOW** (LOW = on).
- Speaker: GPIO 26 (amplifier; also the ESP32 DAC channel used by video-clip for audio).
  `tone()` (core 2.x) queues notes in a background task (128 deep), so jingles are non-blocking but
  can't be cancelled: gate low-priority sound effects with a busy-until timestamp.
- Light sensor (LDR): GPIO 34 (ADC1). Covering it with a fingertip is used as a button.
  With `analogSetPinAttenuation(34, ADC_0db)` the room reads ~0 and a fingertip ~60-500 (reading rises
  when covered); track the room level slowly and treat a rise above it as "covered".
- MicroSD: separate VSPI bus, CS 5, MOSI 23, SCLK 18, MISO 19 (no example uses it yet).
- RAM budget: two 240x160 RGB565 framebuffers (2 x 75 KB) leave ~190 KB free. Wi-Fi adds ~500 KB
  flash and ~120 KB RAM.

## Toolchain, build and flash

- PlatformIO. On the owner's Mac it lives in the repo venv: `.venv/bin/pio` (not on PATH). Elsewhere,
  use `pio` from the PlatformIO VS Code extension.
- Build: `pio run -e <env>` · Flash: `pio run -e <env> -t upload` · Serial: `pio device monitor -e <env>` (115200 baud).
- `platformio.ini` pins `upload_port`/`monitor_port` to the owner's Mac (`/dev/cu.usbserial-11110`).
  On another computer pass `--upload-port <port>` (e.g. `COM3`) or remove those lines.
- Host tools: Python 3 + Pillow (no numpy needed for art scripts), `clang++` for desktop simulators.
- `tools/cyd-emu/shoot.sh examples/<name> <out-prefix> <ms>[,<ms>...] [tap-ms ...]` renders any
  example's real firmware to PNG on the computer (no board needed). Look at the PNG before flashing.

## Repository layout

```
README.md            board guide, pinout, example table, screenshots
CLAUDE.md            detailed notes for AI assistants (read it if you can)
platformio.ini       one [env:...] per example, shared [env] with all display/touch flags
src/main.cpp         env "cyd": hardware diagnostics (pin info, LDR value, tap = LED colour + beep)
tools/cyd-emu/       desktop emulator for screenshots
examples/<name>/     one standalone project each: src/, include/, tools/, docs/, README.md
docs/                the website (GitHub Pages): Screen Mirror installer + USB sharing page,
                     firmware/<name>/manifest.json + merged .bin for ESP Web Tools
```

## Examples (env name: purpose)

| Folder | Env | Purpose |
| :--- | :--- | :--- |
| (root) `src/` | `cyd` | Hardware diagnostics and pinout test |
| `examples/mario-arcade/` | `mario_arcade` | Mario-style platformer tribute, 32 levels; plays itself, 3 touch buttons (JUMP/FIRE/RUN) take over; LDR = backup JUMP |
| `examples/pokemon-kalos/` | `pokemon_kalos` | Pokémon X/Y-style tribute (route walk + battles) in a 240x160 framebuffer; plays itself, 1 tap / 2 taps to play |
| `examples/video-clip/` | `video_clip` | Looping video with sound from a flash partition (JPEG frames + 8-bit DAC audio), or streamed over Wi-Fi; custom `partitions.csv` |
| `examples/cloud-game/` | `cloud_game` | Thin client: a Flappy tribute runs on a server (Sipeed NanoKVM) and streams frames over Wi-Fi; LDR or tap = flap |
| `examples/screen-mirror/` | `screen_mirror` (`screen_mirror_web` = USB-only build for the website) | Mirror a computer screen to the board: website in `docs/` (ESP Web Tools install + Web Serial over USB, https://robo.learnnovice.com/) or Wi-Fi from a Mac with tools/mirror.sh |
| `examples/manga-aura/` | `manga_aura` | Anime eyes + speedlines animation |
| `examples/neo-tokyo-synthwave/` | `neo_tokyo_synthwave` | Synthwave sunset with a scrolling neon grid |
| `examples/rasengan-vortex/` | `rasengan_vortex` | Swirling particle energy ball |
| `examples/matrix-rain/` | `matrix_rain` | Falling green "code rain" |
| `examples/noc-dashboard/` | `noc_dashboard` | Portrait network status dashboard (server cards, LDR %, uptime); optional Wi-Fi |

The small animations (manga, synthwave, rasengan, matrix) draw straight to TFT_eSPI in one short
`src/main.cpp`: the easiest place for a beginner to change colours and speeds.

## Recipe for a new example

1. `mkdir -p examples/<name>/{src,include,tools,docs}`.
2. Add an env to `platformio.ini` by copying `[env:pokemon_kalos]`:
   `build_src_filter = +<../examples/<name>/src/main.cpp>`, `-I examples/<name>/include`, and the
   SPI override (`build_unflags = -D SPI_FREQUENCY=40000000` + `-D SPI_FREQUENCY=80000000`).
3. For games/animations reuse the engine instead of drawing straight to the TFT:
   copy `examples/pokemon-kalos/include/gfx.h` (240x160 framebuffer, sprites, text, gradients) and the
   hardware pieces from its `src/main.cpp` (`pushFrame()` that pushes only changed areas at 2x for
   30 fps, `pollTouch`/`takeTap` tap detector, sound tables, LED, fixed 30 Hz loop). Keep game logic
   in an Arduino-free header (`game_init / game_tick / game_render`) so a desktop simulator can run it.
4. Generate art with Python scripts (`tools/make_art.py` -> `include/art.h`, deterministic seeds);
   don't hand-type pixel arrays.
5. Check visually before flashing (simulator frames or `tools/cyd-emu/shoot.sh`).
6. Build, flash, read serial for ~20 s (expect a heap print and an fps line); the human confirms the
   screen and touch by eye.
7. Docs: `examples/<name>/README.md` (controls, features, build, how to edit) + `docs/screenshot.png`,
   and add the example to the root `README.md` (tree, env table, flash command, screenshot table).

## Conventions

- Display-first: a self-playing demo loop; any tap switches to play mode; ~30 s idle goes back to demo.
- Controls stay simple: 1 tap = one action, 2 taps (300 ms window) = another. Add an LDR-cover fallback.
- Demo mode is silent; sound only while playing. LED flashes match on-screen colours.
- Text auto-advances; never require taps to read.
- Copy the hardware setup (pins, rotation, backlight, touch) from an existing example rather than
  guessing.

## Rules

- **Never commit Wi-Fi names, passwords or private IPs.** Secrets go in `examples/<name>/include/config.h`,
  which is git-ignored (`**/config.h`). Commit only `config.example.h` with empty values; code uses
  `#if __has_include("config.h") ... #else #include "config.example.h"`. The ESP32 only joins 2.4 GHz Wi-Fi.
- **Copyrighted content stays out of the repo**: no ripped game sprites, ROMs, music or video. Fan
  tributes use hand-made/generated art and say so in their README. Video clips stay in the git-ignored
  `examples/video-clip/clips/`.
- Keep each example standalone; don't change another example's code while working on one.
- Don't commit or push unless the human asks. Write temporary frames/screenshots outside the repo.
- Explain which env to upload and what the human should expect to see on the screen.

## If you are helping a grade 6 student

- Use short sentences and everyday words. Explain a technical word the first time (for example:
  "upload = send your program to the board through the USB cable").
- Change one small thing at a time (a colour, a speed, a word on screen), then have them upload and
  **check that they can see the change on the board** before moving on.
- Tell them exactly which file to open, which line to find, and what to type, and show the line
  before and after the change.
- Prefer the simple animation examples for first experiments. Colours use
  `tft.color565(red, green, blue)` with each number from 0 to 255.
- If something breaks, help them undo the last change first, then try again.
- Remind them not to type real Wi-Fi passwords into files they share, and to ask a teacher for help
  with Wi-Fi, servers or anything that needs installing.
