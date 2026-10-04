"""Shape of the visual test cove's seabed (pure Python, no Blender): shared by build_seabed.py (meshes) and
Tools/ue_scripts/create_visual_test_map.py (sea-grass placement on the sculpted surface).

Coordinates are Unreal metres (X towards the sea along the pier, Y across), z relative to the still water level.

  base(x, y)    the broad shape: beach, pier ridge, drop-off, sandbank, headland shoals, soft mounds and long sand
                waves. The collision floor uses it (1 m cells).
  detail(x, y)  sculpted beach sand on top (D37, beach only since the owner's correction): wind ripples about
                1 m apart on the dry sand, soft mounds on the upper beach and swash ridges along the waterline.
                Under water the floor keeps the broad shape (and the painted texture). Returns (dz, crest, trough,
                tone): the height offset and the 0..1 masks the beach material tints.
"""
import math

FLOOR = -17.5          # top of the flat far seabed block in the map (metres)
X_RANGE = (-75.0, 105.0)
Y_RANGE = (-70.0, 70.0)

# Depth profile along X.
PROFILE = [(-75, FLOOR), (-58, -7.0), (-45, -1.6), (-34, 0.4), (-24, 0.6), (-16, 0.1), (-11, -0.5), (-5, -1.4),
           (-1.5, -2.15), (15.0, -2.3), (20.0, -3.4), (30.0, -6.5), (45.0, -10.5), (65.0, -14.5), (88.0, -17.0),
           (105.0, FLOOR)]
HEADLANDS = [(9.0, 39.0), (20.0, 33.0), (34.0, 43.0)]   # rock positions (m)
SANDBANK = (32.0, 18.0, 25.0)   # centre x, y (m) and orientation (deg): light ground about 2 m deep (region looks)

RIPPLE_SPACING = 0.95   # m between wind-ripple crests on the dry beach
RIPPLE_HEIGHT = 0.09    # m crest to trough


def smooth(t):
    t = min(max(t, 0.0), 1.0)
    return t * t * (3.0 - 2.0 * t)


def profile(x):
    for (x0, z0), (x1, z1) in zip(PROFILE[:-1], PROFILE[1:]):
        if x <= x1:
            return z0 + (z1 - z0) * smooth((x - x0) / (x1 - x0))
    return PROFILE[-1][1]


def noise(x, y, scale, seed):
    """Smooth deterministic value in about [-1, 1]: a few incommensurate sine waves (no tiling here)."""
    s = 0.0
    for i, (a, b, c) in enumerate(((0.83, 0.31, 1.3), (-0.27, 0.91, 4.1), (0.58, -0.66, 2.6), (0.12, 0.47, 5.3))):
        s += math.sin((a * x + b * y) / scale * (1.0 + 0.17 * i) + c + seed * 1.7)
    return s / 4.0


def smax(a, b, k):
    """Smooth maximum (polynomial): exact where a and b differ by more than k, rounded in between. A hard max
    leaves a crease that shows as a saw-toothed edge on the fine sand grid."""
    h = max(k - abs(a - b), 0.0) / k
    return max(a, b) + h * h * k * 0.25


def calm(x, y):
    """1 in open sand, low under the pier (its posts stand in smooth sand)."""
    return 1.0 - 0.8 * smooth((3.5 - abs(y)) / 2.0) * smooth((x + 4.0) / 3.0) * smooth((17.0 - x) / 3.0)


def edge(x, y):
    """0 inside, 1 where the cove floor blends into the flat far floor."""
    return max(smooth((abs(y) - 55.0) / 15.0), smooth((x - 95.0) / 10.0), smooth((-65.0 - x) / 10.0))


