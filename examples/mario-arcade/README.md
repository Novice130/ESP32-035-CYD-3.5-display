# 🍄 Super Mario Bros Arcade (fan tribute) for ESP32-035 (CYD 3.5")

[![PlatformIO](https://img.shields.io/badge/PlatformIO-Supported-orange.svg)](https://platformio.org/)
[![Display](https://img.shields.io/badge/Display-3.5%22%20480x320%20ST7796-blue.svg)]()
[![FPS](https://img.shields.io/badge/Speed-up%20to%2030%20fps-green.svg)]()

A side-scrolling plumber platformer for the **ESP32-035 (CYD 3.5" / ST7796 480x320)**. When nobody
touches it, it plays itself. Press any of the three buttons on the right edge and you take over on
the spot.

<img src="docs/screenshot.png" alt="Super Mario Bros Arcade screenshot" width="480">

> **Fan tribute.** All art, levels and sounds here were made for this project (no ripped game
> assets, no ROMs, no music from the games). Mario and related characters are trademarks of
> Nintendo; this is a non-commercial fan project.

## 🧒 Start here (for students)

- **What you will see:** a little plumber running to the right through 32 levels, jumping over holes
  and stomping enemies. At first the game plays itself. Press one of the three round buttons on the
  right edge of the screen (**JUMP**, **FIRE**, **RUN**) and you take control.
- **No working touch screen?** Cover the light sensor on the front of the board with your fingertip
  to jump.
- **You need:** the board, a USB-C cable and a computer set up as in the
  [main Start here](../../README.md#-start-here-for-students).
- **Steps:**
  1. Open this whole project folder in VS Code.
  2. Plug in the board with the USB cable.
  3. In PlatformIO, click **Project Tasks → env:mario_arcade → Upload**.
  4. When it says `SUCCESS`, watch the screen and press a button to play.
- **Try changing:** in `include/game.h`, `JUMP_VEL` sets how hard the plumber jumps (it is `112`).
  Try `140`, upload again, and see how high he goes. The self-playing demo may start making mistakes
  because it was tuned for the normal jump.

---

## 🎮 How to play

The plumber **always walks right on his own**. You steer him with three buttons on the right edge:

| Button | What it does |
| :--- | :--- |
| **⬆ JUMP** (top) | Jump. Tap for a small hop, **hold longer to jump higher**. Holding it while you stomp an enemy bounces you higher. |
| **🔥 FIRE** (middle) | Throw a fireball. Works in every form; after a **Fire Flower** you can have two in the air and throw faster. |
| **» RUN** (bottom) | Switch between walking and running. The button turns gold while running. Running also jumps a bit further. |

- **Demo mode**: when the game starts it plays itself (silently) and the buttons light up when the
  demo player "presses" them. Press **any** button and control switches to you right where you are.
  That press also counts (pressing JUMP jumps).
- If you don't press a button for **5 minutes**, the demo player takes over again.
- **Backup JUMP: cover the light sensor** (the LDR on the front of the board) with a fingertip.
  Covering it jumps, keeping it covered jumps higher, and a quick cover takes over the demo. This is
  for when the touch panel stops reading fingers (see Troubleshooting). While the board detects
  that fault, the plumber also **throws fireballs by himself** at enemies ahead (the FIRE ring turns
  gold), since the FIRE button can't be pressed then.
- Sound is only on while you play, so it can sit on a desk as a display.

## ✨ What's inside

- **32 levels in 8 worlds.** Every world has the same four stages: **x-1** daytime hills,
  **x-2** underground cave with a brick ceiling, **x-3** sunset treetops over bottomless gaps,
  **x-4** night castle grounds with lava pits. World 1 is hand-designed; worlds 2-8 are built by a
  seeded generator that adds more holes, enemies, piranha pipes and floating steps each world.
  After 8-4 it starts over at 1-1. Every level was checked by letting the demo player clear it.
- **Real platformer rules**: lives, holes and lava, a 300 timer, a flagpole (higher grab = more
  points), a walk into the castle and a time bonus count.
- **No long walk back after a death**: you restart about 4 tiles before the spot where you fell
  or got hit, on flat ground, so you get a run-up to try the jump again. A game over sends you
  back to the first level of the current world, not to 1-1.
- **Forgiving jumps**: a JUMP pressed up to 4 frames after walking off an edge still jumps, and
  one pressed just before landing is remembered.
- **Power-ups**: ? blocks give coins, a **Mushroom** (grow big) or a **Fire Flower** (throw
  fireballs). Bricks hide a **Star** (invincible for 10 s), a **1-UP** or a stack of coins. Getting
  hit as big/fire shrinks you back down instead of losing a life. Because the plumber never stops,
  power-ups slide after him and hop over things to catch up.
- **Enemies**: Goombas (stomp them flat), Koopas (stomp them into a shell, kick the shell and it
  takes out other enemies), Piranha Plants in pipes (they hide when you get close).
- Big Mario breaks bricks, block bumps knock out enemies standing on them, stomp chains score
  100 → 200 → 400 … → 1-UP, 100 coins = 1-UP.
- **The demo player** has no special powers: it presses the same three buttons, using the same
  physics. Each tick it tries a few jumps ahead of time (including a follow-up jump) and picks the
  latest one that comes out safe, so it times jumps at the edge of holes and hops onto enemies.
  It still makes the odd mistake.
- **RGB LED** flashes with the action (coins yellow, power-ups green, fire orange, hits red, star
  rainbow). The speaker plays short original sound effects.

## 🖥️ How the graphics work

- Same engine as the [POKéMON Kalos tribute](../pokemon-kalos/): the game draws into a
  **240×160 framebuffer** and sends it to the panel **scaled 2×**, so every pixel is a crisp 2×2
  block on the 480×320 screen. No flicker, sprites overlap properly.
- Only the rows that changed are sent over SPI at 80 MHz. A scrolling level changes most of the
  screen, so it runs at about **26–30 fps** while moving (a full 480×320 repaint takes ~30 ms on the
  bus). The game logic always ticks at 30 Hz, so speed doesn't change.
- **Low input lag**: the frame is sent in strips of 16 rows and the touch panel is read between
  strips. A new press drops the rest of the frame being sent, runs a game tick right away (no
  waiting for the next 33 ms slot) and draws the result.
- Hills and clouds scroll at half speed (parallax); the night level has twinkling stars.

## 🛠️ Build & flash

```bash
pio run -e mario_arcade -t upload
pio device monitor -e mario_arcade     # fps, timings, and every button press
```

On every press Serial prints the raw touch reading, the screen position and which button it hit
(`[Touch] raw <x>,<y> -> <x>,<y> JUMP`). If the buttons react on the wrong side or upside down, flip
`TOUCH_FLIP_X` / `TOUCH_FLIP_Y` (or adjust `TOUCH_RAW_MIN/MAX`) at the top of `src/main.cpp`.

**Touch stops responding?** Check Serial. If every reading shows
`[Touch] ignored raw ~1740,3520 z=~4099`, the cracked touch glass on this board is shorting: it reads
the crack, not your finger, and no software can fix that. It comes and goes, so pressure on the glass
from the case may set it off. Use the light-sensor JUMP until it clears. If covering the sensor does
nothing, compare the `dark N (room N)` value on the fps line with and without your finger, and tune
`LDR_COVER_RISE` / `LDR_DARK_IS_LOW` in `src/main.cpp`.

If the picture has noise or glitches, your panel may not handle 80 MHz SPI. In `platformio.ini`
under `[env:mario_arcade]`, change `-D SPI_FREQUENCY=80000000` to `55000000`.

## 🎨 Editing the art and levels

Everything is made by Python scripts in `tools/` and saved into `include/art.h`:

| File | Contents |
| :--- | :--- |
| `tools/sprites.py` | The plumber (small / big / fire), enemies, items, coins, fireballs (ASCII pixel art) |
| `tools/tiles.py` | 16×16 tiles, hills, bushes, clouds, castle, flag, and the four colour themes |
| `tools/levels.py` | World 1 by hand with a small builder (`L.pipe(37, 3)`, `L.gap(66, 67)`, …) and the generator for worlds 2-8 (`WORLDS = 8`) |
| `tools/font.py` | The pixel font |
| `tools/make_art.py` | Writes `include/art.h` (`--preview DIR` also saves a picture of every level) |
| `tools/sim.cpp` | Runs the real game code on your computer: saves frames, or plays the demo headless and reports deaths |

```bash
cd examples/mario-arcade/tools
python3 make_art.py --preview /tmp/levels        # rebuild include/art.h (needs Pillow)
clang++ -std=c++17 -O2 -I../include sim.cpp -o sim
mkdir -p out && ./sim out 900 15 100:j3 160:j12 200:r 260:f   # take over at tick 100: hop, high jump, run, fire
./sim --stats 700000                              # demo with endless lives: clears and deaths for all 32 levels
```

`make_art.py` prints a warning when an enemy will meet the player under a low row of blocks (there
is no room to hop on it there). Keep levels 10 tiles tall; the ground is rows 8–9.

**More levels**: raise `WORLDS` in `levels.py` (each world adds four levels, about 5 KB of flash;
there is room for well over a hundred more worlds) and run `make_art.py` again. Then run
`./sim --stats` and check that every level gets cleared.

## 📁 Layout

```
mario-arcade/
├── src/main.cpp        # hardware: display push, 3-button touch zones, sound, LED
├── include/game.h      # physics, enemies, items, demo player, HUD, buttons (plain C++, no Arduino)
├── include/gfx.h       # 240x160 renderer (shared with pokemon-kalos)
├── include/art.h       # made by tools/make_art.py
├── tools/              # art + level scripts, desktop simulator
└── docs/screenshot.png
```

---

🤖 Working with an AI assistant? Give it [AI_PROMPT.md](../../AI_PROMPT.md).
