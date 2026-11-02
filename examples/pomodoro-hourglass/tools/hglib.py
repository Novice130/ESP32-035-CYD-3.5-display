#!/usr/bin/env python3
"""Shared art kit for the hourglass scenes. Host only: Python 3 + Pillow, no numpy.

A scene script paints a full-screen portrait background (320x480, screen coordinates), then calls
hero_view() and paints the hourglass in "hero space": x is the screen x, y is the screen y minus oy.
Scene code (include/scenes/*.h) works in hero space too, so oy can move a glass up or down.
With oy=80 the neck at hero row 160 lands on the crack at screen row 240.
Output of emit(): include/art/<name>.h
Everything must stay deterministic: use random.Random(seed), never the global random.
"""
import math
import os
import random
from PIL import Image, ImageDraw, ImageFilter

W, H = 320, 480
HERO_ROWS = 320
CARD_TOP = (6, 3, 314, 77)         # clock card, above the glass
CARD_BOTTOM = (6, 402, 314, 478)   # quote card, below the glass
MAX_DYN_PIXELS = 40000
MAX_FRONT_PIXELS = 14000

HERE = os.path.dirname(os.path.abspath(__file__))
ART_DIR = os.path.join(HERE, "..", "include", "art")


# ------------------------------------------------------------------ small maths
def clamp(v, a=0.0, b=1.0):
    return a if v < a else b if v > b else v


def smooth(a, b, x):
    t = clamp((x - a) / (b - a)) if a != b else (0.0 if x < a else 1.0)
    return t * t * (3 - 2 * t)


def lerp(a, b, t):
    return a + (b - a) * t


def mixc(c0, c1, t):
    return tuple(c0[i] + (c1[i] - c0[i]) * t for i in range(3))


def ramp(stops, t):
    """stops = [(pos, (r,g,b)), ...] sorted by pos."""
    if t <= stops[0][0]:
        return stops[0][1]
    for (p0, c0), (p1, c1) in zip(stops, stops[1:]):
        if t <= p1:
            return mixc(c0, c1, (t - p0) / (p1 - p0 or 1))
    return stops[-1][1]


# ------------------------------------------------------------------ glass shape
class Glass:
    """Hourglass inner profile. radius(y) = inner half width (float), 0 outside."""

    def __init__(self, cx, top, neck, bot, radius_fn):
        self.cx, self.top, self.neck, self.bot, self.radius = cx, top, neck, bot, radius_fn


def classic_profile(top, neck, bot, rmax, rneck, belly=0.62, dome=0.20):
    """Teardrop bulbs: domed shoulder, long taper to a narrow neck, mirrored below."""

    def half(u):  # u: 0 at the neck, 1 at the far end of the bulb
        if u >= 1:
            return 0.0
        taper = smooth(0.0, belly, u) ** 0.85
        r = rneck + (rmax - rneck) * taper
        if u > 1 - dome:  # round the far end into a dome
            d = (u - (1 - dome)) / dome
            r *= math.sqrt(max(0.0, 1 - d * d)) ** 0.85
        return r

    def fn(y):
        if y < top or y > bot:
            return 0.0
        return half((neck - y) / (neck - top)) if y <= neck else half((y - neck) / (bot - neck))

    return fn


# ------------------------------------------------------------------ painting
def new_rgb(color=(0, 0, 0)):
    return Image.new("RGB", (W, H), tuple(int(c) for c in color))


def new_front():
    return Image.new("RGBA", (W, H), (0, 0, 0, 0))


def vgradient(img, stops, x0=0, x1=W, y0=0, y1=H):
    px = img.load()
    for y in range(y0, y1):
        c = tuple(int(v) for v in ramp(stops, (y - y0) / max(1, y1 - y0 - 1)))
        for x in range(x0, x1):
            px[x, y] = c


def hero_view(portrait, oy):
    """A canvas whose row 0 is screen row oy. Paint the hourglass on it, emit() puts it back."""
    view = Image.new("RGB", (W, H))
    view.paste(portrait.crop((0, oy, W, H)), (0, 0))
    return view


def vignette(img, strength=0.45, cx=W * 0.5, cy=H * 0.5):
    px = img.load()
    rad = math.hypot(W, H) * 0.5
    for y in range(H):
        for x in range(W):
            d = math.hypot(x - cx, y - cy) / rad
            k = 1 - strength * smooth(0.45, 1.05, d)
            r, g, b = px[x, y]
            px[x, y] = (int(r * k), int(g * k), int(b * k))


