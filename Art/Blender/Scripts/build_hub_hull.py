"""The hub ship's hull from outside (D62, reworked for D64): a giant, old galleon, hand-made like the player's boat (D38, D52,
D53), planked all the way down to the keel.

Every number comes from Docs/Design/HubShip/HubShip_Data.json through hub_layout.py (the hull's form with its run, entry and
round stern, the profile, the sheer, the decks 3 m apart, the doors, the masts' channels). Build choices that are not in the
data are named constants here (the strake width, the plank thickness, the wales, the port spacing).

The form: the planking follows H.half_width with the section's knuckles rounded (a monotone cubic through the data's section
points; it never lies inside the data's hull, so the rooms' lining stays clear of it). Strakes follow lines that rise with the
sheer towards the ends (the sheer fades out below the waterline and up into the castles, so the bottom runs level into the
keel and the castles' bands stay level with their decks). Below the bilge's knuckle the lines are closer, so the strakes keep
their width round the bilge; the flat bottom is planked fore and aft beside the keel.

The hidden doors (hub.doors): the planking, wales, bands and port lids at x 32..40 from the strake seam at about -2.5 to the
one under 13.0 belong to the door leaves (build_hub_doors.py builds them with the same generator); here they are left out.

Meshes (pivot at the ship's x 80 on the waterline, bow to +X; /Game/Saltbound/Hub/Art; Nanite; visual only):
  SM_Hub_HullStern   both sides aft of the quarterdeck's front (x < 48.3), the keel there, the door openings
  SM_Hub_HullWaistS  the waist's starboard side (x 48.3..134.3), the flat bottom and keel under it
  SM_Hub_HullWaistP  the waist's port side and the bottom's port half
  SM_Hub_HullBow     the forecastle's and the bow's sides up to the stem
Writes Art/Generated/hub_outside.json: every port, window and lid on the sides (side, x, z, kind) for the rooms behind them.

D69 (owner: "boring ... I don't say there can't be windows but only? Create reinforcements on the hull"): the rows differ - big
gunports with ochre frames on the gun deck, tarred lids further apart on the upper and middle decks, round brass portholes on the
lower and quay decks; heavy skids with iron bands over the wales every 12-16 m; carved rosettes along the main deck's band; an
entry port amidships each side with batten steps down to the water and two man-ropes (cables, hub_ropes.json "hull_spans");
iron-lipped scuppers along the deck line. The castles' windows behind the galleries are glazed doors.
Run: Tools/Build-BlenderAsset.ps1 build_hub_hull
"""
import json
import math
import os
import random
import sys

import bmesh  # noqa: F401
import bpy  # noqa: F401
from mathutils import Vector

sys.path.insert(0, os.path.dirname(__file__))
import hub_layout as H  # noqa: E402
import hub_timber as T  # noqa: E402
import sb_blender as sb  # noqa: E402

HUB, Z = H.HUB, H.Z
FORM = HUB["form"]
COUNTER = float(FORM["stern"]["counter"])           # the counter: the tuck below, the round stern above
POST_X = float(FORM["stern"]["post_x"])             # the sternpost's aft face and the tuck
ROUND = float(FORM["stern"]["round"])               # the stern face's corners lie this far forward of its middle
KEEL = float(HUB["keel"])
DOORS = HUB["doors"]
STRAKE = 0.55                        # strake width (m) up the sides: chunky for a ship this size
BILGE_STRAKE = 0.62                  # strake width round the bilge (z -12..-6), measured along the midship section
BOTTOM_W = 0.62                      # bottom planks beside the keel
PLANK_T = 0.14                       # plank thickness (outside the data's surface)
GAP = 0.03                           # the caulked seam between strakes
BOARD_LEN = (5.0, 9.5)               # board lengths along a strake (butts staggered from strake to strake)
TOP = max(z for _, z in HUB["profile"])
PARTS_X = (48.3, 134.3)              # the sides are split here into stern / waist / bow meshes (at the castles' steps)
KEEL_HW, KEEL_DOWN = 0.62, 0.42      # the keel's half-width and how far it stands below the bottom

# Material colours for the Blender previews; Unreal uses the MI_* of the same name (setup_hub_materials.py and the boat's).
COLOURS = {
    "MI_Hub_HullTar": (0.16, 0.11, 0.08), "MI_Hub_HullTarFaded": (0.24, 0.18, 0.13), "MI_Hub_HullTarDark": (0.1, 0.07, 0.05),
    "MI_Hub_Ochre": (0.62, 0.43, 0.16), "MI_Hub_OchreFaded": (0.7, 0.55, 0.3), "MI_Hub_AncientStone": (0.84, 0.82, 0.74),
    "MI_AncientGlow": (0.35, 0.95, 0.85), "MI_Caulk_Dark": (0.04, 0.035, 0.03), "MI_Wood_Tarred": (0.06, 0.05, 0.04),
    "MI_Paint_Cream": (0.86, 0.8, 0.64), "MI_Paint_CreamAged": (0.76, 0.68, 0.5), "MI_Metal_Iron": (0.08, 0.08, 0.08),
    "MI_Metal_Brass": (0.6, 0.45, 0.2), "MI_Glass_Dark": (0.12, 0.16, 0.17), "MI_LanternGlass": (1.0, 0.7, 0.35),
    "MI_Wood_Varnish": (0.36, 0.2, 0.08), "MI_Paint_Hull": (0.42, 0.11, 0.07), "MI_Paint_HullFaded": (0.5, 0.2, 0.13),
    "MI_Paint_HullDark": (0.3, 0.07, 0.05),
    "MI_Wood_Teak": (0.5, 0.33, 0.17), "MI_Wood_TeakDark": (0.38, 0.24, 0.12), "MI_Wood_TeakLight": (0.6, 0.42, 0.22),
    "MI_Rope_Hemp": (0.55, 0.45, 0.28), "MI_Hub_Void": (0.01, 0.01, 0.01), "MI_Hub_RopeTar": (0.12, 0.1, 0.08),
    "MI_Hub_Copper": (0.3, 0.55, 0.47),
}


def material(name):
    return sb.get_material(name, (*COLOURS.get(name, (0.5, 0.5, 0.5)), 1.0))


SEED = [9100]


def nseed():
    SEED[0] += 1
    return SEED[0]


def smooth01(t):
    t = min(max(t, 0.0), 1.0)
    return t * t * (3.0 - 2.0 * t)


# --- the hull's form ------------------------------------------------------------------------------------------------------
_SZ = [float(z) for z, _ in H.SECTION]
_SW = [float(w) for _, w in H.SECTION]


def _pchip_slopes():
    h = [b - a for a, b in zip(_SZ[:-1], _SZ[1:])]
    d = [(w1 - w0) / hh for w0, w1, hh in zip(_SW[:-1], _SW[1:], h)]
    m = [d[0]]
    for k in range(1, len(d)):
        if d[k - 1] * d[k] <= 0.0:
            m.append(0.0)
        else:
            w1, w2 = 2.0 * h[k] + h[k - 1], h[k] + 2.0 * h[k - 1]
            m.append((w1 + w2) / (w1 / d[k - 1] + w2 / d[k]))
    m.append(d[-1])
    return m


_SM = _pchip_slopes()


def sec_lin(z):
    return H._interp(H.SECTION, z)


def sec_smooth(z):
    """The section's half-width with its knuckles rounded (monotone cubic through the data's points; clamped like the data)."""
    if z <= _SZ[0]:
        return _SW[0]
    if z >= _SZ[-1]:
        return _SW[-1]
    for k in range(len(_SZ) - 1):
        if z <= _SZ[k + 1]:
            h = _SZ[k + 1] - _SZ[k]
            t = (z - _SZ[k]) / h
            h00, h10, h01, h11 = 2 * t ** 3 - 3 * t ** 2 + 1, t ** 3 - 2 * t ** 2 + t, -2 * t ** 3 + 3 * t ** 2, t ** 3 - t ** 2
            return h00 * _SW[k] + h10 * h * _SM[k] + h01 * _SW[k + 1] + h11 * h * _SM[k + 1]
    return _SW[-1]


def hw(x, z):
    """The planked hull's half-width: the data's (H.half_width) with the section's knuckles rounded."""
    return H.half_width(x, z) * sec_smooth(z) / max(sec_lin(z), 1e-6)


def surf(x, z, side):
    return Vector((x, side * hw(x, z), z))


def top(x):
    return H.profile_top(x)


def bottom(x):
    return H.profile_bottom(x)


def stern_lean(z):
    return H.stern_lean(z)


def corner_x(z):
    """Where the side meets the stern: the tuck's edge (x 1.6) below the counter, the round stern face's corner above."""
    return POST_X if z < COUNTER else ROUND - stern_lean(z)


def end_x(z):
    """Where the side planking ends forward at height z: in the stem's rabbet (the stem timber covers the ends)."""
    return H.stem_x(z) - 0.05


