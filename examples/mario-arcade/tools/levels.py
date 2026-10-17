"""
levels.py - the level layouts: World 1 is hand-designed, Worlds 2-8 come from a
seeded generator that gets harder world by world. Each world has the same four
stages: x-1 day hills, x-2 underground cave, x-3 sunset treetops, x-4 night lava.

Levels are 10 tiles tall (16 px tiles, 240x160 view = 15 columns). Row 0 sits under
the HUD, the ground is rows 8-9. Each level is built with a small builder so the
layout reads like a script; build() returns the tile rows plus enemy spawns.

Tile chars (consumed by game.h):
  ' ' empty   '#' ground   'B' brick   'b' brick w/ coins   's' brick w/ star
  'u' brick w/ 1-up   '?' ? block (coin)   'M' ? block (mushroom/flower)
  'X' hard block   '[' ']' pipe top   '{' '}' pipe body   'T' treetop   '|' trunk
  '~' lava   'o' coin
Enemy kinds: g goomba, k koopa, p piranha (placed at the pipe's left column).
"""
import random

ROWS = 10
GROUND_ROW = 8


class Level:
    def __init__(self, name, theme, width, time=300):
        self.name, self.theme, self.w, self.time = name, theme, width, time
        self.g = [[" "] * width for _ in range(ROWS)]
        self.ents = []
        self.flag = self.castle = None

    # ------------------------------------------------------------- terrain
    def ground(self, x0, x1, top=GROUND_ROW):
        for x in range(x0, x1 + 1):
            for y in range(top, ROWS):
                self.g[y][x] = "#"

    def gap(self, x0, x1):
        for x in range(x0, x1 + 1):
            for y in range(ROWS):
                if self.g[y][x] == "#":
                    self.g[y][x] = " "

    def lava(self, x0, x1):
        self.gap(x0, x1)
        for x in range(x0, x1 + 1):
            self.g[9][x] = "~"

    def ceiling(self, x0, x1, row=1):
        for x in range(x0, x1 + 1):
            self.g[row][x] = "B"

    def put(self, x, row, s):
        for i, ch in enumerate(s):
            if ch != ".":
                self.g[row][x + i] = ch

    def pipe(self, x, h, piranha=False):
        """h = tiles above the ground (2..4)."""
        top = GROUND_ROW - h
        self.g[top][x], self.g[top][x + 1] = "[", "]"
        for y in range(top + 1, GROUND_ROW):
            self.g[y][x], self.g[y][x + 1] = "{", "}"
        if piranha:
            self.ents.append(("p", x, top))

    def stairs_up(self, x, h):
        for i in range(h):
            for y in range(GROUND_ROW - 1 - i, GROUND_ROW):
                self.g[y][x + i] = "X"

    def stairs_down(self, x, h):
        for i in range(h):
            for y in range(GROUND_ROW - h + i, GROUND_ROW):
                self.g[y][x + i] = "X"

    def column(self, x, h):
        for y in range(GROUND_ROW - h, GROUND_ROW):
            self.g[y][x] = "X"

    def tree(self, x0, x1, row):
        """treetop platform from x0..x1 at `row`, trunk down to the bottom."""
        for x in range(x0, x1 + 1):
            self.g[row][x] = "T"
        mid = (x0 + x1) // 2
        for x in (mid, mid + 1) if x1 - x0 >= 3 else (mid,):
            for y in range(row + 1, ROWS):
                self.g[y][x] = "|"

    def coins(self, x0, x1, row):
        for x in range(x0, x1 + 1):
            self.g[row][x] = "o"

    def enemy(self, kind, x, row=None):
        if row is None:  # stand on the floor of that column (rows 0-4 are ceilings / blocks)
            row = next(y for y in range(5, ROWS) if self.g[y][x] not in " o|") - 1
        self.ents.append((kind, x, row))

    def finish(self, flag_x):
        """staircase-free ending: flagpole on a block, castle 6 tiles later."""
        self.flag = flag_x
        self.g[GROUND_ROW - 1][flag_x] = "X"
        self.castle = flag_x + 5

    def warnings(self):
        """Walking enemies meet the player about 3 columns before where they were
        placed. Meeting them under a low block row leaves no room to hop on them."""
        out = []
        for kind, x, row in self.ents:
            if kind == "p":
                continue
            meet = x - 3
            for c in range(meet - 2, meet + 2):
                if 0 <= c < self.w and any(self.g[r][c] in "BbsuM?X" and self.g[r + 1][c] in " o"
                                           for r in range(max(0, row - 4), row - 1)):
                    out.append("%s: %s at col %d meets the player under blocks (col %d)" % (self.name, kind, x, c))
                    break
        return out

    def build(self):
        assert self.flag is not None, self.name
        rows = ["".join(r) for r in self.g]
        ents = sorted(self.ents, key=lambda e: e[1])
        return rows, ents


