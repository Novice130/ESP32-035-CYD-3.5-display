#!/usr/bin/env python3
"""Zen Ink: a flat, printed hourglass on alabaster paper. The one light scene.
Everything is painted through 8x coverage masks, so each edge is true area coverage."""
import math
import os
import random
from PIL import Image, ImageDraw, ImageFilter
from hglib import *

rng = random.Random(1188)
SS = 8

CX, TOP, NECK, BOT = 160, 28, 160, 292
OY = 80
RMAX, RNECK = 73.5, 4.8
BELLY = 0.70
FILL = 0.66

# 565 bin centres, so flat areas stay truly flat instead of showing the Bayer dither
PAPER = (244, 238, 228)        # shows as 247,239,230
PAPER_IN = (252, 246, 236)     # shows as 255,247,239: the bulb is a slightly cleaner window
INK = (40, 36, 34)
TAUPE_A = (150, 132, 113)
TAUPE_B = (128, 111, 95)


def r_ext(y):
    """Inner half width, defined past the ends so the wall runs on under the caps."""
    u = abs(y - NECK) / (NECK - TOP)
    v = min(1.0, u / BELLY)
    rise = math.sin(v * math.pi / 2) ** 1.7          # S-curve out of the neck, round into the belly
    r = math.hypot(RNECK, (RMAX - RNECK * 0.2) * rise)  # hyperbola: a soft waist, never a pinch
    if u > BELLY:                                    # lean back in toward the caps, tangent at the belly
        k = (u - BELLY) / (1.06 - BELLY)
        r *= 1 - 0.13 * (1 - math.cos(min(k, 1) * math.pi / 2)) * 1.6
    return r


def radius(y):
    return r_ext(y) if TOP <= y <= BOT else 0.0


g = Glass(CX, TOP, NECK, BOT, radius)


def new_mask():
    return Image.new("L", (W * SS, H * SS), 0)


def down(mask):
    return mask.resize((W, H), Image.BOX)


def poly(d, pts, fill=255):
    d.polygon([(x * SS, y * SS) for x, y in pts], fill=fill)


def round_rect(d, x0, y0, x1, y1, r, fill=255):
    d.rounded_rectangle((x0 * SS, y0 * SS, x1 * SS - 1, y1 * SS - 1), r * SS, fill=fill)


def paint(img, mask, color, strength=1.0):
    """Blend a flat colour through a coverage mask."""
    if strength < 1.0:
        mask = mask.point(lambda v: int(v * strength + 0.5))
    img.paste(Image.new("RGB", img.size, color), (0, 0), mask)


# ------------------------------------------------------------------ the wall stroke
def wall_weight(y, side):
    """Brush pressure: heavier through the belly of each bulb, lightest at the neck, and a
    slow wander that differs per side, so the two strokes read as drawn, not offset."""
    u = abs(y - NECK) / (NECK - TOP)
    w = 1.85 + 0.62 * math.exp(-((u - 0.62) / 0.30) ** 2) - 0.25 * math.exp(-(u / 0.14) ** 2)
    w += 0.17 * math.sin(y * 0.041 + (1.3 if side < 0 else 4.1))
    w += 0.09 * math.sin(y * 0.117 + (0.4 if side < 0 else 2.2))
    if y > NECK:
        w += 0.12    # the stroke carries more ink as it comes down
    return w


def wall_points(side):
    inner, outer = [], []
    y = TOP - 4.0
    while y <= BOT + 4.0:
        dr = (r_ext(y + 0.25) - r_ext(y - 0.25)) / 0.5
        nx, ny = 1.0, -dr
        n = math.hypot(nx, ny)
        nx, ny = nx / n, ny / n
        r = r_ext(y)
        w = wall_weight(y, side)
        inner.append((CX + side * r, y))
        outer.append((CX + side * (r + nx * w), y + ny * w))
        y += 0.25
    return inner, outer


