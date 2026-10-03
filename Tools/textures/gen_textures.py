"""Procedural, tileable source textures for Saltbound (original content, no external licenses).

Run with the host Python (numpy + Pillow):  python Tools/textures/gen_textures.py [name ...]
Writes PNGs to Art/Textures/<Group>/. Deterministic (fixed seeds). Import with
Tools/ue_scripts/import_textures.py. Suffix convention: _D albedo (sRGB), _N normal, _M / _ORM masks (linear).
"""
import os
import sys

import numpy as np
from PIL import Image

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
OUT = os.path.join(ROOT, "Art", "Textures")


# --- tileable noise helpers --------------------------------------------------------------

def spectral_noise(size, k_min, k_max, slope=-2.0, seed=0, aniso=(1.0, 1.0)):
    """Periodic noise from a band-limited random spectrum (tiles perfectly). Returns [-1, 1]."""
    rng = np.random.default_rng(seed)
    fx = np.fft.fftfreq(size) * size
    kx, ky = np.meshgrid(fx * aniso[0], fx * aniso[1])
    k = np.sqrt(kx ** 2 + ky ** 2)
    amp = np.where((k >= k_min) & (k <= k_max), np.power(np.maximum(k, 1e-6), slope), 0.0)
    phase = rng.uniform(0, 2 * np.pi, (size, size))
    spec = amp * np.exp(1j * phase)
    field = np.real(np.fft.ifft2(spec))
    field -= field.mean()
    return field / (np.abs(field).max() + 1e-9)


def worley(size, cells, seed=0):
    """Periodic Worley (F1) distance field in [0, 1]."""
    rng = np.random.default_rng(seed)
    pts = rng.uniform(0, 1, (cells * cells, 2))
    grid = (np.stack(np.meshgrid(np.arange(cells), np.arange(cells)), -1).reshape(-1, 2) + pts) / cells
    y, x = np.mgrid[0:size, 0:size] / size
    best = np.full((size, size), 10.0)
    for ox in (-1, 0, 1):
        for oy in (-1, 0, 1):
            for p in grid:
                d = np.sqrt((x - (p[0] + ox)) ** 2 + (y - (p[1] + oy)) ** 2)
                np.minimum(best, d, out=best)
    return best / best.max()


def height_to_normal(h, strength):
    """Tangent-space normal map (DirectX convention, +Y down the image) from a periodic height field."""
    dx = (np.roll(h, -1, axis=1) - np.roll(h, 1, axis=1)) * 0.5
    dy = (np.roll(h, -1, axis=0) - np.roll(h, 1, axis=0)) * 0.5
    n = np.stack([-dx * strength, -dy * strength, np.ones_like(h)], -1)
    n /= np.linalg.norm(n, axis=-1, keepdims=True)
    return n


def save_rgb(path, rgb01):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    img = (np.clip(rgb01, 0, 1) * 255 + 0.5).astype(np.uint8)
    Image.fromarray(img, "RGB").save(path)
    print("SB_TEXTURE", path, img.shape)


def save_normal(path, n):
    save_rgb(path, n * 0.5 + 0.5)


# --- textures --------------------------------------------------------------------------

def ocean_ripples():
    """Flowing wind ripples (U = along the wind): ridged, domain-warped noise at mid frequencies only.
    Crests are sharp and run across the wind; there is no fine noise, so the sun breaks into glints that
    follow the ripple shapes instead of a grainy glitter (owner reference, 2026-10-03)."""
    size = 1024
    wx = spectral_noise(size, 1, 6, -1.8, seed=14) * 0.04
    wy = spectral_noise(size, 1, 6, -1.8, seed=15) * 0.04
    iy, ix = np.mgrid[0:size, 0:size].astype(float)
    def warped(field):
        # Bilinear, periodic lookup (integer offsets leave hatching along the ridges).
        fy, fx = iy + wy * size, ix + wx * size
        y0, x0 = np.floor(fy).astype(int), np.floor(fx).astype(int)
        ty, tx = fy - y0, fx - x0
        y0 %= size; x0 %= size
        y1, x1 = (y0 + 1) % size, (x0 + 1) % size
        return (field[y0, x0] * (1 - tx) * (1 - ty) + field[y0, x1] * tx * (1 - ty)
                + field[y1, x0] * (1 - tx) * ty + field[y1, x1] * tx * ty)
    n1 = warped(spectral_noise(size, 6, 28, -1.6, seed=11, aniso=(1.0, 2.6)))
    n2 = warped(spectral_noise(size, 14, 48, -1.4, seed=12, aniso=(1.0, 2.0)))
    ridge1 = (1.0 - np.abs(n1)) ** 3
    ridge2 = (1.0 - np.abs(n2)) ** 3
    swell = spectral_noise(size, 2, 10, -1.8, seed=13, aniso=(1.0, 1.6))
    h = ridge1 * 0.6 + ridge2 * 0.25 + swell * 0.35
    save_normal(os.path.join(OUT, "Water", "T_OceanRipples_N.png"), height_to_normal(h, 12.0))


