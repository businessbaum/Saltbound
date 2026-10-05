"""Shape of the home island (D60): a tropical ring of land round a lagoon, the harbour inside, one channel out.

Owner (2026-10-05): "a much more interesting start island with nature, trees, palms etc. A tropic island in the form of a
circle with an exit, in the middle water and the harbour."

Pure numpy (no Blender): build_home_island.py (meshes and the scatter of plants and rocks) and Tools/textures/gen_ui.py
(the charts) read it. Every function takes scalars or numpy arrays.

Unreal metres: X north (along the pier, into the lagoon), Y east, z above the still water. The pier and its beach stay
where the old cove had them: the lagoon's south shore runs through (-15.5, 0) and the water by the pier is 2.3 m deep,
so the boat's mooring, the pier and the swimming checks keep their places.

  Lagoon      about 220 m across round CENTRE: a shallow sand rim (2-2.5 m, turquoise) and a floor 6 m down, a small
              sand islet with palms in its north-west.
  Ring        50-95 m of land: beaches inside and out (berms 0.7 / 0.9 m), a grassy, wooded inland 1.5-4 m high with
              rises, and a hill in the west (about 15 m) with a footpath up to it from the pier.
  Channel     the way out, east (across the usual south-south-west wind, so it is a reach in and out): about 50 m wide
              and 5 m deep, through the ring and the reef, between two rocky headlands.
  Outside     a reef flat 2-2.6 m deep (turquoise) round the island, then a drop-off to the 60 m floor.
"""
import math

import numpy as np

CENTRE = (95.0, 5.0)                 # the lagoon's middle (m)
LAGOON_R = 110.5                     # the inner shore's mean radius; exact behind the pier (south)
GAP_ANGLE = math.radians(92.0)       # the channel's bearing from the lagoon's middle (0 north, 90 east)
GAP_HALF = 26.0                      # half the channel's width (m, along the ring)
ISLET = (150.0, -35.0, 9.0)          # the lagoon islet: centre x, y and its shore radius (m)
FLOOR = -60.0                        # the far sea floor (the map's floor block, at -59.5 m)
HALF = 300.0                         # the meshes cover CENTRE +- HALF (m)

# Rises inland: (bearing deg, height m, radius m, offset across the band 0 inner .. 1 outer).
HILLS = [(-90.0, 14.5, 34.0, 0.52), (-35.0, 5.5, 26.0, 0.5), (180.0, 3.2, 34.0, 0.62), (55.0, 4.5, 18.0, 0.5),
         (128.0, 4.0, 18.0, 0.45), (-150.0, 3.0, 26.0, 0.5)]

# The footpath from the pier's beach to the hilltop (m): a Catmull-Rom curve through these points.
PATH = [(-17.0, 2.0), (-24.0, -4.0), (-30.0, -18.0), (-30.0, -42.0), (-18.0, -72.0), (8.0, -102.0), (40.0, -126.0),
        (70.0, -140.0), (92.0, -146.0)]
PATH_HALF = 1.3                      # half the path's width (m)

# Water by depth (the material's sea-floor sand vs. beach sand): as the cove (seabed_shape.py).
RIPPLE_SPACING = 0.95
RIPPLE_HEIGHT = 0.09


# --- helpers ------------------------------------------------------------------------------------------------------
def smooth(t):
    t = np.clip(t, 0.0, 1.0)
    return t * t * (3.0 - 2.0 * t)


def noise(x, y, scale, seed):
    """Smooth deterministic value in about [-1, 1] (a few incommensurate sine waves; no tiling)."""
    s = 0.0
    for i, (a, b, c) in enumerate(((0.83, 0.31, 1.3), (-0.27, 0.91, 4.1), (0.58, -0.66, 2.6), (0.12, 0.47, 5.3))):
        s = s + np.sin((a * x + b * y) / scale * (1.0 + 0.17 * i) + c + seed * 1.7)
    return s / 4.0


def smin(a, b, k):
    """Smooth minimum (polynomial): exact where a and b differ by more than k."""
    h = np.maximum(k - np.abs(a - b), 0.0) / k
    return np.minimum(a, b) - h * h * k * 0.25


def smax(a, b, k):
    h = np.maximum(k - np.abs(a - b), 0.0) / k
    return np.maximum(a, b) + h * h * k * 0.25


