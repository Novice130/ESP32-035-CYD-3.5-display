"""
world.py - overworld art: trainer, 16x16 terrain tiles, map objects, route map.
"""
import random
from artlib import Canvas, from_ascii, hflip, shade_of, light_of

# ---------------------------------------------------------------- trainer
TPAL = {
    "k": (40, 32, 44), "r": (226, 56, 56), "R": (160, 30, 44), "w": (250, 250, 250),
    "s": (252, 210, 166), "S": (222, 160, 122), "h": (70, 48, 40), "e": (30, 30, 40),
    "b": (52, 76, 140), "B": (34, 46, 92), "j": (74, 118, 180), "J": (50, 84, 138),
    "o": (70, 56, 56), "y": (246, 196, 60), "Y": (200, 140, 40),
}

_HEAD_DOWN = [
    ".....kkkkkk.....",
    "....krrrrrrk....",
    "...krrwwwwrrk...",
    "...krrrrrrrrk...",
    "..kRRRRRRRRRRk..",
    "..khhhhhhhhhhk..",
    "..khsssssssshk..",
    "..khsessssesshk.",
]
TRAINER_DOWN = [
    ".....kkkkkk.....",
    "....krrrrrrk....",
    "...krrwwwwrrk...",
    "...krrrrrrrrk...",
    "..kRRRRRRRRRRk..",
    "..khhhhhhhhhhk..",
    "..khsssssssshk..",
    "..khsesssseshk..",
    "...kssssssssk...",
    "....kSssssSk....",
    "...kbbbwwbbbk...",
    "..kbbbbwwbbbbk..",
    "..kbBbbbbbbBbk..",
    "..kbBbbbbbbBbk..",
    "..ksbbyyyybbsk..",
    "...kjjjjjjjjk...",
    "...kjjjjjjjjk...",
    "...kjjjkkjjjk...",
    "...kJjjkkjjJk...",
    "...kook..kook...",
    "...kkkk..kkkk...",
]
TRAINER_DOWN_WALK = TRAINER_DOWN[:17] + [
    "...kjjjkkjjjk...",
    "...kookkkjjJk...",
    "...kkkk.kook....",
    "........kkkk....",
]
TRAINER_UP = [
    ".....kkkkkk.....",
    "....krrrrrrk....",
    "...krrrrrrrrk...",
    "...krrrrrrrrk...",
    "..kRrrrrrrrrRk..",
    "..khhhhhhhhhhk..",
    "..khhhhhhhhhhk..",
    "..khhhhhhhhhhk..",
    "...khhhhhhhhk...",
    "....kSssssSk....",
    "...kbbbbbbbbk...",
    "..kbbkyyyykbbk..",
    "..kbkyyyyyykbk..",
    "..kbkyYYYYykbk..",
    "..kskyyyyyyksk..",
    "...kjjjjjjjjk...",
    "...kjjjjjjjjk...",
    "...kjjjkkjjjk...",
    "...kJjjkkjjJk...",
    "...kook..kook...",
    "...kkkk..kkkk...",
]
TRAINER_UP_WALK = TRAINER_UP[:17] + TRAINER_DOWN_WALK[17:]
TRAINER_SIDE = [
    "......kkkkk.....",
    ".....krrrrrk....",
    "....krrrrwwrk...",
    "....krrrrrrrk...",
    "....kRRRRRRRRRk.",
    "....khhhhhhhk...",
    "....khhhsssssk..",
    "....khhsssseskk.",
    ".....khsssssk...",
    "......kSsssk....",
    ".....kbbbbwbk...",
    "....kybbbbbbk...",
    "....kyybbbbbk...",
    "....kyybsbbbk...",
    "....kyybbyybk...",
    ".....kjjjjjk....",
    ".....kjjjjjk....",
    ".....kjjjjjk....",
    ".....kjJjjJk....",
    ".....koooook....",
    ".....kkkkkkk....",
]
TRAINER_SIDE_WALK = TRAINER_SIDE[:16] + [
    "....kjjjkjjjk...",
    "...kjjjk.kjjjk..",
    "...kJjk...kJjk..",
    "...kook...kook..",
    "...kkk.....kkk..",
]
TRAINER_SIDE_WALK2 = TRAINER_SIDE[:16] + [
    ".....kjjjjjk....",
    "....kjjjkjjk....",
    "....kJjk.kJk....",
    "....kook.kook...",
    "....kkk...kkk...",
]


