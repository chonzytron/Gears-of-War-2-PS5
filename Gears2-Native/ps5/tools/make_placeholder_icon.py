#!/usr/bin/env python3
# Gears of War 2 for PS5 - a plain placeholder home screen icon (sce_sys/icon0.png,
# 512x512): a generic gear drawn from circles and rectangles, and "PS5 PORT".
# No game artwork or logo. Replace package/sce_sys/icon0.png with your own
# picture (512x512 PNG) for a release.
#   python3 ps5/tools/make_placeholder_icon.py ps5/package/sce_sys/icon0.png
import math
import sys

from PIL import Image, ImageDraw, ImageFont

size = 512
img = Image.new("RGB", (size, size), (24, 26, 30))
d = ImageDraw.Draw(img)
cx, cy = size // 2, size // 2 - 30
teeth, r_out, r_in, tooth_w = 10, 170, 135, 46
steel = (150, 156, 164)
for i in range(teeth):
    a = 2 * math.pi * i / teeth
    dx, dy = math.cos(a), math.sin(a)
    px, py = -dy, dx
    pts = []
    for along, side in ((r_in - 10, -1), (r_out, -0.7), (r_out, 0.7), (r_in - 10, 1)):
        pts.append((cx + dx * along + px * side * tooth_w / 2, cy + dy * along + py * side * tooth_w / 2))
    d.polygon(pts, fill=steel)
d.ellipse((cx - r_in, cy - r_in, cx + r_in, cy + r_in), fill=steel)
d.ellipse((cx - 70, cy - 70, cx + 70, cy + 70), fill=(24, 26, 30))
d.ellipse((cx - 40, cy - 40, cx + 40, cy + 40), fill=(150, 30, 30))
try:
    font = ImageFont.truetype(sys.argv[2] if len(sys.argv) > 2 else "DejaVuSans-Bold.ttf", 48)
except OSError:
    font = ImageFont.load_default()
text = "PS5 PORT"
w = d.textlength(text, font=font)
d.text(((size - w) / 2, size - 92), text, fill=(220, 222, 226), font=font)
img.save(sys.argv[1] if len(sys.argv) > 1 else "icon0.png")
