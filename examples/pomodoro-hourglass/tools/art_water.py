#!/usr/bin/env python3
"""Clepsydra: a blown-glass water clock on a wet stone sill, in front of a rain-streaked window at night."""
import math
import random
from PIL import Image, ImageDraw, ImageFilter
from hglib import *
import hglib

rng = random.Random(1731)
CX = 160
OY = 80
TOP, NECK, BOT = 26, 156, 294


def onion_profile(top, neck, bot, rmax, rneck, c=0.60):
    """Blown-glass bulb: S-curve out of the neck into a truly round shoulder."""

    def half(u):
        if u >= 1:
            return 0.0
        if u < c:
            t = u / c
            return rneck + (rmax - rneck) * (t * t * (3 - 2 * t)) ** 0.92
        d = (u - c) / (1 - c)
        return rmax * math.sqrt(max(0.0, 1 - d * d))

    def fn(y):
        if y < top or y > bot:
            return 0.0
        return half((neck - y) / (neck - top)) if y <= neck else half((y - neck) / (bot - neck))

    return fn


g = Glass(CX, TOP, NECK, BOT, onion_profile(TOP, NECK, BOT, 68, 4.0))
TIP = 163  # lowest row of the little glass spout; the drop hangs from here


# ---------------------------------------------------------------- value noise
def make_noise(seed, n=64):
    r = random.Random(seed)
    table = [[r.random() for _ in range(n)] for _ in range(n)]

    def at(x, y):
        ix, iy = math.floor(x), math.floor(y)
        fx, fy = x - ix, y - iy
        fx, fy = fx * fx * (3 - 2 * fx), fy * fy * (3 - 2 * fy)
        a = table[iy % n][ix % n]
        b = table[iy % n][(ix + 1) % n]
        c = table[(iy + 1) % n][ix % n]
        d = table[(iy + 1) % n][(ix + 1) % n]
        return a + (b - a) * fx + (c - a) * fy + (a - b - c + d) * fx * fy

    def fbm(x, y, oct=4):
        v, amp, tot = 0.0, 0.5, 0.0
        for _ in range(oct):
            v += at(x, y) * amp
            tot += amp
            x, y, amp = x * 2.03 + 11.3, y * 2.03 + 7.1, amp * 0.5
        return v / tot

    return fbm


fbm = make_noise(5)

# ---------------------------------------------------------------- night sky through the window
bg = new_rgb()
px = bg.load()
MOON = (288, 112, 12)   # off to the right of the top cap
for y in range(H):
    t = y / (H - 1)
    base = ramp([(0, (5, 14, 28)), (.35, (8, 34, 50)), (.62, (12, 54, 68)), (.8, (7, 34, 46)), (1, (3, 14, 22))], t)
    for x in range(W):
        n = fbm(x * 0.012, y * 0.02, 3)  # slow rain cloud
        c = mixc(base, (20, 62, 80), 0.34 * smooth(0.42, 0.78, n) * (1 - t))
        d = math.hypot(x - MOON[0], y - MOON[1])
        c = tuple(c[i] + (70, 110, 118)[i] * math.exp(-(d / 70) ** 2) * 0.55 + (30, 52, 60)[i] * math.exp(-(d / 170) ** 2) * 0.5
                  for i in range(3))
        px[x, y] = tuple(int(min(255, v)) for v in c)

# dark garden: trees and a hedge, far out of focus
layer = Image.new("RGBA", (W, H), (0, 0, 0, 0))
d = ImageDraw.Draw(layer)
for i in range(26):
    tx = rng.uniform(-20, W + 20)
    tr = rng.uniform(24, 62)
    ty = rng.uniform(256, 312)
    d.ellipse((tx - tr, ty - tr * 1.2, tx + tr, ty + tr * 1.2), fill=(3, 16, 22, 235))
d.rectangle((0, 306, W, 380), fill=(3, 13, 18, 245))
layer = layer.filter(ImageFilter.GaussianBlur(7))
bg.paste(layer, (0, 0), layer)
px = bg.load()


# bokeh: street and garden lights, soft discs with a slightly brighter rim
def bokeh(cx, cy, r, col, a):
    for y in range(int(cy - r - 1), int(cy + r + 2)):
        if not 0 <= y < H:
            continue
        for x in range(int(cx - r - 1), int(cx + r + 2)):
            if not 0 <= x < W:
                continue
            dd = math.hypot(x + .5 - cx, y + .5 - cy) / r
            if dd >= 1.04:
                continue
            cov = clamp((1 - dd) * r + .5) * (0.58 + 0.42 * smooth(.62, 1.0, dd))
            cur = px[x, y]
            px[x, y] = tuple(int(min(255, cur[i] + col[i] * a * cov)) for i in range(3))


