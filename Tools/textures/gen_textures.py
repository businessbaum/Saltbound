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
    size = 512
    # Wind ripples: slightly stretched across the wind, mid/high frequencies.
    h = (spectral_noise(size, 6, 48, -1.6, seed=11, aniso=(1.0, 1.35)) * 0.7 +
         spectral_noise(size, 24, 110, -1.2, seed=12) * 0.3)
    save_normal(os.path.join(OUT, "Water", "T_OceanRipples_N.png"), height_to_normal(h, 6.0))


def ocean_foam():
    size = 512
    cells = 1.0 - worley(size, 14, seed=21)                     # bubbly cells
    breakup = spectral_noise(size, 2, 16, -1.8, seed=22) * 0.5 + 0.5
    streaks = spectral_noise(size, 3, 40, -1.4, seed=23, aniso=(0.35, 1.0)) * 0.5 + 0.5
    foam = np.clip(cells ** 2.2 * 0.75 + streaks * 0.35, 0, 1)
    rgb = np.stack([foam, breakup, streaks], -1)
    save_rgb(os.path.join(OUT, "Water", "T_OceanFoam_M.png"), rgb)


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


def wood_grain():
    """Neutral warm wood grain (tinted per material). Grain runs along U (image X).

    Irregular band spacing (phase driven by noise that varies across the grain), soft late-wood lines of
    varying strength, long fibre streaks and broad tonal drift. Deliberately low contrast.
    """
    size = 1024
    y, x = np.mgrid[0:size, 0:size] / size
    across = spectral_noise(size, 1, 4, -2.0, seed=35, aniso=(40.0, 1.0))   # varies with y only
    warp = (spectral_noise(size, 1, 5, -2.0, seed=31, aniso=(6.0, 1.0)) * 0.03 +
            spectral_noise(size, 3, 16, -1.6, seed=32, aniso=(4.0, 1.0)) * 0.006)
    phase = (y + warp) * 18.0 + across * 2.5                   # ~18 bands per tile, uneven spacing
    line = (0.5 + 0.5 * np.cos(2 * np.pi * phase)) ** 2.2
    strength = 0.55 + 0.45 * n01(spectral_noise(size, 1, 6, -1.8, seed=36, aniso=(3.0, 1.0)))
    rings = line * strength
    fibers = spectral_noise(size, 40, 320, -1.0, seed=33, aniso=(12.0, 1.0))
    broad = spectral_noise(size, 1, 6, -2.0, seed=34)
    albedo = 0.60 - 0.12 * rings - 0.045 * np.abs(fibers) + 0.07 * broad
    rgb = np.stack([albedo * 1.00, albedo * 0.93, albedo * 0.84], -1)
    height = -rings * 0.6 + fibers * 0.2
    save_rgb(os.path.join(OUT, "Wood", "T_WoodGrain_D.png"), rgb)
    save_normal(os.path.join(OUT, "Wood", "T_WoodGrain_N.png"), height_to_normal(height, 2.0))
    ao = 1.0 - 0.12 * rings
    rough = np.clip(0.62 + 0.12 * rings - 0.06 * broad, 0, 1)
    save_rgb(os.path.join(OUT, "Wood", "T_WoodGrain_ORM.png"), np.stack([ao, rough, np.zeros_like(ao)], -1))


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


