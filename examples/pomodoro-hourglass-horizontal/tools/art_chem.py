#!/usr/bin/env python3
"""Titration: hourglass-shaped lab glassware on a dark bench at night, lit by its own reagent."""
import math
import random
from PIL import Image, ImageDraw, ImageFilter
from hglib import *

rng = random.Random(23)

CX = 118
TOP, NECK, BOT = 25, 158, 289
RMAX, RNECK = 65.0, 2.6
STEM = 7           # straight bore each side of the neck, hidden by the stopcock
TIP_Y = 177        # the drip tip ends here (same constant in scenes/chem.h)
RING_Y = 122       # retort ring around the upper bulb's taper
FILL, ANALYTE = 0.74, 0.22   # same constants in scenes/chem.h

GREEN = (120, 255, 70)
VIOLET = (130, 90, 230)


DOME = 0.50


def bulb(u):
    if u >= 1:
        return 0.0
    # a circle of radius RMAX for the far half, a smooth concave funnel into the neck
    if u > DOME:
        d = (u - DOME) / (1 - DOME)
        return RMAX * math.sqrt(max(0.0, 1 - d * d))
    t = u / DOME
    return RNECK + (RMAX - RNECK) * (t * t * (3 - 2 * t)) ** 1.05


def radius(y):
    if y < TOP or y > BOT:
        return 0.0
    if abs(y - NECK) <= STEM:
        return RNECK
    if y < NECK:
        return bulb((NECK - STEM - y) / (NECK - STEM - TOP))
    return bulb((y - NECK - STEM) / (BOT - NECK - STEM))


g = Glass(CX, TOP, NECK, BOT, radius)

# ---------------------------------------------------------------- volume, for truthful graduations
_r = lambda y: ((int(round((CX + radius(y)) * 16)) - int(round((CX - radius(y)) * 16))) / 32.0) if TOP <= y <= BOT else 0
_up = sum(_r(y) ** 2 for y in range(TOP, NECK + 1))
_lo = sum(_r(y) ** 2 for y in range(NECK, BOT + 1))
V0 = min(_up, _lo) * FILL


def lower_level(f):
    target, s = f * V0, 0.0
    for y in range(BOT, NECK - 1, -1):
        v = _r(y) ** 2
        if s + v >= target:
            return y + 1 - (target - s) / max(v, 0.01)
        s += v
    return NECK


def upper_level(f):
    target, s = f * V0, 0.0
    for y in range(NECK, TOP - 1, -1):
        v = _r(y) ** 2
        if s + v >= target:
            return y + 1 - (target - s) / max(v, 0.01)
        s += v
    return TOP


# ---------------------------------------------------------------- the room: wall tiles caught in the glow
bg = new_rgb()
px = bg.load()
TW, TH = 34, 17
tile_tone = {}
for y in range(H):
    row = y // TH
    off = (row & 1) * TW // 2
    gy = y % TH
    for x in range(W):
        col = (x + off) // TW
        key = (col, row)
        if key not in tile_tone:
            tile_tone[key] = 0.92 + rng.random() * 0.16
        gx = (x + off) % TW
        grout = gx == 0 or gy == 0
        bevel = 1.12 if (gy == 1 or gx == 1) else (0.90 if (gy == TH - 1 or gx == TW - 1) else 1.0)
        # light from the upper bulb and a weaker one from the lower bulb
        d1 = math.hypot((x - CX) / 135.0, (y - 90) / 150.0)
        d2 = math.hypot((x - CX) / 110.0, (y - 235) / 90.0)
        l1 = math.exp(-d1 * d1 * 1.5)
        l2 = math.exp(-d2 * d2 * 1.5)
        base = (7.0, 10.0, 15.0)
        c = [base[i] + 3.0 * (x / W) * (1, 1, 1.5)[i] for i in range(3)]
        tone = tile_tone[key] * bevel * (0.62 if grout else 1.0)
        c[0] += (l1 * 9 + l2 * 11) * tone
        c[1] += (l1 * 42 + l2 * 9) * tone
        c[2] += (l1 * 18 + l2 * 24) * tone
        if not grout:
            c = [v * tone for v in c]
        px[x, y] = tuple(int(v) for v in c)