def worley_grid(size, cells_x, cells_y=None, seed=0, warp=None):
    """Periodic Worley F1/F2 on a jittered grid (3x3 neighbourhood, O(9 N)). Distances in cell units.

    warp: optional (wx, wy) periodic offset fields in UV units (domain warping keeps the tiling)."""
    cells_y = cells_y or cells_x
    rng = np.random.default_rng(seed)
    jit = rng.uniform(0.0, 1.0, (cells_y, cells_x, 2))
    v, u = np.mgrid[0:size, 0:size] / size
    if warp is not None:
        u = u + warp[0]
        v = v + warp[1]
    x, y = u * cells_x, v * cells_y
    ix, iy = np.floor(x).astype(int), np.floor(y).astype(int)
    f1 = np.full(x.shape, 9.0)
    f2 = np.full(x.shape, 9.0)
    for oy in (-1, 0, 1):
        for ox in (-1, 0, 1):
            cx, cy = ix + ox, iy + oy
            j = jit[cy % cells_y, cx % cells_x]
            d = np.sqrt((x - cx - j[..., 0]) ** 2 + (y - cy - j[..., 1]) ** 2)
            f2 = np.where(d < f1, f1, np.minimum(f2, d))
            f1 = np.minimum(f1, d)
    return f1, f2


def ocean_foam():
    """Foam pattern for M_Ocean (U runs along the dominant wave direction).

    R  graded lace: 1 on the foam filaments of a warped two-scale cell network, falling off into the cell
       interiors. The material thresholds it by foam coverage: little coverage leaves thin lace, full
       coverage fills the cells.
    G  bubble holes (dots) punched into dense foam.
    B  low-frequency patchiness (where residual foam gathers on the open sea)."""
    size = 1024
    warp_a = (spectral_noise(size, 1, 6, -1.8, seed=24) * 0.045, spectral_noise(size, 1, 6, -1.8, seed=25) * 0.045)
    warp_b = (spectral_noise(size, 2, 12, -1.6, seed=26) * 0.02, spectral_noise(size, 2, 12, -1.6, seed=27) * 0.02)
    # Filaments are stretched along U (wind / wave direction): fewer cells along U than across.
    f1, f2 = worley_grid(size, 5, 8, seed=21, warp=warp_a)
    big = np.clip(1.0 - (f2 - f1) / 0.55, 0, 1) ** 1.6
    f1, f2 = worley_grid(size, 11, 17, seed=22, warp=warp_b)
    small = np.clip(1.0 - (f2 - f1) / 0.45, 0, 1) ** 1.8
    grain = spectral_noise(size, 40, 200, -1.0, seed=28)
    # Uneven filament strength: thin coverage breaks the lace into fragments instead of a closed net.
    strength = 0.62 + 0.38 * n01(spectral_noise(size, 3, 24, -1.5, seed=31))
    lace = np.clip(np.maximum(big, small * 0.85) * strength + 0.06 * grain, 0, 1)
    f1, _ = worley_grid(size, 56, 56, seed=29)
    radius = 0.18 + 0.2 * n01(spectral_noise(size, 4, 30, -1.4, seed=30))
    holes = np.clip((radius - f1) / 0.06, 0, 1)
    patches = n01(spectral_noise(size, 1, 5, -2.0, seed=23))
    save_rgb(os.path.join(OUT, "Water", "T_OceanFoam_M.png"), np.stack([lace, holes, patches], -1))