def rock():
    """Stylised coastal rock for world-aligned (triplanar) use: broad tone, sparse warped cracks, strata."""
    size = 1024
    y, x = np.mgrid[0:size, 0:size] / size
    broad = spectral_noise(size, 1, 8, -2.2, seed=71)
    mid = spectral_noise(size, 6, 40, -1.6, seed=72)
    fine = spectral_noise(size, 40, 200, -1.0, seed=75)
    cracks = worley_f2f1(size, 4, seed=73)
    # Warp crack lines by sampling the distance field at noise-offset coordinates.
    wx = (spectral_noise(size, 2, 10, -1.8, seed=76) * 18).astype(int)
    wy = (spectral_noise(size, 2, 10, -1.8, seed=77) * 18).astype(int)
    iy, ix = np.mgrid[0:size, 0:size]
    cracks = cracks[(iy + wy) % size, (ix + wx) % size]
    presence = np.clip(n01(spectral_noise(size, 1, 6, -1.6, seed=78)) * 1.8 - 0.5, 0, 1)
    crack_line = np.clip(1.0 - cracks / 0.035, 0, 1) ** 1.6 * presence
    strata = 0.5 + 0.5 * np.sin(2 * np.pi * (y * 5 + broad * 0.35))
    height = broad * 0.7 + mid * 0.3 + fine * 0.05 - crack_line * 0.25 + strata * 0.15
    albedo = 0.55 + 0.13 * broad + 0.06 * mid + 0.03 * strata - 0.08 * crack_line
    save_rgb(os.path.join(OUT, "Rock", "T_Rock_D.png"), np.stack([albedo, albedo, albedo * 0.97], -1))
    save_normal(os.path.join(OUT, "Rock", "T_Rock_N.png"), height_to_normal(height, 7.0))
    ao = 1.0 - 0.25 * crack_line
    rough = np.clip(0.78 + 0.1 * mid, 0, 1)
    save_rgb(os.path.join(OUT, "Rock", "T_Rock_ORM.png"), np.stack([ao, rough, np.zeros_like(ao)], -1))


def paint_surface():
    """Painted wood: near-flat colour, soft brush strokes along U, faint grain telegraphing through."""
    size = 1024
    brush = spectral_noise(size, 4, 120, -1.3, seed=91, aniso=(6.0, 1.0))
    blotch = spectral_noise(size, 1, 8, -2.0, seed=92)
    y, x = np.mgrid[0:size, 0:size] / size
    grain = (0.5 + 0.5 * np.cos(2 * np.pi * ((y + spectral_noise(size, 1, 5, -2.0, seed=93, aniso=(6.0, 1.0)) * 0.03) * 18))) ** 2.2
    albedo = 0.62 + 0.02 * brush + 0.025 * blotch - 0.015 * grain
    save_rgb(os.path.join(OUT, "Paint", "T_PaintSurface_D.png"), np.stack([albedo, albedo, albedo], -1))
    height = brush * 0.25 - grain * 0.12 + blotch * 0.05
    save_normal(os.path.join(OUT, "Paint", "T_PaintSurface_N.png"), height_to_normal(height, 1.2))
    rough = np.clip(0.5 + 0.06 * brush + 0.05 * blotch, 0, 1)
    save_rgb(os.path.join(OUT, "Paint", "T_PaintSurface_ORM.png"), np.stack([np.ones_like(rough), rough, np.zeros_like(rough)], -1))


def canvas():
    """Tarred/painted canvas weave for roofs, covers and sails (64 threads per tile)."""
    size = 1024
    y, x = np.mgrid[0:size, 0:size] / size
    n = 64
    warp = np.sin(2 * np.pi * x * n) * np.sign(np.sin(np.pi * y * n))
    weft = np.sin(2 * np.pi * y * n) * -np.sign(np.sin(np.pi * x * n))
    weave = 0.5 + 0.25 * (warp + weft)
    slub = spectral_noise(size, 2, 30, -1.6, seed=95)
    albedo = 0.6 + 0.05 * (weave - 0.5) + 0.04 * slub
    save_rgb(os.path.join(OUT, "Fabric", "T_Canvas_D.png"), np.stack([albedo, albedo * 0.98, albedo * 0.93], -1))
    save_normal(os.path.join(OUT, "Fabric", "T_Canvas_N.png"), height_to_normal(weave + slub * 0.2, 1.4))


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
    "paint_wear": paint_wear,
    "metal": metal,
    "rope": rope,
    "rock": rock,
    "macro_noise": macro_noise,
    "paint_surface": paint_surface,
    "canvas": canvas,
}

if __name__ == "__main__":
    names = sys.argv[1:] or list(GENERATORS)
    for n in names:
        GENERATORS[n]()
