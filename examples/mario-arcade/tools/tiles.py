"""
tiles.py - 16x16 level tiles + scenery, drawn once with symbolic colour keys and
recoloured per theme (day, underground, sunset treetops, night castle).
"""
from artlib import from_ascii

THEMES = ["DAY", "CAVE", "DUSK", "NIGHT"]

# Per-theme colours. Keys are shared by all tile drawings below.
_BASE = {
    "k": (36, 20, 16),
    # ground
    "A": (204, 96, 44), "a": (128, 52, 22), "L": (252, 190, 140),
    # bricks
    "B": (198, 86, 38), "b": (120, 44, 18), "l": (252, 168, 116), "m": (40, 18, 10),
    # ? block
    "Y": (252, 176, 36), "y": (196, 108, 16), "O": (150, 64, 12), "W": (255, 246, 214), "w": (120, 54, 10),
    # used block
    "U": (164, 96, 52), "u": (102, 54, 26),
    # pipes
    "G": (72, 196, 64), "g": (24, 116, 36), "P": (176, 250, 140),
    # treetops + trunk
    "T": (86, 200, 72), "t": (36, 128, 44), "V": (190, 250, 150), "N": (176, 120, 64), "n": (110, 70, 34),
    # lava
    "R": (226, 54, 20), "Q": (255, 150, 30), "Z": (255, 236, 120),
    # scenery
    "H": (64, 180, 60), "h": (30, 112, 40), "J": (150, 230, 110),        # hills / bushes
    "C": (255, 255, 255), "c": (176, 204, 252),                          # clouds
    "S": (200, 200, 208), "s": (120, 116, 132), "D": (24, 16, 24),      # castle stone / door
    "F": (255, 255, 255), "f": (56, 176, 64),                            # flag
}
PALETTES = {
    "DAY": dict(_BASE),
    "CAVE": dict(_BASE,
                 k=(8, 12, 28),
                 A=(52, 124, 176), a=(22, 58, 104), L=(150, 210, 244),
                 B=(44, 130, 168), b=(18, 64, 96), l=(120, 200, 230), m=(4, 10, 24),
                 U=(84, 92, 132), u=(40, 44, 76),
                 H=(30, 60, 90), h=(14, 30, 52), J=(60, 100, 140)),
    "DUSK": dict(_BASE,
                 A=(184, 92, 64), a=(110, 46, 40), L=(250, 186, 150),
                 H=(150, 82, 140), h=(96, 46, 96), J=(206, 132, 180),
                 C=(255, 226, 214), c=(250, 176, 170)),
    "NIGHT": dict(_BASE,
                  k=(14, 14, 24),
                  A=(132, 132, 150), a=(70, 70, 90), L=(206, 206, 222),
                  B=(118, 118, 140), b=(60, 60, 80), l=(190, 190, 210), m=(14, 14, 24),
                  U=(120, 100, 90), u=(70, 56, 50),
                  H=(36, 52, 96), h=(20, 28, 60), J=(70, 90, 140),
                  C=(150, 160, 196), c=(96, 104, 150),
                  S=(110, 110, 130), s=(60, 58, 78)),
}
SKY = {"DAY": (100, 148, 252), "CAVE": (0, 0, 0), "DUSK": (248, 150, 112), "NIGHT": (16, 20, 52)}

