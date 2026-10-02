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


GENERATORS = {
    "ocean_ripples": ocean_ripples,
    "ocean_foam": ocean_foam,
}

if __name__ == "__main__":
    names = sys.argv[1:] or list(GENERATORS)
    for n in names:
        GENERATORS[n]()