def add_glow(img, draw_fn, blur, gain=1.0):
    """Add a blurred mask-coloured layer to an RGB image. draw_fn(ImageDraw) paints on black."""
    layer = Image.new("RGB", img.size, (0, 0, 0))
    draw_fn(ImageDraw.Draw(layer))
    if blur:
        layer = layer.filter(ImageFilter.GaussianBlur(blur))
    a, b = img.load(), layer.load()
    for y in range(img.size[1]):
        for x in range(img.size[0]):
            lr, lg, lb = b[x, y]
            if lr or lg or lb:
                r, g, bb = a[x, y]
                a[x, y] = (min(255, int(r + lr * gain)), min(255, int(g + lg * gain)), min(255, int(bb + lb * gain)))


def blend_px(px, x, y, color, alpha):
    if alpha <= 0 or not (0 <= x < W and 0 <= y < H):
        return
    r, g, b = px[x, y][:3]
    px[x, y] = (int(r + (color[0] - r) * alpha), int(g + (color[1] - g) * alpha), int(b + (color[2] - b) * alpha))


def over_px(px, x, y, color, alpha):
    """Composite color/alpha over an RGBA pixel (front layer)."""
    if alpha <= 0 or not (0 <= x < W and 0 <= y < H):
        return
    r0, g0, b0, a0 = px[x, y]
    a0 /= 255.0
    a = alpha + a0 * (1 - alpha)
    if a <= 0:
        return
    out = [(color[i] * alpha + (r0, g0, b0)[i] * a0 * (1 - alpha)) / a for i in range(3)]
    px[x, y] = (int(out[0]), int(out[1]), int(out[2]), int(a * 255))


def metal_bar(img, x0, x1, y0, y1, stops, radius=2.0, top_light=None, bottom_dark=None):
    """A horizontal turned-metal band. stops are a left-to-right ramp, anti-aliased rounded ends.
    Works on RGB and RGBA images."""
    px = img.load()
    rgba = img.mode == "RGBA"
    for y in range(int(y0), int(math.ceil(y1))):
        for x in range(int(x0), int(math.ceil(x1))):
            dx = max(x0 + radius - (x + .5), (x + .5) - (x1 - radius), 0)
            dy = max(y0 + radius - (y + .5), (y + .5) - (y1 - radius), 0)
            cov = clamp(radius + 0.5 - math.hypot(dx, dy))
            if cov <= 0:
                continue
            t = (x + .5 - x0) / (x1 - x0)
            c = ramp(stops, t)
            v = (y + .5 - y0) / (y1 - y0)
            if top_light is not None and v < 0.25:
                c = mixc(c, top_light, (1 - v / 0.25) * 0.6)
            if bottom_dark is not None and v > 0.7:
                c = mixc(c, bottom_dark, (v - 0.7) / 0.3 * 0.75)
            if rgba:
                over_px(px, x, y, c, cov)
            else:
                blend_px(px, x, y, c, cov)


def rounded_panel(img, x0, y0, x1, y1, r, fill, alpha, border=None, border_alpha=0.0):
    """Translucent rounded card, anti-aliased. Use it to keep text readable over art."""
    px = img.load()
    for y in range(int(y0), int(y1)):
        for x in range(int(x0), int(x1)):
            dx = max(x0 + r - (x + .5), (x + .5) - (x1 - r), 0)
            dy = max(y0 + r - (y + .5), (y + .5) - (y1 - r), 0)
            d = math.hypot(dx, dy)
            cov = clamp(r + 0.5 - d)
            if cov <= 0:
                continue
            blend_px(px, x, y, fill, alpha * cov)
            if border is not None:
                edge = clamp(1 - abs(d - (r - 0.5))) if d > r - 1.5 else 0
                if x0 + r <= x + .5 <= x1 - r:
                    edge = max(edge, clamp(1 - abs(y + .5 - y0 - .5)), clamp(1 - abs(y1 - y - 1)))
                if y0 + r <= y + .5 <= y1 - r:
                    edge = max(edge, clamp(1 - abs(x + .5 - x0 - .5)), clamp(1 - abs(x1 - x - 1)))
                blend_px(px, x, y, border, border_alpha * edge)


