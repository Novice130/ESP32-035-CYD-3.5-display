# 🟢 Digital Katakana Matrix Rain for ESP32-035

<img src="docs/screenshot.png" alt="Matrix Rain screenshot" width="480">

## 🧒 Start here (for students)

- **What you will see:** streams of green letters falling down the screen like rain, each with a
  bright white letter at the front, like the computer code in a sci-fi movie. The LED on the board
  blinks green. It runs by itself; there is nothing to press.
- **You need:** the board, a USB-C cable and a computer set up as in the
  [main Start here](../../README.md#-start-here-for-students).
- **Steps:**
  1. Open this whole project folder in VS Code.
  2. Plug in the board with the USB cable.
  3. In PlatformIO, click **Project Tasks → env:matrix_rain → Upload**.
  4. When it says `SUCCESS`, the rain starts.
- **Try changing:** in `src/main.cpp`, the trail colour is `tft.color565(0, 240, 80)`. The three
  numbers are how much red, green and blue to mix (each 0 to 255). Try `tft.color565(255, 0, 200)`
  for pink rain, then upload again.

## ✨ Features (for the curious)

Cyberpunk digital rain cascade featuring:
- 28 vertical columns of cascading Japanese Katakana glyph streams across 480px width.
- Brilliant white leading drop heads with phosphor green fading trails.
- Dynamic character mutations in the stream.

---

🤖 Working with an AI assistant? Give it [AI_PROMPT.md](../../AI_PROMPT.md).
