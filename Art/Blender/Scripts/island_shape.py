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


# --- D71: the ground's surface (one mesh: sand, earth, grass, litter) -----------------------------------------------
# Owner (2026-10-06): "the floor ... lacks EVERYTHING ELSE ... everything is perfectly flat, there is no good transition
# between grass and sand." The functions above stay as they were (the plants keep their places); these add what the visible
# ground (SM_Home_Ground) needs: where each cover lies and the small relief on top of the broad shape (the collision).

# Hand-placed places (m, yaw deg; Unreal axes). Plants inside `clear` are left out; the ground knows about them too.
PLACES = {
    # The Harbour Keeper's hut on the pier's beach, its counter facing the pier (D71; the M6 helper NPC stands here).
    "keeper_hut": {"x": -23.5, "y": 13.0, "yaw": -31.0, "clear": (-3.5, 4.5, -3.8, 3.8)},
    # "Spare canvas under the leaning palm, three paces seaward" (GameConcept 8.1): the palm (SM_Palm_B, leaning north)
    # at (-25.7, 4.8), the loose sand three paces (2.3 m) seaward of it.
    "dig_spot": {"x": -23.3, "y": 4.9, "yaw": 0.0},
    # A castaway's wreck thrown up on the windward (south-west) outer beach.
    "wreck": {"x": -68.3, "y": -108.0, "yaw": 65.0, "clear": (-6.0, 6.0, -5.0, 5.0)},
    # Tide pools in a shelf of rock at the west coast's surf (by the surf rock at (59.7, -195.2)), along the shore.
    "tide_pools": {"x": 50.5, "y": -188.8, "yaw": -10.0, "clear": (-3.2, 3.2, -2.2, 2.2)},
    # The old lookout on the west hill's crown: a cairn and a sighting post on the Spire (yaw 41: (560, 260) seen from here).
    "lookout": {"x": 95.5, "y": -143.5, "yaw": 41.0, "clear": (-2.0, 2.0, -1.8, 1.8)},
    # The spring at the hill's foot, running out of a broken Old World pipe (local -X up the slope, to the hill).
    "spring": {"x": 73.0, "y": -167.0, "yaw": -135.6, "clear": (-4.4, 2.6, -2.0, 2.0)},
    # A fire pit by the footpath with log seats.
    "fire_pit": {"x": -34.0, "y": -41.0, "yaw": 20.0, "clear": (-2.6, 2.6, -2.6, 2.6)},
    # A palm the storms threw over on the north-west outer beach, its crown out in the shallows by the surf rock (walkable).
    "fallen_palm": {"x": -58.2, "y": 60.7, "yaw": 162.0, "clear": (-1.0, 12.0, -1.2, 1.2)},
}


def _hash2(ix, iy, seed):
    h = (ix * 374761393 + iy * 668265263 + seed * 2147483647) & 0xFFFFFFFF
    h = ((h ^ (h >> 13)) * 1274126177) & 0xFFFFFFFF
    return ((h ^ (h >> 16)) & 0xFFFF) / 32767.5 - 1.0


def vnoise(x, y, cell, seed):
    """Gradient (Perlin) noise in about [-1, 1] on a lattice of `cell` metres (no visible directions or blocks, unlike the
    sum of sines in noise(); for the small relief and the cover's edges)."""
    x = np.asarray(x, dtype=float) / cell
    y = np.asarray(y, dtype=float) / cell
    ix, iy = np.floor(x).astype(np.int64), np.floor(y).astype(np.int64)
    fx, fy = x - ix, y - iy

    def corner(cx, cy, dx, dy):
        a = _hash2(cx, cy, seed) * np.pi
        return np.cos(a) * dx + np.sin(a) * dy

    ux = fx * fx * fx * (fx * (fx * 6 - 15) + 10)
    uy = fy * fy * fy * (fy * (fy * 6 - 15) + 10)
    n00 = corner(ix, iy, fx, fy)
    n10 = corner(ix + 1, iy, fx - 1, fy)
    n01 = corner(ix, iy + 1, fx, fy - 1)
    n11 = corner(ix + 1, iy + 1, fx - 1, fy - 1)
    a = n00 + (n10 - n00) * ux
    b = n01 + (n11 - n01) * ux
    return np.clip((a + (b - a) * uy) * 1.6, -1.0, 1.0)