# ============================================================================ 1-1
def day():
    L = Level("1-1", "DAY", 200)
    L.ground(0, 199)
    L.put(12, 4, "?")
    L.put(17, 4, "B?BMB")
    L.put(19, 1, "?")
    L.pipe(27, 2)
    L.enemy("g", 33)
    L.pipe(37, 3)
    L.enemy("g", 42)
    L.enemy("g", 44)
    L.pipe(49, 3, piranha=True)
    L.coins(55, 58, 5)
    L.enemy("k", 60)
    L.gap(66, 67)
    L.coins(65, 68, 4)
    L.put(74, 4, "B?Bb")
    L.put(82, 4, "BBMBB")
    L.enemy("g", 92)
    L.enemy("g", 94)
    L.put(94, 4, "?.?.?")
    L.enemy("k", 104)
    L.gap(106, 108)
    L.coins(106, 108, 4)
    L.put(113, 4, "BsB")
    L.stairs_up(118, 4)
    L.gap(122, 123)
    L.stairs_down(124, 4)
    L.enemy("g", 132)
    L.enemy("g", 134)
    L.pipe(138, 2)
    L.put(143, 4, "BB?B")
    L.enemy("k", 152)
    L.pipe(155, 2, piranha=True)
    L.coins(160, 163, 5)
    L.enemy("g", 162)
    L.stairs_up(167, 5)
    L.ground(172, 173, GROUND_ROW - 5)
    L.finish(180)
    return L


# ============================================================================ 1-2
def cave():
    L = Level("1-2", "CAVE", 200, time=300)
    L.ground(0, 199)
    L.ceiling(6, 160)
    L.put(10, 4, "MMM")
    L.coins(18, 22, 5)
    L.enemy("g", 20)
    L.enemy("g", 22)
    L.column(28, 2)
    L.column(30, 3)
    L.coins(29, 31, 3)
    L.enemy("k", 33)
    L.put(36, 4, "BBbBB")
    L.gap(46, 47)
    L.put(52, 5, "XXXX")
    L.coins(52, 55, 4)
    L.enemy("g", 61)
    L.enemy("g", 63)
    L.pipe(65, 3, piranha=True)
    L.pipe(72, 2)
    L.enemy("k", 78)
    L.put(82, 4, "BuBBB")
    L.gap(90, 92)
    L.coins(90, 92, 5)
    L.column(96, 1)
    L.column(97, 2)
    L.column(98, 3)
    L.enemy("g", 104)
    L.enemy("g", 106)
    L.enemy("g", 108)
    L.put(112, 4, "B?B?B")
    L.pipe(120, 2, piranha=True)
    L.pipe(128, 3)
    L.enemy("k", 133)
    L.gap(138, 139)
    L.put(143, 4, "BBBsB")
    L.enemy("g", 153)
    L.enemy("k", 156)
    L.pipe(158, 2)
    L.stairs_up(164, 4)
    L.gap(168, 170)
    L.stairs_down(171, 3)
    L.finish(180)
    return L


# ============================================================================ 1-3
def dusk():
    L = Level("1-3", "DUSK", 196, time=300)
    L.ground(0, 14)
    L.tree(17, 21, 6)
    L.coins(18, 20, 4)
    L.tree(24, 29, 5)
    L.enemy("g", 27)
    L.tree(32, 35, 7)
    L.tree(38, 44, 6)
    L.put(40, 1, "M")
    L.tree(47, 50, 5)
    L.coins(47, 50, 3)
    L.tree(53, 60, 6)
    L.enemy("g", 57)
    L.enemy("g", 59)
    L.tree(63, 66, 7)
    L.tree(69, 74, 5)
    L.put(71, 1, "?")
    L.ground(77, 92)
    L.enemy("k", 92)
    L.put(82, 4, "B?BMB")
    L.tree(95, 99, 6)
    L.tree(102, 108, 5)
    L.enemy("g", 106)
    L.tree(111, 114, 6)
    L.coins(111, 114, 4)
    L.tree(117, 123, 7)
    L.enemy("k", 121)
    L.tree(126, 130, 5)
    L.put(128, 1, "s")
    L.tree(133, 138, 6)
    L.enemy("g", 137)
    L.tree(141, 144, 7)
    L.tree(147, 152, 6)
    L.coins(147, 152, 4)
    L.ground(155, 195)
    L.enemy("g", 162)
    L.enemy("g", 164)
    L.stairs_up(166, 4)
    L.finish(176)
    return L