# ---- shelf with out-of-focus bottles, up in the dark on the right of the glass
blur = Image.new("RGBA", (W, H), (0, 0, 0, 0))
d = ImageDraw.Draw(blur)
d.rectangle((186, 86, 300, 91), fill=(26, 44, 34, 255))
d.rounded_rectangle((196, 44, 216, 86), 4, fill=(20, 48, 34, 255))
d.rectangle((202, 32, 210, 46), fill=(20, 48, 34, 255))
d.polygon([(222, 86), (246, 86), (237, 58), (237, 42), (231, 42), (231, 58)], fill=(34, 24, 60, 255))
d.rectangle((201, 60, 212, 74), fill=(70, 84, 70, 110))
blur = blur.filter(ImageFilter.GaussianBlur(1.6))
bg.paste(blur, (0, 0), blur)

# ---- bench: polished black slate, reflecting everything
BENCH = 280
px = bg.load()
for y in range(BENCH, H):
    t = (y - BENCH) / (H - BENCH)
    for x in range(W):
        n = 0.5 + 0.5 * math.sin(x * 0.043 + y * 0.31 + 2.2 * math.sin(y * 0.11 + x * 0.013))
        c = mixc((13, 17, 19), (6, 8, 10), t)
        c = mixc(c, (20, 26, 28), 0.25 * n)
        ref1 = math.exp(-(((x - CX) / 118.0) ** 2) - ((y - 300) / 22.0) ** 2)
        ref2 = math.exp(-(((x - CX) / 56.0) ** 2) - ((y - 292) / 12.0) ** 2)
        c = (c[0] + ref1 * 26 + ref2 * 40, c[1] + ref1 * 92 + ref2 * 34, c[2] + ref1 * 40 + ref2 * 86)
        px[x, y] = tuple(min(255, int(v)) for v in c)
for x in range(W):
    k = math.exp(-((x - CX) / 150.0) ** 2)
    blend_px(px, x, BENCH, (120, 230, 120), 0.08 + 0.55 * k)
    blend_px(px, x, BENCH + 1, (70, 140, 80), 0.05 + 0.25 * k)
vignette(bg, 0.5, CX + 40, 150)


# ---- steel helpers: lit from the glass side (right)
def lit_rod(x0, x1):
    return [(0, (16, 20, 22)), (.30, (54, 62, 64)), (.50, (92, 106, 104)),
            (.74, (96, 150, 100)), (.92, (150, 214, 136)), (1, (60, 96, 62))]


ROD_X0, ROD_X1 = 15, 22
shadow_ellipse(bg, ROD_X0 + 3, 299, 26, 4, alpha=0.55, blur=3)

# ---- iron base plate
BASE_L, BASE_R, BASE_T, BASE_B = 5, 174, 298, 313
shadow_ellipse(bg, 90, 314, 92, 4, alpha=0.7, blur=3)
PLATE = [(0, (20, 26, 26)), (.12, (40, 54, 48)), (.32, (64, 112, 74)), (.55, (96, 190, 104)),
         (.76, (58, 104, 68)), (1, (24, 32, 30))]
metal_bar(bg, BASE_L, BASE_R, BASE_T, BASE_B, [(0, (15, 19, 20)), (.5, (30, 44, 36)), (1, (14, 18, 19))], radius=3,
          bottom_dark=(6, 8, 9))
metal_bar(bg, BASE_L + 1, BASE_R - 1, BASE_T, BASE_T + 4, PLATE, radius=1.5)
# rod foot and hex nut
metal_bar(bg, ROD_X0 - 5, ROD_X1 + 5, BASE_T - 4, BASE_T + 1, lit_rod(0, 0), radius=1)
# the rod
metal_bar(bg, ROD_X0, ROD_X1, 4, BASE_T - 2, lit_rod(0, 0), radius=3)

# ---- cork ring the lower bulb rests in (back half here, front half on the front layer)
CORK = [(0, (32, 22, 14)), (.2, (92, 66, 40)), (.48, (140, 150, 84)), (.72, (92, 74, 44)), (1, (30, 22, 14))]
shadow_ellipse(bg, CX, 298, 42, 3, alpha=0.7, blur=2)
metal_bar(bg, CX - 34, CX + 34, 286, 298, CORK, radius=2.5, bottom_dark=(26, 18, 10))

# ---- retort ring: arm + boss head on the rod, ring round the taper (back half behind the glass)
ring_r = radius(RING_Y) + 4.5
RING_RY = 6.0


