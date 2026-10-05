"""The hub ship's hull from outside (D62; phase 2 of Docs/Design/HubShip/HubShip_Agent_Prompt.md): a giant, old galleon,
hand-made like the player's boat (D38, D52, D53).

Every number comes from Docs/Design/HubShip/HubShip_Data.json through hub_layout.py (the hull's lines, the profile with
the tiered sterncastle, the bands, the ports, the openings of the open spaces). Build choices that are not in the data are
named constants here (the strake width, the plank thickness, the port spacing).

Visual only: the blockout (build_hub_blockout.py) stays in the map as hidden collision, so every walk, boat and headroom
check of D61 still holds while the art replaces it piece by piece.

Meshes (pivot at the ship's x 80 on the waterline, bow to +X; /Game/Saltbound/Hub/Art; Nanite):
  SM_Hub_HullStern   the sterncastle's sides (x 0..44.6), both sides
  SM_Hub_HullWaistS  the waist's starboard side (x 44.6..134.6)
  SM_Hub_HullWaistP  the waist's port side
  SM_Hub_HullBow     the forecastle's and the bow's sides up to the stem
  SM_Hub_Ancient     the ancient lower hull all round (pale seamless stone, two thin turquoise lines), the stern face below +3
Run: Tools/Build-BlenderAsset.ps1 build_hub_hull [sides] [ancient]
"""
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
STONE = float(HUB["ancientTop"])     # timber above, the ancient hull below
STRAKE = 0.55                        # strake width (m): chunky for a ship this size, about 30 courses to the main deck
PLANK_T = 0.14                       # plank thickness
GAP = 0.03                           # the caulked seam between strakes
BOARD_LEN = (5.0, 9.5)               # board lengths along a strake (butts staggered from strake to strake)
TOP = max(z for _, z in HUB["profile"])
PARTS_X = (44.6, 134.6)              # the sides are split here into stern / waist / bow meshes

