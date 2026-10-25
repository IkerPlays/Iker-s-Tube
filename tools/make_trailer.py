#!/usr/bin/env python3
"""Builds the YouTube trailer of Iker's Tube (trailer/IkersTube-trailer.mp4, 1920x1080, 30 fps, about 48 s) with Pillow, numpy and ffmpeg.
The screens shown are the renders of tools/ui_preview (the real view classes drawn on the PC with sample data), not footage from a console.
The music is synthesized here (an original arpeggio loop), so there is nothing to license.
Run: python3 tools/ui_preview/... first (./build.sh && ./run.sh) so that tools/ui_preview/out/*.png exist, then: python3 tools/make_trailer.py [--preview]"""
import math, os, subprocess, sys, wave
import numpy as np
from PIL import Image, ImageDraw, ImageFilter, ImageFont

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
OUT = os.path.join(ROOT, "trailer")
W, H, FPS = 1920, 1080, 30
PREVIEW = "--preview" in sys.argv          # a small, fast version to look at the layout
SCALE = 0.5 if PREVIEW else 1.0
ACCENT = (255, 74, 74)
FONT_DIR = "/usr/share/fonts/opentype/inter/"

def font(name, size):
    return ImageFont.truetype(FONT_DIR + name, int(size))
F_BIG, F_MED, F_SMALL, F_HUGE = font("InterDisplay-ExtraBold.otf", 74), font("Inter-Medium.otf", 40), font("Inter-Medium.otf", 26), font("InterDisplay-Black.otf", 150)

def clamp(x, a=0.0, b=1.0): return max(a, min(b, x))
def ease_out(x): x = clamp(x); return 1 - (1 - x) ** 3
def ease_in_out(x): x = clamp(x); return 3 * x * x - 2 * x ** 3
def back_out(x):
    x = clamp(x); c = 1.70158
    return 1 + (c + 1) * (x - 1) ** 3 + c * (x - 1) ** 2

# ---------- assets ----------
def load(path, size=None):
    im = Image.open(os.path.join(ROOT, path)).convert("RGBA")
    return im.resize(size, Image.LANCZOS) if size else im
SHOTS = {k: load("tools/ui_preview/out/%s.png" % k, (480, 360)) for k in
         ["home_dark", "playback_dark", "home_light", "home_amber", "set_es_dark", "info_dark", "menu_dark"]}
LOGO = load("web/img/icon-512.png")
QR = load("web/img/qr-latest.png", (380, 380))

# (start s, end s, screen, title, subtitle)
SCENES = [
    (6.0, 10.5, "home_dark", "Search. Watch. Sign in.", "YouTube on your Nintendo 3DS, 2DS and New 3DS."),
    (10.5, 15.0, "playback_dark", "Pick your quality", "From 144p to 480p, or audio only."),
    (15.0, 19.5, "home_light", "Light and dark", "A fresh rounded interface."),
    (19.5, 24.0, "home_amber", "Seven colour themes", "Ruby, Ocean, Forest, Violet, Amber, AMOLED, your own."),
    (24.0, 28.5, "set_es_dark", "In your language", "English, Español, Français, Deutsch, Italiano, Português and Japanese."),
    (28.5, 33.0, "info_dark", "Made for playback", "Speed, chapters, sleep timer, queue, SponsorBlock."),
    (33.0, 38.0, "menu_dark", "Yours to keep", "Favorites, watch later, offline downloads, backups."),
]
INTRO_END, OUTRO_START, TOTAL = 6.0, 38.0, 48.0

# ---------- drawing helpers ----------
_bg_cache = {}
def background(t):
    key = int(t * 4)
    if key in _bg_cache: return _bg_cache[key].copy()
    yy, xx = np.mgrid[0:H, 0:W].astype(np.float32)
    cx, cy = W * (0.30 + 0.12 * math.sin(t * 0.4)), H * (0.40 + 0.10 * math.cos(t * 0.33))
    d = np.sqrt(((xx - cx) / W) ** 2 + ((yy - cy) / H) ** 2)
    glow = np.clip(1 - d * 1.5, 0, 1) ** 2
    base = np.array([14, 15, 20], np.float32)
    img = np.zeros((H, W, 3), np.float32) + base
    img += glow[..., None] * np.array([70, 22, 24], np.float32)
    img += (yy / H)[..., None] * np.array([4, 4, 8], np.float32)
    out = Image.fromarray(np.clip(img, 0, 255).astype(np.uint8), "RGB").convert("RGBA")
    _bg_cache[key] = out
    return out.copy()