# wall coverage
wall_m = new_mask()
d = ImageDraw.Draw(wall_m)
sil_m = new_mask()
ds = ImageDraw.Draw(sil_m)
in_m = new_mask()
di = ImageDraw.Draw(in_m)
L_in, L_out = wall_points(-1)
R_in, R_out = wall_points(1)
poly(d, L_in + L_out[::-1])
poly(d, R_in + R_out[::-1])
poly(ds, L_out + R_out[::-1])
poly(di, [p for p in L_in if TOP - 1 <= p[1] <= BOT + 2] + [p for p in R_in[::-1] if TOP - 1 <= p[1] <= BOT + 2])

# cap shapes (ink silhouette, then the taupe inlay)
CAPS = [  # x half width, y0, y1, r, colour
    (97, 8, 18, 4.6, TAUPE_A),
    (86, 17, 28, 1.6, TAUPE_B),
    (86, 293, 303, 1.6, TAUPE_B),
    (97, 302, 312, 4.6, TAUPE_A),
]
cap_m = new_mask()
dc = ImageDraw.Draw(cap_m)
for hw, y0, y1, r, _c in CAPS:
    round_rect(dc, CX - hw, y0, CX + hw, y1, r)
    round_rect(ds, CX - hw, y0, CX + hw, y1, r)

d.rectangle((0, 0, W * SS, TOP * SS - 1), fill=0)   # the caps close the stroke cleanly
d.rectangle((0, (BOT + 1) * SS, W * SS, H * SS), fill=0)
wall_cov = down(wall_m)
sil_cov = down(sil_m)
in_cov = down(in_m)
cap_cov = down(cap_m)


# ------------------------------------------------------------------ paper
def paper():
    img = new_rgb(PAPER)
    # large, soft mottling: pulp density. Low-frequency random field, upscaled.
    field = Image.new("L", (20, 30))
    fp = field.load()
    for y in range(30):
        for x in range(20):
            fp[x, y] = 128 + rng.randint(-60, 60)
    field = field.resize((W, H), Image.BICUBIC).filter(ImageFilter.GaussianBlur(9))
    # fibres: short curved hairs, some darker, some lighter, supersampled
    dark = Image.new("L", (W * 3, H * 3), 0)
    light = Image.new("L", (W * 3, H * 3), 0)
    dd, dl = ImageDraw.Draw(dark), ImageDraw.Draw(light)
    for i in range(1500):
        x, y = rng.random() * W * 3, rng.random() * H * 3
        a = rng.random() * math.tau
        ln = rng.uniform(10, 42)
        bend = rng.uniform(-0.012, 0.012)
        pts = []
        for s in range(9):
            pts.append((x, y))
            x += math.cos(a) * ln / 8
            y += math.sin(a) * ln / 8
            a += bend * ln / 8
        (dd if i % 3 else dl).line(pts, fill=rng.randint(70, 180), width=1)
    dark = dark.resize((W, H), Image.BOX)
    light = light.resize((W, H), Image.BOX)
    p, fi, da, li = img.load(), field.load(), dark.load(), light.load()
    for y in range(H):
        for x in range(W):
            k = (fi[x, y] - 128) * 0.035 - da[x, y] * 0.05 + li[x, y] * 0.045
            r, gg, b = PAPER
            # very gentle fall-off to the corners: the sheet is lit from upper right
            f = 1.0 - 0.035 * smooth(0.45, 1.25, math.hypot((x - 260) / 300, (y - 90) / 400))
            p[x, y] = tuple(int(clamp(v, 0, 255)) for v in (r * f + k, gg * f + k * 0.97, b * f + k * 0.88))
    return img


P = paper()
bg = hero_view(P, OY)


