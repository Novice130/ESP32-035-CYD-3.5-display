#!/usr/bin/env python3
"""Render every creature sprite to a contact sheet PNG for art QA.

usage: python3 preview.py [out.png] [name ...]
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from PIL import Image  # noqa: E402
from artlib import to_image  # noqa: E402
import creatures  # noqa: E402

out = sys.argv[1] if len(sys.argv) > 1 else "preview.png"
names = sys.argv[2:] or list(creatures.ALL)
SCALE = 5
tiles = [to_image(creatures.ALL[n](), SCALE, bg=(200, 228, 200)) for n in names]
cols = min(4, len(tiles))
rows = (len(tiles) + cols - 1) // cols
sheet = Image.new("RGBA", (cols * 64 * SCALE + (cols - 1) * 8, rows * 64 * SCALE + (rows - 1) * 8), (40, 40, 40, 255))
for i, t in enumerate(tiles):
    sheet.paste(t, ((i % cols) * (64 * SCALE + 8), (i // cols) * (64 * SCALE + 8)))
sheet.save(out)
print("wrote", out)
