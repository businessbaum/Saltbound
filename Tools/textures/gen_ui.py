"""Hand-crafted UI textures for the satchel (inventory and map, D50): original content, no external licenses.

Run with the host Python (numpy + Pillow):  python Tools/textures/gen_ui.py [frame parchment slots tabs icons maps markers]
Writes RGBA PNGs to Art/Textures/UI/ (imported by Tools/ue_scripts/import_textures.py UI as UI textures, no mips).

  T_UI_Frame          dark wood frame with rounded corners and iron corner plates; transparent inside (9-slice, 0.3)
  T_UI_Parchment      warm parchment with fibres and stains (tiles)
  T_UI_Slot / _Sel    an inked item slot on darker parchment; the selected one outlined in amber (9-slice, 0.3)
  T_UI_Tab / _Active  a wooden tab and the parchment tab of the open page (rounded top corners, 9-slice, 0.3)
  T_Icon_<Item>       item icons, painted at 4x and reduced: Driftwood, Seashell, RopeCoil, AncientShard, GlassFloat
  T_Map_Home          chart of the home waters: 3 km square round the cove (X -500..2500 m, Y -1500..1500 m)
  T_Map_Sea           chart of the open sea: 12 km square (X -3000..9000 m, Y -6000..6000 m)
  T_Map_Region        D57: the open sea's area as one seamless chart at 4096 px (about 3 m a pixel), without a frame, a
                      compass rose or burnt edges: the chart on the mast and its close look show a window of it round the
                      boat (M_SB_Chart, M_UI_Chart; keep the bounds in sync with SaltChart in Core/SaltPlaces.h)
  T_MapMarker_Player / _Boat   map markers, pointing up (+X world, north on the chart)

Charts: north (up) is world +X, east (right) world +Y. Labels and the scale are drawn by the UI (SaltSatchelWidget),
which uses the same bounds (keep MAPS in sync with USaltSatchelWidget::Charts).
"""
import json
import math
import os
import sys

import numpy as np
from PIL import Image, ImageDraw, ImageFilter

HERE = os.path.dirname(__file__)
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
OUT = os.path.join(ROOT, "Art", "Textures", "UI")
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(ROOT, "Art", "Blender", "Scripts"))
from gen_textures import spectral_noise  # noqa: E402
import island_shape  # noqa: E402

INK = (58, 38, 22)
PARCH = np.array([0.86, 0.77, 0.58])


def save(img, name):
    os.makedirs(OUT, exist_ok=True)
    path = os.path.join(OUT, name + ".png")
    img.save(path)
    print("SB_UI", path, img.size)


def to_img(rgb01, alpha01=None):
    rgb = (np.clip(rgb01, 0, 1) * 255).astype(np.uint8)
    if alpha01 is None:
        return Image.fromarray(rgb, "RGB")
    a = (np.clip(alpha01, 0, 1) * 255).astype(np.uint8)
    return Image.fromarray(np.dstack([rgb, a]), "RGBA")


def rounded_mask(w, h, r, inset=0.0, top_only=False):
    """Anti-aliased rounded-rectangle coverage (float 0..1)."""
    y, x = np.mgrid[0:h, 0:w] + 0.5
    x0, y0, x1, y1 = inset, inset, w - inset, h - inset
    cx = np.clip(x, x0 + r, x1 - r)
    cy = np.clip(y, y0 + r, y1 - r) if not top_only else np.where(y > y0 + r, y, y0 + r)
    d = np.hypot(x - cx, y - cy)
    inside = np.clip(r - d + 0.5, 0, 1)
    box = (x > x0) & (x < x1) & (y > y0) & (y < y1 + (r if top_only else 0))
    return inside * box


def parchment(size, seed):
    n = spectral_noise(size, 2, 60, slope=-1.2, seed=seed)
    fib = spectral_noise(size, 60, 200, slope=-0.6, seed=seed + 1, aniso=(1.0, 0.25))
    stain = spectral_noise(size, 1, 6, slope=-2.0, seed=seed + 2)
    v = 1.0 + 0.05 * n + 0.04 * fib - 0.08 * np.clip(stain, 0, 1) ** 1.5
    return PARCH[None, None, :] * v[..., None]


