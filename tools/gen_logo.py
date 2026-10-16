#!/usr/bin/env python3
"""Genera el icono (48x48) y el banner (256x128) de Iker's Tube. Uso: python3 tools/gen_logo.py [carpeta]"""
import sys
from PIL import Image, ImageDraw, ImageFont

out = sys.argv[1] if len(sys.argv) > 1 else "resource"
BG1, BG2 = (18, 18, 28), (40, 12, 22)
RED, WHITE = (230, 40, 50), (255, 255, 255)
BOLD = "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf"

def gradient(w, h):
    img = Image.new("RGB", (w, h))
    px = img.load()
    for y in range(h):
        t = y / max(1, h - 1)
        c = tuple(int(BG1[i] + (BG2[i] - BG1[i]) * t) for i in range(3))
        for x in range(w):
            px[x, y] = c
    return img

def play_button(d, x, y, w, h, r):
    d.rounded_rectangle([x, y, x + w, y + h], r, fill=RED)
    cx, cy = x + w / 2, y + h / 2
    s = h * 0.30
    d.polygon([(cx - s * 0.8, cy - s), (cx - s * 0.8, cy + s), (cx + s * 1.1, cy)], fill=WHITE)

def font(size):
    try:
        return ImageFont.truetype(BOLD, size)
    except OSError:
        return ImageFont.load_default()

# ---- icono 48x48 (se dibuja a 4x y se reduce para suavizar) ----
S = 4
ic = gradient(48 * S, 48 * S)
d = ImageDraw.Draw(ic)
d.rounded_rectangle([2 * S, 2 * S, 46 * S, 46 * S], 9 * S, outline=RED, width=2 * S)
play_button(d, 8 * S, 12 * S, 32 * S, 22 * S, 6 * S)
f = font(8 * S)
txt = "IKER"
tw = d.textlength(txt, font=f)
d.text(((48 * S - tw) / 2, 35 * S), txt, font=f, fill=WHITE)
ic.resize((48, 48), Image.LANCZOS).save(f"{out}/icon.png")

# ---- banner 256x128 ----
bn = gradient(256 * 2, 128 * 2)
d = ImageDraw.Draw(bn)
play_button(d, 24, 56, 120, 84, 22)
d.text((172, 40), "Iker's", font=font(54), fill=WHITE)
d.text((172, 100), "Tube", font=font(54), fill=RED)
d.text((24, 196), "YouTube para Nintendo 3DS", font=font(22), fill=(200, 200, 210))
bn.resize((256, 128), Image.LANCZOS).save(f"{out}/banner.png")