def fbm(x, y, cell, seed, octaves=3):
    s, amp, tot = 0.0, 1.0, 0.0
    for o in range(octaves):
        s = s + amp * vnoise(x, y, cell / (2.0 ** o), seed + 17 * o)
        tot += amp
        amp *= 0.5
    return s / tot


def place_local(x, y, name):
    """Coordinates in a place's frame: (along its facing, to its right) in metres."""
    p = PLACES[name]
    a = math.radians(p["yaw"])
    dx, dy = x - p["x"], y - p["y"]
    return dx * math.cos(a) + dy * math.sin(a), -dx * math.sin(a) + dy * math.cos(a)


def cover(F, x, y):
    """The ground's cover, 0..1 each: grass, litter (fallen leaves, twigs; replaces the grass under the trees) and earth (the
    footpath, trodden places). The sand is what is left. The grass's edge is the turf's ragged line (turf()), widened into a
    zone where it breaks into tufts, crossed by fingers of sand blown in from the beach and tongues of grass running out on
    to it; the material's height blend draws the clumps."""
    x = np.asarray(x, dtype=float)
    y = np.asarray(y, dtype=float)
    z, e, f = F["z"], F["e"], F["f"]
    line = 0.9 + 0.22 * noise(x, y, 5.5, 41) + 0.1 * noise(x, y, 1.9, 42) + 0.07 * fbm(x, y, 1.2, 43)
    # Fingers across the edge (sand into the grass, grass out on to the sand): noise stretched across the shore, about 1.5
    # by 4 m (the along-shore coordinate is the position's component along the ring's tangent: no seam where the bearing
    # wraps, behind the pier).
    r, th = polar(x, y)
    along = -np.sin(th) * x + np.cos(th) * y
    fingers = (0.7 * vnoise(along / 1.5, r / 4.0, 1.0, 44) + 0.3 * vnoise(x, y, 0.8, 144)) * 0.13
    g = smooth((z - line + fingers + 0.12) / 0.32) * smooth((e - 6.0 + 1.5 * fingers / 0.16) / 3.5) * smooth((f - 8.0) / 3.5)
    # Tufts thinning out over the sand: grass also where the clumpy noise peaks, a little beyond the edge.
    tufts = smooth((fbm(x, y, 0.7, 45) - 0.25) / 0.2) * smooth((z - line + 0.3) / 0.22)
    g = np.maximum(g, 0.5 * tufts)
    pd = F["path"]
    path_edge = S_PATH_EDGE(x, y)
    g = g * smooth((pd - path_edge) / 0.45)
    g = g * (1.0 - F["gap"])
    # The footpath's earth: where it runs inland (on the beach it stays sand), ragged at its sides.
    earth = smooth((path_edge + 0.15 - pd) / 0.5) * smooth((F["inland"] - 0.05) / 0.3)
    # Litter: the forest's clumps, and under the trees' crowns (litter_trees adds those from the placements).
    lit = smooth((forest(F, x, y) - 0.2) / 0.45) * 0.85
    lit = lit * smooth((pd - path_edge - 0.4) / 0.8)
    return g, np.clip(lit, 0.0, 1.0), earth


def S_PATH_EDGE(x, y):
    """The footpath's half width with a ragged edge (m)."""
    return PATH_HALF + 0.12 * vnoise(x, y, 0.7, 46) + 0.08 * vnoise(x, y, 2.3, 47)


