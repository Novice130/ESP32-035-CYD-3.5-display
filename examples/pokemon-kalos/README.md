# ⚡ POKéMON Kalos Tribute for ESP32-035 (CYD 3.5")

[![PlatformIO](https://img.shields.io/badge/PlatformIO-Supported-orange.svg)](https://platformio.org/)
[![Display](https://img.shields.io/badge/Display-3.5%22%20480x320%20ST7796-blue.svg)]()
[![FPS](https://img.shields.io/badge/Speed-30%20fps-green.svg)]()

A Pokémon X / Y / Z themed game for the **ESP32-035 (CYD 3.5" / ST7796 480x320)**, drawn in the
style of the Game Boy Advance games. When nobody touches it, it plays itself as a display.
Tap it and you're playing.

![Screens](docs/screens.png)

> **Fan tribute.** Pokémon X/Y are 3DS games and can't run on an ESP32, so this is a new game built
> for the board. All art here was hand-made for this project (no ripped game assets, no ROMs, no
> original music). Pokémon and its characters are trademarks of Nintendo / Game Freak / Creatures;
> this is a non-commercial fan project.

## 🧒 Start here (for students)

- **What you will see:** a trainer walking along a route full of grass, trees and a pond, and
  battles with creatures like Pikachu and Fennekin, drawn like an old handheld game. When nobody
  touches it, it plays itself.
- **How to play:** tap the screen once to walk or attack, tap twice quickly to search the grass or
  throw a ball. This game needs a working touch screen; without one it keeps playing itself.
- **You need:** the board, a USB-C cable and a computer set up as in the
  [main Start here](../../README.md#-start-here-for-students).
- **Steps:**
  1. Open this whole project folder in VS Code.
  2. Plug in the board with the USB cable.
  3. In PlatformIO, click **Project Tasks → env:pokemon_kalos → Upload**.
  4. When it says `SUCCESS`, watch the screen, then tap to play.
- **Try changing:** in `include/game.h`, the `MOVES` list gives each attack a power number, for
  example `{"THUNDERBOLT", T_ELECTRIC, 90, FX_BOLT}`. Change `90` to `200` (keep it under 256),
  upload again, and see how much faster battles end.

---

## 🎮 How to play

| Where you are | **1 tap** | **2 taps** |
| :--- | :--- | :--- |
| Display (demo) mode | Start playing | Start playing |
| Route (overworld) | Walk 4 steps along the route | Search the grass: start a battle right away |
| Battle | **FIGHT**: your partner uses its best move | Throw a **POKé BALL** |

- Walking through **tall grass** can also start a wild battle, just like the real games.
- Text moves along by itself, so you never have to tap through dialogue.
- After **30 seconds** without a tap, it goes back to display mode on its own.
- Display mode is **silent** so it can sit on a desk. Sound turns on when you play.

## ✨ What's inside

- **Route 4 in Kalos**: a scrolling overworld with a Pokémon Center, a sparkling pond with stone
  edges, flower beds that sway, lamp posts, trees with depth sorting (you walk behind them), and
  tall grass that rustles and hides the trainer's legs.
- **Battles that look like the GBA games**: the creatures slide in, a Poké Ball throw sends out
  your partner, HP bars drain and change colour, the EXP bar fills up, and the text types itself
  out in the battle box.
- **Creatures**: Pikachu, Fennekin, Froakie, Chespin (your team) and wild Fletchling and Bunnelby.
  Rare legendary encounters with **Xerneas (X)**, **Yveltal (Y)** and **Zygarde (Z)**, each with
  its own glowing aura, battle arena and screen transition.
- **Move effects**: Thunderbolt lightning, Ember fire, Water Gun bubbles, a Vine Whip lash, Gust
  whirlwind, Moonblast moon beam, Oblivion Wing, and Thousand Arrows raining down with screen
  shake. Type matchups work ("It's super effective!").
- **Catching**: the ball flies in an arc, pulls the creature in with a red flash, drops, wobbles up
  to 3 times, then you get *Gotcha!* and stars, or the creature breaks free.
- **Team**: the best partner for each matchup is picked for you. Your team levels up, and if one
  faints the next one is sent out.
- **RGB LED** flashes the move's colour; the speaker plays short original jingles.

## 🖥️ How the graphics work

- The game draws into a **240×160 framebuffer** (the GBA's exact resolution) and sends it to the
  panel **scaled 2×**, so every pixel is a crisp 2×2 block on the 480×320 screen.
- Each frame is fully drawn off-screen, so there's no flicker and sprites can overlap properly.
- Only the parts of the screen that changed are sent over SPI (at 80 MHz), which keeps it at
  **30 fps**.
- Gradients are dithered so skies don't show colour bands.

## 🛠️ Build & flash

```bash
pio run -e pokemon_kalos -t upload
pio device monitor -e pokemon_kalos     # shows fps and tap events
```

If the picture has noise or glitches, your panel may not handle 80 MHz SPI. In `platformio.ini`
under `[env:pokemon_kalos]`, change `-D SPI_FREQUENCY=80000000` to `55000000`.

## 🎨 Editing the art

All the art is made by Python scripts in `tools/` and saved into `include/art.h`:

| File | Contents |
| :--- | :--- |
| `tools/creatures.py` | Creature sprites, built from shapes with automatic outlines and shading |
| `tools/world.py` | Trainer, terrain tiles, Pokémon Center, trees, the Route 4 map and walking path |
| `tools/font.py` | The pixel font |
| `tools/make_art.py` | Writes `include/art.h` (`--preview DIR` also saves a map image) |
| `tools/preview.py` | Saves a sheet of all creature sprites as a PNG |
| `tools/sim.cpp` | Runs the real game code on your computer and saves the frames as images |

```bash
cd examples/pokemon-kalos/tools
python3 make_art.py                      # rebuild include/art.h (needs Pillow)
python3 preview.py creatures.png         # look at the creature sprites
clang++ -std=c++17 -O2 -I../include sim.cpp -o sim
mkdir -p out && ./sim out 900 30 300:s 400:d   # 30 s of play, one tap at tick 300, a double tap at 400
```

## 📁 Layout

```
pokemon-kalos/
├── src/main.cpp        # hardware: display push, touch tap detector, sound, LED
├── include/game.h      # overworld, battles, effects, HUD (plain C++, no Arduino)
├── include/gfx.h       # 240x160 renderer: sprites, text, gradients, shapes
├── include/art.h       # made by tools/make_art.py
├── tools/              # art scripts + desktop simulator
└── docs/screens.png
```

---

🤖 Working with an AI assistant? Give it [AI_PROMPT.md](../../AI_PROMPT.md).
