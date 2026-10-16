# 🍄 Super Mario Bros Autonomous Arcade for ESP32-035 (CYD 3.5")

[![PlatformIO](https://img.shields.io/badge/PlatformIO-Supported-orange.svg)](https://platformio.org/)
[![Display](https://img.shields.io/badge/Display-3.5%22%20480x320%20ST7796-blue.svg)]()
[![License](https://img.shields.io/badge/License-MIT-purple.svg)]()

Autonomous self-playing **Super Mario Bros** arcade screensaver with interactive touch controls designed for the **ESP32-035 (CYD 3.5" / ST7796 480x320)** display.


<img src="docs/screenshot.png" alt="Super Mario Bros Arcade screenshot" width="480">

---

## 🎮 Features
- **Pixel-Accurate NES 8-Bit Sprites**: Mario with 3-frame running animation and jump poses, walking Goombas with squished pancake stomp frames, Question Mark  blocks, and Green Warp Pipes.
- **Autonomous Playback**: Mario automatically runs, vaults over pipes, bonks mystery blocks for popping coins (+200 pts), and stomps Goombas (+100 pts).
- **Interactive Controls**:
  - **Tap Screen**: Mario executes an immediate high jump and fires a bouncing fireball!
  - **Optical Tap**: Cover the corner LDR photoresistor (GPIO 34) with your finger to trigger a jump and fireball burst without touching the screen.
- **Hardware Audio & RGB LED**:
  - Classic jump, stomp, fireball, and two-tone coin chimes (B5 -> E6) on **GPIO 26**.
  - Vibrant RGB LED flashes on **GPIO 4, 16, 17**.