def pchip(xk, yk, x):
    """Monotone cubic through the knots (Fritsch-Carlson): no overshoot, no flat steps at the knots."""
    xk = np.asarray(xk, dtype=float)
    yk = np.asarray(yk, dtype=float)
    h = np.diff(xk)
    d = np.diff(yk) / h
    m = np.zeros_like(yk)
    m[0], m[-1] = d[0], d[-1]
    for i in range(1, len(xk) - 1):
        if d[i - 1] * d[i] > 0.0:
            w1, w2 = 2.0 * h[i] + h[i - 1], h[i] + 2.0 * h[i - 1]
            m[i] = (w1 + w2) / (w1 / d[i - 1] + w2 / d[i])
    x = np.clip(np.asarray(x, dtype=float), xk[0], xk[-1])
    i = np.clip(np.searchsorted(xk, x) - 1, 0, len(xk) - 2)
    t = (x - xk[i]) / h[i]
    t2, t3 = t * t, t * t * t
    return ((2 * t3 - 3 * t2 + 1) * yk[i] + (t3 - 2 * t2 + t) * h[i] * m[i] + (-2 * t3 + 3 * t2) * yk[i + 1]
            + (t3 - t2) * h[i] * m[i + 1])


def angdiff(a, b):
    return (a - b + np.pi) % (2.0 * np.pi) - np.pi


def polar(x, y):
    """Distance from the lagoon's middle and the bearing (0 north, +pi/2 east)."""
    dx, dy = x - CENTRE[0], y - CENTRE[1]
    return np.hypot(dx, dy), np.arctan2(dy, dx)


# --- the ring -----------------------------------------------------------------------------------------------------
def inner_radius(th):
    """The lagoon's shore: lobes and bays, a plain arc behind the pier (south) so the old beach stays where it was."""
    lobes = 6.5 * np.sin(2 * th + 0.9) + 4.0 * np.sin(3 * th + 2.2) + 2.0 * np.sin(5 * th + 0.4)
    free = 1.0 - np.exp(-(angdiff(th, np.pi) / 0.32) ** 2)
    return LAGOON_R + lobes * free


def band_width(th):
    """Land across the ring: widest in the west (the hill), narrowest towards the channel."""
    return 70.0 - 20.0 * np.cos(th - GAP_ANGLE) + 6.0 * np.sin(3 * th + 1.0) + 3.5 * np.sin(5 * th + 2.0)


def outer_radius(th):
    return inner_radius(th) + band_width(th)


def gap(r, th):
    """The channel: 1 inside it, falling off over 9 m at its sides; (factor, across) with across in metres from its
    middle line (along the ring)."""
    across = angdiff(th, GAP_ANGLE) * np.clip(r, 60.0, 400.0)
    return smooth((GAP_HALF + 9.0 - np.abs(across)) / 9.0), across


# Profiles across the shores (monotone cubics; metres from the shore, + inland).
_IN_X = [-200.0, -66.0, -50.0, -36.0, -30.0, -14.0, -10.5, -5.0, 0.0, 4.0, 9.0, 16.0, 40.0]
_IN_Z = [-6.0, -6.0, -5.0, -3.2, -2.3, -2.15, -1.4, -0.5, 0.05, 0.4, 0.65, 0.72, 0.75]
_OUT_X = [-200.0, -120.0, -80.0, -55.0, -40.0, -30.0, -14.0, -6.0, 0.0, 5.0, 11.0, 18.0, 40.0]
_OUT_Z = [FLOOR, FLOOR, -40.0, -14.0, -3.4, -2.6, -2.0, -0.85, 0.05, 0.5, 0.9, 1.0, 1.0]


def lagoon_profile(e, th):
    """By the distance from the lagoon's shore (e < 0 in the water): the 2-2.5 m rim is 30 m wide behind the pier and
    16-36 m elsewhere."""
    rim = 26.0 + 10.0 * np.sin(2 * th + 2.5) * (1.0 - np.exp(-(angdiff(th, np.pi) / 0.32) ** 2))
    rim = np.where(np.abs(angdiff(th, np.pi)) < 0.6, 30.0 + (rim - 30.0) * smooth((np.abs(angdiff(th, np.pi)) - 0.25) / 0.35), rim)
    d = -e
    ds = np.where(d > 14.0, 14.0 + (d - 14.0) * 16.0 / np.maximum(rim - 14.0, 4.0), d)
    return pchip(_IN_X, _IN_Z, -ds)


