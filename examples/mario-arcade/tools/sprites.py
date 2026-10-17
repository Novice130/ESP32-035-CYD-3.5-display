"""
sprites.py - hand-made fan-tribute pixel art for the plumber, enemies, items and UI.

Every sprite is ASCII art + a palette dict (artlib.from_ascii). '.' = transparent.
The plumber is drawn once per pose with symbolic colours; the Fire form is the same
drawing run through a different palette.
"""
from artlib import from_ascii, hflip

K = (44, 24, 28)        # dark outline used across characters

MARIO = {
    "k": K,
    "R": (232, 40, 40), "r": (164, 18, 34),        # cap / shirt
    "M": (255, 255, 255),                           # cap emblem
    "S": (255, 202, 150), "s": (222, 146, 98),      # skin
    "H": (96, 50, 24),                              # hair / moustache
    "E": (24, 24, 56),                              # eye
    "O": (52, 100, 236), "o": (28, 58, 164),        # overalls
    "Y": (255, 224, 72),                            # buttons
    "W": (255, 255, 255), "w": (200, 204, 222),     # gloves
    "B": (136, 74, 30), "b": (86, 42, 18),          # shoes
}
FIRE = dict(MARIO, R=(246, 246, 246), r=(196, 196, 212), M=(232, 40, 40),
            O=(226, 44, 40), o=(150, 22, 34))

# ---------------------------------------------------------------- small (16x16)
_SM_HEAD = [
    "....kkkkkk......",
    "...kRRRRRRk.....",
    "..kRRRMMRRRk....",
    "..kRRRMMRRRRkkk.",
    "..kHHSSSSESkRRk.",
    ".kHSHSSSSESSkk..",
    ".kHSHHSSSSSSSSk.",
    ".kHHSSSSSHHHHk..",
    "..kkSSSSSSSSk...",
]
SMALL = {
    "stand": _SM_HEAD + [
        "..kkrRRORRRk....",
        ".kRRRROOOORRk...",
        "kWWRROYOOYORWWk.",
        "kWWOOOOOOOOOWWk.",
        ".kkOOOOkkOOOOk..",
        "..kBBBk..kBBBk..",
        ".kBBBBk..kBBBBk.",
    ],
    "walk1": _SM_HEAD + [
        "..kkrRRORRRkWWk.",
        ".kRRRROOOOOOWWk.",
        "kWWRROYOOYOOkk..",
        "kWWkOOOOOOOOk...",
        ".kkOOOOkkOOOOk..",
        "kBBBBk...kBBBBk.",
        "kkkkk.....kkkkk.",
    ],
    "walk2": _SM_HEAD + [
        "...krRRORRRk....",
        "..kRRROOOORk....",
        "..kWWOYOOYOk....",
        "..kWWOOOOOOk....",
        "...kOOOOOOk.....",
        "...kBBBkBBk.....",
        "..kBBBBkkkk.....",
    ],
    "walk3": _SM_HEAD + [
        "..kkrRRORRRk....",
        ".kRRROOOOORRk...",
        "kWWOOYOOYOOWWk..",
        "kWWOOOOOOOOOWk..",
        ".kkOOOOkOOOOk...",
        "..kBBBk.kOOBBk..",
        "..kkkk..kkBBBk..",
    ],
    "jump": [
        "....kkkkkk...kk.",
        "...kRRRRRRk.kWWk",
        "..kRRRMMRRRkkWWk",
        "..kRRRMMRRRRkRk.",
        "..kHHSSSSESkRRk.",
        ".kHSHSSSSESSkk..",
        ".kHSHHSSSSSSSSk.",
        ".kHHSSSSSHHHHk..",
        "..kkSSSSSSSSk...",
        ".kRRrRRORRRRk...",
        "kWWRRROOOOOk....",
        "kWWkROYOOYOOk...",
        ".kkOOOOOOOOOOBk.",
        "..kOOOOkkOOOBBk.",
        ".kBBBOk..kkBBk..",
        ".kBBBk.....kk...",
    ],
    "dead": [
        "....kkkkkkkk....",
        "...kRRRRRRRRk...",
        "..kRRRRMMRRRRk..",
        ".kkkHSSSSSSHkkk.",
        "kWWkSEkSSkESkWWk",
        "kWWkSSSSSSSSkWWk",
        ".kRkSSHHHHSSkRk.",
        ".kRkSHSSSSHSkRk.",
        "..kRkSSSSSSkRk..",
        "..kRROOOOOORRk..",
        "...kOYOOOOYOk...",
        "...kOOOOOOOOk...",
        "..kOOOOkkOOOOk..",
        "..kBBBk..kBBBk..",
        ".kBBBBk..kBBBBk.",
        ".kkkkk....kkkkk.",
    ],
}

