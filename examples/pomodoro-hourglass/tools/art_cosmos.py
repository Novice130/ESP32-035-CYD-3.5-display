#!/usr/bin/env python3
"""Event Horizon: an hourglass holding the universe. Deep space, a nearly invisible glass and two
obsidian rings. Also bakes the star catalogue, the accretion ring map and the newborn nebula."""
import math
import random
from PIL import Image, ImageDraw, ImageFilter
from hglib import *

rng = random.Random(1977)
CX, TOP, NECK, BOT = 160, 15, 160, 305
OY = 80
RMAX = 84
g = Glass(CX, TOP, NECK, BOT, classic_profile(TOP, NECK, BOT, RMAX, 2.8, belly=0.62, dome=0.30))
DYN = (75, 40, 170, 235)


# ---------------------------------------------------------------- value noise
class Noise:
    def __init__(self, seed, size=256):
        r = random.Random(seed)
        self.size = size
        self.t = [r.random() for _ in range(size * size)]

    def v(self, x, y):
        n = self.size
        ix, iy = math.floor(x), math.floor(y)
        fx, fy = x - ix, y - iy
        fx = fx * fx * (3 - 2 * fx)
        fy = fy * fy * (3 - 2 * fy)
        t = self.t
        a = t[(iy % n) * n + (ix % n)]
        b = t[(iy % n) * n + ((ix + 1) % n)]
        c = t[((iy + 1) % n) * n + (ix % n)]
        d = t[((iy + 1) % n) * n + ((ix + 1) % n)]
        return (a + (b - a) * fx) * (1 - fy) + (c + (d - c) * fx) * fy

    def fbm(self, x, y, oct=5):
        s, amp, tot = 0.0, 0.5, 0.0
        for i in range(oct):
            s += self.v(x, y) * amp
            tot += amp
            x, y = x * 2.03 + 17.1, y * 2.03 + 9.2
            amp *= 0.5
        return s / tot


n1, n2, n3, n4 = Noise(11), Noise(23), Noise(37), Noise(51)

