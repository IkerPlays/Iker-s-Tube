#!/usr/bin/env python3
"""Genera icono (48x48), imagen de banner (256x128) y audio de banner (wav silencioso)."""
import sys, wave, os
from PIL import Image, ImageDraw

out = sys.argv[1] if len(sys.argv) > 1 else "meta"
os.makedirs(out, exist_ok=True)

def note(d, s, ox, oy, col):
    d.ellipse([ox, oy + 62*s, ox + 28*s, oy + 82*s], fill=col)
    d.rectangle([ox + 22*s, oy + 8*s, ox + 28*s, oy + 72*s], fill=col)
    d.polygon([(ox+28*s, oy+8*s), (ox+52*s, oy+22*s), (ox+52*s, oy+36*s), (ox+28*s, oy+22*s)], fill=col)

ic = Image.new("RGB", (48, 48), (20, 24, 48))
d = ImageDraw.Draw(ic)
d.rounded_rectangle([2, 2, 45, 45], 8, outline=(0, 200, 255), width=2)
note(d, 0.36, 10, 6, (255, 255, 255))
ic.save(f"{out}/icon.png")

bn = Image.new("RGB", (256, 128), (20, 24, 48))
d = ImageDraw.Draw(bn)
for i in range(0, 256, 16):
    d.rectangle([i, 128 - (i * 7 % 60) - 10, i + 10, 128], fill=(0, 140, 200))
note(d, 1.0, 20, 20, (255, 255, 255))
d.text((100, 48), "Iker's Music 3DS", fill=(255, 255, 255))
bn.save(f"{out}/banner.png")

w = wave.open(f"{out}/banner.wav", "wb")
w.setnchannels(2); w.setsampwidth(2); w.setframerate(22050)
w.writeframes(b"\x00\x00" * 2 * 22050)
w.close()
