"""Draws the G-Major Reborn icon (knob with cyan arc) -> resources/icon.png + resources/icon.ico"""
import math, sys
from PIL import Image, ImageDraw
out = sys.argv[1]
S = 512
im = Image.new("RGBA", (S, S), (0, 0, 0, 0))
d = ImageDraw.Draw(im)
d.rounded_rectangle([16, 16, S - 16, S - 16], radius=96, fill=(44, 40, 98, 255))
c, r = S / 2, 170
d.arc([c - r - 20, c - r - 20, c + r + 20, c + r + 20], start=135, end=405, fill=(185, 188, 203, 255), width=26)
d.arc([c - r - 20, c - r - 20, c + r + 20, c + r + 20], start=135, end=330, fill=(52, 195, 230, 255), width=30)
for i in range(r, 0, -2):
    t = i / r
    v = int(20 + 90 * (1 - t) ** 0.6)
    d.ellipse([c - i, c - i - 6 * (1 - t), c + i, c + i - 6 * (1 - t)], fill=(v, v, v + 8, 255))
a = math.radians(330)
d.line([c + 30 * math.cos(a), c + 30 * math.sin(a), c + (r - 22) * math.cos(a), c + (r - 22) * math.sin(a)], fill=(255, 255, 255, 255), width=22)
im.save(out + "/icon.png")
im.save(out + "/icon.ico", sizes=[(16, 16), (24, 24), (32, 32), (48, 48), (64, 64), (128, 128), (256, 256)])