# ---------------------------------------------------------------- big (16x32)
_BG_HEAD = [
    ".....kkkkkk.....",
    "...kkRRRRRRkk...",
    "..kRRRMMMMRRRk..",
    "..kRRMMrrMMRRRk.",
    "..kRRRMMMMRRRRkk",
    "..kRRRRRRRRRRRRk",
    "..kHHHSSSSkkkkk.",
    ".kHHSSSSSSESk...",
    ".kHSHSSSSSESSk..",
    ".kHSHHSSSSSSSSk.",
    ".kHHSSSSSSSSSSk.",
    "..kHSSSSSHHHHHk.",
    "..kkSSSSSSHHHk..",
    "...kkSSSSSSSk...",
]
_BG_BODY = [
    "...kkrRRRRRk....",
    "..kRRrRORRROk...",
    ".kRRRrROORRORk..",
    ".kRRRRROOOOORk..",
    "kRRRRROYOOOYORk.",
    "kRRRRROOOOOOORk.",
    "kWWkROOOOOOOOkWk",
    "kWWWkOOOOOOOkWWk",
    ".kWkOOOOOOOOOkk.",
    "..kOOOOOOOOOOOk.",
]
BIG = {
    "stand": _BG_HEAD + _BG_BODY + [
        "..kOOOOkkOOOOOk.",
        "..kOOOk..kOOOk..",
        "..kOOOk..kOOOk..",
        "..kOOk....kOOk..",
        ".kBBBBk..kBBBBk.",
        "kBBBBBk..kBBBBBk",
        "kbbbbbk..kbbbbbk",
        ".kkkkk....kkkkk.",
    ],
    "walk1": _BG_HEAD + [
        "...kkrRRRRRk....",
        "..kRRrRORRROkkk.",
        ".kRRRrROORRORWWk",
        ".kRRRRROOOOOkWWk",
        ".kRRRROYOOOYkkk.",
        ".kRRRROOOOOOOk..",
        "kWWkROOOOOOOOk..",
        "kWWWOOOOOOOOk...",
        ".kkOOOOOOOOOOk..",
        "..kOOOOOOOOOOOk.",
        ".kOOOOOkkkOOOOk.",
        "kOOOOk....kOOOOk",
        "kOOOk......kOOk.",
        "kBBk.......kBBBk",
        "kBBBk.....kBBBBk",
        "kbbbBk....kbbbbk",
        ".kkkkk....kkkkk.",
        "................",
    ],
    "walk2": _BG_HEAD + [
        "...kkrRRRRRk....",
        "..kRRrRORRROk...",
        "..kRRrROORROk...",
        "..kRRRROOOOOk...",
        "..kRRROYOOYOk...",
        "..kWWkOOOOOOk...",
        "..kWWWOOOOOOk...",
        "...kkOOOOOOOk...",
        "...kOOOOOOOOk...",
        "...kOOOOOOOk....",
        "...kOOOOkOOk....",
        "...kOOOk.kOOk...",
        "...kOOOk.kOOk...",
        "...kOOk..kBBk...",
        "..kBBBBk.kBBBk..",
        "..kBBBBk.kbbbk..",
        "..kbbbbk..kkk...",
        "...kkkk.........",
    ],
    "walk3": _BG_HEAD + [
        "...kkrRRRRRk....",
        "..kRRrRORRROk...",
        ".kRRRrROORRORk..",
        ".kRRRROOOOOORk..",
        ".kRRRROYOOYORk..",
        ".kWWkOOOOOOOkWk.",
        ".kWWWOOOOOOkWWk.",
        "..kkOOOOOOOOkk..",
        "..kOOOOOOOOOOk..",
        "..kOOOOkkOOOOk..",
        "..kOOOk..kOOOk..",
        ".kOOOk....kOOk..",
        ".kOOk.....kOOk..",
        ".kBBBk...kBBBk..",
        "kBBBBk...kBBBBk.",
        "kbbbbk...kbbbbk.",
        ".kkkk.....kkkk..",
        "................",
    ],
    "jump": [
        ".....kkkkkk..kk.",
        "...kkRRRRRRkkWWk",
        "..kRRRMMMMRRkWWk",
        "..kRRMMrrMMRkWWk",
        "..kRRRMMMMRRRkRk",
        "..kRRRRRRRRRRRRk",
        "..kHHHSSSSkkkkk.",
        ".kHHSSSSSSESk...",
        ".kHSHSSSSSESSk..",
        ".kHSHHSSSSSSSSk.",
        ".kHHSSSSSSSSSSk.",
        "..kHSSSSSHHHHHk.",
        "..kkSSSSSSHHHk..",
        "...kkSSSSSSSk...",
        "...kkrRRRRRRk...",
        "..kRRrRORRRORk..",
        ".kRRRrROORRORk..",
        ".kRRRRROOOOORk..",
        "kWWkRROYOOOYOk..",
        "kWWWkROOOOOOOk..",
        ".kWkOOOOOOOOOk..",
        "..kOOOOOOOOOOOk.",
        "..kOOOOOOOOOOOOk",
        "..kOOOOkkkOOOBBk",
        ".kOOOOk...kOBBBk",
        ".kOOOk.....kBBk.",
        "kBBBk.......kk..",
        "kBBBBk..........",
        "kbbbbk..........",
        ".kkkk...........",
        "................",
        "................",
    ],
}