def outward(x, z, side, a):
    """The hull's outward normal at (x, z) given the across direction a (up the section)."""
    t = surf(x + 0.05, z, side) - surf(x - 0.05, z, side)
    n = t.cross(a)
    if n.length < 1e-9:
        return Vector((0.0, float(side), 0.0))
    n.normalize()
    if n.y * side < 0.0:
        n = -n
    return n


def hull_frame(x, z, side, slope=0.0):
    """(point on the hull, T along the ship, A up the side, N outward) at (x, z); slope tilts T and A with the sheer."""
    p = surf(x, z, side)
    a = (surf(x, z + 0.05, side) - surf(x, z - 0.05, side)).normalized()
    n = outward(x, z, side, a)
    t = a.cross(n).normalized()
    if t.x < 0.0:
        t = -t
    if slope:
        t, a = (t + a * slope).normalized(), (a - t * slope).normalized()
    return p, t, a, n


def lpt(F, u, v, d):
    p, t, a, n = F
    return p + t * u + a * v + n * d


def lbeam(part, F, u0, v0, u1, v1, width, thick, d, mat, wear=0.8, **kw):
    """A timber on the hull's local plane F from (u0, v0) to (u1, v1), its back at depth d (outward from the surface)."""
    p, t, a, n = F
    pa, pb = lpt(F, u0, v0, d + thick * 0.5), lpt(F, u1, v1, d + thick * 0.5)
    if (pb - pa).length < 1e-3:
        return
    across = n.cross((pb - pa).normalized())
    part.beam(T.straight(pa, pb, across, n), (pb - pa).length, width, thick, mat, nseed(), wear=wear, **kw)


# --- the sheer ------------------------------------------------------------------------------------------------------------
def sheer(x):
    """How far the lines rise at x (hub.sheer): towards the stern and towards the stem."""
    sh = HUB["sheer"]
    if x < sh["aft"]:
        t = min((sh["aft"] - x) / sh["aft"], 1.05)
        return sh["stern"] * t * t
    if x > sh["fore"]:
        t = (x - sh["fore"]) / (162.0 - sh["fore"])
        return sh["bow"] * t * t
    return 0.0


def sheer_k(z0):
    """How much of the sheer a line at midship height z0 takes: none on the bottom, all from the waterline belt to the gun
    deck, fading out up the castles (their bands stay level with their decks, under the level rails)."""
    return smooth01((z0 + 7.0) / 6.0) * (1.0 - smooth01((z0 - Z["gun"]) / (TOP - 8.0 - Z["gun"])))


def line_z(z0, x):
    return z0 + sheer(x) * sheer_k(z0)


def line_slope(z0, x):
    return (line_z(z0, x + 0.5) - line_z(z0, x - 0.5))


def grime_hull(p):
    """G: wet and green at the waterline, fading up; streaks under it."""
    return min(max((1.2 - abs(p.z - 0.2)) / 1.2, 0.0), 1.0) * 0.85


# --- the strake lines -------------------------------------------------------------------------------------------------------
def _grid_origin():
    """The midship height of the seam that lies at the doors' z0 at their middle: the side's seams are laid STRAKE apart from
    it, so the doors' bottom and top edges are seams (z0 at the middle; the top one at z1 at the aft jamb, where the sheer
    lifts it most)."""
    z0 = float(DOORS["z0"])
    st = DOORS["stations"][0]                                # the aft station: the sheer lifts its seams (the fore one is level)
    for _ in range(20):
        z0 -= line_z(z0, 0.5 * (st["x0"] + st["x1"])) - DOORS["z0"]
    return z0


def strake_lines():
    """Midship heights of the strake seams: from the keel's chine (z -12) round the bilge in equal steps of about
    BILGE_STRAKE along the midship section, then STRAKE apart (on the doors' grid) up to the top."""
    g0 = _grid_origin()
    k0 = int(math.floor((-6.0 - g0) / STRAKE))
    first = g0 + STRAKE * k0                               # the lowest grid seam over the bilge's knuckle
    girth, zz, marks = 0.0, KEEL, [(KEEL, 0.0)]
    while zz < first - 1e-9:
        z1 = min(zz + 0.02, first)
        girth += math.hypot(z1 - zz, sec_smooth(z1) - sec_smooth(zz))
        zz = z1
        marks.append((zz, girth))
    n = max(1, int(round(girth / BILGE_STRAKE)))
    zs = [KEEL]
    for k in range(1, n):
        g = girth * k / n
        zs.append(round(next(z for z, gg in marks if gg >= g), 4))
    z = first
    while z < TOP + 2.6:
        zs.append(round(z, 4))
        z += STRAKE
    return zs


LINES = strake_lines()


def nearest_line(z, x, below=None):
    """The index of the seam line nearest z at x (or the highest one at or below z when below is True)."""
    best = None
    for j, z0 in enumerate(LINES):
        zz = line_z(z0, x)
        if below and zz > z + 0.02:
            continue
        if best is None or abs(zz - z) < abs(line_z(LINES[best], x) - z):
            best = j
    return best


# The doors: their bottom and top edges follow seams (the top one stays under z1 even at the aft jamb, where the sheer lifts it).
# D70: a pair in each side at each basin (hub.doors.stations), aft first; the fore station lies where the sheer is level, so the
# same seams bound its leaves.
DOOR_XS = [(float(st["x0"]), 0.5 * (st["x0"] + st["x1"]), float(st["x1"])) for st in DOORS["stations"]]
DOOR_X = DOOR_XS[0]
DOOR_J = (nearest_line(DOORS["z0"], DOOR_X[1]), nearest_line(DOORS["z1"], DOOR_X[0], below=True))
DOOR_CUTS = tuple(x for dx in DOOR_XS for x in dx)


def door_band(z0a, z0b):
    """True when a band between midship heights z0a..z0b lies on the doors (within their seams)."""
    return z0a >= LINES[DOOR_J[0]] - 1e-6 and z0b <= LINES[DOOR_J[1]] + 1e-6


def door_leaf(x, z0a, z0b, side):
    """The leaf a piece at x with midship band z0a..z0b belongs to: (side, 'A' aft / 'F' fore) or None (whichever station)."""
    if not door_band(z0a, z0b):
        return None
    for x0, xm, x1 in DOOR_XS:
        if x0 < x < x1:
            return (side, "A" if x < xm else "F")
    return None


# --- routing: which part a piece goes into -----------------------------------------------------------------------------------
class Router:
    """Where the generated pieces go. hull: the four side meshes (door pieces skipped); doors: only the door pieces, into the
    leaf parts."""

    def __init__(self, parts, doors=None, x_range=(-5.0, 170.0)):
        self.parts, self.doors, self.x_range = parts, doors, x_range

    def __call__(self, x, side, z0a=None, z0b=None):
        leaf = door_leaf(x, z0a, z0b, side) if z0a is not None else None
        if self.doors is not None:
            return self.doors.get(leaf) if leaf else None
        if leaf:
            return None
        if x < PARTS_X[0]:
            return self.parts["stern"]
        if x > PARTS_X[1]:
            return self.parts["bow"]
        return self.parts["waistS"] if side > 0 else self.parts["waistP"]

    def wanted(self, xa, xb):
        return xb > self.x_range[0] and xa < self.x_range[1]


# --- the sides: strakes -------------------------------------------------------------------------------------------------------
def band_edges(za, zb, x):
    """The z of a band's lower and upper edge at x (lines za..zb, clipped by the bottom and the top)."""
    return max(line_z(za, x), bottom(x)), min(line_z(zb, x), top(x) - 0.02)


def runs_for(za, zb, cuts, holes, x_lo=-2.0, x_hi=163.0):
    """The x runs where the band (lines za..zb) exists on the side: under the top, over the bottom, between the stern's
    corner and the stem; broken at the cuts and round the openings it crosses (holes: (x0, x1, z0, z1))."""
    step = 0.25
    x = max(x_lo, -1.2)
    ok_prev, start = False, None
    runs = []
    while x <= min(x_hi, 163.0):
        zl, zu = band_edges(za, zb, x)
        zm = 0.5 * (zl + zu)
        ok = zu - zl >= 0.12 and corner_x(zu) + 0.04 <= x <= end_x(zm) and x >= corner_x(zl) + 0.04
        if ok and not ok_prev:
            start = x
        if not ok and ok_prev:
            runs.append((start, x - step))
        ok_prev = ok
        x += step
    if ok_prev:
        runs.append((start, x - step))
    out = []
    for a, b in runs:
        # Refine the ends to 2 cm (the scan's step leaves gaps at the stern's corner and the stem).
        a, b = _refine(za, zb, a, a - step, -1), _refine(za, zb, b, b + step, 1)
        pieces = [(a, b)]
        for c in cuts:
            nxt = []
            for p, q in pieces:
                nxt += [(p, c), (c, q)] if p + 0.05 < c < q - 0.05 else [(p, q)]
            pieces = nxt
        for hx0, hx1, hz0, hz1 in holes:
            nxt = []
            for p, q in pieces:
                zl, zu = line_z(za, 0.5 * (hx0 + hx1)), line_z(zb, 0.5 * (hx0 + hx1))
                if zu > hz0 + 0.01 and zl < hz1 - 0.01 and p < hx1 and q > hx0:
                    if hx0 - p > 0.1:
                        nxt.append((p, hx0))
                    if q - hx1 > 0.1:
                        nxt.append((hx1, q))
                else:
                    nxt.append((p, q))
            pieces = nxt
        out += [(p, q) for p, q in pieces if q - p > 0.12]
    return out


