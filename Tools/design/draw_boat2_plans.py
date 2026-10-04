"""Shipwright's plan sheet for the second boat (design proposal D51): profile with the rig, deck plan, lower-deck plan
and a midship section with a 1.5 m figure, drawn in ink on parchment. A review drawing, not game content.

Run with the host Python (numpy + Pillow):  python Tools/design/draw_boat2_plans.py
Writes Docs/Design/Boat2_Plans.png. The hull formulas here are the proposal's; the game boat will get them in its own
hull shape (SaltHullShape-style) once the design is agreed.
"""
import math
import os
import sys

import numpy as np
from PIL import Image, ImageDraw, ImageFont

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
sys.path.insert(0, os.path.join(ROOT, "Tools", "textures"))
from gen_ui import parchment  # noqa: E402

OUT = os.path.join(ROOT, "Docs", "Design", "Boat2_Plans.png")
INK = (52, 34, 20, 255)
SOFT = (52, 34, 20, 110)
WATER = (60, 110, 130, 255)
FILL_DECK = (196, 160, 112, 255)
FILL_ROOM = (214, 190, 146, 255)

# --- the proposal (metres; x forward from midship, z up from the design waterline) -------------------------------
L2 = 7.0            # half length on deck (14 m)
B2 = 2.2            # half beam (4.4 m)
FREEBOARD = 1.0     # sheer midship
SHEER_RISE = 0.45   # more at the bow, a little less at the stern
DRAFT = 1.25        # canoe body midship
KEEL_AFT = (-5.0, -2.05)    # heel of the sternpost
FOREFOOT = (4.6, -1.15)
TRANSOM_X = -6.9
SOLE = -0.75        # cabin floor below deck
DECKHOUSE = (-2.7, 1.2, 0.75)    # aft end, fore end, height above the deck
QUARTERDECK = (-6.9, -3.6, 0.55)  # aft, fore, raised by
MAST_X, MAST_H = 1.6, 15.5
BOWSPRIT = 3.2
FIGURE = 1.5


def sheer(x):
    t = x / L2
    return FREEBOARD + SHEER_RISE * (t * t if x > 0 else 0.7 * t * t)


def half_beam(x):
    t = abs(x) / L2
    if x >= 0.0:
        return B2 * max(1.0 - t ** 2.3, 0.0) ** 0.62      # a fine bow
    # A full, wide quarter aft for the cabin, ending in a transom about 2.2 m wide.
    return B2 * max(1.0 - 0.55 * t ** 3.0, 0.0) ** 0.8 if x > TRANSOM_X - 0.01 else 0.0


def bottom(x):
    t = abs(x) / L2
    return -DRAFT * max(1.0 - t ** 2.2, 0.0) ** 0.7


def keel(x):
    if x > FOREFOOT[0]:
        return bottom(x)
    a = (x - KEEL_AFT[0]) / (FOREFOOT[0] - KEEL_AFT[0])
    return KEEL_AFT[1] + (FOREFOOT[1] - KEEL_AFT[1]) * min(max(a, 0.0), 1.0)


def stem_x(z):
    """The raked stem: at the sheer it stands 0.6 m further forward than at the waterline."""
    return L2 - 0.6 * (1.0 - min(max(z / sheer(L2), 0.0), 1.0)) - 0.25 * max(-z, 0.0)


def font(size, bold=False):
    for name in (("georgiab.ttf" if bold else "georgia.ttf"), "arial.ttf"):
        try:
            return ImageFont.truetype(os.path.join(os.environ.get("WINDIR", "C:/Windows"), "Fonts", name), size)
        except OSError:
            continue
    return ImageFont.load_default()