def wrap(text, f, max_width):
    """splits a text into lines that fit max_width pixels."""
    words, lines, cur = text.split(" "), [], ""
    probe = ImageDraw.Draw(Image.new("RGBA", (10, 10)))
    for w in words:
        trial = (cur + " " + w).strip()
        if probe.textlength(trial, font=f) <= max_width or not cur: cur = trial
        else: lines.append(cur); cur = w
    if cur: lines.append(cur)
    return lines

def text_layer(size, lines, x, y, alpha=1.0, dx=0):
    """lines : [(text, font, rgb)] drawn one under the other with a soft shadow."""
    layer = Image.new("RGBA", size, (0, 0, 0, 0))
    d = ImageDraw.Draw(layer)
    cy = y
    for text, f, rgb in lines:
        d.text((x + dx + 3, cy + 4), text, font=f, fill=(0, 0, 0, int(120 * alpha)))
        d.text((x + dx, cy), text, font=f, fill=rgb + (int(255 * alpha),))
        cy += f.size * 1.25
    return layer

def rounded(size, radius, fill):
    im = Image.new("RGBA", size, (0, 0, 0, 0))
    ImageDraw.Draw(im).rounded_rectangle((0, 0, size[0] - 1, size[1] - 1), radius=radius, fill=fill)
    return im

def top_screen(t, size=(600, 360)):
    """a moving, abstract 'video' with a play bar (the top screen of the console)."""
    w, h = size
    yy, xx = np.mgrid[0:h, 0:w].astype(np.float32)
    img = np.zeros((h, w, 3), np.float32)
    for i, (r, g, b) in enumerate([(255, 74, 74), (255, 160, 48), (80, 140, 255)]):
        cx = w * (0.5 + 0.38 * math.sin(t * (0.7 + 0.25 * i) + i * 2.1))
        cy = h * (0.5 + 0.30 * math.cos(t * (0.5 + 0.2 * i) + i))
        d = np.sqrt((xx - cx) ** 2 + (yy - cy) ** 2) / (w * 0.42)
        img += np.clip(1 - d, 0, 1)[..., None] ** 1.6 * np.array([r, g, b], np.float32) * 0.7
    img = np.clip(img * 0.8 + 18, 0, 255)
    im = Image.fromarray(img.astype(np.uint8), "RGB").convert("RGBA")
    d = ImageDraw.Draw(im)
    d.rectangle((0, h - 54, w, h), fill=(0, 0, 0, 150))
    d.ellipse((14, h - 42, 42, h - 14), fill=ACCENT + (255,))
    d.polygon([(23, h - 36), (23, h - 20), (36, h - 28)], fill=(255, 255, 255, 255))
    d.rounded_rectangle((60, h - 31, w - 24, h - 25), radius=3, fill=(255, 255, 255, 70))
    prog = (t * 0.045) % 1.0
    d.rounded_rectangle((60, h - 31, 60 + (w - 84) * prog, h - 25), radius=3, fill=ACCENT + (255,))
    return im