# ---------------------------------------------------------------- enemies
GOOMBA_PAL = {"k": (52, 22, 14), "G": (184, 96, 40), "g": (128, 60, 24), "T": (250, 214, 160),
              "t": (214, 162, 108), "W": (255, 255, 255), "E": (20, 16, 24), "B": (40, 24, 20)}
GOOMBA = [
    "......kkkk......",
    "....kkGGGGkk....",
    "...kGGGGGGGGk...",
    "..kGGGGGGGGGGk..",
    "..kGkkGGGGkkGk..",
    ".kGGGkkGGkkGGGk.",
    ".kGGWWEkkEWWGGk.",
    "kGGGWWEGGEWWGGGk",
    "kGGGWWEGGEWWGGGk",
    "kgGGGGGGGGGGGGgk",
    ".kggGTTTTTTGggk.",
    "..kkTTTTTTTTkk..",
    "...kTTTTTTTtk...",
    "..kBBkttttkBBk..",
    ".kBBBBBkkBBBBBk.",
    ".kkkkkk..kkkkk..",
]
GOOMBA_FLAT = [
    ".....kkkkkk.....",
    "..kkkGGGGGGkkk..",
    ".kGGkkGGGGkkGGk.",
    "kGGGWEkGGkEWGGGk",
    "kgGGGGGGGGGGGGgk",
    ".kkTTTTTTTTTTkk.",
    "kBBBBBkkkkBBBBBk",
    ".kkkkk....kkkkk.",
]

