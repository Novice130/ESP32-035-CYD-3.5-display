#!/usr/bin/env python3
"""Make every picture in EXPLAINER.md from the real engine, running in tools/sim.cpp.

    python3 tools/make_explainer_pics.py

Writes docs/screens.png, docs/screenshot.png and docs/explainer/*.png. Frames go to a temp folder.
"""
import math
import os
import shutil
import subprocess
import tempfile
from PIL import Image, ImageDraw, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
DOCS = os.path.join(ROOT, "docs")
OUT = os.path.join(DOCS, "explainer")
REPO = os.path.dirname(os.path.dirname(ROOT))
SCENES = [("bronze", "Desk Bronze", "sand"), ("ember", "Deep Focus", "glowing embers"), ("water", "Clepsydra", "water drops"),
          ("zen", "Zen Ink", "ink"), ("chem", "Titration", "a chemical"), ("synth", "Neon Flow", "plasma"),
          ("cosmos", "Event Horizon", "a galaxy")]
PAPER, INK, MUTED, ACCENT, LINE = (255, 255, 255), (28, 32, 44), (92, 100, 118), (214, 72, 48), (214, 218, 228)
os.makedirs(OUT, exist_ok=True)
tmp = tempfile.mkdtemp()
sim = os.path.join(tmp, "sim")
subprocess.run(["clang++", "-std=gnu++17", "-O1", "-w", "-I" + os.path.join(ROOT, "include"), os.path.join(HERE, "sim.cpp"), "-o", sim], check=True)


def run(scene, *cmds):
    subprocess.run([sim, tmp, str(scene)] + list(cmds), check=True, stdout=subprocess.DEVNULL)


def frame(name):
    return Image.open(os.path.join(tmp, name + ".ppm")).convert("RGB")


def font(size):
    return ImageFont.load_default(size)


def card(img, border=1):
    d = ImageDraw.Draw(img)
    d.rectangle((0, 0, img.width - 1, img.height - 1), outline=(40, 44, 56), width=border)
    return img


def centred(d, cx, y, text, size, fill=INK):
    f = font(size)
    d.text((cx - f.getlength(text) / 2, y), text, font=f, fill=fill)