def paint_glass(bg, front, g, wall=2.0, edge=(200, 215, 230), body=(180, 200, 220),
                body_alpha=0.10, edge_alpha=0.85, shine=(255, 255, 255), shine_alpha=0.75, right_alpha=0.28,
                outer_glow=None, outer_glow_alpha=0.0, outer_glow_width=9.0, y_trim=0):
    """Shaded glass tube. Back: faint body tint and wall ring, on the background.
    Front: streak highlights and the wall's bright rim, painted over the contents.
    outer_glow: colour of a soft halo outside the wall, painted on the background."""
    bp, fp = bg.load(), front.load()
    cx = g.cx
    for y in range(g.top - 4 + y_trim, g.bot + 5 - y_trim):
        r = g.radius(y) if g.top <= y <= g.bot else 0.0
        rnear = max(g.radius(clamp(y + k, g.top, g.bot)) for k in (-4, 0, 4))
        ro = r + wall
        span = int(max(rnear, r) + wall + outer_glow_width + 2)
        for x in range(cx - span, cx + span + 1):
            dx = x + 0.5 - cx
            ad = abs(dx)
            if ad <= r:  # inside: fresnel tint, stronger at the edge
                u = ad / max(r, 0.5)
                blend_px(bp, x, y, body, body_alpha * (0.35 + 0.65 * u ** 3))
                # highlights in front of the contents
                if r > 5:
                    side = -1 if dx < 0 else 1
                    pos = u * side
                    k = 0.0
                    k = max(k, math.exp(-((pos + 0.66) / 0.075) ** 2) * shine_alpha)
                    k = max(k, math.exp(-((pos - 0.80) / 0.05) ** 2) * right_alpha)
                    # fade highlights near the neck and ends of each bulb
                    fade = smooth(g.top + 2, g.top + 14, y) * smooth(g.bot - 2, g.bot - 14, y)
                    fade *= smooth(6, 16, abs(y - g.neck))
                    over_px(fp, x, y, shine, k * fade)
                    # soft broad sheen on the left
                    over_px(fp, x, y, shine, 0.07 * fade * math.exp(-((pos + 0.45) / 0.30) ** 2))
            # the wall: real distance to the curve, so it closes properly over the domes
            if ad > r - 3 or y < g.top + 8 or y > g.bot - 8:
                dist = 1e9
                for k in range(-20, 21):
                    ry = y + 0.5 + k * 0.25
                    rr = g.radius(ry) if g.top <= ry <= g.bot else 0.0
                    if rr <= 0.0:
                        continue
                    dist = min(dist, math.hypot(ad - rr, k * 0.25))
                inside = g.top <= y <= g.bot and ad < r
                cov = clamp(0.5 - dist) if inside else clamp(wall + 0.5 - dist)
                if cov > 0:
                    over_px(fp, x, y, edge, edge_alpha * cov * (1.0 if dx < 0 else 0.75))
            if outer_glow is not None and ad >= ro:
                d = (ad - ro) / outer_glow_width
                if d < 1:
                    blend_px(bp, x, y, outer_glow, outer_glow_alpha * (1 - d) ** 2)


def shadow_ellipse(img, cx, cy, rx, ry, alpha=0.5, blur=6, color=(0, 0, 0)):
    mask = Image.new("L", img.size, 0)
    ImageDraw.Draw(mask).ellipse((cx - rx, cy - ry, cx + rx, cy + ry), fill=int(255 * alpha))
    mask = mask.filter(ImageFilter.GaussianBlur(blur))
    img.paste(Image.new("RGB", img.size, color), (0, 0), mask)


# ------------------------------------------------------------------ export
BAYER = [0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5]


def dither565(x, y, r, g, b):
    t = BAYER[(y & 3) * 4 + (x & 3)] / 16.0 - 0.47
    r = int(clamp(r + t * 8, 0, 255))
    g = int(clamp(g + t * 4, 0, 255))
    b = int(clamp(b + t * 8, 0, 255))
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)


def rgb565(c):
    return ((int(c[0]) & 0xF8) << 8) | ((int(c[1]) & 0xFC) << 3) | (int(c[2]) >> 3)


def glass_dyn_rect(g, pad_x=4, extra_top=0, extra_bot=0):
    x0 = 10**9
    x1 = -1
    for y in range(g.top, g.bot + 1):
        r = g.radius(y)
        x0 = min(x0, int(g.cx - r))
        x1 = max(x1, int(g.cx + r) + 1)
    return (x0 - pad_x, g.top - 2 - extra_top, x1 - x0 + 2 * pad_x, g.bot - g.top + 5 + extra_top + extra_bot)