def worley_f2f1(size, cells, seed=0):
    """Periodic Worley F2-F1 (cell borders = small values) in [0, 1]."""
    rng = np.random.default_rng(seed)
    pts = rng.uniform(0, 1, (cells * cells, 2))
    grid = (np.stack(np.meshgrid(np.arange(cells), np.arange(cells)), -1).reshape(-1, 2) + pts) / cells
    y, x = np.mgrid[0:size, 0:size] / size
    f1 = np.full((size, size), 10.0)
    f2 = np.full((size, size), 10.0)
    for ox in (-1, 0, 1):
        for oy in (-1, 0, 1):
            for p in grid:
                d = np.sqrt((x - (p[0] + ox)) ** 2 + (y - (p[1] + oy)) ** 2)
                f2 = np.where(d < f1, f1, np.minimum(f2, d))
                f1 = np.minimum(f1, d)
    e = f2 - f1
    return e / e.max()


def n01(a):
    return (a - a.min()) / (a.max() - a.min() + 1e-9)


def wood_field(size, seed=0, rings=90.0, line_w=0.07):
    """Shared straight-grain field (1 tile = 1 m, grain along U), Sea of Thieves deck reference:
    long, mostly straight grain lines of varying spacing and strength that wobble gently, only rarely a
    stretched arch, soft early/late wood bands, long fibre streaks, a few small knots.
    Returns latewood lines, knot cores, band value, fibre streaks and broad tone (all periodic)."""
    y, x = np.mgrid[0:size, 0:size] / size
    rng = np.random.default_rng(38 + seed)
    across = spectral_noise(size, 1, 6, -2.0, seed=31 + seed, aniso=(40.0, 1.0))          # varies with y only
    wobble = (spectral_noise(size, 1, 4, -2.0, seed=32 + seed, aniso=(1.0, 3.0)) * 0.012 +
              spectral_noise(size, 3, 12, -1.6, seed=33 + seed, aniso=(1.0, 4.0)) * 0.002)
    # Occasional long arch: a very deep pith, so the rings meet the surface as stretched curves.
    pith_y = 0.5 + spectral_noise(size, 1, 3, -2.0, seed=37 + seed, aniso=(1.0, 40.0)) * 0.1
    dy = ((y - pith_y + 0.5) % 1.0) - 0.5
    depth = 0.15 + 0.2 * n01(spectral_noise(size, 1, 3, -2.0, seed=39 + seed, aniso=(1.0, 40.0)))
    arch = np.sqrt(dy * dy + depth * depth) - depth
    knot_dark = np.zeros_like(x)
    knot_bend = np.zeros_like(x)
    for _ in range(2):
        kx, ky, kr = rng.uniform(0, 1), rng.uniform(0, 1), rng.uniform(0.006, 0.011)
        ddx = ((x - kx + 0.5) % 1.0) - 0.5
        ddy = ((y - ky + 0.5) % 1.0) - 0.5
        d = np.sqrt((ddx * 0.3) ** 2 + ddy ** 2)
        knot_bend += kr * 1.4 * np.exp(-(d / (kr * 3.5)) ** 2) * np.sign(ddy + 1e-9)
        knot_dark = np.maximum(knot_dark, np.clip(1.0 - d / kr, 0, 1) ** 0.6)
    pos = y + wobble + across * 0.02 + arch * 0.6 + knot_bend
    phase = pos * rings                                          # 90: ~1.1 cm between grain lines
    frac = phase - np.floor(phase)
    strength = 0.35 + 0.65 * n01(spectral_noise(size, 2, 20, -1.5, seed=36 + seed, aniso=(4.0, 1.0)))
    latewood = np.clip(1.0 - np.abs(frac - 0.85) / line_w, 0, 1) * strength       # thin crisp lines
    band = 0.5 + 0.5 * np.sin(2 * np.pi * pos * 22.0)                              # soft wide bands
    fibers = spectral_noise(size, 40, 500, -0.9, seed=34 + seed, aniso=(20.0, 1.0))
    broad = spectral_noise(size, 1, 6, -2.0, seed=35 + seed)
    return latewood, knot_dark, band, fibers, broad