def wood(w, h, seed):
    size = max(w, h)
    grain = spectral_noise(size, 1, 120, slope=-1.0, seed=seed, aniso=(0.08, 1.0))[:h, :w]
    tone = spectral_noise(size, 1, 8, slope=-2.0, seed=seed + 1)[:h, :w]
    base = np.array([0.30, 0.185, 0.10])
    v = 1.0 + 0.22 * grain + 0.1 * tone
    return base[None, None, :] * v[..., None]


# --- panels -------------------------------------------------------------------------------------------------------
def frame():
    s, r, band = 256, 56, 36
    outer = rounded_mask(s, s, r)
    inner = rounded_mask(s, s, r - band * 0.6, inset=band)
    ring = np.clip(outer - inner, 0, 1)
    rgb = wood(s, s, 11)
    y, x = np.mgrid[0:s, 0:s] + 0.5
    # A lighter bevel along the inner edge and a dark line on the outer edge.
    inner_edge = np.clip(rounded_mask(s, s, r - band * 0.6 + 4, inset=band - 4) - inner, 0, 1)
    outer_edge = np.clip(outer - rounded_mask(s, s, r - 3, inset=3), 0, 1)
    rgb = rgb * (1 + 0.45 * inner_edge[..., None]) * (1 - 0.5 * outer_edge[..., None])
    # Iron corner plates with rivets.
    img = to_img(rgb, ring)
    d = ImageDraw.Draw(img)
    for cx, cy in ((26, 26), (s - 26, 26), (26, s - 26), (s - 26, s - 26)):
        d.rounded_rectangle((cx - 18, cy - 18, cx + 18, cy + 18), radius=6, fill=(46, 44, 42, 255), outline=(20, 18, 16, 255), width=2)
        for dx, dy in ((-9, -9), (9, -9), (-9, 9), (9, 9)):
            d.ellipse((cx + dx - 3, cy + dy - 3, cx + dx + 3, cy + dy + 3), fill=(110, 104, 96, 255))
    save(img, "T_UI_Frame")


def parchment_tile():
    save(to_img(parchment(512, 21)), "T_UI_Parchment")


def slot(selected):
    s = 128
    m = rounded_mask(s, s, 18, inset=4)
    rgb = parchment(s, 31) * 0.86
    y, x = np.mgrid[0:s, 0:s] + 0.5
    edge = np.clip(m - rounded_mask(s, s, 14, inset=9 if selected else 7), 0, 1)
    shade = np.clip(1.0 - (np.hypot(x - s / 2, y - s / 2) / (s * 0.7)), 0, 1)
    rgb = rgb * (0.9 + 0.1 * shade[..., None])
    ink = np.array([0.85, 0.52, 0.18]) if selected else np.array(INK) / 255.0
    rgb = rgb * (1 - edge[..., None]) + ink[None, None, :] * edge[..., None]
    save(to_img(rgb, m * 0.95 + edge * 0.05), "T_UI_SlotSel" if selected else "T_UI_Slot")


def tab(active):
    w, h = 256, 96
    m = rounded_mask(w, h, 22, inset=2, top_only=True)
    if active:
        rgb = parchment(256, 41)[:h, :w]
        edge = np.clip(m - rounded_mask(w, h, 18, inset=6, top_only=True), 0, 1)
        rgb = rgb * (1 - edge[..., None]) + (np.array(INK) / 255.0)[None, None, :] * edge[..., None]
    else:
        rgb = wood(w, h, 51) * 1.15
    save(to_img(rgb, m), "T_UI_TabActive" if active else "T_UI_Tab")


# --- icons --------------------------------------------------------------------------------------------------------
def icon_canvas():
    return Image.new("RGBA", (512, 512), (0, 0, 0, 0))


def finish_icon(img, name, glow=None):
    if glow is not None:
        halo = img.split()[3].filter(ImageFilter.GaussianBlur(28))
        g = Image.new("RGBA", img.size, glow + (0,))
        g.putalpha(halo.point(lambda a: int(a * 0.8)))
        img = Image.alpha_composite(g, img)
    save(img.resize((128, 128), Image.LANCZOS), "T_Icon_" + name)


def outline_poly(d, pts, fill, width=14):
    d.polygon(pts, fill=INK + (255,))
    # Shrink towards the centroid for the fill (a thick inked edge).
    cx = sum(p[0] for p in pts) / len(pts)
    cy = sum(p[1] for p in pts) / len(pts)
    inner = []
    for x, y in pts:
        dx, dy = x - cx, y - cy
        l = math.hypot(dx, dy) or 1
        inner.append((x - dx / l * width, y - dy / l * width))
    d.polygon(inner, fill=fill)