def trainer_frames():
    """Returns frames in order: down[stand, walkA, walkB], up[...], left[...], right[...]"""
    d0 = from_ascii(TRAINER_DOWN, TPAL)
    d1 = from_ascii(TRAINER_DOWN_WALK, TPAL)
    u0 = from_ascii(TRAINER_UP, TPAL)
    u1 = from_ascii(TRAINER_UP_WALK, TPAL)
    r0 = from_ascii(TRAINER_SIDE, TPAL)
    r1 = from_ascii(TRAINER_SIDE_WALK, TPAL)
    r2 = from_ascii(TRAINER_SIDE_WALK2, TPAL)
    return [d0, d1, hflip(d1), u0, u1, hflip(u1), hflip(r0), hflip(r1), hflip(r2), r0, r1, r2]


# ---------------------------------------------------------------- tiles
G_BASE = (124, 204, 96)
G_DARK = (90, 170, 76)
G_LITE = (168, 226, 124)
TG_BASE = (66, 160, 74)
TG_DARK = (36, 112, 58)
TG_LITE = (120, 210, 100)
TG_EDGE = (22, 72, 44)


def blank(c):
    return [[c] * 16 for _ in range(16)]


def grass_tile(seed):
    rnd = random.Random(seed)
    t = blank(G_BASE)
    for _ in range(9):
        x, y = rnd.randrange(1, 15), rnd.randrange(2, 15)
        t[y][x] = G_DARK
        t[y - 1][x - 1] = G_DARK
        t[y - 1][x + 1] = G_DARK
    for _ in range(6):
        x, y = rnd.randrange(16), rnd.randrange(16)
        t[y][x] = G_LITE
    return t


