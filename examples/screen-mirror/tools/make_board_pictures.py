#!/usr/bin/env python3
"""Board pictures for the website's board picker (docs/boards/<id>.jpg) and the README chart
(docs/guide/g00-which-board.png), made from real product photos.

The photos belong to the sellers / sites listed in PHOTOS (used, with the project owner's OK, so
children can recognise their board). Two 2.8" variants look the same from the front as the
original, so their pictures get a small badge for the thing that tells them apart.

  python3 examples/screen-mirror/tools/make_board_pictures.py     (downloads, then draws)
"""
import io
import os
import urllib.request
from PIL import Image, ImageChops, ImageDraw, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "..", "..", "..", "docs", "boards")
CHART = os.path.join(HERE, "..", "docs", "guide", "g00-which-board.png")
FONTS = "/System/Library/Fonts/Supplemental/"
W, H = 400, 270

R28 = "https://i0.wp.com/randomnerdtutorials.com/wp-content/uploads/2024/03/ESP32-Cheap-Yellow-Display-CYD-Board-ESP32-2432S028R-front.jpg"
PHOTOS = {   # website id: (photo URL, badge)
    "screen-mirror": ("https://cdn.espboards.net/boards/cyd-esp32-3248s035/cover-600.png", None),
    "screen-mirror-32": ("https://cdn.espboards.net/boards/cyd-esp32-2432s032/cover-600.png", None),
    "screen-mirror-28": (R28, None),
    "screen-mirror-28-2usb": (R28, "+ USB-C socket"),
    "screen-mirror-28-st7789": (R28, '"7789" on the box'),
    "screen-mirror-jc2432w328": ("https://www.atomic14.com/esp32/assets/boards/guition-jc2432w328.webp", None),
    "screen-mirror-24": ("https://cdn.espboards.net/boards/cyd-esp32-2432s024/cover-600.png", None),
}
CHART_ROWS = [("screen-mirror", '3.5"', "ESP32-035 / 3248S035"), ("screen-mirror-32", '3.2"', "ESP32-2432S032"),
              ("screen-mirror-28", '2.8"', "2432S028R, 1 socket"), ("screen-mirror-28-2usb", '2.8"', "2432S028R, 2 sockets"),
              ("screen-mirror-28-st7789", '2.8" "7789"', "2432S028R v3"), ("screen-mirror-jc2432w328", '2.8" Guition', "JC2432W328"),
              ("screen-mirror-24", '2.4"', "ESP32-2432S024")]


def font(size, bold=True):
    try:
        return ImageFont.truetype(FONTS + ("Arial Bold.ttf" if bold else "Arial.ttf"), size)
    except OSError:
        return ImageFont.load_default()


def fetch(url, cache={}):
    if url not in cache:
        req = urllib.request.Request(url, headers={"User-Agent": "Mozilla/5.0"})
        cache[url] = Image.open(io.BytesIO(urllib.request.urlopen(req, timeout=30).read())).convert("RGB")
    return cache[url]


def trim(im):
    """Cut away plain white or grey margins around the product."""
    bg = Image.new("RGB", im.size, im.getpixel((2, 2)))
    box = ImageChops.difference(im, bg).convert("L").point(lambda v: 255 if v > 24 else 0).getbbox()
    return im.crop(box) if box else im


def card(name, url, badge):
    im = trim(fetch(url))
    im.thumbnail((W - 24, H - 24), Image.LANCZOS)
    out = Image.new("RGB", (W, H), "white")
    out.paste(im, ((W - im.width) // 2, (H - im.height) // 2))
    if badge:
        d = ImageDraw.Draw(out)
        f = font(22)
        tw = d.textlength(badge, font=f)
        d.rounded_rectangle([W - tw - 34, 8, W - 8, 46], radius=19, fill=(225, 29, 72))
        d.text((W - tw - 21, 15), badge, font=f, fill="white")
    out.save(os.path.join(OUT, name + ".jpg"), quality=86, optimize=True)
    return out


def chart(cards):
    cw, ch = W, H + 70
    im = Image.new("RGB", (cw * 4, ch * 2), "white")
    d = ImageDraw.Draw(im)
    for i, (name, big, model) in enumerate(CHART_ROWS):
        x, y = (i % 4) * cw, (i // 4) * ch
        im.paste(cards[name], (x, y))
        f = font(26)
        d.text((x + (cw - d.textlength(big, font=f)) / 2, y + H - 2), big, font=f, fill=(32, 33, 36))
        f = font(17, False)
        d.text((x + (cw - d.textlength(model, font=f)) / 2, y + H + 32), model, font=f, fill=(95, 99, 104))
    im.thumbnail((1400, 1000), Image.LANCZOS)
    im.save(CHART, optimize=True)


if __name__ == "__main__":
    os.makedirs(OUT, exist_ok=True)
    cards = {name: card(name, url, badge) for name, (url, badge) in PHOTOS.items()}
    chart(cards)
    print("wrote", os.path.normpath(OUT), "and", os.path.normpath(CHART))
