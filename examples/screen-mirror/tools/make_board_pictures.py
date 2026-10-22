#!/usr/bin/env python3
"""Draw one picture per supported board for the website's board picker (docs/boards/<id>.png).

Drawings, not photos (product photos belong to their sellers). Each one shows what a child can
check on the real board: how big the screen is (drawn to the same scale for every board), how
many USB sockets there are and what kind, and stickers or printing ("7789", "GUITION").
Sizes are approximate (board and screen outlines in millimetres from sellers' listings).

  python3 examples/screen-mirror/tools/make_board_pictures.py
"""
import os
from PIL import Image, ImageDraw, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "..", "..", "..", "docs", "boards")
FONTS = "/System/Library/Fonts/Supplemental/"
SS = 4             # supersampling
MM = 3.4           # pixels per millimetre in the final picture (same for every board)
W, H = 360, 230    # final picture size

YELLOW, YELLOW_EDGE = (250, 204, 21), (196, 150, 6)
DARK, DARK_EDGE = (38, 41, 48), (14, 15, 18)


def font(size, bold=True):
    try:
        return ImageFont.truetype(FONTS + ("Arial Bold.ttf" if bold else "Arial.ttf"), size)
    except OSError:
        return ImageFont.load_default()


def board(name, board_mm, screen_mm, sockets, pcb="yellow", sticker=None, printed=None):
    im = Image.new("RGBA", (W * SS, H * SS), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    s = MM * SS
    bw, bh = board_mm[0] * s, board_mm[1] * s
    x0, y0 = (W * SS - bw) / 2 + 6 * SS, (H * SS - bh) / 2
    fill, edge = (YELLOW, YELLOW_EDGE) if pcb == "yellow" else (DARK, DARK_EDGE)

    # shadow, board, mounting holes
    d.rounded_rectangle([x0 + 3 * SS, y0 + 4 * SS, x0 + bw + 3 * SS, y0 + bh + 4 * SS], radius=3 * s, fill=(0, 0, 0, 50))
    d.rounded_rectangle([x0, y0, x0 + bw, y0 + bh], radius=3 * s, fill=fill, outline=edge, width=2 * SS)
    for hx, hy in ((x0 + 3 * s, y0 + 3 * s), (x0 + bw - 3 * s, y0 + 3 * s), (x0 + 3 * s, y0 + bh - 3 * s), (x0 + bw - 3 * s, y0 + bh - 3 * s)):
        d.ellipse([hx - 1.4 * s, hy - 1.4 * s, hx + 1.4 * s, hy + 1.4 * s], fill=(235, 235, 235), outline=edge, width=SS)

    # screen: dark glass with a little picture so it reads as a screen
    sw, sh = screen_mm[0] * s, screen_mm[1] * s
    sx, sy = x0 + (bw - sw) / 2 + 2 * s, y0 + (bh - sh) / 2
    d.rectangle([sx - 1.2 * s, sy - 1.2 * s, sx + sw + 1.2 * s, sy + sh + 1.2 * s], fill=(20, 20, 24))
    d.rectangle([sx, sy, sx + sw, sy + sh], fill=(28, 52, 92))
    d.ellipse([sx + sw * 0.68, sy + sh * 0.14, sx + sw * 0.86, sy + sh * 0.14 + sw * 0.18], fill=(255, 210, 63))
    d.polygon([(sx, sy + sh), (sx + sw * 0.32, sy + sh * 0.5), (sx + sw * 0.58, sy + sh)], fill=(74, 160, 90))
    d.polygon([(sx + sw * 0.35, sy + sh), (sx + sw * 0.62, sy + sh * 0.62), (sx + sw, sy + sh)], fill=(52, 128, 70))
    d.line([sx + sw * 0.08, sy + sh * 0.1, sx + sw * 0.3, sy + sh * 0.1], fill=(255, 255, 255, 120), width=int(1.4 * s))

    # USB sockets on the left edge
    cy = y0 + bh / 2
    for i, kind in enumerate(sockets):
        oy = cy + (i - (len(sockets) - 1) / 2) * 13 * s
        if kind == "micro":   # trapezoid, silver
            d.polygon([(x0 - 3 * s, oy - 4 * s), (x0 + 4 * s, oy - 4 * s), (x0 + 4 * s, oy + 4 * s), (x0 - 3 * s, oy + 4 * s)], fill=(190, 194, 200), outline=(110, 114, 120))
            d.polygon([(x0 - 2.4 * s, oy - 2.4 * s), (x0 - 2.4 * s, oy + 2.4 * s), (x0 - 1 * s, oy + 1.6 * s), (x0 - 1 * s, oy - 1.6 * s)], fill=(60, 60, 64))
        else:                 # USB-C: rounded pill, darker
            d.rounded_rectangle([x0 - 3 * s, oy - 4.6 * s, x0 + 4 * s, oy + 4.6 * s], radius=2.6 * s, fill=(160, 164, 170), outline=(90, 94, 100), width=SS)
            d.rounded_rectangle([x0 - 2.2 * s, oy - 3 * s, x0 - 0.6 * s, oy + 3 * s], radius=0.8 * s, fill=(50, 50, 54))

    if printed:   # maker's name printed on the board, up the left margin
        f = font(int(3.6 * s))
        tw = int(d.textlength(printed, font=f))
        t = Image.new("RGBA", (tw + 4, int(4.6 * s)), (0, 0, 0, 0))
        ImageDraw.Draw(t).text((2, 0), printed, font=f, fill=(235, 235, 235))
        t = t.rotate(90, expand=True)
        im.alpha_composite(t, (int(x0 + 5.2 * s), int(cy - t.height / 2)))
    if sticker:   # a white sticker with text
        f = font(int(4.6 * s))
        tw = d.textlength(sticker, font=f)
        bx, by = x0 + bw - tw - 8 * s, y0 + 1.6 * s
        d.rounded_rectangle([bx - 2 * s, by - 0.4 * s, bx + tw + 2 * s, by + 6 * s], radius=1.2 * s, fill="white", outline=(150, 150, 150), width=SS)
        d.text((bx, by), sticker, font=f, fill=(200, 30, 50))

    im = im.resize((W, H), Image.LANCZOS)
    im.save(os.path.join(OUT, name + ".png"), optimize=True)


def chart():
    """All boards side by side with names: docs/guide/g00-which-board.png for the README."""
    rows = [("screen-mirror", '3.5"', "ESP32-035 / 3248S035"), ("screen-mirror-32", '3.2"', "ESP32-2432S032"),
            ("screen-mirror-28", '2.8"', "2432S028R, 1 socket"), ("screen-mirror-28-2usb", '2.8"', "2432S028R, 2 sockets"),
            ("screen-mirror-28-st7789", '2.8" "7789"', "2432S028R v3"), ("screen-mirror-jc2432w328", '2.8" Guition', "JC2432W328"),
            ("screen-mirror-24", '2.4"', "ESP32-2432S024")]
    cw, ch = W, H + 70
    im = Image.new("RGB", (cw * 4, ch * 2), "white")
    d = ImageDraw.Draw(im)
    for i, (name, big, model) in enumerate(rows):
        x, y = (i % 4) * cw, (i // 4) * ch
        pic = Image.open(os.path.join(OUT, name + ".png"))
        im.paste(pic, (x, y), pic)
        f = font(26)
        d.text((x + (cw - d.textlength(big, font=f)) / 2, y + H - 4), big, font=f, fill=(32, 33, 36))
        f = font(17, False)
        d.text((x + (cw - d.textlength(model, font=f)) / 2, y + H + 30), model, font=f, fill=(95, 99, 104))
    im.save(os.path.join(HERE, "..", "docs", "guide", "g00-which-board.png"), optimize=True)


if __name__ == "__main__":
    os.makedirs(OUT, exist_ok=True)
    #     website id                 board (mm)   screen (mm)  USB sockets
    board("screen-mirror",            (98, 55),   (74, 49),    ["micro"])
    board("screen-mirror-32",         (93, 55),   (65, 49),    ["micro"])
    board("screen-mirror-28",         (86, 50),   (58, 43),    ["micro"])
    board("screen-mirror-28-2usb",    (86, 50),   (58, 43),    ["micro", "c"])
    board("screen-mirror-28-st7789",  (86, 50),   (58, 43),    ["micro", "c"], sticker="7789")
    board("screen-mirror-jc2432w328", (86, 50),   (58, 43),    ["c"], pcb="dark", printed="GUITION")
    board("screen-mirror-24",         (76, 47),   (49, 37),    ["micro"])
    chart()
    print("wrote", os.path.normpath(OUT))