# Material colours for the Blender previews; Unreal uses the MI_* of the same name (setup_hub_materials.py).
COLOURS = {
    "MI_Hub_HullTar": (0.16, 0.11, 0.08), "MI_Hub_HullTarFaded": (0.24, 0.18, 0.13), "MI_Hub_HullTarDark": (0.1, 0.07, 0.05),
    "MI_Hub_Ochre": (0.62, 0.43, 0.16), "MI_Hub_OchreFaded": (0.7, 0.55, 0.3), "MI_Hub_AncientStone": (0.84, 0.82, 0.74),
    "MI_AncientGlow": (0.35, 0.95, 0.85), "MI_Caulk_Dark": (0.04, 0.035, 0.03), "MI_Wood_Tarred": (0.06, 0.05, 0.04),
    "MI_Paint_Cream": (0.86, 0.8, 0.64), "MI_Paint_CreamAged": (0.76, 0.68, 0.5), "MI_Metal_Iron": (0.08, 0.08, 0.08),
    "MI_Metal_Brass": (0.6, 0.45, 0.2), "MI_Glass_Dark": (0.12, 0.16, 0.17), "MI_LanternGlass": (1.0, 0.7, 0.35),
    "MI_Wood_Varnish": (0.36, 0.2, 0.08), "MI_Paint_Hull": (0.42, 0.11, 0.07), "MI_Paint_HullFaded": (0.5, 0.2, 0.13),
    "MI_Wood_Teak": (0.5, 0.33, 0.17), "MI_Wood_TeakDark": (0.38, 0.24, 0.12), "MI_Wood_TeakLight": (0.6, 0.42, 0.22),
    "MI_Rope_Hemp": (0.55, 0.45, 0.28), "MI_Hub_Void": (0.01, 0.01, 0.01),
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


# --- the hull's lines ----------------------------------------------------------------------------------------------------
def sheer(x):
    """How far the strake lines rise at x (hub.sheer): towards the stern face and towards the stem."""
    sh = HUB["sheer"]
    if x < sh["aft"]:
        t = (sh["aft"] - x) / sh["aft"]
        return sh["stern"] * t * t
    if x > sh["fore"]:
        t = (x - sh["fore"]) / (162.0 - sh["fore"])
        return sh["bow"] * t * t
    return 0.0


def line_z(z0, x):
    """A strake line at midship height z0, risen by the sheer; the ancient band stays level (the sheer fades in above it)."""
    k = min(max((z0 - STONE) / 6.0, 0.0), 1.0)
    return z0 + sheer(x) * k


def stone_top(x):
    """The ancient hull's top: +3 all along, rising a little at the stem (outside.bow)."""
    return STONE + 2.0 * smooth01((x - 148.0) / 12.0)


def top(x):
    return H.profile_top(x)


def surf(x, z, side):
    return Vector((x, side * H.half_width(x, z), z))


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


def grime_hull(p):
    """G: wet and green just over the ancient band, streaks fade upwards."""
    return min(max((STONE + 1.4 - p.z) / 1.4, 0.0), 1.0) * 0.8


# --- openings in the sides -----------------------------------------------------------------------------------------------
def data_openings():
    """The open spaces' windows and ports through the hull side (spaces' props on a wall at |y| >= 19): (side, x0, x1,
    z0, z1) in absolute z. Their rooms are behind them."""
    out = []
    floors = {"slip": Z["quay"], "tally": Z["lower"], "keelgate": Z["quay"], "chandlery": Z["main"]}
    for name, fl in floors.items():
        for pr in H.space(name)["props"]:
            wall = pr.get("wall", "")
            if not wall.startswith("y="):
                continue
            y = float(wall[2:])
            if abs(y) < 18.9:
                continue
            spans = pr.get("spans")
            if spans is None and "note" in pr and "x 47..48.5" in pr["note"]:
                spans = [[47.0, 48.5], [60.5, 62.0]]              # spaces.slip.hall_windows: the shop's windows (its note)
                z0, z1 = 1.6, 3.2
            else:
                z0, z1 = pr.get("z0", 0.8), pr.get("z1", 2.0)
            for x0, x1 in spans or []:
                out.append((1 if y > 0 else -1, float(x0), float(x1), fl + z0, fl + z1))
    return out


# --- the sides: strakes --------------------------------------------------------------------------------------------------
def strake_lines():
    zs, z = [], STONE
    while z < TOP + STRAKE:
        zs.append(round(z, 4))
        z += STRAKE
    return zs


def runs_for(side, za, zb, cuts, holes):
    """The x runs where strake (za..zb) exists: under the profile's top, over the stone, aft of the stem; broken at the
    part boundaries (cuts) and round the openings it crosses (holes: (x0, x1, z0, z1))."""
    xs, step = [], 0.25
    x = 0.06
    ok_prev, start = False, None
    runs = []
    while x <= 163.0:
        zl = max(line_z(za, x), stone_top(x))
        zu = min(line_z(zb, x), top(x) - 0.02)
        zm = 0.5 * (zl + zu)
        ok = zu - zl >= 0.14 and x <= end_x(zm)
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
        pieces = [(a, b)]
        for c in cuts:
            nxt = []
            for p, q in pieces:
                nxt += [(p, c), (c, q)] if p < c < q else [(p, q)]
            pieces = nxt
        for hx0, hx1, hz0, hz1 in holes:
            nxt = []
            for p, q in pieces:
                zl, zu = line_z(za, 0.5 * (hx0 + hx1)), line_z(zb, 0.5 * (hx0 + hx1))
                if zu > hz0 and zl < hz1 and p < hx1 and q > hx0:
                    if hx0 - p > 0.1:
                        nxt.append((p, hx0))
                    if q - hx1 > 0.1:
                        nxt.append((hx1, q))
                else:
                    nxt.append((p, q))
            pieces = nxt
        out += [(p, q) for p, q in pieces if q - p > 0.12]
    return out


def strake_frame(side, za, zb, xa, xb, proud):
    def f(s):
        x = xa + (xb - xa) * s
        zl = max(line_z(za, x), stone_top(x))
        zu = min(line_z(zb, x), top(x) - 0.02)
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


def planking(part_of, holes_by_side):
    """Carvel strakes on both sides, each in boards of 5-9.5 m with the butts staggered; each board its own shade, a few
    millimetres proud or sunk; now and then a newer repair board (lighter, its edges less worn)."""
    rng = random.Random(62)
    lines = strake_lines()
    for side in (1, -1):
        holes = holes_by_side[side]
        for j, (za, zb) in enumerate(zip(lines[:-1], lines[1:])):
            for ra, rb in runs_for(side, za, zb, PARTS_X + (H.HUB["poopX"] + 0.6,), holes):
                core_strip(part_of(0.5 * (ra + rb), side), side, za, zb, ra, rb)
                # Board ends: start at a random offset so the butts do not line up with the strake below.
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
                    fr = strake_frame(side, za, zb, xa + 0.006, xb - 0.006, proud)
                    part = part_of(0.5 * (xa + xb), side)
                    part.board(fr, T.length_of(fr), STRAKE - GAP, PLANK_T, mat, nseed(), seg=0.6,
                               wear=0.35 if repair else rng.uniform(0.6, 1.0), grime=grime_hull,
                               bend=0.02, twist=0.6, drift=0.03, slant=0.03, chamfer=rng.uniform(0.02, 0.035))


def core_strip(part, side, za, zb, ra, rb):
    """The dark caulked shell under one strake's run, so the seams read as tarred grooves (as the boat's hull_core); it
    follows the runs, so the openings stay open."""
    n = max(1, int(math.ceil((rb - ra) / 1.0)))
    xs = [ra + (rb - ra) * k / n for k in range(n + 1)]
    for xa, xb in zip(xs[:-1], xs[1:]):
        pts = []
        for x, up in ((xa, False), (xb, False), (xb, True), (xa, True)):
            z = min(line_z(zb, x), top(x) - 0.02) if up else max(line_z(za, x), stone_top(x))
            pts.append(surf(x, z, side))
        part.polygon(pts, "MI_Caulk_Dark", tone=0.2, facing=outward(0.5 * (xa + xb), 0.5 * (za + zb), side, Vector((0, 0, 1))))


def core(part_of):
    """(Unused since the core follows the strakes: core_strip.)"""
    for side in (1, -1):
        stations = [0.0]
        while stations[-1] < 162.0:
            x = stations[-1]
            stations.append(x + (0.5 if x > 140.0 else 1.0))
        knots = [STONE - 0.2] + [STONE + 0.5 * k for k in range(1, 70)]
        for xa, xb in zip(stations[:-1], stations[1:]):
            part = part_of(0.5 * (xa + xb), side)
            for za, zb in zip(knots[:-1], knots[1:]):
                ta, tb = top(0.5 * (xa + xb)), stone_top(0.5 * (xa + xb)) - 0.2
                z0, z1 = max(za, tb), min(zb, ta)
                if z1 - z0 < 0.01:
                    continue
                xe = end_x(0.5 * (z0 + z1)) + 0.3
                if xa >= xe:
                    continue
                xb_ = min(xb, xe)
                pts = [surf(xa, z0, side), surf(xb_, z0, side), surf(xb_, z1, side), surf(xa, z1, side)]
                if side > 0:
                    pts.reverse()
                part.polygon([tuple(p) for p in pts], "MI_Caulk_Dark", tone=0.2)


# --- bands and wales -----------------------------------------------------------------------------------------------------
BANDS = [  # (centre z at midship, height, proud, material): outside.sides: ochre at 5.5, 17.5, 33.5 (0.9 m), thinner ones
    (STONE + 0.3, 0.6, 0.26, "MI_Wood_Tarred"),            # the heavy wale over the joint with the ancient hull
    (Z["lower"], 0.9, 0.24, "MI_Hub_Ochre"), (Z["middle"], 0.4, 0.14, "MI_Hub_Ochre"), (Z["upper"], 0.4, 0.14, "MI_Hub_Ochre"),
    (Z["main"], 0.9, 0.24, "MI_Hub_Ochre"), (Z["sc1"], 0.4, 0.14, "MI_Hub_Ochre"), (Z["sc2"], 0.9, 0.2, "MI_Hub_Ochre"),
    (Z["sc3"], 0.4, 0.14, "MI_Hub_Ochre"), (Z["poop"], 0.9, 0.24, "MI_Hub_Ochre"),
]


def band_frame(side, zc, h, xa, xb, proud):
    def f(s):
        x = xa + (xb - xa) * s
        zl, zu = line_z(zc - h * 0.5, x), line_z(zc + h * 0.5, x)
        zl = max(zl, stone_top(x) - 0.3)
        zu = min(zu, top(x) - 0.03)
        pl, pu = surf(x, zl, side), surf(x, zu, side)
        acr = pu - pl
        w = max(acr.length, 1e-4)
        a = acr / w
        n = outward(x, 0.5 * (zl + zu), side, a)
        return (pl + pu) * 0.5 + n * (PLANK_T + proud * 0.5), a, n, w / h
    return f


def bands(part_of, holes_by_side):
    rng = random.Random(77)
    for side in (1, -1):
        for zc, h, proud, mat in BANDS:
            for ra, rb in runs_for(side, zc - h * 0.5, zc + h * 0.5, PARTS_X + (H.HUB["poopX"] + 0.6,), holes_by_side[side]):
                x = ra
                while x < rb - 1e-6:
                    xe = min(x + rng.uniform(7.0, 12.0), rb)
                    if rb - xe < 2.0:
                        xe = rb
                    m = mat if mat != "MI_Hub_Ochre" or rng.random() > 0.25 else "MI_Hub_OchreFaded"
                    fr = band_frame(side, zc, h, x + 0.01, xe - 0.01, proud)
                    part_of(0.5 * (x + xe), side).beam(fr, T.length_of(fr), h, proud, m, nseed(), seg=0.6, wear=0.9,
                                                        grime=grime_hull, bend=0.015, twist=0.4, chamfer=min(proud, h) * 0.3)
                    x = xe


# --- the ancient lower hull ----------------------------------------------------------------------------------------------
def ancient(p):
    """Pale seamless stone from the keel up to the band, a rounded lip under the wale; two thin glow lines (outside.sides:
    the ancient band from -1.5 to +3 with two turquoise lines)."""
    stations = [0.0]
    while stations[-1] < 162.0:
        x = stations[-1]
        stations.append(x + (0.5 if x > 130.0 or x < 4.0 else 1.5))

    def ring(x):
        zb, zt = H.profile_bottom(x), stone_top(x)
        zs = [zb, -10.0, -8.0, -6.0, -4.0, -2.0, -1.0, 0.0, 1.0, 2.0, zt - 0.35, zt - 0.12, zt]
        zs = sorted({min(max(z, zb), zt) for z in zs})
        pts = []
        for z in zs:
            w = H.half_width(x, z)
            if zt - 0.35 < z < zt:
                w += 0.08 * math.sin(math.pi * (z - (zt - 0.35)) / 0.35)          # the lip
            pts.append((w, z))
        return pts

    rings = []
    for x in stations:
        xe = H.stem_x(0.0)
        if x > xe + 0.3:
            break
        rings.append((x, ring(x)))
    for side in (1, -1):
        for (xa, ra), (xb, rb) in zip(rings[:-1], rings[1:]):
            n = min(len(ra), len(rb))
            for k in range(n - 1):
                (wa0, za0), (wa1, za1) = ra[k], ra[k + 1]
                (wb0, zb0), (wb1, zb1) = rb[k], rb[k + 1]
                pts = [(xa, side * wa0, za0), (xb, side * wb0, zb0), (xb, side * wb1, zb1), (xa, side * wa1, za1)]
                if side > 0:
                    pts.reverse()
                p.polygon(pts, "MI_Hub_AncientStone", tone=0.5, grime=lambda q: min(max((0.6 - q.z) / 1.2, 0.0), 1.0), smooth=True)
        # The stern face below the band: full width under the basin's floor, the wings beside the mouth, solid stone under
        # the quays (outside.stern).
        floor = HUB["basin"]["floor"]
        zb0 = H.profile_bottom(0.0)
        for (za, zb, y0) in ((zb0, floor, 0.0), (floor, Z["quay"], HUB["basin"]["y1"]), (Z["quay"], STONE, HUB["quayY"])):
            zs = [za + (zb - za) * k / 6 for k in range(7)]
            for z0, z1 in zip(zs[:-1], zs[1:]):
                pts = [(0.0, side * y0, z0), (0.0, side * H.half_width(0.0, z0), z0), (0.0, side * H.half_width(0.0, z1), z1), (0.0, side * y0, z1)]
                p.polygon(pts, "MI_Hub_AncientStone", tone=0.5, grime=lambda q: min(max((0.6 - q.z) / 1.2, 0.0), 1.0), facing=(-1.0, 0.0, 0.0))
        for zl in (-0.6, 1.9):
            y0 = HUB["basin"]["y1"] if zl < Z["quay"] else HUB["quayY"]
            p.polygon([(-0.012, side * y0, zl - 0.04), (-0.012, side * H.half_width(0.0, zl - 0.04), zl - 0.04),
                       (-0.012, side * H.half_width(0.0, zl + 0.04), zl + 0.04), (-0.012, side * y0, zl + 0.04)], "MI_AncientGlow",
                      tone=0.5, facing=(-1.0, 0.0, 0.0))
        # The two lines: thin strips just proud of the stone, all along.
        for zl in (-0.6, 1.9):
            for (xa, _), (xb, _) in zip(rings[:-1], rings[1:]):
                pts = []
                for x in (xa, xb):
                    for z in (zl - 0.04, zl + 0.04):
                        pts.append((x, side * (H.half_width(x, z) + 0.012), z))
                q = [pts[0], pts[2], pts[3], pts[1]]
                if side > 0:
                    q.reverse()
                p.polygon(q, "MI_AncientGlow", tone=0.5)


# --- ports, windows, channels, cap rails ----------------------------------------------------------------------------------
JAMB = 0.2                          # frame timbers round an opening
PORT = 1.0                          # outside.sides: lidded square ports of 1 m below the main deck
WIN_W, WIN_H = 1.3, 2.0             # cream-framed windows above it
PORT_ROWS = ((Z["lower"] + 1.0, 2.0), (Z["middle"] + 1.0, 0.0), (Z["upper"] + 1.0, 2.0))   # sill z, x offset (staggered)
WIN_ROWS = (Z["sc1"] + 0.8, Z["sc2"] + 0.8, Z["sc3"] + 0.8)
CHANNELS = {m: (float(HUB["rig"]["channels"][m]), HUB["rig"]["shrouds"][m]) for m in HUB["rig"]["shrouds"]}   # length, deadeyes
GALLERY = (3.0, 13.0)               # outside.stern: the quarter galleries' x on the wings (z 21.5..32)


def hull_frame(x, z, side):
    """(point on the hull, T along the ship, A up the side, N outward) at (x, z)."""
    p = surf(x, z, side)
    a = (surf(x, z + 0.05, side) - surf(x, z - 0.05, side)).normalized()
    n = outward(x, z, side, a)
    t = a.cross(n).normalized()
    if t.x < 0.0:
        t = -t
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
    """How much the hull turns forward at (x, z): the outward normal's x (0 amidships, about 1 at the stem)."""
    return abs(hull_frame(x, z, side)[3].x)


def fittings():
    """Every opening and lid on the sides: the open spaces' ports and windows (data), the lidded ports below the main
    deck, the castles' windows. kind: room (a real room behind it), shut, open, patched (lid gone, boarded), glass, lit,
    shutter."""
    rng = random.Random(512)
    out = []
    rooms = data_openings()
    for side, x0, x1, z0, z1 in rooms:
        out.append(dict(side=side, x0=x0, x1=x1, z0=z0, z1=z1, kind="room", lid=z1 - z0 < 1.5))
    tally = HUB["tally"]

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

    for side in (1, -1):
        for zs, off in PORT_ROWS:
            x = 6.0 + off
            while x < 158.0:
                ok = (x + PORT * 0.5 < end_x(zs) - 3.0 and bow_turn(x, zs + 0.5, side) < 0.3 and chain_clear(x, zs + PORT)
                      and all(abs(x - c) > 1.4 for c in PARTS_X + (H.HUB["poopX"] + 0.3,)) and clear(side, x, zs, zs + PORT, 2.0)
                      and zs + PORT < top(x) - 1.0)
                if ok:
                    in_room = side > 0 and tally["x0"] - 1.0 < x < tally["x1"] + 1.0 and abs(zs - (Z["lower"] + 1.0)) < 0.1
                    r = rng.random()
                    kind = "shut" if (r < 0.7 or in_room) else ("open" if r < 0.92 else "patched")
                    out.append(dict(side=side, x0=x - PORT * 0.5, x1=x + PORT * 0.5, z0=zs, z1=zs + PORT, kind=kind))
                x += 4.0
        for zs in WIN_ROWS:
            x = 14.5
            while x < 158.0:
                fc = zs == Z["fc1"] + 0.8 and x > PARTS_X[1]
                ok = (zs + WIN_H < top(x) - 0.9 and x + WIN_W * 0.5 < end_x(zs) - 2.0 and bow_turn(x, zs + 1.0, side) < 0.35
                      and chain_clear(x, zs + WIN_H) and not (GALLERY[0] - 1.0 < x < GALLERY[1] + 1.0)
                      and all(abs(x - c) > 1.8 for c in PARTS_X + (H.HUB["poopX"] + 0.3,)) and (x < PARTS_X[0] or fc))
                if ok:
                    r = rng.random()
                    kind = "glass" if r < 0.5 else ("lit" if r < 0.7 else "shutter")
                    out.append(dict(side=side, x0=x - WIN_W * 0.5, x1=x + WIN_W * 0.5, z0=zs, z1=zs + WIN_H, kind=kind))
                x += 4.0
    # Cut strake-aligned holes for every opening with nothing in front of it.
    lines = strake_lines()
    for f in out:
        xc = 0.5 * (f["x0"] + f["x1"])
        lz = [line_z(l, xc) for l in lines]
        f["hz0"] = max([z for z in lz if z <= f["z0"] + 0.02] or [f["z0"]])
        f["hz1"] = min([z for z in lz if z >= f["z1"] - 0.02] or [f["z1"]])
        f["hole"] = f["kind"] in ("room", "open", "glass", "lit", "shutter")
    return out


def holes_of(fits):
    holes = {1: [], -1: []}
    for f in fits:
        if f["hole"]:
            holes[f["side"]].append((f["x0"] - JAMB * 0.5, f["x1"] + JAMB * 0.5, f["hz0"] + 0.02, f["hz1"] - 0.02))
    return holes


def fitting(part, f, F=None):
    """One opening or lid on the hull's local plane at its centre (or on the plane F given: the transom)."""
    side, x0, x1, z0, z1 = f["side"], f["x0"], f["x1"], f["z0"], f["z1"]
    xc, zc = 0.5 * (x0 + x1), 0.5 * (z0 + z1)
    F = F or hull_frame(xc, zc, side)
    hw_, hh = 0.5 * (x1 - x0), 0.5 * (z1 - z0)
    window = f["kind"] in ("glass", "lit", "shutter")
    frame_mat = "MI_Paint_Cream" if window else ("MI_Hub_HullTarDark" if f["kind"] != "room" else "MI_Paint_CreamAged")
    if window and random.random() < 0.3:
        frame_mat = "MI_Paint_CreamAged"
    d0 = PLANK_T - 0.02
    if f["hole"]:
        # The frame: jambs over the cut board ends, a head and a sill filling to the strake lines.
        hz0, hz1 = f["hz0"] - zc, f["hz1"] - zc
        lbeam(part, F, -hw_ - JAMB * 0.5, hz0 - 0.05, -hw_ - JAMB * 0.5, hz1 + 0.05, JAMB, 0.14, d0, frame_mat)
        lbeam(part, F, hw_ + JAMB * 0.5, hz0 - 0.05, hw_ + JAMB * 0.5, hz1 + 0.05, JAMB, 0.14, d0, frame_mat)
        head = max(hz1 - hh, 0.16)
        lbeam(part, F, -hw_ - JAMB, hh + head * 0.5, hw_ + JAMB, hh + head * 0.5, head, 0.16, d0 + 0.01, frame_mat)
        sill = max(-hh - hz0, 0.16)
        lbeam(part, F, -hw_ - JAMB - 0.08, -hh - sill * 0.5, hw_ + JAMB + 0.08, -hh - sill * 0.5, sill, 0.2, d0 + 0.01, frame_mat)
        # The reveal: the opening's lining through the hull, dark inside.
        lining = "MI_Hub_HullTarDark"
        lbeam(part, F, -hw_ + 0.04, -hh, -hw_ + 0.04, hh, 0.08, 0.6, -0.45, lining, wear=0.3)
        lbeam(part, F, hw_ - 0.04, -hh, hw_ - 0.04, hh, 0.08, 0.6, -0.45, lining, wear=0.3)
        lbeam(part, F, -hw_, -hh + 0.04, hw_, -hh + 0.04, 0.08, 0.6, -0.45, lining, wear=0.3)
        lbeam(part, F, -hw_, hh - 0.04, hw_, hh - 0.04, 0.08, 0.6, -0.45, lining, wear=0.3)
    kind = f["kind"]
    if kind in ("open", "room") and f.get("lid", True):
        # A lid hinged at its head, hauled up and out: planked outside, the inside painted oxblood like the boat's hull.
        ang = math.radians(random.uniform(48.0, 72.0))
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
            pb = pa + down * (L * 0.85)
            part.beam(T.straight(pa, pb, t, face), (pb - pa).length, 0.1, 0.03, "MI_Metal_Iron", nseed(), wear=0.5)
        # Its lanyard up into the hull over the port.
        pa = hinge + down * L + face * 0.12
        pb = lpt(F, 0.0, hh + 0.7, PLANK_T)
        part.round(T.straight(pa, pb, t, n), (pb - pa).length, 0.025, "MI_Rope_Hemp", nseed(), sides=5, seg=2.0, wear=0.0)
    if kind in ("open", "room") or (kind == "glass" or kind == "lit"):
        if kind == "open":
            part.polygon([lpt(F, u, v, -0.5) for u, v in ((-hw_, -hh), (-hw_, hh), (hw_, hh), (hw_, -hh))], "MI_Hub_Void", tone=0.1, facing=F[3])
    if kind == "shut":
        # The lid shut in its frame (ochre-edged, so the rows read from afar): three boards, two hinge straps, a ring.
        for sgn in (-1.0, 1.0):
            lbeam(part, F, sgn * (hw_ + 0.12), -hh - 0.22, sgn * (hw_ + 0.12), hh + 0.22, 0.14, 0.09, PLANK_T - 0.02, "MI_Hub_OchreFaded", wear=1.0)
        lbeam(part, F, -hw_ - 0.2, hh + 0.14, hw_ + 0.2, hh + 0.14, 0.16, 0.1, PLANK_T - 0.02, "MI_Hub_Ochre")
        lbeam(part, F, -hw_ - 0.2, -hh - 0.14, hw_ + 0.2, -hh - 0.14, 0.18, 0.12, PLANK_T - 0.02, "MI_Hub_Ochre")
        for k in range(3):
            u = -hw_ + (2 * hw_) * (k + 0.5) / 3
            lbeam(part, F, u, -hh, u, hh, 2 * hw_ / 3 - 0.02, 0.1, PLANK_T - 0.02,
                  random.choice(("MI_Hub_HullTar", "MI_Hub_HullTar", "MI_Hub_HullTarFaded")))
        for u in (-hw_ * 0.55, hw_ * 0.55):
            lbeam(part, F, u, hh + 0.05, u, -hh * 0.6, 0.1, 0.03, PLANK_T + 0.08, "MI_Metal_Iron", wear=0.5)
        part.lathe([(0.0, 0.0), (0.07, 0.0), (0.07, 0.03), (0.0, 0.03)], lpt(F, 0.0, -hh + 0.15, PLANK_T + 0.08), F[3], "MI_Metal_Iron", sides=8)
    if kind == "patched":
        # The lid is gone: boards nailed across the port, unpainted and newer.
        lbeam(part, F, -hw_ - 0.1, hh + 0.14, hw_ + 0.1, hh + 0.14, 0.16, 0.08, PLANK_T - 0.02, "MI_Hub_HullTarDark")
        lbeam(part, F, -hw_ - 0.1, -hh - 0.14, hw_ + 0.1, -hh - 0.14, 0.18, 0.1, PLANK_T - 0.02, "MI_Hub_HullTarDark")
        for k, v in enumerate((-0.32, 0.02, 0.34)):
            tilt = random.uniform(-0.12, 0.12)
            lbeam(part, F, -hw_ - 0.12, v * hh * 2 - tilt, hw_ + 0.12, v * hh * 2 + tilt, 0.26, 0.05, PLANK_T, "MI_Wood_TeakLight", wear=0.3)
    if window:
        # Glass in a cross of mullions (some lit from inside), or shut behind two oxblood shutters.
        glass = "MI_LanternGlass" if kind == "lit" else "MI_Glass_Dark"
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
                          "MI_Paint_Hull" if random.random() < 0.7 else "MI_Paint_HullFaded", wear=1.0)
                lbeam(part, F, sgn * 0.05, hh * 0.6, sgn * (hw_ - 0.05), hh * 0.6, 0.12, 0.04, -0.04, "MI_Metal_Iron")
                lbeam(part, F, sgn * 0.05, -hh * 0.6, sgn * (hw_ - 0.05), -hh * 0.6, 0.12, 0.04, -0.04, "MI_Metal_Iron")