def icon_driftwood():
    img = icon_canvas()
    d = ImageDraw.Draw(img)
    spine = [(70, 380), (170, 330), (260, 300), (350, 230), (440, 140)]
    for wdt, col in ((70, INK + (255,)), (50, (176, 160, 136, 255))):
        d.line(spine, fill=col, width=wdt, joint="curve")
        for p in (spine[0], spine[-1]):
            d.ellipse((p[0] - wdt / 2, p[1] - wdt / 2, p[0] + wdt / 2, p[1] + wdt / 2), fill=col)
    d.line([(250, 305), (300, 380), (310, 420)], fill=INK + (255,), width=34)
    d.line([(250, 305), (300, 380), (310, 420)], fill=(176, 160, 136, 255), width=18)
    for k in range(4):
        off = -14 + k * 9
        d.line([(p[0] + off * 0.4, p[1] + off) for p in spine[1:4]], fill=(130, 114, 96, 255), width=3)
    d.ellipse((205, 300, 235, 324), fill=(110, 92, 74, 255))
    finish_icon(img, "Driftwood")


def icon_seashell():
    img = icon_canvas()
    d = ImageDraw.Draw(img)
    hinge = (256, 410)
    pts = [hinge]
    for k in range(13):
        a = math.radians(200 + k * (140 / 12))
        r = 230 + 14 * (1 if k % 2 else 0)
        pts.append((hinge[0] + r * math.cos(a), hinge[1] + r * math.sin(a)))
    outline_poly(d, pts, (236, 196, 176, 255))
    for k in range(1, 12):
        a = math.radians(200 + k * (140 / 12))
        d.line([hinge, (hinge[0] + 215 * math.cos(a), hinge[1] + 215 * math.sin(a))], fill=(190, 128, 112, 255), width=7)
    d.polygon([(200, 420), (256, 380), (312, 420), (290, 450), (222, 450)], fill=INK + (255,))
    d.polygon([(214, 420), (256, 392), (298, 420), (282, 438), (230, 438)], fill=(214, 170, 150, 255))
    finish_icon(img, "Seashell")


def icon_ropecoil():
    img = icon_canvas()
    d = ImageDraw.Draw(img)
    for k, (rx, ry) in enumerate(((200, 150), (160, 118), (120, 88))):
        box = (256 - rx, 270 - ry, 256 + rx, 270 + ry)
        d.ellipse(box, outline=INK + (255,), width=46)
        d.ellipse((box[0] + 8, box[1] + 8, box[2] - 8, box[3] - 8), outline=(196, 160, 104, 255), width=30)
        for j in range(26):
            a = j * 2 * math.pi / 26
            x, y = 256 + (rx - 4) * math.cos(a), 270 + (ry - 4) * math.sin(a)
            d.line([(x - 9, y - 9), (x + 9, y + 9)], fill=(138, 104, 58, 255), width=5)
    d.line([(420, 290), (470, 380), (450, 440)], fill=INK + (255,), width=34)
    d.line([(420, 290), (470, 380), (450, 440)], fill=(196, 160, 104, 255), width=20)
    finish_icon(img, "RopeCoil")


def icon_ancientshard():
    img = icon_canvas()
    d = ImageDraw.Draw(img)
    pts = [(256, 50), (330, 170), (318, 400), (256, 460), (190, 390), (182, 160)]
    outline_poly(d, pts, (70, 200, 186, 255), width=16)
    d.polygon([(256, 72), (256, 438), (204, 380), (198, 168)], fill=(130, 236, 220, 255))
    d.line([(256, 72), (256, 438)], fill=(30, 110, 100, 255), width=6)
    for y in (180, 260, 330):
        d.line([(214, y), (240, y + 14), (226, y + 34)], fill=(220, 255, 250, 255), width=5)
    finish_icon(img, "AncientShard", glow=(120, 255, 230))