GROUND = [
    "LLLLLLLkLLLLLLLk",
    "LAAAAAakLAAAAAak",
    "LAAAAAakLAAAAAak",
    "LAAAAAakLAAAAAak",
    "LAAAAAakLAAAAAak",
    "LAAAAAakLAAAAAak",
    "aaaaaaakaaaaaaak",
    "kkkkkkkkkkkkkkkk",
    "LLLkLLLLLLLkLLLL",
    "AAakLAAAAAakLAAA",
    "AAakLAAAAAakLAAA",
    "AAakLAAAAAakLAAA",
    "AAakLAAAAAakLAAA",
    "AAakLAAAAAakLAAA",
    "aaakaaaaaaakaaaa",
    "kkkkkkkkkkkkkkkk",
]
BRICK = [
    "llllllllllllllll",
    "BBBBBBBmBBBBBBBm",
    "BBBBBBBmBBBBBBBm",
    "bbbbbbbmbbbbbbbm",
    "mmmmmmmmmmmmmmmm",
    "BBBmlllllllmBBBB",
    "BBBmBBBBBBBmBBBB",
    "bbbmbbbbbbbmbbbb",
    "mmmmmmmmmmmmmmmm",
    "lllllllmllllllll",
    "BBBBBBBmBBBBBBBm",
    "bbbbbbbmbbbbbbbm",
    "mmmmmmmmmmmmmmmm",
    "BBBmlllllllmBBBB",
    "bbbmBBBBBBBmbbbb",
    "mmmmmmmmmmmmmmmm",
]
QBLOCK = [
    ".kkkkkkkkkkkkkk.",
    "kWYYYYYYYYYYYYyk",
    "kYOYYYYYYYYYYOyk",
    "kYYYYwwwwwYYYYyk",
    "kYYYwWWWWWwYYYyk",
    "kYYYwWWwwWWwYYyk",
    "kYYYwwwYwWWwYYyk",
    "kYYYYYYwWWwYYYyk",
    "kYYYYYwWWwYYYYyk",
    "kYYYYYwWWwYYYYyk",
    "kYYYYYYwwYYYYYyk",
    "kYYYYYwWWwYYYYyk",
    "kYYYYYwWWwYYYYyk",
    "kYOYYYYwwYYYYOyk",
    "kyyyyyyyyyyyyyyk",
    ".kkkkkkkkkkkkkk.",
]
USED = [
    ".kkkkkkkkkkkkkk.",
    "kUUUUUUUUUUUUUuk",
    "kUkUUUUUUUUUUkuk",
    "kUUUUUUUUUUUUUuk",
    "kUUUUUUUUUUUUUuk",
    "kUUUUUUUUUUUUUuk",
    "kUUUUUUUUUUUUUuk",
    "kUUUUUUUUUUUUUuk",
    "kUUUUUUUUUUUUUuk",
    "kUUUUUUUUUUUUUuk",
    "kUUUUUUUUUUUUUuk",
    "kUUUUUUUUUUUUUuk",
    "kUUUUUUUUUUUUUuk",
    "kUkUUUUUUUUUUkuk",
    "kuuuuuuuuuuuuuuk",
    ".kkkkkkkkkkkkkk.",
]
HARD = [
    "LLLLLLLLLLLLLLLk",
    "LLLLLLLLLLLLLLak",
    "LLAAAAAAAAAAAaak",
    "LLAAAAAAAAAAAaak",
    "LLAAAAAAAAAAAaak",
    "LLAAAAAAAAAAAaak",
    "LLAAAAAAAAAAAaak",
    "LLAAAAAAAAAAAaak",
    "LLAAAAAAAAAAAaak",
    "LLAAAAAAAAAAAaak",
    "LLAAAAAAAAAAAaak",
    "LLAAAAAAAAAAAaak",
    "LLAAAAAAAAAAAaak",
    "Laaaaaaaaaaaaaak",
    "Laaaaaaaaaaaaaak",
    "kkkkkkkkkkkkkkkk",
]
PIPE_TL = [
    "kkkkkkkkkkkkkkkk",
    "kGPPGGGGGGGGGGGG",
    "kGPPGGGGGGGGGGGG",
    "kGPPGGGGGGGGGGGG",
    "kGPPGGGGGGGGGGGG",
    "kGPPGGGGGGGGGGGG",
    "kGPPGGGGGGGGGGGG",
    "kGPPGGGGGGGGGGGG",
    "kGPPGGGGGGGGGGGG",
    "kGPPGGGGGGGGGGGG",
    "kGPPGGGGGGGGGGGG",
    "kGPPGGGGGGGGGGGG",
    "kGPPGGGGGGGGGGGG",
    "kgggggggggggggggg"[:16],
    "kkkkkkkkkkkkkkkk",
    "..kkkkkkkkkkkkkk",
]
PIPE_TR = [
    "kkkkkkkkkkkkkkkk",
    "GGGGGGGGgGgGgggk",
    "GGGGGGGGgGgGgggk",
    "GGGGGGGGgGgGgggk",
    "GGGGGGGGgGgGgggk",
    "GGGGGGGGgGgGgggk",
    "GGGGGGGGgGgGgggk",
    "GGGGGGGGgGgGgggk",
    "GGGGGGGGgGgGgggk",
    "GGGGGGGGgGgGgggk",
    "GGGGGGGGgGgGgggk",
    "GGGGGGGGgGgGgggk",
    "GGGGGGGGgGgGgggk",
    "gggggggggggggggk",
    "kkkkkkkkkkkkkkkk",
    "kkkkkkkkkkkkkk..",
]
PIPE_L = ["..kGPPGGGGGGGGGG"] * 16
PIPE_R = ["GGGGGGgGgGgggk.."] * 16
TREE_L = [
    "......kkkkkkkkkk",
    "....kkVVVVVVVVVV",
    "...kVVTTTTTTTTTT",
    "..kVTTTTTTTTTTTT",
    ".kVTTTTTTTTTTTTT",
    ".kVTTTTTTVTTTTTT",
    "kVTTTTTTTTTTTTTT",
    "kTTTTTTTTTTTTTTT",
    "kTTTTTTTTTTTTTTT",
    "kTTTTTTTTTTTTTTT",
    "ktTTTTTTTTTTTTTT",
    ".ktTTTTTTTTTTtTT",
    ".kttTTTTTtTTTTtT",
    "..kttttttttttttt",
    "...kkkkkkkkkkkkk",
    "................",
]
TREE_M = [
    "kkkkkkkkkkkkkkkk",
    "VVVVVVVVVVVVVVVV",
    "TTTTTTTTTTTTTTTT",
    "TTTTTTTTTTTTTTTT",
    "TTTTTVTTTTTTTTTT",
    "TTTTTTTTTTTVTTTT",
    "TTTTTTTTTTTTTTTT",
    "TTTTTTTTTTTTTTTT",
    "TTVTTTTTTTTTTTTT",
    "TTTTTTTTTVTTTTTT",
    "TTTTTTTTTTTTTTTT",
    "TTTtTTTTTTTTTtTT",
    "tTTTTTtTTTTTTTTt",
    "tttttttttttttttt",
    "kkkkkkkkkkkkkkkk",
    "................",
]
TREE_R = [r[::-1] for r in TREE_L]
TRUNK = ["...knNNNNNNNnk.."] * 4 + ["...knNNnNNNNnk..", "...knNNnNNNNnk..", "...knNNNNNNNnk..",
                                     "...knNNNNNnNnk..", "...knNNNNNnNnk..", "...knNNNNNNNnk.."] + \
        ["...knNNNNNNNnk.."] * 6