class View:
    """Maps metres to sheet pixels: origin (px) and scale (px per m); z / y up."""

    def __init__(self, d, ox, oy, s):
        self.d, self.ox, self.oy, self.s = d, ox, oy, s

    def p(self, x, z):
        return (self.ox + x * self.s, self.oy - z * self.s)

    def line(self, pts, fill=INK, width=3):
        self.d.line([self.p(*q) for q in pts], fill=fill, width=width, joint="curve")

    def poly(self, pts, fill=None, outline=INK, width=3):
        self.d.polygon([self.p(*q) for q in pts], fill=fill, outline=outline, width=width)

    def dashed(self, pts, fill=SOFT, width=2, dash=10):
        for a, b in zip(pts[:-1], pts[1:]):
            pa, pb = np.array(self.p(*a)), np.array(self.p(*b))
            n = max(int(np.linalg.norm(pb - pa) / dash), 1)
            for k in range(0, n, 2):
                self.d.line([tuple(pa + (pb - pa) * k / n), tuple(pa + (pb - pa) * min(k + 1, n) / n)], fill=fill, width=width)

    def text(self, x, z, s, size=22, anchor="mm", fill=INK, bold=False):
        self.d.text(self.p(x, z), s, font=font(size, bold), fill=fill, anchor=anchor)


def figure(v, x, z, h=FIGURE):
    """A 1.5 m person standing at (x, z) (feet)."""
    head = h * 0.13
    v.d.ellipse([v.p(x - head / 2, z + h), v.p(x + head / 2, z + h - head)], outline=INK, width=3)
    v.line([(x, z + h - head), (x, z + h * 0.45)], width=4)
    v.line([(x - 0.18, z + h * 0.7), (x, z + h * 0.78), (x + 0.18, z + h * 0.7)], width=3)
    v.line([(x - 0.15, z), (x, z + h * 0.45), (x + 0.15, z)], width=4)


def profile(v):
    xs = np.linspace(TRANSOM_X, L2, 160)
    deck = [(x, sheer(x)) for x in xs] + [(stem_x(sheer(L2)), sheer(L2))]
    under = [(stem_x(z), z) for z in np.linspace(sheer(L2), FOREFOOT[1], 12)]
    under += [(x, keel(x)) for x in np.linspace(FOREFOOT[0], KEEL_AFT[0], 40)]
    under += [(KEEL_AFT[0], KEEL_AFT[1]), (KEEL_AFT[0] - 0.4, -0.55), (TRANSOM_X + 0.3, 0.35), (TRANSOM_X, sheer(TRANSOM_X) - 0.95)]
    v.poly(deck + under + [(TRANSOM_X, sheer(TRANSOM_X))], fill=(150, 64, 46, 255))
    # The bulwark, the cream sheer stripe and the waterline.
    v.line([(x, sheer(x) + 0.45) for x in xs], width=3)
    v.line([(x, sheer(x) - 0.12) for x in xs], fill=(232, 214, 170, 255), width=6)
    v.line([(-8.5, 0.0), (10.5, 0.0)], fill=WATER, width=2)
    v.text(9.6, 0.25, "DWL", 18, fill=WATER)
    # Raised quarterdeck and the deckhouse.
    qa, qf, qh = QUARTERDECK
    v.poly([(qa, sheer(qa)), (qa, sheer(qa) + qh), (qf, sheer(qf) + qh), (qf, sheer(qf))], fill=FILL_DECK)
    da, df, dh = DECKHOUSE
    v.poly([(da, sheer(da)), (da, sheer(da) + dh), (df, sheer(df) + dh), (df, sheer(df))], fill=FILL_ROOM)
    for k in range(3):
        cx = da + 0.6 + k * 1.1
        v.d.ellipse([v.p(cx - 0.12, sheer(cx) + dh * 0.68), v.p(cx + 0.12, sheer(cx) + dh * 0.32)], outline=INK, width=3)
    # Inside, dashed: the cabin sole, bulkheads and the companionway ladder.
    v.dashed([(-5.6, SOLE), (5.2, SOLE)])
    for bx in (4.9, 1.9, -2.7, -4.6):
        v.dashed([(bx, SOLE), (bx, sheer(bx))])
    v.dashed([(da + 0.1, sheer(da)), (da + 0.8, SOLE)])
    v.text(3.4, -0.35, "hold", 18, fill=SOFT)
    v.text(-0.4, -0.35, "saloon", 18, fill=SOFT)
    v.text(-3.65, -0.35, "cabin", 18, fill=SOFT)
    figure(v, 0.3, SOLE)
    # Rig: mast, topmast, bowsprit, gaff mainsail, staysail and jib (outlines).
    deck_m = sheer(MAST_X)
    top = deck_m + MAST_H
    v.line([(MAST_X, deck_m), (MAST_X - 0.15, top)], width=6)
    v.line([(L2 - 0.2, sheer(L2) + 0.25), (L2 + BOWSPRIT, sheer(L2) + 0.45)], width=5)
    boom = [(MAST_X - 0.2, deck_m + 1.6), (TRANSOM_X - 0.6, deck_m + 2.4)]
    gaff = [(MAST_X - 0.25, deck_m + 10.0), (-4.6, deck_m + 13.4)]
    v.line(boom, width=5)
    v.line(gaff, width=4)
    v.poly([boom[0], boom[1], gaff[1], gaff[0]], fill=(238, 228, 204, 255), width=2)
    v.poly([(MAST_X + 0.1, deck_m + 11.8), (L2 - 0.1, sheer(L2) + 0.6), (MAST_X + 0.4, deck_m + 1.0)], fill=(238, 228, 204, 255), width=2)
    v.poly([(MAST_X + 0.0, deck_m + 13.6), (L2 + BOWSPRIT - 0.1, sheer(L2) + 0.6), (L2 + 0.9, sheer(L2) + 1.6)], fill=(238, 228, 204, 255), width=2)
    v.text(-1.6, deck_m + 7.0, "gaff main", 20)
    v.text(4.4, deck_m + 4.4, "staysail", 18)
    v.text(7.6, deck_m + 5.6, "jib", 18)
    # Wheel on the quarterdeck.
    wx = -5.2
    wz = sheer(wx) + qh + 0.8
    v.d.ellipse([v.p(wx - 0.38, wz + 0.38), v.p(wx + 0.38, wz - 0.38)], outline=INK, width=4)
    v.line([(wx, sheer(wx) + qh), (wx, wz)], width=4)


