"""The home island's ground textures (D71): sand, grass, forest litter and the footpath's earth, with real surface detail.

Owner (2026-10-06): "the floor is good colored but lacks EVERYTHING ELSE. Only color, there is no real texture everything is
perfectly flat ... Keypoints are a lack of surface texture and material realism, Microtexture, Surface roughness."

Each set is painted from things that lie on the ground, stamped one by one (crisp, hand-made, Sea of Thieves-like; no
photographic noise): sand grains, micro ripples with dark grains in their troughs, shell bits, pebbles; grass in clumps of
blades; leaves, twigs, roots and pebbles on the forest floor; stones pressed into the footpath. The heights of the stamps
make the normal maps, the AO and the roughness, and the material blends the sets by height (ORM.b), so sand settles between
the grass's clumps and the clumps stand out of the sand at the edges.

  T_Isl<Set>_D    albedo (sRGB), coloured: the island's palette (the material tints it only to calibrate)
  T_Isl<Set>_N    normal (DirectX convention as gen_textures.height_to_normal)
  T_Isl<Set>_ORM  R AO, G roughness, B height (0 low .. 1 high, for the height blend)
Sets (tile size in metres = the material's scale): Sand 2.0, Grass 1.6, Litter 2.0, Earth 2.0; 2048 px.

Run with the host Python:  python Tools/textures/gen_island_textures.py [sand grass litter earth] [--preview]
Writes Art/Textures/Island/. Import: python Tools/ue_exec.py Tools/ue_scripts/import_textures.py Island
"""
import math
import os
import sys

import numpy as np
from PIL import Image

sys.path.insert(0, os.path.dirname(__file__))
from gen_textures import (crisp, height_to_normal, n01, save_normal, save_rgb, spectral_noise,  # noqa: E402
                          spectral_noise_dir, worley_cells)

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
OUT = os.path.join(ROOT, "Art", "Textures", "Island")
SIZE = 2048


# --- a tiling canvas and stamps -----------------------------------------------------------------------------------
class Canvas:
    """Albedo (sRGB 0..1), height (in pixels, so slopes are physical), roughness and an occupancy mask; everything wraps."""

    def __init__(self, size, rgb, rough):
        self.n = size
        self.rgb = np.empty((size, size, 3))
        self.rgb[...] = rgb
        self.h = np.zeros((size, size))
        self.rough = np.full((size, size), rough)
        self.cover = np.zeros((size, size))       # how much lies here (for contact shadows and AO)

    def region(self, x0, y0, w, h):
        return np.ix_(np.arange(y0, y0 + h) % self.n, np.arange(x0, x0 + w) % self.n)

    def stamp(self, x, y, s, settle=0.6, mode="pile"):
        """Lays a sprite s = dict(a, rgb, h, rough) centred at (x, y). mode 'pile': it rests on what is under it (the
        footprint's mean height times `settle`, so leaves heap up a little); 'press': pressed into the ground (its top
        at its own height); 'paint': colour and roughness only."""
        a = s["a"]
        sh, sw = a.shape
        ix = self.region(int(round(x - sw / 2)), int(round(y - sh / 2)), sw, sh)
        a3 = a[..., None]
        self.rgb[ix] = self.rgb[ix] * (1.0 - a3) + s["rgb"] * a3
        if "rough" in s:
            self.rough[ix] = self.rough[ix] * (1.0 - a) + s["rough"] * a
        if mode == "paint":
            return
        under = self.h[ix]
        if mode == "pile":
            w = a.sum()
            base = (under * a).sum() / w * settle if w > 1e-6 else 0.0
            top = base + s["h"]
            self.h[ix] = under * (1.0 - a) + np.maximum(under, top) * a
        else:
            w = a.sum()
            base = (under * a).sum() / w if w > 1e-6 else 0.0
            self.h[ix] = under * (1.0 - a) + (base + s["h"]) * a
        self.cover[ix] = np.maximum(self.cover[ix], a)


def grid(w, h):
    y, x = np.mgrid[0:h, 0:w].astype(float)
    return x - (w - 1) / 2.0, y - (h - 1) / 2.0


def rot(x, y, ang):
    c, s = math.cos(ang), math.sin(ang)
    return x * c + y * s, -x * s + y * c


def edge(d, soft=0.8):
    """Anti-aliased inside mask from a signed distance in pixels (d > 0 inside)."""
    return np.clip(d / soft + 0.5, 0.0, 1.0)


def shade(col, k):
    return np.clip(np.asarray(col, dtype=float) * k, 0.0, 1.0)


# --- sprites ------------------------------------------------------------------------------------------------------
def sp_leaf(rng, length, width, ang, col, curl=0.25, vein=0.1, rough=0.6, decay=0.0, lobed=False):
    """A fallen leaf: pointed oval with a stalk, midrib and side veins; it cups (edges up) or arches; decay spots."""
    r = int(length * 0.62 + 4)
    x, y = grid(2 * r + 1, 2 * r + 1)
    u, v = rot(x, y, ang)
    t = u / length + 0.5                                         # 0 at the stalk end .. 1 at the tip
    prof = np.where((t > 0) & (t < 1), np.sin(np.pi * np.clip(t, 0, 1) ** 0.85) ** 0.9, 0.0)
    if lobed:
        prof = prof * (1.0 + 0.12 * np.sin(t * 14.0))
    half = width * 0.5 * prof
    a = edge(half - np.abs(v)) * (t > 0) * (t < 1)
    stalk = edge(1.1 - np.abs(v)) * (t > -0.12) * (t <= 0.02)
    a = np.maximum(a, stalk)
    q = np.clip(np.abs(v) / np.maximum(half, 1e-3), 0, 1)
    rib = np.exp(-(v / 1.1) ** 2)
    side = np.abs(np.sin((t * 7.0 - q * 1.6) * np.pi))           # side veins sweeping to the tip
    side = np.clip(1.0 - side / 0.12, 0, 1) * (q > 0.12) * (q < 0.9)
    arch = 1.0 - (2 * t - 1) ** 2
    h = (curl * width * 0.5 * (q ** 2) + 0.12 * width * arch) - rib * 0.25 - side * 0.12
    h = np.maximum(h, 0.0) + 0.6
    base = np.asarray(col, dtype=float)
    k = 0.82 + 0.25 * t - 0.1 * q
    rgb = base * k[..., None]
    rgb = rgb * (1.0 - vein * side)[..., None]
    rgb = rgb + (np.clip(base * 1.25, 0, 1) - rgb) * (0.45 * rib * (t > 0.02))[..., None]
    if decay > 0:
        spots = np.zeros_like(t)
        for _ in range(rng.integers(1, 4)):
            sx, sy = rng.uniform(-0.3, 0.3) * length, rng.uniform(-0.25, 0.25) * width
            rr = rng.uniform(0.06, 0.16) * length
            spots = np.maximum(spots, edge(rr - np.hypot(u - sx, v - sy), 1.5))
        rgb = rgb * (1.0 - decay * 0.45 * spots)[..., None]
    rgb = np.where(stalk[..., None] > a[..., None] - 1e-6, shade(base, 0.7), rgb)
    return {"a": a, "rgb": np.clip(rgb, 0, 1), "h": h * a, "rough": np.full_like(a, rough)}