def sea_profile(f, th):
    """By the distance from the outer coast (f < 0 at sea): the reef flat, then the drop-off."""
    reef = 1.0 + 0.25 * np.sin(2 * th + 1.7) + 0.15 * np.sin(5 * th + 0.3)
    fs = np.where(f < -14.0, -14.0 + (f + 14.0) / reef, f)
    return pchip(_OUT_X, _OUT_Z, fs)


def path_distance(x, y):
    """Distance (m) from the footpath's middle line."""
    pts = np.array(PATH)
    p = np.concatenate([pts[:1], pts, pts[-1:]])
    samples = []
    for i in range(1, len(p) - 2):
        for t in np.linspace(0.0, 1.0, 12, endpoint=False):
            t2, t3 = t * t, t * t * t
            samples.append(0.5 * ((2 * p[i]) + (-p[i - 1] + p[i + 1]) * t + (2 * p[i - 1] - 5 * p[i] + 4 * p[i + 1] - p[i + 2]) * t2
                                  + (-p[i - 1] + 3 * p[i] - 3 * p[i + 1] + p[i + 2]) * t3))
    samples.append(pts[-1])
    s = np.array(samples)
    best = np.full(np.shape(x), 1e9)
    for (sx0, sy0), (sx1, sy1) in zip(s[:-1], s[1:]):
        vx, vy = sx1 - sx0, sy1 - sy0
        t = np.clip(((x - sx0) * vx + (y - sy0) * vy) / (vx * vx + vy * vy), 0.0, 1.0)
        best = np.minimum(best, np.hypot(x - sx0 - t * vx, y - sy0 - t * vy))
    return best


def hills(r, th, ri, ro):
    h = 0.0
    for bearing, height, radius, across in HILLS:
        b = math.radians(bearing)
        rr = inner_radius(b) + across * band_width(b)
        hx, hy = CENTRE[0] + rr * math.cos(b), CENTRE[1] + rr * math.sin(b)
        x, y = CENTRE[0] + r * np.cos(th), CENTRE[1] + r * np.sin(th)
        q = np.hypot(x - hx, y - hy) / radius
        # A rounded rise with a soft shoulder (stylised: chunky, no peaks).
        h = h + height * smooth(1.0 - q) ** 1.3
    return h


def fields(x, y):
    """Everything the meshes and the scatter need, in one pass: a dict of arrays.

      z        the broad height (collision; the turf and the sand add their detail on top)
      e, f     metres from the lagoon's shore and from the outer coast (+ inland)
      inland   0 on the beaches, 1 where the turf, the trees and the rises begin
      gap      1 in the channel
      path     metres from the footpath
    """
    x = np.asarray(x, dtype=float)
    y = np.asarray(y, dtype=float)
    r, th = polar(x, y)
    ri, ro = inner_radius(th), outer_radius(th)
    e, f = r - ri, ro - r
    beach = smin(pchip(_IN_X, _IN_Z, e), pchip(_OUT_X, _OUT_Z, f), 0.3)
    water = np.where(e < 0.0, lagoon_profile(e, th), sea_profile(f, th))
    z = np.where((e >= 0.0) & (f >= 0.0), beach, water)
    # Inland: the ground rises gently behind the beaches, rolls, and lifts into the rises and the west hill.
    inland = smooth((e - 9.0) / 14.0) * smooth((f - 11.0) / 14.0)
    roll = 1.0 + 0.55 * noise(x, y, 17.0, 3) + 0.3 * noise(x, y, 7.0, 4)
    z = z + inland * (roll + hills(r, th, ri, ro))
    # Soft dunes along the beaches' backs.
    dunes = smooth(e / 6.0) * smooth(f / 6.0) * (1.0 - inland) * 0.35 * np.maximum(noise(x, y, 4.5, 5), 0.0)
    z = z + dunes
    # The lagoon's floor: soft mounds, calm round the pier (its posts stand in smooth sand).
    calm = 1.0 - 0.8 * smooth((3.5 - np.abs(y)) / 2.0) * smooth((x + 4.0) / 3.0) * smooth((17.0 - x) / 3.0)
    deep = smooth((-e - 12.0) / 10.0) * (e < 0.0)
    z = z + deep * calm * (0.5 * noise(x, y, 9.0, 1) + 0.25 * noise(x, y, 3.7, 2))
    # The islet: a small sand cay with a shelf.
    ix, iy, ir = ISLET
    di = np.hypot(x - ix, y - iy) / ir
    cay = np.where(di < 4.0, pchip([0.0, 0.5, 1.0, 1.6, 2.6, 4.0], [1.35, 0.95, 0.0, -1.3, -3.0, -6.5], di), -1000.0)
    z = smax(z, cay, 0.6)
    # The channel: cut through the ring and the reef, deepest in its middle.
    g, across = gap(r, th)
    reach = smooth((r - (ri - 40.0)) / 10.0)
    channel = -5.2 + 2.6 * (np.clip(across / GAP_HALF, -1.4, 1.4)) ** 2
    z = np.where(g * reach > 0.0, z + (np.minimum(z, channel) - z) * g * reach, z)
    # Out at the edge of the meshes: the far floor.
    edge = smooth((np.maximum(np.abs(x - CENTRE[0]), np.abs(y - CENTRE[1])) - (HALF - 30.0)) / 30.0)
    z = z + (FLOOR - z) * edge
    pd = path_distance(x, y)
    # The path is worn a little into the ground.
    z = z - 0.08 * smooth((PATH_HALF + 0.6 - pd) / 0.8) * inland
    return {"z": z, "e": e, "f": f, "r": r, "th": th, "inland": inland, "gap": g * reach, "path": pd}