def plan(v, lower):
    xs = np.linspace(TRANSOM_X, L2, 160)
    shrink = 0.28 if lower else 0.0                 # inside the planking, at the sole the hull is narrower
    side = [(x, max(half_beam(x) * (0.86 if lower else 1.0) - shrink, 0.0)) for x in xs]
    outline = side + [(x, -y) for x, y in reversed(side)]
    v.poly(outline, fill=FILL_ROOM if lower else FILL_DECK)
    if not lower:
        # Deck plan: planking lines, quarterdeck, deckhouse with skylight and companionway, hatch, mast, windlass, wheel.
        for y in np.arange(-B2 + 0.3, B2, 0.3):
            pts = [(x, y) for x in xs if half_beam(x) > abs(y) + 0.15]
            if len(pts) > 1:
                v.line(pts, fill=(150, 116, 76, 255), width=1)
        qa, qf, _ = QUARTERDECK
        v.poly([(qa, -half_beam(qa) + 0.15), (qf, -half_beam(qf) + 0.15), (qf, half_beam(qf) - 0.15), (qa, half_beam(qa) - 0.15)], fill=(204, 172, 124, 255))
        v.text((qa + qf) / 2, 0.9, "quarterdeck", 18)
        da, df, _ = DECKHOUSE
        v.poly([(da, -1.2), (df, -1.2), (df, 1.2), (da, 1.2)], fill=FILL_ROOM)
        v.poly([(da + 1.2, -0.45), (df - 0.6, -0.45), (df - 0.6, 0.45), (da + 1.2, 0.45)], fill=(170, 200, 210, 255), width=2)
        v.poly([(da, -0.4), (da + 0.8, -0.4), (da + 0.8, 0.4), (da, 0.4)], fill=(120, 84, 52, 255), width=2)
        v.text((da + df) / 2, 1.55, "deckhouse, skylight", 18)
        v.text(da + 0.4, -0.7, "companion", 15)
        v.poly([(3.1, -0.7), (4.4, -0.7), (4.4, 0.7), (3.1, 0.7)], fill=(150, 112, 70, 255))
        v.text(3.75, 1.0, "hatch", 16)
        v.d.ellipse([v.p(MAST_X - 0.16, 0.16), v.p(MAST_X + 0.16, -0.16)], fill=INK)
        v.poly([(5.6, -0.6), (5.95, -0.6), (5.95, 0.6), (5.6, 0.6)], fill=(120, 84, 52, 255), width=2)
        v.text(5.8, 0.95, "windlass", 15)
        v.line([(-5.2, -0.45), (-5.2, 0.45)], width=5)
        v.text(-5.2, -0.75, "wheel", 15)
        v.line([(L2, 0.0), (L2 + BOWSPRIT, 0.0)], width=5)
    else:
        # Lower deck: hold, saloon with table and benches, chart table and stove, captain's cabin, ladders.
        for bx in (4.9, 1.9, -2.7, -4.6):
            hb = half_beam(bx) * 0.86 - 0.28
            v.line([(bx, -hb), (bx, -0.4)], width=4)
            v.line([(bx, 0.4), (bx, hb)], width=4)
        v.text(3.0, 1.2, "hold (cargo)", 17)
        v.poly([(-1.5, -0.45), (0.9, -0.45), (0.9, 0.45), (-1.5, 0.45)], fill=(160, 120, 78, 255))
        v.text(-0.3, 0.0, "table", 16, fill=(240, 226, 196, 255))
        for s in (-1, 1):
            v.poly([(-2.3, s * 0.85), (1.6, s * 0.85), (1.6, s * 1.35), (-2.3, s * 1.35)], fill=(186, 150, 104, 255), width=2)
        v.text(-0.3, 1.1, "bench / bunk", 15)
        v.text(-0.3, -1.1, "bench / bunk", 15)
        v.poly([(-2.6, -1.55), (-2.0, -1.55), (-2.0, -0.95), (-2.6, -0.95)], fill=(90, 90, 90, 255), width=2)
        v.text(-2.3, -1.85, "stove", 14)
        v.poly([(1.0, 1.0), (1.8, 1.0), (1.8, 1.55), (1.0, 1.55)], fill=(186, 150, 104, 255), width=2)
        v.text(1.4, 1.85, "chart table", 14)
        v.poly([(-4.4, -1.3), (-3.0, -1.3), (-3.0, -0.4), (-4.4, -0.4)], fill=(186, 150, 104, 255), width=2)
        v.text(-3.7, 0.15, "captain's cabin", 15)
        v.dashed([(-2.65, -0.35), (-1.9, -0.35), (-1.9, 0.35), (-2.65, 0.35), (-2.65, -0.35)], fill=INK, width=2)
        v.text(-2.3, 0.6, "ladder", 13)
        v.dashed([(3.2, -0.6), (4.3, -0.6), (4.3, 0.6), (3.2, 0.6), (3.2, -0.6)], fill=INK, width=2)
        figure_plan = v.p(-0.3, -0.6)
        v.d.ellipse([figure_plan[0] - 7, figure_plan[1] - 7, figure_plan[0] + 7, figure_plan[1] + 7], fill=INK)