def sp_twig(rng, length, radius, ang, col, bend=0.08, rough=0.8, fork=False):
    """A fallen twig: a slightly bent round stick, lit along its top, with bark flecks; sometimes a side twig."""
    r = int(length * 0.55 + radius * 2 + 4)
    x, y = grid(2 * r + 1, 2 * r + 1)
    u, v = rot(x, y, ang)
    s = np.clip(u / length + 0.5, 0, 1)
    vv = v - bend * length * (4 * s * (1 - s)) * (1 if rng.random() < 0.5 else -1)
    rad = radius * (1.0 - 0.35 * s)
    inside = (np.abs(u) < length * 0.5)
    d = rad - np.abs(vv)
    a = edge(d) * inside
    hh = np.sqrt(np.clip(1.0 - (vv / np.maximum(rad, 0.5)) ** 2, 0, 1)) * rad
    if fork:
        fs = rng.uniform(0.25, 0.6)
        fang = rng.choice([-1, 1]) * rng.uniform(0.4, 0.8)
        fu, fv = rot(u - (fs - 0.5) * length, vv, fang)
        flen = length * rng.uniform(0.25, 0.4)
        frad = radius * 0.6
        fin = (fu > 0) & (fu < flen)
        fa = edge(frad - np.abs(fv)) * fin
        fh = np.sqrt(np.clip(1.0 - (fv / frad) ** 2, 0, 1)) * frad
        a = np.maximum(a, fa)
        hh = np.maximum(hh * (a > 0), fh * fa)
    light = np.clip(0.5 - vv / np.maximum(rad, 0.5) * 0.5, 0, 1)    # lit along one side (the painter's light)
    base = np.asarray(col, dtype=float)
    fleck = (rng.random(a.shape) < 0.04).astype(float)
    rgb = base * (0.75 + 0.45 * light)[..., None] * (1.0 - 0.25 * fleck)[..., None]
    return {"a": a, "rgb": np.clip(rgb, 0, 1), "h": (hh + 0.4) * a, "rough": np.full_like(a, rough)}


def sp_pebble(rng, rx, ry, ang, col, rough=0.55, height=0.6, flat=0.0):
    """A pebble: a rounded blob (superellipse), domed or flat-topped (worn into a path), lit on top, speckled."""
    r = int(max(rx, ry) + 4)
    x, y = grid(2 * r + 1, 2 * r + 1)
    u, v = rot(x, y, ang)
    k = rng.uniform(2.2, 3.2)
    wob = 1.0 + 0.08 * np.sin(np.arctan2(v, u) * rng.integers(2, 5) + rng.uniform(0, 6))
    d = (np.abs(u / rx) ** k + np.abs(v / ry) ** k) ** (1.0 / k) / wob
    a = edge((1.0 - d) * min(rx, ry))
    dome = np.sqrt(np.clip(1.0 - d * d, 0, 1))
    dome = np.minimum(dome, 1.0 - flat) / max(1.0 - flat, 1e-3)
    h = dome * min(rx, ry) * height
    base = np.asarray(col, dtype=float)
    top = np.clip(1.0 - (u * 0.3 + v * 0.6) / max(rx, ry), 0.6, 1.4)
    speck = (rng.random(a.shape) < 0.05) * rng.uniform(-0.25, 0.25)
    rgb = base * (0.82 + 0.18 * top + speck)[..., None]
    return {"a": a, "rgb": np.clip(rgb, 0, 1), "h": h * a, "rough": np.full_like(a, rough)}


def sp_shell_bit(rng, size, ang, col, rough=0.5):
    """A shell fragment: an irregular flat shard, pale, with a darker growth line or two."""
    r = int(size + 3)
    x, y = grid(2 * r + 1, 2 * r + 1)
    u, v = rot(x, y, ang)
    n = rng.integers(4, 7)
    angs = np.sort(rng.uniform(0, 2 * np.pi, n))
    rads = rng.uniform(0.45, 1.0, n) * size
    th = np.arctan2(v, u) % (2 * np.pi)
    # A polygon's radius by angle (linear between its corners).
    a2 = np.concatenate([angs, angs[:1] + 2 * np.pi])
    r2 = np.concatenate([rads, rads[:1]])
    rr = np.interp(th, a2, r2, period=None)
    rr = np.where(th < angs[0], np.interp(th + 2 * np.pi, a2, r2), rr)
    d = rr - np.hypot(u, v)
    a = edge(d)
    growth = np.clip(1.0 - np.abs(np.sin(np.hypot(u + size, v) / size * rng.uniform(5, 9))) / 0.15, 0, 1)
    base = np.asarray(col, dtype=float)
    rgb = base * (1.0 - 0.15 * growth)[..., None]
    h = (0.5 + 0.3 * np.clip(d / max(size, 1) * 2, 0, 1)) * a
    return {"a": a, "rgb": np.clip(rgb, 0, 1), "h": h, "rough": np.full_like(a, rough)}


