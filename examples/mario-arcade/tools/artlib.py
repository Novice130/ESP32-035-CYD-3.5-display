"""
artlib.py - tiny pixel-art construction kit used by make_art.py

Creatures are built from simple shapes (ellipses, polygons, thick lines) drawn at
4x supersampling into a *part map*, then reduced to the target resolution.  After
reduction every part is automatically:
  * outlined   - dark, hue-matched outline on silhouette edges (Gen 5 style)
  * separated  - internal lines where a part overlaps a part drawn before it
  * cel-shaded - 1-light-source shading (top-left light): shadow band + highlight
Finally hand-placed detail pixels (eyes, sparkles) are stamped on top.
"""
import colorsys
from PIL import Image, ImageDraw

SS = 4  # supersampling factor


def clamp(v, lo=0.0, hi=1.0):
    return max(lo, min(hi, v))


def _hsv_adj(rgb, dv, ds, hue_target, hue_amt):
    r, g, b = [c / 255 for c in rgb]
    h, s, v = colorsys.rgb_to_hsv(r, g, b)
    # nudge hue toward hue_target (0..1) along shortest path
    d = ((hue_target - h + 0.5) % 1.0) - 0.5
    if s > 0.08:
        h = (h + d * hue_amt) % 1.0
    s = clamp(s * ds)
    v = clamp(v * dv)
    r, g, b = colorsys.hsv_to_rgb(h, s, v)
    return (round(r * 255), round(g * 255), round(b * 255))


def shade_of(rgb):
    return _hsv_adj(rgb, 0.76, 1.12, 0.70, 0.10)


def light_of(rgb):
    r, g, b = rgb
    l = _hsv_adj(rgb, 1.0, 0.70, 0.16, 0.08)
    return tuple(min(255, int(c + (255 - c) * 0.35)) for c in l)


def outline_of(rgb):
    o = _hsv_adj(rgb, 0.36, 1.25, 0.72, 0.15)
    return tuple(min(c, 70) for c in o) if sum(rgb) < 120 else o