# ============================================================================ 1-4
def night():
    L = Level("1-4", "NIGHT", 200, time=300)
    L.ground(0, 199)
    L.put(11, 4, "?M?")
    L.enemy("g", 20)
    L.lava(23, 25)
    L.coins(23, 25, 4)
    L.column(29, 2)
    L.enemy("k", 34)
    L.pipe(38, 3, piranha=True)
    L.lava(44, 46)
    L.coins(44, 46, 5)
    L.enemy("g", 51)
    L.enemy("g", 53)
    L.put(57, 4, "BbB")
    L.column(63, 3)
    L.lava(64, 66)
    L.column(67, 3)
    L.enemy("k", 74)
    L.put(78, 4, "MB?B")
    L.enemy("g", 87)
    L.enemy("g", 89)
    L.pipe(91, 2)
    L.lava(96, 98)
    L.enemy("k", 104)
    L.pipe(108, 3, piranha=True)
    L.put(115, 4, "BBuBB")
    L.lava(124, 126)
    L.coins(124, 126, 3)
    L.stairs_up(130, 3)
    L.lava(133, 135)
    L.stairs_down(136, 3)
    L.enemy("g", 143)
    L.enemy("k", 148)
    L.put(150, 4, "?s?")
    L.pipe(156, 2, piranha=True)
    L.lava(161, 162)
    L.stairs_up(166, 5)
    L.ground(171, 172, GROUND_ROW - 5)
    L.finish(180)
    return L


# ============================================================================ worlds 2-8
# The generator strings together short "set pieces" (pipes, holes, enemy groups,
# block rows, stairs, platforms) on flat ground. Difficulty d = 0 (world 2) .. 6
# (world 8) raises the odds of the harder pieces, the enemy count and the hole
# widths, and shortens the breathing room between pieces. Holes stay at most 3
# tiles wide (4 with a floating step in the middle), so every jump works at
# walking speed. Fixed seeds keep the output identical on every run.

BLOCK_ROWS = ["B?BMB", "?B?", "BBMBB", "B?Bb", "?.?.?", "BsB", "BBuBB", "?M?", "BbB", "B?B?B"]


def _blocks_over(L, x0, x1):
    return any(L.g[y][x] in "BbsuM?" for y in range(2, 7) for x in range(max(0, x0), min(L.w, x1 + 1)))


def _enemies(L, r, x, d, n=None):
    """a group of walkers on the flat ground from x on, moved right until no block row
    hangs over where they meet the player; returns the width used from x"""
    n = n or r.choice([1, 2, 2, 3] if d >= 2 else [1, 1, 2])
    start = x
    while _blocks_over(L, x - 6, x + 2 * n):
        x += 1
    for i in range(n):
        L.enemy("k" if r.random() < 0.2 + 0.05 * d else "g", x + 2 * i)
    return x - start + 2 * n


def _hole(L, r, x, d, lava):
    w = r.choice([2, 2, 3] if d < 3 else [2, 3, 3])
    (L.lava if lava else L.gap)(x, x + w - 1)
    if r.random() < 0.5:
        L.coins(x, x + w - 1, r.choice([4, 5]))
    return w


def _pipes(L, r, x, d, cave):
    hs = [2, 3] if cave else [2, 2, 3, 3]  # a 4-high pipe needs a pixel-perfect jump
    piranha = r.random() < 0.3 + 0.08 * d
    L.pipe(x, r.choice(hs), piranha)
    if r.random() < 0.5:  # a second pipe with a walker bouncing between them
        L.enemy("g", x + 5)
        L.pipe(x + 8, r.choice(hs), r.random() < 0.2 + 0.08 * d)
        return 10
    return 2


def _blocks(L, r, x, d, force_power=False):
    pat = "B?BMB" if force_power else r.choice(BLOCK_ROWS)
    L.put(x, 4, pat)
    if r.random() < 0.4:
        L.coins(x, x + len(pat) - 1, 2)
    return len(pat)


def _pyramid(L, r, x, d, cave):
    # under the cave ceiling a 4-high top leaves no room to jump
    h = 3 if cave else r.choice([3, 4])
    L.stairs_up(x, h)
    g = 0 if cave else r.choice([0, 2] if d < 2 else [2, 2, 3])
    if g:
        L.gap(x + h, x + h + g - 1)
    L.stairs_down(x + h + g, h)
    return 2 * h + g


def _columns(L, r, x, d):
    hs = [r.choice([1, 2]), r.choice([2, 3]), r.choice([1, 2, 3])]
    for i, h in enumerate(hs):
        L.column(x + 2 * i, h)
    L.coins(x + 1, x + 3, 4)
    return 5