def sp_cockle(rng, size, ang, col, rough=0.45):
    """A small whole cockle: a fan with radial ribs and a hinge, domed."""
    r = int(size * 1.1 + 3)
    x, y = grid(2 * r + 1, 2 * r + 1)
    u, v = rot(x, y, ang)
    vv = v + size * 0.45
    rad = np.hypot(u, vv)
    th = np.arctan2(u, vv)
    a = edge(size - rad) * edge((1.25 - np.abs(th)) * size * 0.6)
    ribs = 0.5 + 0.5 * np.cos(th * 18.0)
    dome = np.sqrt(np.clip(1.0 - (rad / size) ** 2, 0, 1))
    h = (dome * size * 0.45 + ribs * 0.5) * a
    base = np.asarray(col, dtype=float)
    band = 0.5 + 0.5 * np.sin(rad / size * 9.0)
    rgb = base * (0.85 + 0.12 * ribs + 0.06 * band)[..., None]
    return {"a": a, "rgb": np.clip(rgb, 0, 1), "h": h, "rough": np.full_like(a, rough)}


def sp_blade(rng, length, width, ang, col_base, col_tip, curve=0.15, rough=0.55, lift=1.0):
    """A grass blade seen from above: from its base (thick, dark, high in the clump) to a fine light tip, curving."""
    r = int(length * 0.55 + width + 3)
    x, y = grid(2 * r + 1, 2 * r + 1)
    u, v = rot(x, y, ang)
    s = np.clip(u / length + 0.5, 0, 1)
    vv = v - curve * length * s * s
    half = width * 0.5 * (1.0 - s ** 1.4) + 0.35
    inside = (np.abs(u) <= length * 0.5)
    a = edge(half - np.abs(vv), 0.7) * inside
    ridge = np.sqrt(np.clip(1.0 - (vv / np.maximum(half, 0.4)) ** 2, 0, 1))
    h = ((1.0 - s) * 4.0 * lift + ridge * width * 0.35) * a
    t = s[..., None]
    rgb = np.asarray(col_base) * (1 - t) + np.asarray(col_tip) * t
    rgb = rgb * (0.88 + 0.2 * ridge)[..., None]
    return {"a": a, "rgb": np.clip(rgb, 0, 1), "h": h, "rough": np.full_like(a, rough)}


def sp_root(rng, length, radius, ang, col, rough=0.75):
    """An exposed root snaking over the ground (a fat twig with a wave), half sunk."""
    s = sp_twig(rng, length, radius, ang, col, bend=0.18, rough=rough)
    s["h"] = s["h"] * 0.7
    return s


# --- helpers ------------------------------------------------------------------------------------------------------
def blur_wrap(a, r):
    """Box blur with wrap-around, r pixels (three passes are near Gaussian)."""
    out = a.copy()
    for _ in range(3):
        acc = np.zeros_like(out)
        for k in range(-r, r + 1):
            acc += np.roll(out, k, axis=0)
        out = acc / (2 * r + 1)
        acc = np.zeros_like(out)
        for k in range(-r, r + 1):
            acc += np.roll(out, k, axis=1)
        out = acc / (2 * r + 1)
    return out


def contact_ao(c, strength=0.35, r=3):
    """Soft darkening round what lies on the ground (its contact shadow), none on top of it."""
    halo = blur_wrap(c.cover, r)
    return 1.0 - strength * np.clip(halo - c.cover, 0, 1) * 2.0


def cavity(h, r=6, strength=0.04):
    """AO from the height: hollows (below their surroundings) darker."""
    return np.clip(1.0 - strength * np.clip(blur_wrap(h, r) - h, 0, None), 0.0, 1.0)


def finish(c, name, normal_strength, ao_extra=None, height_norm=None):
    rgb = np.clip(c.rgb, 0, 1)
    h = c.h
    n = height_to_normal(h, normal_strength)
    ao = contact_ao(c) * cavity(h)
    if ao_extra is not None:
        ao = ao * ao_extra
    hn = height_norm if height_norm is not None else n01(blur_wrap(h, 1))
    save_rgb(os.path.join(OUT, "T_Isl%s_D.png" % name), rgb)
    save_normal(os.path.join(OUT, "T_Isl%s_N.png" % name), n)
    save_rgb(os.path.join(OUT, "T_Isl%s_ORM.png" % name), np.stack([np.clip(ao, 0, 1), np.clip(c.rough, 0, 1), hn], -1))
    lin = np.where(rgb <= 0.04045, rgb / 12.92, ((rgb + 0.055) / 1.055) ** 2.4)
    print("SB_ISL_TEX", name, "srgb mean", rgb.mean((0, 1)).round(3), "lin mean", lin.mean((0, 1)).round(4),
          "rough mean", round(float(c.rough.mean()), 3))
    return rgb, n, ao, hn


def scatter_points(rng, n, size, density=None):
    """n points over the tile, kept with the probability density (an array 0..1) if given."""
    pts = []
    while len(pts) < n:
        x, y = rng.uniform(0, size, 2)
        if density is None or rng.random() < density[int(y) % size, int(x) % size]:
            pts.append((x, y))
    return pts