def _refine(za, zb, x_in, x_out, sgn):
    """Bisect between a scan point inside the band's run and one outside it."""
    def ok(x):
        zl, zu = band_edges(za, zb, x)
        return zu - zl >= 0.12 and corner_x(zu) + 0.04 <= x <= end_x(0.5 * (zl + zu)) and x >= corner_x(zl) + 0.04
    if ok(x_out):
        return x_in
    for _ in range(8):
        m = 0.5 * (x_in + x_out)
        if ok(m):
            x_in = m
        else:
            x_out = m
    return x_in


def strake_frame(side, za, zb, xa, xb, proud):
    def f(s):
        x = xa + (xb - xa) * s
        zl, zu = band_edges(za, zb, x)
        pl, pu = surf(x, zl, side), surf(x, zu, side)
        acr = pu - pl
        w = max(acr.length, 1e-4)
        a = acr / w
        n = outward(x, 0.5 * (zl + zu), side, a)
        return (pl + pu) * 0.5 + n * (PLANK_T * 0.5 + proud), a, n, max(w - GAP, 0.02) / (STRAKE - GAP)
    return f


def plank_mat(rng):
    r = rng.random()
    return "MI_Hub_HullTarFaded" if r < 0.22 else ("MI_Hub_HullTarDark" if r < 0.36 else "MI_Hub_HullTar")


def cuts_for(za, zb):
    return PARTS_X + (DOOR_CUTS if door_band(za, zb) else ())


def planking(route, holes_by_side):
    """Carvel strakes on both sides, each in boards of 5-9.5 m with the butts staggered; each board its own shade, a few
    millimetres proud or sunk; now and then a newer repair board (lighter, its edges less worn)."""
    rng = random.Random(62)
    for side in (1, -1):
        holes = holes_by_side[side]
        for j, (za, zb) in enumerate(zip(LINES[:-1], LINES[1:])):
            for ra, rb in runs_for(za, zb, cuts_for(za, zb), holes, *route.x_range):
                core_strip(route, side, za, zb, ra, rb)
                x = ra
                first = rng.uniform(1.5, BOARD_LEN[1])
                ends = []
                while x < rb - 1e-6:
                    ln = first if not ends else rng.uniform(*BOARD_LEN)
                    xe = min(x + ln, rb)
                    if rb - xe < 1.2:
                        xe = rb
                    ends.append((x, xe))
                    x = xe
                for xa, xb in ends:
                    proud = rng.uniform(-0.008, 0.012)
                    repair = rng.random() < 0.035
                    mat = "MI_Hub_HullTarFaded" if repair else plank_mat(rng)
                    wear, chamfer = (0.35 if repair else rng.uniform(0.6, 1.0)), rng.uniform(0.02, 0.035)
                    part = route(0.5 * (xa + xb), side, za, zb)
                    if part is None:
                        continue
                    fr = strake_frame(side, za, zb, xa + 0.006, xb - 0.006, proud)
                    deep = line_z(zb, 0.5 * (xa + xb)) < -3.0                 # under water: fewer rings
                    part.board(fr, T.length_of(fr, 8), STRAKE - GAP, PLANK_T, mat, nseed(), seg=1.2 if deep else 0.6,
                               wear=wear, grime=grime_hull, bend=0.02, twist=0.6, drift=0.03, slant=0.03, chamfer=chamfer)


def core_strip(route, side, za, zb, ra, rb):
    """The dark caulked shell under one strake's run, so the seams read as tarred grooves (as the boat's hull_core); it
    follows the runs, so the openings stay open."""
    n = max(1, int(math.ceil((rb - ra) / 1.0)))
    xs = [ra + (rb - ra) * k / n for k in range(n + 1)]
    for xa, xb in zip(xs[:-1], xs[1:]):
        part = route(0.5 * (xa + xb), side, za, zb)
        if part is None:
            continue
        pts = []
        for x, up in ((xa, False), (xb, False), (xb, True), (xa, True)):
            zl, zu = band_edges(za, zb, x)
            pts.append(surf(x, zu if up else zl, side))
        part.polygon(pts, "MI_Caulk_Dark", tone=0.2, facing=outward(0.5 * (xa + xb), 0.5 * (za + zb), side, Vector((0, 0, 1))))


# --- the bottom and the keel --------------------------------------------------------------------------------------------------
BOTTOM_X = (4.6, 131.5)


def chine_y(x):
    """The flat bottom's half-width at x (where the side meets it)."""
    return hw(x, bottom(x))


def bottom_planks(route):
    """The flat bottom beside the keel: planks fore and aft, the outer ones ending under a chine plank that follows the edge."""
    rng = random.Random(64)
    x0, x1 = BOTTOM_X
    for side in (1, -1):
        part_of = lambda x: route(x, side)                                                     # noqa: E731
        k = 0
        while True:
            yc = KEEL_HW - 0.05 + BOTTOM_W * (k + 0.5)
            k += 1
            if yc + BOTTOM_W * 0.5 > 6.3:
                break
            # Where this plank fits inside the chine plank's middle.
            xs = [x for x in [x0 + 0.25 * i for i in range(int((x1 - x0) / 0.25) + 1)] if yc + BOTTOM_W * 0.5 < chine_y(x) - 0.3]
            if not xs:
                continue
            a, b = xs[0], xs[-1]
            x = a
            while x < b - 0.2:
                xe = min(x + rng.uniform(6.0, 10.0), b)
                if b - xe < 1.5:
                    xe = b
                part = part_of(0.5 * (x + xe))
                if part is not None and route.wanted(x, xe):
                    pts = [Vector((xx, side * yc, bottom(xx) - PLANK_T * 0.5)) for xx in (x + 0.01, 0.5 * (x + xe), xe - 0.01)]
                    fr = T.polyline(pts, Vector((0.0, 1.0, 0.0)), Vector((0.0, 0.0, -1.0)))
                    part.board(fr, T.length_of(fr), BOTTOM_W - GAP, PLANK_T, plank_mat(rng), nseed(), seg=1.5, wear=0.5, bend=0.01,
                               twist=0.3, chamfer=0.025)
                    part.polygon([(x, side * (yc - BOTTOM_W * 0.5), bottom(x)), (xe, side * (yc - BOTTOM_W * 0.5), bottom(xe)),
                                  (xe, side * (yc + BOTTOM_W * 0.5), bottom(xe)), (x, side * (yc + BOTTOM_W * 0.5), bottom(x))],
                                 "MI_Caulk_Dark", tone=0.2, facing=(0.0, 0.0, -1.0))
                x = xe
        # The chine piece over the corner where the bottom meets the side: it takes the bottom planks' ends and the side
        # strakes' ends where the bottom rises towards the ends.
        x = x0
        while x < x1 - 0.2:
            xe = min(x + rng.uniform(5.0, 8.0), x1)
            part = part_of(0.5 * (x + xe))
            if part is not None and route.wanted(x, xe):
                n = max(2, int((xe - x) / 1.0))

                def chine_frame(s, x=x, xe=xe):
                    xx = x + (xe - x) * s
                    zb = bottom(xx)
                    up = (surf(xx, zb + 0.6, side) - surf(xx, zb, side)).normalized()        # up the side from the chine
                    out_b = Vector((0.0, 0.0, -1.0))
                    out_s = outward(xx, zb + 0.3, side, up)
                    nrm = (out_b + out_s).normalized()
                    acr = (up + Vector((0.0, side, 0.0))).normalized()                     # across the corner
                    acr = (acr - nrm * acr.dot(nrm)).normalized()
                    return Vector((xx, side * chine_y(xx), zb)) + nrm * 0.12, acr, nrm, 1.0
                part.beam(chine_frame, T.length_of(chine_frame, n), 1.1, 0.26, "MI_Hub_HullTarDark", nseed(), seg=1.5, wear=0.6, bend=0.01,
                          twist=0.3, drift=0.0)
            x = xe