# ------------------------------------------------------------------ enso behind the glass
def enso_mask():
    m = new_mask()
    d = ImageDraw.Draw(m)
    ecx, ecy, R = CX + 1.0, 160.0, 103.0
    start = math.radians(-62)            # opens at the upper right, like a hand-closed circle
    sweep = math.radians(332)
    bristles = 22
    for b in range(bristles):
        across = (b + 0.5) / bristles * 2 - 1
        off0 = across * 5.6
        # the brush runs dry: outer bristles leave early and the rest skip
        end_t = 1.0 - rng.random() * (0.04 + 0.42 * abs(across))
        beg_t = rng.random() * 0.018
        ph = rng.random() * math.tau
        amp = rng.uniform(0.2, 0.9)
        strength = rng.randint(90, 255)
        pts, n = [], 500
        for i in range(n + 1):
            t = i / n
            if t < beg_t or t > end_t:
                continue
            press = 0.55 + 0.45 * math.sin(math.pi * min(1, t * 1.25)) ** 0.7    # lands, swells, lifts
            press *= 1 - 0.55 * smooth(0.72, 1.0, t)
            ang = start + sweep * t
            rr = R + 1.8 * math.sin(ang * 1.5 + 0.7) + off0 * press + amp * math.sin(t * 19 + ph)
            # skipping dry bristles
            if t > 0.30 and math.sin(t * (46 + b * 7) + ph) > 1.2 - 1.15 * smooth(0.30, 1.0, t):
                if len(pts) > 1:
                    d.line(pts, fill=strength, width=7)
                pts = []
                continue
            pts.append(((ecx + rr * math.cos(ang)) * SS, (ecy + rr * math.sin(ang)) * SS))
        if len(pts) > 1:
            d.line(pts, fill=strength, width=7)
    return down(m)


enso = enso_mask()
paint(bg, enso, (96, 88, 80), 0.24)

# ------------------------------------------------------------------ soft shadow
shadow = sil_cov.filter(ImageFilter.GaussianBlur(7))
shadow = Image.composite(shadow, Image.new("L", (W, H), 0), Image.new("L", (W, H), 255))
sh2 = Image.new("L", (W, H), 0)
sh2.paste(shadow, (5, 6))
paint(bg, sh2, (96, 80, 62), 0.20)
tight = Image.new("L", (W, H), 0)
tight.paste(sil_cov.filter(ImageFilter.GaussianBlur(1.6)), (1, 2))
paint(bg, tight, (96, 80, 62), 0.10)

# ------------------------------------------------------------------ the bulb window
paint(bg, in_cov, PAPER_IN)
px = bg.load()

# ------------------------------------------------------------------ caps
ink_only = Image.new("L", (W, H), 0)
paint(bg, cap_cov, INK)
for hw, y0, y1, r, col in CAPS:
    m = new_mask()
    dm = ImageDraw.Draw(m)
    round_rect(dm, CX - hw + 1.7, y0 + 1.7, CX + hw - 1.5, y1 - 1.45, max(0.6, r - 1.5))
    paint(bg, down(m), col)
# faint cedar grain in the inlays, printed not rendered
for hw, y0, y1, r, col in CAPS:
    for y in range(y0 + 2, y1 - 2):
        phase = rng.random() * 9
        freq = rng.uniform(0.018, 0.05)
        for x in range(CX - hw + 3, CX + hw - 2):
            k = 3.4 * math.sin(x * freq + phase + 1.6 * math.sin(x * 0.011 + y)) + rng.uniform(-1, 1)
            v = px[x, y]
            px[x, y] = tuple(int(clamp(c + k, 0, 255)) for c in v)

# ------------------------------------------------------------------ a small seal: the print's chop mark
SEAL = (172, 66, 46)
seal_m = new_mask()
ds2 = ImageDraw.Draw(seal_m)
sx, sy, ss_ = 262, 296, 15
round_rect(ds2, sx, sy, sx + ss_, sy + ss_, 1.6)
# knock out a tiny hourglass in the seal
poly(ds2, [(sx + 3.6, sy + 3.2), (sx + ss_ - 3.6, sy + 3.2), (sx + ss_ / 2 + .6, sy + ss_ / 2),
           (sx + ss_ - 3.6, sy + ss_ - 3.2), (sx + 3.6, sy + ss_ - 3.2), (sx + ss_ / 2 - .6, sy + ss_ / 2)], fill=0)
seal_cov = down(seal_m)
# worn stamp edges
sp = seal_cov.load()
for y in range(sy, sy + ss_):
    for x in range(sx, sx + ss_):
        if rng.random() < 0.10:
            sp[x, y] = int(sp[x, y] * rng.uniform(0.62, 0.92))
paint(bg, seal_cov, SEAL, 0.92)

# ------------------------------------------------------------------ front: the ink wall
front = Image.new("RGBA", (W, H), INK + (0,))
front.putalpha(wall_cov)