# --- the sets -----------------------------------------------------------------------------------------------------
def sand():
    """Beach sand, tile 2 m (1 px ~ 1 mm): broad tone patches, micro wind ripples in patches with dark heavy grains in
    their troughs and pale crests, crisp grains, shell bits, a few small cockles, pebbles and coral crumbs."""
    rng = np.random.default_rng(7101)
    n = SIZE
    base = np.array([0.80, 0.745, 0.64])
    c = Canvas(n, base, 0.9)
    tone = sum(crisp(n01(spectral_noise(n, 2, 8, -2.0, seed=7102)), t, 0.05) for t in (0.4, 0.62)) / 2.0
    warm = crisp(n01(spectral_noise(n, 2, 7, -1.9, seed=7103)), 0.55, 0.08)
    mott = spectral_noise(n, 10, 40, -1.4, seed=7104)
    k = (0.95 + 0.07 * (tone - 0.5) + 0.02 * mott)
    c.rgb = c.rgb * k[..., None]
    c.rgb[..., 0] *= 1.0 + 0.03 * warm
    c.rgb[..., 2] *= 1.0 - 0.05 * warm
    # Micro ripples (6-9 cm), in patches; asymmetric (gentle windward side, steep lee).
    y, x = np.mgrid[0:n, 0:n] / n
    w1 = spectral_noise(n, 1, 4, -2.0, seed=7105)
    w2 = spectral_noise(n, 2, 7, -1.8, seed=7106)
    phase = 2 * np.pi * (26 * x + 1.3 * w1 + 0.3 * np.sin(2 * np.pi * (2 * y + w2)) + 0.8 * w2)
    saw = (phase / (2 * np.pi)) % 1.0
    prof = np.where(saw < 0.72, saw / 0.72, (1.0 - saw) / 0.28)            # rising slowly, dropping fast
    prof = prof * prof * (3 - 2 * prof)
    patch = crisp(n01(spectral_noise(n, 1, 5, -1.9, seed=7107) + 0.3 * spectral_noise(n, 5, 14, -1.5, seed=7108)), 0.6, 0.08)
    rip_h = prof * patch * 2.6
    trough = (1.0 - prof) ** 3 * patch
    crest = np.clip((prof - 0.8) / 0.2, 0, 1) * patch
    c.h += rip_h
    # Heavy dark grains gather in the troughs; pale quartz on the crests.
    dark_g = (rng.random((n, n)) < 0.06 + 0.35 * trough) & (rng.random((n, n)) < 0.5)
    c.rgb = c.rgb * (1.0 - 0.32 * dark_g * (0.4 + 0.6 * trough))[..., None]
    c.rgb = c.rgb * (1.0 + 0.06 * crest)[..., None]
    # Grains: crisp light and dark specks, some 2 px.
    gl = crisp(n01(spectral_noise(n, 300, 900, -0.6, seed=7109)), 0.86, 0.02)
    gd = crisp(n01(spectral_noise(n, 300, 900, -0.6, seed=7110)), 0.88, 0.02)
    gb = crisp(n01(spectral_noise(n, 120, 360, -0.8, seed=7111)), 0.9, 0.02)
    c.rgb = c.rgb * (1.0 + 0.1 * gl - 0.16 * gd - 0.1 * gb)[..., None]
    c.h += 0.35 * gl + 0.45 * gb + 0.6 * spectral_noise(n, 80, 300, -1.0, seed=7112)
    # Shell bits, cockles, pebbles and coral crumbs (sparser in the ripple patches, where the wind sorted the sand).
    keep = 1.0 - 0.6 * patch
    shell_cols = [(0.93, 0.9, 0.84), (0.95, 0.86, 0.8), (0.88, 0.82, 0.72), (0.9, 0.78, 0.7), (0.8, 0.74, 0.66)]
    for (px, py) in scatter_points(rng, 800, n, keep):
        col = shell_cols[rng.integers(len(shell_cols))]
        c.stamp(px, py, sp_shell_bit(rng, rng.uniform(3.5, 12), rng.uniform(0, 6.3), col, rough=0.5), mode="press")
    for (px, py) in scatter_points(rng, 26, n, keep):
        col = shell_cols[rng.integers(len(shell_cols))]
        c.stamp(px, py, sp_cockle(rng, rng.uniform(9, 15), rng.uniform(0, 6.3), shade(col, 1.02), rough=0.42), mode="pile", settle=0.3)
    peb_cols = [(0.55, 0.52, 0.48), (0.62, 0.57, 0.5), (0.42, 0.4, 0.38), (0.68, 0.6, 0.5), (0.5, 0.44, 0.38)]
    for (px, py) in scatter_points(rng, 70, n, keep):
        rx = rng.uniform(3, 8)
        c.stamp(px, py, sp_pebble(rng, rx, rx * rng.uniform(0.6, 0.95), rng.uniform(0, 6.3), peb_cols[rng.integers(5)],
                                  rough=0.55, height=0.55), mode="pile", settle=0.4)
    for (px, py) in scatter_points(rng, 30, n, keep):
        c.stamp(px, py, sp_twig(rng, rng.uniform(10, 22), rng.uniform(1.6, 2.6), rng.uniform(0, 6.3), (0.95, 0.93, 0.88),
                                bend=0.12, rough=0.6, fork=rng.random() < 0.6), mode="pile", settle=0.3)
    c.rough = np.clip(c.rough + 0.04 * trough - 0.04 * crest, 0, 1)
    finish(c, "Sand", 0.9, height_norm=np.clip(0.25 + c.h / 14.0, 0, 1))