def keel(route):
    """The keel along the bottom's middle: oak in lengths scarfed together, standing below the bottom (the stem and the
    sternpost are the ends' timbers)."""
    rng = random.Random(65)
    x = BOTTOM_X[0] - 0.3
    while x < 132.0:
        xe = min(x + rng.uniform(9.0, 13.0), 132.6)
        if 132.6 - xe < 3.0:
            xe = 132.6
        part = route(0.5 * (x + xe), 1)
        if part is not None and route.wanted(x, xe):
            n = max(2, int((xe - x) / 1.5))
            # Its bottom KEEL_DOWN under the bottom planks' face, its top 0.3 up inside the hull.
            depth = PLANK_T + KEEL_DOWN + 0.3
            pts = [Vector((xx, 0.0, bottom(xx) + 0.3 - depth * 0.5)) for xx in [x + 0.01 + (xe - x - 0.02) * i / n for i in range(n + 1)]]
            fr = T.polyline(pts, Vector((0.0, 1.0, 0.0)), Vector((0.0, 0.0, 1.0)))
            part.beam(fr, T.length_of(fr), 2 * KEEL_HW, depth, "MI_Hub_HullTarDark", nseed(), seg=1.5, wear=0.8,
                      grime=lambda q: 0.6)
        x = xe


# --- bands and wales ------------------------------------------------------------------------------------------------------------
# (midship centre z, height, proud, material, moulded): the waterline belt of three heavy tarred wales, wales at the deck lines,
# broad ochre bands at the main deck, the quarterdeck and the poop.
WALES = [
    (-0.55, 0.55, 0.34, "MI_Wood_Tarred", True), (0.45, 0.6, 0.38, "MI_Wood_Tarred", True), (Z["quay"] - 0.02, 0.6, 0.36, "MI_Wood_Tarred", True),
    (Z["lower"], 0.42, 0.22, "MI_Hub_Ochre", True), (Z["middle"], 0.4, 0.2, "MI_Wood_Tarred", True),
    (Z["upper"], 0.42, 0.22, "MI_Hub_Ochre", True), (Z["gun"], 0.45, 0.24, "MI_Wood_Tarred", True),
    (Z["main"], 0.85, 0.26, "MI_Hub_Ochre", False),
    (Z["sc1"], 0.38, 0.18, "MI_Hub_Ochre", True), (Z["sc2"], 0.85, 0.24, "MI_Hub_Ochre", False),
    (Z["sc3"], 0.38, 0.18, "MI_Hub_Ochre", True), (Z["poop"], 0.85, 0.24, "MI_Hub_Ochre", False),
]


def moulding_section(h, p):
    """A wale's section (across up the hull, out from it): a flat back, a fillet and a full half-round face that catches the
    light (D64, owner: more shape)."""
    f = min(0.05, h * 0.12)
    sec = [(-h * 0.5, 0.0, ""), (h * 0.5, 0.0, ""), (h * 0.5, p * 0.28, "c"), (h * 0.5 - f, p * 0.36, "c")]
    n = 7
    for k in range(1, n):
        th = math.pi * k / n
        sec.append(((h * 0.5 - f) * math.cos(th), p * 0.36 + p * 0.64 * math.sin(th), "t" if 1 < k < n - 1 else "c"))
    sec += [(-h * 0.5 + f, p * 0.36, "c"), (-h * 0.5, p * 0.28, "c")]
    return sec


def band_frame(side, z0, h, xa, xb, out=0.0):
    def f(s):
        x = xa + (xb - xa) * s
        zl, zu = line_z(z0 - h * 0.5, x), line_z(z0 + h * 0.5, x)
        zu = min(zu, top(x) - 0.03)
        zl = min(zl, zu - 0.05)
        pl, pu = surf(x, zl, side), surf(x, zu, side)
        acr = pu - pl
        w = max(acr.length, 1e-4)
        a = acr / w
        n = outward(x, 0.5 * (zl + zu), side, a)
        return (pl + pu) * 0.5 + n * (PLANK_T + out), a, n, w / h
    return f


def wales(route, holes_by_side):
    rng = random.Random(77)
    for side in (1, -1):
        for z0, h, proud, mat, moulded in WALES:
            za, zb = z0 - h * 0.5, z0 + h * 0.5
            for ra, rb in runs_for(za, zb, cuts_for(za, zb), holes_by_side[side], *route.x_range):
                x = ra
                while x < rb - 1e-6:
                    xe = min(x + rng.uniform(7.0, 12.0), rb)
                    if rb - xe < 2.0:
                        xe = rb
                    part = route(0.5 * (x + xe), side, za, zb)
                    x0, x = x, xe
                    if part is None:
                        continue
                    m = mat if mat != "MI_Hub_Ochre" or rng.random() > 0.25 else "MI_Hub_OchreFaded"
                    if moulded:
                        fr = band_frame(side, z0, h, x0 + 0.01, xe - 0.01)
                        part.sweep(fr, T.length_of(fr, 8), moulding_section(h, proud), m, nseed(), seg=0.7, wear=0.9,
                                   grime=grime_hull, bend=0.015, twist=0.3, drift=0.02, wobble=0.006, slant=0.03)
                    else:
                        fr = band_frame(side, z0, h, x0 + 0.01, xe - 0.01, proud * 0.5)
                        part.beam(fr, T.length_of(fr, 8), h, proud, m, nseed(), seg=0.7, wear=0.9, grime=grime_hull, bend=0.015,
                                  twist=0.4, chamfer=min(proud, h) * 0.3)
                        # A thin tarred fillet along each edge of the broad bands: they read as mouldings, not stripes.
                        for e in (-1.0, 1.0):
                            fe = band_frame(side, z0 + e * (h * 0.5 + 0.05), 0.1, x0 + 0.01, xe - 0.01, 0.0)
                            part.beam(fe, T.length_of(fe, 8), 0.1, proud + 0.06, "MI_Wood_Tarred", nseed(), seg=0.9, wear=0.6,
                                      bend=0.01, twist=0.2)


# --- ports, windows, channels, cap rails ------------------------------------------------------------------------------------------
JAMB = 0.2                          # frame timbers round an opening
PORT = 1.0                          # outside.sides: lidded square ports of 1 m below the main deck
WIN_W, WIN_H = 1.3, 2.0             # cream-framed windows above it
# D69: each deck its own row: (deck, sill over the deck, port size, pitch, first x, style). gun: big gunports, ochre-framed;
# upper / middle: tarred lids twice as far apart; lower / quay: round brass portholes.
PORT_ROWS = [("quay", 1.1, 0.62, 6.0, 9.0, "round"), ("lower", 1.1, 0.62, 6.0, 12.0, "round"), ("middle", 1.0, 0.9, 8.0, 10.0, "lid"),
             ("upper", 1.0, 0.9, 8.0, 14.0, "lid"), ("gun", 0.9, 1.15, 4.0, 6.0, "gun")]
WIN_ROWS = [Z[k] + 0.8 for k in ("main", "sc1", "sc2", "sc3")]
CHANNELS = {m: (float(HUB["rig"]["channels"][m]), HUB["rig"]["shrouds"][m]) for m in HUB["rig"]["shrouds"]}   # length, deadeyes
GAL_X1 = float(HUB["galleries"]["x1"])                 # D69: the galleries run along the sides aft of here
GAL_ROWS = (Z["sc1"] + 0.8, Z["sc2"] + 0.8)            # their floors' window rows: glazed doors onto them
# D69: the skids (vertical fenders over the wales, in pairs) and the entry port amidships (ship x).
FENDERS = (21.0, 46.0, 56.5, 76.5, 116.0, 126.5)
ENTRY_X = 85.5


def mast_x(mid):
    return next(m["x"] for m in HUB["masts"] if m["id"] == mid)


def channel_span(mid):
    ln = CHANNELS[mid][0]
    return mast_x(mid) - ln * 0.5, mast_x(mid) + ln * 0.5


def rail_z(x):
    return top(x)


def channel_z(mid):
    return rail_z(mast_x(mid)) - 0.45


def bow_turn(x, z, side):
    """How much the hull turns at (x, z): the outward normal's |x| (0 amidships, about 1 at the stem)."""
    return abs(hull_frame(x, z, side)[3].x)


def data_openings():
    """The open spaces' ports through the hull side (the Tally House's hull_ports): (side, x0, x1, z0, z1), absolute z."""
    out = []
    floors = {"slip": Z["quay"], "tally": Z["lower"], "chandlery": Z["main"]}
    for name, fl in floors.items():
        for pr in H.space(name)["props"]:
            wall = pr.get("wall", "")
            if not wall.startswith("y=") or abs(float(wall[2:])) < 18.9:
                continue
            for x0, x1 in pr.get("spans") or []:
                out.append((1 if float(wall[2:]) > 0 else -1, float(x0), float(x1), fl + pr.get("z0", 0.8), fl + pr.get("z1", 2.0)))
    return out