def base(x, y):
    # Wavy coastline: shifts the shallow profile along X (only near the coast).
    coast = smooth((8.0 - x) / 18.0)
    xs = x + coast * (3.0 * math.sin(y / 11.0) + 1.6 * math.sin(y / 5.3 + 1.0))
    z = profile(xs)
    # Deeper away from the pier, so the pier stands on a gentle sandy ridge.
    near_pier = smooth((x + 6.0) / 8.0) * smooth((24.0 - x) / 6.0)
    z -= near_pier * min(0.06 * max(abs(y) - 3.0, 0.0), 1.8)
    # Shallower towards the headland rocks (always submerged under them).
    for hx, hy in HEADLANDS:
        d = math.hypot(x - hx, y - hy)
        # Raised towards -1.2 m, saturating smoothly (no crease at the cap, no change far away).
        room = -1.2 - z
        if room > 1e-4:
            z += room * (1.0 - math.exp(-7.0 * math.exp(-(d / 13.0) ** 2) / room))
    # Sandbank east of the pier: a long shallow plateau.
    bx, by, ba = SANDBANK
    a = math.radians(ba)
    u = (x - bx) * math.cos(a) + (y - by) * math.sin(a)
    v = -(x - bx) * math.sin(a) + (y - by) * math.cos(a)
    bank = math.exp(-((u / 15.0) ** 2 + (v / 8.0) ** 2) ** 1.5)
    z = smax(z, -1.9 - 8.0 * (1.0 - bank), 0.8)
    # Uneven sand: broad mounds and long sand waves, calm under the pier and on the dry beach.
    wet = smooth((0.2 - z) / 0.8)
    mounds = 0.55 * noise(x, y, 7.0, 1) + 0.25 * noise(x, y, 3.1, 2)
    phase = (x + 1.8 * math.sin(y / 7.0) + 0.9 * math.sin(y / 3.3)) / 3.4 * 2.0 * math.pi
    sandwave = 0.11 * (math.sin(phase) + 0.25 * math.sin(2.0 * phase + 0.6)) * (0.6 + 0.4 * noise(x, y, 9.0, 3))
    z += calm(x, y) * (mounds * (0.4 + 0.6 * wet) + sandwave * wet)
    return z + (FLOOR - z) * edge(x, y)


def beach_weight(z):
    """0 on the sea floor, 1 on the beach: the sand material blends to the beach texture set (just below the water)."""
    return smooth((z + 0.35) / 0.4)


def detail(x, y, z_base=None):
    """Sculpted beach on top of base(): (dz, crest, trough, tone). Zero under water."""
    z = base(x, y) if z_base is None else z_base
    tone = 0.5 + 0.5 * noise(x, y, 4.5, 7)
    beach = smooth((z + 0.15) / 0.3) * (1.0 - edge(x, y))
    if beach <= 1e-3:
        return 0.0, 0.0, 0.0, tone
    # Wind ripples on the dry sand: crests roughly along the shore, wandering and gently crooked, steep lee side.
    dry = smooth((z - 0.05) / 0.2)
    patch = smooth((0.5 + 0.5 * noise(x, y, 9.0, 11) + 0.25 * noise(x, y, 3.5, 12) + 0.12) / 0.25)
    theta = 0.35 * noise(x, y, 18.0, 13)
    u = x * math.cos(theta) + y * math.sin(theta)
    phi = 2.0 * math.pi * u / RIPPLE_SPACING + 3.2 * noise(x, y, 6.0, 14) + 1.3 * noise(x, y, 2.4, 15)
    phi += 0.5 * math.sin(phi)
    p = (0.5 + 0.5 * math.cos(phi)) ** 2.0
    ripple_amt = dry * patch * (0.7 + 0.3 * noise(x, y, 3.0, 16))
    dz = RIPPLE_HEIGHT * ripple_amt * (p - 0.375)
    # Soft mounds on the upper beach (wind-blown sand), chunky and rounded.
    upper = smooth((z - 0.3) / 0.25)
    n = 0.7 * noise(x, y, 3.2, 21) + 0.3 * noise(x, y, 1.5, 22)
    mound = 0.3 * max(n - 0.1, 0.0) ** 1.4 * upper
    dz += mound
    # Swash ridges: low lines left by the waves, following the shore's contours.
    swash = 0.0
    for k, zk in enumerate((-0.02, 0.07, 0.16)):
        swash += math.exp(-((z - zk) / 0.03) ** 2) * (0.55 + 0.45 * noise(x, y, 5.0, 30 + k))
    dz += 0.03 * swash * smooth((z + 0.1) / 0.1)
    dz *= beach
    crest = max(smooth((p - 0.55) / 0.35) * ripple_amt, smooth(mound / 0.12), min(swash, 1.0) * 0.6) * beach
    trough = smooth((0.18 - p) / 0.18) * ripple_amt * beach
    return dz, crest, trough, tone


def height(x, y):
    """Visible sand surface: base plus ripples."""
    zb = base(x, y)
    return zb + detail(x, y, zb)[0]