def icon_glassfloat():
    img = icon_canvas()
    d = ImageDraw.Draw(img)
    d.ellipse((76, 86, 436, 446), fill=INK + (255,))
    d.ellipse((92, 102, 420, 430), fill=(74, 150, 112, 255))
    d.ellipse((140, 140, 260, 250), fill=(170, 226, 196, 255))
    d.ellipse((160, 156, 220, 200), fill=(236, 252, 244, 255))
    for k in range(4):
        rx = 164 * (1 - k / 4)                       # meridians of the rope net, all inside the ball
        d.arc((256 - rx, 102, 256 + rx, 430), 0, 360, fill=(170, 136, 86, 255), width=8)
    for yy in (190, 266, 342):
        half = math.sqrt(max(164 ** 2 - (yy - 266) ** 2, 0))
        d.line([(256 - half, yy), (256 + half, yy)], fill=(170, 136, 86, 255), width=8)
    d.line([(256, 102), (256, 46)], fill=INK + (255,), width=22)
    d.line([(256, 102), (256, 50)], fill=(196, 160, 104, 255), width=10)
    finish_icon(img, "GlassFloat")


# D73 (M6 step 2): the first materials, painted like the others (ink edge, broad flat colour, a few strokes of detail).
def icon_canvas_bolt():
    img = icon_canvas()
    d = ImageDraw.Draw(img)
    cloth = (222, 210, 182, 255)
    shade = (186, 170, 140, 255)
    for k, (x0, y0, x1, y1) in enumerate(((96, 300, 432, 380), (88, 238, 424, 318), (80, 176, 416, 256))):
        d.rounded_rectangle((x0 - 10, y0 - 10, x1 + 10, y1 + 10), radius=34, fill=INK + (255,))
        d.rounded_rectangle((x0, y0, x1, y1), radius=26, fill=cloth if k == 2 else shade)
        d.line([(x0 + 30, y1 - 14), (x1 - 30, y1 - 14)], fill=(160, 144, 114, 255), width=5)
    # Twine crosswise and its knot.
    d.line([(256, 160), (256, 396)], fill=INK + (255,), width=22)
    d.line([(256, 162), (256, 394)], fill=(196, 160, 104, 255), width=10)
    d.line([(70, 210), (442, 340)], fill=INK + (255,), width=20)
    d.line([(72, 211), (440, 339)], fill=(196, 160, 104, 255), width=9)
    d.ellipse((232, 236, 280, 284), fill=INK + (255,))
    d.ellipse((240, 244, 272, 276), fill=(196, 160, 104, 255))
    finish_icon(img, "Canvas")


def icon_iron_nails():
    img = icon_canvas()
    d = ImageDraw.Draw(img)
    iron = (92, 90, 86, 255)
    for (x0, y0, x1, y1) in ((150, 120, 330, 400), (210, 100, 400, 360), (110, 210, 380, 300)):
        d.line([(x0, y0), (x1, y1)], fill=INK + (255,), width=40)
        d.line([(x0, y0), (x1, y1)], fill=iron, width=22)
        d.line([(x0 + 6, y0 + 2), (x1 - 6, y1 - 6)], fill=(150, 148, 140, 255), width=5)
        d.ellipse((x0 - 34, y0 - 34, x0 + 34, y0 + 34), fill=INK + (255,))
        d.ellipse((x0 - 24, y0 - 24, x0 + 24, y0 + 24), fill=iron)
        d.ellipse((x0 - 14, y0 - 18, x0 + 4, y0 - 4), fill=(160, 158, 150, 255))
        d.polygon([(x1 - 4, y1 - 14), (x1 + 26, y1 + 22), (x1 - 14, y1 + 2)], fill=INK + (255,))
    finish_icon(img, "IronNails")


def icon_tar():
    img = icon_canvas()
    d = ImageDraw.Draw(img)
    wood = (88, 62, 44, 255)
    body = [(126, 140), (386, 140), (406, 260), (386, 420), (126, 420), (106, 260)]
    outline_poly(d, body, wood, width=16)
    for x in (168, 214, 256, 298, 344):
        d.line([(x, 150), (x + (x - 256) * 0.08, 262), (x, 410)], fill=(56, 38, 26, 255), width=5)
    for y in (186, 374):
        d.rounded_rectangle((100, y - 16, 412, y + 16), radius=10, fill=INK + (255,))
        d.rounded_rectangle((108, y - 9, 404, y + 9), radius=7, fill=(110, 106, 98, 255))
    # The black, glossy tar in the open head, a drip over the rim.
    d.ellipse((120, 96, 392, 180), fill=INK + (255,))
    d.ellipse((134, 106, 378, 170), fill=(26, 22, 20, 255))
    d.ellipse((180, 118, 250, 136), fill=(120, 116, 110, 255))
    d.polygon([(300, 160), (330, 160), (322, 236), (308, 236)], fill=(26, 22, 20, 255))
    d.ellipse((302, 224, 328, 250), fill=(26, 22, 20, 255))
    finish_icon(img, "Tar")