def fittings():
    """Every opening and lid on the sides: the Tally House's ports (data), lidded ports on every deck below the main deck,
    windows on the castles' storeys. kind: room (a real room behind it, open), shut, open, lit (lid hauled up, a lantern
    inside), patched (lid gone, boarded), glass, glow (a lit window), shutter. z0 is the sill's midship height (the sheer
    lifts it towards the ends)."""
    rng = random.Random(512)
    out = []
    for side, x0, x1, z0, z1 in data_openings():
        out.append(dict(side=side, x0=x0, x1=x1, z0=z0, z1=z1, kind="room"))

    def clear(side, x, z0, z1, margin):
        for f in out:
            if f["side"] == side and f["x0"] - margin < x < f["x1"] + margin and f["z0"] - 1.0 < z1 and f["z1"] + 1.0 > z0:
                return False
        return True

    def chain_clear(x, z):
        for mid in CHANNELS:
            a, b = channel_span(mid)
            if a - 1.5 < x < b + 1.5 and channel_z(mid) - 5.2 < z < channel_z(mid) + 0.5:
                return False
        return True

    def skid_clear(x, half):
        return all(abs(x - f) > half + 0.75 for f in FENDERS) and abs(x - ENTRY_X) > half + 1.4

    def seam_clear(x, half, z0, z1):
        cuts = PARTS_X + (HUB["poopX"], HUB["qdX"], HUB["fcX"]) + (DOOR_CUTS if door_band(z0 - 0.6, z1 + 0.6) or door_band(z0, z1) else ())
        return all(abs(x - c) > half + 0.45 for c in cuts)

    for side in (1, -1):
        for deck_id, sill, size, pitch, x_first, style in PORT_ROWS:
            zs = Z[deck_id] + sill
            x = x_first
            while x < 158.0:
                zl = line_z(zs, x)
                ok = (x > corner_x(zl) + 4.0 and x + size * 0.5 < end_x(zl) - 3.0 and bow_turn(x, zl + 0.5, side) < 0.3
                      and chain_clear(x, zl + size) and seam_clear(x, size * 0.5, zs, zs + size) and clear(side, x, zs, zs + size, 2.0)
                      and skid_clear(x, size * 0.5) and line_z(zs + size, x) < top(x) - 1.0)
                if ok:
                    on_door = door_leaf(x, zs, zs + size, side) is not None
                    r = rng.random()
                    if style == "round":
                        kind = "round" if (r < 0.8 or on_door) else "round_lit"
                    elif style == "gun":
                        kind = "shut" if (r < 0.55 or on_door) else ("open" if r < 0.85 else ("lit" if r < 0.93 else "patched"))
                    else:
                        kind = "lid" if (r < 0.75 or on_door) else ("open" if r < 0.9 else "patched")
                    out.append(dict(side=side, x0=x - size * 0.5, x1=x + size * 0.5, z0=zs, z1=zs + size, kind=kind))
                x += pitch
        for zs in WIN_ROWS:
            x = 15.6
            while x < 158.0:
                zl = line_z(zs, x)
                castle = x < HUB["qdX"] or x > HUB["fcX"]
                gal = zs in GAL_ROWS and x < GAL_X1 + 0.5               # D69: onto a gallery: a glazed door, sill at the floor
                z0, z1 = (zs - 0.65, zs + WIN_H + 0.2) if gal else (zs, zs + WIN_H)
                ok = (castle and line_z(z1, x) < top(x) - 0.9 and x + WIN_W * 0.5 < end_x(zl) - 2.5
                      and bow_turn(x, zl + 1.0, side) < 0.35 and chain_clear(x, zl + WIN_H) and seam_clear(x, WIN_W * 0.5 + 0.4, zs, zs + WIN_H))
                if ok:
                    r = rng.random()
                    kind = "glass" if (r < 0.5 or gal) else ("glow" if r < 0.68 else "shutter")
                    if gal and r > 0.75:
                        kind = "glow"
                    out.append(dict(side=side, x0=x - WIN_W * 0.5, x1=x + WIN_W * 0.5, z0=z0, z1=z1, kind=kind))
                x += 3.6
    # Strake-aligned holes for every opening with nothing in front of it (at the opening's middle).
    for f in out:
        xc = 0.5 * (f["x0"] + f["x1"])
        lz = [line_z(l, xc) for l in LINES]
        zl0, zl1 = line_z(f["z0"], xc), line_z(f["z1"], xc)
        f["hz0"] = max([z for z in lz if z <= zl0 + 0.02] or [zl0])
        f["hz1"] = min([z for z in lz if z >= zl1 - 0.02] or [zl1])
        f["hole"] = f["kind"] not in ("shut", "patched", "lid", "round", "round_lit")
    return out


def holes_of(fits):
    holes = {1: [], -1: []}
    for f in fits:
        if f["hole"]:
            holes[f["side"]].append((f["x0"] - JAMB * 0.5, f["x1"] + JAMB * 0.5, f["hz0"] + 0.02, f["hz1"] - 0.02))
    return holes


def fitting(part, f, F=None, rng=None):
    """One opening or lid on the hull's local plane at its middle (tilted with the sheer), or on the plane F given."""
    rng = rng or random.Random(nseed())
    side, x0, x1 = f["side"], f["x0"], f["x1"]
    xc = 0.5 * (x0 + x1)
    zl0, zl1 = line_z(f["z0"], xc), line_z(f["z1"], xc)
    zc = 0.5 * (zl0 + zl1)
    F = F or hull_frame(xc, zc, side, slope=line_slope(f["z0"], xc))
    hw_, hh = 0.5 * (x1 - x0), 0.5 * (zl1 - zl0)
    kind = f["kind"]
    window = kind in ("glass", "glow", "shutter")
    frame_mat = "MI_Paint_Cream" if window else ("MI_Hub_HullTarDark" if kind != "room" else "MI_Paint_CreamAged")
    if kind in ("round", "round_lit"):
        # D69: a round brass porthole on the planking: a heavy ring with bolts, the glass (dark, or lit from inside), a hood.
        r = hw_
        c = lpt(F, 0.0, 0.0, PLANK_T + 0.02)
        part.lathe([(r + 0.12, 0.0), (r + 0.14, 0.05), (r + 0.1, 0.12), (r - 0.02, 0.12), (r - 0.04, 0.06), (r - 0.04, 0.0)], c, F[3], "MI_Metal_Brass",
                   sides=18, caps=False, wear=0.9)
        part.lathe([(0.0, 0.04), (r - 0.03, 0.04)], c, F[3], "MI_LanternGlass" if kind == "round_lit" else "MI_Glass_Dark", sides=18, caps=False)
        for k in range(8):
            a = 2.0 * math.pi * (k + 0.5) / 8
            q = c + (F[1] * math.cos(a) + F[2] * math.sin(a)) * (r + 0.06)
            part.lathe([(0.0, 0.0), (0.03, 0.0), (0.03, 0.13), (0.0, 0.15)], q, F[3], "MI_Metal_Brass", sides=6)
        lbeam(part, F, -r - 0.2, r + 0.28, r + 0.2, r + 0.28, 0.1, 0.14, PLANK_T, "MI_Hub_HullTarDark", wear=0.8)
        return
    if window and rng.random() < 0.3:
        frame_mat = "MI_Paint_CreamAged"
    d0 = PLANK_T - 0.02
    if f.get("hole"):
        # The frame: jambs over the cut board ends, a head and a sill filling to the strake lines.
        hz0, hz1 = f["hz0"] - zc, f["hz1"] - zc
        lbeam(part, F, -hw_ - JAMB * 0.5, hz0 - 0.05, -hw_ - JAMB * 0.5, hz1 + 0.05, JAMB, 0.14, d0, frame_mat)
        lbeam(part, F, hw_ + JAMB * 0.5, hz0 - 0.05, hw_ + JAMB * 0.5, hz1 + 0.05, JAMB, 0.14, d0, frame_mat)
        head = max(hz1 - hh, 0.16)
        lbeam(part, F, -hw_ - JAMB, hh + head * 0.5, hw_ + JAMB, hh + head * 0.5, head, 0.16, d0 + 0.01, frame_mat)
        sill = max(-hh - hz0, 0.16)
        lbeam(part, F, -hw_ - JAMB - 0.08, -hh - sill * 0.5, hw_ + JAMB + 0.08, -hh - sill * 0.5, sill, 0.2, d0 + 0.01, frame_mat)
        # The reveal: the opening's lining through the planking and the frames (it stays inside the hull's 0.35 m).
        for u0, v0, u1, v1 in ((-hw_ + 0.04, -hh, -hw_ + 0.04, hh), (hw_ - 0.04, -hh, hw_ - 0.04, hh),
                               (-hw_, -hh + 0.04, hw_, -hh + 0.04), (-hw_, hh - 0.04, hw_, hh - 0.04)):
            lbeam(part, F, u0, v0, u1, v1, 0.08, 0.42, -0.28, "MI_Hub_HullTarDark", wear=0.3)
    if kind in ("open", "lit", "room"):
        # A lid hinged at its head, hauled up and out: planked outside, the inside painted oxblood like the boat's hull.
        ang = math.radians(rng.uniform(48.0, 72.0))
        p, t, a, n = F
        hinge = lpt(F, 0.0, hh + 0.04, PLANK_T + 0.02)
        down = -a * math.cos(ang) + n * math.sin(ang)            # hinge -> free edge, swung out
        face = n * math.cos(ang) + a * math.sin(ang)             # the lid's outer face
        L = 2 * hh + 0.08
        for k in range(3):
            u = -hw_ - 0.05 + (2 * hw_ + 0.1) * (k + 0.5) / 3
            pa, pb = hinge + t * u + face * 0.05, hinge + t * u + down * L + face * 0.05
            part.beam(T.straight(pa, pb, t, face), L, (2 * hw_ + 0.1) / 3 - 0.02, 0.1,
                      "MI_Hub_HullTar" if k != 1 else "MI_Hub_HullTarFaded", nseed(), wear=0.8)
            part.beam(T.straight(pa - face * 0.075, pb - face * 0.075, t, face), L, (2 * hw_ + 0.1) / 3 - 0.02, 0.05,
                      "MI_Paint_Hull", nseed(), wear=0.9)
        for u in (-hw_ * 0.55, hw_ * 0.55):
            pa = hinge + t * u + face * 0.115
            part.beam(T.straight(pa, pa + down * (L * 0.85), t, face), L * 0.85, 0.1, 0.03, "MI_Metal_Iron", nseed(), wear=0.5)
        pa = hinge + down * L + face * 0.12
        pb = lpt(F, 0.0, hh + 0.7, PLANK_T)
        part.round(T.straight(pa, pb, t, n), (pb - pa).length, 0.025, "MI_Rope_Hemp", nseed(), sides=5, seg=2.0, wear=0.0)
        if kind in ("open", "lit"):
            # Inside: the dark of the deck, or a lantern's glow (the back stays within the hull's thickness).
            mat = "MI_LanternGlass" if kind == "lit" else "MI_Hub_Void"
            part.polygon([lpt(F, u, v, -0.3) for u, v in ((-hw_, -hh), (-hw_, hh), (hw_, hh), (hw_, -hh))], mat, tone=0.1, facing=F[3])
    if kind == "shut":
        shut_lid(part, F, hw_, hh, rng)
    if kind == "lid":
        shut_lid(part, F, hw_, hh, rng, edge="MI_Hub_HullTarDark")
    if kind == "patched":
        # The lid is gone: boards nailed across the port, unpainted and newer.
        lbeam(part, F, -hw_ - 0.1, hh + 0.14, hw_ + 0.1, hh + 0.14, 0.16, 0.08, PLANK_T - 0.02, "MI_Hub_HullTarDark")
        lbeam(part, F, -hw_ - 0.1, -hh - 0.14, hw_ + 0.1, -hh - 0.14, 0.18, 0.1, PLANK_T - 0.02, "MI_Hub_HullTarDark")
        for v in (-0.32, 0.02, 0.34):
            tilt = rng.uniform(-0.12, 0.12)
            lbeam(part, F, -hw_ - 0.12, v * hh * 2 - tilt, hw_ + 0.12, v * hh * 2 + tilt, 0.26, 0.05, PLANK_T, "MI_Wood_TeakLight", wear=0.3)
    if window:
        # Glass in a cross of mullions (some lit from inside), or shut behind two oxblood shutters.
        glass = "MI_LanternGlass" if kind == "glow" else "MI_Glass_Dark"
        part.polygon([lpt(F, u, v, -0.25) for u, v in ((-hw_, -hh), (hw_, -hh), (hw_, hh), (-hw_, hh))], glass, tone=0.5, facing=F[3])
        lbeam(part, F, 0.0, -hh, 0.0, hh, 0.09, 0.08, -0.25, frame_mat, wear=0.6)
        for v in (-hh * 0.33, hh * 0.33):
            lbeam(part, F, -hw_, v, hw_, v, 0.08, 0.08, -0.25, frame_mat, wear=0.6)
        lbeam(part, F, -hw_ - 0.3, hh + 0.45, hw_ + 0.3, hh + 0.45, 0.14, 0.24, PLANK_T + 0.1, frame_mat)     # the hood
        if kind == "shutter":
            for sgn in (-1.0, 1.0):
                for k in range(3):
                    u = sgn * (hw_ * (k + 0.5) / 3)
                    lbeam(part, F, u, -hh + 0.02, u, hh - 0.02, hw_ / 3 - 0.015, 0.06, -0.1,
                          "MI_Paint_Hull" if rng.random() < 0.7 else "MI_Paint_HullFaded", wear=1.0)
                lbeam(part, F, sgn * 0.05, hh * 0.6, sgn * (hw_ - 0.05), hh * 0.6, 0.12, 0.04, -0.04, "MI_Metal_Iron")
                lbeam(part, F, sgn * 0.05, -hh * 0.6, sgn * (hw_ - 0.05), -hh * 0.6, 0.12, 0.04, -0.04, "MI_Metal_Iron")