def device(t, screen_key, prev_key, blend, tap):
    """the console : a dark body, the top screen and the bottom screen with the app."""
    dw, dh = 760, 880
    body = Image.new("RGBA", (dw, dh), (0, 0, 0, 0))
    bd = ImageDraw.Draw(body)
    bd.rounded_rectangle((0, 0, dw - 1, dh - 1), radius=46, fill=(30, 33, 42, 255), outline=(58, 62, 76, 255), width=3)
    bd.rounded_rectangle((12, 12, dw - 13, 420), radius=36, fill=(22, 24, 31, 255))        # upper shell
    bd.rounded_rectangle((12, 440, dw - 13, dh - 13), radius=36, fill=(22, 24, 31, 255))   # lower shell
    bd.rounded_rectangle((60, 438, dw - 61, 446), radius=4, fill=(12, 13, 18, 255))        # hinge
    body.alpha_composite(top_screen(t), (80, 36))
    # the bottom screen : the render of the app, crossfaded
    shot = SHOTS[screen_key].copy()
    if prev_key and blend < 1:
        shot = Image.blend(SHOTS[prev_key], shot, ease_in_out(blend))
    if tap:
        sd = ImageDraw.Draw(shot, "RGBA")
        px, py, p = tap
        r = 10 + 46 * p
        sd.ellipse((px - r, py - r, px + r, py + r), outline=(255, 255, 255, int(200 * (1 - p))), width=4)
    body.alpha_composite(shot, (140, 470))
    # a pad and buttons
    bd = ImageDraw.Draw(body)
    bd.rounded_rectangle((70, 560, 120, 580), radius=6, fill=(48, 52, 64, 255))
    bd.rounded_rectangle((85, 545, 105, 595), radius=6, fill=(48, 52, 64, 255))
    for i, (bx, by) in enumerate([(650, 540), (690, 575), (650, 610), (610, 575)]):
        bd.ellipse((bx - 14, by - 14, bx + 14, by + 14), fill=(48, 52, 64, 255))
    return body

def caption_layer(idx, local, total):
    """title and subtitle of the scene, sliding in and fading out."""
    _, _, _, title, sub = SCENES[idx]
    a_in, a_out = ease_out(local / 0.6), 1 - ease_in_out((local - (total - 0.45)) / 0.45)
    alpha = clamp(min(a_in, a_out))
    layer = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    d = ImageDraw.Draw(layer)
    x, y = 130, 360 + (1 - a_in) * 50
    d.rounded_rectangle((x, y + 6, x + 12, y + 84), radius=6, fill=ACCENT + (int(255 * alpha),))
    layer.alpha_composite(text_layer((W, H), [(title, F_BIG, (255, 255, 255))], x + 40, y, alpha))
    sub_font = font("Inter-Medium.otf", 36)
    layer.alpha_composite(text_layer((W, H), [(line, sub_font, (200, 204, 216)) for line in wrap(sub, sub_font, 820)], x + 40, y + 100, alpha * 0.95))
    # scene dots
    for i in range(len(SCENES)):
        on = i == idx
        d.ellipse((x + 40 + i * 34, 820, x + 40 + i * 34 + 14, 834), fill=(ACCENT if on else (90, 94, 108)) + (int(255 * (1 if on else 0.8) * clamp(a_in)),))
    return layer