def relief(F, x, y, g, lit, earth):
    """Small relief over the broad shape (m), within about +-10 cm where people walk (the collision is the broad shape):
    the beach's ripples and swash ridges (sand_detail), a storm berm at the top of the beach, tussocks of grass breaking
    into single clumps at the edge, humps on the forest floor, ruts along the footpath. Returns (dz, crest, trough)."""
    z = F["z"]
    dz, crest, trough, tone = sand_detail(x, y, z)
    bare = 1.0 - np.maximum(g, earth)
    dz = dz * bare
    crest = crest * bare
    trough = trough * bare
    # The berm: a low ridge where the waves' run-up ends (higher on the outer beaches), a shallow runnel behind it.
    outer = smooth((F["e"] - F["f"]) / 30.0)
    zb = 0.42 + 0.05 * outer + 0.04 * vnoise(x, y, 3.0, 48)
    berm = (0.045 + 0.04 * outer) * np.exp(-((z - zb) / 0.07) ** 2) - 0.02 * np.exp(-((z - zb - 0.16) / 0.06) ** 2)
    dz = dz + berm * bare
    # Grass: a thatch a few centimetres thick, hummocks all over (0.5-1.5 m), tussocks standing out where it thins.
    hum = np.maximum(fbm(x, y, 1.1, 49), -0.3)
    tus = smooth((fbm(x, y, 0.6, 50) + 0.1) / 0.4) * smooth((0.9 - g) / 0.4)
    dz = dz + g * (0.03 + 0.045 * hum + 0.05 * tus)
    # The forest floor: soft humps and hollows (roots below, leaves heaped).
    dz = dz + lit * (0.05 * fbm(x, y, 1.4, 51) + 0.02 * vnoise(x, y, 0.45, 52))
    # The footpath: two ruts worn by feet and carts, a slightly raised middle, crumbled edges.
    pd = F["path"]
    rut = np.exp(-((pd - 0.42) / 0.14) ** 2)
    dz = dz + earth * (-0.035 * rut * (0.7 + 0.3 * vnoise(x, y, 1.5, 53)) + 0.012 * vnoise(x, y, 0.35, 54))
    return dz, crest, trough


# The broadleaf trees' roots (build_vegetation.py builds them at these bearings, so the ground's ridges continue them):
# (bearing deg in the plant's own frame, length m).
TREE_ROOTS = {
    "SM_Tree_A": [(12.0, 1.6), (85.0, 1.35), (150.0, 1.7), (222.0, 1.4), (290.0, 1.55)],
    "SM_Tree_B": [(40.0, 1.3), (118.0, 1.1), (190.0, 1.35), (262.0, 1.2), (330.0, 1.15)],
    "SM_Tree_C": [(0.0, 1.2), (100.0, 1.0), (205.0, 1.25), (290.0, 1.05)],
}


def plant_relief(x, y, plants):
    """Mounds round the trunks and ridges along the trees' roots (m), and the litter under the crowns (0..1).
    plants: rows of (mesh, x, y, yaw deg, scale) in metres."""
    x = np.asarray(x, dtype=float)
    y = np.asarray(y, dtype=float)
    dz = np.zeros(x.shape)
    lit = np.zeros(x.shape)
    for mesh, px, py, yaw, sc in plants:
        reach = 5.0 * sc if "Tree" in mesh else 1.2 * sc
        m = (np.abs(x - px) < reach) & (np.abs(y - py) < reach)
        if not m.any():
            continue
        dx, dy = x[m] - px, y[m] - py
        d = np.hypot(dx, dy)
        if "Tree" in mesh:
            mound = 0.11 * sc * np.exp(-(d / (0.9 * sc)) ** 2)
            ridge = np.zeros(d.shape)
            for bearing, length in TREE_ROOTS.get(mesh, TREE_ROOTS["SM_Tree_A"]):
                a = math.radians(yaw + bearing)
                ux, uy = math.cos(a), math.sin(a)
                along = dx * ux + dy * uy
                across = -dx * uy + dy * ux
                L = length * sc * 1.25
                t = np.clip(along / L, 0.0, 1.0)
                w = 0.22 * sc * (1.0 - 0.6 * t)
                ridge = np.maximum(ridge, 0.09 * sc * (1.0 - t) ** 1.5 * np.exp(-(across / w) ** 2) * (along > -0.2))
            dz[m] += mound + ridge
            ragged = 1.0 + 0.3 * vnoise(x[m], y[m], 1.3, 55) + 0.15 * vnoise(x[m], y[m], 0.5, 56)
            lit[m] = np.maximum(lit[m], 0.75 * smooth((3.4 * sc * ragged - d) / (1.2 * sc)))
        else:
            dz[m] += 0.05 * sc * np.exp(-(d / (0.55 * sc)) ** 2)
            lit[m] = np.maximum(lit[m], 0.35 * smooth((1.0 * sc - d) / (0.6 * sc)))
    return dz, lit
