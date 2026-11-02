#!/usr/bin/env python3
"""Neon Flow: violet neon glass floating in the void over a perspective grid floor.
All bloom on the glass is baked here with big Gaussian blurs; only the plasma is live."""
import math
import random
from PIL import Image, ImageChops, ImageDraw, ImageFilter
from hglib import *

rng = random.Random(23)
SS = 4
CX, TOP, NECK, BOT = 160, 27, 160, 293
OY = 80
RMAX, RNECK, WALL = 69.0, 3.4, 3.0
PLATE_HW, COLLAR_HW = 88, 72
HORIZON = 324    # screen row
NEON = (150, 126, 255)
NEON_HOT = (214, 202, 255)
VIOLET = (108, 74, 255)


def profile(y):
    """Round shoulders, near-vertical sides, S-curve down to a short neck."""
    if y < TOP or y > BOT:
        return 0.0
    u = (NECK - y) / (NECK - TOP) if y <= NECK else (y - NECK) / (BOT - NECK)
    s = smooth(0.0, 0.70, u) ** 0.86
    r = RNECK + (RMAX - RNECK) * s
    if u > 0.88:  # lean in to meet the collar
        d = (u - 0.88) / 0.12
        r -= 6.0 * d * d
    return r


g = Glass(CX, TOP, NECK, BOT, profile)


# ------------------------------------------------------------------ mask helpers
def hi_mask():
    m = Image.new("L", (W * SS, H * SS), 0)
    return m, ImageDraw.Draw(m)


def lo(m):
    return m.resize((W, H), Image.LANCZOS)


def side_points(offset, y0=TOP, y1=BOT, sign=1):
    pts = []
    y = float(y0)
    while y <= y1:
        pts.append(((CX + sign * (profile(y) + offset)) * SS, (y + 0.5) * SS))
        y += 0.5
    return pts


def wall_stroke(offset, width):
    m, d = hi_mask()
    for sign in (-1, 1):
        d.line(side_points(offset, sign=sign), fill=255, width=int(round(width * SS)), joint="curve")
    return lo(m)


def plates_mask(inset=0.0, only=None):
    """The neon plates and collars. inset > 0 shrinks them, for stroke cut-outs."""
    m, d = hi_mask()
    shapes = [(PLATE_HW, 7, 17, 4), (COLLAR_HW, 17, 26.5, 2), (COLLAR_HW, 293.5, 303, 2), (PLATE_HW, 303, 313, 4)]
    for hw, y0, y1, r in shapes:
        d.rounded_rectangle(((CX - hw + inset) * SS, (y0 + inset) * SS, (CX + hw - inset) * SS, (y1 - inset) * SS),
                            max(0, (r - inset)) * SS, fill=255)
    return lo(m)


def tinted(mask, color, gain=1.0):
    return Image.merge("RGB", [mask.point(lambda v, c=c: int(min(255, v * c / 255 * gain))) for c in color])


def add_mask(img, mask, color, gain=1.0):
    return ImageChops.add(img, tinted(mask, color, gain))


def over(img, mask, color, gain=1.0):
    img.paste(tuple(color), (0, 0), mask.point(lambda v: int(min(255, v * gain))))


def front_over(front, mask, color, gain=1.0):
    layer = Image.new("RGBA", (W, H), tuple(color) + (0,))
    layer.putalpha(mask.point(lambda v: int(min(255, v * gain))))
    return Image.alpha_composite(front, layer)


def to_screen(mask):
    """A hero-space mask moved to screen rows, so its bloom can spread above the hero canvas."""
    out = Image.new("L", (W, H), 0)
    out.paste(mask, (0, OY))
    return out


# ------------------------------------------------------------------ the void and the grid floor (screen coordinates)
bg = new_rgb((3, 3, 9))
px = bg.load()
for y in range(H):
    for x in range(W):
        # the faintest indigo lift behind the hourglass
        d = math.hypot((x - CX) / 230, (y - 230) / 290)
        k = clamp(1 - d) ** 2
        px[x, y] = (int(3 + 11 * k), int(3 + 6 * k), int(9 + 26 * k))
