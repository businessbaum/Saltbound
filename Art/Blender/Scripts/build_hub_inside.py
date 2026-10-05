"""The hub ship's open spaces, inside (D62; phase 2 of the hub prompt): every floor, wall, ceiling, rail, stair and prop of the
open 20 %, hand-made like the boat (owner: "you need to Look at Everything, every Wall every Corner floor").

The art follows the blockout's own construction (build_hub_blockout.py: the same wall runs along the open zones' edges,
the same openings, flights and prop boxes), so it lies on the blockout's surfaces, which stay in the map as hidden
collision: floors' tops at the deck heights, walls' faces at the collision walls' faces, rails on their lines, the steps
on the blockout's steps. Every number from HubShip_Data.json through hub_layout.py; the build's own choices are named
constants below.

The look below decks: deck planks fore and aft over caulked seams; walls of planks in rows on frame posts, with a skirting
and a top plate; the decks overhead on heavy beams; turned balusters on the rails (the castles' balustrades); the ancient
stone of the dock (pale, seamless) with timber fenders, an iron-shod kerb and oak decking on it; warm teak and dark
tarred timber, iron fittings, rope.

Meshes (/Game/Saltbound/Hub/Art, ship pivot; Nanite; no collision of their own):
  SM_Hub_Dock    the Stern Dock: basin floor and walls, the slipway, fenders, ladders, mooring rings, the quays' decking and
                 kerbs, the arcades (posts, braces, beams, the wing's underside), the arcades' back walls, bollards,
                 lanterns on brackets, the berth board, the wall crane and its winch, the port landing
  SM_Hub_Slip    the Slip: the boat hall and the shop (floors, walls, ceilings), the counter, plans board, half-models,
                 workbench, timber racks, the 14 m cutter on her cradles, her mast on trestles, lanterns, the sign
Run: Tools/Build-BlenderAsset.ps1 build_hub_inside [dock] [slip]
"""
import math
import os
import random
import sys

from mathutils import Vector

sys.path.insert(0, os.path.dirname(__file__))
import build_hub_blockout as B  # noqa: E402  (the blockout's construction: wall runs, openings, flights)
import build_hub_ends as HE  # noqa: E402
import build_hub_hull as HH  # noqa: E402
import hub_layout as H  # noqa: E402
import hub_timber as T  # noqa: E402
import sb_blender as sb  # noqa: E402

HUB, Z = H.HUB, H.Z
X, Y, UP = Vector((1.0, 0.0, 0.0)), Vector((0.0, 1.0, 0.0)), Vector((0.0, 0.0, 1.0))
nseed = HH.nseed
WALL = B.WALL                       # the collision walls' thickness (centred on their lines)
SLAB = B.SLAB                       # the collision floors' thickness
FLOOR_W, FLOOR_T = 0.26, 0.09       # deck boards below decks
QUAY_W = 0.32                       # the quays' decking boards
BEAM_EVERY = 1.3                    # deck beams overhead
POST_EVERY = 2.0                    # frame posts in the walls
TEAK = ("MI_Wood_Teak", "MI_Wood_Teak", "MI_Wood_TeakDark", "MI_Wood_TeakLight")
WALL_MATS = ("MI_Wood_Planks", "MI_Wood_Planks", "MI_Wood_Planks", "MI_Wood_TeakDark", "MI_Wood_TeakDark", "MI_Wood_Teak", "MI_Wood_DockWeathered")
DOCK = ("MI_Wood_DockWeathered", "MI_Wood_DockWeathered", "MI_Wood_Planks")
DARK = "MI_Hub_HullTarDark"
STONE = "MI_Hub_AncientStone"
HH.COLOURS.update({"MI_Hub_AncientGlowLow": (0.3, 0.7, 0.65), "MI_Hub_CanvasRed": (0.45, 0.12, 0.08), "MI_Wood_Planks": (0.42, 0.32, 0.22), "MI_Wood_DockWeathered": (0.5, 0.47, 0.42), "MI_Paint_HullDark": (0.3, 0.07, 0.05),
                   "MI_Wood_Piling": (0.2, 0.17, 0.14), "MI_Canvas": (0.8, 0.76, 0.66)})


def export(p):
    HH.export(p)


def grime_floor(z0):
    """G: dirt in the corners is the walls' business; floors get a little all over and more near z0 (wet quays)."""
    return lambda q: min(max((z0 + 0.25 - q.z) / 0.5, 0.0), 1.0) * 0.6


# --- the kit --------------------------------------------------------------------------------------------------------------
def floor(p, rect, z, holes=(), mats=TEAK, width=FLOOR_W, thick=FLOOR_T, along="x", grime=None, lmin=2.5, lmax=6.0):
    """Deck boards with their tops at z over rect minus holes, fore and aft (or across), a caulked core under the seams."""
    rng = random.Random(nseed())
    for r in B.subtract(rect, list(holes)):
        x0, x1, y0, y1 = r
        if along == "x":
            lo, hi, a0, a1 = y0, y1, x0, x1
        else:
            lo, hi, a0, a1 = x0, x1, y0, y1
        c = lo
        while c < hi - 0.03:
            cb = min(c + width, hi)
            if along == "x":
                core = [(a0, c, z - thick), (a1, c, z - thick), (a1, cb, z - thick), (a0, cb, z - thick)]
            else:
                core = [(c, a0, z - thick), (cb, a0, z - thick), (cb, a1, z - thick), (c, a1, z - thick)]
            p.polygon(core, "MI_Caulk_Dark", tone=0.2, facing=UP)
            a = a0
            first = True
            while a < a1 - 1e-6:
                ae = min(a + (rng.uniform(0.4, lmax) if first else rng.uniform(lmin, lmax)), a1)
                first = False
                if a1 - ae < 0.6:
                    ae = a1
                m = 0.5 * (c + cb)
                if along == "x":
                    pa, pb, acr = Vector((a + 0.004, m, z - thick * 0.5)), Vector((ae - 0.004, m, z - thick * 0.5)), Y
                else:
                    pa, pb, acr = Vector((m, a + 0.004, z - thick * 0.5)), Vector((m, ae - 0.004, z - thick * 0.5)), X
                if (pb - pa).length > 0.1:
                    p.board(T.straight(pa, pb, acr, UP), (pb - pa).length, (cb - c) - 0.012, thick, rng.choice(mats), nseed(), seg=0.8,
                            wear=rng.uniform(0.5, 1.0), bend=0.004, twist=0.3, grime=grime, cup=0.003, groove=0.002)
                a = ae
            c = cb


def ceiling(p, rect, z_top, every=BEAM_EVERY, boards=True, beam=(0.3, 0.3), knees=True):
    """The deck overhead: its planks' undersides (grooved side down) at the collision slab's underside (z_top - SLAB) and
    beams across under them every `every` metres, with hanging knees at their ends."""
    rng = random.Random(nseed())
    x0, x1, y0, y1 = rect
    zb = z_top - SLAB
    if boards:
        y = y0
        while y < y1 - 0.03:
            yb = min(y + FLOOR_W * 1.2, y1)
            p.polygon([(x0, y, zb + 0.12), (x0, yb, zb + 0.12), (x1, yb, zb + 0.12), (x1, y, zb + 0.12)], "MI_Caulk_Dark", tone=0.2, facing=-UP)
            a = x0
            while a < x1 - 1e-6:
                ae = min(a + rng.uniform(2.5, 6.0), x1)
                if x1 - ae < 0.6:
                    ae = x1
                m = 0.5 * (y + yb)
                p.board(T.straight((a + 0.004, m, zb + 0.05), (ae - 0.004, m, zb + 0.05), Y, -UP), ae - a - 0.008, (yb - y) - 0.012, 0.1,
                        rng.choice(TEAK), nseed(), seg=1.0, wear=0.5, bend=0.004)
                a = ae
            y = yb
    bw, bd = beam
    n = max(1, int(round((x1 - x0) / every)))
    for k in range(n + 1):
        x = x0 + (x1 - x0) * k / n
        x = min(max(x, x0 + bw * 0.5), x1 - bw * 0.5)
        p.beam(T.straight((x, y0, zb - bd * 0.5), (x, y1, zb - bd * 0.5), X, UP), y1 - y0, bw, bd, rng.choice(("MI_Wood_TeakDark", DARK)), nseed(),
               seg=1.0, wear=0.8, bend=0.02)
        if knees:
            for yy, s in ((y0, 1.0), (y1, -1.0)):
                a = Vector((x, yy + s * 0.08, zb - bd - 0.7))
                b = Vector((x, yy + s * 0.7, zb - bd + 0.05))
                p.beam(T.straight(a, b, X, (b - a).normalized().cross(X) * s), (b - a).length, bw * 0.7, 0.16, "MI_Wood_TeakDark", nseed())


def wall(p, axis, line, a0, a1, z0, z1, side, openings=(), mats=WALL_MATS, posts=True, plate=True, skirting=True):
    """The face of a partition on its `side` (+1: the face towards +x for an x line, +y for a y line) from a0 to a1 along
    it and z0 to z1: boards in rows (their fronts at the collision wall's face), frame posts, a skirting and a top plate;
    frames round the openings [(b0, b1, oz0, oz1)] (absolute z)."""
    face = line + side * WALL * 0.5
    n = (X if axis == "x" else Y) * side
    t = Y if axis == "x" else X
    org = (Vector((face - side * 0.12, 0.0, 0.0)) if axis == "x" else Vector((0.0, face - side * 0.12, 0.0)))
    F = (org, t, UP, n)
    ops = [(min(b0, b1), max(b0, b1), oz0, oz1) for b0, b1, oz0, oz1 in openings]

    def keep(u, v):
        return all(not (b0 - 0.05 < u < b1 + 0.05 and oz0 - 0.05 < v < oz1 + 0.05) for b0, b1, oz0, oz1 in ops)
    HE.wall_boards(p, F, a0, a1, z0, z1, keep, mats=mats)
    rng = random.Random(nseed())
    if posts:
        k = 1
        n_p = max(1, int(round((a1 - a0) / POST_EVERY)))
        for k in range(n_p + 1):
            u = a0 + (a1 - a0) * k / n_p
            u = min(max(u, a0 + 0.12), a1 - 0.12)
            if any(b0 - 0.3 < u < b1 + 0.3 for b0, b1, _, _ in ops):
                continue
            HH.lbeam(p, F, u, z0, u, z1, 0.22, 0.12, 0.12, "MI_Wood_TeakDark", wear=0.9)
    for b0, b1, oz0, oz1 in ops:                                            # door and window frames
        for u in (b0 - 0.1, b1 + 0.1):
            HH.lbeam(p, F, u, oz0 if oz0 > z0 + 0.05 else z0, u, oz1 + 0.12, 0.2, 0.14, 0.1, "MI_Wood_TeakDark", wear=1.0)
        HH.lbeam(p, F, b0 - 0.25, oz1 + 0.12, b1 + 0.25, oz1 + 0.12, 0.24, 0.16, 0.1, "MI_Wood_TeakDark", wear=1.0)
        if oz0 > z0 + 0.05:
            HH.lbeam(p, F, b0 - 0.2, oz0 - 0.06, b1 + 0.2, oz0 - 0.06, 0.12, 0.18, 0.1, "MI_Wood_TeakDark", wear=1.0)
        # The reveal through the wall's thickness.
        for u in (b0, b1):
            HH.lbeam(p, F, u, oz0, u, oz1, 0.04, WALL + 0.02, -WALL + 0.11, "MI_Wood_Planks", wear=0.4)
        HH.lbeam(p, F, b0, oz1, b1, oz1, 0.04, WALL + 0.02, -WALL + 0.11, "MI_Wood_Planks", wear=0.4)
    if skirting:
        segs = [(a0, a1)]
        for b0, b1, oz0, oz1 in ops:
            if oz0 <= z0 + 0.05:
                segs = [s for sg in segs for s in ((sg[0], min(sg[1], b0 - 0.2)), (max(sg[0], b1 + 0.2), sg[1])) if s[1] - s[0] > 0.1]
        for sa, sb_ in segs:
            HH.lbeam(p, F, sa, z0 + 0.1, sb_, z0 + 0.1, 0.2, 0.05, 0.12, "MI_Wood_TeakDark", wear=0.8)
    if plate:
        HH.lbeam(p, F, a0, z1 - 0.13, a1, z1 - 0.13, 0.26, 0.1, 0.12, "MI_Wood_TeakDark", wear=0.8)


def rail(p, axis, line, a0, a1, z, height=B.RAIL_H):
    """A rail on its line: a balustrade of turned balusters between posts, under a cap rail."""
    if axis == "x":
        HE.balustrade(p, (line, a0), (line, a1), z, height=height, posts=1.6, mat="MI_Wood_Varnish", baluster="MI_Wood_TeakLight")
    else:
        HE.balustrade(p, (a0, line), (a1, line), z, height=height, posts=1.6, mat="MI_Wood_Varnish", baluster="MI_Wood_TeakLight")


def open_side(is_open, axis, line, a0, a1):
    """Which faces of a wall run look into open space: [+1] / [-1] / both."""
    mids = [a0 + (a1 - a0) * f for f in (0.25, 0.5, 0.75)]
    out = []
    for s in (1.0, -1.0):
        hits = 0
        for m in mids:
            px, py = (line + s * 0.25, m) if axis == "x" else (m, line + s * 0.25)
            hits += is_open(px, py)
        if hits >= 2:
            out.append(s)
    return out or [1.0]


def level_walls(p, z, opens, holes, rail_voids, wall_voids, water, heights, gaps, openings, z_ceiling):
    """Every wall and rail of a level, as the blockout runs them, in art."""
    runs = B.rasterise(z, opens, holes, rail_voids, wall_voids, water, heights, gaps)

    def is_open(x, y):
        return any(H.inside(r, x, y) for r in opens) and not any(H.inside(r, x, y) for r in holes)
    for kind, axis, line, a0, a1, hgt in runs:
        if kind == "rail":
            rail(p, axis, line, a0, a1, z)
            continue
        ops = [(o[2], o[3], z + o[4], z + o[5]) for o in openings if o[0] == axis and abs(o[1] - line) < 0.01 and o[3] > a0 and o[2] < a1]
        top = min(z + hgt, z_ceiling(0.5 * (a0 + a1) if axis == "y" else line, line if axis == "y" else 0.5 * (a0 + a1)))
        for s in open_side(is_open, axis, line, a0, a1):
            wall(p, axis, line, a0, a1, z, top, s, ops)


def lantern(p, c, chain_to=None, r=0.2):
    """A hanging lantern: brass cage, glass, a ring on top, its chain or hook up to `chain_to`."""
    p.lathe([(0.0, 0.0), (r * 0.8, 0.0), (r, 0.08), (r * 0.95, 0.12), (0.0, 0.12)], c, UP, "MI_Metal_Brass", sides=8)
    p.lathe([(r * 0.82, 0.12), (r * 0.88, 0.4), (r * 0.82, 0.55), (0.0, 0.55)], c, UP, "MI_LanternGlass", sides=8, caps=False)
    for k in range(4):
        a = 2 * math.pi * k / 4 + 0.4
        d = Vector((math.cos(a), math.sin(a), 0.0))
        p.beam(T.straight(c + d * r * 0.9 + UP * 0.1, c + d * r * 0.9 + UP * 0.58, d.cross(UP), d), 0.48, 0.03, 0.03, "MI_Metal_Brass", nseed(), seg=1.0)
    p.lathe([(r, 0.55), (r * 1.05, 0.6), (r * 0.6, 0.75), (r * 0.2, 0.82), (0.0, 0.84)], c, UP, "MI_Metal_Brass", sides=8)
    p.beam(T.arc(c + UP * 0.9, X, UP, 0.06, 0.0, 2 * math.pi), 0.4, 0.02, 0.02, "MI_Metal_Iron", nseed(), seg=0.1)
    if chain_to is not None:
        a, b = c + UP * 0.96, Vector(chain_to)
        L = (b - a).length
        k = 0
        while k * 0.12 < L:
            q = a + (b - a) * (k * 0.12 / L)
            ax = X if k % 2 else Y
            p.beam(T.arc(q + (b - a).normalized() * 0.06, ax, (b - a).normalized(), 0.05, 0.0, 2 * math.pi), 0.33, 0.016, 0.016,
                   "MI_Metal_Iron", nseed(), seg=0.08)
            k += 1


def bracket_lantern(p, post_face, out, z_hook, drop=0.55):
    """An iron bracket out from a post's face with a lantern hanging from its end (lanterns' hook_z)."""
    out = Vector(out).normalized()
    a = Vector(post_face) + UP * z_hook
    b = a + out * 0.75
    p.beam(T.straight(a, b, UP, out.cross(UP)), 0.75, 0.06, 0.04, "MI_Metal_Iron", nseed(), seg=1.0)
    s0, s1 = a - UP * 0.45, a + out * 0.5
    p.beam(T.straight(s0, s1, out.cross(UP), (s1 - s0).normalized().cross(out.cross(UP))), (s1 - s0).length, 0.04, 0.04, "MI_Metal_Iron", nseed())
    lantern(p, b - UP * (drop + 0.84), chain_to=b)


