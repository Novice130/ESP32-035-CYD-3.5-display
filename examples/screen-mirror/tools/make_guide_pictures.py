#!/usr/bin/env python3
"""Draw the student-guide pictures that a web screenshot can't show (deterministic, Pillow only):

  docs/guide/g00-which-board.png    how to tell the CYD boards apart
  docs/guide/g02b-port-picker.png   Chrome's "connect to a serial port" window
  docs/guide/g04b-share-picker.png  Chrome's "choose what to share" window
  docs/guide/g08-board-ready.png    what the board shows when it's ready (3.5" and 2.8")

The website screenshots (g01..g07) are taken from the real page with Playwright; see
LESSONS.md / README "Updating the guide pictures".

  python3 examples/screen-mirror/tools/make_guide_pictures.py
"""
import os
from PIL import Image, ImageDraw, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "..", "docs", "guide")
FONTS = "/System/Library/Fonts/Supplemental/"
RED = (225, 29, 72)


def font(size, bold=False):
    for name in (("Arial Bold.ttf" if bold else "Arial.ttf"), "Arial Unicode.ttf"):
        try:
            return ImageFont.truetype(FONTS + name, size)
        except OSError:
            pass
    return ImageFont.load_default()


def tag(d, xy, text, size=26):
    """Red rounded label, like the ones on the website screenshots."""
    f = font(size, True)
    x, y = xy
    w = d.textlength(text, font=f)
    d.rounded_rectangle([x, y, x + w + 32, y + size + 20], radius=(size + 20) // 2, fill=RED)
    d.text((x + 16, y + 9), text, font=f, fill="white")


def ring(d, box, width=6):
    x0, y0, x1, y1 = box
    d.rounded_rectangle([x0 - 8, y0 - 8, x1 + 8, y1 + 8], radius=14, outline=RED, width=width)


def window(w, h, title):
    im = Image.new("RGB", (w, h), (248, 249, 251))
    d = ImageDraw.Draw(im)
    d.rounded_rectangle([0, 0, w - 1, h - 1], radius=18, outline=(200, 204, 210), width=2, fill="white")
    d.text((32, 28), title, font=font(26, True), fill=(32, 33, 36))
    return im, d


def button(d, box, text, primary):
    fill, ink = ((26, 115, 232), "white") if primary else ("white", (26, 115, 232))
    d.rounded_rectangle(box, radius=22, fill=fill, outline=(26, 115, 232), width=2)
    f = font(22, True)
    tw = d.textlength(text, font=f)
    d.text(((box[0] + box[2] - tw) / 2, box[1] + 12), text, font=f, fill=ink)


def port_picker():
    im, d = window(760, 430, "robo.learnnovice.com wants to connect to a serial port")
    rows = ["USB Serial (cu.usbserial-11110)", "Bluetooth port", "debug-console"]
    for i, r in enumerate(rows):
        y = 100 + i * 64
        if i == 0:
            d.rounded_rectangle([28, y, 732, y + 52], radius=8, fill=(232, 240, 254))
        d.text((48, y + 13), r, font=font(24, i == 0), fill=(32, 33, 36))
    ring(d, (28, 100, 732, 152))
    tag(d, (330, 222), "pick the one with USB, CH340 or serial", 20)
    button(d, (420, 350, 560, 400), "Cancel", False)
    button(d, (580, 350, 732, 400), "Connect", True)
    ring(d, (580, 350, 732, 400))
    tag(d, (300, 284), "then click Connect", 22)
    im.save(os.path.join(OUT, "g02b-port-picker.png"))


def share_picker():
    im, d = window(900, 560, "Choose what to share with robo.learnnovice.com")
    tabs = ["Chrome Tab", "Window", "Entire Screen"]
    x = 32
    for i, t in enumerate(tabs):
        f = font(22, True)
        w = d.textlength(t, font=f)
        d.text((x, 86), t, font=f, fill=(26, 115, 232) if i == 1 else (95, 99, 104))
        if i == 1:
            d.rectangle([x, 118, x + w, 122], fill=(26, 115, 232))
            ring(d, (x - 6, 80, x + w + 6, 124), 5)
        x += w + 48
    # three window thumbnails, the first one chosen
    for i, (name, col) in enumerate([("Paint", (255, 236, 179)), ("Slides", (200, 230, 201)), ("Game", (187, 222, 251))]):
        x0 = 32 + i * 284
        d.rounded_rectangle([x0, 150, x0 + 262, 320], radius=10, fill=col,
                            outline=(26, 115, 232) if i == 0 else (218, 220, 224), width=4 if i == 0 else 2)
        d.text((x0 + 12, 330), name, font=font(22, i == 0), fill=(32, 33, 36))
    ring(d, (32, 150, 294, 360))
    tag(d, (32, 378), "1) pick the window you want on the board", 22)
    button(d, (560, 480, 700, 530), "Cancel", False)
    button(d, (720, 480, 868, 530), "Share", True)
    ring(d, (720, 480, 868, 530))
    tag(d, (470, 420), "2) click Share", 22)
    im.save(os.path.join(OUT, "g04b-share-picker.png"))


def board(w, h, label, scale):
    """A CYD with its waiting screen (same words the firmware draws)."""
    bw, bh = int(w * scale) + 120, int(h * scale) + 90
    im = Image.new("RGB", (bw, bh + 40), "white")
    d = ImageDraw.Draw(im)
    d.rounded_rectangle([0, 0, bw - 1, bh - 1], radius=18, fill=(250, 204, 21), outline=(202, 160, 8), width=4)
    sx, sy = 60, 45
    d.rectangle([sx - 6, sy - 6, sx + w * scale + 6, sy + h * scale + 6], fill=(40, 40, 40))
    d.rectangle([sx, sy, sx + w * scale, sy + h * scale], fill="black")
    k = scale * h / 320
    cx = sx + w * scale / 2

    def line(y, text, size, col, bold=False):
        f = font(max(10, int(size * k)), bold)
        tw = d.textlength(text, font=f)
        d.text((cx - tw / 2, sy + y * k - size * k / 2), text, font=f, fill=col)

    small = w < 400
    line(50, "Screen mirror", 26, "white", True)
    line(100, "Open this website in Chrome or Edge:", 16, (255, 255, 0))
    line(124, "robo.learnnovice.com", 16, (0, 255, 255))
    line(148, "then click  Share my screen." if small else "connect the board, then click  Share my screen.", 16, (255, 255, 0))
    line(200, "Wi-Fi is ready. Board name:", 16, (0, 255, 0))
    line(228, "cyd.local", 26, (0, 255, 0), True)
    line(260, "or address 192.168.0.42", 16, (128, 128, 128))
    f = font(24, True)
    tw = d.textlength(label, font=f)
    d.text(((bw - tw) / 2, bh + 6), label, font=f, fill=(32, 33, 36))
    return im


def board_ready():
    a = board(480, 320, '3.5" board', 1.0)
    b = board(320, 240, '2.8" / 2.4" board', 1.0)
    im = Image.new("RGB", (a.width + b.width + 60, max(a.height, b.height)), "white")
    im.paste(a, (0, 0))
    im.paste(b, (a.width + 60, (a.height - b.height) // 2))
    im.save(os.path.join(OUT, "g08-board-ready.png"))


def which_board():
    im = Image.new("RGB", (1200, 470), "white")
    d = ImageDraw.Draw(im)
    cards = [('3.5" screen', "the biggest one", "1 USB socket", "ESP32-035 / 3248S035"),
             ('2.8" screen', "1 micro-USB socket", "(the most common CYD)", "ESP32-2432S028R"),
             ('2.8" screen', "2 sockets: micro-USB + USB-C", "(newer)", "ESP32-2432S028R v2"),
             ('2.4" screen', "a bit smaller", "1 USB socket", "ESP32-2432S024R")]
    for i, (big, l1, l2, model) in enumerate(cards):
        x = 20 + i * 295
        d.rounded_rectangle([x, 20, x + 275, 450], radius=16, outline=(230, 220, 190), width=3, fill=(255, 251, 235))
        sw = {0: 210, 1: 180, 2: 180, 3: 160}[i]
        sh = sw * 2 // 3
        bx = x + (275 - sw - 30) // 2
        d.rounded_rectangle([bx, 50, bx + sw + 30, 50 + sh + 30], radius=10, fill=(250, 204, 21), outline=(202, 160, 8), width=3)
        d.rectangle([bx + 15, 65, bx + 15 + sw, 65 + sh], fill=(30, 30, 30))
        # USB sockets on the left edge
        d.rectangle([bx - 8, 65 + sh // 2 - 18, bx + 6, 65 + sh // 2 - 2], fill=(150, 150, 150))
        if i == 2:
            d.rounded_rectangle([bx - 8, 65 + sh // 2 + 6, bx + 6, 65 + sh // 2 + 26], radius=4, fill=(90, 90, 90))
        d.text((x + 20, 260), big, font=font(30, True), fill=(32, 33, 36))
        d.text((x + 20, 304), l1, font=font(18), fill=(60, 64, 67))
        d.text((x + 20, 330), l2, font=font(18), fill=(60, 64, 67))
        d.text((x + 20, 400), model, font=font(16, True), fill=(120, 120, 120))
    im.save(os.path.join(OUT, "g00-which-board.png"))


if __name__ == "__main__":
    os.makedirs(OUT, exist_ok=True)
    which_board()
    port_picker()
    share_picker()
    board_ready()
    print("wrote", OUT)