# a very sparse star field in the sky
for _ in range(46):
    x, y = rng.randrange(W), rng.randrange(HORIZON - 30)
    a = rng.uniform(0.10, 0.34)
    c = rng.choice([(190, 170, 255), (255, 170, 230), (150, 190, 255)])
    blend_px(px, x, y, c, a)

# perspective floor: lines drawn at 4x, faded toward the horizon
grid, d = hi_mask()
VP = CX
for i in range(-40, 41):
    x_bottom = VP + i * 46
    d.line(((VP + (x_bottom - VP) * 0.02) * SS, HORIZON * SS, x_bottom * SS, H * SS), fill=255, width=SS)
z = 1.0
while True:
    y = HORIZON + 150 / z
    if y >= H + 2 and z > 1:
        pass
    if z > 40:
        break
    if y < H:
        d.line((0, y * SS, W * SS, y * SS), fill=255, width=SS)
    z += 0.55
grid = lo(grid)
gp = grid.load()
floor = Image.new("RGB", (W, H), (0, 0, 0))
fp = floor.load()
for y in range(HORIZON, H):
    t = (y - HORIZON) / (H - HORIZON)
    c = mixc((120, 44, 190), (62, 60, 210), t)
    fade = 0.16 + 0.34 * t
    for x in range(W):
        v = gp[x, y] / 255 * fade
        fp[x, y] = (int(c[0] * v), int(c[1] * v), int(c[2] * v))
bg = ImageChops.add(bg, floor)

# horizon haze
px = bg.load()
for y in range(H):
    dy = y - HORIZON
    k = math.exp(-(dy / (15 if dy < 0 else 8)) ** 2)
    if k < 0.01:
        continue
    for x in range(W):
        fx = 0.45 + 0.55 * math.exp(-((x - CX) / 170) ** 2)
        a = k * fx
        r, gg, b = px[x, y]
        px[x, y] = (min(255, int(r + 40 * a)), min(255, int(gg + 12 * a)), min(255, int(b + 70 * a)))

# pool of light on the floor under the base
pool, d = hi_mask()
d.ellipse(((CX - 96) * SS, 386 * SS, (CX + 96) * SS, 410 * SS), fill=150)
bg = add_mask(bg, lo(pool).filter(ImageFilter.GaussianBlur(9)), (92, 54, 210), 0.9)

# ------------------------------------------------------------------ baked neon halo
stroke = wall_stroke(WALL / 2, WALL)
plates = plates_mask()
plates_hollow = ImageChops.subtract(plates, plates_mask(1.4))
neon = ImageChops.lighter(stroke, plates_hollow)
inner, d = hi_mask()
poly = side_points(0, sign=-1) + list(reversed(side_points(0, sign=1)))
d.polygon(poly, fill=255)
inner = lo(inner)

screen_neon = to_screen(neon)
bg = add_mask(bg, screen_neon.filter(ImageFilter.GaussianBlur(22)), (84, 52, 255), 1.5)
bg = add_mask(bg, screen_neon.filter(ImageFilter.GaussianBlur(9)), (100, 66, 255), 1.15)
bg = add_mask(bg, screen_neon.filter(ImageFilter.GaussianBlur(3.2)), (124, 92, 255), 0.75)
P = bg
bg = hero_view(P, OY)

# violet glass body: faint fill, thick fresnel where the wall curves away
over(bg, inner, (36, 22, 92), 0.26)
fres = ImageChops.multiply(neon.filter(ImageFilter.GaussianBlur(7)), inner)
bg = add_mask(bg, fres, (112, 74, 245), 0.55)