def emit(name, bg, front, g, dyn=None, portrait=None, oy=80, card=None):
    """Write include/art/<name>.h. bg and front are hero-space canvases, portrait the full-screen
    background they came from. card = (fill, alpha, border, border_alpha) for the two text cards."""
    assert bg.size == (W, H) and portrait.size == (W, H)
    dx, dy, dw, dh = dyn or glass_dyn_rect(g)
    assert dx >= 0 and dy >= 0 and dx + dw <= W and dy + dh <= HERO_ROWS, "dynamic rect off screen"
    assert dw <= 255, "dynamic rect wider than 255"
    assert dw * dh <= MAX_DYN_PIXELS, f"dynamic rect {dw}x{dh}={dw*dh} over {MAX_DYN_PIXELS}"
    assert CARD_TOP[3] <= dy + oy and dy + dh + oy <= CARD_BOTTOM[1], "dynamic rect runs under a text card"

    screen = portrait.convert("RGB")
    screen.paste(bg.crop((0, 0, W, H - oy)), (0, oy))
    if card:
        for c in (CARD_TOP, CARD_BOTTOM):
            rounded_panel(screen, *c, 10, *card)
    bpx = screen.load()
    words = []
    for y in range(H):
        for x in range(W):
            r, gg, b = bpx[x, y]
            words.append(dither565(x, y, r, gg, b))

    # front layer -> runs (only inside the dynamic rect; the rest is baked into bg)
    idx, stream = [], bytearray()
    front_count = 0
    if front is not None:
        fpx = front.load()
        # anything outside the dynamic rect is flattened into the background
        for y in range(H - oy):
            for x in range(W):
                if dx <= x < dx + dw and dy <= y < dy + dh:
                    continue
                r, gg, b, a = fpx[x, y]
                if a:
                    base = words[(y + oy) * W + x]
                    br, bg_, bb = ((base >> 11) & 31) * 255 // 31, ((base >> 5) & 63) * 255 // 63, (base & 31) * 255 // 31
                    k = a / 255
                    words[(y + oy) * W + x] = dither565(x, y + oy, br + (r - br) * k, bg_ + (gg - bg_) * k, bb + (b - bb) * k)
        for y in range(dy, dy + dh):
            idx.append(len(stream))
            runs = []
            x = 0
            while x < dw:
                if fpx[dx + x, y][3] < 6:
                    x += 1
                    continue
                s = x
                while x < dw and fpx[dx + x, y][3] >= 6 and x - s < 255:
                    x += 1
                runs.append((s, x - s))
            assert len(runs) < 256
            stream.append(len(runs))
            for s, n in runs:
                stream.append(s)
                stream.append(n)
                for i in range(n):
                    r, gg, b, a = fpx[dx + s + i, y]
                    c = rgb565((r, gg, b))
                    stream += bytes((c & 255, c >> 8, 255 if a >= 250 else a))
                    front_count += 1
    assert front_count <= MAX_FRONT_PIXELS, f"front layer {front_count} px over {MAX_FRONT_PIXELS}"

    gl, gr = [], []
    for y in range(HERO_ROWS):
        r = g.radius(y) if g.top <= y <= g.bot else 0.0
        gl.append(int(round((g.cx - r) * 16)))
        gr.append(int(round((g.cx + r) * 16)))

    os.makedirs(ART_DIR, exist_ok=True)
    path = os.path.join(ART_DIR, f"{name}.h")
    with open(path, "w") as f:
        f.write(f"// Generated by tools/art_{name}.py - do not edit.\n#pragma once\n#include \"../hg_core.h\"\n")
        f.write(f"static const uint16_t ART_{name}_bg[{W*H}] = {{\n")
        for i in range(0, len(words), 24):
            f.write(",".join(f"0x{v:04X}" for v in words[i:i + 24]) + ",\n")
        f.write("};\n")
        if front is not None:
            f.write(f"static const uint32_t ART_{name}_fidx[{dh}] = {{" + ",".join(map(str, idx)) + "};\n")
            f.write(f"static const uint8_t ART_{name}_front[{len(stream)}] = {{\n")
            for i in range(0, len(stream), 40):
                f.write(",".join(str(v) for v in stream[i:i + 40]) + ",\n")
            f.write("};\n")
        f.write(f"static const int16_t ART_{name}_gl[{HERO_ROWS}] = {{" + ",".join(map(str, gl)) + "};\n")
        f.write(f"static const int16_t ART_{name}_gr[{HERO_ROWS}] = {{" + ",".join(map(str, gr)) + "};\n")
        fr = (f"ART_{name}_front, ART_{name}_fidx" if front is not None else "nullptr, nullptr")
        f.write(f"static const SceneArt ART_{name} = {{ ART_{name}_bg, {fr}, ART_{name}_gl, ART_{name}_gr, "
                f"{dx}, {dy}, {dw}, {dh}, {g.cx}, {g.top}, {g.neck}, {g.bot}, {oy} }};\n")
    print(f"{name}: dyn {dx},{dy} {dw}x{dh} = {dw*dh} px, front {front_count} px, wrote {os.path.relpath(path)}")
    return path