dyn = (CX - 75, TOP, 150, BOT - TOP + 1)
path = emit("zen", bg, front, g, dyn=dyn, portrait=P, oy=OY)


# ------------------------------------------------------------------ tables, appended after the art
# The mound: a bell of revolution standing on the floor. Height and footprint follow the real
# volume, with the same rule GlassVolume uses, so top and bottom stay in sync.
def row_r(y):
    r = radius(y)
    return (int(round((CX + r) * 16)) - int(round((CX - r) * 16))) / 32.0


A, B_ = 1.62, 1.55   # bell: round shoulder at the tip, long calm toe


def prof(u):
    return (1 - u ** A) ** B_ if u < 1 else 0.0


def prof_inv(v):
    return (1 - v ** (1 / B_)) ** (1 / A)


rows_up = sum(row_r(y) ** 2 for y in range(TOP, NECK + 1))
rows_lo = sum(row_r(y) ** 2 for y in range(NECK, BOT + 1))
v0 = min(rows_up, rows_lo) * FILL
FLOOR = BOT + 1
SLOPE, HMAX = 1.85, 44.0
BW = RMAX + 4


def mound_at(t):
    """t 0..1 grow the hill, then 1..2 the hill keeps its size and the whole bed rises."""
    if t <= 1:
        return 0.0, HMAX * t, min(BW, SLOPE * HMAX * t + 0.01)
    return (t - 1) * 90.0, HMAX, BW


def mound_vol(e, h, b):
    if h <= 0.01:
        return 0.0
    tot = 0.0
    for y in range(NECK, BOT + 1):
        v = (FLOOR - e - (y + 0.5)) / h
        r = row_r(y)
        if v <= 0:
            tot += r * r
        elif v < 1:
            tot += min(r, b * prof_inv(v)) ** 2
    return tot


TAB = 64
tab_e, tab_h, tab_b = [], [], []
for i in range(TAB + 1):
    p = (i / TAB) ** 3   # index runs on the cube root: hills grow as p^(1/3)
    target = p * v0
    lo, hi = 0.0, 2.0
    for _ in range(40):
        mid = (lo + hi) / 2
        if mound_vol(*mound_at(mid)) < target:
            lo = mid
        else:
            hi = mid
    e, h, b = mound_at((lo + hi) / 2)
    tab_e.append(e)
    tab_h.append(h)
    tab_b.append(b)

taper = []          # thread half width above its resting width, Q8, by rows below the neck
for k in range(48):
    t = k / 47
    taper.append(int(round(256 * (RNECK + 0.6 - 1.15) * (1 - smooth(0, 1, t)) ** 1.7)))


def fl(v):
    return ",".join(f"{x:.3f}f" for x in v)


with open(path, "a") as f:
    f.write(f"#define ZEN_FILL {FILL}f\n#define ZEN_TAB {TAB}\n")
    f.write(f"static const float ZEN_MOUND_E[{TAB+1}] = {{{fl(tab_e)}}};\n")
    f.write(f"static const float ZEN_MOUND_H[{TAB+1}] = {{{fl(tab_h)}}};\n")
    f.write(f"static const float ZEN_MOUND_B[{TAB+1}] = {{{fl(tab_b)}}};\n")
    f.write("static const uint16_t ZEN_PROF[257] = {"
            + ",".join(str(int(round(prof(i / 256) * 32767))) for i in range(257)) + "};\n")
    # slope (1/16 steps) -> vertical-to-true distance factor Q8, and half the vertical AA run Q8
    hyp = [1 / math.sqrt(1 + (i / 16) ** 2) for i in range(96)]
    f.write("static const uint16_t ZEN_HYP[96] = {" + ",".join(str(int(round(v * 256))) for v in hyp) + "};\n")
    f.write("static const uint16_t ZEN_RUN[96] = {" + ",".join(str(int(round(128 / v))) for v in hyp) + "};\n")
    f.write("static const uint16_t ZEN_TAPER[48] = {" + ",".join(map(str, taper)) + "};\n")
print(f"zen: v0 {v0:.0f}, mound at p=1 e={tab_e[-1]:.1f} h={tab_h[-1]:.1f}, peak y={FLOOR-tab_e[-1]-tab_h[-1]:.1f}")
