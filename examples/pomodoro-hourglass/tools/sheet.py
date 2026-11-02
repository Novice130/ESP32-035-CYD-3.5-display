#!/usr/bin/env python3
"""Contact sheet of every .ppm in a folder -> folder/sheet.png (and each frame as .png)."""
import glob, os, sys
from PIL import Image
d = sys.argv[1]
cols = int(sys.argv[2]) if len(sys.argv) > 2 else 2
files = sorted(glob.glob(os.path.join(d, "*.ppm")), key=os.path.getmtime)
ims = [Image.open(f).convert("RGB") for f in files]
if ims:
    w, h = ims[0].size
    rows = (len(ims) + cols - 1) // cols
    sheet = Image.new("RGB", (cols * (w + 6), rows * (h + 6)), (64, 64, 64))
    for i, im in enumerate(ims):
        sheet.paste(im, ((i % cols) * (w + 6), (i // cols) * (h + 6)))
        im.save(os.path.splitext(files[i])[0] + ".png")
    sheet.save(os.path.join(d, "sheet.png"))
    print("sheet", sheet.size, [os.path.basename(f) for f in files])
