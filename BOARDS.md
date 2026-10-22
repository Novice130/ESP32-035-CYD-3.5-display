# BOARDS.md — the "Cheap Yellow Display" family and their pins

This repo was written for the **ESP32-035** (3.5", ST7796, 480×320). It's the same board Sunton
sells as **ESP32-3248S035R**: every pin matches. The most common CYD, though, is the **2.8"
ESP32-2432S028R**. This page lists the boards we know, what differs between them, and how to build
for each one.

Sources (checked October 2026):
[rzeldent board files](https://github.com/rzeldent/platformio-espressif32-sunton) ·
[witnessmenow CYD repo](https://github.com/witnessmenow/ESP32-Cheap-Yellow-Display) (PINS.md, Variants, CYD2USB) ·
[openHASP configs](https://github.com/HASwitchPlate/openHASP/tree/master/user_setups/esp32) ·
[Random Nerd Tutorials 2.8" pinout](https://randomnerdtutorials.com/esp32-cheap-yellow-display-cyd-pinout-esp32-2432s028r/) ·
[mischianti 3.2" pinout](https://mischianti.org/esp32-2432s032-3-2%e2%80%b3-sunton-cheap-yellow-display-v2-pinout-datasheet-schema-and-specs/)

## The same on every SPI CYD (ESP32-WROOM)

| What | Pins |
| :--- | :--- |
| Display bus (HSPI) | MOSI 13, MISO 12, SCLK 14, CS 15, DC 2, RST −1 (tied to the board's reset) |
| RGB LED | red 4, green 16, blue 17, **active LOW** (LOW = on) |
| Speaker (amplifier) | GPIO 26 |
| Light sensor (LDR) | GPIO 34 |
| SD card (VSPI) | CS 5, MOSI 23, SCLK 18, MISO 19 |
| BOOT button | GPIO 0 |
| Resistive touch IRQ | GPIO 36 |
| Capacitive touch ("C" models) | I2C SDA 33, SCL 32, RST 25, INT 21 |

## What changes from board to board

| Board | Screen | Driver | Backlight | Touch | Colour notes | PlatformIO env (screen mirror) |
| :--- | :--- | :--- | :---: | :--- | :--- | :--- |
| **ESP32-035 / 3248S035R** (ours) | 3.5" 480×320 | ST7796 | **27** | XPT2046 **sharing the display bus**, CS 33 | none | `screen_mirror` |
| 3248S035C | 3.5" 480×320 | ST7796 | 27 | GT911 capacitive (I2C) | none | `screen_mirror` (touch not used) |
| **2432S028R** (most common, 1 micro-USB) | 2.8" 320×240 | ILI9341 | **21** | XPT2046 on **its own pins**: CLK 25, MOSI 32, MISO 39, CS 33, IRQ 36 | none | `screen_mirror_28` |
| 2432S028R "2 USB" (micro-USB + USB-C) | 2.8" 320×240 | ST7789 (some lots: ILI9341 with inverted colours) | 21 | same as above | colours inverted / swapped unless `TFT_RGB_ORDER=TFT_BGR` + `TFT_INVERSION_OFF` | `screen_mirror_28_2usb` |
| 2432S024R | 2.4" 320×240 | ILI9341 | **27** | XPT2046 sharing the display bus, CS 33 | `TFT_INVERSION_OFF` | `screen_mirror_24` |
| 2432S024C | 2.4" 320×240 | ILI9341 | 27 | CST820 capacitive (I2C 0x15) | as 024R | `screen_mirror_24` (touch not used) |
| 2432S028R v3 (box says "7789") | 2.8" 320×240 | ST7789 (RGB order) | 21 | same as the 2.8" above | RGB order, no inversion | `screen_mirror_28_st7789` |
| Guition JC2432W328 (2.8" look-alike) | 2.8" 320×240 | ST7789 (RGB order) | **27** | CST816/820 capacitive (I2C) | no inversion | `screen_mirror_jc2432w328` |
| 2432S032R/C | 3.2" 320×240 IPS | ST7789 | 27 | R: unconfirmed (shared bus or own pins) · C: GT911 | IPS: inversion on | `screen_mirror_32` |
| 2432S022C | 2.2" | ST7789 on an **8-bit parallel** bus | 0 | CST816S | different bus: needs other code | — |
| 8048S043 / 050 / 070, 4827S043 | 4.3–7" 800×480 | RGB parallel on **ESP32-S3** | 2 | GT911 | different chip and bus | — |

Notes:
- **Backlight is the #1 surprise:** GPIO 21 on the 2.8", GPIO 27 on everything else. Wrong pin =
  black screen even though the program runs (serial log looks fine).
- **Touch wiring differs:** on the 2.8" it has its own SPI pins (use a second `SPIClass` on
  25/39/32/33); on the 3.5" and 2.4" it shares the display bus (our `touchSPI.begin(14, 12, 13, 33)`).
  Capacitive "C" boards use I2C instead.
- The 2-USB 2.8" board's USB-C socket has no CC resistors: a USB-C to USB-C cable doesn't power
  it. Use USB-A to USB-C, or the micro-USB socket.
- SPI speed: 80 MHz works on our 3.5" (repo experience); sources use 55 MHz for the 2.8"/2.4" and
  up to 65 MHz on the 3.5"/3.2". The 2.8"/2.4" envs use 55 MHz.
- Some sources disagree on the 2.4" green/blue LED order and on the 3.2" touch bus; marked
  unconfirmed above. If colours or touch look wrong on one of those, check those first.

## Colours wrong on a board? (no rebuild needed)

The same model number ships with different panel lots (inverted IPS panels, BGR panels). The
screen mirror has a **Fix colours** button on the website: it sends `'I'` to the board, which
cycles through four colour fixes and saves the choice in NVS (`Preferences` key `colours`):
normal → inverted → red/blue swapped → both → normal. Inversion uses `tft.invertDisplay()`
relative to the build's `TFT_INVERSION_ON`; the red/blue swap is done in software on each JPEG
block and on the waiting-screen text colours. So the builds above only need the right **driver,
size and backlight pin**; colour quirks are fixable by the student.

## Building for another board

The screen mirror reads everything board-specific from the build flags:

| Flag | Used for |
| :--- | :--- |
| `TFT_WIDTH` / `TFT_HEIGHT` (portrait) | panel size → `PANEL_W` × `PANEL_H` in landscape; the page and `mirror.py` learn it from the board's hello |
| `TFT_BL` | backlight pin |
| `TOUCH_CS` | kept HIGH so the touch chip stays off the bus |
| `ILI9341_2_DRIVER` / `ST7789_DRIVER` / `ST7796_DRIVER`, `TFT_RGB_ORDER`, `TFT_INVERSION_*` | TFT_eSPI driver and colours |

```bash
.venv/bin/pio run -e screen_mirror_28 -t upload       # 2.8", one USB socket
.venv/bin/pio run -e screen_mirror_28_2usb -t upload  # 2.8", two USB sockets
.venv/bin/pio run -e screen_mirror_24 -t upload       # 2.4"
.venv/bin/pio run -e screen_mirror_28_st7789 -t upload   # 2.8" v3 ("7789" on the box)
.venv/bin/pio run -e screen_mirror_jc2432w328 -t upload  # Guition 2.8"
.venv/bin/pio run -e screen_mirror_32 -t upload       # 3.2"
examples/screen-mirror/tools/build-web-firmware.sh    # rebuild all website installers
```

To add a board: copy the `[env:screen_mirror_28]` block in `platformio.ini`, change the driver,
size, backlight and colour flags, add a `_web` twin, add it to `build-web-firmware.sh` and to
the board choice in `docs/index.html`.

**Not tested on real hardware yet:** the 2.8", 3.2" and 2.4" builds compile and use the pins above, but
only the 3.5" has been run. If a 2.8" shows a white or black screen, check the backlight pin and
the driver; if colours are inverted, try the other 2.8" build.

The other examples (games, animations) still assume the 3.5" 480×320 panel: their 240×160
framebuffer is drawn at 2× scale. On a 320×240 panel it would need 1× scale or a different
layout, so they aren't ported yet.
