# 🖥️ cyd-emu — screenshots of any example without the board

Runs an example's real `main.cpp` on your computer. The real **TFT_eSPI** library is compiled
against small stand-in Arduino/SPI headers (`stubs/`), and every byte it sends goes to a tiny
model of the ST7796 panel (`emu.cpp`). The result is exactly what the screen would show,
including TFT_eSPI's built-in fonts, saved as a PNG.

```bash
# from the repo root (needs clang++, Python 3 + Pillow, and the PlatformIO libraries
# downloaded once with: .venv/bin/pio run -e cyd)
tools/cyd-emu/shoot.sh examples/matrix-rain out/matrix 3000,8000
#   -> out/matrix_3000.png, out/matrix_8000.png   (times are in ms since boot)

tools/cyd-emu/shoot.sh examples/mario-arcade out/mario 5000 2000 2600
#   extra numbers = scripted screen taps at those times
```

Notes:
- Time is simulated, so a 10 s capture finishes in a moment and gives the same result every run.
- WiFi always reports offline; background FreeRTOS tasks are not run.
- ESP-NOW sends go nowhere. To fake received packets, add `examples/<name>/tools/emu_feed.cpp`
  with an `emu_tick()` that calls `emu_espnow_receive()` (see `examples/parking-sensor`).
- The light sensor reads "bright room"; sound is ignored.
- A private `include/config.h` is never used (the build copies the example's headers without it),
  so real WiFi passwords or IP addresses can't end up in a screenshot.
- Portrait (`setRotation(0)`) and landscape (`setRotation(1)`) both work.