def wood_grain():
    """Hand-painted straight-grain wood (tinted per material), 1 tile = 1 m at 2048 px, grain along U."""
    size = 2048
    latewood, knot_dark, band, fibers, broad = wood_field(size)
    albedo = (0.6 + 0.06 * broad + 0.035 * (band - 0.5) - 0.14 * latewood - 0.05 * np.abs(fibers)
              + 0.02 * fibers - 0.3 * knot_dark)
    rgb = np.stack([albedo * 1.00, albedo * 0.92, albedo * 0.82], -1)
    height = -latewood * 0.45 + fibers * 0.35 - knot_dark * 0.4 + band * 0.05
    save_rgb(os.path.join(OUT, "Wood", "T_WoodGrain_D.png"), rgb)
    save_normal(os.path.join(OUT, "Wood", "T_WoodGrain_N.png"), height_to_normal(height, 3.0))
    ao = 1.0 - 0.12 * latewood - 0.2 * knot_dark
    rough = np.clip(0.62 + 0.12 * latewood - 0.05 * broad + 0.04 * np.abs(fibers), 0, 1)
    save_rgb(os.path.join(OUT, "Wood", "T_WoodGrain_ORM.png"), np.stack([ao, rough, np.zeros_like(ao)], -1))


def wood_weathered():
    """Weathered dock timber (tinted per material, 1 tile = 1 m, 2048 px, grain along U), D20:
    broad simplified grain (wide soft bands ~3 cm apart, low contrast), long weathering streaks and
    sculpted grooves in the normal map, almost no fine grain, matte."""
    size = 2048
    latewood, knot_dark, band, fibers, broad = wood_field(size, seed=11, rings=32.0, line_w=0.17)
    streaks = spectral_noise_dir(size, 3, 30, -1.6, 120, 0.0, 12.0)
    blotch = spectral_noise(size, 1, 6, -2.0, seed=121)
    albedo = (0.6 + 0.05 * blotch + 0.035 * streaks - 0.06 * latewood + 0.015 * (band - 0.5)
              - 0.01 * np.abs(fibers) - 0.16 * knot_dark)
    rgb = np.stack([albedo, albedo * 0.96, albedo * 0.9], -1)
    save_rgb(os.path.join(OUT, "Wood", "T_WoodWeathered_D.png"), rgb)
    height = -latewood * 1.0 + streaks * 0.6 + band * 0.15 - knot_dark * 0.5 + fibers * 0.04
    save_normal(os.path.join(OUT, "Wood", "T_WoodWeathered_N.png"), height_to_normal(height, 4.0))
    ao = 1.0 - 0.1 * latewood - 0.15 * knot_dark
    rough = np.clip(0.86 + 0.04 * latewood + 0.03 * streaks, 0, 1)
    save_rgb(os.path.join(OUT, "Wood", "T_WoodWeathered_ORM.png"), np.stack([ao, rough, np.zeros_like(ao)], -1))


def paint_wear():
    """Masks for worn paint and grime: R chips (threshold in material), G streaks, B fine grime."""
    size = 1024
    chips = n01(spectral_noise(size, 3, 40, -1.3, seed=41) * 0.7 + (1 - worley(size, 18, seed=42)) * 0.3)
    streaks = n01(spectral_noise(size, 2, 60, -1.2, seed=43, aniso=(0.15, 1.0)))
    grime = n01(spectral_noise(size, 8, 160, -1.0, seed=44))
    save_rgb(os.path.join(OUT, "Wood", "T_PaintWear_M.png"), np.stack([chips, streaks, grime], -1))


def metal():
    """Subtle hammered/handled metal: normal + ORM (R AO, G roughness, B metal mask = 1)."""
    size = 512
    dents = spectral_noise(size, 6, 40, -1.8, seed=51)
    scratches = spectral_noise(size, 60, 220, -0.8, seed=52, aniso=(10.0, 1.0))
    height = dents * 0.8 + scratches * 0.08
    save_normal(os.path.join(OUT, "Metal", "T_Metal_N.png"), height_to_normal(height, 1.6))
    rough = np.clip(0.38 + 0.12 * n01(spectral_noise(size, 2, 20, -1.5, seed=53)) + 0.06 * scratches, 0, 1)
    save_rgb(os.path.join(OUT, "Metal", "T_Metal_ORM.png"), np.stack([np.ones_like(rough), rough, np.ones_like(rough)], -1))


def rope():
    """Three-strand twisted rope; U along the rope (tile = 8 strand crossings), V around it."""
    size = 512
    y, x = np.mgrid[0:size, 0:size] / size
    twist = np.sin(2 * np.pi * (x * 8 + y * 3))                # 3 strands around, 8 turns per tile
    strand = (0.5 + 0.5 * twist) ** 0.6
    fibers = spectral_noise(size, 30, 200, -1.0, seed=61, aniso=(1.0, 6.0))
    height = strand + fibers * 0.12
    albedo = 0.55 + 0.25 * strand - 0.06 * np.abs(fibers)
    save_rgb(os.path.join(OUT, "Fabric", "T_Rope_D.png"), np.stack([albedo, albedo * 0.9, albedo * 0.72], -1))
    save_normal(os.path.join(OUT, "Fabric", "T_Rope_N.png"), height_to_normal(height, 5.0))