def ring_arc(img, front_half):
    p = img.load()
    rgba = img.mode == "RGBA"
    for y in range(int(RING_Y - RING_RY - 4), int(RING_Y + RING_RY + 5)):
        for x in range(int(CX - ring_r - 5), int(CX + ring_r + 6)):
            dx = (x + .5 - CX) / ring_r
            dy = (y + .5 - RING_Y) / RING_RY
            is_front = dy > 0
            if is_front != front_half:
                continue
            e = math.sqrt(dx * dx + dy * dy)
            dist = abs(e - 1.0) * RING_RY * (0.6 + 0.4 * abs(dy) / max(e, .01))
            cov = clamp(1.8 - dist)
            if cov <= 0:
                continue
            u = dx * 0.5 + 0.5
            lit = 0.25 + 0.75 * math.exp(-((u - 0.62) / 0.28) ** 2)
            c = mixc((30, 40, 38), (200, 250, 180), lit)
            if dy < 0:
                c = mixc(c, (12, 18, 18), 0.45)
            if rgba:
                over_px(p, x, y, c, cov)
            else:
                blend_px(p, x, y, c, cov)


ring_arc(bg, False)
ARM_Y0, ARM_Y1 = RING_Y - 1.5, RING_Y + 1.5
ARM_STOPS = [(0, (54, 66, 62)), (.6, (88, 150, 96)), (1, (134, 214, 120))]
metal_bar(bg, ROD_X1 - 1, CX - ring_r + 1, ARM_Y0, ARM_Y1, ARM_STOPS, radius=0.5, bottom_dark=(16, 24, 22))
# boss head
metal_bar(bg, ROD_X0 - 4, ROD_X1 + 5, RING_Y - 8, RING_Y + 8, [(0, (14, 18, 18)), (.35, (40, 46, 46)), (.8, (84, 134, 86)), (1, (48, 80, 52))],
          radius=2, top_light=(110, 150, 110), bottom_dark=(10, 12, 12))
metal_bar(bg, ROD_X0 - 12, ROD_X0 - 4, RING_Y - 2, RING_Y + 2, [(0, (20, 24, 24)), (.5, (66, 74, 72)), (1, (30, 36, 34))], radius=1)
metal_bar(bg, ROD_X0 - 13, ROD_X0 - 10, RING_Y - 5, RING_Y + 5, [(0, (30, 34, 34)), (.6, (78, 88, 84)), (1, (28, 32, 32))], radius=1.4)

# ---- small Erlenmeyer on the bench, a violet ember of the analyte
fl = Image.new("RGBA", (W * 3, H * 3), (0, 0, 0, 0))
fd = ImageDraw.Draw(fl)
FX, FB, FT = 206 * 3, 295 * 3, 252 * 3
fd.polygon([(FX - 5 * 3, FT), (FX + 5 * 3, FT), (FX + 5 * 3, FT + 12 * 3), (FX + 17 * 3, FB), (FX - 17 * 3, FB), (FX - 5 * 3, FT + 12 * 3)],
           fill=(90, 130, 110, 46))
fd.polygon([(FX - 11 * 3, 281 * 3), (FX + 11 * 3, 281 * 3), (FX + 16 * 3, FB - 2), (FX - 16 * 3, FB - 2)], fill=(92, 58, 170, 200))
fd.polygon([(FX - 11 * 3, 281 * 3), (FX + 11 * 3, 281 * 3), (FX + 12 * 3, 284 * 3), (FX - 12 * 3, 284 * 3)], fill=(168, 134, 240, 220))
fd.line([(FX - 5 * 3, FT), (FX - 5 * 3, FT + 12 * 3), (FX - 17 * 3, FB)], fill=(190, 255, 170, 190), width=3)
fd.line([(FX + 5 * 3, FT), (FX + 5 * 3, FT + 12 * 3), (FX + 17 * 3, FB)], fill=(120, 150, 170, 120), width=2)
fd.line([(FX - 7 * 3, FT), (FX + 7 * 3, FT)], fill=(200, 255, 190, 200), width=3)
fd.line([(FX - 17 * 3, FB), (FX + 17 * 3, FB)], fill=(120, 150, 190, 160), width=3)
fl = fl.resize((W, H), Image.LANCZOS)
bg.paste(fl, (0, 0), fl)
add_glow(bg, lambda dr: dr.ellipse((FX / 3 - 20, 270, FX / 3 + 20, 306), fill=(30, 12, 54)), 8)
# mirrored faintly in the slate
ref = fl.crop((180, 252, 232, 296)).transpose(Image.FLIP_TOP_BOTTOM).filter(ImageFilter.GaussianBlur(1.4))
ra = ref.split()[3].point(lambda v: int(v * 0.2))
ref.putalpha(ra)
bg.paste(ref.crop((0, 0, 52, 22)), (180, 296), ref.crop((0, 0, 52, 22)))