def section(v):
    """Midship section (x = 0): hull, deck with camber, the deckhouse, the sole and a 1.5 m figure."""
    pts = []
    for k in range(25):
        a = k / 24
        z = bottom(0.0) + (sheer(0.0) - bottom(0.0)) * a
        y = half_beam(0.0) * (max(1 - (1 - a) ** 2.2, 0.0)) ** 0.55
        pts.append((y, z))
    hull = [(-y, z) for y, z in reversed(pts)] + pts
    keel_pts = [(0.14, bottom(0.0) + 0.05), (0.14, keel(0.0)), (-0.14, keel(0.0)), (-0.14, bottom(0.0) + 0.05)]
    v.poly(keel_pts, fill=(60, 50, 46, 255))
    v.line(hull + [hull[0]], width=4)
    v.poly(hull, fill=(150, 64, 46, 255))
    inner = [(y * 0.9, max(z, SOLE - 0.15)) for y, z in hull]
    v.poly(inner, fill=FILL_ROOM, width=2)
    deck_z = sheer(0.0)
    v.line([(-half_beam(0.0), deck_z), (0.0, deck_z + 0.08), (half_beam(0.0), deck_z)], width=6)
    da, df, dh = DECKHOUSE
    v.poly([(-1.2, deck_z + 0.08), (-1.2, deck_z + dh), (1.2, deck_z + dh + 0.06), (1.2, deck_z + 0.08)], fill=FILL_ROOM)
    v.line([(-half_beam(0.0) * 0.85, SOLE), (half_beam(0.0) * 0.85, SOLE)], width=5)
    v.line([(-2.8, 0.0), (2.8, 0.0)], fill=WATER, width=2)
    figure(v, -0.2, SOLE)
    # Headroom marks.
    v.line([(0.45, SOLE), (0.45, deck_z - 0.1)], width=2)
    v.text(0.62, (SOLE + deck_z) / 2 - 0.3, "%.2f m under the beams" % (deck_z - 0.1 - SOLE), 18, anchor="lm")
    v.line([(1.0, SOLE), (1.0, deck_z + dh - 0.06)], width=2)
    v.text(1.6, deck_z + dh + 0.25, "%.2f m under the deckhouse" % (deck_z + dh - 0.06 - SOLE), 18, anchor="lm")