def render(t):
    img = background(t)
    if t < INTRO_END:                                   # logo intro
        s = back_out((t - 0.3) / 1.2)
        size = int(300 * clamp(s, 0, 1.15))
        if size > 8:
            logo = LOGO.resize((size, size), Image.LANCZOS)
            pad = 120
            padded = Image.new("RGBA", (size + 2 * pad, size + 2 * pad), (0, 0, 0, 0)); padded.alpha_composite(logo, (pad, pad))
            glow = padded.filter(ImageFilter.GaussianBlur(40))
            img.alpha_composite(glow, (W // 2 - size // 2 - pad, 250 - size // 2 + 60 - pad))
            img.alpha_composite(logo, (W // 2 - size // 2, 250 - size // 2 + 60))
        a = ease_out((t - 1.1) / 0.8)
        l1 = text_layer((W, H), [("Iker's Tube", F_HUGE, (255, 255, 255))], 0, 0, a)
        bbox = ImageDraw.Draw(l1).textbbox((0, 0), "Iker's Tube", font=F_HUGE)
        img.alpha_composite(text_layer((W, H), [("Iker's Tube", F_HUGE, (255, 255, 255))], (W - (bbox[2] - bbox[0])) // 2, 440 + (1 - a) * 40, a))
        a2 = ease_out((t - 2.2) / 0.8)
        tag = "YouTube for your Nintendo 3DS"
        tb = ImageDraw.Draw(l1).textbbox((0, 0), tag, font=F_MED)
        img.alpha_composite(text_layer((W, H), [(tag, F_MED, (255, 140, 140))], (W - (tb[2] - tb[0])) // 2, 640, a2))
        fade = clamp((t - (INTRO_END - 0.6)) / 0.6)
        if fade > 0: img.alpha_composite(Image.new("RGBA", (W, H), (14, 15, 20, int(255 * fade))))
    elif t < OUTRO_START + 0.4 or True:
        dev_in = ease_out((t - 5.4) / 1.0) if t < OUTRO_START else 1 - ease_in_out((t - OUTRO_START) / 0.8)
        # which scene
        idx = 0
        for i, (a, b, *_rest) in enumerate(SCENES):
            if a <= t: idx = i
        a, b = SCENES[idx][0], SCENES[idx][1]
        local = clamp(t - a, 0, 99)
        prev = SCENES[idx - 1][2] if idx > 0 else None
        blend = clamp(local / 0.6) if idx > 0 else 1
        tap = None
        if 1.2 < local < 2.0: tap = (90 + (idx * 53) % 260, 130 + (idx * 71) % 140, (local - 1.2) / 0.8)
        if dev_in > 0.01:
            dev = device(t, SCENES[min(idx, len(SCENES) - 1)][2], prev, blend, tap)
            x = int(W - 880 + (1 - dev_in) * 900)
            y = int(100 + 8 * math.sin(t * 0.9))
            shadow = Image.new("RGBA", (dev.width + 120, dev.height + 120), (0, 0, 0, 0))
            shadow.paste((0, 0, 0, 150), (60, 80, dev.width + 60, dev.height + 80))
            shadow = shadow.filter(ImageFilter.GaussianBlur(36))
            img.alpha_composite(shadow, (x - 60, y - 40))
            img.alpha_composite(dev, (x, y))
        if t < OUTRO_START:
            img.alpha_composite(caption_layer(idx, local, b - a))
            img.alpha_composite(text_layer((W, H), [("Interface preview", F_SMALL, (150, 154, 168))], 130, 980, 0.8))
        else:                                           # outro
            o = t - OUTRO_START
            a = ease_out(o / 0.8)
            card = rounded((440, 440), 36, (255, 255, 255, 255))
            card.alpha_composite(QR, (30, 30))
            img.alpha_composite(card, (int(W - 560 + (1 - a) * 60), 330), ) if False else None
            xq = int(1280 + (1 - a) * 80)
            layer = Image.new("RGBA", (W, H), (0, 0, 0, 0))
            layer.alpha_composite(card, (xq, 330))
            layer.putalpha(layer.getchannel("A").point(lambda v: int(v * a)))
            img.alpha_composite(layer)
            img.alpha_composite(text_layer((W, H), [("Free. Open source.", F_BIG, (255, 255, 255))], 130, 300, a, (1 - a) * -60))
            ao = ease_out((o - 0.4) / 0.8)
            img.alpha_composite(text_layer((W, H), [("Download it for your 3DS", F_MED, (255, 140, 140))], 130, 450, ao))
            img.alpha_composite(text_layer((W, H), [("github.com/IkerPlays/Iker-s-Tube", F_MED, (200, 204, 216))], 130, 520, ao))
            img.alpha_composite(text_layer((W, H), [("Scan with FBI: Remote Install > Scan QR code", F_SMALL, (150, 154, 168))], 130, 600, ao))
    # global fade in at the start and out at the end
    f_out = clamp((t - (TOTAL - 1.2)) / 1.2)
    if f_out > 0: img.alpha_composite(Image.new("RGBA", (W, H), (14, 15, 20, int(255 * f_out))))
    return img.convert("RGB")

# ---------- music ----------
SR = 44100
def synth(duration):
    n = int(SR * duration)
    out = np.zeros(n, np.float32)
    bpm = 112.0; beat = 60.0 / bpm
    chords = [(57, [57, 60, 64]), (53, [53, 57, 60]), (48, [48, 52, 55]), (55, [55, 59, 62])]   # Am F C G
    f = lambda m: 440.0 * 2 ** ((m - 69) / 12)
    def add(start, dur, freq, amp, kind):
        i0 = int(start * SR); i1 = min(n, int((start + dur) * SR))
        if i1 <= i0: return
        t = np.arange(i1 - i0) / SR
        if kind == "tri": w = 2 / np.pi * np.arcsin(np.sin(2 * np.pi * freq * t))
        elif kind == "sq": w = np.sign(np.sin(2 * np.pi * freq * t)) * 0.5 + 0.5 * np.sin(2 * np.pi * freq * t)
        else: w = np.sin(2 * np.pi * freq * t)
        env = np.minimum(1, t / 0.006) * np.exp(-t * (6 if kind != "sin" else 3)) 
        out[i0:i1] += (w * env * amp).astype(np.float32)
    bar = 4 * beat
    nbars = int(duration / bar) + 1
    rng = np.random.default_rng(3)
    for b in range(nbars):
        root, notes = chords[b % 4]
        t0 = b * bar
        lead_on = t0 > 6.0
        for k in range(8):                                      # arpeggio in eighths
            m = notes[[0, 1, 2, 1, 0, 1, 2, 1][k]] + 12
            add(t0 + k * beat / 2, beat * 0.9, f(m), 0.10, "tri")
            if lead_on and k % 2 == 0:
                add(t0 + k * beat / 2, beat * 0.45, f(m + 12), 0.05, "sq")
        add(t0, bar * 0.98, f(root - 12), 0.18, "sin")          # bass
        add(t0 + 2 * beat, beat * 0.9, f(root - 12), 0.12, "sin")
        for k in range(4):                                      # kick
            if t0 > 4.0:
                i0 = int((t0 + k * beat) * SR); ln = int(0.18 * SR)
                if i0 + ln < n:
                    tt = np.arange(ln) / SR
                    out[i0:i0 + ln] += (np.sin(2 * np.pi * (90 * np.exp(-tt * 18) + 40) * tt) * np.exp(-tt * 14) * 0.35).astype(np.float32)
        for k in range(8):                                      # hats
            if t0 > 8.0 and k % 2 == 1:
                i0 = int((t0 + k * beat / 2) * SR); ln = int(0.05 * SR)
                if i0 + ln < n:
                    out[i0:i0 + ln] += (rng.standard_normal(ln) * np.exp(-np.arange(ln) / SR * 80) * 0.05).astype(np.float32)
    # a little echo
    d1 = int(0.27 * SR); echo = np.zeros_like(out); echo[d1:] = out[:-d1] * 0.28; out = out + echo
    out /= max(1e-6, np.abs(out).max()) / 0.85
    fade_in = np.minimum(1, np.arange(n) / (SR * 1.0)); fade_out = np.minimum(1, (n - np.arange(n)) / (SR * 2.5))
    return (out * fade_in * fade_out).astype(np.float32)

def write_wav(path, data):
    with wave.open(path, "wb") as w:
        w.setnchannels(1); w.setsampwidth(2); w.setframerate(SR)
        w.writeframes((np.clip(data, -1, 1) * 32767).astype(np.int16).tobytes())

def main():
    os.makedirs(OUT, exist_ok=True)
    wav = os.path.join(OUT, "music.wav")
    write_wav(wav, synth(TOTAL))
    if "--still" in sys.argv:                                    # python3 tools/make_trailer.py --still 12.0
        t = float(sys.argv[sys.argv.index("--still") + 1])
        render(t).save(os.path.join(OUT, "still.png")); return
    out = os.path.join(OUT, "IkersTube-trailer%s.mp4" % ("-preview" if PREVIEW else ""))
    size = (int(W * SCALE), int(H * SCALE))
    cmd = ["ffmpeg", "-y", "-loglevel", "error", "-f", "rawvideo", "-pix_fmt", "rgb24", "-s", "%dx%d" % size, "-r", str(FPS), "-i", "-",
           "-i", wav, "-c:v", "libx264", "-preset", "medium", "-crf", "18", "-pix_fmt", "yuv420p", "-c:a", "aac", "-b:a", "192k", "-shortest", "-movflags", "+faststart", out]
    p = subprocess.Popen(cmd, stdin=subprocess.PIPE)
    frames = int(TOTAL * FPS)
    for i in range(frames):
        im = render(i / FPS)
        if PREVIEW: im = im.resize(size, Image.BILINEAR)
        p.stdin.write(im.tobytes())
        if i % 150 == 0: print("frame", i, "/", frames, flush=True)
    p.stdin.close(); p.wait()
    print("written", out)

if __name__ == "__main__":
    main()