# --- the Stern Dock -------------------------------------------------------------------------------------------------------
def build_dock():
    p = T.Part("SM_Hub_Dock")
    bs = HUB["basin"]
    sw = H.prop("dock", "slipway")
    half, quay = bs["y1"], HUB["quayY"]
    fl, zq = bs["floor"], Z["quay"]
    sg = HUB["seagate"]
    # The basin's floor and walls: ancient stone, pale and seamless; the slipway a stone ramp with oak ways on it.
    p.polygon([(bs["x0"], -half, fl), (sw["x0"], -half, fl), (sw["x0"], half, fl), (bs["x0"], half, fl)], STONE, facing=UP)
    for s in (-1.0, 1.0):
        ya, yb = sorted((s * sw["y1"], s * half))
        p.polygon([(sw["x0"], ya, fl), (bs["x1"], ya, fl), (bs["x1"], yb, fl), (sw["x0"], yb, fl)], STONE, facing=UP)
        # The quay's wall face over the basin, with a timber fender against it at the waterline.
        y = s * half
        for xa, xb in ((bs["x0"], bs["x1"]),):
            p.polygon([(xa, y, fl), (xb, y, fl), (xb, y, zq - 0.25), (xa, y, zq - 0.25)], STONE, facing=(0.0, -s, 0.0),
                      grime=lambda q: min(max((0.9 - q.z) / 1.2, 0.0), 1.0))
        x = 3.0
        while x < bs["x1"] - 0.5:
            if abs(x - sg["x"]) > 1.6 and all(abs(x - lx) > 0.9 for lx in H.prop("dock", "rung_ladders")["at"]):
                p.beam(T.straight((x, y - s * 0.14, -1.6), (x, y - s * 0.14, zq - 0.3), X, Y * -s), zq + 1.3, 0.3, 0.26, "MI_Wood_Piling", nseed(),
                       seg=0.8, wear=1.0, grime=lambda q: min(max((0.7 - q.z) / 1.5, 0.0), 1.0))
            x += 3.0
        for zw in (-0.4, 0.9):
            p.beam(T.straight((sg["x"] + 1.2, y - s * 0.32, zw), (bs["x1"] - 0.4, y - s * 0.32, zw), UP, Y * -s), bs["x1"] - sg["x"] - 1.6, 0.26, 0.14,
                   "MI_Wood_Piling", nseed(), seg=1.0, wear=1.0, grime=lambda q: min(max((0.7 - q.z) / 1.5, 0.0), 1.0))
        # Iron rings to moor to, and the rung ladders up from the water.
        for xr in range(8, 44, 6):
            c = Vector((xr + 1.5, y - s * 0.05, zq - 0.7))
            p.beam(T.arc(c - Y * s * 0.12, X, UP, 0.16, 0.0, 2 * math.pi), 1.0, 0.04, 0.04, "MI_Metal_Iron", nseed(), seg=0.1)
            p.block(c.x - 0.08, c.x + 0.08, min(c.y, c.y - s * 0.08), max(c.y, c.y - s * 0.08), c.z + 0.1, c.z + 0.22, "MI_Metal_Iron", nseed(), axis="y")
        for lx in H.prop("dock", "rung_ladders")["at"]:
            zr = -1.8
            while zr < zq - 0.1:
                for dx in (-0.25, 0.25):
                    p.beam(T.straight((lx + dx, y, zr), (lx + dx, y - s * 0.16, zr), UP, X), 0.16, 0.04, 0.04, "MI_Metal_Iron", nseed(), seg=1.0)
                p.beam(T.straight((lx - 0.27, y - s * 0.16, zr), (lx + 0.27, y - s * 0.16, zr), UP, Y), 0.54, 0.045, 0.045, "MI_Metal_Iron", nseed(), seg=1.0)
                zr += 0.3
            for dx in (-0.3, 0.3):                                                # grab handles over the kerb
                p.beam(T.arc(Vector((lx + dx, y + s * 0.15, zq + 0.02)), Y * s, UP, 0.28, 0.0, math.pi), 0.9, 0.05, 0.05, "MI_Metal_Iron", nseed(), seg=0.1)
        # The dock head beside the slipway: its stone face and the hall floor's edge over it.
        ys = sorted((s * sw["y1"], s * half))
        p.polygon([(bs["x1"], ys[0], fl), (bs["x1"], ys[1], fl), (bs["x1"], ys[1], zq - SLAB), (bs["x1"], ys[0], zq - SLAB)], STONE, facing=(-1.0, 0.0, 0.0))
        yy = s * sw["y1"]
        p.polygon([(bs["x1"], yy, fl), (sw["x1"], yy, fl), (sw["x1"], yy, zq - SLAB), (bs["x1"], yy, zq - SLAB)], STONE, facing=(0.0, -s, 0.0))
    # The slipway: its stone face, oak ways the keels run on, cross sleepers.
    w = sw["y1"]
    z0s, z1s = sw["z_at_x0"], sw["z_at_x1"]
    p.polygon([(sw["x0"], -w, z0s), (sw["x1"], -w, z1s), (sw["x1"], w, z1s), (sw["x0"], w, z0s)], STONE, facing=UP)
    p.polygon([(sw["x0"], -w, fl), (sw["x0"], w, fl), (sw["x0"], w, z0s), (sw["x0"], -w, z0s)], STONE, facing=-X)
    slope = Vector((sw["x1"] - sw["x0"], 0.0, z1s - z0s)).normalized()
    nrm = Y.cross(slope) if Y.cross(slope).z > 0 else slope.cross(Y)
    L = (Vector((sw["x1"], 0, z1s)) - Vector((sw["x0"], 0, z0s))).length
    k = 0.6
    while k < L - 0.3:
        c = Vector((sw["x0"], 0.0, z0s)) + slope * k + nrm * 0.08
        p.beam(T.straight(c - Y * (w - 0.3), c + Y * (w - 0.3), slope, nrm), 2 * w - 0.6, 0.24, 0.16, "MI_Wood_Piling", nseed(), seg=1.0,
               grime=lambda q: min(max((0.6 - q.z) / 1.4, 0.0), 1.0))
        k += 1.1
    for yw in (-1.6, 1.6):
        a = Vector((sw["x0"] + 0.3, yw, z0s)) + nrm * 0.25
        b = Vector((sw["x1"] - 0.1, yw, z1s)) + nrm * 0.25
        p.beam(T.straight(a, b, Y, nrm), (b - a).length, 0.4, 0.2, "MI_Wood_TeakDark", nseed(), seg=1.0, wear=1.0,
               grime=lambda q: min(max((0.6 - q.z) / 1.4, 0.0), 1.0))
    # The quays: oak decking on the stone, an iron-shod kerb along the edge; the port landing.
    for s in (-1.0, 1.0):
        ya, yb = sorted((s * half, s * quay))
        floor(p, (bs["x0"] + 0.05, bs["x1"], ya, yb), zq, mats=DOCK, width=QUAY_W, grime=grime_floor(zq))
        y = s * (half + 0.15)
        p.beam(T.straight((bs["x0"] + 0.05, y, zq - 0.1), (bs["x1"], y, zq - 0.1), Y, UP), bs["x1"] - 0.05, 0.3, 0.22, DARK, nseed(), seg=1.0, wear=1.0)
        p.beam(T.straight((bs["x0"] + 0.05, s * half + 0.0, zq - 0.06), (bs["x1"], s * half, zq - 0.06), UP, Y * -s), bs["x1"], 0.12, 0.02,
               "MI_Metal_Iron", nseed(), seg=2.0)
    for zid, r in H.open_rects("quay"):
        if zid == "dock" and r[0] >= bs["x1"]:
            floor(p, r, zq, mats=DOCK, width=QUAY_W, grime=grime_floor(zq))
    # The arcades: posts with braces to the edge beam carrying the wing, the wing's underside, bollards, lanterns.
    ap = H.prop("dock", "arcade_posts")
    for s in (-1.0, 1.0):
        y = s * ap["y"]
        x = ap["from"]
        posts = []
        while x <= ap["to"] + 1e-6:
            posts.append(x)
            x += ap["every"]
        hp = ap["size"] * 0.5
        for x in posts:
            p.beam(T.straight((x, y, zq), (x, y, zq + ap["h"] - 0.36), X, Y), ap["h"] - 0.36, ap["size"], ap["size"], DARK, nseed(), seg=0.8, wear=1.0,
                   grime=grime_floor(zq))
            p.block(x - hp - 0.06, x + hp + 0.06, y - hp - 0.06, y + hp + 0.06, zq, zq + 0.3, "MI_Metal_Iron", nseed(), axis="z")   # iron shoe
            for d in (-1.0, 1.0):                                                    # braces up to the beam
                if (x + d * 1.5) < 0.5 or (x + d * 1.5) > HUB["basin"]["x1"] + 0.5:
                    continue
                a = Vector((x + d * (hp + 0.02), y, zq + ap["h"] - 1.8))
                b = Vector((x + d * 1.6, y, zq + ap["h"] - 0.38))
                p.beam(T.straight(a, b, Y, (b - a).normalized().cross(Y) * d), (b - a).length, 0.28, 0.26, DARK, nseed(), wear=1.0)
        p.beam(T.straight((0.2, y, zq + ap["h"] - 0.2), (HUB["basin"]["x1"], y, zq + ap["h"] - 0.2), Y, UP), HUB["basin"]["x1"] - 0.2, 0.62, 0.4,
               DARK, nseed(), seg=1.0, wear=1.0)
        ya, yb = sorted((s * half, s * quay))
        ceiling(p, (0.3, HUB["basin"]["x1"], ya, yb), Z["lower"], every=1.5, knees=False)
        bo = H.prop("dock", "bollards")
        x = bo["from"]
        while x <= bo["to"] + 1e-6:
            c = Vector((x, s * bo["y"], zq))
            p.lathe([(0.0, 0.0), (bo["r"] + 0.05, 0.0), (bo["r"], 0.08), (bo["r"] * 0.82, 0.3), (bo["r"] * 0.85, bo["h"] - 0.14),
                     (bo["r"] * 1.15, bo["h"] - 0.08), (bo["r"] * 1.12, bo["h"]), (0.0, bo["h"] + 0.04)], c, UP, "MI_Metal_Iron", sides=12, wear=1.0)
            x += bo["every"]
    for x, y, zh in H.prop("dock", "lanterns")["at"]:
        s = 1.0 if y > 0 else -1.0
        bracket_lantern(p, Vector((x, s * (ap["y"] - ap["size"] * 0.5), zq)), (0.0, -s, 0.0), H.prop("dock", "lanterns")["hook_z"], drop=0.1)
    # The berth board at the fitting berth: a painted board on a post at the quay's edge.
    fb = next(e for e in H.space("slip")["e"] if e["id"] == "fitting_berth")
    c = Vector((fb["x"], fb["y"] - 0.6, zq))
    p.beam(T.straight(c, c + UP * 1.5, X, Y), 1.5, 0.16, 0.16, DARK, nseed())
    p.block(c.x - 0.5, c.x + 0.5, c.y - 0.12, c.y - 0.04, zq + 1.0, zq + 1.6, "MI_Paint_Cream", nseed(), axis="x")
    for zz in (zq + 1.18, zq + 1.3, zq + 1.42):
        p.block(c.x - 0.38, c.x + 0.3 - random.random() * 0.2, c.y - 0.13, c.y - 0.11, zz - 0.025, zz + 0.025, "MI_Paint_HullDark", nseed(), axis="x")
    crane(p)
    walls_quay_level(p, "dock")
    # Over the hall's mouth: the face from the upper deck up under the main deck's edge (the Slip's sign hangs on it), and
    # the corners by the wings.
    xf = bs["x1"] + WALL * 0.5
    wall(p, "x", xf, -half, half, Z["upper"] - SLAB - 0.6, Z["main"] - SLAB, -1.0)
    for s in (-1.0, 1.0):
        a0, a1 = sorted((s * HUB["hall"]["y1"], s * half))
        wall(p, "x", xf, a0, a1, Z["lower"], Z["upper"] - SLAB - 0.6, -1.0, posts=False)
    export(p)


def crane(p):
    """The Slip's wall crane (spaces.slip.crane): bracketed to the wing's face over an arcade post, its jib out over the
    berth, the fall to a hook block, the rope down to a winch under the arcade."""
    cr = H.prop("slip", "crane")
    zq = Z["quay"]
    zb = zq + cr["bracket_z"]
    x, y = cr["x"], cr["y"]
    jt = Vector((cr["jib_to"][0], cr["jib_to"][1], zq + cr["jib_to"][2]))
    p.beam(T.straight((x, y - 0.25, zb - 2.4), (x, y - 0.25, zb + 0.5), X, Y), 2.9, 0.4, 0.4, DARK, nseed(), wear=1.0)         # the post on the wall
    for zz in (zb - 2.0, zb + 0.2):
        p.block(x - 0.3, x + 0.3, y - 0.47, y - 0.02, zz - 0.06, zz + 0.06, "MI_Metal_Iron", nseed(), axis="x")
    a = Vector((x, y - 0.3, zb))
    p.beam(T.straight(a, jt + Vector((0, 0, 0.1)), X, UP), (jt - a).length, 0.32, 0.36, DARK, nseed(), wear=1.0)
    s0, s1 = Vector((x, y - 0.35, zb - 2.1)), Vector((x, jt.y + 0.5, zb - 0.1))
    p.beam(T.straight(s0, s1, X, (s1 - s0).normalized().cross(X)), (s1 - s0).length, 0.24, 0.24, DARK, nseed())
    p.lathe([(0.0, -0.12), (0.3, -0.12), (0.34, 0.0), (0.3, 0.12), (0.0, 0.12)], jt + Vector((0, 0, 0.1)), X, "MI_Wood_Varnish", sides=12)
    hook = Vector((jt.x, jt.y, zq + 3.0))
    p.round(T.straight(jt - UP * 0.15, hook + UP * 0.25, X, Y), (jt - hook).length - 0.4, 0.035, "MI_Rope_Hemp", nseed(), sides=6, seg=10.0, wear=0.0)
    p.block(hook.x - 0.14, hook.x + 0.14, hook.y - 0.1, hook.y + 0.1, hook.z - 0.15, hook.z + 0.25, "MI_Wood_Varnish", nseed(), axis="z")
    p.beam(T.arc(hook - UP * 0.4, Y, UP, 0.14, math.pi * 0.95, math.pi * 2.2), 0.6, 0.05, 0.05, "MI_Metal_Iron", nseed(), seg=0.08)
    wx, wy = cr["winch"]
    w = Vector((wx, wy, zq))
    for dx in (-0.45, 0.45):                                                 # the winch: two cheeks, a drum, crank handles
        p.block(wx + dx - 0.06, wx + dx + 0.06, wy - 0.4, wy + 0.4, zq, zq + 0.95, DARK, nseed(), axis="z")
    p.lathe([(0.2, -0.38), (0.24, -0.34), (0.24, 0.34), (0.2, 0.38)], w + UP * 0.7, X, "MI_Wood_Varnish", sides=12)
    for k in range(3):
        p.beam(T.arc(w + UP * 0.7 + X * (-0.2 + 0.2 * k), Y, UP, 0.27, 0.0, 2 * math.pi), 1.7, 0.06, 0.06, "MI_Rope_Hemp", nseed(), seg=0.1)
    for dx in (-0.55, 0.55):
        c = w + UP * 0.7 + X * dx
        p.beam(T.straight(c, c + X * (0.12 if dx > 0 else -0.12) - UP * 0.35, Y, X), 0.37, 0.05, 0.05, "MI_Metal_Iron", nseed())
    p.round(T.straight(w + UP * 0.95, Vector((x, y - 0.2, zq + 3.9)), X, Y), (Vector((x, y - 0.2, zq + 3.9)) - w).length, 0.03, "MI_Rope_Hemp", nseed(),
            sides=6, seg=10.0, wear=0.0)


def walls_quay_level(p, which):
    """The quay level's walls as the blockout runs them; the dock gets those along the quays, the Slip those of the hall
    and the shop (the Keelgate's come with the Keelgate)."""
    zq = Z["quay"]
    hall, shop = H.R(HUB["hall"]), H.R(HUB["shop"])
    bs = HUB["basin"]
    stair = H.R(HUB["stair"])
    hall_h = H.prop("slip", "hall")["h"]
    opens = [r for zid, r in H.open_rects("quay") if zid != "stair"]
    land = next(r for zid, r in H.open_rects("lower") if zid == "tally" and r[1] - r[0] < 5.0)
    foot = land[0] - 22 * B.TREAD
    openings = [("y", stair[2], stair[0], foot, 0.0, 2.6)]
    for sd, x0_, x1_, z0_, z1_ in HH.data_openings():                        # the shop's windows through the hull
        if sd > 0 and z1_ < Z["lower"]:
            openings.append(("y", shop[3], x0_, x1_, z0_ - zq, z1_ - zq))

    def heights(x, y):
        return hall_h if H.inside(hall, x, y) else Z["lower"] - zq
    water = [(bs["x0"], bs["x1"], bs["y0"], bs["y1"])]
    runs = B.rasterise(zq, opens, [], [], [], water, heights, [])

    def is_open(x, y):
        return any(H.inside(r, x, y) for r in opens)

    def mine(axis, line, a0, a1):
        mid = 0.5 * (a0 + a1)
        x, y = (line, mid) if axis == "x" else (mid, line)
        landing = x < bs["x1"] + 4.5 and y < -9.0                               # the port landing's walls go with the dock
        if which == "dock":
            return x < bs["x1"] - 0.01 or landing
        if which == "slip":
            return bs["x1"] - 0.01 <= x <= hall[1] + 0.01 and not landing
        return False
    for kind, axis, line, a0, a1, hgt in runs:
        if kind != "wall" or not mine(axis, line, a0, a1):
            continue
        ops = [(o[2], o[3], zq + o[4], zq + o[5]) for o in openings if o[0] == axis and abs(o[1] - line) < 0.01 and o[3] > a0 and o[2] < a1]
        for s in open_side(is_open, axis, line, a0, a1):
            if axis == "y" and abs(line) >= HUB["quayY"] - 0.01 and line * s > 0:
                continue                                                        # (only the face towards the quay)
            wall(p, axis, line, a0, a1, zq, zq + hgt - SLAB, s, ops)