def spectral_noise_dir(size, k_min, k_max, slope, seed, angle_deg, stretch):
    """Like spectral_noise, but anisotropic along an arbitrary direction: features are `stretch` times
    longer along angle_deg (image space, 0 = +U) than across it. Tiles perfectly."""
    rng = np.random.default_rng(seed)
    f = np.fft.fftfreq(size) * size
    kx, ky = np.meshgrid(f, f)
    a = np.radians(angle_deg)
    ku = kx * np.cos(a) + ky * np.sin(a)          # along the strokes
    kv = -kx * np.sin(a) + ky * np.cos(a)         # across
    k = np.sqrt((ku * stretch) ** 2 + kv ** 2)
    amp = np.where((k >= k_min) & (k <= k_max), np.power(np.maximum(k, 1e-6), slope), 0.0)
    field = np.real(np.fft.ifft2(amp * np.exp(1j * rng.uniform(0, 2 * np.pi, (size, size)))))
    field -= field.mean()
    return field / (np.abs(field).max() + 1e-9)


def worley_cells(size, cells, seed=0, warp=None):
    """Periodic Worley with the nearest cell's index: returns f1, f2 (cell units) and cell id (int)."""
    rng = np.random.default_rng(seed)
    jit = rng.uniform(0.0, 1.0, (cells, cells, 2))
    v, u = np.mgrid[0:size, 0:size] / size
    if warp is not None:
        u = u + warp[0]
        v = v + warp[1]
    x, y = u * cells, v * cells
    ix, iy = np.floor(x).astype(int), np.floor(y).astype(int)
    f1 = np.full(x.shape, 9.0)
    f2 = np.full(x.shape, 9.0)
    cid = np.zeros(x.shape, dtype=np.int64)
    for oy in (-1, 0, 1):
        for ox in (-1, 0, 1):
            cx, cy = (ix + ox) % cells, (iy + oy) % cells
            j = jit[cy, cx]
            d = np.sqrt((x - (ix + ox) - j[..., 0]) ** 2 + (y - (iy + oy) - j[..., 1]) ** 2)
            closer = d < f1
            f2 = np.where(closer, f1, np.minimum(f2, d))
            cid = np.where(closer, cy * cells + cx, cid)
            f1 = np.minimum(f1, d)
    return f1, f2, cid


def facet_normals(size, cells, seed, tilt, warp=None, bevel=0.06):
    """Chiselled stone: every Voronoi cell is a flat facet with a random tilt (crisp borders), the
    borders slightly rounded. Returns the tangent-space normal (x, y) slope field and an edge mask."""
    f1, f2, cid = worley_cells(size, cells, seed, warp)
    rng = np.random.default_rng(seed + 1)
    slopes = rng.normal(0, tilt, (cells * cells, 2))
    sx, sy = slopes[cid, 0], slopes[cid, 1]
    edge = np.clip(1.0 - (f2 - f1) / bevel, 0, 1)
    return sx * (1.0 - 0.6 * edge), sy * (1.0 - 0.6 * edge), edge


def crisp(x, edge, width=0.004):
    """Anti-aliased hard threshold (painted shapes keep crisp edges at full resolution)."""
    return np.clip((x - edge) / width + 0.5, 0, 1)