# ------------------------------------------------------------------ plates (static, all on the bg)
plate_fill = plates_mask(0.4)
over(bg, plate_fill, (34, 26, 86), 0.92)
# a soft vertical sheen in each plate
px = bg.load()
pm = plate_fill.load()
for y, h0 in ((7, 10), (17, 9), (294, 9), (303, 10)):
    for yy in range(y, y + h0):
        v = (yy - y) / h0
        for x in range(CX - PLATE_HW - 1, CX + PLATE_HW + 2):
            a = pm[x, yy] / 255
            if a:
                u = abs(x + 0.5 - CX) / PLATE_HW
                blend_px(px, x, yy, (92, 78, 200), a * 0.34 * (1 - v) * (1 - u * u))
over(bg, plates_hollow, NEON, 1.0)
top_line = Image.new("L", (W, H), 0)
d = ImageDraw.Draw(top_line)
for y0, hw in ((8, PLATE_HW - 5), (304, PLATE_HW - 5)):
    for x in range(CX - hw, CX + hw):
        d.point((x, y0), fill=int(255 * (1 - smooth(-0.1, 0.9, (x - CX) / hw))))
over(bg, top_line.filter(ImageFilter.GaussianBlur(0.4)), NEON_HOT, 0.8)

# ------------------------------------------------------------------ front layer: wall and gloss
front = new_front()
# wall body, inner hairline, outer bright line
front = front_over(front, wall_stroke(WALL / 2 - 0.25, WALL + 0.5), (92, 74, 214), 0.62)
front = front_over(front, wall_stroke(WALL - 0.4, 1.3), (176, 154, 255), 0.95)
front = front_over(front, wall_stroke(0.1, 0.9), (168, 148, 255), 0.42)


def gloss(points, blur=2.0):
    m, d = hi_mask()
    d.polygon([(x * SS, y * SS) for x, y in points], fill=255)
    return lo(m.filter(ImageFilter.GaussianBlur(blur * SS / 2)))


def crescent(y0, y1, off0, off1, sign=-1):
    """A sliver hugging the inside of the wall, tapered at both ends."""
    n = 40
    a, b = [], []
    for i in range(n + 1):
        t = i / n
        y = y0 + (y1 - y0) * t
        w = math.sin(math.pi * t) ** 0.7
        r = profile(y)
        a.append((CX + sign * (r - off0), y))
        b.append((CX + sign * (r - off0 - (off1 - off0) * w), y))
    return a + list(reversed(b))


def vfade(mask, y0, y1, a0, a1):
    p = mask.load()
    for y in range(max(0, int(y0)), min(H, int(y1) + 1)):
        k = lerp(a0, a1, clamp((y - y0) / max(1, y1 - y0)))
        for x in range(0, W):
            if p[x, y]:
                p[x, y] = int(p[x, y] * k)
    return mask


# upper bulb: tall rounded window highlight, the signature of the look
capsule, d = hi_mask()
d.rounded_rectangle(((CX - 58) * SS, 36 * SS, (CX - 47) * SS, 74 * SS), 5 * SS, fill=255)
capsule = lo(capsule.filter(ImageFilter.GaussianBlur(0.6 * SS)))
capsule = vfade(capsule, 36, 76, 1.0, 0.0)
front = front_over(front, capsule, (226, 214, 255), 0.70)
# thin crescents along the walls
m = vfade(gloss(crescent(38, 112, 5, 10), 1.2), 38, 112, 0.65, 0.05)
front = front_over(front, m, (214, 196, 255), 0.42)
m = vfade(gloss(crescent(44, 104, 4, 7, sign=1), 1.0), 44, 104, 0.8, 0.1)
front = front_over(front, m, (176, 160, 255), 0.26)
# lower bulb: broad curved gloss on the left, thin return on the right
m = vfade(gloss(crescent(214, 285, 7, 20), 2.2), 214, 285, 0.2, 1.0)
front = front_over(front, m, (206, 190, 255), 0.34)
m = vfade(gloss(crescent(232, 284, 4, 7, sign=1), 1.0), 232, 284, 0.1, 0.9)
front = front_over(front, m, (176, 160, 255), 0.3)

emit("synth", bg, front, g, dyn=(CX - 81, 50, 162, 245), portrait=P, oy=OY, card=((7, 5, 20), 0.86, (118, 92, 240), 0.42))