def strip(frames, captions, path, gap=20, top=0, cap=52, scale=1.0, between=None):
    """Frames side by side on white, a caption under each, optional sign between them."""
    w = int(frames[0].width * scale)
    h = int(frames[0].height * scale)
    n = len(frames)
    sheet = Image.new("RGB", (n * w + (n + 1) * gap, top + h + cap + gap), PAPER)
    d = ImageDraw.Draw(sheet)
    for i, (im, c) in enumerate(zip(frames, captions)):
        x = gap + i * (w + gap)
        sheet.paste(card(im.resize((w, h), Image.LANCZOS if scale != 1 else Image.NEAREST)), (x, gap // 2 + top))
        for k, line in enumerate(c.split("\n")):
            centred(d, x + w / 2, gap // 2 + top + h + 8 + k * 22, line, 18 if k == 0 else 16, INK if k == 0 else MUTED)
        if between and i < n - 1:
            centred(d, x + w + gap / 2, gap // 2 + top + h / 2 - 16, between[i], 26, ACCENT)
    sheet.save(path, optimize=True)


def arrow(d, x0, y0, x1, y1, color=ACCENT, width=3):
    d.line((x0, y0, x1, y1), fill=color, width=width)
    a = math.atan2(y1 - y0, x1 - x0)
    for s in (2.6, -2.6):
        d.line((x1, y1, x1 + 11 * math.cos(a + s), y1 + 11 * math.sin(a + s)), fill=color, width=width)


# ---------------------------------------------------------------- every scene
for i, (key, name, what) in enumerate(SCENES):
    run(i, "run=95", "p=0.4", "run=10", f"shot=s_{key}")
    frame(f"s_{key}").save(os.path.join(OUT, f"scene-{key}.png"), optimize=True)
run(2, "run=95", "p=0.3", "break", "p=0.31", "run=40", "shot=s_break")

names = [s[1] for s in SCENES] + ["A break (Clepsydra)"]
keys = [s[0] for s in SCENES] + ["break"]
sheet = Image.new("RGB", (4 * 320 + 5 * 12, 2 * (480 + 34) + 12), (18, 20, 28))
d = ImageDraw.Draw(sheet)
for i, (k, n) in enumerate(zip(keys, names)):
    x = 12 + (i % 4) * 332
    y = 12 + (i // 4) * (480 + 34)
    sheet.paste(frame("s_" + k), (x, y))
    centred(d, x + 160, y + 484, n, 18, (232, 236, 244))
sheet.save(os.path.join(DOCS, "screens.png"), optimize=True)
shutil.copy(os.path.join(OUT, "scene-synth.png"), os.path.join(DOCS, "screenshot.png"))

# ---------------------------------------------------------------- tour of the screen
ZOOM = 1.5
base = frame("s_bronze").resize((480, 720), Image.LANCZOS)
tour = Image.new("RGB", (1000, 760), PAPER)
tour.paste(card(base), (20, 20))
d = ImageDraw.Draw(tour)
# number, x and y on the 320x480 screen (in an empty spot beside the thing), title, explanation
PARTS = [(1, 10, 12, "The clock", "Minutes and seconds left."),
         (2, 296, 18, "FOCUS or BREAK", "What to do right now."),
         (3, 272, 31, "30 min", "How long this session is."),
         (4, 297, 58, "40% and Worked", "How far along you are, and your total work time."),
         (5, 170, 73, "The progress bar", "It fills up as time passes."),
         (6, 215, 240, "The hourglass", "The sand falls while you work."),
         (7, 300, 462, "The quote", "A short saying. It changes about every 30 seconds.")]
ly = 40
for n, sx, sy, title, sub in PARTS:
    cx, cy = 20 + sx * ZOOM, 20 + sy * ZOOM
    d.ellipse((cx - 11, cy - 11, cx + 11, cy + 11), fill=ACCENT, outline=PAPER, width=2)
    centred(d, cx, cy - 9, str(n), 15, PAPER)
    d.ellipse((530, ly, 566, ly + 36), fill=ACCENT)
    centred(d, 548, ly + 5, str(n), 22, PAPER)
    d.text((580, ly - 2), title, font=font(26), fill=INK)
    d.text((580, ly + 32), sub, font=font(18), fill=MUTED)
    ly += 100
tour.save(os.path.join(OUT, "tour.png"), optimize=True)

# ---------------------------------------------------------------- one whole session
run(0, "run=3", "shot=a")
run(0, "run=95", "p=0.5", "run=5", "praise", "run=20", "shot=b")
run(0, "run=95", "p=1.0", "run=40", "ask", "run=5", "shot=c")
run(0, "run=95", "p=0.3", "break", "p=0.5", "run=40", "shot=d")
strip([frame(x) for x in "abcd"],
      ["1. Start\nSand is all on top", "2. Halfway\nA well-done message", "3. Time is up\nThe board asks you", "4. Break\nSand turns green"],
      os.path.join(OUT, "session.png"), scale=0.72)

# ---------------------------------------------------------------- layers
run(0, "run=95", "p=0.4", "run=10", "shot=l_all", "layers=l")
strip([frame("l_bg"), frame("l_live"), frame("l_front"), frame("l_all")],
      ["Background\nSaved, never changes", "Sand\nDrawn new every frame", "Glass and rods\nSaved, goes on top", "Finished picture\nSent to the screen"],
      os.path.join(OUT, "layers.png"), scale=0.72, between=["+", "+", "="])

# ---------------------------------------------------------------- flipbook: one falling drop
run(2, "run=95", "p=0.4", "run=70", "seq=f,16,3")
all_frames = [frame(f"f_{i}") for i in range(16)]
box = (110, 225, 210, 345)
crops = [all_frames[i * 2 + 3].crop(box).resize((200, 240), Image.LANCZOS) for i in range(6)]
strip(crops, ["Frame 1", "Frame 2", "Frame 3", "Frame 4", "Frame 5", "Frame 6"], os.path.join(OUT, "flipbook.png"), gap=12, cap=36)

# ---------------------------------------------------------------- level in a curved glass
run(5, "run=95", "p=0.1", "run=10", "shot=g0", "p=0.5", "run=10", "shot=g1", "p=0.9", "run=10", "shot=g2")
strip([frame("g0"), frame("g1"), frame("g2")], ["10% of the time", "50% of the time", "90% of the time"],
      os.path.join(OUT, "levels.png"), scale=0.72, cap=38)

# ---------------------------------------------------------------- turning the crack
dg = Image.new("RGB", (980, 500), PAPER)
d = ImageDraw.Draw(dg)
LS = (40, 150, 310, 330)
d.rounded_rectangle(LS, 12, fill=(30, 34, 46), outline=INK, width=4)
d.line((175, 150, 175, 330), fill=ACCENT, width=4)
centred(d, 175, 104, "Lying down", 24)
centred(d, 175, 344, "The line is up and down,", 18, MUTED)
centred(d, 175, 368, "in the middle of the screen", 18, MUTED)
arrow(d, 330, 240, 410, 240, width=5)
centred(d, 370, 200, "turn", 20, ACCENT)
PS = (430, 60, 610, 330 + 60)
d.rounded_rectangle(PS, 12, fill=(30, 34, 46), outline=INK, width=4)
d.line((430, 225, 610, 225), fill=ACCENT, width=4)
centred(d, 520, 24, "Standing up", 24)
centred(d, 520, 404, "Now the line goes", 18, MUTED)
centred(d, 520, 428, "side to side", 18, MUTED)
shot = frame("s_synth").resize((240, 360), Image.LANCZOS)
sd = ImageDraw.Draw(shot)
for x in range(0, 240, 12):
    sd.line((x, 180, x + 6, 180), fill=(255, 70, 70), width=2)
dg.paste(card(shot), (700, 60))
centred(d, 820, 24, "The trick", 24)
centred(d, 820, 428, "The thin neck stands on the line.", 17, MUTED)
centred(d, 820, 452, "Words stay far away from it.", 17, MUTED)
dg.save(os.path.join(OUT, "crack-trick.png"), optimize=True)

# ---------------------------------------------------------------- which way up
upright = frame("s_synth").resize((240, 360), Image.LANCZOS)
ud = Image.new("RGB", (640, 520), PAPER)
d = ImageDraw.Draw(ud)
for x, im, code, note in ((40, upright, "tft.setRotation(2);", "The right way up"),
                          (340, upright.rotate(180), "tft.setRotation(0);", "Upside down")):
    centred(d, x + 130, 16, code, 24, ACCENT)
    d.rounded_rectangle((x, 56, x + 260, 56 + 380), 14, fill=(30, 34, 46))
    ud.paste(im, (x + 10, 66))
    centred(d, x + 130, 448, note, 22)
centred(d, 320, 484, "Same board, held the same way", 18, MUTED)
ud.save(os.path.join(OUT, "upside-down.png"), optimize=True)

# ---------------------------------------------------------------- where the parts are on the board
photo = Image.open(os.path.join(REPO, "docs", "images", "esp32_035_display_front.png")).convert("RGB")
pw = Image.new("RGB", (800, 620), PAPER)
crop = photo.crop((0, 640, 460, 880))
crop = crop.resize((460 * 3 // 2 // 1, 240 * 3 // 2), Image.LANCZOS)
pw.paste(crop, (50, 170))
d = ImageDraw.Draw(pw)
centred(d, 400, 22, "The bottom of the board, up close", 28)
d.text((60, 76), "Light sensor", font=font(24), fill=INK)
d.text((60, 106), "Cover it with one fingertip", font=font(18), fill=MUTED)
arrow(d, 150, 134, int(50 + 105 * 1.5), int(170 + (782 - 640) * 1.5))
d.text((470, 76), "Coloured light", font=font(24), fill=INK)
d.text((470, 106), "Flashes when things happen", font=font(18), fill=MUTED)
arrow(d, 530, 134, int(50 + 298 * 1.5), int(170 + (820 - 640) * 1.5) - 22)
pw = pw.crop((0, 0, 800, 170 + crop.height + 16))
pw.save(os.path.join(OUT, "board-parts.png"), optimize=True)

shutil.rmtree(tmp)
print("pictures written to", os.path.relpath(OUT))