def channels(part_of):
    """Channels at each mast: a thick plank out from the side on knees, deadeyes on its outer edge, chain plates down the
    side (outside.sides: channels with deadeyes at each mast). The shrouds come with the rig."""
    for side in (1, -1):
        for mid, (ln, n_de) in CHANNELS.items():
            xa, xb = channel_span(mid)
            zc = channel_z(mid)
            part = part_of(0.5 * (xa + xb), side)

            def fr(s, xa=xa, xb=xb, zc=zc, depth=0.0):
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
                part.beam(T.straight(pa, pb, t, (pb - pa).normalized().cross(t) if False else n), (pb - pa).length, 0.12, 0.05,
                          "MI_Metal_Iron", nseed(), wear=0.6)


def cap_rails(part_of):
    """The cap rail along the sides' top: the waist's bulwark, the quarterdeck's, the poop's, the forecastle's."""
    for side in (1, -1):
        for xa, xb in ((0.3, H.HUB["poopX"] + 0.3), (H.HUB["poopX"] + 0.9, PARTS_X[0]), (PARTS_X[0] + 0.05, PARTS_X[1]),
                       (PARTS_X[1] + 0.05, end_x(top(150.0)) - 0.6)):
            x = xa
            while x < xb - 1e-6:
                xe = min(x + random.uniform(6.0, 10.0), xb)
                if xb - xe < 2.0:
                    xe = xb

                def fr(s, x0=x, x1=xe):
                    xx = x0 + (x1 - x0) * s
                    zt = top(0.5 * (x0 + x1))
                    p, t, a, n = hull_frame(xx, zt - 0.05, side)
                    return p + n * (PLANK_T * 0.5) + Vector((0, 0, 0.08)), n, Vector((0.0, 0.0, 1.0)), 1.0
                part_of(0.5 * (x + xe), side).beam(fr, xe - x, 0.5, 0.17, "MI_Wood_Varnish", nseed(), seg=0.8, wear=1.0)
                x = xe


