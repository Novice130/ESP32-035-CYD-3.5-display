# 🎬 Video Clip Player for ESP32-035 (CYD 3.5")

[![PlatformIO](https://img.shields.io/badge/PlatformIO-Supported-orange.svg)](https://platformio.org/)
[![Display](https://img.shields.io/badge/Display-3.5%22%20480x320%20ST7796-blue.svg)]()
[![FPS](https://img.shields.io/badge/Speed-15%20fps%20%2B%20sound-green.svg)]()

Plays a short video clip with sound on the **ESP32-035 (CYD 3.5" / ST7796 480x320)**, looping
forever. A script on your computer turns any video into a `clip.bin` (JPEG frames + 8-bit audio),
and a second script writes it into its own area of the board's 4 MB flash. No SD card needed.

<img src="docs/screenshot.png" alt="Video clip player showing the built-in test pattern" width="480">

<sub>Screenshot: the synthetic test pattern from `make_clip.py --test-pattern`.</sub>

> **Only convert videos you have the rights to use** (your own footage, public-domain or suitably
> licensed clips). Keep source videos and `clip.bin` files out of this repository.

## 🧒 Start here (for students)

- **What you will see:** a short video with sound playing on the board, over and over. Tap once to
  pause, tap twice to mute, or cover the light sensor with your fingertip to skip ahead 4 seconds.
- **You need:** the board, a USB-C cable, and a Mac or Linux computer set up as in the
  [main Start here](../../README.md#-start-here-for-students). Making a clip also needs Python (step 2),
  so a teacher may want to help with that part.
- **Steps:**
  1. Upload the player: in PlatformIO, click **Project Tasks → env:video_clip → Upload**. The screen
     will say **"No video clip in flash"**. That is normal: there is no video on the board yet.
  2. Make a clip. The commands in "2. Make a clip on your computer" below turn a video into a file
     called `clip.bin`, or make a colourful test video with `--test-pattern` if you have no video.
  3. Send the clip to the board with `tools/flash_clip.sh` (see "3. Flash the clip" below). The board
     restarts and plays it.
- **Only use videos you are allowed to use**, like ones you filmed yourself.
- **Try changing:** in `src/main.cpp`, `SKIP_MS` is how far the light-sensor skip jumps (`4000`
  means 4 seconds). Try `10000` for 10-second skips.

---

## 🎮 Controls

| Touch | What it does |
| :--- | :--- |
| **1 tap** | Pause / resume (sound goes quiet while paused) |
| **2 taps** | Mute / unmute |
| **Cover the light sensor** | Skip 4 s ahead (each cover = one skip; wraps to the start at the end) |

`PAUSED`, `>> +4 s` and `MUTED` show in the black bar under the picture. A single tap is acted on once the
300 ms double-tap window has passed.

The light sensor (LDR, GPIO 34, on the front of the board) uses the same detector as the mario-arcade
backup JUMP: the room level is tracked slowly and a fingertip over the sensor (a reading at least
`LDR_COVER_RISE` above it) counts as one skip. The serial stats line prints `ldr N (room M)` so you
can tune `LDR_COVER_RISE` / `SKIP_MS` in `src/main.cpp`.

## 📡 Stream any length over Wi-Fi

The flash holds ~50 s at good quality. For whole videos, keep the clip on another machine and let
the CYD fetch it over Wi-Fi as it plays: the same `clip.bin`, made with `make_clip.py --budget 0x10000000`
(no size limit), served by the tiny [`clip_server`](https://github.com/Novice130/Sipeed_Examples/tree/main/cyd-stream)
(written for a Sipeed NanoKVM, but it's plain C++ for any Linux box).

1. Copy `include/config.example.h` to `include/config.h` (git-ignored) and set `WIFI_SSID`,
   `WIFI_PASSWORD` (2.4 GHz network) and `STREAM_HOST` (the server's IP). Build and flash.
2. At boot the CYD joins Wi-Fi, asks the server for the file, and plays it with the same clock, taps
   and light-sensor skip. Each frame is one request of a few KB; audio is fetched 4 KB (half a second)
   at a time with 128 ms queued in DMA, so short Wi-Fi hiccups don't reach the speaker.
3. No Wi-Fi or no server: it says so on serial and plays the flash clip. If it can't join, the serial
   log lists every network in range (`[WiFi]   "my_network"  -61 dBm  ch 9`).

Measured with a 4-minute clip at quality 70 (5.6 KB per frame): 15 fps, ~55 ms per frame including
the network fetch. Wi-Fi adds ~500 KB to the firmware and uses ~120 KB of RAM.

## ✨ How it works

- **Picture**: each frame is a baseline JPEG, shown at the biggest whole-number zoom that fits the
  screen: 240×136 at **2× (480×272)**, or 160×90 at **3× (480×270)** for long clips, with black bars
  above and below. The firmware reads a frame from flash into RAM, decodes
  it with [JPEGDEC](https://github.com/bitbank2/JPEGDEC) a few 16×16 blocks at a time, repeats each
  pixel 2 or 3 times and sends them over 80 MHz SPI **by DMA while the next blocks decode**. A frame takes about
  30 ms, so 15 fps leaves plenty of room.
- **Timing**: the frame shown is picked from the time since playback started, so the clip runs at
  its real speed. If a frame is ever late, the player skips ahead instead of slowing down.
- **Sound**: unsigned 8-bit mono PCM at 8 kHz goes through I2S into the ESP32's **built-in DAC on
  GPIO 26** (the speaker amplifier). It is fed by its own task on core 0 so decoding never starves
  it, and it follows the same clock as the picture. The DAC runs at 32 kHz with interpolation:
  asking the ESP32 for 8 kHz in DAC mode really runs at ~44 kHz.
- **Looping**: at the end the clip starts again from the first frame, picture and sound together.

## 🛠️ Build, make a clip, flash it

### 1. Flash the firmware (once)

```bash
pio run -e video_clip -t upload
pio device monitor -e video_clip     # shows fps
```

This also writes the partition table below. With no clip in flash yet, the screen says
**"No video clip in flash"** and tells you to run `tools/flash_clip.sh`.

### 2. Make a clip on your computer

`tools/make_clip.py` needs Python 3.10+ with PyAV, numpy and Pillow. Give the example its own venv
(the repo's top-level `.venv` is the PlatformIO one) and keep your videos and clips in `clips/`.
Both are git-ignored, so nothing copyrighted ends up in a commit. Run it from `examples/video-clip/`:

```bash
python3 -m venv .venv && .venv/bin/pip install av numpy pillow      # once
.venv/bin/python tools/make_clip.py clips/my_video.mp4 --start 12 --dur 30 -o clips/clip.bin --preview clips/preview.png
.venv/bin/python tools/make_clip.py --test-pattern --dur 10 -o clips/test.bin   # synthetic clip, no video needed
```

YouTube gives video and audio as separate files at most sizes; format 18 (360p) has both in one
file, which is plenty for 240x136: `yt-dlp -f 18 -P clips URL`. Otherwise pass the audio with `--audio`.

| Option | Default | Meaning |
| :--- | :---: | :--- |
| `--start SEC` / `--dur SEC` | whole video | Part of the video to use |
| `--audio FILE` | the video's own sound | Take the sound from a separate file |
| `--no-audio` | | Picture only |
| `--fps N` | 15 | Frame rate |
| `--width W --height H` | 240 × 136 | Decoded frame size, max 240 × 160. Zoom is automatic: 2× for 240 × 136, 3× for 160 × 90 |
| `--rate HZ` | 8000 | Audio sample rate |
| `--quality auto\|1-95` | auto | JPEG quality; `auto` picks the best one that still fits the flash |
| `--budget BYTES` | 0x2F0000 | Space available (the clip partition) |
| `--preview PNG` | | Contact sheet of frames decoded back from the file |

`tools/check_clip.py clip.bin` checks a finished file (header, index, every JPEG, audio, size).

How long a clip fits? The partition holds 3,080,192 bytes. At 15 fps with 8 kHz sound, a 51 s
clip of game footage fits at quality ~55 (about 3.4 KB per frame); anime with lots of effects
needs ~30 for 52 s and still looks clean at 2x. Shorter clips get higher quality automatically.

For longer clips, shrink the picture and let the firmware zoom it 3x. A 2.5-minute anime clip fits at
quality ~21 with `--width 160 --height 90 --fps 9 --rate 6000` (soft, but every scene reads clearly).
If a clip doesn't fit, `make_clip.py` tells you how many seconds would.

### 3. Flash the clip

```bash
tools/flash_clip.sh clips/clip.bin                  # default port /dev/cu.usbserial-11120
tools/flash_clip.sh clips/clip.bin /dev/cu.usbserial-XXXX
```

It checks the size and the `CLIP` header, then writes the file at `0x110000` with the repo's
esptool (`.venv/bin/python -m esptool`). The board resets and starts playing. The firmware isn't
touched, so you can swap clips without rebuilding, and reflash the firmware without re-sending
the clip.

## 💾 Flash layout (`partitions.csv`)

| Name | Type | SubType | Offset | Size |
| :--- | :--- | :--- | :--- | :--- |
| nvs | data | nvs | `0x9000` | `0x5000` |
| otadata | data | ota | `0xE000` | `0x2000` |
| app0 | app | ota_0 | `0x10000` | `0x100000` (1 MB) |
| clip | data | `0x40` | `0x110000` | `0x2F0000` (2.94 MB) |

## 📦 `clip.bin` format

Little endian. A 64-byte header, then the frame index, the JPEGs and the audio. Offsets are from
the start of the file (= start of the partition).

| Offset | Type | Field |
| :---: | :--- | :--- |
| 0 | `char[4]` | magic `CLIP` |
| 4 | `u16` | version = 1 |
| 6 | `u16` | width (240) |
| 8 | `u16` | height (136) |
| 10 | `u16` | fps × 100 (1500 = 15 fps) |
| 12 | `u32` | frame count N |
| 16 | `u32` | index offset: N+1 `u32` offsets, frame i = bytes `[idx[i], idx[i+1])` |
| 20 | `u32` | audio rate in Hz (8000), 0 = no audio |
| 24 | `u32` | audio offset |
| 28 | `u32` | audio bytes |
| 32 | `u8[32]` | zero padding |

Frames are baseline JPEG (not progressive), YCbCr 4:2:0, each starting 4-byte aligned. Audio is
unsigned 8-bit mono (128 = silence), the same length as the video.

## 🩺 Serial output

```
[Clip] 240x136 x2 @ 15.00 fps, 768 frames (51.2 s), biggest frame 6688 bytes
[Clip] audio 8000 Hz u8 (409600 bytes), DAC at 32000 Hz
[ESP32-035] Heap free: 267692 bytes (largest block 110580)
[ESP32-035] 15.0 fps (0 skipped, 30.0 ms/frame), audio 7987 Hz, ldr 0 (room 0)
[LDR] covered 146 (room 18)
[LDR] skip +4 s
```

- `esp_core_dump_flash: No core dump partition found!` at boot is harmless: this layout gives
  the clip the space a core dump partition would use.
- If the picture has noise or glitches, your panel may not handle 80 MHz SPI. In `platformio.ini`
  under `[env:video_clip]`, change `-D SPI_FREQUENCY=80000000` to `55000000`.

## 📁 Layout

```
video-clip/
├── src/main.cpp          # player: flash reads, JPEG decode + 2x/3x DMA push, DAC audio task, taps, LDR skip
├── partitions.csv        # 1 MB app + 2.94 MB clip partition
├── tools/make_clip.py    # video -> clip.bin (or --test-pattern)
├── tools/check_clip.py   # validate a clip.bin, render a frame like the board does
├── tools/flash_clip.sh   # write clip.bin to 0x110000
└── docs/screenshot.png
```

---

🤖 Working with an AI assistant? Give it [AI_PROMPT.md](../../AI_PROMPT.md).