def rock():
    """Hand-painted stone, crisp (Sea of Thieves reference) for triplanar use: tile ~2.5 m at 2048 px.

    Flat tone patches with hard edges (posterised noise), a sparse network of painted cracks (dark groove
    line with a light lower lip), angular chips (Voronoi cells, lighter inside, lit upper edge and shaded
    lower edge), a few crisp specks and very little fine grain. Smooth areas stay calm in between, so
    the eye reads defined shapes instead of soft noise."""
    size = 2048
    y, x = np.mgrid[0:size, 0:size] / size
    warp = (spectral_noise(size, 1, 6, -1.8, seed=76) * 0.03, spectral_noise(size, 1, 6, -1.8, seed=77) * 0.03)
    # 1. Tone patches: 4 crisp levels plus a warm/cool patch layer.
    tone_src = n01(spectral_noise(size, 2, 10, -1.9, seed=71))
    levels = sum(crisp(tone_src, t, 0.006) for t in (0.3, 0.5, 0.7)) / 3.0
    hue_src = n01(spectral_noise(size, 2, 8, -1.8, seed=72))
    warm = crisp(hue_src, 0.62, 0.006)
    # 2. Cracks: thin segments of a warped Voronoi border network, partly hidden.
    f1, f2 = worley_grid(size, 3, 4, seed=73, warp=warp)
    e = f2 - f1
    show = crisp(n01(spectral_noise(size, 2, 9, -1.7, seed=74)), 0.6, 0.01)
    crack = crisp(0.018 - e, 0.0, 0.006) * show
    crack_lip = np.clip(np.roll(crack, 3, axis=0) - crack, 0, 1)       # light edge just below the groove
    # 3. Chips: a few angular cells, recessed with a lit top edge and a shaded bottom edge.
    f1c, f2c, cid = worley_cells(size, 26, seed=79, warp=warp)
    pick = (np.random.default_rng(80).random(26 * 26) < 0.04)[cid]
    chip = pick * crisp(f2c - f1c, 0.05, 0.02)
    chip_top = np.clip(chip - np.roll(chip, 4, axis=0), 0, 1)
    chip_bottom = np.clip(chip - np.roll(chip, -4, axis=0), 0, 1)
    # 4. Specks and grain.
    specks_l = crisp(n01(spectral_noise(size, 90, 300, -1.0, seed=81)), 0.9, 0.01)
    specks_d = crisp(n01(spectral_noise(size, 90, 300, -1.0, seed=82)), 0.91, 0.01)
    grain = spectral_noise(size, 200, 600, -0.8, seed=83)
    v = (0.5 + 0.07 * (levels - 0.5) - 0.03 * chip + 0.07 * chip_top - 0.06 * chip_bottom
         - 0.2 * crack + 0.06 * crack_lip + 0.035 * specks_l - 0.06 * specks_d + 0.008 * grain)
    rgb = np.stack([v * (1.0 + 0.04 * warm), v * (1.0 + 0.01 * warm), v * (1.0 - 0.04 * warm)], -1)
    save_rgb(os.path.join(OUT, "Rock", "T_Rock_D.png"), rgb)
    height = -crack * 1.0 - chip * 0.7 + spectral_noise(size, 2, 12, -1.8, seed=86) * 0.25
    save_normal(os.path.join(OUT, "Rock", "T_Rock_N.png"), height_to_normal(height, 6.0))
    ao = 1.0 - 0.45 * crack - 0.15 * chip_bottom
    rough = np.clip(0.82 - 0.06 * chip + 0.04 * (levels - 0.5), 0, 1)
    save_rgb(os.path.join(OUT, "Rock", "T_Rock_ORM.png"), np.stack([ao, rough, np.zeros_like(ao)], -1))


def rock_detail():
    """Stylised rock detail masks (world-aligned/triplanar use, tile ~2.5 m):
    R barnacles: small volcano-shaped dots (1 on the rim, darker centre), clustered;
    G lichen: blobby patches with crisp, lobed edges (thresholded in the material);
    B grit: fine speckle for albedo breakup."""
    size = 1024
    f1, f2 = worley_grid(size, 90, 90, seed=111)
    radius = 0.22 + 0.16 * n01(spectral_noise(size, 6, 40, -1.4, seed=112))
    r = f1 / radius
    rim = np.clip(1.0 - np.abs(r - 0.75) / 0.3, 0, 1) * (r < 1.05)
    centre = np.clip(1.0 - r / 0.45, 0, 1)
    cluster = np.clip(n01(spectral_noise(size, 3, 20, -1.5, seed=113)) * 2.2 - 0.7, 0, 1)
    barnacles = np.clip(rim - centre * 0.6, 0, 1) * cluster
    lobes = spectral_noise(size, 2, 14, -1.7, seed=114) * 0.75 + spectral_noise(size, 20, 90, -1.2, seed=115) * 0.25
    lichen = n01(lobes)
    grit = n01(spectral_noise(size, 60, 300, -0.8, seed=116))
    save_rgb(os.path.join(OUT, "Rock", "T_RockDetail_M.png"), np.stack([barnacles, lichen, grit], -1))