def grass():
    """Island grass under the lawn's blades (D78), tile 1.6 m (1 px ~ 0.8 mm). Owner: "change the green floor desing ... its
    horrible" (D71's set had clumps of blades radiating from their middles: from above they read as neon stars or pine needles).
    Now a calm thatch: thousands of fine blades lying combed along a slow flow (the wind's), dark old blades and a little earth
    between them, broad soft patches of deeper and drier green. No clumps, no flowers, muted colours: the 3D lawn is the grass,
    this is what shows between its blades and, far off, where the lawn ends."""
    rng = np.random.default_rng(7201)
    n = SIZE
    soil = np.array([0.15, 0.16, 0.08])
    c = Canvas(n, soil, 0.9)
    c.rgb = c.rgb * (0.85 + 0.3 * n01(spectral_noise(n, 4, 30, -1.5, seed=7202)))[..., None]
    c.h += 0.8 * spectral_noise(n, 6, 40, -1.6, seed=7203)
    lush = n01(spectral_noise(n, 1, 5, -2.0, seed=7204))
    dry = crisp(n01(spectral_noise(n, 1, 6, -1.9, seed=7205)), 0.68, 0.12)
    flow = spectral_noise(n, 1, 4, -2.0, seed=7206) * 1.6 + 0.6           # the combing direction (radians), slowly wandering
    greens = [((0.12, 0.2, 0.06), (0.3, 0.42, 0.13)), ((0.11, 0.19, 0.06), (0.27, 0.4, 0.12)),
              ((0.13, 0.21, 0.07), (0.33, 0.44, 0.15)), ((0.1, 0.17, 0.06), (0.24, 0.35, 0.11))]
    straw = ((0.2, 0.19, 0.09), (0.38, 0.36, 0.19))
    old = ((0.14, 0.15, 0.07), (0.25, 0.26, 0.13))
    # Three layers: old flattened blades, the thatch, a few fresher blades on top; each blade a thin dash along the flow.
    for layer, (count, lmin, lmax, wmin, wmax, lift) in enumerate(((7000, 30, 70, 3.0, 5.0, 0.3), (16000, 25, 60, 2.5, 4.5, 0.6),
                                                                    (5000, 20, 45, 2.2, 3.8, 0.9))):
        for _ in range(count):
            px, py = rng.uniform(0, n, 2)
            iy, ix = int(py) % n, int(px) % n
            ang = flow[iy, ix] + rng.normal(0.0, 0.35)
            if layer == 0:
                col_b, col_t = old
            elif dry[iy, ix] > 0.5 and rng.random() < 0.15 or rng.random() < 0.03:
                col_b, col_t = straw
            else:
                col_b, col_t = greens[rng.integers(len(greens))]
            k = rng.uniform(0.85, 1.12) * (0.9 + 0.2 * lush[iy, ix])
            c.stamp(px, py, sp_blade(rng, rng.uniform(lmin, lmax), rng.uniform(wmin, wmax), ang, shade(col_b, k), shade(col_t, k),
                                     curve=rng.uniform(-0.15, 0.15), rough=0.75, lift=lift), mode="pile", settle=0.6)
    c.rgb = c.rgb * (0.94 + 0.12 * lush)[..., None]
    c.rough = np.clip(c.rough + 0.1, 0, 1)                                 # thatch is matte (no sheen between the blades)
    finish(c, "Grass", 0.45, height_norm=np.clip(c.h / max(np.percentile(c.h, 99.5), 1e-3), 0, 1))


def litter():
    """The forest floor under the 3D leaves (D78), tile 2 m: dark humus, moss, roots, twigs, small broken leaf fragments and
    seed husks. Owner: "real leaves for the braun part" (D71 painted whole flat leaves here); the whole leaves are geometry now
    (SM_Litter_*), this keeps only what is too small to model and, far off, the browns of the leaves."""
    rng = np.random.default_rng(7301)
    n = SIZE
    earth = np.array([0.17, 0.12, 0.075])
    c = Canvas(n, earth, 0.92)
    soil = n01(spectral_noise(n, 3, 30, -1.6, seed=7302))
    c.rgb = c.rgb * (0.8 + 0.4 * soil)[..., None]
    c.h += 2.0 * spectral_noise(n, 3, 20, -1.8, seed=7303) + 0.4 * spectral_noise(n, 60, 300, -1.0, seed=7304)
    moss = crisp(n01(spectral_noise(n, 2, 9, -1.9, seed=7305) + 0.3 * spectral_noise(n, 10, 50, -1.4, seed=7306)), 0.7, 0.05)
    moss_col = np.array([0.18, 0.25, 0.08]) * (0.8 + 0.4 * n01(spectral_noise(n, 40, 200, -1.0, seed=7307)))[..., None]
    c.rgb = c.rgb * (1 - moss[..., None]) + moss_col * moss[..., None]
    c.h += moss * (1.5 + 0.8 * spectral_noise(n, 60, 250, -0.9, seed=7308))
    c.rough = c.rough * (1 - moss) + 0.8 * moss
    root_col = (0.33, 0.25, 0.17)
    for _ in range(9):
        px, py = rng.uniform(0, n, 2)
        ang = rng.uniform(0, 6.3)
        for seg in range(rng.integers(2, 4)):
            c.stamp(px, py, sp_root(rng, rng.uniform(140, 260), rng.uniform(7, 12), ang, shade(root_col, rng.uniform(0.85, 1.15))),
                    mode="pile", settle=0.4)
            px += math.cos(ang) * 120
            py += math.sin(ang) * 120
            ang += rng.uniform(-0.5, 0.5)
    frag_cols = [(0.25, 0.17, 0.095), (0.29, 0.2, 0.105), (0.22, 0.155, 0.09), (0.33, 0.24, 0.12), (0.3, 0.23, 0.11), (0.24, 0.22, 0.11)]
    away = 1.0 - 0.85 * moss
    # Broken leaf bits: small (6-22 px, 0.6-2 cm), torn, dense, under and between where the 3D leaves lie.
    for (px, py) in scatter_points(rng, 7000, n, away):
        col = frag_cols[rng.integers(len(frag_cols))]
        L = rng.uniform(8, 26)
        if rng.random() < 0.5:
            spr = sp_shell_bit(rng, L * 0.4, rng.uniform(0, 6.3), shade(col, rng.uniform(0.85, 1.1)), rough=0.8)
        else:                                               # torn slivers of leaves, long and thin
            spr = sp_leaf(rng, L, L * rng.uniform(0.25, 0.4), rng.uniform(0, 6.3), shade(col, rng.uniform(0.85, 1.1)), curl=0.1,
                          vein=0.05, rough=0.8, decay=0.3)
        c.stamp(px, py, spr, mode="pile", settle=0.6)
    for _ in range(40):
        px, py = rng.uniform(0, n, 2)
        c.stamp(px, py, sp_twig(rng, rng.uniform(40, 200), rng.uniform(2, 5), rng.uniform(0, 6.3), shade((0.32, 0.25, 0.18), rng.uniform(0.8, 1.2)),
                                bend=rng.uniform(0.02, 0.1), rough=0.85, fork=rng.random() < 0.5), mode="pile", settle=0.6)
    peb_cols = [(0.42, 0.4, 0.36), (0.5, 0.47, 0.42), (0.36, 0.33, 0.3)]
    for _ in range(30):
        px, py = rng.uniform(0, n, 2)
        rx = rng.uniform(5, 12)
        c.stamp(px, py, sp_pebble(rng, rx, rx * rng.uniform(0.6, 0.9), rng.uniform(0, 6.3), peb_cols[rng.integers(3)], rough=0.6),
                mode="pile", settle=0.3)
    for _ in range(40):                                     # husks and seed pods
        px, py = rng.uniform(0, n, 2)
        c.stamp(px, py, sp_pebble(rng, rng.uniform(5, 9), rng.uniform(3, 5), rng.uniform(0, 6.3), (0.34, 0.22, 0.11), rough=0.6, height=0.7),
                mode="pile", settle=0.8)
    finish(c, "Litter", 0.6, height_norm=np.clip((c.h - np.percentile(c.h, 1)) / max(np.percentile(c.h, 99.5) - np.percentile(c.h, 1), 1e-3), 0, 1))