def icon_pale_float():
    img = icon_canvas()
    d = ImageDraw.Draw(img)
    d.ellipse((86, 116, 426, 456), fill=INK + (255,))
    d.ellipse((102, 132, 410, 440), fill=(226, 218, 196, 255))
    d.ellipse((150, 170, 270, 270), fill=(244, 240, 226, 255))
    # The moulding seam, the eye on top and the moulded number.
    d.arc((102, 236, 410, 336), 0, 180, fill=(170, 160, 136, 255), width=10)
    d.ellipse((222, 40, 290, 108), outline=INK + (255,), width=26)
    d.ellipse((226, 44, 286, 104), outline=(226, 218, 196, 255), width=12)
    d.rectangle((236, 100, 276, 140), fill=INK + (255,))
    d.rectangle((244, 104, 268, 136), fill=(226, 218, 196, 255))
    d.text((236, 330), "17", fill=(150, 140, 118, 255), font=None)
    d.line([(236, 340), (236, 384)], fill=(150, 140, 118, 255), width=9)
    d.line([(262, 340), (292, 340), (270, 384)], fill=(150, 140, 118, 255), width=9)
    finish_icon(img, "PaleFloat")


# --- charts -------------------------------------------------------------------------------------------------------
MAPS = {
    # name: (centre X m, centre Y m, size m, grid m)
    "T_Map_Home": (1000.0, 0.0, 3000.0, 500.0),
    "T_Map_Sea": (3000.0, 0.0, 12000.0, 1000.0),
    "T_Map_Region": (3000.0, 0.0, 12000.0, 500.0),
}
REGION = {"T_Map_Region": 4096}
ISLE = (1600.0, 600.0)
FAR = [((5500.0, 2500.0), 20.0, (159.0, 130.0), 71), ((7000.0, -1500.0), 140.0, (134.0, 119.0), 72),
       ((4200.0, -4800.0), 75.0, (223.0, 87.0), 73)]
SPIRE = (560.0, 260.0)            # moved out for the home island (D60)
STORM = (0.0, -3000.0, 1600.0)


def isle_coast(a):
    return 55.0 * (1.0 + 0.16 * np.sin(3 * a + 0.7) + 0.09 * np.sin(5 * a + 2.1) + 0.05 * np.sin(9 * a + 4.0))