def shut_lid(part, F, hw_, hh, rng, edge=None):
    """A port's lid shut in its frame: three boards, two hinge straps, a ring. The gunports' frames are ochre (they read from
    afar); edge gives the frame's material otherwise (D69: tarred on the upper and middle decks)."""
    side_mat, head_mat = (edge, edge) if edge else ("MI_Hub_OchreFaded", "MI_Hub_Ochre")
    for sgn in (-1.0, 1.0):
        lbeam(part, F, sgn * (hw_ + 0.12), -hh - 0.22, sgn * (hw_ + 0.12), hh + 0.22, 0.14, 0.09, PLANK_T - 0.02, side_mat, wear=1.0)
    lbeam(part, F, -hw_ - 0.2, hh + 0.14, hw_ + 0.2, hh + 0.14, 0.16, 0.1, PLANK_T - 0.02, head_mat)
    lbeam(part, F, -hw_ - 0.2, -hh - 0.14, hw_ + 0.2, -hh - 0.14, 0.18, 0.12, PLANK_T - 0.02, head_mat)
    for k in range(3):
        u = -hw_ + (2 * hw_) * (k + 0.5) / 3
        lbeam(part, F, u, -hh, u, hh, 2 * hw_ / 3 - 0.02, 0.1, PLANK_T - 0.02, rng.choice(("MI_Hub_HullTar", "MI_Hub_HullTar", "MI_Hub_HullTarFaded")))
    for u in (-hw_ * 0.55, hw_ * 0.55):
        lbeam(part, F, u, hh + 0.05, u, -hh * 0.6, 0.1, 0.03, PLANK_T + 0.08, "MI_Metal_Iron", wear=0.5)
    part.lathe([(0.0, 0.0), (0.07, 0.0), (0.07, 0.03), (0.0, 0.03)], lpt(F, 0.0, -hh + 0.15, PLANK_T + 0.08), F[3], "MI_Metal_Iron", sides=8)


def build_fittings(route, fits):
    for k, f in enumerate(fits):
        xc = 0.5 * (f["x0"] + f["x1"])
        if not route.wanted(f["x0"], f["x1"]):
            continue
        part = route(xc, f["side"], f["z0"], f["z1"])
        if part is not None:
            fitting(part, f, rng=random.Random(4040 + k))


def pilasters(route, fits):
    """Oxblood pilasters with ochre capitals between the castles' side windows (as on the stern face), from the storey's band
    to the next one: the castles read as panelled, not as a flat wall with holes."""
    rows = {}
    for f in fits:
        if f["kind"] in ("glass", "glow", "shutter"):
            rows.setdefault((f["side"], f["z0"]), []).append(0.5 * (f["x0"] + f["x1"]))
    for (side, zs), xs in rows.items():
        xs.sort()
        spots = []
        for a, b in zip(xs[:-1], xs[1:]):
            if b - a < 3.8:
                spots.append(0.5 * (a + b))
        if xs:
            spots += [xs[0] - 1.8, xs[-1] + 1.8]
        for x in spots:
            zl, zu = line_z(zs - 0.38, x), line_z(zs + WIN_H + 0.55, x)
            zu = min(zu, top(x) - 0.45)
            if zu - zl < 1.0 or x < corner_x(zl) + 1.0 or x > end_x(zu) - 2.0 or bow_turn(x, zl, side) > 0.4:
                continue
            if any(abs(x - c) < 0.5 for c in (HUB["poopX"], HUB["qdX"], HUB["fcX"])) or (HUB["poopX"] < x < HUB["poopX"] + 0.9):
                continue
            part = route(x, side)
            if part is None or not route.wanted(x - 1, x + 1):
                continue
            F = hull_frame(x, 0.5 * (zl + zu), side, slope=line_slope(zs, x))
            hh = 0.5 * (zu - zl)
            lbeam(part, F, 0.0, -hh, 0.0, hh, 0.44, 0.12, PLANK_T, "MI_Paint_Hull", wear=1.0)
            lbeam(part, F, 0.0, hh - 0.3, 0.0, hh, 0.62, 0.19, PLANK_T, "MI_Hub_Ochre", wear=1.0)
            lbeam(part, F, 0.0, -hh, 0.0, -hh + 0.22, 0.56, 0.16, PLANK_T, "MI_Hub_Ochre", wear=1.0)