def earth():
    """The footpath, tile 2 m: sandy earth packed hard by feet (smoother, a little glossier), small stones pressed flush,
    loose pebbles at the sides, fine cracks in drier patches, a few leaves and twigs blown on to it."""
    rng = np.random.default_rng(7401)
    n = SIZE
    base = np.array([0.72, 0.64, 0.52])
    c = Canvas(n, base, 0.8)
    tone = n01(spectral_noise(n, 2, 10, -1.9, seed=7402))
    darker = crisp(n01(spectral_noise(n, 2, 8, -1.8, seed=7403) + 0.25 * spectral_noise(n, 8, 30, -1.5, seed=7410)), 0.6, 0.025)
    c.rgb = c.rgb * (0.9 + 0.15 * tone)[..., None]
    c.rgb = c.rgb * (1 - 0.18 * darker)[..., None]
    c.rgb[..., 2] *= 1.0 - 0.06 * darker
    c.h += 1.5 * spectral_noise(n, 3, 25, -1.7, seed=7404) + 0.3 * spectral_noise(n, 80, 300, -0.9, seed=7405)
    gd = crisp(n01(spectral_noise(n, 250, 800, -0.6, seed=7406)), 0.87, 0.02)
    gl = crisp(n01(spectral_noise(n, 250, 800, -0.6, seed=7407)), 0.88, 0.02)
    c.rgb = c.rgb * (1 - 0.15 * gd + 0.08 * gl)[..., None]
    # Fine cracks in the dry, packed patches.
    web = spectral_noise(n, 8, 30, -1.4, seed=7408)
    web = web / (np.abs(web).mean() + 1e-9)
    crack = np.clip(1.0 - np.abs(web) / 0.05, 0, 1) * crisp(n01(spectral_noise(n, 1, 5, -2.0, seed=7409)), 0.72, 0.05)
    c.rgb = c.rgb * (1 - 0.3 * crack)[..., None]
    c.h -= crack * 1.5
    c.rough = c.rough - 0.08 * (1 - darker) + 0.1 * crack
    stone_cols = [(0.58, 0.55, 0.5), (0.5, 0.47, 0.43), (0.66, 0.6, 0.52), (0.45, 0.43, 0.4), (0.62, 0.52, 0.42)]
    for _ in range(90):                                     # stones pressed flush by feet (flat, worn tops)
        px, py = rng.uniform(0, n, 2)
        rx = rng.uniform(8, 22)
        c.stamp(px, py, sp_pebble(rng, rx, rx * rng.uniform(0.55, 0.9), rng.uniform(0, 6.3), stone_cols[rng.integers(5)],
                                  rough=0.55, height=0.35, flat=0.55), mode="press")
    for _ in range(160):
        px, py = rng.uniform(0, n, 2)
        rx = rng.uniform(3, 7)
        c.stamp(px, py, sp_pebble(rng, rx, rx * rng.uniform(0.6, 0.95), rng.uniform(0, 6.3), stone_cols[rng.integers(5)], rough=0.6),
                mode="pile", settle=0.5)
    for _ in range(160):                                    # D78: small broken leaf bits (whole leaves are geometry)
        px, py = rng.uniform(0, n, 2)
        col = [(0.45, 0.3, 0.15), (0.55, 0.42, 0.2), (0.32, 0.22, 0.12)][rng.integers(3)]
        c.stamp(px, py, sp_shell_bit(rng, rng.uniform(4, 10), rng.uniform(0, 6.3), col, rough=0.75), mode="pile", settle=0.5)
    for _ in range(8):
        px, py = rng.uniform(0, n, 2)
        c.stamp(px, py, sp_twig(rng, rng.uniform(50, 140), rng.uniform(2.5, 4), rng.uniform(0, 6.3), (0.38, 0.3, 0.22), rough=0.8),
                mode="pile", settle=0.6)
    for _ in range(60):
        px, py = rng.uniform(0, n, 2)
        c.stamp(px, py, sp_shell_bit(rng, rng.uniform(3, 7), rng.uniform(0, 6.3), (0.9, 0.86, 0.78)), mode="press")
    finish(c, "Earth", 0.8, height_norm=np.clip(0.35 + c.h / 16.0, 0, 1))