def paint_surface():
    """Painted wood (tinted per material), 1 tile = 1 m at 2048 px: visible brush strokes along U,
    the board grain telegraphing through the paint, soft blotches. Crisp but calm."""
    size = 2048
    latewood, knot_dark, _, fibers, broad = wood_field(size, seed=7)
    brush = (spectral_noise_dir(size, 10, 160, -1.25, 91, 0.0, 7.0) * 0.7 +
             spectral_noise_dir(size, 30, 300, -1.1, 94, 4.0, 10.0) * 0.3)
    blotch = spectral_noise(size, 1, 8, -2.0, seed=92)
    albedo = 0.62 + 0.05 * brush + 0.025 * blotch - 0.035 * latewood - 0.05 * knot_dark
    save_rgb(os.path.join(OUT, "Paint", "T_PaintSurface_D.png"), np.stack([albedo, albedo, albedo], -1))
    height = brush * 0.6 - latewood * 0.3 - knot_dark * 0.2 + blotch * 0.05
    save_normal(os.path.join(OUT, "Paint", "T_PaintSurface_N.png"), height_to_normal(height, 2.2))
    rough = np.clip(0.5 + 0.07 * brush + 0.05 * blotch + 0.06 * latewood, 0, 1)
    save_rgb(os.path.join(OUT, "Paint", "T_PaintSurface_ORM.png"), np.stack([np.ones_like(rough), rough, np.zeros_like(rough)], -1))


def canvas():
    """Painted canvas for roofs, covers and later sails (1 tile = 0.5 m at UVScale 2, 2048 px): a visible
    plain weave (64 threads per tile), a sewn seam with stitches across U, soft creases and weathering."""
    size = 2048
    y, x = np.mgrid[0:size, 0:size] / size
    n = 96
    warp = np.sin(2 * np.pi * x * n) * np.sign(np.sin(np.pi * y * n))
    weft = np.sin(2 * np.pi * y * n) * -np.sign(np.sin(np.pi * x * n))
    weave = 0.5 + 0.25 * (warp + weft)
    slub = spectral_noise(size, 2, 40, -1.5, seed=95)
    creases = spectral_noise_dir(size, 2, 20, -1.6, 96, 80.0, 5.0)
    # Seam: a double fold line at v = 0.5 with a row of stitches beside it.
    sv = np.abs(y - 0.5)
    seam = np.clip(1.0 - sv / 0.012, 0, 1)
    fold = np.clip(1.0 - np.abs(sv - 0.016) / 0.004, 0, 1)
    stitches = np.clip(1.0 - np.abs(sv - 0.03) / 0.0035, 0, 1) * (np.sin(2 * np.pi * x * 80) > 0.2)
    grime = n01(spectral_noise(size, 1, 10, -1.8, seed=97))
    albedo = (0.6 + 0.06 * (weave - 0.5) + 0.04 * slub + 0.03 * creases - 0.08 * fold - 0.1 * stitches
              + 0.03 * seam - 0.06 * grime)
    save_rgb(os.path.join(OUT, "Fabric", "T_Canvas_D.png"), np.stack([albedo, albedo * 0.98, albedo * 0.93], -1))
    height = weave * 0.6 + slub * 0.2 + creases * 1.2 + seam * 0.8 - fold * 0.6 + stitches * 0.4
    save_normal(os.path.join(OUT, "Fabric", "T_Canvas_N.png"), height_to_normal(height, 1.6))


def macro_noise():
    """General-purpose world-space variation: R low, G mid, B high frequency (all 0..1)."""
    size = 512
    r = n01(spectral_noise(size, 1, 4, -2.0, seed=81))
    g = n01(spectral_noise(size, 4, 16, -1.8, seed=82))
    b = n01(spectral_noise(size, 16, 64, -1.4, seed=83))
    save_rgb(os.path.join(OUT, "Common", "T_MacroNoise_M.png"), np.stack([r, g, b], -1))


GENERATORS = {
    "ocean_ripples": ocean_ripples,
    "ocean_foam": ocean_foam,
    "wood_grain": wood_grain,
    "wood_weathered": wood_weathered,
    "paint_wear": paint_wear,
    "metal": metal,
    "rope": rope,
    "rock": rock,
    "rock_detail": rock_detail,
    "macro_noise": macro_noise,
    "paint_surface": paint_surface,
    "canvas": canvas,
}

if __name__ == "__main__":
    names = sys.argv[1:] or list(GENERATORS)
    for n in names:
        GENERATORS[n]()
