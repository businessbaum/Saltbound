"""The first islands' plan as a chart (Docs/Design/Islands.md): the Inner Waters with Rowan's trail, and the whole sea with the Gale.

    python Tools/design/islands_map.py            -> Docs/Design/Islands_Map.png

Positions are metres from the cove, north up (x north, y east, as the game: bearing = atan2(y, x)). The home island's coast comes
from island_shape.py (the same numbers the meshes are built from); the planned islands are drawn as their outlines in the plan.
"""
import math
import os
import sys

import numpy as np
from PIL import Image, ImageDraw, ImageFilter, ImageFont

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
sys.path.insert(0, os.path.join(ROOT, "Art", "Blender", "Scripts"))
import island_shape as S  # noqa: E402

FONT = os.path.join(ROOT, "Art", "Fonts", "IMFellEnglish", "IMFeENrm28P.ttf")
FONT_I = os.path.join(ROOT, "Art", "Fonts", "IMFellEnglish", "IMFeENit28P.ttf")
OUT = os.path.join(ROOT, "Docs", "Design", "Islands_Map.png")

PAPER = (233, 220, 189)
SEA = (196, 205, 196)
INK = (52, 36, 22)
FADED = (120, 98, 72)
TURQ = (40, 170, 160)
RED = (150, 40, 28)

# The places (north m, east m, radius m or None for a point, label, kind).
EXISTING = [((560, 260), 18, "The Spire", "isle"), ((1600, 600), 55, "Ruin Isle", "isle")]
PLANNED = [((900, 520), 22, "Rowan's camp", "camp"), ((-700, -900), 30, "The weather mast", "mast")]
HOLLOWKEEL = (0, 420)
OUTER = [((5500, 2500), 150, "Mistholm"), ((7000, -1500), 130, "Gull Rock"), ((4200, -4800), 200, "Longshoal")]


def font(size, italic=False):
    return ImageFont.truetype(FONT_I if italic else FONT, size)