KOOPA_PAL = {"k": (24, 44, 20), "G": (60, 184, 56), "g": (28, 120, 36), "L": (150, 232, 120),
             "Y": (255, 214, 80), "y": (214, 160, 40), "W": (255, 255, 255), "E": (20, 20, 30),
             "C": (250, 246, 228), "c": (210, 196, 160)}
# faces LEFT (walks toward the player)
KOOPA = [
    "....kkk.........",
    "...kYYYk........",
    "..kYYYYYk.......",
    "..kYWWEYk.......",
    "..kYWWEYYk......",
    ".kYYYYYYYk......",
    ".kYYkYYYYk......",
    "..kkYYYYk.......",
    "...kYYYk.kkkk...",
    "...kYYk.kGLGGk..",
    "..kYYkkGLGGGGGk.",
    "..kYYkGLGGkGGGGk",
    "..kYkGGGGkGkGGgk",
    "...kcGGGkGGGkGgk",
    "..kccGGGkGGGkGgk",
    "..kcckGGGkGkGGgk",
    "..kccCkGGGkGGggk",
    "..kcCCCkggggggk.",
    "...kccCCkkkkkkk.",
    "..kYYkcccccck...",
    ".kYYYYkkkkkk....",
    ".kYYYYk..kYYk...",
    "..kkkk..kYYYYk..",
    "........kkkkk...",
]
SHELL = [
    ".....kkkkkk.....",
    "...kkGLLGGGkk...",
    "..kGLGGkkGGGGk..",
    ".kGLGGkGGkGGGgk.",
    ".kLGGkGGGGkGGgk.",
    "kGGGkGGGGGGkGggk",
    "kGGGkGGGGGGkGggk",
    "kGGGGkGGGGkGGggk",
    "kgGGGGkkkkGGGggk",
    "kkggggggggggggkk",
    "kCCkkkkkkkkkkCCk",
    "kcCCCCCCCCCCCCck",
    ".kccCCCCCCCCcck.",
    "..kkccccccccck..",
    "....kkkkkkkk....",
    "................",
]

PIRANHA_PAL = {"k": (40, 20, 24), "R": (226, 40, 44), "r": (160, 20, 32), "W": (255, 255, 255),
               "w": (220, 214, 210), "G": (56, 176, 64), "g": (24, 112, 40), "L": (140, 230, 120)}
PIRANHA_OPEN = [
    "...kkk....kkk...",
    "..kRRRk..kRRRk..",
    ".kRWRRRkkRRRWRk.",
    ".kRRRRWkkWRRRRk.",
    "kRRWRRkwwkRRWRRk",
    "kRRRRkw..wkRRRRk",
    "kRRRkw....wkRRRk",
    "kRWRkw....wkRWRk",
    "kRRRRkw..wkRRRRk",
    ".kRRRRkwwkRRRRk.",
    ".kRrRRRkkRRRrRk.",
    "..krrRRRRRRrrk..",
    "...kkrrrrrrkk...",
    ".....kkGGkk.....",
    "..kkk.kGGk.kkk..",
    ".kGLGk.kGkkGLGk.",
    "kGLGGGkkGkGGGLGk",
    "kGGGggkGGkggGGGk",
    ".kGgg.kGk.kggGk.",
    "..kk..kGGk..kk..",
    "......kGGk......",
    "......kGGk......",
    "......kGgk......",
    "......kGgk......",
]
PIRANHA_SHUT = [
    "................",
    "................",
    "....kkkkkkkk....",
    "..kkRRRRRRRRkk..",
    ".kRRWRRRRRRWRRk.",
    "kRRRRRRWRRRRRRRk",
    "kRWRRRRRRRRRWRRk",
    "kRRRRkwwwwwkRRRk",
    "kRRRRRkkkkkRRRRk",
    ".kRRWRRRRRRWRRk.",
    ".kRrRRRRRRRRrRk.",
    "..krrRRRRRRrrk..",
    "...kkrrrrrrkk...",
    ".....kkGGkk.....",
    "..kkk.kGGk.kkk..",
    ".kGLGk.kGkkGLGk.",
    "kGLGGGkkGkGGGLGk",
    "kGGGggkGGkggGGGk",
    ".kGgg.kGk.kggGk.",
    "..kk..kGGk..kk..",
    "......kGGk......",
    "......kGGk......",
    "......kGgk......",
    "......kGgk......",
]