# --- the plants' surfaces (build_vegetation.py's UVs: bark U round the stem in whole tiles of about 1 m, V along it in
# metres; a leaf's U across it 0..1, V from its base 0 to its tip 1) ---------------------------------------------------
def save_detail(name, rgb, h, rough, ao, strength):
    """Plant detail maps: _D albedo modulation (0.5 = no change, the material doubles it), _N normal, _ORM (AO, rough, h)."""
    save_rgb(os.path.join(OUT, "T_Isl%s_D.png" % name), np.clip(rgb, 0, 1))
    save_normal(os.path.join(OUT, "T_Isl%s_N.png" % name), height_to_normal(h, strength))
    save_rgb(os.path.join(OUT, "T_Isl%s_ORM.png" % name), np.stack([np.clip(ao, 0, 1), np.clip(rough, 0, 1), n01(h)], -1))
    print("SB_ISL_TEX", name, "mean", np.clip(rgb, 0, 1).mean((0, 1)).round(3))


def bark_palm():
    """A coconut palm's trunk, 1 m along (V) by one tile round (U), 1024 px: five ring scars (every 0.2 m, as the mesh's
    rings), each a crisp, slightly wavy groove under a rounded ledge, broken in places; between them grey fibrous bark
    with fine vertical cracks, darker weathered streaks running down from the scars and a few pale lichen spots."""
    n = 1024
    rng = np.random.default_rng(7501)
    v, u = np.mgrid[0:n, 0:n] / n                    # image rows = V (along the trunk), columns = U (round it)
    wav = spectral_noise(n, 1, 6, -2.0, seed=7502, aniso=(1.0, 8.0)) * 0.012
    ring = (v * 5.0 + wav * 5.0) % 1.0               # 0 at a scar, rising up the ring
    scar = np.clip(1.0 - np.minimum(ring, 1.0 - ring) / 0.035, 0, 1)
    gaps = crisp(n01(spectral_noise(n, 2, 12, -1.6, seed=7503, aniso=(1.0, 6.0))), 0.22, 0.05)
    scar = scar * gaps
    ledge = np.clip(1.0 - ring / 0.25, 0, 1) ** 2   # the ring's lower rim stands out just above the scar
    swell = np.sin(np.pi * ring) ** 0.6
    fib = spectral_noise_dir(n, 20, 160, -1.1, 7504, 90.0, 12.0)
    cracks = np.clip(1.0 - np.abs(spectral_noise_dir(n, 6, 40, -1.4, 7505, 90.0, 9.0)) / 0.05, 0, 1)
    cracks = cracks * crisp(n01(spectral_noise(n, 2, 10, -1.7, seed=7506)), 0.45, 0.1)
    streak = n01(spectral_noise_dir(n, 3, 30, -1.6, 7507, 90.0, 10.0)) * (1.0 - ring) ** 0.5
    lichen = crisp(n01(spectral_noise(n, 8, 40, -1.4, seed=7508)), 0.86, 0.02)
    base = 0.5 + 0.06 * fib - 0.16 * scar - 0.1 * cracks - 0.08 * streak + 0.05 * ledge + 0.03 * swell
    rgb = np.stack([base, base * 0.98, base * 0.94], -1)
    rgb = rgb * (1 - lichen[..., None]) + np.array([0.68, 0.7, 0.62]) * lichen[..., None]
    h = swell * 3.0 + ledge * 3.5 - scar * 6.0 - cracks * 1.5 + fib * 0.6 + lichen * 0.6
    rough = 0.82 + 0.08 * scar - 0.06 * ledge
    ao = 1.0 - 0.35 * scar - 0.15 * cracks
    save_detail("BarkPalm", rgb, h, rough, ao, 1.6)


def bark_tree():
    """Broadleaf bark, 1 m by one tile round, 1024 px: long plates split by furrows running up the stem (stretched Voronoi),
    flat-topped with crisp edges, fine cross-cracks on the plates, a few knot scars (eyes) and lichen patches."""
    n = 1024
    rng = np.random.default_rng(7601)
    warp = (spectral_noise(n, 1, 5, -1.8, seed=7602) * 0.03, spectral_noise(n, 1, 5, -1.8, seed=7603) * 0.02)
    v, u = np.mgrid[0:n, 0:n] / n
    # Plates: Voronoi on a grid stretched along V (cells 9 round by 3 along).
    cx, cy = 9, 3
    jit = rng.uniform(0, 1, (cy, cx, 2))
    x, y = (u + warp[0]) * cx, (v + warp[1]) * cy
    ix, iy = np.floor(x).astype(int), np.floor(y).astype(int)
    f1 = np.full(x.shape, 9.0)
    f2 = np.full(x.shape, 9.0)
    for oy in (-2, -1, 0, 1, 2):
        for ox in (-1, 0, 1):
            j = jit[(iy + oy) % cy, (ix + ox) % cx]
            d = np.sqrt(((x - ix - ox - j[..., 0])) ** 2 + ((y - iy - oy - j[..., 1]) * 0.35) ** 2)
            f2 = np.where(d < f1, f1, np.minimum(f2, d))
            f1 = np.minimum(f1, d)
    furrow = np.clip(1.0 - (f2 - f1) / 0.09, 0, 1)
    plate = 1.0 - furrow
    cross = np.clip(1.0 - np.abs(spectral_noise_dir(n, 10, 60, -1.3, 7604, 0.0, 6.0)) / 0.04, 0, 1) * plate
    cross = cross * crisp(n01(spectral_noise(n, 3, 14, -1.6, seed=7605)), 0.5, 0.1)
    grain = spectral_noise_dir(n, 30, 200, -1.0, 7606, 90.0, 8.0)
    knots = np.zeros((n, n))
    for _ in range(3):
        kx, ky = rng.uniform(0, 1, 2)
        ddx = ((u - kx + 0.5) % 1.0) - 0.5
        ddy = ((v - ky + 0.5) % 1.0) - 0.5
        d = np.sqrt((ddx / 0.03) ** 2 + (ddy / 0.05) ** 2)
        knots = np.maximum(knots, np.clip(1.0 - np.abs(d - 0.8) / 0.25, 0, 1))
    lichen = crisp(n01(spectral_noise(n, 4, 24, -1.5, seed=7607) + 0.3 * spectral_noise(n, 30, 120, -1.0, seed=7608)), 0.8, 0.02)
    base = 0.5 + 0.05 * grain - 0.22 * furrow - 0.08 * cross - 0.12 * knots + 0.04 * n01(spectral_noise(n, 2, 10, -1.8, seed=7609))
    rgb = np.stack([base, base * 0.97, base * 0.92], -1)
    rgb = rgb * (1 - 0.7 * lichen[..., None]) + np.array([0.6, 0.62, 0.52]) * 0.7 * lichen[..., None]
    h = plate * 6.0 - cross * 1.5 + grain * 0.6 + knots * 2.0 + lichen * 0.5
    save_detail("BarkTree", rgb, h, 0.86 - 0.08 * plate + 0.1 * furrow, 1.0 - 0.4 * furrow - 0.15 * cross, 1.4)


