"""Image metrics for the water surface (reflection-breakup diagnostics, D26).

  python Tools/water_metrics.py img1.png [img2.png ...] [--top 0.5]

Measured on the lower part of the frame (water only; --top = fraction of the height where the region
starts):
  silver   share of pixels that are pale and unsaturated (HSV S < 0.30, V > 0.50): silver/grey reflection coating
  luma     mean / std of luminance (0..255)
  streak   structure-tensor coherence (0 = isotropic detail, 1 = parallel lines), energy weighted
  horiz    share of gradient energy from near-horizontal streaks (gradient within 20 deg of vertical)
  fine     mean |Laplacian| of luminance: density of tight, crumpled detail (near field = bottom third)
"""
import argparse
import os

import numpy as np
from PIL import Image


def box(a, r):
    """Box blur (separable, edge clamp) with radius r."""
    k = 2 * r + 1
    p = np.pad(a, r, mode="edge")
    c = np.cumsum(np.cumsum(p, 0), 1)
    c = np.pad(c, ((1, 0), (1, 0)))
    return (c[k:, k:] - c[:-k, k:] - c[k:, :-k] + c[:-k, :-k]) / (k * k)


def metrics(path, top=0.5):
    im = Image.open(path).convert("RGB")
    a = np.asarray(im).astype(np.float64) / 255.0
    h = a.shape[0]
    a = a[int(h * top):]
    mx, mn = a.max(2), a.min(2)
    sat = np.where(mx > 1e-6, (mx - mn) / np.maximum(mx, 1e-6), 0)
    silver = float(((sat < 0.30) & (mx > 0.50)).mean())
    y = (0.2126 * a[..., 0] + 0.7152 * a[..., 1] + 0.0722 * a[..., 2]) * 255
    gx = np.zeros_like(y); gy = np.zeros_like(y)
    gx[:, 1:-1] = (y[:, 2:] - y[:, :-2]) * 0.5
    gy[1:-1, :] = (y[2:, :] - y[:-2, :]) * 0.5
    jxx, jyy, jxy = box(gx * gx, 6), box(gy * gy, 6), box(gx * gy, 6)
    tr = jxx + jyy
    det_term = np.sqrt((jxx - jyy) ** 2 + 4 * jxy ** 2)
    coh = np.where(tr > 1e-3, det_term / np.maximum(tr, 1e-3), 0)
    streak = float((coh * tr).sum() / max(tr.sum(), 1e-6))
    ang = np.degrees(np.arctan2(np.abs(gy), np.abs(gx)))        # 90 = vertical gradient = horizontal line
    e = gx * gx + gy * gy
    horiz = float(e[ang > 70].sum() / max(e.sum(), 1e-6))
    lap = np.abs(4 * y[1:-1, 1:-1] - y[:-2, 1:-1] - y[2:, 1:-1] - y[1:-1, :-2] - y[1:-1, 2:])
    third = lap.shape[0] // 3
    return {"silver": silver, "luma": float(y.mean()), "luma_std": float(y.std()), "streak": streak,
            "horiz": horiz, "fine_near": float(lap[-third:].mean()), "fine_far": float(lap[:third].mean())}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("images", nargs="+")
    ap.add_argument("--top", type=float, default=0.5)
    ns = ap.parse_args()
    print("%-34s %7s %6s %6s %7s %6s %9s %8s" % ("image", "silver", "luma", "std", "streak", "horiz", "fine_near", "fine_far"))
    for p in ns.images:
        m = metrics(p, ns.top)
        print("%-34s %6.1f%% %6.1f %6.1f %7.3f %6.3f %9.2f %8.2f" % (os.path.basename(p)[:34], m["silver"] * 100, m["luma"],
              m["luma_std"], m["streak"], m["horiz"], m["fine_near"], m["fine_far"]))


if __name__ == "__main__":
    main()
