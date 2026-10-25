#!/usr/bin/env python3
"""Builds the YouTube thumbnail of the trailer (trailer/thumbnail.png, 1280x720) from the same assets as tools/make_trailer.py.
Run: python3 tools/make_thumbnail.py [--lang es]"""
import os, sys
sys.argv = [a for a in sys.argv]
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import make_trailer as mt                      # (its assets are loaded on import; nothing is rendered)
from PIL import Image, ImageDraw, ImageFilter, ImageFont

W, H = 1280, 720
LANG = mt.LANG
LINES = {"en": ("YOUTUBE", "ON YOUR", "3DS", "Free and open source"),
         "es": ("YOUTUBE", "EN TU", "3DS", "Gratis y de código abierto")}[LANG]

def f(name, size): return ImageFont.truetype(mt.FONT_DIR + name, size)

def main():
    bg = mt.background(1.0).resize((W, H), Image.LANCZOS).convert("RGBA")
    # a stronger glow behind the console
    glow = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    ImageDraw.Draw(glow).ellipse((620, -40, 1330, 760), fill=(255, 74, 74, 90))
    bg.alpha_composite(glow.filter(ImageFilter.GaussianBlur(90)))
    dev = mt.device(2.0, "home_dark", None, 1, None)
    scale = 0.76
    dev = dev.resize((int(dev.width * scale), int(dev.height * scale)), Image.LANCZOS)
    x, y = W - dev.width - 20, (H - dev.height) // 2
    shadow = Image.new("RGBA", (dev.width + 120, dev.height + 120), (0, 0, 0, 0))
    shadow.paste((0, 0, 0, 190), (60, 80, dev.width + 60, dev.height + 80))
    bg.alpha_composite(shadow.filter(ImageFilter.GaussianBlur(30)), (x - 60, y - 40))
    bg.alpha_composite(dev, (x, y))
    d = ImageDraw.Draw(bg)
    big, huge = f("InterDisplay-Black.otf", 128), f("InterDisplay-Black.otf", 210)
    def shadowed(xy, text, font, fill):
        d.text((xy[0] + 5, xy[1] + 6), text, font=font, fill=(0, 0, 0, 170))
        d.text(xy, text, font=font, fill=fill)
    shadowed((56, 170), LINES[0], big, (255, 255, 255, 255))
    shadowed((56, 290), LINES[1], big, mt.ACCENT + (255,))
    shadowed((56, 380), LINES[2], huge, mt.ACCENT + (255,))
    # the app icon and a small line
    icon = mt.LOGO.resize((84, 84), Image.LANCZOS)
    bg.alpha_composite(icon, (60, 60))
    d.text((165, 78), "Iker's Tube", font=f("InterDisplay-ExtraBold.otf", 46), fill=(255, 255, 255, 255))
    d.text((62, 640), LINES[3], font=f("Inter-SemiBold.otf", 34), fill=(215, 218, 228, 255))
    out = os.path.join(mt.OUT, "thumbnail%s.png" % ("-es" if LANG == "es" else ""))
    os.makedirs(mt.OUT, exist_ok=True)
    bg.convert("RGB").save(out, optimize=True)
    print("written", out, os.path.getsize(out) // 1024, "KB")

if __name__ == "__main__":
    main()