def leaf_broad():
    """A broad leaf's detail (512 across by 1024 along): a raised pale midrib, eight pairs of side veins curving towards
    the tip, the blade a little puffed between them, a lighter rim and a few tiny spots."""
    w, n = 512, 1024
    v, u = np.mgrid[0:n, 0:w].astype(float)
    v, u = v / n, u / w
    s = np.abs(u - 0.5) * 2.0                          # 0 on the midrib .. 1 at the edge
    rib = np.clip(1.0 - np.abs(u - 0.5) / 0.012, 0, 1)
    phase = (v * 8.5 - s * 1.5 - 0.15 * s * s) % 1.0     # side veins leaving the midrib, curving up to the tip
    vein = np.clip(1.0 - np.minimum(phase, 1.0 - phase) / 0.03, 0, 1) * (s > 0.03) * (s < 0.93)
    puff = np.sin(np.pi * phase) ** 0.7
    rim = np.clip((s - 0.9) / 0.1, 0, 1)
    spots = crisp(n01(spectral_noise(n, 40, 120, -1.0, seed=7701)[:, :w]), 0.94, 0.02)
    m = 0.5 + 0.1 * rib + 0.05 * vein - 0.04 * (1 - puff) + 0.05 * rim - 0.08 * spots
    rgb = np.stack([m, m, m], -1)
    h = rib * 2.0 - vein * 1.2 + puff * 1.0 - s * 1.5
    save_detail("LeafBroad", rgb, h, 0.5 - 0.1 * rib + 0.05 * vein, 1.0 - 0.15 * vein, 2.0)


def leaf_palm():
    """A palm leaflet's detail (512 across by 1024 along): the raised midrib, parallel veins along it, a fine sheen
    between them, the very tip dried."""
    w, n = 512, 1024
    v, u = np.mgrid[0:n, 0:w].astype(float)
    v, u = v / n, u / w
    rib = np.clip(1.0 - np.abs(u - 0.5) / 0.02, 0, 1)
    par = 0.5 + 0.5 * np.cos(2 * np.pi * u * 9.0)
    tip = np.clip((v - 0.9) / 0.1, 0, 1)
    m = 0.5 + 0.1 * rib + 0.04 * par - 0.12 * tip
    rgb = np.stack([m + 0.06 * tip, m + 0.02 * tip, m - 0.06 * tip], -1)
    h = rib * 2.5 + par * 0.6
    save_detail("LeafPalm", rgb, h, 0.45 - 0.08 * rib + 0.2 * tip, 1.0 - 0.1 * (1 - par), 2.0)


def preview(names):
    """A sheet: each set's albedo (whole tile, then a 1:1 crop) and its normal map lit from the upper left."""
    tiles = []
    for name in names:
        d = np.asarray(Image.open(os.path.join(OUT, "T_Isl%s_D.png" % name)).convert("RGB")).astype(float) / 255.0
        nm = np.asarray(Image.open(os.path.join(OUT, "T_Isl%s_N.png" % name)).convert("RGB")).astype(float) / 255.0 * 2 - 1
        light = np.array([-0.5, -0.5, 0.7])
        light /= np.linalg.norm(light)
        lit = np.clip((nm * light).sum(-1), 0, 1)[..., None]
        shaded = np.clip(d * (0.35 + 0.9 * lit), 0, 1)
        whole = Image.fromarray((np.clip(shaded * (1.0 if d.mean() > 0.3 else 1.0), 0, 1) * 255).astype(np.uint8)).resize((512, 512), Image.LANCZOS)
        crop = Image.fromarray((shaded[:512, :512] * 255).astype(np.uint8))
        orm = np.asarray(Image.open(os.path.join(OUT, "T_Isl%s_ORM.png" % name)).convert("RGB"))
        rough = Image.fromarray(orm[:512, :512, 1]).convert("RGB")
        height = Image.fromarray(orm[..., 2]).convert("RGB").resize((512, 512))
        row = Image.new("RGB", (2048, 512))
        for k, im in enumerate((whole, crop, rough, height)):
            row.paste(im, (k * 512, 0))
        tiles.append(row)
    sheet = Image.new("RGB", (2048, 512 * len(tiles)))
    for k, t in enumerate(tiles):
        sheet.paste(t, (0, k * 512))
    path = os.path.join(os.environ.get("TEMP", OUT), "isl_tex_preview.png")
    sheet.save(path)
    print("SB_ISL_PREVIEW", path)


SETS = {"sand": sand, "grass": grass, "litter": litter, "earth": earth, "barkpalm": bark_palm, "barktree": bark_tree,
        "leafbroad": leaf_broad, "leafpalm": leaf_palm}

if __name__ == "__main__":
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    wanted = args or list(SETS)
    os.makedirs(OUT, exist_ok=True)
    for k in wanted:
        SETS[k]()
    if "--preview" in sys.argv:
        names = {"barkpalm": "BarkPalm", "barktree": "BarkTree", "leafbroad": "LeafBroad", "leafpalm": "LeafPalm"}
        preview([names.get(k, k.capitalize()) for k in wanted])
