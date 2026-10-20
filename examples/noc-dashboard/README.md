# 📊 NOC IT Infrastructure & System Diagnostics Monitor

<img src="docs/screenshot.png" alt="NOC Dashboard screenshot" width="320">

## 🧒 Start here (for students)

- **What you will see:** a control-room style status screen, shown upright (turn the board so the
  long side is up and down). It lists four computers and whether they answer, plus how bright the
  room is and how long the board has been on. Tap **Info >** at the top right for a second screen.
  Covering the light sensor with your fingertip makes the LED flash a new colour.
- **You need:** the board, a USB-C cable and a computer set up as in the
  [main Start here](../../README.md#-start-here-for-students). Wi-Fi (wireless internet) is
  optional: without it the board just shows the screen in an offline demo.
- **Steps:**
  1. Open this whole project folder in VS Code.
  2. Plug in the board with the USB cable.
  3. In PlatformIO, click **Project Tasks → env:noc_dashboard → Upload**.
  4. When it says `SUCCESS`, the dashboard appears.
- **Try changing:** copy `include/config.example.h` to a new file `include/config.h`, change
  `NOC_TITLE` to your own title (like `"ROOM 6B MONITOR"`), and upload again. You can also put your
  Wi-Fi name and password there; `config.h` is kept off GitHub so they stay private.

## ✨ Features (for the curious)

Dual-screen enterprise network monitor featuring:
- 4 customizable server / infrastructure cards showing Name, IP, Role, and Status badge.
- Live ambient room light percentage from the onboard LDR (GPIO 34).
- Real-time uptime counter & Free Heap RAM telemetry.
- Interactive touch testing pad with live coordinate inspection.

---

🤖 Working with an AI assistant? Give it [AI_PROMPT.md](../../AI_PROMPT.md).
