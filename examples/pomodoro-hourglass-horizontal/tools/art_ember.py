#!/usr/bin/env python3
"""Deep Focus: a thin steel hourglass on a navy night grid. The embers are all live, see scenes/ember.h."""
import math
import random
from PIL import Image, ImageDraw, ImageFilter
from hglib import *

rng = random.Random(33)
CX = 112
TOP, NECK, BOT = 25, 160, 295
RMAX, RNECK = 60.0, 3.0
WALL = 2.4


def half(u):
    """u: 0 at the neck, 1 at the cap. Long concave funnel, rounded shoulder, slight return under the cap."""
    u = clamp(u)
    s = smooth(0.0, 0.70, u)
    r = RNECK + (RMAX - RNECK) * s ** 1.12
    return r * (1 - 0.055 * smooth(0.80, 1.0, u) ** 2)


def radius(y):
    if y < TOP or y > BOT:
        return 0.0
    return half((NECK - y) / (NECK - TOP)) if y <= NECK else half((y - NECK) / (BOT - NECK))


g = Glass(CX, TOP, NECK, BOT, radius)

# ---- night: deep navy, lifted softly behind the glass, grid fading towards the text
bg = new_rgb()
px = bg.load()
for y in range(H):
    for x in range(W):
        d = math.hypot((x - CX) / 210, (y - 160) / 210)
        k = clamp(1 - d) ** 1.6
        c = mixc((7, 10, 17), (15, 23, 38), k)
        gx, gy = (x - CX + 160) % 20, (y - 160 + 160) % 20
        line = 1.0 if gx == 0 or gy == 0 else 0.0
        if line:
            fade = 0.35 + 0.65 * (1 - smooth(200, 300, x))
            c = mixc(c, (40, 58, 84), 0.22 * fade)
        px[x, y] = tuple(int(v) for v in c)
vignette(bg, 0.5, 200, 160)


def outline(d, color, width):
    left = [(CX - radius(y) - 1, y) for y in range(TOP, BOT + 1)]
    right = [(CX + radius(y) + 1, y) for y in range(TOP, BOT + 1)]
    d.line(left, fill=color, width=width)
    d.line(right, fill=color, width=width)


# ---- faint blue rim glow, outside and inside the wall
add_glow(bg, lambda d: outline(d, (26, 46, 72), 4), 6.0, 0.7)
add_glow(bg, lambda d: outline(d, (24, 42, 66), 2), 2.0, 0.4)

front = new_front()
bp, fp = bg.load(), front.load()
STEEL = (84, 100, 120)
STEEL_HI = (128, 146, 168)
STEEL_LO = (54, 66, 84)
for y in range(TOP - 1, BOT + 2):
    r = radius(clamp(y, TOP, BOT))
    span = int(r + WALL + 5)
    for x in range(CX - span, CX + span + 1):
        dx = x + 0.5 - CX
        ad = abs(dx)
        # glass tint: faint fill, thicker towards the wall, a touch stronger in the lower half of each bulb
        if ad < r:
            u = ad / max(r, 0.5)
            blend_px(bp, x, y, (64, 92, 124), 0.055 + 0.20 * u ** 5)
        # true distance to the curve
        dist = 1e9
        for k in range(-24, 25):
            ry = y + 0.5 + k * 0.25
            rr = radius(ry)
            if rr <= 0:
                continue
            dist = min(dist, math.hypot(ad - rr, k * 0.25))
        inside = ad < radius(y + 0.5)
        sd = -dist if inside else dist     # 0 on the inner edge, WALL on the outer edge
        cov = clamp(sd + 0.5) * clamp(WALL + 0.5 - sd)
        if cov <= 0:
            # thin soft inner line: light caught inside the wall
            if inside and dist < 2.5:
                over_px(fp, x, y, (80, 110, 146), 0.20 * (1 - dist / 2.5))
            continue
        v = sd / WALL
        c = mixc(STEEL_HI, STEEL, smooth(0.05, 0.5, v))
        c = mixc(c, STEEL_LO, smooth(0.6, 1.0, v))
        if dx > 0:
            c = mixc(c, STEEL_LO, 0.28)
        over_px(fp, x, y, c, 0.95 * cov)

# ---- two slim highlights on the shoulders, very low so the embers stay clean
for y in range(TOP + 8, NECK - 34):
    r = radius(y)
    fade = smooth(TOP + 8, TOP + 26, y) * smooth(NECK - 34, NECK - 62, y)
    for x in range(int(CX - r + 3), int(CX - r + 9)):
        t = (x + 0.5 - (CX - r + 6)) / 2.0
        over_px(fp, x, y, (190, 214, 240), 0.13 * fade * math.exp(-t * t))
for y in range(NECK + 44, BOT - 8):
    r = radius(y)
    fade = smooth(NECK + 44, NECK + 70, y) * smooth(BOT - 8, BOT - 28, y)
    for x in range(int(CX - r + 3), int(CX - r + 9)):
        t = (x + 0.5 - (CX - r + 6)) / 2.0
        over_px(fp, x, y, (190, 214, 240), 0.10 * fade * math.exp(-t * t))

# ---- caps: dark slate plates with a thin lit edge
PLATE = [(0, (30, 38, 50)), (.18, (62, 74, 92)), (.42, (48, 58, 74)), (.8, (34, 42, 54)), (1, (22, 28, 38))]
COLLAR = [(0, (22, 28, 38)), (.2, (46, 56, 72)), (.5, (34, 42, 56)), (1, (18, 24, 34))]
LIGHT = (136, 156, 180)
DARK = (12, 16, 24)
metal_bar(bg, CX - 84, CX + 84, 6, 16, PLATE, radius=3.5, top_light=LIGHT, bottom_dark=DARK)
metal_bar(bg, CX - 73, CX + 73, 16, 25, COLLAR, radius=1.5, top_light=(84, 100, 122), bottom_dark=(10, 14, 20))
metal_bar(bg, CX - 73, CX + 73, 295, 304, COLLAR, radius=1.5, top_light=(84, 100, 122), bottom_dark=(10, 14, 20))
metal_bar(bg, CX - 84, CX + 84, 304, 314, PLATE, radius=3.5, top_light=LIGHT, bottom_dark=DARK)

emit("ember", bg, front, g, dyn=(CX - 72, 24, 144, 272))