# --- the Slip -------------------------------------------------------------------------------------------------------------
def build_slip():
    p = T.Part("SM_Hub_Slip")
    zq = Z["quay"]
    hall, shop = H.R(HUB["hall"]), H.R(HUB["shop"])
    sw = H.prop("dock", "slipway")
    sl = {pr["id"]: pr for pr in H.space("slip")["props"]}
    # Floors: the hall and the shop, round the slipway's head; the ways run on up the hall floor to the cradles.
    floor(p, (hall[0], hall[1], hall[2], shop[3]), zq, holes=[(hall[0], sw["x1"], sw["y0"], sw["y1"])], grime=grime_floor(zq))
    # Ceilings: the hall's 12 m up under the upper deck (heavy beams, knees), the shop's under the Tally House.
    ceiling(p, hall, Z["upper"], every=2.0, beam=(0.42, 0.45))
    ceiling(p, shop, Z["lower"], every=1.3)
    walls_quay_level(p, "slip")
    # The hall's starboard side above the shop: the Tally House's front with the windows looking down, the middle deck.
    win = H.prop("tally", "hall_windows")
    zl = Z["lower"]
    wall(p, "y", hall[3], hall[0], hall[1], zl - SLAB, Z["upper"] - SLAB, -1.0,
         openings=[(a, b, zl + win["z0"], zl + win["z1"]) for a, b in win["spans"]])
    # Over the shop's open side to the hall: a beam on posts carrying the Tally House's front.
    for x in (hall[0] + 0.4, hall[1] - 0.4):                                 # (at the walls: nothing stands in the way)
        p.beam(T.straight((x, shop[2], zq), (x, shop[2], Z["lower"] - SLAB - 0.3), X, Y), Z["lower"] - SLAB - 0.3 - zq, 0.42, 0.42, DARK, nseed(), wear=1.0)
    p.beam(T.straight((hall[0], shop[2], Z["lower"] - SLAB - 0.15), (hall[1], shop[2], Z["lower"] - SLAB - 0.15), Y, UP), hall[1] - hall[0], 0.5, 0.36,
           DARK, nseed(), seg=1.0)
    # The hall's walls above the shop (the Tally House's windows look down here) and to port: the middle deck's level.
    # (The Tally House's own wall with the windows is built with the Tally House.)
    # The counter across the shop: a planked front, a top board, a flap at its hall end.
    c = sl["counter"]
    hgt = c["h"]
    Fc = (Vector((c["x0"], 0.0, 0.0)), Y, UP, -X)
    HE.wall_boards(p, Fc, c["y0"], c["y1"], zq, zq + hgt - 0.06, None, mats=("MI_Wood_Varnish", "MI_Wood_TeakDark"))
    p.block(c["x0"] - 0.12, c["x1"] + 0.08, c["y0"] - 0.05, c["y1"] + 0.05, zq + hgt - 0.07, zq + hgt, "MI_Wood_TeakLight", nseed(), axis="y")
    for yy in (c["y0"] + 0.05, 0.5 * (c["y0"] + c["y1"]), c["y1"] - 0.05):
        p.block(c["x0"], c["x1"], yy - 0.06, yy + 0.06, zq, zq + hgt - 0.07, "MI_Wood_TeakDark", nseed(), axis="z")
    # On the counter: a ledger, an inkpot, a few tools.
    p.block(c["x0"] + 0.15, c["x0"] + 0.6, 13.0, 13.6, zq + hgt, zq + hgt + 0.06, "MI_Paint_HullDark", nseed(), axis="y")
    p.lathe([(0.0, 0.0), (0.05, 0.0), (0.05, 0.06), (0.02, 0.08), (0.0, 0.08)], Vector((c["x0"] + 0.4, 14.0, zq + hgt)), UP, "MI_Glass_Dark", sides=8)
    # The plans board by the door from the quay; the half-models over the shop's forward wall; the workbench.
    pb = sl["plans_board"]
    xw = 44.0 + WALL * 0.5
    p.block(xw, xw + 0.06, pb["y0"], pb["y1"], zq + pb["z0"], zq + pb["z1"], "MI_Wood_Teak", nseed(), axis="y")
    rng = random.Random(9)
    for k in range(5):
        y0 = pb["y0"] + 0.2 + k * 0.75
        z0 = zq + pb["z0"] + 0.2 + rng.uniform(0.0, 0.5)
        p.polygon([(xw + 0.07, y0, z0), (xw + 0.07, y0 + 0.62, z0 - 0.02), (xw + 0.07, y0 + 0.64, z0 + 0.82), (xw + 0.07, y0 + 0.02, z0 + 0.8)],
                  "MI_Paint_CreamAged", tone=rng.random(), facing=X)
    hm = sl["half_models"]
    xw = 64.0 - WALL * 0.5
    for k in range(3):
        y0 = hm["y0"] + 0.3 + k * 2.0
        p.block(xw - 0.04, xw, y0 - 0.1, y0 + 1.6, zq + hm["z0"] - 0.05, zq + hm["z1"] + 0.05, "MI_Wood_TeakDark", nseed(), axis="y")
        pts = []
        for i in range(13):
            t = i / 12
            pts.append(Vector((xw - 0.05 - 0.22 * math.sin(math.pi * t) ** 0.7, y0 + 1.5 * t, zq + hm["z1"] - 0.05 - 0.35 * math.sin(math.pi * t) ** 0.8)))
        f = T.polyline(pts, lambda q: UP, lambda q: -X)
        p.beam(f, T.length_of(f), 0.12, 0.14, ("MI_Paint_Hull", "MI_Paint_Cream", "MI_Wood_Varnish")[k], nseed(), seg=0.1)
    wb = sl["workbench"]
    p.block(wb["x0"], wb["x1"], wb["y0"], wb["y1"], zq + wb["h"] - 0.12, zq + wb["h"], "MI_Wood_TeakDark", nseed(), axis="x")
    for xx in (wb["x0"] + 0.15, wb["x1"] - 0.15):
        for yy in (wb["y0"] + 0.15, wb["y1"] - 0.15):
            p.block(xx - 0.08, xx + 0.08, yy - 0.08, yy + 0.08, zq, zq + wb["h"] - 0.12, "MI_Wood_TeakDark", nseed(), axis="z")
    p.block(wb["x0"] + 0.1, wb["x1"] - 0.1, wb["y0"] + 0.1, wb["y1"] - 0.1, zq + 0.18, zq + 0.24, "MI_Wood_Teak", nseed(), axis="x")
    p.block(wb["x0"] + 0.6, wb["x0"] + 1.0, wb["y0"] + 0.4, wb["y0"] + 1.0, zq + wb["h"], zq + wb["h"] + 0.14, "MI_Metal_Iron", nseed(), axis="y")    # a vice
    # The timber racks on the hall's forward wall: four rows of planks on iron brackets.
    tr = sl["timber_racks"]
    xw = 64.0 - WALL * 0.5
    for k in range(4):
        z = zq + tr["z0"] + (tr["z1"] - tr["z0"]) * k / 3
        for yb in (tr["y0"] + 0.4, 0.5 * (tr["y0"] + tr["y1"]), tr["y1"] - 0.4):
            p.block(xw - 0.6, xw, yb - 0.04, yb + 0.04, z - 0.06, z, "MI_Metal_Iron", nseed(), axis="x")
        for j in range(3):
            y0 = tr["y0"] + rng.uniform(-0.3, 0.6)
            p.board(T.straight((xw - 0.15 - 0.15 * j, y0, z + 0.05 + 0.08 * j), (xw - 0.15 - 0.15 * j, tr["y1"] - rng.uniform(0.0, 1.5), z + 0.05 + 0.08 * j), X, UP),
                    tr["y1"] - y0 - 0.7, 0.14, 0.07, rng.choice(("MI_Wood_TeakLight", "MI_Wood_Teak", "MI_Wood_Planks")), nseed(), seg=1.0)
    # The 14 m cutter on her cradles, her mast on trestles.
    cutter(p, sl["display_boat"])
    mt = sl["mast_on_trestles"]
    for tx in (mt["x0"] + 2.0, mt["x1"] - 2.0):
        for dy in (-0.45, 0.45):
            a, b = Vector((tx - 0.3, mt["y"] + dy * 1.4, zq)), Vector((tx, mt["y"] + dy * 0.2, zq + mt["z"]))
            p.beam(T.straight(a, b, X, (b - a).normalized().cross(X)), (b - a).length, 0.12, 0.12, DARK, nseed())
        p.block(tx - 0.12, tx + 0.12, mt["y"] - 0.5, mt["y"] + 0.5, zq + mt["z"] - 0.18, zq + mt["z"], "MI_Wood_TeakDark", nseed(), axis="y")
    a, b = Vector((mt["x0"], mt["y"], zq + mt["z"] + 0.24)), Vector((mt["x1"], mt["y"], zq + mt["z"] + 0.2))
    p.round(T.straight(a, b, Y, UP), mt["x1"] - mt["x0"], lambda s: 0.24 - 0.1 * s, "MI_Wood_Varnish", nseed(), sides=10, seg=1.0, wear=0.6)
    # Lanterns hanging on chains from the hall's beams; the sign over the hall's mouth.
    for x, y, zz in sl["lanterns"]["at"]:
        lantern(p, Vector((x, y, zq + zz)), chain_to=(x, y, Z["upper"] - SLAB - 0.45), r=0.28)
    sg = sl["sign"]
    zc = zq + sg["z"]
    p.block(sg["x"] - 0.35, sg["x"] - 0.05, sg["y"] - 3.3, sg["y"] + 3.3, zc - 0.55, zc + 0.55, "MI_Paint_HullDark", nseed(), axis="y")
    p.block(sg["x"] - 0.4, sg["x"] - 0.05, sg["y"] - 3.4, sg["y"] + 3.4, zc - 0.62, zc - 0.5, "MI_Hub_Ochre", nseed(), axis="y")
    p.block(sg["x"] - 0.4, sg["x"] - 0.05, sg["y"] - 3.4, sg["y"] + 3.4, zc + 0.5, zc + 0.62, "MI_Hub_Ochre", nseed(), axis="y")
    sign_text(p, sg["text"], sg["x"] - 0.36, sg["y"], zc, 0.6)
    export(p)


SIGN_LETTERS = {
    "S": [((1, 0.9), (0.15, 1)), ((0.15, 1), (0, 0.75)), ((0, 0.75), (1, 0.28)), ((1, 0.28), (0.85, 0)), ((0.85, 0), (0, 0.1))],
    "I": [((0.5, 0), (0.5, 1))],
    "P": [((0, 0), (0, 1)), ((0, 1), (0.85, 0.95)), ((0.85, 0.95), (0.95, 0.72)), ((0.95, 0.72), (0.8, 0.5)), ((0.8, 0.5), (0, 0.48))],
    "R": [((0, 0), (0, 1)), ((0, 1), (0.85, 0.95)), ((0.85, 0.95), (0.95, 0.72)), ((0.95, 0.72), (0.8, 0.5)), ((0.8, 0.5), (0, 0.48)),
          ((0.45, 0.48), (1, 0))],
    "G": [((1, 0.85), (0.6, 1)), ((0.6, 1), (0.1, 0.8)), ((0.1, 0.8), (0, 0.4)), ((0, 0.4), (0.3, 0)), ((0.3, 0), (0.9, 0.05)),
          ((0.9, 0.05), (1, 0.45)), ((1, 0.45), (0.55, 0.45))],
    "T": [((0, 1), (1, 1)), ((0.5, 1), (0.5, 0))],
    "C": [((1, 0.85), (0.6, 1)), ((0.6, 1), (0.1, 0.8)), ((0.1, 0.8), (0, 0.4)), ((0, 0.4), (0.3, 0)), ((0.3, 0), (1, 0.1))],
    "A": [((0, 0), (0.5, 1)), ((0.5, 1), (1, 0)), ((0.2, 0.4), (0.8, 0.4))],
    "N": [((0, 0), (0, 1)), ((0, 1), (1, 0)), ((1, 0), (1, 1))],
    "D": [((0, 0), (0, 1)), ((0, 1), (0.7, 0.92)), ((0.7, 0.92), (1, 0.5)), ((1, 0.5), (0.7, 0.08)), ((0.7, 0.08), (0, 0))],
    "Y": [((0, 1), (0.5, 0.5)), ((1, 1), (0.5, 0.5)), ((0.5, 0.5), (0.5, 0))],
}


def sign_text(p, text, x, yc, zc, h):
    """Painted-on letters in cream strokes (reads from astern), centred at (yc, zc)."""
    letters = dict(HE.LETTERS)
    letters.update(SIGN_LETTERS)
    w, gap = h * 0.62, h * 0.3
    total = len(text) * w + (len(text) - 1) * gap
    y = yc - total * 0.5
    for ch in text:
        strokes = letters.get(ch)
        if strokes == "ring":
            pts = [(0.5 + 0.5 * math.cos(2 * math.pi * k / 14), 0.5 + 0.5 * math.sin(2 * math.pi * k / 14)) for k in range(15)]
            strokes = list(zip(pts[:-1], pts[1:]))
        for (u0, v0), (u1, v1) in strokes or []:
            a = Vector((x, y + u0 * w, zc - h * 0.5 + v0 * h))
            b = Vector((x, y + u1 * w, zc - h * 0.5 + v1 * h))
            d = (b - a).normalized()
            p.beam(T.straight(a - d * 0.03, b + d * 0.03, (-X).cross(d), -X), (b - a).length + 0.06, h * 0.13, 0.03, "MI_Paint_Cream", nseed(), seg=2.0, wear=0.8)
        y += w + gap


def cutter(p, db, depth=2.4, cradles=True):
    """The next boat for sale (spaces.slip.display_boat: the 14 m cutter, D51), unrigged on her cradles: carvel strakes in
    oxblood under a cream sheer strake (the player's boat's colours), a varnished wale and rail, a planked deck, her keel,
    stem and transom."""
    zq = Z["quay"]
    x0, x1, yc = db["x0"], db["x1"], db["y"]
    hb = db["beam"] * 0.5
    zk = zq + db["keel_z"]
    L = x1 - x0

    def sheer(t):
        return zk + depth + 0.35 * (2.0 * t - 1.0) ** 2 + 0.15 * t

    def half(t, v):
        """Half-breadth at t (0 stern .. 1 bow) and v (0 keel .. 1 sheer)."""
        plan = math.sin(math.pi * min(max(0.08 + 0.92 * t, 0.0), 1.0)) ** 0.55 * (1.0 if t < 0.5 else 1.0 - 0.15 * (2 * t - 1) ** 2)
        plan = max(plan, 0.55 if t < 0.04 else 0.0)                                    # her transom
        return hb * plan * (v ** 0.55)

    def pt(t, v, side):
        x = x0 + L * t
        zt = sheer(t)
        z = zk + (zt - zk) * v
        return Vector((x, yc + side * half(t, v), z))
    n_st = 9
    for side in (1.0, -1.0):
        for k in range(n_st):
            va, vb = k / n_st, (k + 1) / n_st
            top = k == n_st - 1
            cuts = sorted(random.uniform(0.25, 0.75) for _ in range(random.choice((1, 2))))
            ts = [0.02] + cuts + [0.985]
            for ta, tb in zip(ts[:-1], ts[1:]):
                def fr(s, ta=ta, tb=tb, va=va, vb=vb, side=side):
                    t = ta + (tb - ta) * s
                    pa, pb = pt(t, va, side), pt(t, vb, side)
                    acr = pb - pa
                    w = max(acr.length, 1e-3)
                    a = acr / w
                    tt = pt(min(t + 0.01, 1.0), 0.5 * (va + vb), side) - pt(max(t - 0.01, 0.0), 0.5 * (va + vb), side)
                    n = tt.cross(a).normalized()
                    if n.y * side < 0:
                        n = -n
                    return (pa + pb) * 0.5 + n * 0.03, a, n, max(w - 0.015, 0.02) / 0.4
                mat = ("MI_Paint_CreamAged" if random.random() < 0.3 else "MI_Paint_Cream") if top else \
                      ("MI_Paint_HullFaded" if random.random() < 0.2 else "MI_Paint_Hull")
                p.board(fr, T.length_of(fr), 0.4, 0.06, mat, nseed(), seg=0.4, wear=0.7, bend=0.004)
        # The wale and the cap rail along the sheer.
        for zoff, mat, wd in ((-0.12, "MI_Wood_Varnish", 0.16), (0.08, "MI_Wood_Varnish", 0.14)):
            pts = [pt(t, 1.0, side) + Vector((0.0, side * 0.06, zoff)) for t in (i / 24 * 0.96 + 0.02 for i in range(25))]
            f = T.polyline(pts, lambda q: UP, lambda q, s=side: Vector((0.0, s, 0.0)))
            p.beam(f, T.length_of(f), wd, 0.08, mat, nseed(), seg=0.4)
    # Keel, stem and transom; the deck.
    p.beam(T.straight((x0 + 0.2, yc, zk - 0.15), (x1 - 0.6, yc, zk - 0.15), Y, UP), L - 0.8, 0.24, 0.36, "MI_Wood_TeakDark", nseed(), seg=1.0)
    stem = [pt(1.0, v, 1.0) * 0 + Vector((x0 + L * (0.985 + 0.012 * v), yc, zk + (sheer(1.0) - zk) * v)) for v in (i / 8 for i in range(9))]
    f = T.polyline(stem, lambda q: Y, lambda q: X)
    p.beam(f, T.length_of(f), 0.2, 0.22, "MI_Wood_Varnish", nseed(), seg=0.3)
    tr = [pt(0.02, v, 1.0) for v in (i / 6 for i in range(7))]
    for v0 in (0.2, 0.5, 0.8):
        a, b = pt(0.02, v0, 1.0), pt(0.02, v0, -1.0)
        p.board(T.straight(a - X * 0.04, b - X * 0.04, UP, -X), (b - a).length, 0.6, 0.06, "MI_Paint_Hull", nseed(), seg=0.4)
    yd = -hb * 0.95
    while yd < hb * 0.95:
        # Each deck plank only where the hull is wider than it (her plan narrows to the ends), following the sheer.
        ts = [i / 200 for i in range(201)]
        inside = [t for t in ts if half(t, 1.0) - 0.12 > abs(yd) + 0.09]
        if len(inside) > 2:
            ta, tb = inside[0], inside[-1]
            pts = [Vector((x0 + L * t, yc + yd, sheer(t) - 0.09)) for t in (ta + (tb - ta) * k / 10 for k in range(11))]
            f = T.polyline(pts, lambda q: Y, lambda q: UP)
            p.board(f, T.length_of(f), 0.17, 0.04, random.choice(TEAK), nseed(), seg=0.8)
        yd += 0.18
    # The cradles at x 50, 54, 58 (display_boat note): timber saddles shaped to her bottom, wedges, shores.
    for cx in ((50.0, 54.0, 58.0) if cradles else ()):
        t = (cx - x0) / L
        p.block(cx - 0.18, cx + 0.18, yc - 1.4, yc + 1.4, zq, zk - 0.25, "MI_Wood_TeakDark", nseed(), axis="y")
        for side in (1.0, -1.0):
            q = pt(t, 0.25, side)
            p.beam(T.straight(Vector((cx, yc + side * 1.3, zq)), q + Vector((0, 0, -0.05)), X, Vector((0, side, 0.3)).normalized()),
                   (q - Vector((cx, yc + side * 1.3, zq))).length, 0.16, 0.16, "MI_Wood_TeakDark", nseed())
            q2 = pt(t, 0.7, side) + Vector((0, side * 0.05, 0))
            a = Vector((cx + 0.3, yc + side * (hb + 1.0), zq))
            p.beam(T.straight(a, q2, X, (q2 - a).normalized().cross(X)), (q2 - a).length, 0.14, 0.14, "MI_Wood_Planks", nseed())