def chart(name, size_px=1024):
    region = name in REGION
    size_px = REGION.get(name, size_px)
    cx, cy, size, grid = MAPS[name]
    px = size / size_px
    j, i = np.mgrid[0:size_px, 0:size_px] + 0.5
    X = cx + (0.5 - j / size_px) * size           # up = +X
    Y = cy + (i / size_px - 0.5) * size           # right = +Y
    land = np.zeros_like(X)
    shallow = np.zeros_like(X)
    # Ruin Isle: land inside the coast, shallows to 1.35 x the coast radius.
    dx, dy = X - ISLE[0], Y - ISLE[1]
    t = np.hypot(dx, dy) / isle_coast(np.arctan2(dy, dx))
    land = np.maximum(land, np.clip((1.0 - t) * 40, 0, 1))
    shallow = np.maximum(shallow, np.clip((1.45 - t) * 8, 0, 1))
    # Far islands: noisy ellipses.
    for (fx, fy), yaw, (ax, ay), seed in FAR:
        c, s = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))
        lx = ((X - fx) * c + (Y - fy) * s) / ax
        ly = (-(X - fx) * s + (Y - fy) * c) / ay
        ang = np.arctan2(ly, lx)
        rr = 1.0 + 0.12 * np.sin(3 * ang + seed) + 0.07 * np.sin(7 * ang + seed * 1.3)
        t = np.hypot(lx, ly) / rr
        land = np.maximum(land, np.clip((1.0 - t) * 40, 0, 1))
        shallow = np.maximum(shallow, np.clip((1.3 - t) * 8, 0, 1))
    # The spire.
    t = np.hypot(X - SPIRE[0], Y - SPIRE[1]) / 14.0
    land = np.maximum(land, np.clip((1.0 - t) * 4, 0, 1))
    shallow = np.maximum(shallow, np.clip((2.5 - t), 0, 1))
    # The home island (D60): its ring of land round the lagoon, the reef flat and the channel (island_shape.py).
    rows = np.where(np.abs(X[:, 0] - island_shape.CENTRE[0]) < island_shape.HALF)[0]
    cols = np.where(np.abs(Y[0, :] - island_shape.CENTRE[1]) < island_shape.HALF)[0]
    if len(rows) and len(cols):
        sl = (slice(rows[0], rows[-1] + 1), slice(cols[0], cols[-1] + 1))
        z = island_shape.base(X[sl], Y[sl])
        land[sl] = np.maximum(land[sl], np.clip(z * 4.0 + 0.5, 0, 1))
        shallow[sl] = np.maximum(shallow[sl], np.clip((z + 3.0) * 2.0, 0, 1))
    # Rocks of the pier's beach and of the island (map script and build_home_island.py positions, m).
    with open(os.path.join(ROOT, "Art", "Generated", "home_island.json"), encoding="utf-8") as fh:
        home_rocks = [(r[1] / 100.0, r[2] / 100.0, 2.4 * r[5]) for r in json.load(fh)["rocks"]]
    for rx, ry, rad in [(-7.5, 1.5, 4), (-5.5, -7.5, 3), (-11, -2.5, 5), (-2.6, 5.2, 3), (-2.2, -3.8, 3), (-6, 8.5, 4)] + home_rocks:
        t = np.hypot(X - rx, Y - ry) / max(rad, px * 1.5)
        land = np.maximum(land, np.clip((1.0 - t) * 4, 0, 1))
    land = np.clip(land, 0, 1)
    shallow = np.clip(np.maximum(shallow, land), 0, 1)

    rgb = parchment(size_px, 61)
    # Burnt, darker edges.
    vig = np.clip(np.hypot(i / size_px - 0.5, j / size_px - 0.5) * 2.0, 0, 1) ** 3
    if not region:
        rgb *= (1 - 0.35 * vig)[..., None]
    # Shallows: a teal wash; land: darker parchment.
    rgb = rgb * (1 - 0.35 * shallow[..., None]) + np.array([0.64, 0.76, 0.66])[None, None, :] * 0.35 * shallow[..., None]
    rgb = rgb * (1 - land[..., None]) + (parchment(size_px, 71) * np.array([0.86, 0.82, 0.7]))[...] * land[..., None]
    # Storm waters: diagonal hatching inside the storm zone.
    sd = np.hypot(X - STORM[0], Y - STORM[1]) / STORM[2]
    hatch = ((i + j) % 14 < 2) & (sd < 1.0)
    rgb[hatch] = rgb[hatch] * 0.55
    img = to_img(rgb).convert("RGBA")
    d = ImageDraw.Draw(img)
    ink = INK + (255,)
    faint = INK + (70,)
    # Grid.
    for g in np.arange(math.floor((cx - size / 2) / grid) * grid, cx + size / 2 + grid, grid):
        v = (0.5 - (g - cx) / size) * size_px
        for k in range(0, size_px, 12):
            d.line([(k, v), (k + 5, v)], fill=faint, width=1)
    for g in np.arange(math.floor((cy - size / 2) / grid) * grid, cy + size / 2 + grid, grid):
        u = (0.5 + (g - cy) / size) * size_px
        for k in range(0, size_px, 12):
            d.line([(u, k), (u, k + 5)], fill=faint, width=1)
    # Coastlines in ink, and short hatch strokes along the outside of each coast.
    coast = (land > 0.5)
    edge = coast & ~(np.roll(coast, 1, 0) & np.roll(coast, -1, 0) & np.roll(coast, 1, 1) & np.roll(coast, -1, 1))
    ys, xs = np.nonzero(edge)
    for y0, x0 in zip(ys, xs):
        d.ellipse((x0 - 1.4, y0 - 1.4, x0 + 1.4, y0 + 1.4), fill=ink)
    sh = (shallow > 0.5) & ~coast
    sedge = sh & ~(np.roll(sh, 1, 0) & np.roll(sh, -1, 0) & np.roll(sh, 1, 1) & np.roll(sh, -1, 1))
    ys, xs = np.nonzero(sedge)
    for y0, x0 in list(zip(ys, xs))[::3]:
        d.point((x0, y0), fill=INK + (150,))
    # Little wave marks scattered on the open water.
    rng = np.random.default_rng(5)
    for _ in range(140 if not region else 2200):
        u, v = rng.uniform(40, size_px - 40, 2)
        if shallow[int(v), int(u)] > 0.1:
            continue
        d.arc((u - 8, v - 4, u, v + 4), 200, 340, fill=INK + (90,), width=1)
        d.arc((u, v - 4, u + 8, v + 4), 200, 340, fill=INK + (90,), width=1)
    # Compass rose (north up) in the top right corner, with an "N" drawn in strokes (not on the seamless region chart).
    rx, ry, rr = (size_px - 110, 110, 62) if not region else (-1000, -1000, 62)
    for k in range(8):
        a = k * math.pi / 4 - math.pi / 2
        ln = rr if k % 2 == 0 else rr * 0.6
        tip = (rx + ln * math.cos(a), ry + ln * math.sin(a))
        side1 = (rx + 10 * math.cos(a + math.pi / 2), ry + 10 * math.sin(a + math.pi / 2))
        side2 = (rx + 10 * math.cos(a - math.pi / 2), ry + 10 * math.sin(a - math.pi / 2))
        d.polygon([tip, side1, (rx, ry)], fill=ink)
        d.polygon([tip, side2, (rx, ry)], fill=(200, 176, 132, 255), outline=ink)
    d.ellipse((rx - rr * 0.45, ry - rr * 0.45, rx + rr * 0.45, ry + rr * 0.45), outline=ink, width=2)
    nx, ny = rx, ry - rr - 22
    d.line([(nx - 7, ny + 9), (nx - 7, ny - 9), (nx + 7, ny + 9), (nx + 7, ny - 9)], fill=ink, width=3)
    # Ruin symbol (a little arch) on Ruin Isle's knoll.
    ku = (0.5 + (ISLE[1] - 8 - cy) / size) * size_px
    kv = (0.5 - (ISLE[0] + 14 - cx) / size) * size_px
    sc = max(4.0, 10.0 * 3000.0 / size)
    d.line([(ku - sc, kv + sc), (ku - sc, kv - sc * 0.2)], fill=ink, width=2)
    d.line([(ku + sc, kv + sc), (ku + sc, kv - sc * 0.2)], fill=ink, width=2)
    d.arc((ku - sc, kv - sc * 1.2, ku + sc, kv + sc * 0.8), 180, 360, fill=ink, width=2)
    # Double border.
    if not region:
        d.rectangle((10, 10, size_px - 11, size_px - 11), outline=ink, width=3)
        d.rectangle((18, 18, size_px - 19, size_px - 19), outline=INK + (160,), width=1)
    save(img, name)