def fenders(route):
    """Pairs of upright skids over the wales from the waterline belt to the main deck's band, bolted through the wales (D69: six
    pairs a side, iron-banded at every wale; they break the rows and show the hull is strengthened where boats lie alongside)."""
    for side in (1, -1):
        for xc in FENDERS:
            for dx in (-0.32, 0.32):
                x = xc + dx
                part = route(x, side)
                if part is None or not route.wanted(x - 1, x + 1):
                    continue
                z0, z1 = line_z(-0.9, x), line_z(Z["main"] - 0.3, x)
                n = 10
                pts = []
                for k in range(n + 1):
                    z = z0 + (z1 - z0) * k / n
                    p, t, a, nn = hull_frame(x, z, side)
                    pts.append(p + nn * (PLANK_T + 0.42 + 0.14))
                fr = T.polyline(pts, Vector((1.0, 0.0, 0.0)), lambda q, s=side: hull_frame(x, q.z, s)[3])
                part.beam(fr, T.length_of(fr), 0.34, 0.28, "MI_Hub_HullTarDark", nseed(), seg=0.9, wear=1.0, grime=grime_hull)
                for z0_, h, proud, mat, moulded in WALES:
                    if z0_ > Z["main"] - 0.5:
                        continue
                    z = line_z(z0_, x)
                    F = hull_frame(x, z, side)
                    lbeam(part, F, -0.2, 0.0, 0.2, 0.0, 0.16, 0.06, PLANK_T + 0.7, "MI_Metal_Iron", wear=0.5)
                    for v in (-0.05, 0.05):
                        part.lathe([(0.0, 0.0), (0.045, 0.0), (0.04, 0.03), (0.0, 0.04)], lpt(F, 0.0, v, PLANK_T + 0.76), F[3], "MI_Metal_Iron", sides=6)


def rosettes(route):
    """D69: carved rosettes along the main deck's broad band (an ochre flower on a dark boss, a brass button), every 3.2 m where
    the band runs clear of the channels, the skids, the entry port and the castles' steps."""
    for side in (1, -1):
        x = 52.0
        while x < 132.0:
            ok = (all(not (channel_span(m)[0] - 1.2 < x < channel_span(m)[1] + 1.2) for m in CHANNELS) and all(abs(x - f) > 1.0 for f in FENDERS)
                  and abs(x - ENTRY_X) > 2.2)
            part = route(x, side)
            if ok and part is not None and route.wanted(x - 1, x + 1):
                F = hull_frame(x, line_z(Z["main"], x), side)
                c = lpt(F, 0.0, 0.0, PLANK_T + 0.26)
                part.lathe([(0.0, 0.0), (0.3, 0.0), (0.3, 0.05), (0.24, 0.1), (0.0, 0.12)], c, F[3], "MI_Hub_HullTarDark", sides=16, wear=0.8)
                for k in range(8):
                    a = 2.0 * math.pi * k / 8
                    d = F[1] * math.cos(a) + F[2] * math.sin(a)
                    q0, q1 = c + d * 0.06 + F[3] * 0.1, c + d * 0.27 + F[3] * 0.08
                    part.beam(T.straight(q0, q1, d.cross(F[3]), F[3]), 0.22, 0.12, 0.07, "MI_Hub_Ochre", nseed(), seg=0.3, wear=1.0)
                part.lathe([(0.0, 0.0), (0.07, 0.0), (0.08, 0.04), (0.0, 0.08)], c + F[3] * 0.12, F[3], "MI_Metal_Brass", sides=10)
            x += 3.2


def entry_ports(route):
    """D69: an entry port amidships on each side (ENTRY_X): a carved hood over the rail between two posts with brass caps, batten
    steps down the side to the waterline belt, and two man-ropes from the posts down beside the steps (real cables: hub_ropes.json
    hull_spans)."""
    spans = []
    for side in (1, -1):
        x = ENTRY_X
        part = route(x, side)
        if part is None or not route.wanted(x - 2, x + 2):
            continue
        zt = top(x)
        for dx in (-0.75, 0.75):                                            # the posts, standing over the rail
            F = hull_frame(x + dx, zt - 0.5, side)
            lbeam(part, F, 0.0, -0.9, 0.0, 1.3, 0.24, 0.24, PLANK_T + 0.12, "MI_Wood_Varnish", wear=1.0)
            part.lathe([(0.0, 0.0), (0.16, 0.0), (0.18, 0.08), (0.1, 0.18), (0.0, 0.24)], lpt(F, 0.0, 1.3, PLANK_T + 0.24), F[2], "MI_Metal_Brass", sides=10)
        F = hull_frame(x, zt + 0.7, side)
        lbeam(part, F, -1.0, 0.0, 1.0, 0.0, 0.2, 0.3, PLANK_T + 0.12, "MI_Hub_Ochre", wear=1.0)       # the hood's lintel
        for sgn in (-1.0, 1.0):
            lbeam(part, F, sgn * 0.95, 0.05, sgn * 0.35, 0.55, 0.18, 0.22, PLANK_T + 0.12, "MI_Hub_Ochre", wear=1.0)
        part.lathe([(0.0, 0.0), (0.2, 0.0), (0.22, 0.05), (0.0, 0.08)], lpt(F, 0.0, 0.62, PLANK_T + 0.2), F[3], "MI_Metal_Brass", sides=12)
        # Battens: steps down the side every 0.36 m, from under the rail to the waterline belt.
        z = zt - 0.7
        while z > 1.3:
            F = hull_frame(x, z, side)
            lbeam(part, F, -0.32, 0.0, 0.32, 0.0, 0.1, 0.12, PLANK_T, random.choice(("MI_Wood_TeakDark", "MI_Wood_Teak")), wear=1.0)
            z -= 0.36
        for dx in (-0.6, 0.6):
            a = lpt(hull_frame(x + dx, zt + 0.75, side), 0.0, 0.0, PLANK_T + 0.3)
            b = lpt(hull_frame(x + dx * 0.9, 1.5, side), 0.0, 0.0, PLANK_T + 0.35)
            spans.append(dict(a=[round(v, 3) for v in a], b=[round(v, 3) for v in b], slack=1.03, width=0.045, wind=1.0, what="man-rope"))
    return spans


def scuppers(route):
    """D69: scuppers along the deck line: iron-lipped holes just under the main deck's band, every 3.5 m (dark inside)."""
    for side in (1, -1):
        x = 51.0
        while x < 132.0:
            if all(abs(x - f) > 0.8 for f in FENDERS) and abs(x - ENTRY_X) > 1.2:
                part = route(x, side)
                if part is not None and route.wanted(x - 1, x + 1):
                    F = hull_frame(x, line_z(Z["main"] - 0.7, x), side)
                    c = lpt(F, 0.0, 0.0, PLANK_T)
                    part.lathe([(0.12, 0.0), (0.16, 0.02), (0.15, 0.06), (0.1, 0.06)], c, F[3], "MI_Metal_Iron", sides=10, caps=False)
                    part.lathe([(0.0, 0.01), (0.11, 0.01)], c, F[3], "MI_Hub_Void", sides=10, caps=False)
            x += 3.5


def channels(route):
    """Channels at each mast: a thick plank out from the side on knees, deadeyes on its outer edge, chain plates down the
    side (outside.sides). The shrouds come with the rig."""
    for side in (1, -1):
        for mid, (ln, n_de) in CHANNELS.items():
            xa, xb = channel_span(mid)
            zc = channel_z(mid)
            part = route(0.5 * (xa + xb), side)
            if part is None or not route.wanted(xa, xb):
                continue

            def fr(s, xa=xa, xb=xb, zc=zc):
                x = xa + (xb - xa) * s
                p, t, a, n = hull_frame(x, zc, side)
                return p + n * (PLANK_T + 0.9) - a * 0.11, n, a, 1.0
            part.beam(fr, xb - xa, 1.8, 0.22, "MI_Hub_HullTarDark", nseed(), seg=1.0, wear=0.9)
            for k in range(4):                                                     # the knees under it
                x = xa + 0.6 + (xb - xa - 1.2) * k / 3
                F = hull_frame(x, zc - 0.7, side)
                p, t, a, n = F
                lbeam(part, F, 0.0, -0.6, 0.0, 0.55, 0.24, 0.2, PLANK_T, "MI_Hub_HullTarDark")
                pa, pb = lpt(F, 0.0, -0.5, PLANK_T + 0.1), lpt(F, 0.0, 0.5, PLANK_T + 1.3)
                part.beam(T.straight(pa, pb, t, (pb - pa).normalized().cross(t)), (pb - pa).length, 0.22, 0.2, "MI_Hub_HullTarDark", nseed())
            for k in range(n_de):
                x = xa + 0.8 + (xb - xa - 1.6) * k / max(n_de - 1, 1)
                F = hull_frame(x, zc, side)
                p, t, a, n = F
                c = lpt(F, 0.0, 0.32, PLANK_T + 1.75)
                part.lathe([(0.0, -0.1), (0.3, -0.1), (0.36, -0.04), (0.36, 0.04), (0.3, 0.1), (0.0, 0.1)], c + a * 0.2, n, "MI_Wood_Tarred", sides=12)
                # The chain plate: from under the deadeye down the side, leaning towards the mast's foot.
                lean = (mast_x(mid) - x) * 0.04
                pa = lpt(F, 0.0, 0.1, PLANK_T + 1.75)
                pb = lpt(F, -lean, -4.2, PLANK_T + 0.05)
                part.beam(T.straight(pa, pb, t, n), (pb - pa).length, 0.12, 0.05, "MI_Metal_Iron", nseed(), wear=0.6)
                part.lathe([(0.0, 0.0), (0.09, 0.0), (0.09, 0.05), (0.0, 0.05)], pb + n * 0.03, n, "MI_Metal_Iron", sides=8)