LAVA0 = [
    "ZZ......ZZ......",
    "QQZ....ZQQZ....Z",
    "RQQZZZZQRRQZZZZQ",
    "RRQQQQQRRRRQQQQR",
] + ["RRRRRRRRRRRRRRRR", "RRRRQRRRRRRRRQRR", "RRRRRRRRRRRRRRRR", "RRRRRRRRQRRRRRRR"] * 3
LAVA1 = [r[8:] + r[:8] for r in LAVA0]

TILE_ORDER = ["GROUND", "BRICK", "Q0", "Q1", "Q2", "USED", "HARD", "PIPE_TL", "PIPE_TR", "PIPE_L", "PIPE_R",
              "TREE_L", "TREE_M", "TREE_R", "TRUNK", "LAVA0", "LAVA1"]


def _shift(rgb, k):
    return tuple(max(0, min(255, int(c * k))) for c in rgb)


def _check(name, rows):
    w = len(rows[0])
    for i, r in enumerate(rows):
        assert len(r) == w, "%s row %d: %d != %d" % (name, i, len(r), w)


def _grid(rows, pal):
    _check("tile", rows)
    return from_ascii(rows, pal)


# ---------------------------------------------------------------- scenery (procedural shapes)
def _shape(w, h, inside, pal, body="H", dark="h", light="J", outline="k", spots=()):
    """inside(x, y) -> bool. Outline on the silhouette edge, dark rim at the bottom-right,
    light rim top-left, optional spot pixels."""
    g = [[inside(x, y) for x in range(w)] for y in range(h)]

    def at(x, y):
        return 0 <= x < w and 0 <= y < h and g[y][x]

    out = [[None] * w for _ in range(h)]
    for y in range(h):
        for x in range(w):
            if not g[y][x]:
                continue
            if not (at(x + 1, y) and at(x - 1, y) and at(x, y + 1) and at(x, y - 1)):
                out[y][x] = pal[outline]
            elif not (at(x + 2, y) and at(x, y + 2)) and dark:
                out[y][x] = pal[dark]
            elif not (at(x - 2, y) and at(x, y - 2)) and light:
                out[y][x] = pal[light]
            else:
                out[y][x] = pal[body]
    for x, y, key in spots:
        if 0 <= x < w and 0 <= y < h and out[y][x] == pal[body]:
            out[y][x] = pal[key]
    return out


def hill(pal):
    w, h = 80, 38

    def inside(x, y):
        cx = 39.5
        top = 2
        # rounded dome: half-ellipse on a flat base
        dx = (x - cx) / 40.0
        yy = top + (h - top) * (dx * dx) ** 0.9
        return y >= yy and y < h

    spots = []
    for (sx, sy) in ((30, 14), (44, 20), (38, 9), (24, 24), (54, 26)):
        spots += [(sx, sy, "h"), (sx + 1, sy, "h"), (sx, sy + 1, "h"), (sx + 1, sy + 1, "h"), (sx, sy + 2, "h")]
    return _shape(w, h, inside, pal, spots=spots)