def main():
    W, H = 2600, 1800
    img = Image.fromarray((np.clip(parchment(2600, 81)[:H, :W], 0, 1) * 255).astype(np.uint8)).convert("RGBA")
    d = ImageDraw.Draw(img, "RGBA")
    d.rectangle((14, 14, W - 15, H - 15), outline=INK, width=4)
    d.rectangle((24, 24, W - 25, H - 25), outline=SOFT, width=2)
    d.text((60, 46), "SALTBOUND  -  the second boat, a 14 m gaff cutter (design proposal)", font=font(40, True), fill=INK)
    d.text((60, 100), "14.0 m on deck (17.2 m with the bowsprit)  -  4.4 m beam  -  1.25 m canoe body, 2.05 m with the keel  -"
                      "  about 30 t  -  headroom below 1.65 m under the beams, 2.45 m under the deckhouse", font=font(24), fill=INK)
    v = View(d, 420, 1280, 55)
    profile(v)
    d.text((80, 1610), "PROFILE  (with the rig; inside dashed)", font=font(26, True), fill=INK)
    vp = View(d, 1820, 470, 62)
    plan(vp, lower=False)
    d.text((1300, 250), "DECK PLAN  -  top deck", font=font(26, True), fill=INK)
    vl = View(d, 1820, 900, 62)
    plan(vl, lower=True)
    d.text((1300, 680), "LOWER DECK  -  under deck", font=font(26, True), fill=INK)
    vs = View(d, 2130, 1440, 95)
    section(vs)
    d.text((1300, 1130), "MIDSHIP SECTION  (1.5 m figure)", font=font(26, True), fill=INK)
    # Scale bars.
    for view, x0, z0, n in ((v, -7.5, -3.0, 5), (vp, -7.0, -2.9, 5)):
        view.line([(x0, z0), (x0 + n, z0)], width=4)
        for k in range(n + 1):
            view.line([(x0 + k, z0 - 0.1), (x0 + k, z0 + 0.1)], width=2)
        view.text(x0 + n + 0.3, z0, "%d m" % n, 18, anchor="lm")
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    img.convert("RGB").save(OUT)
    print("SB_PLANS", OUT, img.size)


if __name__ == "__main__":
    main()