# ---------------------------------------------------------------- items
MUSH_PAL = {"k": (50, 20, 20), "R": (232, 44, 40), "r": (170, 24, 30), "W": (255, 255, 255),
            "C": (255, 230, 190), "c": (222, 180, 130), "E": (20, 20, 40)}
ONEUP_PAL = dict(MUSH_PAL, R=(56, 190, 64), r=(28, 124, 40))
MUSHROOM = [
    ".....kkkkkk.....",
    "...kkRRWWRRkk...",
    "..kRRRWWWWRRRk..",
    ".kRWWRWWWWRRRRk.",
    ".kWWWRRWWRRRWWk.",
    "kRWWWRRRRRRWWWRk",
    "kRRWRRRRRRRWWWRk",
    "kRRRRRRWWRRRWRRk",
    "krRRRRWWWWRRRRrk",
    ".kkrrrrrrrrrrkk.",
    "..kCCkCCCCkCCk..",
    "..kCCkCCCCkCCk..",
    "..kCCkCCCCkCck..",
    "..kcCCCCCCCCck..",
    "...kkccccccck...",
    ".....kkkkkk.....",
]
FLOWER_PAL = {"k": (48, 20, 20), "O": (255, 140, 30), "R": (232, 44, 40), "Y": (255, 236, 90),
              "W": (255, 255, 255), "G": (56, 180, 64), "g": (24, 112, 40), "E": (20, 20, 40)}
FLOWER = [
    "....kkkkkkkk....",
    "..kkRRRRRRRRkk..",
    ".kRROOOOOOOORRk.",
    "kRROYYYYYYYYORRk",
    "kROYWWEWWEWWYORk",
    "kROYWWEWWEWWYORk",
    "kRROYYYYYYYYORRk",
    ".kRROOOOOOOORRk.",
    "..kkRRRRRRRRkk..",
    "....kkkGGkkk....",
    ".kkk..kGGk..kkk.",
    "kGGGk.kGGk.kGGGk",
    "kGgGGkkGGkkGGgGk",
    ".kggGGkGGkGGggk.",
    "..kkggkGGkggkk..",
    "....kkkkkkkk....",
]
STAR_PAL = {"k": (90, 50, 10), "Y": (255, 220, 50), "y": (230, 160, 20), "W": (255, 255, 230),
            "E": (30, 20, 20)}