WARM = [(255, 176, 92), (255, 136, 88), (255, 208, 130), (255, 110, 110)]
COOL = [(90, 220, 220), (120, 180, 255), (170, 240, 220)]
placed = 0
while placed < 34:
    warm = rng.random() < 0.62
    col = rng.choice(WARM if warm else COOL)
    r = rng.uniform(4, 15) * (1.25 if placed < 6 else 1.0)
    x = rng.uniform(0, W)
    y = rng.uniform(285, 366)
    # the drop falls down the middle: that stretch of the window stays dark so it reads
    keep = abs(x - CX) / 62 + max(0, y - 230) / 150 * .55
    if rng.random() > keep:
        continue
    bokeh(x, y, r, col, rng.uniform(.14, .40))
    placed += 1
for i in range(12):  # high distant windows, off to the sides
    side = rng.choice((rng.uniform(0, 56), rng.uniform(264, W)))
    bokeh(side, rng.uniform(150, 270), rng.uniform(2.0, 4.5), rng.choice(WARM), rng.uniform(.18, .4))

# the moon: pale, slightly veiled by cloud
for y in range(int(MOON[1] - MOON[2] - 2), int(MOON[1] + MOON[2] + 3)):
    for x in range(int(MOON[0] - MOON[2] - 2), int(MOON[0] + MOON[2] + 3)):
        dd = math.hypot(x + .5 - MOON[0], y + .5 - MOON[1])
        cov = clamp(MOON[2] - dd + .5)
        if cov > 0:
            m = fbm(x * 0.13 + 40, y * 0.13, 3)
            c = mixc((150, 180, 186), (214, 228, 226), smooth(.3, .62, m))
            blend_px(px, x, y, c, cov * .88)

bg = bg.filter(ImageFilter.GaussianBlur(1.4))
sharp = bg.copy()
spx = sharp.load()
px = bg.load()


# ---------------------------------------------------------------- rain on the pane: tiny lenses
def pane_drop(cx, cy, rx, ry):
    for y in range(int(cy - ry - 1), int(cy + ry + 2)):
        for x in range(int(cx - rx - 1), int(cx + rx + 2)):
            if not (0 <= x < W and 0 <= y < H):
                continue
            nx, ny = (x + .5 - cx) / rx, (y + .5 - cy) / ry
            dd = math.hypot(nx, ny)
            if dd >= 1.08:
                continue
            sx = int(clamp(cx - nx * 34, 0, W - 1))
            sy = int(clamp(cy - ny * 38, 0, H - 1))
            c = spx[sx, sy]
            c = mixc(c, (2, 8, 14), 0.55 * smooth(.25, 1.0, -ny * dd))  # dark upper edge
            c = mixc(c, (190, 235, 240), 0.42 * smooth(.45, 1.0, ny) * smooth(.3, .8, dd))  # lit lower lip
            blend_px(px, x, y, c, clamp((1.08 - dd) * min(rx, ry)) * .92)
    if rx > 1.6:
        blend_px(px, int(cx - rx * .35), int(cy - ry * .38), (235, 250, 250), .8)


# trickle tracks first, beads on top
for i in range(34):
    x = rng.uniform(0, W)
    y = rng.uniform(50, 280)
    ln = rng.uniform(30, 150)
    phase = rng.uniform(0, 6)
    wid = rng.uniform(.5, 1.0)
    for k in range(int(ln)):
        yy = y + k
        xx = x + 2.4 * math.sin(k * .05 + phase) + .8 * math.sin(k * .17 + phase * 2)
        a = 0.26 * smooth(0, 8, k) * smooth(ln, ln * .5, k) * wid
        ix = int(xx)
        f = xx - ix
        blend_px(px, ix, int(yy), (150, 205, 215), a * (1 - f))
        blend_px(px, ix + 1, int(yy), (150, 205, 215), a * f)
        blend_px(px, ix + 2, int(yy), (0, 6, 10), a * .6)
        if rng.random() < .05:
            pane_drop(xx, yy, rng.uniform(.8, 1.5), rng.uniform(1.0, 1.9))
    pane_drop(x + 2.4 * math.sin(ln * .05 + phase) + .8 * math.sin(ln * .17 + phase * 2), y + ln,
              rng.uniform(1.8, 3.0), rng.uniform(2.4, 4.0))
for i in range(620):
    r = rng.random() ** 2.4 * 2.4 + .55
    pane_drop(rng.uniform(0, W), rng.uniform(0, 372), r, r * rng.uniform(1.0, 1.35))