# --- stairs, the hull's inner side -----------------------------------------------------------------------------------------
def stairs(p, x0, x1, y0, y1, up, z0, z1, rails=True, soffit=True, sides=(True, True)):
    """A flight on the blockout's steps (22 risers of 18 cm on 30 cm treads from the bottom end, the rest a landing):
    oak treads with a nosing, risers, stringers along both sides, a boarded soffit under it, balustrades when railed."""
    n = int(round((z1 - z0) / B.RISER))
    run = n * B.TREAD
    along_x = up[1] == "x"
    sign = 1.0 if up[0] == "+" else -1.0
    start = (x0 if sign > 0 else x1) if along_x else (y0 if sign > 0 else y1)
    lo_c, hi_c = (y0, y1) if along_x else (x0, x1)
    rng = random.Random(nseed())

    def P(t, c, z):
        a = start + sign * t
        return Vector((a, c, z)) if along_x else Vector((c, a, z))
    fwd = (X if along_x else Y) * sign
    acr = Y if along_x else X
    for k in range(n):
        top = z0 + (k + 1) * B.RISER
        ta, tb = k * B.TREAD, (k + 1) * B.TREAD
        a, b = P(0.5 * (ta + tb) - 0.015, lo_c + 0.06, top - 0.03), P(0.5 * (ta + tb) - 0.015, hi_c - 0.06, top - 0.03)
        p.board(T.straight(a, b, fwd, UP), (b - a).length, B.TREAD + 0.03, 0.06, rng.choice(TEAK), nseed(), seg=1.0, wear=1.0)
        a, b = P(ta + 0.02, lo_c + 0.06, top - B.RISER * 0.5 - 0.03), P(ta + 0.02, hi_c - 0.06, top - B.RISER * 0.5 - 0.03)
        p.board(T.straight(a, b, UP, -fwd), (b - a).length, B.RISER - 0.06, 0.03, "MI_Wood_TeakDark", nseed(), seg=1.0, wear=0.5)
    for c, keep in ((lo_c + 0.05, sides[0]), (hi_c - 0.05, sides[1])):            # stringers
        if not keep:
            continue
        a, b = P(-0.15, c, z0 + 0.12), P(run + 0.05, c, z1 + 0.12)
        d = (b - a).normalized()
        w = d.cross(acr)
        if w.z < 0:
            w = -w
        p.beam(T.straight(a, b, w, acr), (b - a).length, 0.36, 0.1, "MI_Wood_TeakDark", nseed(), seg=1.0, wear=0.9)
    if soffit:
        a, b = P(0.0, 0.5 * (lo_c + hi_c), z0), P(run, 0.5 * (lo_c + hi_c), z1 - B.RISER)
        d = (b - a).normalized()
        nrm = d.cross(acr)
        if nrm.z > 0:
            nrm = -nrm
        c = lo_c + 0.1
        while c < hi_c - 0.1:
            cb = min(c + 0.3, hi_c - 0.1)
            q0, q1 = P(0.1, 0.5 * (c + cb), z0 - 0.05), P(run, 0.5 * (c + cb), z1 - B.RISER - 0.05)
            p.board(T.straight(q0, q1, acr, nrm), (q1 - q0).length, cb - c - 0.01, 0.05, "MI_Wood_Planks", nseed(), seg=1.0, wear=0.5)
            c = cb
    if rails:
        for c in (lo_c, hi_c):
            a, b = P(0.0, c, z0), P(run, c, z1)
            d = (b - a)
            L = d.length
            d.normalize()
            o = d.cross(acr)
            if o.z < 0:
                o = -o
            p.beam(T.straight(a + UP * B.RAIL_H, b + UP * B.RAIL_H, acr, o), L, 0.12, 0.1, "MI_Wood_Varnish", nseed(), seg=1.0)
            nb = int(L / 0.32)
            for k in range(nb + 1):
                q = a + (b - a) * (k / nb)
                h = B.RAIL_H - 0.05
                p.lathe([(0.05, 0.0), (0.05, 0.05), (0.035, 0.1), (0.06, h * 0.45), (0.035, h * 0.8), (0.045, h * 0.9), (0.045, h)], q, UP,
                        "MI_Wood_TeakLight", sides=6, wear=0.6)
            for t, zz in ((0.0, z0), (run, z1)):                                   # newel posts
                q = P(t, c, zz)
                p.beam(T.straight(q, q + UP * (B.RAIL_H + 0.15), X, Y), B.RAIL_H + 0.15, 0.18, 0.18, "MI_Wood_Varnish", nseed())


def hull_lining(p, side, x0, x1, z0, z1, holes=()):
    """The hull's inner side where a room meets it: curved frames every metre (following the section's tumblehome) and
    lining boards across them, their faces on the hull's collision (the blockout shell); holes: (x0, x1, z0, z1)."""
    rng = random.Random(nseed())

    def yin(x, z, d):
        return side * (H.half_width(x, z) - d)
    x = x0 + 0.2
    while x < x1 - 0.1:
        if not any(h[0] - 0.15 < x < h[1] + 0.15 for h in holes):
            pts = [Vector((x, yin(x, z, 0.14), z)) for z in [z0 + (z1 - z0) * k / 6 for k in range(7)]]
            f = T.polyline(pts, lambda q: X, lambda q, s=side: Vector((0.0, -s, 0.0)))
            p.beam(f, T.length_of(f), 0.24, 0.16, "MI_Wood_TeakDark", nseed(), seg=0.5, wear=0.8)
        x += 1.0
    z = z0
    while z < z1 - 0.05:
        zt = min(z + 0.3, z1)
        zm = 0.5 * (z + zt)
        segs = [(x0, x1)]
        for h in holes:
            if h[2] - 0.02 < zm < h[3] + 0.02:
                segs = [q for sg in segs for q in ((sg[0], min(sg[1], h[0])), (max(sg[0], h[1]), sg[1])) if q[1] - q[0] > 0.1]
        for a, b in segs:
            xa = a
            while xa < b - 1e-6:
                xb = min(xa + rng.uniform(3.0, 6.0), b)
                if b - xb < 0.6:
                    xb = b

                def fr(s_, xa=xa, xb=xb, z=z, zt=zt):
                    xx = xa + (xb - xa) * s_
                    pa, pb = Vector((xx, yin(xx, z, 0.06), z)), Vector((xx, yin(xx, zt, 0.06), zt))
                    acr = (pb - pa)
                    w = acr.length
                    return (pa + pb) * 0.5, acr / w, Vector((0.0, -side, 0.0)), max(w - 0.015, 0.02) / 0.29
                p.board(fr, xb - xa, 0.29, 0.07, rng.choice(WALL_MATS), nseed(), seg=0.8, wear=rng.uniform(0.4, 0.9))
                xa = xb
        z = zt
    for h in holes:                                                              # the ports' linings through the side
        for xx in (h[0], h[1]):
            a, b = Vector((xx, yin(xx, h[2], 0.0), h[2])), Vector((xx, yin(xx, h[3], 0.0), h[3]))
            p.beam(T.straight(a + Y * side * 0.1, b + Y * side * 0.1, X, Y * side), (b - a).length, 0.05, 0.42, "MI_Wood_Planks", nseed())
        for zz in (h[2], h[3]):
            a, b = Vector((h[0], yin(h[0], zz, 0.0), zz)), Vector((h[1], yin(h[1], zz, 0.0), zz))
            p.beam(T.straight(a + Y * side * 0.1, b + Y * side * 0.1, UP, Y * side), (b - a).length, 0.05, 0.42, "MI_Wood_Planks", nseed())
        a = Vector((h[0] - 0.15, yin(h[0], h[2] - 0.08, 0.14), h[2] - 0.08))
        b = Vector((h[1] + 0.15, yin(h[1], h[2] - 0.08, 0.14), h[2] - 0.08))
        p.beam(T.straight(a, b, UP, Vector((0.0, -side, 0.0))), (b - a).length, 0.14, 0.2, "MI_Wood_TeakDark", nseed())          # sill