# --- build ---------------------------------------------------------------------------------------------------------------
def export(part):
    objs = part.finish(material, ucx=False)
    sb.save_blend(part.name)
    sb.export_fbx(objs, os.path.join("Hub", part.name + ".fbx"))


def build_sides():
    parts = {k: T.Part(n) for k, n in (("stern", "SM_Hub_HullStern"), ("waistS", "SM_Hub_HullWaistS"),
                                       ("waistP", "SM_Hub_HullWaistP"), ("bow", "SM_Hub_HullBow"))}

    def part_of(x, side):
        if x < PARTS_X[0]:
            return parts["stern"]
        if x > PARTS_X[1]:
            return parts["bow"]
        return parts["waistS"] if side > 0 else parts["waistP"]

    fits = fittings()
    holes = holes_of(fits)
    planking(part_of, holes)
    bands(part_of, holes)
    random.seed(4040)
    for f in fits:
        fitting(part_of(0.5 * (f["x0"] + f["x1"]), f["side"]), f)
    channels(part_of)
    cap_rails(part_of)
    print("SB_FITTINGS", {k: sum(1 for f in fits if f["kind"] == k) for k in ("room", "shut", "open", "patched", "glass", "lit", "shutter")})
    for part in parts.values():
        export(part)


def build_ancient():
    p = T.Part("SM_Hub_Ancient")
    ancient(p)
    export(p)


BUILDERS = {"sides": build_sides, "ancient": build_ancient}

if __name__ == "__main__":
    wanted = sb.script_args() or list(BUILDERS)
    for key in wanted:
        sb.reset_scene()
        BUILDERS[key]()
    print("SB_HUBHULL_OK", wanted)