# ---- ground-glass stopper on the top of the funnel (outside the dynamic rect)
GLS = [(0, (30, 50, 40)), (.18, (110, 170, 120)), (.34, (230, 255, 220)), (.55, (120, 180, 120)), (.85, (70, 120, 80)), (1, (40, 64, 50))]
metal_bar(bg, CX - 7, CX + 7, 15, 26, [(0, (30, 56, 40)), (.25, (130, 196, 136)), (.4, (214, 250, 200)), (.7, (90, 150, 96)), (1, (40, 70, 48))], radius=1)
for yy in range(16, 26, 2):   # frosted joint
    metal_bar(bg, CX - 7, CX + 7, yy, yy + 1, [(0, (60, 90, 70)), (.5, (190, 220, 186)), (1, (60, 90, 70))], radius=0)
metal_bar(bg, CX - 10, CX + 10, 11, 16, GLS, radius=2)
sp = bg.load()
for y in range(3, 12):
    for x in range(CX - 12, CX + 13):
        dx = (x + .5 - CX) / 11.0
        dy = (y + .5 - 7.5) / 4.8
        e = math.hypot(dx, dy)
        cov = clamp((1 - e) * 5)
        if cov > 0:
            c = ramp(GLS, dx * .5 + .5)
            c = mixc(c, (250, 255, 240), 0.7 * math.exp(-(((dx + .35) / .25) ** 2 + ((dy + .35) / .4) ** 2)))
            blend_px(sp, x, y, c, cov * 0.92)

# ---------------------------------------------------------------- the glass
front = new_front()
paint_glass(bg, front, g, wall=1.7, edge=(206, 250, 220), body=(150, 220, 180), body_alpha=0.13,
            edge_alpha=0.62, shine=(236, 250, 244), shine_alpha=0.30, right_alpha=0.08,
            outer_glow=(80, 200, 90), outer_glow_alpha=0.15, outer_glow_width=8.0)
fp = front.load()
bp = bg.load()

# window-pane reflection curving over each belly: the only cold light in the room
for (yc, rr) in ((TOP + 48, 54.0), (BOT - 52, 56.0)):
    for y in range(int(yc - rr), int(yc + rr)):
        rg = radius(y)
        if rg < 18:
            continue
        for x in range(CX - int(rg), CX - int(rg * 0.30)):
            u = (x + .5 - CX) / rg           # -1 .. -.3
            v = (y - yc) / rr
            arc = math.exp(-((u + 0.56 + 0.17 * v * v) / 0.055) ** 2)
            k = arc * smooth(1.0, 0.70, abs(v)) * 0.10
            if k > 0.03:
                over_px(fp, x, y, (215, 235, 255), k)

# ---- drip tip: a short glass nozzle reaching into the lower bulb (behind the contents only at its core)
for y in range(NECK + STEM, TIP_Y + 2):
    t = (y - NECK - STEM) / (TIP_Y + 1 - NECK - STEM)
    rt = 3.8 - 1.3 * t
    for x in range(CX - 7, CX + 8):
        dx = abs(x + .5 - CX)
        cov = clamp(rt + 0.5 - dx) * clamp(dx - (rt - 1.6) + 0.5)
        if cov > 0:
            over_px(fp, x, y, (214, 255, 216) if x < CX else (120, 180, 150), 0.72 * cov)
for x in range(CX - 3, CX + 4):
    over_px(fp, x, TIP_Y + 1, (230, 255, 230), 0.8 * clamp(3.6 - abs(x + .5 - CX)))

# ---- stopcock: glass barrel across the neck, PTFE key through it
SC_Y = NECK
for y in range(SC_Y - 11, SC_Y + 12):
    v = (y + .5 - SC_Y) / 10.5
    if abs(v) >= 1:
        continue
    for x in range(CX - 21, CX + 20):
        u = (x + .5 - CX)
        end = clamp((21 - abs(u + 0.5)) / 2.5)
        cov = end * clamp((1 - abs(v)) * 6)
        if cov <= 0:
            continue
        sh = 0.5 + 0.5 * math.cos(v * 2.2 + 0.55)
        c = mixc((18, 56, 50), (130, 230, 190), sh)
        a = 0.26 + 0.52 * abs(v) ** 2
        if abs(u) < 3.2 and abs(v) < 0.42:        # the open bore: see the reagent through it
            a = 0.0
        # hot highlight along the barrel
        hl = math.exp(-((v + 0.48) / 0.11) ** 2)
        c = mixc(c, (250, 255, 245), 0.85 * hl)
        a = max(a, 0.9 * hl)
        over_px(fp, x, y, c, a * cov)