# ---------------------------------------------------------------- deep space, half resolution then upsampled
lo = Image.new("RGB", (W // 2, H // 2))
lp = lo.load()
for j in range(H // 2):
    for i in range(W // 2):
        x, y = i * 2.0, j * 2.0
        wx = n3.fbm(x * 0.009, y * 0.009, 3) - 0.5
        wy = n4.fbm(x * 0.009 + 5, y * 0.009 + 3, 3) - 0.5
        a = n1.fbm(x * 0.008 + wx * 3.2, y * 0.010 + wy * 3.2, 5)
        b = n2.fbm(x * 0.012 - wy * 2.4 + 8, y * 0.012 + wx * 2.4, 5)
        a = smooth(0.46, 0.80, a)
        b = smooth(0.50, 0.78, b)
        # the clouds gather in a diagonal band through the hourglass
        band = math.exp(-(((y - 240) - (x - 160) * 0.55) / 150.0) ** 2)
        c = (5, 6, 15)
        c = mixc(c, (44, 30, 96), a * band * 0.90)
        c = mixc(c, (118, 40, 96), a * b * band * 0.72)
        c = mixc(c, (18, 40, 88), b * (1 - a) * 0.50)
        lp[i, j] = tuple(int(v) for v in c)
P = lo.resize((W, H), Image.BICUBIC)
vignette(P, 0.45, CX, 240)
# stars above and below the hero canvas
pp = P.load()
for i in range(240):
    y = rng.randrange(0, OY) if i % 2 else rng.randrange(OY + HERO_ROWS, H)
    blend_px(pp, rng.randrange(W), y, rng.choice([(255, 255, 255), (214, 228, 255), (255, 240, 224)]), 0.12 + 0.8 * rng.random() ** 4.5)
bg = hero_view(P, OY)

# ---------------------------------------------------------------- the inside of the glass is a little darker
px = bg.load()
for y in range(TOP, BOT + 1):
    r = g.radius(y)
    for x in range(int(CX - r), int(CX + r) + 2):
        d = abs(x + .5 - CX)
        if d <= r:
            k = 1 - 0.34 * clamp(1 - d / max(r, 1)) ** 0.6
            q = px[x, y]
            px[x, y] = (int(q[0] * k), int(q[1] * k), int(q[2] * k))

# ---------------------------------------------------------------- fine stars
WHITES = [(255, 255, 255), (214, 228, 255), (190, 210, 255), (255, 240, 224), (255, 218, 190)]
twinkle = []
for i in range(800):
    x, y = rng.randrange(W), rng.randrange(HERO_ROWS)
    m = rng.random() ** 4.5
    c = rng.choice(WHITES)
    v = 0.12 + 0.80 * m
    inside = g.top <= y <= g.bot and abs(x + .5 - CX) < g.radius(y)
    if inside:      # the bulbs are for their own contents
        if rng.random() < 0.5:
            continue
        m *= 0.3
        v *= 0.6
    in_dyn = DYN[0] <= x < DYN[0] + DYN[2] - 1 and DYN[1] <= y < DYN[1] + DYN[3]
    if in_dyn and not inside and m > 0.004 and len(twinkle) < 44 and abs(y - NECK) > 18:
        # lives in the dynamic rect, twinkles there: not baked
        twinkle.append((x, y, int(10 + 21 * v), rng.randrange(4), rng.randrange(256), rng.randrange(1, 5)))
        continue
    blend_px(px, x, y, c, v)
    if m > 0.45:
        blend_px(px, x + 1, y, c, v * 0.45)
for i in range(11):   # a few big ones with a tiny halo, away from the text
    x, y = rng.randrange(W), rng.randrange(HERO_ROWS)
    if abs(x - CX) < g.radius(y) + 4:
        continue
    c = rng.choice(WHITES)
    for dy in range(-3, 4):
        for dx in range(-3, 4):
            blend_px(px, x + dx, y + dy, c, 0.75 * math.exp(-(dx * dx + dy * dy) / 1.5))
    for k in range(1, 5):
        a = 0.34 * (1 - k / 5)
        for (dx, dy) in ((k, 0), (-k, 0), (0, k), (0, -k)):
            blend_px(px, x + dx, y + dy, c, a)


# ---------------------------------------------------------------- obsidian rings
def ring_layer(img, cy, rx, ry, thick, front_half):
    """A slim obsidian hoop seen slightly from above: dark polished top face, darker outer wall,
    a thin line of starlight along the rim. front_half picks the near or the far part."""
    p = img.load()
    rgba = img.mode == "RGBA"
    SS = 4
    WIDTH = 0.11
    for y in range(int(cy - ry - 2), int(cy + ry + thick + 3)):
        for x in range(int(CX - rx - 2), int(CX + rx + 3)):
            r_, g_, b_, cov = 0.0, 0.0, 0.0, 0
            for sy in range(SS):
                for sx in range(SS):
                    u = (x + (sx + .5) / SS - CX) / rx
                    v0 = y + (sy + .5) / SS - cy
                    d0 = math.hypot(u, v0 / ry)
                    c = None
                    if 1 - WIDTH <= d0 <= 1:                 # top face
                        if (v0 > 0) == front_half:
                            spec = math.exp(-((u + 0.46) / 0.17) ** 2) + 0.5 * math.exp(-((u - 0.60) / 0.10) ** 2)
                            c = mixc((20, 20, 34), (150, 162, 225), min(1, spec) * (1.0 if front_half else 0.5))
                            if d0 > 1 - WIDTH * 0.32:        # bevel
                                c = mixc(c, (190, 204, 250), 0.55 if front_half else 0.28)
                    elif front_half and v0 > 0:               # the near outer wall hangs below the face
                        d1 = math.hypot(u, (v0 - thick) / ry)
                        if d0 > 1 and d1 <= 1:
                            spec = math.exp(-((u + 0.46) / 0.20) ** 2)
                            c = mixc((9, 9, 16), (64, 70, 110), spec)
                    if c is not None:
                        r_ += c[0]; g_ += c[1]; b_ += c[2]; cov += 1
            if not cov:
                continue
            c = (r_ / cov, g_ / cov, b_ / cov)
            a = cov / (SS * SS)
            if rgba:
                over_px(p, x, y, c, a)
            else:
                blend_px(p, x, y, c, a)


ring_layer(bg, 9, 62, 6, 2.5, False)
ring_layer(bg, 306, 68, 7, 3, False)

front = new_front()
# the wall is thin and lives in the background: stars never cross it. Starlight varies along it.
def wall_painter():
    SS = 3
    for y in range(TOP - 2, BOT + 3):
        r = g.radius(y + .5) if TOP <= y <= BOT else 0.0
        rn = max(g.radius(clamp(y + k, TOP, BOT)) for k in (-3, -1.5, 0, 1.5, 3))
        for x in range(int(CX - rn - 4), int(CX + rn + 5)):
            ad = abs(x + .5 - CX)
            if ad < r - 3 and TOP + 7 < y < BOT - 7:
                continue
            dist = 1e9
            for k in range(-16, 17):
                ry = y + .5 + k * 0.25
                rr = g.radius(ry) if TOP <= ry <= BOT else 0.0
                if rr > 0:
                    dist = min(dist, math.hypot(ad - rr, k * 0.25))
            cov = clamp(1.1 - dist)
            if cov <= 0:
                continue
            lightn = n2.fbm(x * 0.012 + 40, y * 0.020 + 11, 3)
            k = 0.08 + 0.50 * smooth(0.42, 0.70, lightn)
            if x < CX:
                k *= 1.25
            blend_px(px, x, y, (176, 196, 250), clamp(k) * cov)
            if dist < 4:   # the faintest halo either side of the wall
                pass
wall_painter()
ring_layer(front, 9, 62, 6, 2.5, True)
ring_layer(front, 306, 68, 7, 3, True)

# starlight caught on the rim: little bright beads along the wall
fp = front.load()
for i in range(18):
    y = rng.randrange(TOP + 4, BOT - 3)
    if abs(y - NECK) < 22:
        continue
    side = rng.choice((-1, -1, 1))
    v = 0.30 + 0.70 * rng.random() ** 2
    for dy in range(-5, 6):
        x = CX + side * g.radius(y + dy + .5)
        over_px(fp, int(x), y + dy, (225, 235, 255), v * math.exp(-(dy * dy) / 7.0))
path = emit("cosmos", bg, front, g, dyn=DYN, portrait=P, oy=OY, card=((7, 8, 19), 0.88, (150, 160, 225), 0.30))


# ================================================================ tables for the scene
def table(f, ctype, name, vals, per=32):
    f.write(f"static const {ctype} {name}[{len(vals)}] = {{\n")
    for i in range(0, len(vals), per):
        f.write(",".join(str(v) for v in vals[i:i + per]) + ",\n")
    f.write("};\n")


# ---- star catalogue, sorted by radius, so "the innermost n stars" is one number.
# r is in thirds of a pixel. kind 0 = free orbit, 1 = arm star, 2 = gas puff.
R_MAX = 80.0
PITCH = 0.44
stars = []


def arm_angle(r):
    return math.log(max(r, 3.0) / 10.0) / PITCH


def add(r, ang, lvl, col, kind):
    stars.append((min(255, int(r * 3)), int(ang / (2 * math.pi) * 65536) & 0xFFFF, lvl, col, kind, rng.randrange(256)))


for i in range(240):            # bulge: old gold stars
    r = 1.0 + 15 * rng.random() ** 1.7
    add(r, rng.random() * 6.2832, rng.randint(9, 24), 1, 0)
for i in range(280):            # disc: faint field stars on true orbits
    r = 7 + (R_MAX - 7) * rng.random() ** 0.8
    add(r, rng.random() * 6.2832, 5 + int(13 * rng.random() ** 2.0), 0 if rng.random() < 0.82 else 1, 0)
for i in range(700):            # arms: young blue-white stars, born in the density wave
    r = 12 + (R_MAX - 12) * rng.random() ** 0.62
    arm = rng.randrange(2)
    th = arm_angle(r) + arm * math.pi + rng.gauss(0, 0.17 - 0.06 * (r / R_MAX))
    add(r, th, 13 + int(18 * rng.random() ** 1.5), 0, 1)
for i in range(300):            # arms: glowing gas
    r = 12 + (R_MAX - 12) * rng.random() ** 0.7
    arm = rng.randrange(2)
    th = arm_angle(r) + arm * math.pi + rng.gauss(-0.05, 0.14)
    add(r, th, rng.randint(4, 8), 2 if rng.random() < 0.6 else 3, 2)
stars.sort(key=lambda s: s[0])

# ---- black hole: thin tilted disc, shadow, photon ring, lensed far side
RW, RH = 76, 40
SHADOW = 7.6
ROLL = math.radians(-13)
QUASH = 0.165
ring = []
cr, sr = math.cos(ROLL), math.sin(ROLL)
for j in range(RH):
    for i in range(RW):
        total, a_s = 0.0, None
        for sy in (.2, .5, .8):
            for sx in (.2, .5, .8):
                dx = i + sx - RW / 2
                dy = j + sy - RH / 2
                u = dx * cr + dy * sr
                v = -dx * sr + dy * cr
                Y = v / QUASH
                rho = math.hypot(u, Y)
                phi = math.atan2(Y, u)
                rad = math.hypot(dx, dy)
                e = 0.0
                if 11 < rho < 36:
                    prof = smooth(11, 13.5, rho) * (1 - smooth(18, 36, rho)) ** 1.5
                    dop = 1 + 0.72 * (-math.cos(phi - 0.3))   # the approaching side is brighter
                    if not (v < 0 and rad < SHADOW + 0.5):    # the far half passes behind the shadow
                        e += prof * dop * 0.85
                # photon ring, razor thin, all the way round
                e += 1.15 * math.exp(-((rad - (SHADOW + 1.1)) / 0.62) ** 2) * (1.0 + 0.35 * (-dx / max(rad, 1)))
                # the far side of the disc, lensed up and over the shadow
                if v < 0.5:
                    e += 0.40 * math.exp(-((rad - 11.4) / 1.3) ** 2) * smooth(0.5, -4.0, v)
                else:
                    e += 0.10 * math.exp(-((rad - 10.6) / 0.9) ** 2)
                total += e
        total /= 9
        dx = i + .5 - RW / 2
        dy = j + .5 - RH / 2
        u = dx * cr + dy * sr
        v = -dx * sr + dy * cr
        Y = v / QUASH
        rho = max(math.hypot(u, Y), 1)
        rad = math.hypot(dx, dy)
        if rho < 10 or rad < 13.5:
            ang = math.atan2(dy, dx) * 2
        else:
            ang = math.atan2(Y, u) + math.log(rho) * 2.2
        ring += [int(ang / (2 * math.pi) * 256) & 255, min(255, int(total * 255))]

# ---- newborn nebula: 5 bits of light + 3 bits of hue, and d, the moment each pixel is reached
NEB_X, NEB_Y, NEB_W, NEB_H = 77, 190, 166, 84
NEB_CX, NEB_CY = CX, 234
neb = []
for j in range(NEB_H):
    for i in range(NEB_W):
        x, y = NEB_X + i + .5, NEB_Y + j + .5
        dx, dy = x - NEB_CX, y - NEB_CY
        rr = g.radius(y)
        wall = rr - abs(dx)
        if wall < 1:
            neb += [0, 255]
            continue
        wx = n3.fbm(x * 0.03, y * 0.03, 3) - 0.5
        wy = n4.fbm(x * 0.03 + 7, y * 0.03 + 2, 3) - 0.5
        q = n1.fbm(x * 0.028 + wx * 2.6, y * 0.040 + wy * 2.6, 5)
        q2 = n2.fbm(x * 0.05 - wy * 2.0, y * 0.06 + wx * 2.0, 4)
        e = math.hypot(dx / 80.0, (dy + dx * 0.10) / 38.0)
        billow = 0.62 + 0.75 * n2.fbm(x * 0.022 + 3, y * 0.03 + 8, 4)
        d = e * billow
        fall = (1 - smooth(0.55, 1.0, e)) * smooth(0, 4, wall)
        ridge = 1 - abs(2 * q - 1)            # thin bright filaments
        dust = smooth(0.50, 0.66, n4.fbm(x * 0.05 + wx * 3, y * 0.08 + wy * 3 + 20, 4))   # dark lanes
        light = fall * (0.10 + 0.95 * ridge ** 3 + 0.35 * smooth(0.45, 0.8, q)) * (1 - 0.62 * dust) + 0.42 * math.exp(-(e / 0.22) ** 2)
        hue = clamp(0.98 - e * 0.85 + (q2 - 0.5) * 1.1)
        v5 = min(31, int(light * 26))
        neb += [(v5 << 3) | min(7, int(hue * 8)), min(255, int(d * 255))]

# trim each row to the pixels that can ever show: a span table saves the scene most of the work
spans, stream = [], []
count = 0
for j in range(NEB_H):
    row = neb[j * NEB_W * 2:(j + 1) * NEB_W * 2]
    on = [i for i in range(NEB_W) if (row[i * 2] >> 3) >= 2]
    a0 = on[0] if on else 0
    a1 = on[-1] + 1 if on else 0
    spans += [count, a0, a1 - a0]
    for i in range(a0, a1):
        b = row[i * 2]
        stream += [0 if (b >> 3) < 2 else b, row[i * 2 + 1]]
    count += a1 - a0
print("nebula px", count)

with open(path, "a") as f:
    f.write(f"#define COSMOS_NSTARS {len(stars)}\n#define COSMOS_RW {RW}\n#define COSMOS_RH {RH}\n")
    f.write(f"#define COSMOS_NTW {len(twinkle)}\n")
    f.write(f"#define COSMOS_NEB_X {NEB_X}\n#define COSMOS_NEB_Y {NEB_Y}\n#define COSMOS_NEB_W {NEB_W}\n#define COSMOS_NEB_H {NEB_H}\n")
    f.write(f"#define COSMOS_NEB_CY {NEB_CY}\n")
    f.write("struct CosmosStar { uint16_t ang; uint8_t r, lc, seed, kind; };\n")
    f.write(f"static const CosmosStar ART_cosmos_stars[{len(stars)}] = {{\n")
    for k in range(0, len(stars), 12):
        f.write(",".join("{%d,%d,%d,%d,%d}" % (s[1], s[0], s[2] | (s[3] << 5), s[5], s[4]) for s in stars[k:k + 12]) + ",\n")
    f.write("};\n")
    f.write("struct CosmosTw { int16_t x, y; uint8_t lvl, col, ph, sp; };\n")
    f.write(f"static const CosmosTw ART_cosmos_tw[{len(twinkle)}] = {{" + ",".join("{%d,%d,%d,%d,%d,%d}" % t for t in twinkle) + "};\n")
    table(f, "uint8_t", "ART_cosmos_ring", ring)
    table(f, "uint8_t", "ART_cosmos_neb", stream)
    table(f, "uint16_t", "ART_cosmos_nebspan", spans)
print("stars", len(stars), "twinkle", len(twinkle))