# ---------------------------------------------------------------- wet stone sill with a mirror sheen
SILL = 370
src = bg.copy().filter(ImageFilter.GaussianBlur(2.2))
sp = src.load()
for y in range(SILL, H):
    t = (y - SILL) / (H - SILL)
    for x in range(W):
        my = int(clamp(2 * SILL - y - 2, 0, H - 1))
        n = fbm(x * .05, y * .22, 3)
        stone = mixc((14, 30, 38), (5, 14, 20), t * .9 + .2 * n)
        c = tuple(stone[i] + sp[x, my][i] * (.34 - .22 * t) for i in range(3))
        px[x, y] = tuple(int(min(255, v)) for v in c)
for x in range(W):  # sill's lit front lip
    blend_px(px, x, SILL, (120, 170, 180), .35)
    blend_px(px, x, SILL + 1, (120, 170, 180), .14)
shadow_ellipse(bg, CX + 4, 395, 110, 6, alpha=0.8, blur=4, color=(0, 4, 8))
# light the glass throws on the sill
add_glow(bg, lambda dr: dr.ellipse((CX - 60, 392, CX + 110, 399), fill=(20, 68, 76)), 4, 1.0)
vignette(bg, 0.42, CX, 230)
P = bg
bg = hero_view(P, OY)

# ---------------------------------------------------------------- oxidised copper frame
COPPER = [(0, (30, 20, 16)), (.10, (78, 48, 34)), (.26, (176, 118, 84)), (.34, (120, 78, 56)),
          (.60, (70, 44, 34)), (.86, (38, 26, 24)), (1, (18, 14, 14))]
ROD = [(0, (22, 18, 16)), (.28, (150, 104, 76)), (.42, (214, 170, 130)), (.56, (96, 62, 46)), (1, (18, 14, 14))]
DARK = [(0, (16, 14, 14)), (.3, (86, 60, 46)), (.6, (44, 32, 28)), (1, (12, 10, 10))]
patina_noise = make_noise(21)


def patina(img, x0, y0, x1, y1, strength=.8, streak=1.6):
    """Verdigris: blooms in the recesses, running down in streaks."""
    p = img.load()
    rgba = img.mode == "RGBA"
    for y in range(int(y0), int(y1)):
        for x in range(int(x0), int(x1)):
            if not (0 <= x < W and 0 <= y < H):
                continue
            v = p[x, y]
            if rgba and v[3] == 0:
                continue
            n = patina_noise(x * .17, y * .17 / streak + 9, 4)
            n2 = patina_noise(x * .4 + 30, y * .4 / streak, 2)
            k = smooth(.53, .74, n + .12 * (n2 - .5)) * strength
            if k <= 0:
                continue
            lum = (v[0] * .3 + v[1] * .6 + v[2] * .1) / 170
            green = mixc((30, 78, 72), (112, 178, 156), clamp(lum * 1.1 + .4 * (n2 - .35)))
            c = mixc(v[:3], green, k)
            p[x, y] = tuple(int(q) for q in c) + ((v[3],) if rgba else ())


# posts
for rx in (CX - 84, CX + 84):
    metal_bar(bg, rx - 3.5, rx + 3.5, 18, 304, ROD, radius=0)
    patina(bg, rx - 4, 18, rx + 4, 304, .5, 3)
    for by, bh in ((28, 5), (NECK - 3, 6), (284, 5)):  # turned collars
        metal_bar(bg, rx - 6, rx + 6, by, by + bh, ROD, radius=2, top_light=(232, 190, 150), bottom_dark=(12, 10, 10))
        patina(bg, rx - 6, by, rx + 6, by + bh, .5, 2)

# glass and everything in front of the water
front = new_front()
paint_glass(bg, front, g, wall=1.8, edge=(190, 232, 240), body=(120, 190, 205), body_alpha=0.10, edge_alpha=0.78,
            shine=(226, 248, 252), shine_alpha=0.50, right_alpha=0.20, outer_glow=(70, 170, 185), outer_glow_alpha=0.20,
            outer_glow_width=6.0)

# reflection of the window on the shoulder of each bulb: a curved pair of panes
fp = front.load()
for (yc, ht, ang, amp) in ((76, 40, -.60, .24), (238, 34, -.60, .17)):
    for y in range(yc - ht, yc + ht):
        r = g.radius(y)
        if r < 22:
            continue
        v = (y - yc) / ht
        centre = CX + r * (ang + .10 * v * v)
        half = r * (0.105 - .03 * v * v)
        if abs(v) < .035:  # mullion
            continue
        for x in range(int(centre - half - 1), int(centre + half + 2)):
            cov = clamp(half + .5 - abs(x + .5 - centre))
            if cov > 0:
                over_px(fp, x, y, (232, 250, 255), amp * cov * smooth(1.0, .75, abs(v)))

