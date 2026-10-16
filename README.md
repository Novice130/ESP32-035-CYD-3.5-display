# 📺 ESP32-035 (CYD 3.5" / ESP32-3248S035R) Working Guide & Dashboard

[![PlatformIO](https://img.shields.io/badge/PlatformIO-Supported-orange.svg)](https://platformio.org/)
[![Display](https://img.shields.io/badge/Display-3.5%22%20480x320%20ST7796-blue.svg)]()
[![Touch](https://img.shields.io/badge/Touch-XPT2046%20Resistive-green.svg)]()
[![License](https://img.shields.io/badge/License-MIT-purple.svg)]()

> **⚠️ WARNING FOR MAKERS & AI CODING ASSISTANTS:**  
> Even though this board is often marketed as a "Cheap Yellow Display" (CYD), **it is NOT the same as the classic 2.8" CYD (`ESP32-2432S028R`)**.  
> If you upload standard 2.8" CYD code to this board, **the screen will stay completely black** because the backlight is wired to **GPIO 27** (instead of GPIO 21) and the touch controller shares the **HSPI bus** rather than using a separate SPI port.

---

## 📸 Hardware Identification

Look at the silkscreen marking on the back of your PCB:
- Right between the ribbon cable connector and the ESP32-WROOM module, you will see a silkscreen box that reads: **`ESP32-035`**.
- This board is sold on AliExpress/Alibaba under names such as **TZT ESP32 3.5 Inch LCD**, **Sunton ESP32-3248S035R**, or **MSP3520**.

| Front Display (480x320) | Back PCB (`ESP32-035` Silkscreen) |
| :---: | :---: |
| ![Display Front](docs/images/esp32_035_display_front.png) | ![PCB Back](docs/images/esp32_035_pcb_back.png) |

---

## ⚡ Key Differences: 2.8" CYD vs. 3.5" ESP32-035

| Specification / Pin | Classic 2.8" CYD (`ESP32-2432S028R`) | 3.5" Board (**`ESP32-035` / `3248S035R`**) |
| :--- | :--- | :--- |
| **Screen Resolution** | 320 × 240 (QVGA) | **480 × 320 (HVGA)** |
| **Display Controller** | ILI9341 / ST7789 | **ST7796** *(or ILI9488 on some panel batches)* |
| **Backlight Pin (BL)** | `GPIO 21` | **`GPIO 27` (Drives transistor Q2)** |
| **Touch Controller** | XPT2046 on separate VSPI (`25, 32, 39, 33`) | **XPT2046 on shared HSPI (`14, 12, 13, CS: 33, IRQ: 36`)** |
| **RGB LED** | Pins 4 (R), 16 (G), 17 (B) — Active LOW | **Pins 4 (R), 16 (G), 17 (B) — Active LOW** *(Identical)* |
| **Light Sensor (LDR)**| Analog ADC1 (`GPIO 34`) | **Analog ADC1 (`GPIO 34`)** *(Identical)* |
| **USB Interface** | Micro-USB (or dual Type-C on Rev 2) | **USB Type-C** (connected to CH340C serial chip) |

---

## 🛠️ Complete Pinout Reference Table

| Peripheral | Signal / Function | ESP32 GPIO Pin | Description |
| :--- | :--- | :---: | :--- |
| **TFT Display** | Backlight Control | **`GPIO 27`** | Active HIGH. Drives backlight driver transistor Q2 |
| | MOSI / SDI | **`GPIO 13`** | Hardware HSPI Data Out |
| | MISO / SDO | **`GPIO 12`** | Hardware HSPI Data In |
| | SCLK / SCK | **`GPIO 14`** | Hardware HSPI Clock |
| | Chip Select (CS)| **`GPIO 15`** | Dedicated LCD Chip Select (Active LOW) |
| | DC / RS | **`GPIO 2`** | Data / Command register select |
| | Reset (RST) | **`-1` / `EN`** | Connected to board system reset |
| **Touch (XPT2046)**| Chip Select (CS)| **`GPIO 33`** | Dedicated Touch Chip Select |
| | Interrupt (IRQ) | **`GPIO 36`** | Touch detect interrupt (`SENSOR_VP`) |
| | MOSI / MISO / CLK| **`13, 12, 14`** | Shares hardware HSPI bus with the display |
| **Onboard RGB LED**| Red Channel | **`GPIO 4`** | Active LOW (0 = ON, 1 = OFF) |
| | Green Channel | **`GPIO 16`** | Active LOW (0 = ON, 1 = OFF) |
| | Blue Channel | **`GPIO 17`** | Active LOW (0 = ON, 1 = OFF) |
| **Sensors & Audio**| LDR Light Sensor | **`GPIO 34`** | Ambient light CdS photoresistor (ADC1) |
| | Speaker | **`GPIO 26`** | Audio amplifier (NS4168 / FM8002A) |
| **MicroSD Slot** | TF SPI Bus | **`CS: 5, MOSI: 23, SCLK: 18, MISO: 19`** | Dedicated separate VSPI bus |

---

## 🎮 Example Repositories & Interactive Firmware Hub

The default firmware on this board boots into an interactive **Repository Launcher Hub** on the display. You can tap any card to launch it live, or explore each example repository directly in the codebase:

| Repository / Project | Type | Description | Direct Link |
| :--- | :---: | :--- | :---: |
| **Super Mario Bros Arcade** | `Game / Arcade` | Autonomous self-playing NES Mario runner. Tap screen or cover LDR to jump and shoot bouncing fireballs! Pixel-accurate 8-bit sprites and sound effects on GPIO 26. | [`examples/mario-arcade/`](examples/mario-arcade/) |
| **Manga Ultra Instinct** | `Animation / Anime` | Glowing anime eyes, dynamic radial action speedlines, menacing JoJo SFX (ゴゴゴ / ドドド), and crackling electric lightning arcs. | [`examples/manga-aura/`](examples/manga-aura/) |
| **Neo-Tokyo 2088 Synthwave** | `3D Wireframe` | 3D perspective infinite neon wireframe grid, retro anime sun with scanline cuts, and Mt. Fuji silhouette. | [`examples/neo-tokyo-synthwave/`](examples/neo-tokyo-synthwave/) |
| **Rasengan Ki Energy Vortex** | `3D Particles` | 90+ particles swirling in a 3D spherical vortex, concentric opposing rings, and pulsing energy shockwaves. | [`examples/rasengan-vortex/`](examples/rasengan-vortex/) |
| **Digital Katakana Matrix Rain** | `Cyberpunk` | 28 cascading vertical columns of Japanese Katakana code with glowing white lead drops and phosphor green trails. | [`examples/matrix-rain/`](examples/matrix-rain/) |
| **NOC Infrastructure Monitor** | `Telemetry / IoT` | Enterprise server and network monitor with live TCP probes, uptime counter, Free Heap RAM telemetry, and touch diagnostics. | [`examples/noc-dashboard/`](examples/noc-dashboard/) |

> **Navigation Tip:** In any demo, tap the **`< Hub`** button in the top-right corner (or place your finger over the LDR photoresistor on GPIO 34) to return to the Main Repository Hub immediately.

---

## 📱 Multi-Screen Dashboard Features

This codebase includes a ready-to-use dual-screen interactive monitor:

### Screen 0: Infrastructure Overview
- 4 customizable server / infrastructure cards showing Name, IP, Role, and Status badge.
- Live ambient room light percentage & raw ADC reading from the onboard LDR.
- Real-time uptime counter.
- **On-Tap RGB LED Flash**: The annoying continuous LED blinking is disabled. The LED flashes and cycles vibrant colors only when the screen or the bottom-right corner is tapped.
- Top-right **`System Info >`** button toggles to the diagnostics screen.

### Screen 1: Hardware & System Diagnostics
- Live Free Heap RAM & Minimum Free Heap monitoring.
- Board & SoC architecture specs (ESP32-D0WD-V3 @ 240MHz, 4MB Flash).
- Display & SPI bus telemetry (ST7796, HSPI @ 40MHz).
- Interactive touch testing pad with live (X, Y) coordinates and touch color indicator.
- Top-left **`< Back`** button returns to the overview dashboard.

---

## 🚀 Quick Start Guide

### 1. Clone the Repository
```bash
git clone https://github.com/Novice130/ESP32-035-CYD-3.5-display.git
cd ESP32-035-CYD-3.5-display
```

### 2. Configure Your Network / Server Settings
Copy the example configuration file:
```bash
cp include/config.example.h include/config.h
```
Edit `include/config.h` with your own server names, IPs, and optional WiFi credentials.  
*(Note: `include/config.h` is protected by `.gitignore` so your private credentials and internal IPs are never committed).*

### 3. Build & Upload with PlatformIO
Plug your `ESP32-035` into your computer using a USB Type-C data cable and run:

```bash
pio run -t upload
```

Or using the included Python helper script:
```bash
python3 flash_cyd.py
```

---

## ⚙️ `platformio.ini` Configuration Snippet

If you are incorporating this display into your own project, copy these `build_flags` into your `platformio.ini`:

```ini
[env:esp32-035]
platform = espressif32
board = esp32dev
framework = arduino
monitor_speed = 115200

lib_deps =
    bodmer/TFT_eSPI @ ^2.5.43
    https://github.com/PaulStoffregen/XPT2046_Touchscreen.git

build_flags =
    -D USER_SETUP_LOADED=1
    -D USE_HSPI_PORT=1
    -D ST7796_DRIVER=1
    -D TFT_WIDTH=320
    -D TFT_HEIGHT=480
    -D TFT_MISO=12
    -D TFT_MOSI=13
    -D TFT_SCLK=14
    -D TFT_CS=15
    -D TFT_DC=2
    -D TFT_RST=-1
    -D TFT_BL=27
    -D TFT_BACKLIGHT_ON=HIGH
    -D TOUCH_CS=33
    -D LOAD_GLCD=1
    -D LOAD_FONT2=1
    -D LOAD_FONT4=1
    -D LOAD_FONT6=1
    -D LOAD_FONT7=1
    -D LOAD_FONT8=1
    -D LOAD_GFXFF=1
    -D SMOOTH_FONT=1
    -D SPI_FREQUENCY=40000000
    -D SPI_READ_FREQUENCY=20000000
    -D SPI_TOUCH_FREQUENCY=2500000
```

> **Tip:** If your panel displays an inverted or white screen after uploading, your specific panel revision uses the `ILI9488` controller instead. Simply replace `-D ST7796_DRIVER=1` with `-D ILI9488_DRIVER=1` and set `-D SPI_FREQUENCY=27000000`.

---

## 📄 License
This project is licensed under the MIT License — feel free to use it in your own dashboards, Home Assistant setups, or IoT projects!