def _step_hole(L, r, x, d, lava):
    """a wide hole with a floating hard-block step in the middle"""
    (L.lava if lava else L.gap)(x, x + 5)
    L.put(x + 2, 6, "XX")
    L.coins(x + 2, x + 3, 4)
    return 6


def _lava_bridge(L, r, x, d):
    L.column(x, 3)
    L.lava(x + 1, x + 3)
    L.column(x + 4, 3)
    return 5


def _ending(L, flag=180):
    h = 4 if L.theme != "DAY" else 5
    L.stairs_up(flag - 13, h)
    L.ground(flag - 13 + h, flag - 13 + h + 1, GROUND_ROW - h)
    L.finish(flag)


def gen_ground(world, stage, theme):
    d = world - 2
    r = random.Random(1000 * world + stage)
    L = Level("%d-%d" % (world, stage), theme, 200, time=300 if world < 6 else 250)
    L.ground(0, 199)
    cave, lava = theme == "CAVE", theme == "NIGHT"
    if cave:
        L.ceiling(6, 160)
    pieces = [("enemies", 4), ("hole", 3 + d // 2), ("pipes", 3), ("blocks", 3), ("pyramid", 2),
              ("columns", 1 + cave), ("step", d // 2), ("bridge", 2 * lava * (d >= 1))]
    names = [n for n, w in pieces for _ in range(w)]
    x, last = 12, None
    x += _blocks(L, r, x, d, force_power=True) + 4
    while x < 150:
        kind = r.choice([n for n in names if n != last])
        last = kind
        if kind == "enemies":
            w = _enemies(L, r, x + 2, d) + 2
        elif kind == "hole":
            w = _hole(L, r, x, d, lava)
        elif kind == "pipes":
            w = _pipes(L, r, x, d, cave)
        elif kind == "blocks":
            w = _blocks(L, r, x, d)
            if r.random() < 0.3 + 0.06 * d:  # walkers come at you after the blocks
                w += _enemies(L, r, x + w + 4, d) + 4
        elif kind == "pyramid":
            w = _pyramid(L, r, x, d, cave)
        elif kind == "columns":
            w = _columns(L, r, x, d)
        elif kind == "step":
            w = _step_hole(L, r, x, d, lava)
        else:
            w = _lava_bridge(L, r, x, d)
        if kind in ("hole", "pyramid", "columns", "step", "bridge") and r.random() < 0.25 + 0.05 * d:
            w += _enemies(L, r, x + w + 4, d) + 4  # walkers waiting on the far side
        x += w + r.randint(max(3, 6 - d // 2), max(5, 9 - d // 2))
    if r.random() < 0.5 + 0.07 * d:
        _enemies(L, r, 158, d)
    _ending(L)
    return L


def gen_dusk(world, stage):
    d = world - 2
    r = random.Random(1000 * world + stage)
    L = Level("%d-%d" % (world, stage), "DUSK", 200, time=300 if world < 6 else 250)
    L.ground(0, 14)
    x, row, gap, midground = 17, 7, 2, False
    while x < 150:
        if not midground and x > 70:
            # a stretch of solid ground with blocks, then walkers
            midground = True
            L.ground(x, x + 19)
            L.put(x + 2, 4, r.choice(["B?BMB", "?M?", "BsB"]))
            _enemies(L, r, x + 12, d, n=r.choice([1, 2]))
            x, row, gap = x + 22, 7, 2
            continue
        w = r.randint(max(3, 6 - d // 2), max(4, 7 - d // 3))
        new_row = min(7, max(5, row + r.choice([-1, 0, 1])))
        if new_row < row and gap > 2:  # climbing: keep the hole short
            x -= 1
        row = new_row
        L.tree(x, x + w - 1, row)
        if w >= 5 and r.random() < 0.35 + 0.08 * d:
            L.enemy("k" if r.random() < 0.25 + 0.05 * d else "g", x + w - 2)
        else:
            if r.random() < 0.4:
                L.coins(x, x + w - 1, row - 2)
            if r.random() < 0.2:
                L.put(x + w // 2, 1, r.choice("?Ms"))
        gap = r.choice([2, 2, 3] if d < 3 else [2, 3, 3])
        x += w + gap
    L.ground(x, 199)
    if x < 152 and r.random() < 0.5 + 0.07 * d:
        _enemies(L, r, x + 6, d)
    _ending(L)
    return L


def generated(world, stage):
    theme = ["DAY", "CAVE", "DUSK", "NIGHT"][stage - 1]
    return gen_dusk(world, stage) if theme == "DUSK" else gen_ground(world, stage, theme)


WORLDS = 8
ALL = [day, cave, dusk, night] + [
    (lambda w=w, s=s: generated(w, s)) for w in range(2, WORLDS + 1) for s in range(1, 5)]