# caps
metal_bar(bg, CX - 94, CX + 94, 6, 15, COPPER, radius=3, top_light=(240, 196, 156), bottom_dark=(14, 10, 10))
metal_bar(bg, CX - 88, CX + 88, 15, 20, DARK, radius=1.5, top_light=(130, 96, 76))
metal_bar(bg, CX - 72, CX + 72, 20, 27, COPPER, radius=2, top_light=(214, 160, 120), bottom_dark=(10, 8, 8))
metal_bar(bg, CX - 72, CX + 72, 295, 301, COPPER, radius=2, top_light=(214, 160, 120), bottom_dark=(10, 8, 8))
metal_bar(bg, CX - 88, CX + 88, 301, 306, DARK, radius=1.5, top_light=(130, 96, 76))
metal_bar(bg, CX - 96, CX + 96, 306, 315, COPPER, radius=3, top_light=(240, 196, 156), bottom_dark=(10, 8, 8))
patina(bg, CX - 96, 5, CX + 96, 28, .7)
patina(bg, CX - 96, 294, CX + 96, 316, .72)

# neck collar, drawn over the water
metal_bar(front, CX - 12, CX + 12, NECK - 11, NECK - 8, ROD, radius=1.2, top_light=(244, 210, 170))
metal_bar(front, CX - 9, CX + 9, NECK - 8, NECK - 2, COPPER, radius=1, bottom_dark=(10, 8, 8))
metal_bar(front, CX - 11, CX + 11, NECK - 2, NECK + 1, ROD, radius=1.2, bottom_dark=(16, 12, 12))
patina(front, CX - 12, NECK - 11, CX + 12, NECK + 2, .72, 1.5)
# the spout: two fine glass lines and a lip
for y in range(NECK + 1, TIP + 1):
    for sx, a in ((-4.0, .55), (3.0, .36)):
        x = CX + int(sx)
        over_px(fp, x, y, (205, 240, 248), a)
for x in range(CX - 4, CX + 4):
    over_px(fp, x, TIP, (225, 248, 252), .22)

# drop the faintest veil of the front layer: invisible on screen, costly per pixel
fp = front.load()
for y in range(H):
    for x in range(W):
        if fp[x, y][3] < 12:
            fp[x, y] = (0, 0, 0, 0)

path = emit("water", bg, front, g, dyn=(CX - 72, 24, 144, 274), portrait=P, oy=OY, card=((4, 18, 26), 0.82, (130, 200, 205), 0.30))


# ---------------------------------------------------------------- real caustics: photons through a rippled surface
N = 128
crng = random.Random(99)
waves = []
for i in range(10):
    kx, ky = crng.randint(-11, 11), crng.randint(-11, 11)
    if kx == 0 and ky == 0:
        kx = 3
    k = math.hypot(kx, ky)
    waves.append((kx, ky, crng.uniform(0, 6.283), crng.uniform(.6, 1.0) / k ** 1.2))
TAU = 2 * math.pi
STRENGTH = 5.0
hist = [0.0] * (N * N)
SS = 4
for j in range(N * SS):
    py = j / SS
    for i in range(N * SS):
        pxx = i / SS
        dx = dy = 0.0
        for kx, ky, ph, a in waves:
            v = math.cos(TAU * (kx * pxx + ky * py) / N + ph) * a
            dx += v * kx
            dy += v * ky
        qx = pxx + dx * STRENGTH / 4
        qy = py + dy * STRENGTH / 4
        ix, iy = math.floor(qx), math.floor(qy)
        fx, fy = qx - ix, qy - iy
        hist[(iy % N) * N + (ix % N)] += (1 - fx) * (1 - fy)
        hist[(iy % N) * N + ((ix + 1) % N)] += fx * (1 - fy)
        hist[((iy + 1) % N) * N + (ix % N)] += (1 - fx) * fy
        hist[((iy + 1) % N) * N + ((ix + 1) % N)] += fx * fy
# light wrap-around blur
blurred = hist
for _pass in range(5):
    src_ = blurred
    blurred = [0.0] * (N * N)
    for y in range(N):
        for x in range(N):
            s = 0.0
            for oy, ky in ((-1, 1), (0, 2), (1, 1)):
                for ox, kx in ((-1, 1), (0, 2), (1, 1)):
                    s += src_[((y + oy) % N) * N + ((x + ox) % N)] * kx * ky
            blurred[y * N + x] = s / 16
avg = sum(blurred) / len(blurred)
out = []
for v in blurred:
    t = v / avg  # 1 = unfocused light level
    out.append(int(clamp(smooth(.80, 2.6, t)) * 255))
with open(path, "a") as f:
    f.write(f"static const uint8_t ART_water_caustic[{N * N}] = {{\n")
    for i in range(0, len(out), 48):
        f.write(",".join(map(str, out[i:i + 48])) + ",\n")
    f.write("};\n")
print("caustic mean", sum(out) / len(out), "above 80:", sum(1 for v in out if v > 80) / len(out))