class Canvas:
    def __init__(self, w, h):
        self.w, self.h = w, h
        self.big = Image.new("L", (w * SS, h * SS), 0)
        self.dr = ImageDraw.Draw(self.big)
        self.parts = [None]  # index 0 = empty
        self.byname = {}
        self.details = []  # (x, y, rgb)

    # ---------------------------------------------------------------- parts
    def part(self, name, rgb, line=True, shade=True, outline=None, light=True):
        """line: draw separation line where this part overlaps earlier parts.
        shade: apply cel shading. outline: override outline colour."""
        idx = len(self.parts)
        self.parts.append(dict(name=name, rgb=rgb, line=line, shade=shade, light=light,
                               outline=outline or outline_of(rgb)))
        self.byname[name] = idx
        return name

    def _id(self, name):
        return self.byname[name]

    # ---------------------------------------------------------------- shapes
    def ellipse(self, name, cx, cy, rx, ry):
        b = [(cx - rx) * SS, (cy - ry) * SS, (cx + rx) * SS - 1, (cy + ry) * SS - 1]
        self.dr.ellipse(b, fill=self._id(name))

    def circle(self, name, cx, cy, r):
        self.ellipse(name, cx, cy, r, r)

    def poly(self, name, pts):
        self.dr.polygon([(x * SS, y * SS) for x, y in pts], fill=self._id(name))

    def rect(self, name, x0, y0, x1, y1):
        self.dr.rectangle([x0 * SS, y0 * SS, x1 * SS - 1, y1 * SS - 1], fill=self._id(name))

    def thick(self, name, pts, width):
        pid = self._id(name)
        p = [(x * SS, y * SS) for x, y in pts]
        self.dr.line(p, fill=pid, width=int(width * SS), joint="curve")
        r = width * SS / 2
        for x, y in p:
            self.dr.ellipse([x - r, y - r, x + r, y + r], fill=pid)

    def erase_ellipse(self, cx, cy, rx, ry):
        b = [(cx - rx) * SS, (cy - ry) * SS, (cx + rx) * SS - 1, (cy + ry) * SS - 1]
        self.dr.ellipse(b, fill=0)

    def erase_poly(self, pts):
        self.dr.polygon([(x * SS, y * SS) for x, y in pts], fill=0)

    # ---------------------------------------------------------------- details
    def px(self, x, y, rgb):
        self.details.append((int(x), int(y), rgb))

    def pxs(self, pts, rgb):
        for x, y in pts:
            self.px(x, y, rgb)

    def hline(self, x0, x1, y, rgb):
        for x in range(int(x0), int(x1) + 1):
            self.px(x, y, rgb)

    def eye(self, x, y, w=2, h=3, iris=(30, 30, 40), shine=(255, 255, 255), iris2=None):
        """Classic GBA-style eye: dark oval with a white catch-light top-left."""
        for yy in range(h):
            for xx in range(w):
                c = iris
                if iris2 and yy >= h - 1 - (h > 3):
                    c = iris2
                self.px(x + xx, y + yy, c)
        self.px(x, y, shine)

    # ---------------------------------------------------------------- render
    def _reduce(self):
        W, H = self.w, self.h
        src = self.big.load()
        grid = [[0] * W for _ in range(H)]
        half = SS * SS // 2
        for y in range(H):
            for x in range(W):
                counts = {}
                for sy in range(SS):
                    for sx in range(SS):
                        v = src[x * SS + sx, y * SS + sy]
                        counts[v] = counts.get(v, 0) + 1
                if counts.get(0, 0) > half:
                    continue
                counts.pop(0, None)
                # prefer topmost (highest id) on ties -> crisper overlaps
                grid[y][x] = max(counts.items(), key=lambda kv: (kv[1], kv[0]))[0]
        return grid

    def render(self):
        W, H = self.w, self.h
        g = self._reduce()
        out = [[None] * W for _ in range(H)]

        def at(x, y):
            if 0 <= x < W and 0 <= y < H:
                return g[y][x]
            return 0

        for y in range(H):
            for x in range(W):
                p = g[y][x]
                if not p:
                    continue
                P = self.parts[p]
                col = P["rgb"]
                # silhouette edge -> outline
                if any(at(x + dx, y + dy) == 0 for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1))):
                    out[y][x] = P["outline"]
                    continue
                # internal separation line (on the part that sits on top)
                if P["line"]:
                    sep = False
                    for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                        q = at(x + dx, y + dy)
                        if q and q != p and q < p and self.parts[q]["line"] is not None:
                            sep = True
                            break
                    if sep:
                        out[y][x] = P["outline"]
                        continue
                if P["shade"]:
                    # light from upper-left: pixels near lower-right part edge get shade
                    if at(x + 2, y + 2) != p or at(x + 1, y + 2) != p or at(x + 2, y + 1) != p:
                        col = shade_of(col)
                    elif P["light"] and (at(x - 2, y - 2) != p) and at(x - 1, y - 1) == p and \
                            ((x + y) % 2 == 0 or at(x - 3, y - 3) != p):
                        col = light_of(col)
                out[y][x] = col
        for x, y, rgb in self.details:
            if 0 <= x < W and 0 <= y < H:
                out[y][x] = rgb
        return out


def to_image(pix, scale=1, bg=None):
    H = len(pix)
    W = len(pix[0])
    im = Image.new("RGBA", (W, H), (0, 0, 0, 0) if bg is None else bg + (255,))
    pp = im.load()
    for y in range(H):
        for x in range(W):
            if pix[y][x] is not None:
                pp[x, y] = pix[y][x] + (255,)
    if scale != 1:
        im = im.resize((W * scale, H * scale), Image.NEAREST)
    return im


def from_ascii(rows, pal):
    """rows: list[str]; pal: {char: rgb}. '.' or ' ' = transparent."""
    out = []
    for r in rows:
        out.append([None if ch in ". " else pal[ch] for ch in r])
    return out


def hflip(pix):
    return [list(reversed(r)) for r in pix]