STAR = [
    ".......kk.......",
    "......kYYk......",
    "......kYYk......",
    ".....kYWYYk.....",
    "kkkkkkYWYYkkkkkk",
    "kYYYYYYYYYYYYYyk",
    ".kYYYYEYYEYYYyk.",
    "..kYYYEYYEYYyk..",
    "...kYYYYYYYyk...",
    "...kYYYYYYYyk...",
    "..kYYYYyyYYYyk..",
    "..kYYYykkyYYyk..",
    ".kYYyykk.kkyyyk.",
    ".kYykk.....kkyk.",
    "kyykk.......kkk.",
    "kkk.............",
]
COIN_PAL = {"k": (100, 56, 10), "Y": (255, 214, 40), "y": (222, 150, 20), "W": (255, 250, 210)}
COIN = [
    [
        "..kkkk..",
        ".kYYYYk.",
        "kYWYYYyk",
        "kYWYyYyk",
        "kYWYyYyk",
        "kYWYyYyk",
        "kYWYyYyk",
        "kYWYyYyk",
        "kYWYyYyk",
        "kYWYyYyk",
        "kYWYyYyk",
        "kYYYYYyk",
        ".kyyyyk.",
        "..kkkk..",
    ],
    [
        "...kk...",
        "..kYYk..",
        "..kWYk..",
        "..kWyk..",
        "..kWyk..",
        "..kWyk..",
        "..kWyk..",
        "..kWyk..",
        "..kWyk..",
        "..kWyk..",
        "..kWyk..",
        "..kYyk..",
        "..kyyk..",
        "...kk...",
    ],
    [
        "...kk...",
        "...kk...",
        "...Wk...",
        "...Wk...",
        "...Wk...",
        "...Wk...",
        "...Wk...",
        "...Wk...",
        "...Wk...",
        "...Wk...",
        "...Wk...",
        "...Wk...",
        "...kk...",
        "...kk...",
    ],
]
FIREBALL_PAL = {"k": (120, 20, 10), "R": (240, 60, 20), "O": (255, 160, 40), "Y": (255, 240, 140)}
FIREBALL = [
    "..kkkk..",
    ".kROORk.",
    "kROYYORk",
    "kOYYYYOk",
    "kOYYYOOk",
    "kROOOORk",
    ".kRRRRk.",
    "..kkkk..",
]
DEBRIS = [
    ".kkkk.",
    "kAAAak",
    "kAaaak",
    "kaaaak",
    ".kkkk.",
]


def _check(name, rows, w=None):
    ww = len(rows[0])
    for i, r in enumerate(rows):
        assert len(r) == ww, "%s row %d is %d wide, expected %d" % (name, i, len(r), ww)
    if w:
        assert ww == w, name


def _mk(name, rows, pal):
    _check(name, rows)
    return from_ascii(rows, pal)


def rotate90(pix):
    h, w = len(pix), len(pix[0])
    return [[pix[h - 1 - y][x] for y in range(h)] for x in range(w)]


def build():
    """returns dict name -> pixel grid (rows of rgb|None)."""
    out = {}
    for form, pal in (("S", MARIO), ("F", FIRE)):
        if form == "S":
            for k, rows in SMALL.items():
                out["MS_" + k.upper()] = _mk("small " + k, rows, pal)
        for k, rows in BIG.items():
            out["M%s_%s" % ("B" if form == "S" else "F", k.upper())] = _mk("big " + k, rows, pal)
    out["GOOMBA"] = _mk("goomba", GOOMBA, GOOMBA_PAL)
    out["GOOMBA_FLAT"] = _mk("goomba flat", GOOMBA_FLAT, GOOMBA_PAL)
    out["KOOPA"] = _mk("koopa", KOOPA, KOOPA_PAL)
    out["SHELL"] = _mk("shell", SHELL, KOOPA_PAL)
    out["PIRANHA0"] = _mk("piranha open", PIRANHA_OPEN, PIRANHA_PAL)
    out["PIRANHA1"] = _mk("piranha shut", PIRANHA_SHUT, PIRANHA_PAL)
    out["MUSHROOM"] = _mk("mushroom", MUSHROOM, MUSH_PAL)
    out["ONEUP"] = _mk("1up", MUSHROOM, ONEUP_PAL)
    out["FLOWER"] = _mk("flower", FLOWER, FLOWER_PAL)
    out["STAR"] = _mk("star", STAR, STAR_PAL)
    for i, c in enumerate(COIN):
        out["COIN%d" % i] = _mk("coin", c, COIN_PAL)
    fb = _mk("fireball", FIREBALL, FIREBALL_PAL)
    for i in range(4):
        out["FIREBALL%d" % i] = fb
        fb = rotate90(fb)
    out["DEBRIS"] = _mk("debris", DEBRIS, {"k": K, "A": (240, 150, 90), "a": (180, 80, 40)})
    return out


__all__ = ["build", "hflip"]
