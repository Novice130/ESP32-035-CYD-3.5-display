# 🌀 Rasengan Ki Energy Vortex for ESP32-035

<img src="docs/screenshot.png" alt="Rasengan Vortex screenshot" width="480">

## 🧒 Start here (for students)

- **What you will see:** a glowing blue-and-white ball of energy in the middle of the screen, with
  90 tiny dots swirling around it and rings that keep growing outwards, like an energy attack from
  an anime. The LED flickers green and light blue. It runs by itself; there is nothing to press.
- **You need:** the board, a USB-C cable and a computer set up as in the
  [main Start here](../../README.md#-start-here-for-students).
- **Steps:**
  1. Open this whole project folder in VS Code.
  2. Plug in the board with the USB cable.
  3. In PlatformIO, click **Project Tasks → env:rasengan_vortex → Upload**.
  4. When it says `SUCCESS`, the energy ball starts spinning.
- **Try changing:** in `src/main.cpp`, the colour of the ball is `tft.color565(0, 200, 255)`. The
  three numbers are how much red, green and blue to mix (each 0 to 255). Try
  `tft.color565(255, 80, 0)` for a fire ball, then upload again.

## ✨ Features (for the curious)

3D particle physics visualization featuring:
- 90+ high-speed particles swirling in a dense 3D spherical vortex.
- Concentric glowing energy rings rotating at opposing angles.
- Expanding shockwave rings pulsing outward and dissipating.
- Dynamic pulsating white/cyan energy core.

---

🤖 Working with an AI assistant? Give it [AI_PROMPT.md](../../AI_PROMPT.md).
