# 🖥️ Screen Mirror: your computer's screen on the ESP32-035

A live copy of a computer's screen (or one window, or one browser tab) on the 480x320 panel.
Two ways to use it:

- **🌐 Website** (any computer with Chrome or Edge, including Chromebooks): install, then share
  over the USB cable or, after a one-time Wi-Fi setup, over Wi-Fi with no cable, from
  **[the Screen Mirror website](https://novice130.github.io/ESP32-035-CYD-3.5-display/)**. Nothing to download or set up.
- **📶 Wi-Fi from a Mac**: `tools/mirror.sh` sends the Mac's screen over Wi-Fi, no cable needed.

> 💡 **Thank you, Abdul Samad Idowu!** This project was Abdul Samad's idea: he's a grade 6 student
> who asked whether the little yellow display could show what's on the computer. It can now.

## 🧒 Start here (for students)

The board becomes a tiny second screen. Whatever you share on the computer appears on it, live.

You need: the CYD board, a USB cable that carries data (some cables only charge), and a computer
with **Google Chrome** or **Microsoft Edge**.

1. Plug the board into the computer.
2. Open **https://novice130.github.io/ESP32-035-CYD-3.5-display/**
3. **Step 1 on the page:** click **Install Screen Mirror**, pick your board, click **Install**.
   Wait about a minute. (Only the first time.)
4. **Step 2:** click **Connect board** and pick your board.
5. **Step 3:** click **Share my screen** and choose a window, tab or whole screen. Done!

**No cable next time:** while connected with the cable, fill in **Set up Wi-Fi** (your home Wi-Fi name
and password, 2.4 GHz). The board shows its name, like **cyd.local**. From then on, power the board
from any USB charger, click **Connect over Wi-Fi** on the website and allow Chrome to *access devices
on your local network* the first time. School Wi-Fi often blocks this; the cable always works.

**Try this:** share a single browser tab with a big picture or a video, then switch between
*Smooth* and *Sharp* on the page and watch the difference on the board.

## 🔧 How it works (for the curious)

The computer does the hard part: it captures the screen, shrinks it to 480x320 and compresses
only what changed. The board decodes and draws. Over USB the web page sends the frames through the
cable ([Web Serial](https://developer.mozilla.org/docs/Web/API/Web_Serial_API)); over Wi-Fi the
Mac script sends them through your network.

```
Mac: capture ─▶ fit into 480x320 ─▶ diff with last frame ─▶ JPEG of the changed rectangle ──Wi-Fi──▶ CYD: decode ─▶ panel
   (2 threads)      + mouse pointer      (nothing changed = nothing sent)   (full or half res)  ◀── ack ──  (core 0 receives, core 1 draws)
```

Full-screen video plays at **~28 fps**, close to the panel's limit (see below), and the picture is
full 480x320 whenever it isn't moving much.

## 🌐 The website (USB)

[`docs/index.html`](../../docs/index.html) and [`docs/mirror-worker.js`](../../docs/mirror-worker.js),
published with GitHub Pages at https://novice130.github.io/ESP32-035-CYD-3.5-display/. It works in Chrome and Edge on Windows, macOS, Linux and
ChromeOS (they have Web Serial; Safari and Firefox don't).

- **Install:** [ESP Web Tools](https://esphome.github.io/esp-web-tools/) flashes
  `docs/firmware/screen-mirror/screen-mirror.bin` (built from `[env:screen_mirror_web]`, which never
  reads a local `config.h`, so no Wi-Fi password can end up in it).
- **Capture:** the browser's own screen/window/tab picker (`getDisplayMedia`). The video track goes
  to a worker through `MediaStreamTrackProcessor`, so it keeps running at full speed while the
  page is behind other windows.
- **Send:** the worker applies the same rules as `mirror.py` (changed rectangle, auto half/full
  resolution, two frames in flight) and the page writes the frames to the cable at 2,000,000 baud.
- **Wi-Fi from the website:** the page saves the Wi-Fi name, password and a board name on the board
  over the cable (`'W'` message, kept in flash with `Preferences`). The board then runs a tiny HTTP
  server: `GET /hello`, `POST /frame` (body = the same 12-byte header + JPEG, answer `K`). Chrome 142+
  lets a secure page make these requests to `.local` names and private IP addresses once the visitor
  allows local network access ([Local Network Access](https://developer.chrome.com/blog/local-network-access));
  a page can't search the network, so it asks for the board's name. Measured: ~20-28 fps of
  full-screen half-resolution frames.
- While pictures come over the cable the board pauses Wi-Fi (`'U'` message): with the radio on, the
  2,000,000 baud line lost bytes. Wi-Fi comes back 15 s after the cable goes quiet.
- Memory is tight with Wi-Fi: frames are capped at 40 KB (the senders lower JPEG quality to fit).

Rebuild the installer firmware after changing the code:

```bash
pio run -e screen_mirror_web
B=.pio/build/screen_mirror_web
.venv/bin/python -m esptool --chip esp32 merge_bin -o docs/firmware/screen-mirror/screen-mirror.bin \
  --flash_mode dio --flash_freq 40m --flash_size 4MB 0x1000 $B/bootloader.bin 0x8000 $B/partitions.bin \
  0xe000 ~/.platformio/packages/framework-arduinoespressif32/tools/partitions/boot_app0.bin 0x10000 $B/firmware.bin
```

## 📶 Wi-Fi from a Mac

```bash
# once: Wi-Fi for the board, then flash (this build also keeps working over USB)
cp examples/screen-mirror/include/config.example.h examples/screen-mirror/include/config.h
# edit config.h: WIFI_SSID / WIFI_PASSWORD (a 2.4 GHz network)
pio run -e screen_mirror -t upload

# on the Mac (the first run sets up examples/screen-mirror/.venv with mss + Pillow)
examples/screen-mirror/tools/mirror.sh
```

The board's waiting screen shows its Wi-Fi address, and the Mac finds it as
`cyd.local` (mDNS). If `.local` names don't resolve on your network, pass the IP shown on the
screen: `mirror.sh 192.168.0.177`. Ctrl-C stops mirroring; the board goes back to waiting.

**macOS permission:** the app you run it from (Terminal, iTerm, VS Code...) needs **System
Settings → Privacy & Security → Screen Recording**. Without it you get only the wallpaper.

## 🎛️ What to mirror

| Command | Shows |
| :--- | :--- |
| `mirror.sh` | the MacBook's built-in display |
| `mirror.sh --list` | lists displays; then `mirror.sh --display 2` for another one (e.g. an external monitor) |
| `mirror.sh --follow` | 480x320 points around the mouse pointer: a moving magnifier, **text stays readable** |
| `mirror.sh --follow 960` | 960x640 points around the pointer, shrunk to fit |
| `mirror.sh --pick` | point at two corners of an area (a window, a video), press Enter after each |
| `mirror.sh --region X,Y,W,H` | a fixed area in points (`--pick` prints this so you can reuse it) |

| Option | Default | Effect |
| :--- | :--- | :--- |
| *(auto)* | on | small changes at full 480x320; when over 35% of the screen moves (video, scrolling) the change goes at 240x160 shown 2x, then the area is re-sent sharp 0.25 s after it calms down |
| `--fast` | off | always 240x160 shown 2x: blurry, fastest |
| `--sharp` | off | always full 480x320: sharpest, ~15 fps when the whole screen moves |
| `--quality N` | 75 | JPEG quality 20-95 (lowered automatically if a frame won't fit the board's 48 KB buffer) |
| `--fps N` | 60 | capture at most N times a second |
| `--no-pointer` | off | don't draw the mouse pointer (macOS leaves it out of captures; the script draws one) |

A whole 1470-point-wide screen squeezed into 480 pixels makes text tiny; use `--follow` or
`--pick` for anything you need to read. The picture keeps its shape, with black bars if the
area isn't 3:2 (an ultrawide display gets wide bars).

## ⏱️ Measured on this board

With a video playing full screen, so ~90% of the screen changes every frame:

| Mode | fps | per frame | board: receive / draw |
| :--- | :---: | :---: | :---: |
| auto (default) / `--fast` | 28 | 6.4 KB | 15 ms / 30 ms |
| `--sharp` | 15 | 19 KB | 53 ms / 66 ms |

**Why not more:** sending a full 480x320 picture to the panel takes 30.7 ms at 80 MHz, the
ESP32's top SPI speed, so ~32 fps is the ceiling whenever the whole screen changes; half-res
frames already draw in 30 ms. At full resolution, decoding (66 ms) and Wi-Fi into the ESP32
(~300 KB/s with the Arduino core's small TCP window) limit it to ~15 fps, which is why auto mode
drops to half resolution while a lot moves. Smaller changes are faster: typing, the pointer or a
video in a window send only the rectangle that changed, and a still screen sends nothing (just a
12-byte ping every 2 s). Delay from Mac to panel is about three frames (~100 ms).

## ✨ How it works

1. **Capture threads (Mac):** two threads take turns: `mss` grabs the area (~25 ms each), Pillow
   fits it into 480x320 and draws the pointer, whose position comes from CoreGraphics (macOS
   leaves it out of captures). The newest frame is always ready (~54 a second).
2. **Diff:** the sender compares that frame with the last one it sent and snaps the changed
   bounding box to 16-pixel JPEG blocks. Nothing changed → nothing sent.
3. **Pick resolution (auto):** a box over 35% of the panel goes at half resolution and is
   remembered as blurry; once nothing big has moved for 0.25 s, the blurry area is re-sent sharp.
4. **Send:** a 12-byte header (`'J'`, zoom, x, y, length) plus a baseline JPEG of just that box.
   Up to two frames may be un-acked, so the next one is already on its way while the board
   takes the last.
5. **Receive (CYD core 0):** a FreeRTOS task next to the Wi-Fi stack reads the frame into one of
   two 48 KB buffers and acks at once, so the Mac sends the next frame while this one is drawn.
6. **Draw (CYD core 1):** JPEGDEC decodes a few 16x16 blocks at a time, zooms them if needed and
   pushes them by DMA while it decodes the next blocks (same pipeline as `video-clip`).

Protocol (TCP port 8767, or the USB serial line at 2,000,000 baud; little endian). Over USB the
sender repeats `"CYMH"` until the hello arrives (the board skips any late copy, since every frame
starts with `'J'` or `'Q'`), and the board prints no log lines while a USB sender is active. For
headroom at 2,000,000 baud the UART interrupt runs on core 0 and empties the chip's 128-byte
buffer every 32 bytes (the core's default is 112). Measured: 80 s of full-screen frames over USB,
12-14.5 fps, no lost bytes. On connect the board sends
`"CYM1", u16 width, u16 height, u32 max JPEG bytes, u8 max zoom`. Then the Mac repeats
`u8 'J', u8 zoom, u16 x, u16 y, u16 0, u32 len, len bytes of JPEG`; the board answers each with
a one-byte `'K'` once it has it, and the Mac keeps at most two frames un-acked.
`len = 0` is a keep-alive and tag `'Q'` means the sender stopped; the board drops a Mac it hasn't heard from for 10 s, and a new
connection always replaces the old one, so restarting the script just works.

## Files

| File | Runs on | Purpose |
| :--- | :--- | :--- |
| `src/main.cpp` | CYD | USB + Wi-Fi/mDNS receive task, JPEG drawing |
| `include/config.example.h` | CYD | copy to `config.h` (git-ignored) for Wi-Fi; without it the board uses USB only |
| `../../docs/index.html`, `../../docs/mirror-worker.js` | browser | the website: install, connect, capture, send over USB |
| `tools/mirror.sh` | Mac | creates `.venv` (git-ignored) on first run, then runs `mirror.py` |
| `tools/mirror.py` | Mac | capture, diff, pick resolution, encode, send |

## Serial output

```
[WiFi] joined "my_network" as 192.168.0.177
[Mirror] listening on cyd.local:8767
[Mirror] Mac connected from 192.168.0.137
[ESP32-035] 28.4 fps, 6.4 KB/frame, receive 15.0 ms, draw 30.0 ms (89% of panel per frame)
```

No sound: the board's 8-bit speaker isn't worth streaming system audio to.

🤖 Working with an AI assistant? Give it [AI_PROMPT.md](../../AI_PROMPT.md).