def marker(kind):
    s = 256
    img = Image.new("RGBA", (s, s), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    if kind == "Player":
        pts = [(128, 20), (196, 210), (128, 168), (60, 210)]
        d.polygon(pts, fill=INK + (255,))
        d.polygon([(128, 52), (174, 186), (128, 154), (82, 186)], fill=(176, 52, 36, 255))
    else:
        pts = [(128, 16), (170, 70), (180, 150), (160, 230), (96, 230), (76, 150), (86, 70)]
        d.polygon(pts, fill=INK + (255,))
        d.polygon([(128, 38), (158, 80), (166, 150), (150, 214), (106, 214), (90, 150), (98, 80)], fill=(232, 214, 176, 255))
        d.line([(128, 60), (128, 200)], fill=INK + (255,), width=6)
    save(img.resize((64, 64), Image.LANCZOS), "T_MapMarker_" + kind)


BUILDERS = {
    "frame": frame, "parchment": parchment_tile, "slots": lambda: (slot(False), slot(True)),
    "tabs": lambda: (tab(False), tab(True)),
    "icons": lambda: (icon_driftwood(), icon_seashell(), icon_ropecoil(), icon_ancientshard(), icon_glassfloat()),
    # D73: run "materials" alone to paint only these.
    "materials": lambda: (icon_canvas_bolt(), icon_iron_nails(), icon_tar(), icon_pale_float()),
    "maps": lambda: [chart(n) for n in MAPS], "markers": lambda: (marker("Player"), marker("Boat")),
}

if __name__ == "__main__":
    for key in sys.argv[1:] or list(BUILDERS):
        BUILDERS[key]()