def tall_grass_tile(phase):
    """dense blades of varying height, light tips, dark roots - two staggered bands."""
    t = blank(TG_DARK)
    heights = [7, 5, 8, 6, 4, 7, 5, 8]
    for band, oy in ((0, 0), (1, 8)):
        off = 3 if band else 0
        for x in range(16):
            k = (x + off) % 16
            if k % 2:
                continue
            h = heights[(k // 2 + band * 3) % 8]
            for i in range(h):
                y = oy + 7 - i
                xx = x
                if i >= h - 2 and phase:  # tips sway one pixel
                    xx = (x + (1 if (k // 2) % 2 else -1)) % 16
                col = TG_LITE if i == h - 1 else (TG_BASE if i > 1 else shade_of(TG_BASE))
                t[y][xx] = col
                if i == h - 2 and 0 <= xx + 1 < 16 and t[y][xx + 1] == TG_DARK:
                    t[y][xx + 1] = shade_of(TG_BASE)
        for x in range(16):
            if t[oy + 7][x] == TG_DARK:
                t[oy + 7][x] = TG_EDGE
    return t


P_BASE = (232, 214, 170)
P_GROUT = (196, 172, 128)
P_LITE = (248, 236, 204)


def path_tile():
    t = blank(P_BASE)
    for y in range(16):
        for x in range(16):
            off = 4 if (y // 8) % 2 else 0
            if y % 8 == 7 or (x + off) % 8 == 7:
                t[y][x] = P_GROUT
            elif y % 8 == 0 or (x + off) % 8 == 0:
                t[y][x] = P_LITE
    return t


def flower_tile(color, phase, seed):
    t = grass_tile(seed)
    center = (250, 240, 120) if color != (250, 230, 80) else (240, 130, 40)
    for (fx, fy) in ((3, 3), (11, 5), (6, 11), (13, 13)):
        fx += phase
        for dx, dy in ((0, -1), (-1, 0), (1, 0), (0, 1)):
            t[(fy + dy) % 16][(fx + dx) % 16] = color
        t[fy][fx % 16] = center
        t[(fy + 2) % 16][fx % 16] = G_DARK
    return t


W_BASE = (72, 140, 228)
W_LITE = (150, 204, 250)
W_DEEP = (52, 108, 200)


def water_tile(phase):
    t = blank(W_BASE)
    for y in range(16):
        for x in range(16):
            if (y // 4) % 2:
                if (x + phase * 2 + y) % 8 in (0, 1):
                    t[y][x] = W_DEEP
            if y % 8 == 2 and (x + phase * 2) % 16 in (2, 3, 4, 5):
                t[y][x] = W_LITE
            if y % 8 == 6 and (x - phase * 2 + 8) % 16 in (9, 10, 11):
                t[y][x] = W_LITE
    return t


def hedge_tile():
    c = Canvas(16, 16)
    c.part("bush", (64, 146, 72))
    for cx in (3, 8, 13):
        c.circle("bush", cx, 7, 5)
    c.rect("bush", 0, 7, 16, 16)
    pix = c.render()
    # tile seamlessly horizontally: force side columns to body colour
    for y in range(7, 16):
        for x in (0, 15):
            pix[y][x] = (64, 146, 72) if y < 15 else (30, 80, 42)
    for x in range(16):
        pix[15][x] = (30, 80, 42)
    under = grass_tile(77)
    return [[pix[y][x] if pix[y][x] is not None else under[y][x] for x in range(16)] for y in range(16)]


def fence_tile():
    t = grass_tile(31)
    W = (250, 248, 240)
    S = (190, 180, 170)
    K = (110, 96, 90)
    for y in (5, 6, 10, 11):
        for x in range(16):
            t[y][x] = W if y in (5, 10) else S
    for px in (2, 10):
        for y in range(2, 15):
            t[y][px] = K
            t[y][px + 1] = W
            t[y][px + 2] = W
            t[y][px + 3] = S
            t[y][px + 4] = K
        for x in range(px + 1, px + 4):
            t[1][x] = K
        t[15][px + 1] = t[15][px + 2] = t[15][px + 3] = G_DARK
    return t


TILES = {}  # name -> pix


def build_tiles():
    TILES.clear()
    TILES["grass"] = grass_tile(1)
    TILES["grass2"] = grass_tile(2)
    TILES["tall0"] = tall_grass_tile(0)
    TILES["tall1"] = tall_grass_tile(1)
    TILES["path"] = path_tile()
    TILES["flowerR0"] = flower_tile((236, 70, 92), 0, 5)
    TILES["flowerR1"] = flower_tile((236, 70, 92), 1, 5)
    TILES["flowerY0"] = flower_tile((250, 230, 80), 0, 6)
    TILES["flowerY1"] = flower_tile((250, 230, 80), 1, 6)
    TILES["flowerW0"] = flower_tile((250, 250, 255), 0, 7)
    TILES["flowerW1"] = flower_tile((250, 250, 255), 1, 7)
    for i in range(4):
        TILES["water%d" % i] = water_tile(i)
    TILES["hedge"] = hedge_tile()
    TILES["fence"] = fence_tile()
    return TILES


# tile index order used by the C side (keep in sync with game.h)
TILE_ORDER = ["grass", "grass2", "tall0", "tall1", "path", "flowerR0", "flowerR1", "flowerY0", "flowerY1",
              "flowerW0", "flowerW1", "water0", "water1", "water2", "water3", "hedge", "fence"]


# ---------------------------------------------------------------- objects
def tree():
    c = Canvas(32, 40)
    c.part("shadow", (60, 120, 60), line=False, shade=False)
    c.part("trunk", (130, 86, 54))
    c.part("leaf", (54, 150, 70))
    c.part("leaf2", (74, 176, 80))
    c.part("leaf3", (110, 200, 96))
    c.ellipse("shadow", 16, 37, 12, 3)
    c.rect("trunk", 12, 24, 20, 38)
    c.ellipse("leaf", 16, 18, 15, 13)
    c.ellipse("leaf", 8, 22, 7, 6)
    c.ellipse("leaf", 24, 22, 7, 6)
    c.ellipse("leaf2", 14, 13, 10, 8)
    c.ellipse("leaf3", 12, 9, 5, 4)
    pix = c.render()
    # soft drop shadow: semi-dark pixels (rendered as a darker grass tone)
    for y in range(34, 40):
        for x in range(32):
            if pix[y][x] == (60, 120, 60) or pix[y][x] == shade_of((60, 120, 60)):
                pix[y][x] = (84, 156, 70)
    return pix


def lamp():
    rows = [
        "......kkkk......",
        ".....kyyyyk.....",
        "....kyYwwYyk....",
        "....kyYwwYyk....",
        "....kyYyyYyk....",
        ".....kkkkkk.....",
        "......kddk......",
        ".......kk.......",
        ".......dk.......",
        ".......dk.......",
        ".......dk.......",
        ".......dk.......",
        ".......dk.......",
        ".......dk.......",
        ".......dk.......",
        ".......dk.......",
        ".......dk.......",
        ".......dk.......",
        ".......dk.......",
        "......kddk......",
        ".....kdddkk.....",
        "....kkddddkk....",
        "...ggkkkkkkgg...",
    ]
    pal = {"k": (30, 30, 40), "d": (70, 70, 86), "y": (250, 220, 120), "Y": (255, 244, 190),
           "w": (255, 255, 240), "g": (84, 156, 70)}
    return from_ascii(rows, pal)


def sign():
    rows = [
        "..kkkkkkkkkkkk..",
        ".kbbbbbbbbbbbbk.",
        ".kbBBBBBBBBBBbk.",
        ".kbBwwwwwwwwBbk.",
        ".kbBBBBBBBBBBbk.",
        ".kbBwwwwwwBBBbk.",
        ".kbBBBBBBBBBBbk.",
        ".kddddddddddddk.",
        "..kkkkkddkkkkk..",
        "......kddk......",
        "......kddk......",
        "....ggkddkgg....",
    ]
    pal = {"k": (60, 40, 30), "b": (190, 130, 80), "B": (220, 168, 110), "w": (130, 86, 50),
           "d": (150, 100, 60), "g": (84, 156, 70)}
    return from_ascii(rows, pal)


def pokecenter():
    W, H = 64, 56
    RED = (226, 64, 64)
    RD = (170, 40, 52)
    RL = (248, 120, 110)
    WALL = (246, 242, 236)
    WS = (206, 200, 196)
    K = (50, 40, 50)
    GL = (150, 210, 250)
    GLD = (90, 150, 220)
    p = [[None] * W for _ in range(H)]

    def fill(x0, y0, x1, y1, c):
        for y in range(max(0, y0), min(H, y1)):
            for x in range(max(0, x0), min(W, x1)):
                p[y][x] = c

    # roof (sloped look with ridges)
    fill(2, 4, 62, 24, RED)
    for y in range(4, 24, 4):
        fill(2, y, 62, y + 1, RD)
    fill(2, 4, 62, 6, RL)
    fill(0, 22, 64, 26, RD)
    fill(0, 2, 64, 4, K)
    fill(0, 2, 2, 26, K)
    fill(62, 2, 64, 26, K)
    # walls
    fill(3, 26, 61, 54, WALL)
    fill(3, 26, 61, 28, WS)
    fill(3, 52, 61, 54, WS)
    fill(2, 26, 3, 54, K)
    fill(61, 26, 62, 54, K)
    fill(2, 54, 62, 56, K)
    # windows
    for wx in (7, 47):
        fill(wx, 32, wx + 10, 42, K)
        fill(wx + 1, 33, wx + 9, 41, GL)
        fill(wx + 1, 37, wx + 9, 41, GLD)
        fill(wx + 2, 34, wx + 4, 36, (230, 248, 255))
    # sliding glass doors
    fill(24, 36, 40, 54, K)
    fill(25, 37, 32, 54, GL)
    fill(32, 37, 39, 54, GL)
    fill(25, 45, 39, 54, GLD)
    fill(32, 37, 33, 54, K)
    # red banner + Poke Ball emblem on roof
    fill(22, 27, 42, 32, RED)
    fill(22, 31, 42, 32, RD)
    cx, cy, r = 32, 13, 7
    for y in range(cy - r - 1, cy + r + 2):
        for x in range(cx - r - 1, cx + r + 2):
            d2 = (x - cx) ** 2 + (y - cy) ** 2
            if d2 <= (r + 1) ** 2:
                p[y][x] = K
            if d2 <= r * r:
                p[y][x] = WALL if y > cy else (250, 250, 250)
                if y < cy:
                    p[y][x] = (255, 255, 255)
                if abs(y - cy) <= 0 and True:
                    p[y][x] = K
    for y in range(cy - r, cy):
        for x in range(cx - r, cx + r + 1):
            if (x - cx) ** 2 + (y - cy) ** 2 <= r * r:
                p[y][x] = (240, 90, 90)
    for y in range(cy - 2, cy + 3):
        for x in range(cx - 2, cx + 3):
            d2 = (x - cx) ** 2 + (y - cy) ** 2
            if d2 <= 5:
                p[y][x] = K
            if d2 <= 1:
                p[y][x] = (255, 255, 255)
    # roof sign letters 'PC' (simple blocks)
    return p


def pokeball(open_=False):
    rows = [
        "...kkkkkk...",
        "..krrrrrrk..",
        ".krrwrrrrrk.",
        "krrwrrrrrrrk",
        "krrrrrrrrrrk",
        "kkkkkwwkkkkk",
        "kkkkwkkwkkkk",
        "kwwwkwwkwwwk",
        "kwwwwkkwwwwk",
        ".kwwwwwwwwk.",
        "..kSwwwwSk..",
        "...kkkkkk...",
    ]
    pal = {"k": (40, 34, 44), "r": (236, 60, 60), "w": (250, 250, 250), "S": (200, 200, 210)}
    return from_ascii(rows, pal)


def star():
    rows = [
        "...y...",
        "...y...",
        "..yYy..",
        "yyYwYyy",
        "..yYy..",
        "...y...",
        "...y...",
    ]
    return from_ascii(rows, {"y": (255, 220, 80), "Y": (255, 246, 170), "w": (255, 255, 255)})


def tall_grass_overlay():
    """lower half of tall grass that covers the trainer's legs."""
    t = TILES["tall0"]
    return [[t[y][x] if y >= 8 else None for x in range(16)] for y in range(16)]


# ---------------------------------------------------------------- map
MAP_W, MAP_H = 30, 22
TILE_CH = {"grass": ".", "grass2": ",", "tall": "T", "path": "=", "flowerR": "r", "flowerY": "y",
           "flowerW": "o", "water": "w", "hedge": "h", "fence": "F"}


def build_map():
    rnd = random.Random(42)
    g = [["." if rnd.random() < 0.7 else "," for _ in range(MAP_W)] for _ in range(MAP_H)]

    def rect(ch, x0, y0, x1, y1):
        for y in range(y0, y1 + 1):
            for x in range(x0, x1 + 1):
                g[y][x] = ch

    # hedges framing the route
    rect("h", 0, 20, 29, 21)
    # route loop path
    rect("=", 3, 8, 26, 8)
    rect("=", 5, 8, 5, 18)
    rect("=", 24, 8, 24, 18)
    rect("=", 5, 18, 24, 18)
    rect("=", 14, 5, 15, 7)        # path up to the Pokemon Center door
    # tall grass patches (the loop runs through them!)
    rect("T", 2, 11, 8, 15)
    rect("T", 13, 16, 20, 19)
    rect("T", 21, 10, 28, 13)
    # pond in the middle of the loop
    rect("w", 10, 10, 19, 14)
    rect("w", 11, 9, 18, 9)
    rect("w", 11, 15, 18, 15)
    # flower beds
    rect("r", 17, 6, 19, 7)
    rect("y", 20, 6, 22, 7)
    rect("o", 7, 6, 9, 7)
    rect("r", 10, 6, 11, 7)
    rect("y", 7, 16, 11, 16)
    rect("o", 26, 15, 28, 17)
    # white fence along the top of the route
    rect("F", 0, 4, 9, 4)
    rect("F", 20, 4, 29, 4)
    return ["".join(r) for r in g]


# objects: (kind, pixel x, pixel y) - y is the bottom ("feet") line used for depth sorting
OBJECTS = [
    ("center", 12 * 16, 6 * 16 - 2),
    ("lamp", 12 * 16, 8 * 16 - 2), ("lamp", 17 * 16, 8 * 16 - 2),
    ("lamp", 4 * 16, 19 * 16 - 1), ("lamp", 25 * 16, 19 * 16 - 1),
    ("sign", 6 * 16, 8 * 16 - 1),
]
for tx, ty in ((0, 1), (2, 1), (4, 1), (6, 1), (8, 0), (21, 1), (23, 0), (25, 1), (27, 0), (28, 2),
               (0, 6), (0, 9), (0, 15), (1, 18), (27, 6), (28, 15), (27, 18), (9, 19), (19, 19),
               (17, 3)):
    OBJECTS.append(("tree", tx * 16, ty * 16 + 38))

# trainer loop around the route (tile coords); trainer starts at first waypoint
ROUTE = [(14, 8), (24, 8), (24, 12), (26, 12), (26, 11), (24, 11), (24, 18), (5, 18), (5, 13),
         (3, 13), (3, 12), (5, 12), (5, 8)]