def paper(w, h, seed):
    rng = np.random.default_rng(seed)
    base = np.ones((h, w, 3)) * np.array(PAPER, float)
    n = rng.normal(0, 1, (h // 8 + 2, w // 8 + 2))
    n = np.array(Image.fromarray(((n - n.min()) / np.ptp(n) * 255).astype(np.uint8)).resize((w, h), Image.BICUBIC), float) / 255
    base *= (0.94 + 0.08 * n)[..., None]
    return Image.fromarray(np.clip(base, 0, 255).astype(np.uint8))


class Chart:
    def __init__(self, img, box, half, centre=(0.0, 0.0)):
        self.img, self.d, self.box, self.half, self.centre = img, ImageDraw.Draw(img), box, half, centre

    def px(self, n, e):
        x0, y0, x1, y1 = self.box
        u = (e - self.centre[1]) / (2 * self.half) + 0.5
        v = 0.5 - (n - self.centre[0]) / (2 * self.half)
        return x0 + u * (x1 - x0), y0 + v * (y1 - y0)

    def scale(self):
        return (self.box[2] - self.box[0]) / (2 * self.half)

    def blob(self, n, e, r, fill, outline=INK, seed=1, width=2):
        pts = []
        for k in range(48):
            a = k / 48 * math.tau
            rr = r * (1 + 0.18 * math.sin(3 * a + seed) + 0.08 * math.sin(7 * a + 2 * seed))
            pts.append(self.px(n + math.cos(a) * rr, e + math.sin(a) * rr))
        self.d.polygon(pts, fill=fill, outline=outline, width=width)

    def label(self, n, e, text, size=26, dx=12, dy=-14, italic=False, col=INK, anchor="ls"):
        x, y = self.px(n, e)
        self.d.text((x + dx, y + dy), text, fill=col, font=font(size, italic), anchor=anchor)

    def dashed(self, pts, col=RED, width=4, dash=14, gap=10):
        for (a, b) in zip(pts, pts[1:]):
            (x0, y0), (x1, y1) = self.px(*a), self.px(*b)
            L = math.hypot(x1 - x0, y1 - y0)
            t = 0.0
            while t < L:
                t1 = min(t + dash, L)
                self.d.line([(x0 + (x1 - x0) * t / L, y0 + (y1 - y0) * t / L), (x0 + (x1 - x0) * t1 / L, y0 + (y1 - y0) * t1 / L)], fill=col, width=width)
                t += dash + gap

    def number(self, n, e, k):
        x, y = self.px(n, e)
        self.d.ellipse([x - 17, y - 17, x + 17, y + 17], fill=PAPER, outline=RED, width=3)
        self.d.text((x, y + 1), str(k), fill=RED, font=font(24), anchor="mm")


def home_island(chart):
    """The home island's land from island_shape (height above the sea), drawn as ink-edged land."""
    x0, y0, x1, y1 = [int(v) for v in chart.box]
    cn, ce = S.CENTRE
    step = 4.0
    ns = np.arange(cn - S.HALF, cn + S.HALF, step)
    es = np.arange(ce - S.HALF, ce + S.HALF, step)
    land = np.zeros((len(ns), len(es)), bool)
    for i, n in enumerate(ns):
        for j, e in enumerate(es):
            land[i, j] = S.base(n, e) > 0.0
    for i, n in enumerate(ns):
        for j, e in enumerate(es):
            if land[i, j]:
                x, y = chart.px(n, e)
                s = max(1.0, step * chart.scale())
                chart.d.rectangle([x - s / 2, y - s / 2, x + s / 2, y + s / 2], fill=(205, 186, 140))
    # the coast: land cells next to sea
    for i in range(1, len(ns) - 1):
        for j in range(1, len(es) - 1):
            if land[i, j] and not (land[i - 1, j] and land[i + 1, j] and land[i, j - 1] and land[i, j + 1]):
                x, y = chart.px(ns[i], es[j])
                chart.d.ellipse([x - 1.2, y - 1.2, x + 1.2, y + 1.2], fill=INK)


def compass(d, x, y, r):
    for a, L in [(0, r), (90, r * .7), (180, r * .7), (270, r * .7)]:
        t = math.radians(a)
        tip = (x + math.sin(t) * L, y - math.cos(t) * L)
        side = [(x + math.sin(t + 1.57) * r * .14, y - math.cos(t + 1.57) * r * .14), (x - math.sin(t + 1.57) * r * .14, y + math.cos(t + 1.57) * r * .14)]
        d.polygon([tip, side[0], (x, y), side[1]], fill=INK if a == 0 else FADED)
    d.text((x, y - r - 16), "N", fill=INK, font=font(28), anchor="mm")


def inner(img, box):
    c = Chart(img, box, 2000.0, (200.0, 0.0))
    d = c.d
    d.rectangle(box, fill=SEA)
    for r, txt in [(2500, "")]:
        pass
    # depth hints: the reef rings round the planned islands and the Spire
    for (n, e), r, name, kind in EXISTING + PLANNED:
        for k, rr in enumerate([r * 3.2, r * 2.2]):
            c.blob(n, e, rr, None, outline=(150, 168, 160), seed=k + 3, width=2)
    home_island(c)
    for (n, e), r, name, kind in EXISTING:
        c.blob(n, e, r, (205, 186, 140), seed=2)
    c.blob(900, 520, 22, (214, 196, 150), seed=5)
    c.blob(-700, -900, 30, (180, 176, 160), seed=7)
    # the mast on its reef: a leaning lattice glyph with a turquoise head
    x, y = c.px(-700, -900)
    d.line([(x - 4, y + 18), (x + 10, y - 30)], fill=INK, width=4)
    d.line([(x + 6, y + 18), (x + 10, y - 30)], fill=INK, width=3)
    d.ellipse([x + 4, y - 38, x + 16, y - 26], fill=TURQ, outline=INK)
    # the camp: a small lean-to and a fire
    x, y = c.px(900, 520)
    d.polygon([(x - 10, y + 6), (x + 2, y - 10), (x + 10, y + 6)], outline=INK, fill=(230, 222, 200))
    d.ellipse([x + 12, y - 2, x + 18, y + 4], fill=RED)
    # the Spire's beacon
    x, y = c.px(560, 260)
    d.ellipse([x - 8, y - 8, x + 8, y + 8], outline=TURQ, width=4)
    # the Hollowkeel at her berth
    x, y = c.px(*HOLLOWKEEL)
    d.rounded_rectangle([x - 26, y - 9, x + 26, y + 9], radius=9, fill=(150, 112, 70), outline=INK, width=2)
    c.label(HOLLOWKEEL[0], HOLLOWKEEL[1], "the Hollowkeel (hidden at first)", 20, dx=32, dy=6, italic=True, col=FADED)
    # the trail
    trail = [(-62, -104), (-20, 10), (560, 260), (900, 520), (-700, -900), (-1900, -700)]
    c.dashed(trail[1:])
    for k, (n, e) in enumerate([(-20, 10), (560, 260), (900, 520), (-700, -900)], 1):
        c.number(n, e, k)
    # labels
    c.label(-20, 10, "Saltbound Cove", 28, dx=-70, dy=-20, anchor="rs")
    c.label(-62, -104, "the wreck: you wake here", 20, dx=-46, dy=34, italic=True, col=FADED, anchor="rs")
    c.label(560, 260, "The Spire: the beacon, 'ERTH 17'", 26, dx=26, dy=-10)
    c.label(900, 520, "Rowan's camp", 26, dx=26, dy=-8)
    c.label(900, 520, "the chart, the telescope, the capsule", 20, dx=26, dy=18, italic=True, col=FADED)
    c.label(-700, -900, "The weather mast (a Gale relay)", 26, dx=24, dy=-6)
    c.label(-700, -900, "the Grey first seen here", 20, dx=24, dy=20, italic=True, col=FADED)
    c.label(1600, 600, "Ruin Isle: the shard (hour 2)", 24, dx=-20, dy=-70, anchor="rs")
    x, y = c.px(-1900, -700)
    d.text((x + 6, y - 6), "south, to the station and the storm's eye", fill=RED, font=font(22, True), anchor="lb")
    # the storm side: a breakout drifting in from the south-west
    for k in range(5):
        r = 120 + k * 70
        x, y = c.px(-1300, -1450)
        s = r * c.scale()
        d.arc([x - s, y - s, x + s, y + s], 200 + k * 30, 330 + k * 30, fill=(80, 90, 100), width=3)
    c.label(-1300, -1450, "a breakout drifting in", 20, dx=-70, dy=-100, italic=True, col=(70, 80, 90))
    # scale bar
    x0, y0, x1, y1 = box
    L = 500 * c.scale()
    d.line([(x1 - 60 - L, y1 - 40), (x1 - 60, y1 - 40)], fill=INK, width=4)
    d.text((x1 - 60 - L / 2, y1 - 52), "500 m  (2 minutes at 8 knots)", fill=INK, font=font(20), anchor="ms")
    compass(d, x1 - 70, y0 + 90, 46)
    d.text((x0 + 30, y0 + 24), "The Inner Waters: Rowan's trail", fill=INK, font=font(38), anchor="la")
    d.rectangle(box, outline=INK, width=3)


def whole(img, box):
    c = Chart(img, box, 11500.0)
    d = c.d
    d.rectangle(box, fill=SEA)
    # the Gale: a dark band from 9 to 10.5 km, ragged
    x0, y0, x1, y1 = box
    w, h = x1 - x0, y1 - y0
    band = Image.new("L", (w, h), 0)
    bd = ImageDraw.Draw(band)
    for k in range(720):
        a = k / 720 * math.tau
        for r in np.linspace(9000, 10500, 8):
            rr = r + 400 * math.sin(5 * a) * math.sin(3 * a + 1) + 250 * math.sin(13 * a)
            px, py = c.px(math.cos(a) * rr, math.sin(a) * rr)
            bd.ellipse([px - x0 - 9, py - y0 - 9, px - x0 + 9, py - y0 + 9], fill=200)
    band = band.filter(ImageFilter.GaussianBlur(6))
    dark = Image.new("RGB", (w, h), (58, 64, 74))
    img.paste(dark, (x0, y0), band)
    # lightning in the wall
    rng = np.random.default_rng(3)
    for _ in range(14):
        a = rng.uniform(0, math.tau)
        r = rng.uniform(9300, 10300)
        n, e = math.cos(a) * r, math.sin(a) * r
        pts = [c.px(n, e)]
        for _ in range(4):
            px, py = pts[-1]
            pts.append((px + rng.uniform(-8, 8), py + rng.uniform(6, 14)))
        d.line(pts, fill=(250, 240, 180), width=2)
    # breakouts torn loose
    for (n, e) in [(-6500, -2500), (-4800, 3500), (3000, -7000)]:
        for k in range(4):
            s = (300 + k * 260) * c.scale()
            x, y = c.px(n, e)
            d.arc([x - s, y - s, x + s, y + s], 30 + k * 40, 220 + k * 40, fill=(70, 78, 90), width=3)
    c.label(-6500, -2500, "breakouts", 22, dx=-40, dy=50, italic=True, col=(50, 56, 66))
    # rings
    for r, txt in [(2500, "the Inner Waters"), (6000, "the Outer Isles")]:
        s = r * c.scale()
        x, y = c.px(0, 0)
        d.ellipse([x - s, y - s, x + s, y + s], outline=FADED, width=2)
        d.text((x, y - s - 6), txt, fill=FADED, font=font(22, True), anchor="ms")
    # the storm waters and the station, south
    for k in range(3):
        c.blob(-4300, 600, 900 + k * 350, None, outline=(80, 88, 100), seed=k + 11, width=3)
    c.label(-4300, 600, "the Storm Waters, the station and the eye", 22, dx=-20, dy=-10, italic=True, col=(50, 56, 66), anchor="rs")
    # islands
    c.blob(95, 5, 260, (205, 186, 140), seed=1)
    c.label(0, 0, "Saltbound Cove", 24, dx=14, dy=34)
    for (n, e), r, name, kind in EXISTING + PLANNED:
        c.blob(n, e, max(r * 3, 120), (214, 196, 150), seed=3)
    for (n, e), r, name in OUTER:
        c.blob(n, e, r * 2.5, (205, 186, 140), seed=4)
        c.label(n, e, name, 24, dx=16, dy=-12)
    x, y = c.px(0, 10300)
    d.text((x - 20, y), "THE GALE", fill=(235, 232, 220), font=font(30), anchor="rm")
    d.text((x0 + 30, y0 + 24), "The whole sea: the Gale round it, about 9 km out", fill=INK, font=font(38), anchor="la")
    L = 2000 * c.scale()
    d.line([(x0 + 40, y1 - 40), (x0 + 40 + L, y1 - 40)], fill=INK, width=4)
    d.text((x0 + 40 + L / 2, y1 - 52), "2 km", fill=INK, font=font(20), anchor="ms")
    compass(d, x1 - 70, y0 + 90, 46)
    d.rectangle(box, outline=INK, width=3)


def main():
    W, H = 2200, 1140
    img = paper(W, H, 7)
    inner(img, (40, 40, 1100, 1100))
    whole(img, (1140, 40, 2160, 1060))
    img.save(OUT)
    print("SB_MAP", OUT, img.size)


if __name__ == "__main__":
    main()