RAILS = ((None, HUB["poopX"] - 0.05), (HUB["poopX"] + 0.65, HUB["qdX"] - 0.05), (HUB["qdX"] + 0.65, HUB["fcX"] - 0.05), (HUB["fcX"] + 0.65, None))


def cap_rails(route):
    """The cap rail along the sides' top: the poop's, the quarterdeck's, the waist's bulwark, the forecastle's."""
    rng = random.Random(81)
    for side in (1, -1):
        for xa, xb in RAILS:
            xa = corner_x(top(1.0)) + 0.35 if xa is None else xa
            xb = end_x(top(158.0)) - 0.9 if xb is None else xb
            x = xa
            while x < xb - 1e-6:
                xe = min(x + rng.uniform(6.0, 10.0), xb)
                if xb - xe < 2.0:
                    xe = xb
                part = route(0.5 * (x + xe), side)
                if part is not None and route.wanted(x, xe):
                    n = max(2, int((xe - x) / 0.8))
                    pts = []
                    for i in range(n + 1):
                        xx = x + (xe - x) * i / n
                        zt = top(xx)
                        p, t, a, nn = hull_frame(xx, zt - 0.05, side)
                        pts.append(p + nn * (PLANK_T * 0.5) + Vector((0, 0, 0.08)))
                    fr = T.polyline(pts, lambda q, s=side, xx0=x: hull_frame(q.x, q.z - 0.1, s)[3], Vector((0.0, 0.0, 1.0)))
                    part.beam(fr, T.length_of(fr), 0.5, 0.17, "MI_Wood_Varnish", nseed(), seg=0.8, wear=1.0)
                x = xe


def drifts(route):
    """Carved drifts where the rail steps up to a castle (x poopX, qdX, fcX): a scrolled board in ochre from the lower rail up
    to the higher one, with a volute at its foot and a brass boss in it."""
    for side in (1, -1):
        for xs, up_aft in ((HUB["poopX"], True), (HUB["qdX"], True), (HUB["fcX"], False)):
            sgn = 1.0 if up_aft else -1.0                # the low side lies towards x + sgn
            part = route(xs + sgn * 1.0, side)
            if part is None or not route.wanted(xs - 3.0, xs + 3.0):
                continue
            z_hi, z_lo = top(xs - sgn * 1.0), top(xs + sgn * 3.0)
            # The castle's corner post: over the strakes' ends where the profile steps, from under the low rail to the high one.
            xp = xs - sgn * 0.12
            pts = []
            for k in range(9):
                z = (z_lo - 0.9) + (z_hi + 0.12 - z_lo + 0.9) * k / 8
                F = hull_frame(xp, z, side)
                pts.append(F[0] + F[3] * (PLANK_T + 0.2))
            fc = T.polyline(pts, Vector((1.0, 0.0, 0.0)), lambda q, s=side, xp=xp: hull_frame(xp, q.z, s)[3])
            part.beam(fc, T.length_of(fc), 0.7, 0.4, "MI_Hub_HullTarDark", nseed(), seg=0.8, wear=1.0)
            # The scroll: from the high rail at the step down in an S to the low rail 2.5 m along, curling into a volute.
            xz = []
            for k in range(13):
                t = k / 12
                xz.append((xs + sgn * (0.35 + 2.2 * t), z_hi - (z_hi - z_lo) * (0.5 - 0.5 * math.cos(math.pi * t)) - 0.1))
            cx_, cz_ = xz[-1][0], xz[-1][1] - 0.32
            for k in range(1, 12):
                phi = 1.6 * math.pi * k / 11
                r = 0.32 * (1.0 - 0.55 * k / 11)
                xz.append((cx_ + sgn * r * math.sin(phi), cz_ + r * math.cos(phi)))
            q = []
            for xx, zz in xz:
                F = hull_frame(xx, zz, side)
                q.append(F[0] + F[3] * (PLANK_T + 0.11))
            fr = T.polyline(q, lambda qq, s=side: hull_frame(qq.x, qq.z, s)[3])
            part.beam(fr, T.length_of(fr, 60), 0.2, 0.55, "MI_Hub_Ochre", nseed(), seg=0.2, wear=1.0)
            F = hull_frame(cx_, cz_, side)
            part.lathe([(0.0, 0.0), (0.15, 0.0), (0.17, 0.06), (0.1, 0.14), (0.0, 0.16)], F[0] + F[3] * (PLANK_T + 0.3), F[3],
                       "MI_Metal_Brass", sides=10, wear=0.9)


# --- build ---------------------------------------------------------------------------------------------------------------
def export(part, pivot=None, ucx=False):
    """Finish a part, save its .blend (with nothing else in it: one session may build several parts) and its FBX."""
    objs = part.finish(material, ucx=ucx, pivot=pivot) if pivot is not None else part.finish(material, ucx=ucx)
    for o in list(bpy.data.objects):
        if o not in objs:
            bpy.data.objects.remove(o)
    sb.save_blend(part.name)
    sb.export_fbx(objs, os.path.join("Hub", part.name + ".fbx"))
    return objs


def skin(route, fits):
    """Everything on the sides that the route wants: strakes, the bottom, wales, ports and windows."""
    holes = holes_of(fits)
    planking(route, holes)
    wales(route, holes)
    build_fittings(route, fits)


def write_hull_ropes(spans):
    """The man-ropes go into hub_ropes.json beside the rig's spans (hub_place.ropes makes cables of both)."""
    path = os.path.join(H.ROOT, "Art", "Generated", "hub_ropes.json")
    data = {}
    if os.path.exists(path):
        with open(path, encoding="utf-8") as fh:
            data = json.load(fh)
    data["hull_spans"] = spans
    with open(path, "w", encoding="utf-8") as fh:
        json.dump(data, fh, indent=1)
    print("SB_HULL_ROPES", len(spans))


def write_outside_json(fits):
    out = os.path.join(H.ROOT, "Art", "Generated", "hub_outside.json")
    rows = []
    for f in fits:
        xc = 0.5 * (f["x0"] + f["x1"])
        rows.append({"side": f["side"], "x0": round(f["x0"], 3), "x1": round(f["x1"], 3),
                     "z0": round(line_z(f["z0"], xc), 3), "z1": round(line_z(f["z1"], xc), 3), "kind": f["kind"],
                     "door": door_leaf(xc, f["z0"], f["z1"], f["side"]) is not None})
    with open(out, "w", encoding="utf-8") as fh:
        json.dump({"_comment": "D64 the hull's ports and windows from outside (build_hub_hull.py), ship metres, absolute z. "
                   "kind: shut / patched (closed), open / lit / room (lid hauled up), glass / glow / shutter (castle windows). "
                   "Every opening is closed within the hull's 0.35 m (a dark or lit back) except kind room.",
                   "lines": [round(z, 4) for z in LINES], "door_lines": [LINES[DOOR_J[0]], LINES[DOOR_J[1]]], "fittings": rows}, fh, indent=1)
    print("SB_OUTSIDE_JSON", out, len(rows))


def build_sides():
    parts = {k: T.Part(n) for k, n in (("stern", "SM_Hub_HullStern"), ("waistS", "SM_Hub_HullWaistS"),
                                       ("waistP", "SM_Hub_HullWaistP"), ("bow", "SM_Hub_HullBow"))}
    route = Router(parts)
    fits = fittings()
    skin(route, fits)
    bottom_planks(route)
    keel(route)
    channels(route)
    cap_rails(route)
    drifts(route)
    pilasters(route, fits)
    fenders(route)
    rosettes(route)
    scuppers(route)
    write_hull_ropes(entry_ports(route))
    write_outside_json(fits)
    print("SB_FITTINGS", {k: sum(1 for f in fits if f["kind"] == k) for k in ("room", "shut", "lid", "open", "lit", "patched", "round", "round_lit", "glass", "glow", "shutter")})
    print("SB_DOOR_LINES", DOOR_J, LINES[DOOR_J[0]], LINES[DOOR_J[1]], [round(line_z(LINES[j], x), 3) for j in DOOR_J for x in DOOR_CUTS])
    for part in parts.values():
        export(part)


BUILDERS = {"sides": build_sides}

if __name__ == "__main__":
    wanted = sb.script_args() or list(BUILDERS)
    for key in wanted:
        sb.reset_scene()
        BUILDERS[key]()
    print("SB_HUBHULL_OK", wanted)