def base(x, y):
    return fields(x, y)["z"]


def beach_weight(z):
    """0 on the sea floor, 1 on the beach (the sand material blends to its beach texture just below the water)."""
    return smooth((z + 0.35) / 0.4)


def sand_detail(x, y, z):
    """Wind ripples and swash ridges on the dry beach (as the cove, D37): (dz, crest, trough, tone)."""
    tone = 0.5 + 0.5 * noise(x, y, 4.5, 7)
    beach = smooth((z + 0.15) / 0.3)
    dry = smooth((z - 0.05) / 0.2)
    patch = smooth((0.5 + 0.5 * noise(x, y, 9.0, 11) + 0.25 * noise(x, y, 3.5, 12) + 0.12) / 0.25)
    theta = 0.35 * noise(x, y, 18.0, 13)
    u = x * np.cos(theta) + y * np.sin(theta)
    phi = 2.0 * np.pi * u / RIPPLE_SPACING + 3.2 * noise(x, y, 6.0, 14) + 1.3 * noise(x, y, 2.4, 15)
    phi = phi + 0.5 * np.sin(phi)
    p = (0.5 + 0.5 * np.cos(phi)) ** 2.0
    amt = dry * patch * (0.7 + 0.3 * noise(x, y, 3.0, 16))
    dz = RIPPLE_HEIGHT * amt * (p - 0.375)
    swash = 0.0
    for k, zk in enumerate((-0.02, 0.07, 0.16)):
        swash = swash + np.exp(-((z - zk) / 0.03) ** 2) * (0.55 + 0.45 * noise(x, y, 5.0, 30 + k))
    dz = (dz + 0.03 * swash * smooth((z + 0.1) / 0.1)) * beach
    crest = np.maximum(smooth((p - 0.55) / 0.35) * amt, np.minimum(swash, 1.0) * 0.6) * beach
    trough = smooth((0.18 - p) / 0.18) * amt * beach
    return dz, crest, trough, tone


def turf(F, x, y):
    """Grass cover 0..1: inland of the beaches' backs (a ragged edge), none on the path, the channel's headland rocks or
    the islet's sand."""
    line = 0.9 + 0.22 * noise(x, y, 5.5, 41) + 0.1 * noise(x, y, 1.9, 42)
    t = smooth((F["z"] - line) / 0.12) * smooth((F["e"] - 7.0) / 3.0) * smooth((F["f"] - 9.0) / 3.0)
    t = t * smooth((F["path"] - PATH_HALF) / 0.35)
    return t * (1.0 - F["gap"])


def forest(F, x, y):
    """Where the trees stand thick (0..1): clumps inland, the west hill's flanks wooded, open round the path's start."""
    clumps = smooth((noise(x, y, 21.0, 51) + 0.35 * noise(x, y, 8.0, 52) + 0.15) / 0.35)
    deep = smooth((F["e"] - 16.0) / 12.0) * smooth((F["f"] - 18.0) / 12.0)
    open_ = smooth((np.hypot(x + 20.0, y) - 25.0) / 15.0)
    return clumps * deep * open_