# key cone inside the barrel: visible darker core either side of the bore
for y in range(SC_Y - 5, SC_Y + 6):
    for x in range(CX - 19, CX + 20):
        if abs(x + .5 - CX) < 4:
            continue
        v = (y + .5 - SC_Y) / 5.5
        cov = clamp((1 - abs(v)) * 4)
        sh = 0.5 + 0.5 * math.cos(v * 2.4 + 0.5)
        over_px(fp, x, y, mixc((36, 60, 66), (196, 226, 226), sh), 0.42 * cov)
# neck collars above and below the barrel
for yy in (NECK - 12, NECK + 12):
    for x in range(CX - 6, CX + 7):
        dx = abs(x + .5 - CX)
        over_px(fp, x, yy, (214, 255, 216), 0.6 * clamp(5.5 - dx))

# ---- front half of the retort ring and cork
ring_arc(front, True)
metal_bar(front, CX - 36, CX + 36, 285, 292.0, CORK, radius=2.0, top_light=(190, 200, 120))
cork = front.load()
for _ in range(120):
    x = rng.randint(CX - 34, CX + 33)
    y = rng.randint(286, 291)
    r, gg, b, a = cork[x, y]
    if a:
        k = 0.72 + rng.random() * 0.5
        cork[x, y] = (min(255, int(r * k)), min(255, int(gg * k)), min(255, int(b * k)), a)


# ---------------------------------------------------------------- printed graduations
DIG = {"0": "111101101101111", "1": "010110010010111", "2": "111001111100111", "3": "111001111001111",
       "4": "101101111001001", "5": "111100111001111", "6": "111100111101111", "7": "111001010010010",
       "8": "111101111101111", "9": "111101111001111", "m": "000110111101101", "L": "100100100100111",
       ".": "000000000000010", "C": "011100100100011", "A": "010101111101101", "o": "010101010000000"}


def tiny(xr, y, s, a=0.86):
    """Right-aligned 3x5 text ending at xr, printed white."""
    x = xr - 4 * len(s) + 1
    for ch in s:
        bits = DIG[ch]
        for j in range(5):
            for i in range(3):
                if bits[j * 3 + i] == "1":
                    over_px(fp, x + i, y + j, (245, 255, 250), a)
        x += 4


def grad_mark(y, major, xa):
    """A short printed arc wrapping round the glass: the ends curve a touch."""
    r = radius(int(y))
    if r < 16:
        return
    L = 13 if major else 7
    for k in range(-1, L + 1):
        x = xa + k
        yy = y - 2.4 * ((x - CX) / max(r, 1)) ** 2
        for yi in (int(math.floor(yy)), int(math.floor(yy)) + 1):
            frac = yy - math.floor(yy)
            cov = (1 - frac) if yi == int(math.floor(yy)) else frac
            end = 1.0 if 0 <= k < L else 0.4
            over_px(fp, int(x), yi, (245, 255, 250), cov * end * (0.86 if major else 0.66))


# lower bulb: amount delivered, reading as a titration scale in mL
XA_LOW = CX + 22
for i in range(0, 11):
    f = i / 10.0
    y = lower_level(ANALYTE + f)
    major = i % 2 == 0
    if radius(int(y)) < 22:
        continue
    grad_mark(y, major, XA_LOW)
    if major:
        tiny(XA_LOW - 3, int(y - 2.4 * ((XA_LOW - CX) / radius(int(y))) ** 2) - 2, str(i * 5))
tiny(XA_LOW + 12, BOT - 26, "mL", 0.7)
# upper bulb: what is left in the funnel
for i in range(1, 11):
    f = i / 10.0
    y = upper_level(f)
    major = i % 2 == 0
    if radius(int(y)) < 22:
        continue
    grad_mark(y, major, XA_LOW)
    if major:
        tiny(XA_LOW - 3, int(y - 2.4 * ((XA_LOW - CX) / radius(int(y))) ** 2) - 2, str(i * 5))
tiny(XA_LOW + 8, TOP + 12, "20oC", 0.55) if False else None

# ---------------------------------------------------------------- info card
rounded_panel(bg, 250, 6, 476, 314, 10, (7, 11, 11), 0.86, (120, 230, 130), 0.30)

emit("chem", bg, front, g, dyn=(CX - 74, 22, 148, 270))