def _bumps(w, h, centres, r, pal, body, dark, light):
    def inside(x, y):
        if y >= h - 1:
            return False
        for cx in centres:
            if (x - cx) ** 2 + (y - (h - 1)) ** 2 <= r * r:
                return True
        return False
    return _shape(w, h, inside, pal, body, dark, light)


def bush(pal):
    return _bumps(48, 17, (9, 20, 28, 38), 9, pal, "H", "h", "J")


def cloud(pal):
    w, h = 40, 22

    def inside(x, y):
        for cx, cy, r in ((10, 13, 8), (20, 9, 9), (29, 12, 8), (16, 15, 6), (25, 15, 6)):
            if (x - cx) ** 2 + (y - cy) ** 2 <= r * r:
                return True
        return False
    return _shape(w, h, inside, pal, "C", "c", None, outline="c")


def castle(pal):
    """80x80 stone keep with battlements, windows and a dark gate."""
    w, h = 80, 80
    rows = [[None] * w for _ in range(h)]

    def put(x, y, key):
        if 0 <= x < w and 0 <= y < h:
            rows[y][x] = pal[key]

    def wall(x0, y0, x1, y1):
        for y in range(y0, y1):
            for x in range(x0, x1):
                # stone courses: mortar every 6 rows, staggered joints every 8 columns
                yy = y - y0
                joint = (x - x0 + (4 if (yy // 6) % 2 else 0)) % 8 == 0
                if yy % 6 == 5 or joint:
                    put(x, y, "s")
                else:
                    put(x, y, "S")
            put(x0, y, "k")
            put(x1 - 1, y, "k")
        for x in range(x0, x1):
            put(x, y0, "k")

    def battlements(x0, x1, y):
        for x in range(x0, x1, 8):
            for yy in range(y - 6, y):
                for xx in range(x, min(x + 5, x1)):
                    put(xx, yy, "S")
                put(x, yy, "k")
                put(min(x + 4, x1 - 1), yy, "k")
            for xx in range(x, min(x + 5, x1)):
                put(xx, y - 6, "k")

    wall(0, 36, 80, 80)
    battlements(0, 80, 36)
    wall(20, 8, 60, 36)
    battlements(20, 60, 8)
    # windows
    for wx in (28, 46):
        for y in range(16, 28):
            for x in range(wx, wx + 6):
                put(x, y, "D")
        put(wx, 16, "s")
        put(wx + 5, 16, "s")
    # gate (arched)
    for y in range(54, 80):
        for x in range(30, 50):
            dx = x - 39.5
            if y >= 62 or dx * dx + (y - 62) ** 2 <= 100:
                put(x, y, "D")
    return rows


FLAG = [
    "kkkkkkkkkkkkkkk.",
    ".kFFFFFFFFFFFFk.",
    "..kFFFFFFFFFFk..",
    "...kFFfffFFFk...",
    "....kFfFfFFk....",
    ".....kFfffk.....",
    "......kFFk......",
    ".......kk.......",
]
FLAGBALL = [
    "..kkkk..",
    ".kJffJk.",
    "kJffffhk",
    "kfffffhk",
    "kfffffhk",
    "kffffhhk",
    ".khhhhk.",
    "..kkkk..",
]

_TILES = {"GROUND": GROUND, "BRICK": BRICK, "Q0": QBLOCK, "Q1": QBLOCK, "Q2": QBLOCK, "USED": USED,
          "HARD": HARD, "PIPE_TL": PIPE_TL, "PIPE_TR": PIPE_TR, "PIPE_L": PIPE_L, "PIPE_R": PIPE_R,
          "TREE_L": TREE_L, "TREE_M": TREE_M, "TREE_R": TREE_R, "TRUNK": TRUNK, "LAVA0": LAVA0, "LAVA1": LAVA1}


def build(theme):
    """returns (tiles: {name: grid}, scenery: {name: grid}, sky rgb) for one theme."""
    pal = PALETTES[theme]
    tiles = {}
    for n in TILE_ORDER:
        p = pal
        if n == "Q1":
            p = dict(pal, Y=_shift(pal["Y"], 1.12), W=(255, 255, 255))
        elif n == "Q2":
            p = dict(pal, Y=_shift(pal["Y"], 0.86), y=_shift(pal["y"], 0.86))
        tiles[n] = _grid(_TILES[n], p)
    scen = {
        "HILL": hill(pal),
        "BUSH": bush(pal),
        "CLOUD": cloud(pal),
        "CASTLE": castle(pal),
        "FLAG": _grid(FLAG, pal),
        "FLAGBALL": _grid(FLAGBALL, pal),
    }
    return tiles, scen, SKY[theme]