def chalk_slips(p, xw, y0, y1, z0, z1, n, facing, rng, mat="MI_Paint_CreamAged"):
    """Paper slips pinned up on a board (its face towards +x or -x), a little askew."""
    cols = 3
    rows = max(1, (n + cols - 1) // cols)
    for k in range(n):
        r, c = divmod(k, cols)
        yc = y0 + (y1 - y0) * (c + 0.5) / cols + rng.uniform(-0.1, 0.1)
        zc = z1 - (z1 - z0) * (r + 0.5) / rows + rng.uniform(-0.08, 0.08)
        w, h = rng.uniform(0.42, 0.55), rng.uniform(0.5, 0.65)
        tilt = rng.uniform(-0.08, 0.08)
        pts = []
        for du, dv in ((-w / 2, -h / 2), (w / 2, -h / 2), (w / 2, h / 2), (-w / 2, h / 2)):
            u, v = du * math.cos(tilt) - dv * math.sin(tilt), du * math.sin(tilt) + dv * math.cos(tilt)
            pts.append((xw + facing * 0.012, yc + u, zc + v))
        p.polygon(pts, mat, tone=rng.random(), facing=(facing, 0.0, 0.0))
        p.lathe([(0.0, 0.0), (0.025, 0.0), (0.025, 0.02), (0.0, 0.03)], Vector((xw + facing * 0.014, yc + rng.uniform(-0.05, 0.05), zc + h / 2 - 0.05)),
                Vector((facing, 0.0, 0.0)), "MI_Metal_Brass", sides=6)


def sign_text_plane(p, text, c, right, out, h):
    """Painted cream letters in a plane: `right` is the reader's right, `out` towards the reader."""
    letters = dict(HE.LETTERS)
    letters.update(SIGN_LETTERS)
    w, gap = h * 0.62, h * 0.3
    total = len(text) * w + (len(text) - 1) * gap
    o = c - right * (total * 0.5)
    for ch in text:
        strokes = letters.get(ch)
        if strokes == "ring":
            pts = [(0.5 + 0.5 * math.cos(2 * math.pi * k / 14), 0.5 + 0.5 * math.sin(2 * math.pi * k / 14)) for k in range(15)]
            strokes = list(zip(pts[:-1], pts[1:]))
        for (u0, v0), (u1, v1) in strokes or []:
            a = o + right * (u0 * w) + UP * (v0 * h - h * 0.5)
            b = o + right * (u1 * w) + UP * (v1 * h - h * 0.5)
            d = (b - a).normalized()
            p.beam(T.straight(a - d * 0.02, b + d * 0.02, out.cross(d), out), (b - a).length + 0.04, h * 0.13, 0.02, "MI_Paint_Cream", nseed(), seg=2.0, wear=0.8)
        o = o + right * (w + gap)


def sign_on_x(p, text, x, yc, zc, h, facing):
    """Letters on a face looking along x (facing -1: the reader stands at smaller x; their right is +y... mirrored for +1)."""
    right = Vector((0.0, 1.0 if facing < 0 else -1.0, 0.0))
    sign_text_plane(p, text, Vector((x, yc, zc)), right, Vector((facing, 0.0, 0.0)), h)


def sign_on_y(p, text, xc, y, zc, h, facing):
    right = Vector((-1.0 if facing < 0 else 1.0, 0.0, 0.0))
    sign_text_plane(p, text, Vector((xc, y, zc)), right, Vector((0.0, facing, 0.0)), h)


# --- the dock stair and the Tally House -------------------------------------------------------------------------------------
def build_tally():
    p = T.Part("SM_Hub_Tally")
    zq, zl, zmid = Z["quay"], Z["lower"], Z["middle"]
    st = H.R(HUB["stair"])
    land = next(r for zid, r in H.open_rects("lower") if zid == "tally" and r[1] - r[0] < 5.0)
    foot = land[0] - 22 * B.TREAD
    tally, lane = H.R(HUB["tally"]), H.R(HUB["lane"])
    tl = {pr["id"]: pr for pr in H.space("tally")["props"]}
    rng = random.Random(77)
    # The dock stair: in through the arcade's opening, a landing, 22 steps up to the Tally House; walls, a handrail.
    floor(p, (st[0], foot, st[2], st[3]), zq, grime=grime_floor(zq))
    stairs(p, foot, land[0], st[2], st[3], "+x", zq, zl, rails=False, soffit=True)
    floor(p, (land[0], land[1], land[2], land[3]), zl)
    wall(p, "y", st[3], st[0], st[1], zq, zmid - SLAB, -1.0)
    wall(p, "x", st[0], st[2], st[3], zq, zmid - SLAB, 1.0)
    wall(p, "y", st[2], st[0], st[1], zl, zmid - SLAB, 1.0, posts=False)
    wall(p, "y", st[2], st[0], foot, zq, zl, 1.0, openings=[(st[0], foot, zq, zq + 2.6)], posts=False, plate=False)
    wall(p, "y", st[2], foot, st[1], zq, zl, 1.0, posts=False, plate=False)
    ceiling(p, st, zmid, every=1.4, knees=False)
    for yy, s in ((st[3], -1.0), (st[2], 1.0)):                                  # a handrail on iron brackets up each side
        a = Vector((foot, yy + s * (WALL * 0.5 + 0.1), zq + 0.95))
        b = Vector((land[0], yy + s * (WALL * 0.5 + 0.1), zl + 0.95))
        d = (b - a).normalized()
        p.round(T.straight(a, b, Y, d.cross(Y)), (b - a).length, 0.045, "MI_Wood_Varnish", nseed(), sides=8)
        for t in (0.15, 0.5, 0.85):
            q = a + (b - a) * t
            p.beam(T.straight(q - Y * s * 0.1, q, UP, X), 0.1, 0.04, 0.04, "MI_Metal_Iron", nseed())
    lantern(p, Vector((st[0] + 1.5, 0.5 * (st[2] + st[3]), zq + 2.4)), chain_to=(st[0] + 1.5, 0.5 * (st[2] + st[3]), zmid - SLAB - 0.3))
    # The Tally House and the lane: floors, the deck over them, the walls (the stair's arrival, the windows into the Slip,
    # the doors), the hull's side with its two ports.
    room = B.hull_clip(tally, zl)
    floor(p, room, zl)
    floor(p, lane, zl)
    ceiling(p, B.hull_clip(tally, zmid), zmid)
    ceiling(p, lane, zmid, knees=False)
    win = tl["hall_windows"]
    opens = [r for zid, r in H.open_rects("lower") if zid in ("tally", "lane")]
    openings = [("x", land[0], land[2], land[3], 0.0, zmid - zl - SLAB)]
    openings += [("y", tally[2], a, b, win["z0"], win["z1"]) for a, b in win["spans"]]
    openings += [("x", lane[0], tl["lane_door"]["y0"], tl["lane_door"]["y1"], 0.0, B.DOOR_H)]
    runs = B.rasterise(zl, opens, [], [], [H.R(HUB["hall"])], [], lambda x, y: zmid - zl, [])

    def is_open(x, y):
        return any(H.inside(r, x, y) for r in opens)
    for kind, axis, line, a0, a1, hgt in runs:
        if kind != "wall":
            continue
        mid = 0.5 * (a0 + a1)
        if (line if axis == "x" else mid) > lane[1] - 0.01:
            continue                                                             # the Well's gallery walls go with the Well
        ops = [(o[2], o[3], zl + o[4], zl + o[5]) for o in openings if o[0] == axis and abs(o[1] - line) < 0.01 and o[3] > a0 and o[2] < a1]
        for sd in open_side(is_open, axis, line, a0, a1):
            wall(p, axis, line, a0, a1, zl, zmid - SLAB, sd, ops)
    ports = [(a, b, zl + tl["hull_ports"]["z0"], zl + tl["hull_ports"]["z1"]) for a, b in tl["hull_ports"]["spans"]]
    hull_lining(p, 1.0, tally[0], tally[1], zl, zmid - SLAB, holes=ports)
    # The job board on the forward wall: six slips, the TALLY header, a chalk slate under it.
    jb = tl["job_board"]
    xw = tally[1] - WALL * 0.5
    p.block(xw - 0.06, xw, jb["y0"], jb["y1"], zl + jb["z0"], zl + jb["z1"], "MI_Wood_TeakDark", nseed(), axis="y")
    for yy in (jb["y0"] + 0.04, jb["y1"] - 0.04):
        p.block(xw - 0.1, xw, yy - 0.06, yy + 0.06, zl + jb["z0"] - 0.05, zl + jb["z1"] + 0.05, "MI_Wood_Varnish", nseed(), axis="z")
    chalk_slips(p, xw - 0.06, jb["y0"] + 0.3, jb["y1"] - 0.3, zl + jb["z0"] + 0.9, zl + jb["z1"] - 0.15, 6, -1.0, rng)
    p.block(xw - 0.05, xw - 0.02, jb["y0"] + 1.2, jb["y1"] - 1.2, zl + jb["z0"] + 0.1, zl + jb["z0"] + 0.75, "MI_Glass_Dark", nseed(), axis="y")  # slate
    hy0, hy1, hz0, hz1 = 13.6, 17.4, zl + 3.35, zl + 3.85                        # the TALLY header (job_board note)
    p.block(xw - 0.08, xw, hy0, hy1, hz0, hz1, "MI_Paint_HullDark", nseed(), axis="y")
    sign_on_x(p, "TALLY", xw - 0.09, 0.5 * (hy0 + hy1), 0.5 * (hz0 + hz1), 0.34, -1.0)
    # The pay booth on the hull wall: timber walls, a grilled hatch over a ledge, a door in its forward side, PAY.
    pb = tl["pay_booth"]
    dy0, dy1 = pb["door"]
    booth_h = pb["h"]
    hatch = (51.5, 54.5, zl + 1.0, zl + 2.0)
    Fb = (Vector((0.0, pb["y0"] + 0.08, 0.0)), X, UP, -Y)
    HE.wall_boards(p, Fb, pb["x0"], pb["x1"], zl, zl + booth_h, lambda u, v: not (hatch[0] < u < hatch[1] and hatch[2] < v < hatch[3]),
                   mats=("MI_Wood_Varnish", "MI_Wood_TeakDark"))
    for xx in (pb["x0"] + 0.06, hatch[0] - 0.06, hatch[1] + 0.06, pb["x1"] - 0.06):
        p.block(xx - 0.07, xx + 0.07, pb["y0"] - 0.08, pb["y0"] + 0.04, zl, zl + booth_h, "MI_Wood_TeakDark", nseed(), axis="z")
    for k in range(9):                                                           # the grille: iron bars across the hatch
        xx = hatch[0] + (hatch[1] - hatch[0]) * (k + 0.5) / 9
        p.beam(T.straight((xx, pb["y0"] - 0.02, hatch[2]), (xx, pb["y0"] - 0.02, hatch[3]), X, Y), hatch[3] - hatch[2], 0.025, 0.025, "MI_Metal_Iron", nseed())
    p.block(hatch[0] - 0.1, hatch[1] + 0.1, 17.0, pb["y0"], zl + 0.95, zl + 1.0, "MI_Wood_TeakLight", nseed(), axis="x")          # the ledge
    Fs = (Vector((pb["x0"] - 0.08, 0.0, 0.0)), -Y, UP, -X)
    HE.wall_boards(p, Fs, -pb["y1"], -pb["y0"], zl, zl + booth_h, None, mats=("MI_Wood_Varnish", "MI_Wood_TeakDark"))
    Fd = (Vector((pb["x1"] - 0.08, 0.0, 0.0)), Y, UP, X)
    HE.wall_boards(p, Fd, pb["y0"], pb["y1"], zl, zl + booth_h, lambda u, v: not (dy0 < u < dy1 and v < zl + B.DOOR_H),
                   mats=("MI_Wood_Varnish", "MI_Wood_TeakDark"))
    for k in range(3):                                                           # the door, shut, on strap hinges
        u = dy0 + (dy1 - dy0) * (k + 0.5) / 3
        p.block(pb["x1"] - 0.02, pb["x1"] + 0.04, u - (dy1 - dy0) / 6 + 0.01, u + (dy1 - dy0) / 6 - 0.01, zl + 0.02, zl + B.DOOR_H - 0.02,
                "MI_Wood_TeakDark", nseed(), axis="z")
    for zz in (zl + 0.4, zl + 1.8):
        p.block(pb["x1"] + 0.04, pb["x1"] + 0.06, dy0 + 0.05, dy0 + 0.7, zz - 0.04, zz + 0.04, "MI_Metal_Iron", nseed(), axis="y")
    p.block(pb["x0"] - 0.15, pb["x1"] + 0.15, pb["y0"] - 0.2, pb["y1"] - 0.1, zl + 2.7, zl + 2.82, "MI_Wood_TeakDark", nseed(), axis="x")    # its roof
    p.block(52.3, 53.7, pb["y0"] - 0.09, pb["y0"] - 0.05, zl + 2.15, zl + 2.55, "MI_Paint_HullDark", nseed(), axis="x")
    sign_on_y(p, "PAY", 53.0, pb["y0"] - 0.1, zl + 2.35, 0.26, -1.0)
    # The weighing bench with a big balance; sacks and a crate beside it; the salt cakes by the booth.
    wb = tl["weighing_bench"]
    p.block(wb["x0"], wb["x1"], wb["y0"], wb["y1"], zl + wb["h"] - 0.1, zl + wb["h"], "MI_Wood_TeakDark", nseed(), axis="x")
    for xx in (wb["x0"] + 0.15, wb["x1"] - 0.15):
        p.block(xx - 0.1, xx + 0.1, wb["y0"] + 0.1, wb["y1"] - 0.1, zl, zl + wb["h"] - 0.1, "MI_Wood_TeakDark", nseed(), axis="z")
    bx, by = 0.5 * (wb["x0"] + wb["x1"]), 0.5 * (wb["y0"] + wb["y1"])
    zt = zl + wb["h"]
    p.lathe([(0.0, 0.0), (0.22, 0.0), (0.18, 0.06), (0.06, 0.1), (0.05, 0.9), (0.08, 0.95), (0.0, 1.0)], Vector((bx, by, zt)), UP, "MI_Metal_Brass", sides=10)
    p.beam(T.straight((bx - 0.7, by, zt + 0.95), (bx + 0.7, by, zt + 0.95), Y, UP), 1.4, 0.05, 0.06, "MI_Metal_Brass", nseed())
    for xx in (bx - 0.65, bx + 0.65):
        for dy in (-0.12, 0.12):
            p.beam(T.straight((xx, by, zt + 0.93), (xx, by + dy, zt + 0.35), X, Y), 0.6, 0.01, 0.01, "MI_Metal_Brass", nseed())
        p.lathe([(0.0, 0.0), (0.2, 0.02), (0.22, 0.08), (0.0, 0.06)], Vector((xx, by, zt + 0.3)), UP, "MI_Metal_Brass", sides=12)
    for k in range(3):                                                           # sacks against the hull under the bench's end
        c = Vector((wb["x0"] + 0.5 + 0.8 * k, wb["y0"] - 0.45, zl))
        p.lathe([(0.0, 0.0), (0.26, 0.0), (0.32, 0.2), (0.3, 0.45), (0.2, 0.6), (0.08, 0.68), (0.0, 0.7)], c, UP, "MI_Canvas", sides=10, wear=0.0)
    sc = tl["salt_cakes"]
    zz = zl
    while zz < zl + sc["h"] - 0.05:
        for xx in (sc["x0"] + 0.2, sc["x1"] - 0.2):
            for yy in (sc["y0"] + 0.2, sc["y1"] - 0.2):
                p.block(xx - 0.18, xx + 0.18, yy - 0.18, yy + 0.18, zz, zz + 0.14, "MI_Paint_Cream", nseed(), axis="x", wear=0.3)
        zz += 0.15
    # The long table and its benches; tankards on it.
    tb = tl["table"]
    p.block(tb["x0"], tb["x1"], tb["y0"], tb["y1"], zl + tb["h"] - 0.08, zl + tb["h"], "MI_Wood_Teak", nseed(), axis="x")
    for xx in (tb["x0"] + 0.3, 0.5 * (tb["x0"] + tb["x1"]), tb["x1"] - 0.3):
        p.block(xx - 0.08, xx + 0.08, tb["y0"] + 0.15, tb["y1"] - 0.15, zl, zl + tb["h"] - 0.08, "MI_Wood_TeakDark", nseed(), axis="z")
    p.block(tb["x0"] + 0.3, tb["x1"] - 0.3, 0.5 * (tb["y0"] + tb["y1"]) - 0.05, 0.5 * (tb["y0"] + tb["y1"]) + 0.05, zl + 0.15, zl + 0.25,
            "MI_Wood_TeakDark", nseed(), axis="x")
    for by0, by1 in ((14.2, 14.6), (11.8, 12.2)):
        p.block(tb["x0"], tb["x1"], by0, by1, zl + 0.39, zl + 0.45, "MI_Wood_Teak", nseed(), axis="x")
        for xx in (tb["x0"] + 0.25, 0.5 * (tb["x0"] + tb["x1"]), tb["x1"] - 0.25):
            p.block(xx - 0.06, xx + 0.06, by0 + 0.05, by1 - 0.05, zl, zl + 0.39, "MI_Wood_TeakDark", nseed(), axis="z")
    for xx in (50.2, 52.8, 55.9):
        p.lathe([(0.0, 0.0), (0.07, 0.0), (0.075, 0.14), (0.07, 0.16), (0.0, 0.16)], Vector((xx, 13.1 + rng.uniform(-0.2, 0.2), zl + tb["h"])), UP,
                "MI_Metal_Brass" if xx > 55 else "MI_Wood_Varnish", sides=10)
    # Lanterns from the beams.
    for x, y, zz in tl["lanterns"]["at"]:
        lantern(p, Vector((x, y, zl + zz - 0.9)), chain_to=(x, y, zmid - SLAB - 0.3))
    export(p)


# --- the Well and the Keelgate ----------------------------------------------------------------------------------------------
def well_level(p, level):
    """A gallery level of the Well (or the lower deck's part of it): its floor round the shaft, its walls with their doors
    and gates, its rails round the shaft and the flights' holes."""
    z = Z[level]
    sh, ring_, ch = H.R(HUB["shaft"]), H.R(HUB["ring"]), H.R(HUB["chamber"])
    hoist = H.R(HUB["hoist"])
    holes = B.flight_holes(level)
    nxt = {"lower": "middle", "middle": "upper", "upper": "main"}[level]
    if level == "lower":
        opens = [r for zid, r in H.open_rects("lower") if zid == "well_l"]
        floor(p, ch, z, holes=[sh] + [h for h, _ in holes])
    else:
        opens = [r for zid, r in H.open_rects(level)]
        floor(p, ring_, z, holes=[sh] + [h for h, _ in holes])
    gaps = [("x", sh[0], hoist[2], hoist[3])] if level == "lower" else []
    gaps += [(ax, ln, h[2] if ax == "x" else h[0], h[3] if ax == "x" else h[1]) for h, (ax, ln) in holes]
    openings = []
    for gm in H.GATE_MARKS:
        if gm["level"] == level and gm["g"] == "G2":
            openings.append(("x", gm["x"], gm["y"] - 1.2, gm["y"] + 1.2, 0.0, 2.6))
    runs = B.rasterise(z, opens, [h for h, _ in holes], [sh], [H.R(HUB["hall"])] if level == "lower" else [], [],
                       lambda x, y: Z[nxt] - z, gaps)

    def is_open(x, y):
        return any(H.inside(r, x, y) for r in opens) and not any(H.inside(h, x, y) for h, _ in holes) and not H.inside(sh, x, y)
    lane = H.R(HUB["lane"])
    for kind, axis, line, a0, a1, hgt in runs:
        mid = 0.5 * (a0 + a1)
        if (line if axis == "x" else mid) < lane[1] - 0.01:
            continue                                                             # the lane's walls go with the Tally House
        if kind == "rail":
            rail(p, axis, line, a0, a1, z)
            continue
        ops = [(o[2], o[3], z + o[4], z + o[5]) for o in openings if o[0] == axis and abs(o[1] - line) < 0.01 and o[3] > a0 and o[2] < a1]
        for sd in open_side(is_open, axis, line, a0, a1):
            wall(p, axis, line, a0, a1, z, Z[nxt] - SLAB, sd, ops)
    # The deck over the gallery: its beams (the boards are the floor above; over the top gallery the main deck's).
    over = ring_ if level != "lower" else ch
    for r in B.subtract(over, [sh]):
        ceiling(p, r, Z[nxt], boards=False, knees=False)
    # Lanterns on the gallery's corners (well.lanterns note).
    for x in (ring_[0] + 0.6, ring_[1] - 0.6):
        for y in (ring_[2] + 0.6, ring_[3] - 0.6):
            bracket_lantern(p, Vector((x - (0.3 if x > 84 else -0.3), y - (0.3 if y > 0 else -0.3), z)),
                            ((-1.0 if x > 84 else 1.0), 0.0, 0.0), 3.4, drop=0.2)


def build_well():
    p = T.Part("SM_Hub_Well")
    for level in ("lower", "middle", "upper"):
        well_level(p, level)
    for f in H.flights():
        stairs(p, f["x0"], f["x1"], f["y0"], f["y1"], f["up"], Z[f["from"]], Z[f["to"]], rails=True, soffit=True)
    gantry_and_hoist(p)
    export(p)


def gantry_and_hoist(p):
    """The hoist (well.hoist and its notes): the cage at its quay stop between two guide posts, its ropes up to the
    gantry's sheaves, the counterweight up there (the cage is down) over its railed bay; the gantry an A-frame over the
    opening on the main deck with guys to the deck, its beam out over the hoist, the brake drum at its head."""
    zq, zm = Z["quay"], Z["main"]
    hs = H.R(HUB["hoist"])
    top = zm + 7.5
    apex = Vector((84.0, 0.0, top))
    for fx in (76.5, 91.5):
        a = Vector((fx, 0.0, zm))
        p.beam(T.straight(a, apex, Y, (apex - a).normalized().cross(Y)), (apex - a).length, 0.45, 0.45, DARK, nseed(), seg=1.0, wear=1.0)
        p.block(fx - 0.5, fx + 0.5, -0.5, 0.5, zm, zm + 0.25, "MI_Metal_Iron", nseed(), axis="x")
    for s in (-1.0, 1.0):                                                       # guys to ring bolts on the deck
        b = Vector((84.0, s * 9.0, zm))
        p.round(T.straight(apex, b, X, Y), (b - apex).length, 0.05, "MI_Hub_RopeTar", nseed(), sides=6, seg=20.0, wear=0.0)
        p.beam(T.arc(b + UP * 0.08, X, UP, 0.12, 0.0, 2 * math.pi), 0.8, 0.04, 0.04, "MI_Metal_Iron", nseed(), seg=0.1)
    hx, hy = 0.5 * (hs[0] + hs[1]), 0.5 * (hs[2] + hs[3])
    end = Vector((hx, hy, top))
    p.beam(T.straight(apex + UP * 0.2, end + UP * 0.2, UP, (end - apex).normalized().cross(UP)), (end - apex).length, 0.38, 0.38, DARK, nseed(), wear=1.0)
    cw = H.prop("well", "counterweight")
    cwx, cwy = 0.5 * (cw["x0"] + cw["x1"]), 0.5 * (cw["y0"] + cw["y1"])
    for c in (end, Vector((cwx, cwy, top))):
        p.lathe([(0.0, -0.12), (0.45, -0.12), (0.5, -0.04), (0.42, 0.0), (0.5, 0.04), (0.45, 0.12), (0.0, 0.12)], c - UP * 0.25, X, "MI_Wood_Varnish", sides=14)
    p.lathe([(0.0, -0.4), (0.35, -0.4), (0.38, 0.4), (0.0, 0.4)], apex - UP * 0.6, Y, "MI_Metal_Iron", sides=14)            # the brake drum
    # The cage at the quay stop: four posts, a floor, rails to the shoulder, a roof frame; its aft side open (the door).
    z0 = zq
    cz = z0 + 2.4
    for cx in (hs[0] + 0.06, hs[1] - 0.06):
        for cy in (hs[2] + 0.06, hs[3] - 0.06):
            p.beam(T.straight((cx, cy, z0), (cx, cy, cz), X, Y), 2.4, 0.12, 0.12, "MI_Wood_TeakDark", nseed())
    floor(p, (hs[0] + 0.02, hs[1] - 0.02, hs[2] + 0.02, hs[3] - 0.02), z0 + 0.12, width=0.22, thick=0.06)
    for zz in (z0 + 1.05, cz - 0.06):
        for (a, b) in (((hs[1] - 0.06, hs[2] + 0.06), (hs[1] - 0.06, hs[3] - 0.06)), ((hs[0] + 0.06, hs[2] + 0.06), (hs[1] - 0.06, hs[2] + 0.06)),
                       ((hs[0] + 0.06, hs[3] - 0.06), (hs[1] - 0.06, hs[3] - 0.06))):
            pa, pb = Vector((a[0], a[1], zz)), Vector((b[0], b[1], zz))
            p.beam(T.straight(pa, pb, UP, (pb - pa).normalized().cross(UP)), (pb - pa).length, 0.1, 0.08, "MI_Wood_Varnish", nseed())
    pa, pb = Vector((hs[0] + 0.06, hs[2] + 0.06, cz - 0.06)), Vector((hs[0] + 0.06, hs[3] - 0.06, cz - 0.06))
    p.beam(T.straight(pa, pb, UP, X), (pb - pa).length, 0.1, 0.08, "MI_Wood_Varnish", nseed())
    for zz in (z0 + 0.5, z0 + 0.8):                                              # lattice under the rails
        for (a, b) in (((hs[1] - 0.06, hs[2] + 0.06), (hs[1] - 0.06, hs[3] - 0.06)),):
            p.beam(T.straight((a[0], a[1], zz), (b[0], b[1], zz), UP, X), hs[3] - hs[2] - 0.12, 0.05, 0.04, "MI_Wood_TeakDark", nseed())
    for k in range(4):                                                            # the ropes up to the gantry from a yoke
        cx = hx + (-0.4 if k < 2 else 0.4)
        cy = hy + (-0.4 if k % 2 else 0.4)
        p.round(T.straight((cx, cy, cz), (hx, hy, cz + 0.7), X, Y), 0.9, 0.025, "MI_Rope_Hemp", nseed(), sides=5, seg=2.0, wear=0.0)
    p.block(hx - 0.3, hx + 0.3, hy - 0.08, hy + 0.08, cz + 0.62, cz + 0.8, "MI_Metal_Iron", nseed(), axis="x")
    p.round(T.straight((hx, hy, cz + 0.8), end - UP * 0.25, X, Y), top - cz - 1.05, 0.05, "MI_Rope_Hemp", nseed(), sides=6, seg=40.0, wear=0.0)
    p.round(T.straight((hx + 0.2, hy, cz + 1.2), (hx + 0.2, hy, top - 0.8), X, Y), top - cz - 2.0, 0.015, "MI_Rope_Hemp", nseed(), sides=4, seg=40.0,
            wear=0.0)                                                             # the brake's hand line
    p.beam(T.straight((hx + 0.3, hy - 0.5, z0 + 0.15), (hx + 0.3, hy - 0.45, z0 + 1.2), X, Y), 1.05, 0.05, 0.05, "MI_Metal_Iron", nseed())  # the lever
    # The guide posts at the cage's aft corners, braced to each gallery's edge.
    for gy in (hs[2], hs[3] - 0.1):
        p.beam(T.straight((hs[0] - 0.08, gy + 0.05, zq), (hs[0] - 0.08, gy + 0.05, top - 1.0), X, Y), top - 1.0 - zq, 0.16, 0.12, "MI_Wood_TeakDark",
               nseed(), seg=1.0)
        for lv in ("lower", "middle", "upper", "main"):
            zz = Z[lv] - 0.4
            a, b = Vector((hs[0] - 0.08, gy + 0.05, zz)), Vector((H.R(HUB["shaft"])[0] - 0.05, gy + 0.05, zz))
            if (a - b).length > 0.05:
                p.beam(T.straight(a, b, UP, Y), (a - b).length, 0.1, 0.08, "MI_Metal_Iron", nseed())
    # The counterweight up under the gantry: stone blocks in an iron frame, its rope over the second sheave; its bay below.
    ztop = top - 1.0
    p.block(cw["x0"], cw["x1"], cw["y0"], cw["y1"], ztop - cw["h"], ztop, STONE, nseed(), axis="z")
    for zz in (ztop - cw["h"] + 0.1, ztop - 0.1):
        p.block(cw["x0"] - 0.04, cw["x1"] + 0.04, cw["y0"] - 0.04, cw["y1"] + 0.04, zz - 0.05, zz + 0.05, "MI_Metal_Iron", nseed(), axis="x")
    p.round(T.straight((cwx, cwy, ztop), (cwx, cwy, top - 0.25), X, Y), top - 0.25 - ztop, 0.04, "MI_Rope_Hemp", nseed(), sides=6, wear=0.0)
    for (ax, ln, a0, a1) in (("y", cw["y0"] - 0.2, cw["x0"] - 0.2, cw["x1"] + 0.3), ("x", cw["x1"] + 0.3, cw["y0"] - 0.2, cw["y1"]),
                             ("x", cw["x0"] - 0.2, cw["y0"] - 0.2, cw["y1"])):
        rail(p, ax, ln, a0, a1, zq)


KEEL_LINE = 0.07              # the gate's glowing lines' width (m)


def build_keelgate():
    p = T.Part("SM_Hub_Keelgate")
    zq = Z["quay"]
    ch, sh = H.R(HUB["chamber"]), H.R(HUB["shaft"])
    kg = H.prop("keelgate", "keelgate")
    kgx, kgy = HUB["keelgate"]["x"], HUB["keelgate"]["y"]
    rng = random.Random(88)
    # The chamber's floor round the gate (stone flags near the gate, oak further out) and its walls.
    floor(p, ch, zq, holes=[(kgx - kg["ring_r"] - 0.3, kgx + kg["ring_r"] + 0.3, kgy - kg["ring_r"] - 0.3, kgy + kg["ring_r"] + 0.3)])
    rr = kg["ring_r"] + 0.3
    n = 28
    for k in range(n):                                                          # the floor round the ring: a stone apron
        a0, a1 = 2 * math.pi * k / n, 2 * math.pi * (k + 1) / n
        pts = []
        for a in (a0, a1):
            pts.append((kgx + math.cos(a) * kg["ring_r"], kgy + math.sin(a) * kg["ring_r"], zq))
        for a in (a1, a0):
            r_ = rr / max(abs(math.cos(a)), abs(math.sin(a)))
            pts.append((kgx + math.cos(a) * r_, kgy + math.sin(a) * r_, zq))
        p.polygon(pts, STONE, tone=0.5, facing=UP)
    hzq = Z["lower"] - zq
    opens = [r for zid, r in H.open_rects("quay") if zid != "stair"]
    runs = B.rasterise(zq, opens, [], [], [], [(0.0, 44.0, -10.0, 10.0)], lambda x, y: hzq, [])

    def is_open(x, y):
        return any(H.inside(r, x, y) for r in opens)
    for kind, axis, line, a0, a1, hgt in runs:
        mid = 0.5 * (a0 + a1)
        x = line if axis == "x" else mid
        if kind != "wall" or x < ch[0] - 0.01:
            continue
        for sd in open_side(is_open, axis, line, a0, a1):
            wall(p, axis, line, a0, a1, zq, Z["lower"] - SLAB, sd)
    # The Keelgate (G5): the ring and the disc of pale seamless stone, the turquoise lines (two circles, seven spokes), seven
    # empty sockets round it (spaces.keelgate.keelgate).
    raised = kg["raised"]
    prof = [(0.0, 0.0), (kg["ring_r"], 0.0), (kg["ring_r"], raised * 0.6 - 0.02), (kg["ring_r"] - 0.04, raised * 0.6), (kg["disc_r"] + 0.05, raised * 0.6),
            (kg["disc_r"] + 0.02, raised * 0.6 + 0.01)]
    p.lathe(prof + [(kg["disc_r"] + 0.02, raised * 0.6 + 0.01), (0.0, raised * 0.6 + 0.01)], Vector((kgx, kgy, zq)), UP, STONE, sides=64, wear=0.0)
    p.lathe([(kg["disc_r"], 0.0), (kg["disc_r"], raised - 0.02), (kg["disc_r"] - 0.03, raised), (0.0, raised)], Vector((kgx, kgy, zq)), UP, STONE,
            sides=64, wear=0.0)
    zt = zq + raised + 0.004
    for r in (1.0, 2.1):
        for k in range(48):
            a0, a1 = 2 * math.pi * k / 48, 2 * math.pi * (k + 1) / 48
            pts = [(kgx + math.cos(a) * rr_, kgy + math.sin(a) * rr_, zt) for a, rr_ in ((a0, r - KEEL_LINE * 0.5), (a1, r - KEEL_LINE * 0.5),
                                                                                       (a1, r + KEEL_LINE * 0.5), (a0, r + KEEL_LINE * 0.5))]
            p.polygon(pts, "MI_AncientGlow", tone=0.5, facing=UP)
    for k in range(7):
        a = 2 * math.pi * k / 7 + 0.2
        d, nrm = Vector((math.cos(a), math.sin(a), 0.0)), Vector((-math.sin(a), math.cos(a), 0.0))
        c0, c1 = Vector((kgx, kgy, zt)) + d * 1.0, Vector((kgx, kgy, zt)) + d * 2.8
        p.polygon([c0 - nrm * KEEL_LINE * 0.5, c1 - nrm * KEEL_LINE * 0.5, c1 + nrm * KEEL_LINE * 0.5, c0 + nrm * KEEL_LINE * 0.5], "MI_AncientGlow",
                  tone=0.5, facing=UP)
        s = Vector((kgx, kgy, zq + raised * 0.6 + 0.012)) + d * 3.5
        p.lathe([(0.0, 0.0), (0.2, 0.0), (0.2, 0.002), (0.0, 0.002)], s, UP, "MI_Hub_Void", sides=16)
        p.lathe([(0.2, -0.0), (0.24, 0.0), (0.24, 0.01), (0.2, 0.01)], s, UP, STONE, sides=16, caps=False)
    # The reader's desk with books, a brass lamp and rubbings; the chair behind it.
    dk = H.prop("keelgate", "desk")
    zt = zq + dk["h"]
    p.block(dk["x0"], dk["x1"], dk["y0"], dk["y1"], zt - 0.07, zt, "MI_Wood_Varnish", nseed(), axis="x")
    for xx in (dk["x0"] + 0.1, dk["x1"] - 0.1):
        p.block(xx - 0.07, xx + 0.07, dk["y0"] + 0.08, dk["y1"] - 0.08, zq, zt - 0.07, "MI_Wood_TeakDark", nseed(), axis="z")
    p.block(dk["x0"] + 0.15, dk["x1"] - 0.15, dk["y0"] + 0.12, dk["y0"] + 0.16, zq + 0.1, zt - 0.1, "MI_Wood_TeakDark", nseed(), axis="x")
    for k in range(5):
        bx = dk["x0"] + 0.25 + 0.18 * k
        h = rng.uniform(0.22, 0.32)
        p.block(bx - 0.07, bx + 0.07, dk["y1"] - 0.4, dk["y1"] - 0.15, zt, zt + h, rng.choice(("MI_Paint_HullDark", "MI_Wood_TeakDark", "MI_Hub_Ochre")),
                nseed(), axis="z", wear=0.6)
    p.block(dk["x0"] + 1.4, dk["x0"] + 2.0, dk["y0"] + 0.3, dk["y0"] + 0.75, zt, zt + 0.05, "MI_Paint_HullDark", nseed(), axis="x")    # an open book
    p.polygon([(dk["x0"] + 1.42, dk["y0"] + 0.32, zt + 0.052), (dk["x0"] + 1.98, dk["y0"] + 0.32, zt + 0.052), (dk["x0"] + 1.98, dk["y0"] + 0.73, zt + 0.052),
               (dk["x0"] + 1.42, dk["y0"] + 0.73, zt + 0.052)], "MI_Paint_CreamAged", facing=UP)
    lamp = Vector((dk["x1"] - 0.4, dk["y1"] - 0.4, zt))
    p.lathe([(0.0, 0.0), (0.12, 0.0), (0.1, 0.03), (0.03, 0.06), (0.025, 0.3), (0.05, 0.32), (0.0, 0.33)], lamp, UP, "MI_Metal_Brass", sides=10)
    p.lathe([(0.0, 0.3), (0.06, 0.3), (0.08, 0.38), (0.06, 0.46), (0.0, 0.47)], lamp, UP, "MI_LanternGlass", sides=10)
    ch_ = dk["chair"]
    p.block(ch_[0], ch_[1], ch_[2], ch_[3], zq + 0.42, zq + 0.48, "MI_Wood_Teak", nseed(), axis="x")
    for xx in (ch_[0] + 0.05, ch_[1] - 0.05):
        for yy in (ch_[2] + 0.05, ch_[3] - 0.05):
            p.block(xx - 0.03, xx + 0.03, yy - 0.03, yy + 0.03, zq, zq + 0.42, "MI_Wood_TeakDark", nseed(), axis="z")
    p.block(ch_[0], ch_[1], ch_[2], ch_[2] + 0.06, zq + 0.48, zq + 0.95, "MI_Wood_Teak", nseed(), axis="x")
    # The shelves of salvaged ancient pieces on the port wall: jars and fragments, about a quarter faintly glowing.
    sv = H.prop("keelgate", "shelves")
    yw = -12.0 + WALL * 0.5
    for xx in [sv["x0"] + (sv["x1"] - sv["x0"]) * k / 6 for k in range(7)]:
        p.block(xx - 0.05, xx + 0.05, yw, yw + sv["depth"], zq, zq + sv["z1"], "MI_Wood_TeakDark", nseed(), axis="z")
    for k in range(5):
        zz = zq + 0.15 + (sv["z1"] - 0.3) * k / 4
        p.block(sv["x0"], sv["x1"], yw, yw + sv["depth"], zz - 0.04, zz, "MI_Wood_Teak", nseed(), axis="x")
        if k == 4:
            continue
        x = sv["x0"] + 0.25
        while x < sv["x1"] - 0.2:
            r = rng.uniform(0.07, 0.14)
            h = rng.uniform(0.18, 0.4)
            glow = rng.random() < 0.25
            prof = [(0.0, 0.0), (r * 0.8, 0.0), (r, h * 0.3), (r * 0.9, h * 0.8), (r * 0.5, h * 0.9), (r * 0.55, h), (0.0, h)]
            p.lathe(prof, Vector((x, yw + rng.uniform(0.18, 0.42), zz)), UP, "MI_Hub_AncientGlowLow" if glow else rng.choice(("MI_Glass_Dark", STONE, "MI_Hub_HullTarDark")),
                    sides=8)
            x += r * 2 + rng.uniform(0.08, 0.3)
    # The rubbings on the forward wall: paper with the gate's lines rubbed in charcoal.
    rb = H.prop("keelgate", "rubbings")
    xw = 96.0 - WALL * 0.5
    y = rb["y0"]
    while y < rb["y1"] - 0.3:
        w = rng.uniform(0.7, 1.0)
        z0 = zq + rb["z0"] + rng.uniform(0.0, 0.15)
        z1 = zq + rb["z1"] - rng.uniform(0.0, 0.15)
        p.polygon([(xw - 0.01, y, z0), (xw - 0.01, y + w, z0), (xw - 0.01, y + w, z1), (xw - 0.01, y, z1)], "MI_Paint_CreamAged", tone=rng.random(), facing=-X)
        c = Vector((xw - 0.015, y + w * 0.5, 0.5 * (z0 + z1)))
        for r in (0.15, 0.3):
            for k in range(16):
                a0, a1 = 2 * math.pi * k / 16, 2 * math.pi * (k + 1) / 16
                q0 = c + Vector((0.0, math.cos(a0) * r, math.sin(a0) * r))
                q1 = c + Vector((0.0, math.cos(a1) * r, math.sin(a1) * r))
                d = (q1 - q0).normalized()
                p.polygon([q0 - d.cross(X) * 0.008, q1 - d.cross(X) * 0.008, q1 + d.cross(X) * 0.008, q0 + d.cross(X) * 0.008], "MI_Hub_HullTarDark",
                          tone=0.2, facing=-X)
        p.lathe([(0.0, 0.0), (0.02, 0.0), (0.02, 0.015), (0.0, 0.02)], Vector((xw - 0.012, y + w * 0.5, z1 - 0.05)), -X, "MI_Metal_Brass", sides=6)
        y += w + rng.uniform(0.1, 0.25)
    for x, y, zz in H.prop("keelgate", "lanterns")["at"]:
        lantern(p, Vector((x, y, zq + zz - 0.9)), chain_to=(x, y, Z["lower"] - SLAB - 0.3))
    export(p)


# --- the main deck -----------------------------------------------------------------------------------------------------------
def small_boat(p, x0, x1, yc, beam, zk, depth, cover=True):
    """A ship's boat on chocks: planked, oxblood with a cream sheer strake, a canvas cover laced over her."""
    db = {"x0": x0, "x1": x1, "y": yc, "beam": beam, "keel_z": zk - Z["quay"]}
    cutter(p, db, depth=depth, cradles=False)
    if cover:
        L = x1 - x0
        for k in range(12):
            t0, t1 = 0.06 + 0.88 * k / 12, 0.06 + 0.88 * (k + 1) / 12
            pts = []
            for t, v in ((t0, -1.0), (t1, -1.0), (t1, 1.0), (t0, 1.0)):
                hb = beam * 0.5 * math.sin(math.pi * min(max(t, 0.0), 1.0)) ** 0.55 * 0.98
                pts.append((x0 + L * t, yc + v * hb, zk + depth + 0.25 + 0.25 * (1.0 - v * v)))
            mid = [((pts[0][0] + pts[1][0]) * 0.5, yc, zk + depth + 0.55), ]
            p.polygon([pts[0], pts[1], (pts[1][0], yc, zk + depth + 0.5)], "MI_Canvas", tone=0.5, facing=UP)
            p.polygon([pts[0], (pts[1][0], yc, zk + depth + 0.5), (pts[0][0], yc, zk + depth + 0.5)], "MI_Canvas", tone=0.5, facing=UP)
            p.polygon([(pts[0][0], yc, zk + depth + 0.5), (pts[1][0], yc, zk + depth + 0.5), pts[2], pts[3]], "MI_Canvas", tone=0.5, facing=UP)
    for t in (0.25, 0.75):                                                        # chocks
        x = x0 + (x1 - x0) * t
        p.block(x - 0.2, x + 0.2, yc - beam * 0.4, yc + beam * 0.4, Z["main"], zk - 0.1, "MI_Wood_TeakDark", nseed(), axis="y")


def capstan_big(p, c, r):
    """The main capstan (main_deck.capstan) that heaves the bow cables: a waisted drum with whelps, a drumhead with bars."""
    p.lathe([(0.0, 0.0), (r + 0.15, 0.0), (r + 0.1, 0.12), (r * 0.78, 0.4), (r * 0.72, 0.75), (r * 0.8, 0.95), (r * 0.95, 1.0), (r, 1.05),
             (r, 1.35), (r * 0.85, 1.42), (r * 0.4, 1.48), (0.0, 1.5)], c, UP, "MI_Wood_Varnish", sides=18, wear=1.0)
    for k in range(10):
        a = 2 * math.pi * (k + 0.5) / 10
        d = Vector((math.cos(a), math.sin(a), 0.0))
        f = T.polyline([c + d * r * 0.85 + UP * 0.2, c + d * r * 0.76 + UP * 0.6, c + d * r * 0.86 + UP * 0.98], lambda q, d=d: d.cross(UP), lambda q, d=d: d)
        p.beam(f, T.length_of(f), 0.14, 0.12, "MI_Wood_TeakDark", nseed(), seg=0.3)
    for k in range(10):
        a = 2 * math.pi * k / 10
        d = Vector((math.cos(a), math.sin(a), 0.0))
        p.beam(T.straight(c + d * r * 0.95 + UP * 1.2, c + d * (r + 1.2) + UP * 1.2, UP, d.cross(UP)), 1.2, 0.12, 0.12, "MI_Wood_TeakLight", nseed(), wear=1.0)
    p.lathe([(r + 0.25, 0.0), (r + 0.4, 0.0), (r + 0.4, 0.12), (r + 0.25, 0.12)], c, UP, "MI_Metal_Iron", sides=24, caps=False)   # the pawl ring


def fife_rail(p, mx, my, zb, size=4.0, h=0.9):
    """A fife rail round a mast (main_deck.masts note): turned posts, a rail at waist height with belaying pins, coils."""
    hs = size * 0.5
    for sx in (-1.0, 1.0):
        for sy in (-1.0, 1.0):
            c = Vector((mx + sx * (hs - 0.08), my + sy * (hs - 0.08), zb))
            p.lathe([(0.1, 0.0), (0.1, 0.1), (0.07, 0.15), (0.1, h * 0.5), (0.07, h * 0.85), (0.09, h)], c, UP, "MI_Wood_Varnish", sides=8)
    for a, b in (((mx - hs, my - hs + 0.08), (mx + hs, my - hs + 0.08)), ((mx - hs, my + hs - 0.08), (mx + hs, my + hs - 0.08)),
                 ((mx - hs + 0.08, my - hs), (mx - hs + 0.08, my + hs)), ((mx + hs - 0.08, my - hs), (mx + hs - 0.08, my + hs))):
        pa, pb = Vector((a[0], a[1], zb + h)), Vector((b[0], b[1], zb + h))
        d = (pb - pa).normalized()
        p.beam(T.straight(pa, pb, UP.cross(d), UP), size, 0.2, 0.14, "MI_Wood_Varnish", nseed(), wear=1.0)
        n = 6
        for k in range(1, n):
            q = pa + (pb - pa) * (k / n)
            p.lathe([(0.03, -0.2), (0.035, 0.0), (0.05, 0.05), (0.045, 0.14), (0.0, 0.18)], q + UP * 0.07, UP, "MI_Wood_TeakLight", sides=6)
            if k % 2 == 0:
                for j in range(3):                                                 # a coil hung on the pin
                    p.beam(T.arc(q - UP * (0.25 + 0.03 * j), UP.cross(d).normalized(), UP, 0.22 - 0.02 * j, math.pi * 0.1, math.pi * 1.9),
                           1.2, 0.03, 0.03, "MI_Rope_Hemp", nseed(), seg=0.1, wear=0.0)


def build_main():
    p = T.Part("SM_Hub_MainDeck")
    zm = Z["main"]
    bs = HUB["basin"]
    sh = H.R(HUB["shaft"])
    comp = next(h for h, _ in B.flight_holes("main"))
    md = {pr["id"]: pr for pr in H.space("main_deck")["props"]}
    # The deck from the dock head to the forecastle, in strips following the hull's width; round the Well and the companion.
    w = min(H.half_width(xx, zm) for xx in range(int(bs["x1"]), 109)) - 0.2
    floor(p, (bs["x1"], 108.0, -w, w), zm, holes=[sh, comp], lmin=3.0, lmax=7.0)
    x = 108.0
    while x < HUB["fcX"] + 0.6 - 1e-6:
        xb = min(x + 2.0, HUB["fcX"] + 0.6)
        w = H.half_width(xb, zm) - 0.2
        floor(p, (x, xb, -w, w), zm, lmin=3.0, lmax=7.0)
        x = xb
    # The waterways and the bulwarks' inner side: lining on stanchions up to the cap rail.
    for side in (1.0, -1.0):
        pts = [Vector((xx, side * (H.half_width(xx, zm) - 0.2), zm + 0.06)) for xx in [bs["x1"] + 0.3 + k * 2.0 for k in range(46)] if xx < HUB["fcX"] + 0.5]
        f = T.polyline(pts, lambda q: UP, lambda q, s=side: Vector((0.0, -s, 0.0)))
        p.beam(f, T.length_of(f, 80), 0.12, 0.3, "MI_Wood_TeakDark", nseed(), seg=1.0)
        xx = bs["x1"] + 0.6
        while xx < HUB["fcX"]:
            q0 = Vector((xx, side * (H.half_width(xx, zm) - 0.2), zm))
            q1 = Vector((xx, side * (H.half_width(xx, zm + 0.85) - 0.2), zm + 0.86))
            p.beam(T.straight(q0, q1, X, Vector((0.0, -side, 0.0))), (q1 - q0).length, 0.16, 0.12, "MI_Wood_TeakDark", nseed())
            xx += 1.2
        for zr in (zm + 0.12, zm + 0.42, zm + 0.72):
            pts = [Vector((xx, side * (H.half_width(xx, zr) - 0.1), zr)) for xx in [bs["x1"] + 0.3 + k * 2.0 for k in range(46)] if xx < HUB["fcX"] + 0.4]
            f = T.polyline(pts, lambda q: UP, lambda q, s=side: Vector((0.0, -s, 0.0)))
            p.board(f, T.length_of(f, 80), 0.29, 0.06, random.choice(("MI_Wood_Planks", "MI_Wood_TeakDark")), nseed(), seg=1.0, wear=0.8)
    # The dock head rail over the courtyard.
    rail(p, "x", bs["x1"] + 0.1, -bs["y1"], bs["y1"], zm)
    # The Well's opening: the coaming, the rails (open at the hoist's stop).
    hoist = H.R(HUB["hoist"])
    co = H.prop("well", "coaming")
    for (ax, ln, a0, a1) in (("x", sh[0], sh[2], sh[3]), ("x", sh[1], sh[2], sh[3]), ("y", sh[2], sh[0], sh[1]), ("y", sh[3], sh[0], sh[1])):
        if ax == "x":
            p.block(ln - 0.18, ln + 0.18, a0 - 0.2, a1 + 0.2, zm, zm + co["h"], "MI_Wood_TeakDark", nseed(), axis="y")
        else:
            p.block(a0 - 0.2, a1 + 0.2, ln - 0.18, ln + 0.18, zm, zm + co["h"], "MI_Wood_TeakDark", nseed(), axis="x")
        pieces = [(a0, a1)]
        if ax == "x" and abs(ln - sh[0]) < 0.01:
            pieces = [(a0, hoist[2]), (hoist[3], a1)]
        for pa, pb in pieces:
            rail(p, ax, ln, pa, pb, zm + co["h"], height=B.RAIL_H - co["h"])
    # The companion: rails round the hatch where the last flight comes up, a hood over its head.
    for (ax, ln, a0, a1) in (("x", comp[0], comp[2], comp[3]), ("x", comp[1], comp[2], comp[3]), ("y", comp[2], comp[0], comp[1])):
        rail(p, ax, ln, a0, a1, zm)
    hz = zm + 2.5
    for xx in (comp[0], comp[1]):
        for yy in (comp[3] - 2.0, comp[3]):
            p.beam(T.straight((xx, yy, zm + 1.0), (xx, yy, hz), X, Y), hz - zm - 1.0, 0.14, 0.14, "MI_Wood_Varnish", nseed())
    for s in (-1.0, 1.0):
        xa, xb = (comp[0] - 0.2, 0.5 * (comp[0] + comp[1])) if s < 0 else (0.5 * (comp[0] + comp[1]), comp[1] + 0.2)
        ya, yb = comp[3] - 2.2, comp[3] + 0.2
        hi, lo = hz + 0.5, hz
        edge = xa if s < 0 else xb
        p.polygon([(edge, ya, lo), (edge, yb, lo), (0.5 * (comp[0] + comp[1]), yb, hi), (0.5 * (comp[0] + comp[1]), ya, hi)], "MI_Wood_TeakDark",
                  tone=0.4, facing=(s * 0.4, 0.0, 1.0))
        p.polygon([(edge, ya, lo - 0.05), (0.5 * (comp[0] + comp[1]), ya, hi - 0.05), (0.5 * (comp[0] + comp[1]), yb, hi - 0.05), (edge, yb, lo - 0.05)],
                  "MI_Wood_Planks", tone=0.4, facing=(-s * 0.4, 0.0, -1.0))
    # Fife rails round the masts (the fore's on the forecastle), with the masts' coats at the partners.
    for mx, my in md["masts"]["at"]:
        zb = zm if mx < HUB["fcX"] else Z["fcTop"]
        fife_rail(p, mx, my, zb)
        g = next(m for m in HUB["masts"] if m["x"] == mx)
        r = HUB["rig"]["thick"][0] * 0.5 * (0.85 + 0.15 * g["h"] / 76.0)
        p.lathe([(r + 0.22, 0.0), (r + 0.2, 0.08), (r + 0.1, 0.22), (r + 0.03, 0.4), (r + 0.015, 0.45)], Vector((mx, my, zb)), UP, "MI_Wood_Tarred",
                sides=16, caps=False, wear=0.0)                                    # the mast's coat: tarred canvas
        for k in range(12):                                                         # wedges round the mast at the partners
            a = 2 * math.pi * k / 12
            d = Vector((math.cos(a), math.sin(a), 0.0))
            p.beam(T.straight(Vector((mx, my, zb)) + d * (r + 0.05), Vector((mx, my, zb)) + d * (r + 0.05) + UP * 0.12, d.cross(UP), d), 0.12, 0.2, 0.08,
                   "MI_Wood_TeakDark", nseed())
    # The cargo hatches (gratings, padlocked), the ship's boats on chocks, the capstan, the galley's chimney.
    for x0, x1, y0, y1 in md["cargo_hatches"]["boxes"]:
        for (a, b) in (((x0, y0), (x1, y0)), ((x0, y1), (x1, y1))):
            p.block(a[0], b[0], a[1] - 0.12, a[1] + 0.12, zm, zm + 0.3, "MI_Wood_TeakDark", nseed(), axis="x")
        for xx in (x0, x1):
            p.block(xx - 0.12, xx + 0.12, y0, y1, zm, zm + 0.3, "MI_Wood_TeakDark", nseed(), axis="y")
        xx = x0 + 0.25
        while xx < x1 - 0.2:
            p.block(xx - 0.05, xx + 0.05, y0 + 0.1, y1 - 0.1, zm + 0.22, zm + 0.3, "MI_Wood_Teak", nseed(), axis="y")
            xx += 0.25
        yy = y0 + 0.25
        while yy < y1 - 0.2:
            p.block(x0 + 0.1, x1 - 0.1, yy - 0.04, yy + 0.04, zm + 0.18, zm + 0.24, "MI_Wood_Teak", nseed(), axis="x")
            yy += 0.25
        c = Vector((x1 + 0.1, 0.5 * (y0 + y1), zm + 0.2))
        p.block(c.x - 0.04, c.x + 0.04, c.y - 0.1, c.y + 0.1, c.z - 0.12, c.z + 0.06, "MI_Metal_Iron", nseed(), axis="y")
        p.beam(T.arc(c + UP * 0.1, Y, UP, 0.07, 0.0, math.pi), 0.25, 0.02, 0.02, "MI_Metal_Iron", nseed(), seg=0.05)
    for x0, x1, y0, y1 in md["ships_boats"]["boxes"]:
        small_boat(p, x0, x1, 0.5 * (y0 + y1), y1 - y0, zm + 0.4, 1.2)
    cp = md["capstan"]
    capstan_big(p, Vector((cp["x"], cp["y"], zm)), cp["r"])
    gc = md["galley_chimney"]
    c = Vector((gc["x"], gc["y"], zm))
    p.block(c.x - 0.55, c.x + 0.55, c.y - 0.55, c.y + 0.55, zm, zm + 0.35, "MI_Wood_TeakDark", nseed(), axis="z")   # the timber collar
    p.block(c.x - 0.48, c.x + 0.48, c.y - 0.48, c.y + 0.48, zm + 0.35, zm + 0.4, "MI_Metal_Iron", nseed(), axis="x")       # its iron plate
    p.lathe([(0.32, 0.5), (0.3, 1.0), (0.28, gc["h"] - 0.6), (0.34, gc["h"] - 0.55), (0.36, gc["h"] - 0.3), (0.2, gc["h"] - 0.15), (0.0, gc["h"])], c, UP,
            "MI_Metal_Iron", sides=12, wear=0.8)
    for zz in (1.4, 2.6, 3.6):
        p.lathe([(0.31, zz - 0.05), (0.34, zz), (0.31, zz + 0.05)], c, UP, "MI_Metal_Iron", sides=12, caps=False)
    for x, y, zz in md["lanterns"]["at"]:
        if abs(x - 72.4) < 0.1:                                                  # at the companion: from its hood's post
            bracket_lantern(p, Vector((comp[0] - 0.08, comp[3], zm)), (-0.6, 0.6, 0.0), md["lanterns"]["hook_z"], drop=0.05)
    export(p)


# --- the Chandlery ------------------------------------------------------------------------------------------------------------
def build_chandlery():
    p = T.Part("SM_Hub_Chandlery")
    zm = Z["main"]
    cd = {pr["id"]: pr for pr in H.space("chandlery")["props"]}
    dh = cd["deckhouse"]
    eav, ridge = dh["h"], dh["ridge"]
    yr = 0.5 * (dh["y0"] + dh["y1"])
    rng = random.Random(41)
    PAINT = ("MI_Paint_Cream", "MI_Paint_Cream", "MI_Paint_CreamAged")
    # The deckhouse's walls: cream-painted boards outside on posts, plain planks inside; the window in the forward wall.
    win = cd["window"]
    door = cd["door"]
    cf = cd["counter_front"]
    for (axis, line, a0, a1, s_out) in (("y", dh["y0"] + 0.15, dh["x0"], dh["x1"], -1.0), ("x", dh["x0"] + 0.15, dh["y0"], dh["y1"], -1.0),
                                        ("x", dh["x1"] - 0.15, dh["y0"], dh["y1"], 1.0)):
        ops = [(win["y0"], win["y1"], zm + win["z0"], zm + win["z1"])] if (axis == "x" and line > 110) else []
        wall(p, axis, line, a0, a1, zm, zm + eav, s_out, ops, mats=PAINT)
        wall(p, axis, line, a0, a1, zm, zm + eav, -s_out, ops, posts=False)
    if True:                                                                      # the window: glass in its frame
        F = (Vector((dh["x1"], 0.5 * (win["y0"] + win["y1"]), zm + 0.5 * (win["z0"] + win["z1"]))), Y, UP, X)
        HE.window_unit(p, F, 0.5 * (win["y1"] - win["y0"]), 0.5 * (win["z1"] - win["z0"]), -0.15, "glass", "MI_Paint_Cream", panes=(2, 1))
    # The front: the counter (cf), its posts, the door, the fascia over the opening with the CHANDLER sign, the awning.
    yf = dh["y1"]
    Fc = (Vector((0.0, cf["y1"], 0.0)), X, UP, Y)
    HE.wall_boards(p, Fc, cf["x0"], cf["x1"], zm, zm + cf["h"], None, mats=("MI_Wood_Varnish", "MI_Wood_TeakDark"))
    p.block(cf["x0"] - 0.05, cf["x1"] + 0.05, cf["y0"] - 0.2, cf["y1"] + 0.06, zm + cf["h"], zm + cf["top"], "MI_Wood_TeakLight", nseed(), axis="x")
    for px0, px1 in ((98.0, 99.5), (110.0, 111.0), (113.0, 114.0)):
        p.block(px0, px1, cf["y0"], cf["y1"], zm, zm + eav, "MI_Paint_Cream", nseed(), axis="z")
    dw = door["x1"] - door["x0"]                                                  # the door, open: swung in against its post
    for k in range(3):
        v = cf["y0"] - 0.1 - dw * (k + 0.5) / 3
        p.block(door["x1"] - 0.08, door["x1"] - 0.02, v - dw / 6 + 0.01, v + dw / 6 - 0.01, zm + 0.02, zm + B.DOOR_H,
                "MI_Paint_Hull" if k != 1 else "MI_Paint_HullFaded", nseed(), axis="z")
    p.block(door["x0"], door["x1"], cf["y0"], cf["y1"], zm + B.DOOR_H + 0.2, zm + eav, "MI_Paint_Cream", nseed(), axis="x")
    aw = cd["awning"]
    (ay0, az0), (ay1, az1) = aw["from"], aw["to"]
    p.block(cf["x0"], cf["x1"], cf["y0"], cf["y1"], zm + az0, zm + eav, "MI_Paint_Cream", nseed(), axis="x")       # the fascia
    sg = cd["sign"]
    p.block(sg["x0"], sg["x1"], yf, yf + 0.08, zm + sg["z0"], zm + sg["z1"], "MI_Paint_HullDark", nseed(), axis="x")
    sign_on_y(p, sg["text"], 0.5 * (sg["x0"] + sg["x1"]), yf + 0.09, zm + 0.5 * (sg["z0"] + sg["z1"]), 0.5, 1.0)
    n = 10                                                                        # striped canvas, oxblood and cream
    for k in range(n):
        xa = aw["x0"] + (aw["x1"] - aw["x0"]) * k / n
        xb = aw["x0"] + (aw["x1"] - aw["x0"]) * (k + 1) / n
        mat = "MI_Hub_CanvasRed" if k % 2 == 0 else "MI_Canvas"
        sag = 0.06
        p.polygon([(xa, ay0, zm + az0), (xb, ay0, zm + az0), (xb, ay1, zm + az1 - sag), (xa, ay1, zm + az1 - sag)], mat, tone=0.5, facing=UP)
        p.polygon([(xa, ay0, zm + az0 - 0.01), (xa, ay1, zm + az1 - sag - 0.01), (xb, ay1, zm + az1 - sag - 0.01), (xb, ay0, zm + az0 - 0.01)], mat, tone=0.5,
                  facing=-UP)
        for x0_ in (xa,):                                                        # the scalloped valance
            p.polygon([(xa, ay1, zm + az1 - sag), (xb, ay1, zm + az1 - sag), (0.5 * (xa + xb), ay1 + 0.01, zm + az1 - sag - 0.25)], mat, tone=0.5,
                      facing=Y)
    for ax_ in (aw["x0"], aw["x1"]):
        p.beam(T.straight((ax_, ay1, zm), (ax_, ay1, zm + az1 + 0.05), X, Y), az1 + 0.05, 0.12, 0.12, "MI_Wood_Varnish", nseed())
        for x, y, zz in H.prop("main_deck", "lanterns")["at"]:
            if abs(x - ax_) < 0.6 and abs(y - ay1) < 0.3:
                bracket_lantern(p, Vector((ax_ + 0.06, ay1, zm)), (0.0, 1.0, 0.0), zz + 0.05, drop=0.05)
    # The pitched roof: rafters, boards, shingle rows of tarred boards, a ridge board, the gables' trim.
    for s in (-1.0, 1.0):
        ye = dh["y0"] - 0.35 if s < 0 else dh["y1"] + 0.35
        ze = zm + eav - 0.12 * 0.35 / ((yr - dh["y0"]) / (ridge - eav))
        top = Vector((0.0, yr, zm + ridge))
        bot = Vector((0.0, ye, zm + eav - 0.35 * (ridge - eav) / abs(yr - dh["y0"])))
        d = (top - bot)
        L = d.length
        d.normalize()
        nrm = X.cross(d) if X.cross(d).z > 0 else d.cross(X)
        k = 0
        while k * 0.35 < L:
            c0 = bot + d * (k * 0.35 + 0.17) + nrm * 0.06
            p.board(T.straight(Vector((dh["x0"] - 0.35, c0.y, c0.z)), Vector((dh["x1"] + 0.35, c0.y, c0.z)), d, nrm), dh["x1"] - dh["x0"] + 0.7, 0.4, 0.05,
                    rng.choice(("MI_Hub_HullTarDark", "MI_Hub_HullTar", "MI_Hub_HullTarFaded")), nseed(), seg=1.0, wear=0.8)
            k += 1
        p.polygon([(dh["x0"] - 0.35, ye, bot.z), (dh["x0"] - 0.35, yr, top.z), (dh["x1"] + 0.35, yr, top.z), (dh["x1"] + 0.35, ye, bot.z)], "MI_Wood_Planks",
                  tone=0.4, facing=-nrm)
        for xr in [dh["x0"] + (dh["x1"] - dh["x0"]) * j / 8 for j in range(9)]:
            xr = min(max(xr, dh["x0"] + 0.1), dh["x1"] - 0.1)
            p.beam(T.straight(Vector((xr, ye, bot.z - 0.1)), Vector((xr, yr, top.z - 0.1)), X, -nrm), L, 0.12, 0.16, "MI_Wood_TeakDark", nseed())
    p.beam(T.straight((dh["x0"] - 0.4, yr, zm + ridge + 0.08), (dh["x1"] + 0.4, yr, zm + ridge + 0.08), Y, UP), dh["x1"] - dh["x0"] + 0.8, 0.3, 0.12,
           "MI_Wood_Varnish", nseed())
    for xg in (dh["x0"], dh["x1"]):                                              # the gables
        s = -1.0 if xg == dh["x0"] else 1.0
        p.polygon([(xg + s * 0.0, dh["y0"], zm + eav), (xg, dh["y1"], zm + eav), (xg, yr, zm + ridge)], "MI_Paint_Cream", tone=0.5, facing=(s, 0.0, 0.0))
        p.polygon([(xg - s * 0.3, dh["y0"], zm + eav), (xg - s * 0.3, yr, zm + ridge), (xg - s * 0.3, dh["y1"], zm + eav)], "MI_Wood_Planks", tone=0.5,
                  facing=(-s, 0.0, 0.0))
    # Inside: the shelves with their stock, the ladder on its rail, the display table, the beams with hanging stock.
    shv = cd["shelves"]
    for (sx0, sx1, sy0, sy1) in shv["runs"]:
        along_x = (sx1 - sx0) > (sy1 - sy0)
        for k in range(6):
            zz = zm + 0.2 + (shv["h"] - 0.3) * k / 5
            p.block(sx0, sx1, sy0, sy1, zz - 0.04, zz, "MI_Wood_Teak", nseed(), axis="x" if along_x else "y")
            if k == 5:
                continue
            a, b = (sx0, sx1) if along_x else (sy0, sy1)
            u = a + 0.15
            while u < b - 0.2:
                w = rng.uniform(0.12, 0.35)
                kind = rng.random()
                c = Vector(((u + w / 2) if along_x else 0.5 * (sx0 + sx1), 0.5 * (sy0 + sy1) if along_x else (u + w / 2), zz))
                if kind < 0.35:                                                  # tins and jars
                    p.lathe([(0.0, 0.0), (w * 0.4, 0.0), (w * 0.42, w * 0.9), (w * 0.3, w * 1.0), (0.0, w * 1.0)], c, UP,
                            rng.choice(("MI_Metal_Brass", "MI_Metal_Iron", "MI_Glass_Dark", "MI_Paint_Hull")), sides=8)
                elif kind < 0.6:                                                 # coils of rope
                    for j in range(3):
                        p.beam(T.arc(c + UP * (0.04 + 0.05 * j), X, Y, w * 0.38, 0.0, 2 * math.pi), w * 2.4, 0.04, 0.04, "MI_Rope_Hemp", nseed(), seg=0.08,
                               wear=0.0)
                else:                                                            # boxes
                    p.block(c.x - w * 0.45, c.x + w * 0.45, c.y - 0.2, c.y + 0.2, zz, zz + rng.uniform(0.15, 0.35),
                            rng.choice(("MI_Wood_Planks", "MI_Wood_TeakLight", "MI_Paint_CreamAged")), nseed(), axis="x")
                u += w + rng.uniform(0.05, 0.15)
        for xx in ((sx0 + 0.03, sx1 - 0.03) if along_x else (0.5 * (sx0 + sx1),)):
            pass
        if along_x:
            for xx in [sx0 + (sx1 - sx0) * j / 5 for j in range(6)]:
                p.block(xx - 0.04, xx + 0.04, sy0, sy1, zm, zm + shv["h"], "MI_Wood_TeakDark", nseed(), axis="z")
            rail_y = sy1 + 0.25
            p.round(T.straight((sx0, rail_y, zm + shv["h"] - 0.2), (sx1, rail_y, zm + shv["h"] - 0.2), Y, UP), sx1 - sx0, 0.03, "MI_Metal_Brass", nseed(),
                    sides=6)
            lx = sx0 + (sx1 - sx0) * 0.62                                         # the ladder on its rail
            for dx in (-0.25, 0.25):
                p.beam(T.straight((lx + dx, sy1 + 1.0, zm), (lx + dx, rail_y, zm + shv["h"] - 0.15), X, Y), 3.5, 0.06, 0.08, "MI_Wood_Varnish", nseed())
            for j in range(1, 11):
                t = j / 11
                q = Vector((lx, sy1 + 1.0 + (rail_y - sy1 - 1.0) * t, zm + (shv["h"] - 0.15) * t))
                p.round(T.straight(q - X * 0.25, q + X * 0.25, Y, UP), 0.5, 0.025, "MI_Wood_Varnish", nseed(), sides=6)
    dt = cd["display_table"]
    p.block(dt["x0"], dt["x1"], dt["y0"], dt["y1"], zm + dt["h"] - 0.06, zm + dt["h"], "MI_Wood_Varnish", nseed(), axis="x")
    for xx in (dt["x0"] + 0.15, dt["x1"] - 0.15):
        for yy in (dt["y0"] + 0.15, dt["y1"] - 0.15):
            p.lathe([(0.06, 0.0), (0.05, 0.1), (0.07, dt["h"] * 0.5), (0.05, dt["h"] - 0.06)], Vector((xx, yy, zm)), UP, "MI_Wood_TeakDark", sides=8)
    tz = zm + dt["h"]
    p.lathe([(0.0, 0.0), (0.16, 0.0), (0.16, 0.08), (0.14, 0.1), (0.0, 0.1)], Vector((104.0, -13.5, tz)), UP, "MI_Metal_Brass", sides=14)        # compass
    p.polygon([(103.88, -13.62, tz + 0.105), (104.12, -13.62, tz + 0.105), (104.12, -13.38, tz + 0.105), (103.88, -13.38, tz + 0.105)], "MI_Paint_CreamAged",
              facing=UP)
    p.round(T.straight((105.3, -13.2, tz + 0.06), (106.4, -13.6, tz + 0.06), Y, UP), 1.17, lambda s: 0.05 + 0.02 * s, "MI_Metal_Brass", nseed(), sides=10)  # spyglass
    lantern(p, Vector((107.4, -13.4, tz)), r=0.13)
    for bx in cd["beams"]["at_x"]:
        zb = zm + cd["beams"]["z"]
        p.beam(T.straight((bx, dh["y0"] + 0.3, zb), (bx, dh["y1"] - 0.3, zb), X, UP), dh["y1"] - dh["y0"] - 0.6, 0.24, 0.26, "MI_Wood_TeakDark", nseed())
        for k, yy in enumerate([dh["y0"] + 1.5 + 2.2 * j for j in range(4)]):
            if k % 3 == 0:
                lantern(p, Vector((bx, yy, zb - 1.2)), chain_to=(bx, yy, zb - 0.12), r=0.15)
            elif k % 3 == 1:
                for j in range(4):
                    p.beam(T.arc(Vector((bx, yy, zb - 0.55 - 0.04 * j)), X, UP, 0.28 - 0.01 * j, -math.pi * 0.4, math.pi * 1.4), 1.4, 0.04, 0.04, "MI_Rope_Hemp",
                           nseed(), seg=0.08, wear=0.0)
            else:
                pts = [Vector((bx + 0.0, yy - 0.6 + 1.2 * t, zb - 0.15 - 0.9 * math.sin(math.pi * t))) for t in (j / 8 for j in range(9))]
                for dz in (0.0, -0.2, -0.4):
                    f = T.polyline([q + UP * dz for q in pts], lambda q: X, lambda q: UP)
                    p.round(f, T.length_of(f), 0.012, "MI_Rope_Hemp", nseed(), sides=4, wear=0.0)
    # Outside the front: the try-it spyglass on its tripod, the binnacle, the barrels and crates.
    tp = cd["tripod_spyglass"]
    c = Vector((tp["x"], tp["y"], zm))
    for k in range(3):
        a = 2 * math.pi * k / 3 + 0.3
        foot = c + Vector((math.cos(a) * 0.45, math.sin(a) * 0.45, 0.0))
        p.beam(T.straight(foot, c + UP * (tp["h"] - 0.15), X, Y), (c + UP * (tp["h"] - 0.15) - foot).length, 0.05, 0.05, "MI_Wood_Varnish", nseed())
    p.round(T.straight(c + UP * tp["h"] + Vector((-0.45, 0.2, -0.03)), c + UP * tp["h"] + Vector((0.45, -0.2, 0.06)), Y, UP), 0.99,
            lambda s: 0.045 + 0.035 * s, "MI_Metal_Brass", nseed(), sides=10)
    bn = cd["binnacle"]
    c = Vector((bn["x"], bn["y"], zm))
    p.lathe([(0.0, 0.0), (0.32, 0.0), (0.3, 0.1), (0.22, 0.2), (0.2, bn["h"] - 0.3), (0.3, bn["h"] - 0.2), (0.32, bn["h"] - 0.05), (0.0, bn["h"] - 0.05)],
            c, UP, "MI_Wood_Varnish", sides=12)
    p.lathe([(0.0, bn["h"] - 0.05), (0.28, bn["h"] - 0.05), (0.3, bn["h"] + 0.1), (0.22, bn["h"] + 0.22), (0.0, bn["h"] + 0.26)], c, UP, "MI_Metal_Brass",
            sides=14)
    bc = cd["barrels_crates"]
    for k, (bx, by) in enumerate(((bc["x0"] + 0.4, bc["y0"] + 0.4), (bc["x0"] + 1.3, bc["y0"] + 0.45))):
        prof = [(0.0, 0.0), (0.3, 0.0), (0.36, 0.25), (0.38, 0.45), (0.36, 0.65), (0.3, 0.9), (0.0, 0.9)]
        p.lathe(prof, Vector((bx, by, zm)), UP, "MI_Wood_Planks", sides=14)
        for zz in (0.15, 0.75):
            p.lathe([(0.34, zz - 0.04), (0.36, zz), (0.34, zz + 0.04)], Vector((bx, by, zm)), UP, "MI_Metal_Iron", sides=14, caps=False)
    p.block(bc["x0"] + 0.1, bc["x1"] - 0.1, bc["y1"] - 0.75, bc["y1"] - 0.05, zm, zm + 0.6, "MI_Wood_Planks", nseed(), axis="x")
    p.block(bc["x0"] + 0.4, bc["x1"] - 0.5, bc["y1"] - 0.65, bc["y1"] - 0.15, zm + 0.6, zm + 1.0, "MI_Wood_TeakLight", nseed(), axis="x")
    # Inside lanterns are the beams' stock; the inside under the roof gets two more from the rafters.
    export(p)


# --- the gates ---------------------------------------------------------------------------------------------------------------
def build_gates():
    """Every gate of gate_marks in art (gates in the data): G1 a barred door, G2 rope and a board, G3 a chained ladder or
    a padlocked hatch, G4 shuttered bays, G6 an iron grille with timber behind it. (G5 is the Keelgate, built with it; the
    cargo rope is G2 on the main deck.)"""
    p = T.Part("SM_Hub_Gates")
    rng = random.Random(66)
    for gm in H.GATE_MARKS:
        z = H.level_z(gm["level"])
        g, x, y = gm["g"], gm["x"], gm["y"]
        if g == "G5":
            continue
        if g == "G2" and gm["level"] == "main":
            w = H.half_width(x, z) - 0.1
            step = H.prop("main_deck", "cargo_rope")["every"]
            k = -math.floor(w / step)
            stn = []
            while k * step <= w:
                stn.append(k * step)
                k += 1
            for yy in stn:
                p.lathe([(0.09, 0.0), (0.09, 0.06), (0.06, 0.1), (0.07, 0.9), (0.09, 0.95), (0.05, 1.05), (0.0, 1.08)], Vector((x, yy, z)), UP, "MI_Wood_Varnish",
                        sides=8)
            for a, b in zip(stn[:-1], stn[1:]):
                pts = [Vector((x, a + (b - a) * t, z + 0.95 - 0.12 * math.sin(math.pi * t))) for t in (j / 8 for j in range(9))]
                f = T.polyline(pts, lambda q: X, lambda q: UP)
                p.round(f, T.length_of(f), 0.03, "MI_Rope_Hemp", nseed(), sides=6, wear=0.0)
            p.block(x - 0.04, x + 0.04, y - 0.6, y + 0.6, z + 0.45, z + 0.85, "MI_Paint_CreamAged", nseed(), axis="y")
            sign_on_x(p, "CARGO", x - 0.045, y, z + 0.65, 0.2, -1.0)
            continue
        found = B.gate_axis(gm)
        wdt, hgt = B.GATE_SIZE[g]
        if found is None and gm["level"] == "main" and abs(y) > 15.0:
            # The shrouds' ratlines start out of reach: a chained plate at the bulwark.
            side = 1.0 if y > 0 else -1.0
            yy = side * (H.half_width(x, z + 1.0) - 0.25)
            p.block(x - 0.75, x + 0.75, yy - 0.04, yy + 0.04, z + 0.2, z + 2.5, "MI_Wood_TeakDark", nseed(), axis="z")
            for zz in (z + 0.9, z + 1.7):
                cl = [Vector((x - 0.8 + 1.6 * t, yy - side * 0.06, zz - 0.15 * math.sin(math.pi * t))) for t in (j / 10 for j in range(11))]
                for q in cl:
                    p.beam(T.arc(q, X if int(q.x * 10) % 2 else UP, Y, 0.04, 0.0, 2 * math.pi), 0.25, 0.012, 0.012, "MI_Metal_Iron", nseed(), seg=0.05)
            continue
        if found is None:
            # A padlocked hatch on the deck.
            for (x0_, x1_, y0_, y1_) in ((x - 0.8, x + 0.8, y - 0.8, y - 0.68), (x - 0.8, x + 0.8, y + 0.68, y + 0.8), (x - 0.8, x - 0.68, y - 0.8, y + 0.8),
                                         (x + 0.68, x + 0.8, y - 0.8, y + 0.8)):
                p.block(x0_, x1_, y0_, y1_, z, z + 0.3, "MI_Wood_TeakDark", nseed(), axis="x")
            floor(p, (x - 0.68, x + 0.68, y - 0.68, y + 0.68), z + 0.3, width=0.22, thick=0.06)
            p.block(x + 0.62, x + 0.7, y - 0.1, y + 0.1, z + 0.12, z + 0.3, "MI_Metal_Iron", nseed(), axis="y")
            continue
        ax, ln, _, a0, a1 = found
        along = y if ax == "x" else x
        if a1 - a0 >= wdt:
            along = min(max(along, a0 + wdt / 2), a1 - wdt / 2)
        z0 = z + (1.0 if g == "G4" else 0.0)
        # Which face is the open one: the gate is seen from there (the blockout's block stands through the wall).
        rects = [r for _, r in H.open_rects(gm["level"])]
        s = 1.0
        for cand in (1.0, -1.0):
            px, py = (ln + cand * 0.6, along) if ax == "x" else (along, ln + cand * 0.6)
            if any(H.inside(r, px, py) for r in rects):
                s = cand
                break
        n = (X if ax == "x" else Y) * s
        t = Y if ax == "x" else X
        face = ln + s * (WALL * 0.5)
        if gm["level"] == "main" and ax == "x":                                   # on the castles' fronts (build_hub_ends)
            if abs(ln - HUB["basin"]["x1"]) < 1.0:
                face = HE.FRONT_X + 0.12
            elif abs(ln - HUB["fcX"]) < 1.0:
                face = HUB["fcX"] + 0.6 - 0.12
        org = Vector((face, along, z0)) if ax == "x" else Vector((along, face, z0))
        F = (org, t, UP, n)
        if g == "G1":                                                            # a barred door: planks, a frame, two bars
            for k in range(4):
                u = -wdt / 2 + wdt * (k + 0.5) / 4
                HH.lbeam(p, F, u, 0.02, u, hgt, wdt / 4 - 0.02, 0.08, 0.0, rng.choice(("MI_Wood_TeakDark", "MI_Hub_HullTar")), wear=1.0)
            for u in (-wdt / 2 - 0.12, wdt / 2 + 0.12):
                HH.lbeam(p, F, u, 0.0, u, hgt + 0.15, 0.22, 0.14, 0.0, "MI_Wood_TeakDark", wear=1.0)
            HH.lbeam(p, F, -wdt / 2 - 0.25, hgt + 0.12, wdt / 2 + 0.25, hgt + 0.12, 0.24, 0.16, 0.0, "MI_Wood_TeakDark", wear=1.0)
            for v in (hgt * 0.35, hgt * 0.7):
                HH.lbeam(p, F, -wdt / 2 - 0.3, v, wdt / 2 + 0.3, v, 0.16, 0.12, 0.09, "MI_Wood_Varnish", wear=1.0)
                for u in (-wdt / 2 - 0.2, wdt / 2 + 0.2):
                    HH.lbeam(p, F, u, v - 0.12, u, v + 0.12, 0.08, 0.14, 0.08, "MI_Metal_Iron", wear=0.6)
        elif g == "G2":                                                          # a roped doorway with a board
            for u in (-wdt / 2, wdt / 2):
                HH.lbeam(p, F, u, 0.0, u, 1.05, 0.12, 0.12, 0.05, "MI_Wood_Varnish")
            a, b = HH.lpt(F, -wdt / 2, 0.95, 0.1), HH.lpt(F, wdt / 2, 0.95, 0.1)
            pts = [a + (b - a) * tt - UP * (0.12 * math.sin(math.pi * tt)) for tt in (j / 8 for j in range(9))]
            f = T.polyline(pts, lambda q: UP, lambda q, n=n: n)
            p.round(f, T.length_of(f), 0.03, "MI_Rope_Hemp", nseed(), sides=6, wear=0.0)
            HH.lbeam(p, F, -0.5, 0.65, 0.5, 0.65, 0.35, 0.03, 0.14, "MI_Paint_CreamAged", wear=0.6)
        elif g == "G3":                                                          # a ladder up, chained shut
            for u in (-wdt / 2, wdt / 2):
                HH.lbeam(p, F, u, 0.0, u, hgt, 0.08, 0.1, 0.1, "MI_Wood_Varnish")
            v = 0.3
            while v < hgt:
                HH.lbeam(p, F, -wdt / 2, v, wdt / 2, v, 0.06, 0.06, 0.12, "MI_Wood_Varnish")
                v += 0.3
            for k in range(14):
                tt = k / 13
                q = HH.lpt(F, -wdt / 2 + wdt * tt, 1.2 + 0.25 * math.sin(math.pi * tt) * 0 + (0.4 * tt), 0.2)
                p.beam(T.arc(q, t if k % 2 else UP, n, 0.035, 0.0, 2 * math.pi), 0.22, 0.012, 0.012, "MI_Metal_Iron", nseed(), seg=0.05)
            p.block(*(lambda q: (q.x - 0.06, q.x + 0.06, q.y - 0.06, q.y + 0.06, q.z - 0.08, q.z + 0.08))(HH.lpt(F, wdt / 2, 1.6, 0.22)), "MI_Metal_Iron", nseed(),
                    axis="z")
        elif g == "G4":                                                          # a shuttered bay in the arcade's wall
            for sgn in (-1.0, 1.0):
                for k in range(3):
                    u = sgn * (wdt / 2 * (k + 0.5) / 3)
                    HH.lbeam(p, F, u, 0.03, u, hgt - 0.03, wdt / 6 - 0.015, 0.05, 0.0, rng.choice(("MI_Paint_Hull", "MI_Paint_HullFaded", "MI_Wood_TeakDark")),
                             wear=1.0)
            HH.lbeam(p, F, -wdt / 2 - 0.2, -0.08, wdt / 2 + 0.2, -0.08, 0.16, 0.16, 0.0, "MI_Wood_TeakDark")
            HH.lbeam(p, F, -wdt / 2 - 0.2, hgt + 0.08, wdt / 2 + 0.2, hgt + 0.08, 0.16, 0.16, 0.0, "MI_Wood_TeakDark")
            HH.lbeam(p, F, -wdt / 2, hgt * 0.5, wdt / 2, hgt * 0.5, 0.08, 0.05, 0.05, "MI_Metal_Iron")
        elif g == "G6":                                                          # an iron grille, stacked timber behind it
            for k in range(int(wdt / 0.18) + 1):
                u = -wdt / 2 + k * 0.18
                HH.lbeam(p, F, u, 0.0, u, hgt, 0.035, 0.035, 0.1, "MI_Metal_Iron", wear=0.5)
            for v in (0.1, hgt * 0.5, hgt - 0.05):
                HH.lbeam(p, F, -wdt / 2 - 0.05, v, wdt / 2 + 0.05, v, 0.06, 0.03, 0.12, "MI_Metal_Iron", wear=0.5)
            for k in range(6):                                                    # the timber store behind it
                v = 0.15 + k * 0.22
                HH.lbeam(p, F, -wdt / 2 + rng.uniform(0.0, 0.3), v, wdt / 2 - rng.uniform(0.0, 0.3), v, 0.2, 0.18, -0.45 - rng.uniform(0.0, 0.3),
                         rng.choice(("MI_Wood_TeakLight", "MI_Wood_Planks", "MI_Wood_Teak")), wear=1.0)
    export(p)


BUILDERS = {"dock": build_dock, "slip": build_slip, "tally": build_tally, "well": build_well, "keelgate": build_keelgate,
            "main": build_main, "chandlery": build_chandlery, "gates": build_gates}

if __name__ == "__main__":
    random.seed(62)
    wanted = sb.script_args() or list(BUILDERS)
    for key in wanted:
        sb.reset_scene()
        BUILDERS[key]()
    print("SB_HUBINSIDE_OK", wanted)
