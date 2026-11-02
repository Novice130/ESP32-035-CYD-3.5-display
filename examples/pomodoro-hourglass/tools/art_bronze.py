#!/usr/bin/env python3
"""Desk Bronze: a brushed-bronze hourglass on a sunlit wooden desk."""
import math
import random
from PIL import Image, ImageDraw, ImageFilter
from hglib import *

rng = random.Random(7)
BRONZE = [(0, (58, 36, 20)), (.10, (120, 80, 46)), (.27, (232, 188, 132)), (.38, (176, 128, 80)),
          (.62, (120, 80, 48)), (.86, (70, 44, 26)), (1, (40, 25, 15))]
ROD = [(0, (70, 62, 52)), (.3, (226, 214, 194)), (.55, (150, 138, 120)), (1, (52, 46, 40))]
CX = 160
OY = 112           # this glass has its neck at hero row 128: 128 + 112 = 240, the crack
DESK = 364         # screen row where the desk starts
g = Glass(CX, 30, 128, 232, classic_profile(30, 128, 232, 66, 3.4, belly=0.66, dome=0.18))

# ---- room: warm plaster wall, window light from the upper right (screen coordinates)
P = new_rgb()
px = P.load()
for y in range(H):
    for x in range(W):
        d = math.hypot((x - 300) / 340, (y - 70) / 420)
        k = clamp(1.1 - d)
        px[x, y] = tuple(int(v) for v in mixc((70, 46, 28), (236, 200, 150), k ** 1.3))
# books, out of focus, on a shelf behind the desk
blur = Image.new("RGBA", (W, H), (0, 0, 0, 0))
d = ImageDraw.Draw(blur)
x = -6
for w, h, c in [(30, 196, (92, 36, 22)), (26, 170, (120, 74, 36)), (34, 214, (58, 34, 22)), (24, 176, (132, 60, 30)),
                (32, 200, (78, 52, 30)), (26, 158, (40, 30, 26)), (30, 190, (104, 44, 26)), (30, 168, (86, 62, 34)),
                (28, 206, (66, 40, 26)), (32, 180, (118, 66, 34)), (30, 164, (92, 40, 24))]:
    d.rectangle((x, DESK - h, x + w, DESK), fill=c + (255,))
    d.rectangle((x + 6, DESK - h + 22, x + w - 6, DESK - h + 26), fill=(214, 170, 96, 200))
    x += w + 1
blur = blur.filter(ImageFilter.GaussianBlur(5))
P.paste(blur, (0, 0), blur)
# desk
px = P.load()
for y in range(DESK, H):
    t = (y - DESK) / (H - DESK)
    for x in range(W):
        n = 0.5 + 0.5 * math.sin(x * 0.025 + y * 0.42 + 3 * math.sin(y * 0.09 + x * 0.006))
        n2 = 0.5 + 0.5 * math.sin(x * 0.11 + y * 1.3)
        base = mixc((120, 70, 36), (66, 36, 18), t * 0.8 + 0.15 * n)
        c = mixc(base, (160, 100, 56), 0.16 * n2)
        sheen = 0.26 * clamp(1 - math.hypot((x - 250) / 150, (y - DESK - 12) / 22))
        px[x, y] = tuple(int(v) for v in mixc(c, (250, 215, 165), sheen))
shadow_ellipse(P, CX + 8, 395, 112, 7, alpha=0.62, blur=5, color=(18, 9, 4))
vignette(P, 0.45, 210, 210)
bg = hero_view(P, OY)

# ---- back rods, seen through the glass
for rx in (CX - 24, CX + 24):
    metal_bar(bg, rx - 3, rx + 3, 26, 238, [(0, (46, 36, 26)), (.4, (120, 104, 86)), (1, (40, 32, 24))], radius=0)
# ---- outer rods
for rx in (CX - 85, CX + 85):
    metal_bar(bg, rx - 4, rx + 4, 24, 238, ROD, radius=0)

front = new_front()
paint_glass(bg, front, g, wall=2.0, edge=(255, 240, 215), body=(255, 236, 205), body_alpha=0.16,
            edge_alpha=0.7, shine=(255, 250, 235), shine_alpha=0.7, right_alpha=0.26)

# ---- front rods pass in front of the bulbs
for rx in (CX - 47, CX + 47):
    metal_bar(front, rx - 3.5, rx + 3.5, 24, 238, ROD, radius=0)

# ---- top cap
metal_bar(bg, CX - 94, CX + 94, 8, 22, BRONZE, radius=3, top_light=(255, 232, 190), bottom_dark=(44, 26, 14))
metal_bar(bg, CX - 88, CX + 88, 22, 28, [(0, (54, 44, 34)), (.3, (196, 180, 156)), (.7, (106, 92, 76)), (1, (46, 38, 30))], radius=1)
for bx in (CX - 85, CX + 85, CX - 47, CX + 47):
    metal_bar(bg, bx - 3, bx + 3, 5, 9, ROD, radius=1.5)
# ---- base
metal_bar(bg, CX - 88, CX + 88, 234, 240, [(0, (54, 44, 34)), (.3, (196, 180, 156)), (.7, (106, 92, 76)), (1, (46, 38, 30))], radius=1)
metal_bar(bg, CX - 100, CX + 100, 240, 272, BRONZE, radius=3, top_light=(255, 226, 180), bottom_dark=(48, 28, 15))
metal_bar(bg, CX - 96, CX + 96, 272, 282, [(0, (112, 96, 74)), (.25, (214, 196, 166)), (.5, (182, 164, 136)), (1, (92, 78, 60))],
          radius=3, bottom_dark=(70, 58, 44))
# OLED bezel (the digits are live, drawn by the scene)
rounded_panel(bg, CX - 54, 245, CX + 54, 267, 3, (8, 8, 9), 1.0, (84, 72, 56), 0.9)

emit("bronze", bg, front, g, dyn=(CX - 74, 28, 148, 240), portrait=P, oy=OY, card=((26, 18, 12), 0.76, (232, 196, 146), 0.32))
