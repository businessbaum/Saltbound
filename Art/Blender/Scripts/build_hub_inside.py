"""The hub ship's designed spaces, inside (D62, D64): what the owner sees in the open spaces besides their floors, walls and
ceilings (those are every floor's own, build_hub_rooms.py, built with the kit below): the harbour's quays and bridges (D70), the
Slip, the dock stair and the Tally House, the Well's flights and hoist, the main deck, the Chandlery, the gates.
Hand-made like the boat (owner: "you need to Look at Everything, every Wall every Corner floor"; D64: "Look in the details and
check if what you build makes sense").

The art lies on the blockout's surfaces (build_hub_blockout.py), which stay in the map as hidden collision: floors' tops at the
deck heights, walls' faces at the collision walls' faces, rails on their lines, the steps on the blockout's steps; nothing stands
where nothing collides. Every number from HubShip_Data.json through hub_layout.py and hub_plan.py; the build's own choices are
named constants below.

The look below decks: deck planks fore and aft over caulked seams; walls of planks in rows on frame posts, with a skirting and a
top plate; the decks overhead on heavy beams (2.4 m clear under them, D64: lanterns hang from cleats as deep as the beams);
turned balusters on the rails; the quays' timber faces on piles; warm teak and dark tarred timber, iron fittings, rope.

Meshes (/Game/Saltbound/Hub/Art, ship pivot; Nanite; no collision of their own):
  SM_Hub_Dock       the harbour (D70): the water's bed, the quays' faces, piles, fenders, kerbs, rings along the canal and the
                    basins, the hull's inside over the basins round the hidden doors, the lifting bridges' hinge frames, the
                    footbridges, the galleries' pillars, bollards, rung ladders, the docking stations' boards and lanterns, lanterns
  SM_Hub_BridgeLeaf one leaf of a lifting bridge (pivot at its hinge; its own collision: it moves)
  SM_Hub_Slip       the Slip's things: counter, plans, half-models, workbench, timber racks, the work on the quay, the cutter
                    moored at her berth, lanterns, the sign
  SM_Hub_Tally      the dock stair, the Tally House's job board, pay booth, weighing bench, table, lanterns
  SM_Hub_Well       the Well's flights, the gantry and the hoist's guides, the counterweight's bay, lanterns, the ring lantern
  SM_Hub_HoistCage, SM_Hub_HoistWeight   the hoist's moving parts (their own pivots)
  SM_Hub_MainDeck   the bulwarks' inner side, the Well's coaming and rails, the companion's hood, fife rails, hatches, boats,
                    capstan, chimney, lanterns
  SM_Hub_Chandlery  the shop in the forecastle's front (D69): its front's shop windows, sign, awning, door and goods outside;
                    the counter, shelves, ladder, beams with stock, the display table and the spyglass inside
  SM_Hub_Gates      every gate
Run: Tools/Build-BlenderAsset.ps1 build_hub_inside [dock bridge slip tally well hoist main chandlery gates]
"""
import json
import math
import os
import random
import sys

from mathutils import Vector

sys.path.insert(0, os.path.dirname(__file__))
import build_hub_blockout as B  # noqa: E402  (the blockout's construction: wall runs, openings, flights)
import hub_kit as HE  # noqa: E402  (D64: the kit, so the outside's rework does not touch the inside)
import hub_kit as HH  # noqa: E402
import hub_layout as H  # noqa: E402
import hub_plan as PL  # noqa: E402
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


LAMPS = []                          # the lit lanterns' glass of the part being built (ship metres)
LAMPS_JSON = os.path.join(os.path.dirname(__file__), "..", "..", "Generated", "hub_lamps.json")


def export(p):
    HH.export(p)
    write_lamps(p.name)


def write_lamps(name):
    """The lit lanterns by part (Art/Generated/hub_lamps.json): hub_place.py hangs a light in each, where the art hangs it."""
    data = {}
    if os.path.exists(LAMPS_JSON):
        with open(LAMPS_JSON, encoding="utf-8") as fh:
            data = json.load(fh)
    if LAMPS or name in data:
        data[name] = [[round(v, 3) for v in q] for q in LAMPS]
        with open(LAMPS_JSON, "w", encoding="utf-8") as fh:
            json.dump(data, fh, indent=1, sort_keys=True)
    del LAMPS[:]


def lamp(q):
    """A light without a lantern of its own (the ring lantern's: one for its six)."""
    LAMPS.append(tuple(q))


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


def wall(p, axis, line, a0, a1, z0, z1, side, openings=(), mats=WALL_MATS, posts=True, plate=True, skirting=True, seal=0.15):
    """The face of a partition on its `side` (+1: the face towards +x for an x line, +y for a y line) from a0 to a1 along
    it and z0 to z1: boards in rows (their fronts at the collision wall's face), frame posts, a skirting and a top plate;
    frames round the openings [(b0, b1, oz0, oz1)] (absolute z). `seal`: how far a dark backing reaches up behind the top
    (into the deck's slab: up to the floor's boards where there is no boarded ceiling; none where the top is outdoors)."""
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
    # A dark backing behind the top, up into the deck's slab: the ceiling's boards wobble, and where they met the wall's top
    # at the same height hairline slits showed the slab's daylit inside (D63: a pale line along the Well's chamber).
    if seal > 0.0:
        c = face - side * 0.03
        lo, hi = z1 - 0.1, z1 + seal
        if axis == "x":
            pts = [(c, a0, lo), (c, a1, lo), (c, a1, hi), (c, a0, hi)]
        else:
            pts = [(a0, c, lo), (a1, c, lo), (a1, c, hi), (a0, c, hi)]
        p.polygon(pts, "MI_Caulk_Dark", tone=0.2, facing=tuple(n))


SEAL_TO_FLOOR = SLAB - 0.03    # a wall under a deck without a boarded ceiling: its backing reaches into the floor's boards


def seam_backing(p, rect, z, holes=()):
    """Under a floor seen from below (the Well's galleries and the main deck over them): a dark face just inside its boards,
    facing down, so the seams between the boards do not show the daylit floor above (D63)."""
    for r in B.subtract(rect, list(holes)):
        x0, x1, y0, y1 = r
        p.polygon([(x0, y0, z - 0.03), (x1, y0, z - 0.03), (x1, y1, z - 0.03), (x0, y1, z - 0.03)], "MI_Caulk_Dark", tone=0.2, facing=-UP)


def rail(p, axis, line, a0, a1, z, height=B.RAIL_H):
    """A rail on its line: a balustrade of turned balusters between posts, under a cap rail."""
    if axis == "x":
        HE.balustrade(p, (line, a0), (line, a1), z, height=height, posts=1.6, mat="MI_Wood_Varnish", baluster="MI_Wood_TeakLight")
    else:
        HE.balustrade(p, (a0, line), (a1, line), z, height=height, posts=1.6, mat="MI_Wood_Varnish", baluster="MI_Wood_TeakLight")


def lantern(p, c, chain_to=None, r=0.2, s=1.0, lit=True, shadow=None):
    """A hanging lantern: brass cage, glass, a ring on top, its chain or hook up to `chain_to`. c: its bottom; s: its height's
    scale (0.7 under the 3 m decks' beams, D64); its ring's top at c + LANTERN_H(s). lit: it gets a light (hub_lamps.json).
    shadow: whether its parts cast shadows (D82: not when a light hangs inside it; by default only unlit lanterns do)."""
    def h(v):
        return v * s
    brass, glass, iron = ("MI_Metal_Brass", "MI_LanternGlass", "MI_Metal_Iron")
    if lit:
        lamp(Vector(c) + UP * h(0.33))
    if not (shadow if shadow is not None else not lit):
        brass, glass, iron = T.no_shadow(brass), T.no_shadow(glass), T.no_shadow(iron)
    p.lathe([(0.0, 0.0), (r * 0.8, 0.0), (r, h(0.08)), (r * 0.95, h(0.12)), (0.0, h(0.12))], c, UP, brass, sides=8)
    p.lathe([(r * 0.82, h(0.12)), (r * 0.88, h(0.4)), (r * 0.82, h(0.55)), (0.0, h(0.55))], c, UP, glass, sides=8, caps=False)
    for k in range(4):
        a = 2 * math.pi * k / 4 + 0.4
        d = Vector((math.cos(a), math.sin(a), 0.0))
        p.beam(T.straight(c + d * r * 0.9 + UP * h(0.1), c + d * r * 0.9 + UP * h(0.58), d.cross(UP), d), h(0.48), 0.03, 0.03, brass, nseed(),
               seg=1.0)
    p.lathe([(r, h(0.55)), (r * 1.05, h(0.6)), (r * 0.6, h(0.75)), (r * 0.2, h(0.82)), (0.0, h(0.84))], c, UP, brass, sides=8)
    p.beam(T.arc(c + UP * (h(0.84) + 0.06), X, UP, 0.06, 0.0, 2 * math.pi), 0.4, 0.02, 0.02, iron, nseed(), seg=0.1)
    if chain_to is not None:
        a, b = c + UP * LANTERN_H(s), Vector(chain_to)
        L = (b - a).length
        k = 0
        while k * 0.12 < L:
            q = a + (b - a) * (k * 0.12 / L)
            ax = X if k % 2 else Y
            p.beam(T.arc(q + (b - a).normalized() * 0.06, ax, (b - a).normalized(), 0.05, 0.0, 2 * math.pi), 0.33, 0.016, 0.016,
                   iron, nseed(), seg=0.08)
            k += 1


def LANTERN_H(s=1.0):
    """A lantern's height from its bottom to its ring's top."""
    return 0.84 * s + 0.12


def bracket_lantern(p, post_face, out, z_hook, drop=0.55, arm=0.75, s=1.0, lit=True):
    """An iron bracket out from a post's face with a lantern hanging from its end (lanterns' hook_z); its ring `drop` under the
    bracket on a short chain."""
    out = Vector(out).normalized()
    a = Vector(post_face) + UP * z_hook
    b = a + out * arm
    p.beam(T.straight(a, b, UP, out.cross(UP)), arm, 0.06, 0.04, "MI_Metal_Iron", nseed(), seg=1.0)
    s0, s1 = a - UP * 0.45, a + out * (arm * 0.67)
    p.beam(T.straight(s0, s1, out.cross(UP), (s1 - s0).normalized().cross(out.cross(UP))), (s1 - s0).length, 0.04, 0.04, "MI_Metal_Iron", nseed())
    lantern(p, b - UP * (drop + LANTERN_H(s)), chain_to=b, s=s, lit=lit)


def deck_lantern(p, x, y, z_above, r=0.17):
    """A lantern under a 3 m deck (D64): on an iron hook in a cleat under the deck's planking (as deep as its beams, so it reads
    the same between them or on one), its bottom about 1.7 m over the floor: over a head."""
    zb = z_above - SLAB
    p.block(x - 0.08, x + 0.08, y - 0.08, y + 0.08, zb - 0.3, zb + 0.03, "MI_Wood_TeakDark", nseed(), axis="z")
    top = zb - 0.3
    lantern(p, Vector((x, y, top - LANTERN_H(0.7))), chain_to=(x, y, top), r=r, s=0.7)


# --- the harbour (D70) -----------------------------------------------------------------------------------------------------
LEAF_L = 6.0                        # a lifting bridge's leaf: half the basin's width
LEAF_T = 0.42                       # its depth from the deck's top to its girders' undersides


def face_frame(axis, line, side):
    """A quay's face (PL.water_edges): a point on it from (u along it, off out over the water, z), and its normal (out over
    the water)."""
    n = (X if axis == "x" else Y) * -side

    def P(u, off, z):
        return Vector((line, u, z)) + n * off if axis == "x" else Vector((u, line, z)) + n * off
    return P, n


def build_dock():
    """The harbour (D70, spaces.harbour): the water's bed under the canal and the basins, the quays' timber faces on piles with
    fenders, kerbs and rings along every edge of the water, the hull's inside over the basins round the hidden doors, the
    lifting bridges' hinge frames, the footbridges over the canal, the galleries' pillars, bollards, rung ladders, the docking
    stations' boards and lantern posts, lanterns. (The quays' decking, the galleries and the walls are the floors' (build_hub_rooms);
    the door leaves and their capstans the outside's (build_hub_doors); the bridges' leaves move (SM_Hub_BridgeLeaf).)"""
    p = T.Part("SM_Hub_Dock")
    fl, zq = HUB["dock"]["floor"], Z["quay"]
    pr = {q["id"]: q for q in H.space("harbour")["props"]}
    wet = lambda q: min(max((0.7 - q.z) / 1.5, 0.0), 1.0)                                  # noqa: E731
    # The bed: the ship's own bottom inside, dark planks under the water, across the canal and the basins.
    for x0, x1, y0, y1 in H.water_rects():
        x = x0
        while x < x1 - 1e-6:
            xb = min(x + 6.0, x1)
            hw = min(H.half_width(x, fl), H.half_width(xb, fl)) - 0.4
            y = max(y0, -hw)
            while y < min(y1, hw) - 0.05:
                yb = min(y + 0.4, y1, hw)
                p.polygon([(x, y, fl), (xb, y, fl), (xb, yb, fl), (x, yb, fl)], "MI_Wood_Piling", tone=random.random(), facing=UP)
                y = yb
            x = xb
    # The quays' faces along every edge of the water: vertical planking from the bed up under the deck, piles standing proud
    # every 1.5 m (and at the corners), two fenders, an iron-shod kerb, mooring rings every 5 m.
    for axis, line, a0, a1, side in PL.water_edges():
        P, n = face_frame(axis, line, side)
        along = Y if axis == "x" else X
        origin = Vector((line, 0.0, 0.5 * (fl + zq - 0.25))) if axis == "x" else Vector((0.0, line, 0.5 * (fl + zq - 0.25)))
        HE.board_runs(p, (origin, along, UP, n), [(a0, a1)], -(zq - 0.25 - fl) * 0.5, (zq - 0.25 - fl) * 0.5, 0.0,
                      ("MI_Wood_Piling", "MI_Wood_DockWeathered"), lmin=3.0, lmax=6.0)
        L = a1 - a0
        k = max(int(round(L / 1.5)), 1)
        for i in range(k + 1):
            u = a0 + 0.15 + (L - 0.3) * i / k
            p.beam(T.straight(P(u, 0.14, fl), P(u, 0.14, zq - 0.3), along, n), zq - 0.3 - fl, 0.3, 0.26, "MI_Wood_Piling", nseed(),
                   seg=0.8, wear=1.0, grime=wet)
        for zw in (-0.4, 0.9):
            p.beam(T.straight(P(a0 + 0.2, 0.32, zw), P(a1 - 0.2, 0.32, zw), UP, n), L - 0.4, 0.26, 0.14, "MI_Wood_Piling",
                   nseed(), seg=1.0, wear=1.0, grime=wet)
        p.beam(T.straight(P(a0, 0.12, zq - 0.1), P(a1, 0.12, zq - 0.1), n, UP), L, 0.3, 0.22, DARK, nseed(), seg=1.0, wear=1.0)
        p.beam(T.straight(P(a0, 0.0, zq - 0.06), P(a1, 0.0, zq - 0.06), UP, n), L, 0.12, 0.02, "MI_Metal_Iron", nseed(), seg=2.0)
        u = a0 + 2.5
        while u < a1 - 1.0:
            c = P(u, 0.05, zq - 0.7)
            p.beam(T.arc(c + n * 0.12, along, UP, 0.16, 0.0, 2 * math.pi), 1.0, 0.04, 0.04, "MI_Metal_Iron", nseed(), seg=0.1)
            u += 5.0
    # The hull's inside over the basins, up to the gun deck, round the hidden doors (their jambs, head, sill, hinge posts and
    # leaves are the doors' own: build_hub_doors.py).
    zc = Z["gun"] - SLAB
    dr = HUB["doors"]
    for (bx0, bx1, _, _), (dx0, dx1) in zip(H.basins(), H.door_stations()):
        for side in (1.0, -1.0):
            hull_lining(p, side, bx0, bx1, -0.6, zc, holes=[(dx0, dx1, dr["z0"], dr["z1"])])
    # The lifting bridges' hinge frames at the basins' edges: two oak posts either side of each leaf's hinge, an iron axle
    # housing across, a stop the raised leaf leans back on.
    for bx0, bx1, by0, by1 in H.lift_bridges():
        for xh, s in ((bx0, 1.0), (bx1, -1.0)):
            for yy in (by0 - 0.25, by1 + 0.25):
                p.beam(T.straight((xh - s * 0.25, yy, zq), (xh - s * 0.25, yy, zq + 1.5), X, Y), 1.5, 0.34, 0.3, DARK, nseed(), wear=1.0)
                p.lathe([(0.0, -0.12), (0.2, -0.12), (0.2, 0.12), (0.0, 0.12)], Vector((xh, yy, zq - 0.2)), Y, "MI_Metal_Iron", sides=12)
            p.round(T.straight((xh, by0 - 0.3, zq - 0.2), (xh, by1 + 0.3, zq - 0.2), X, UP), by1 - by0 + 0.6, 0.09, "MI_Metal_Iron", nseed(), sides=10)
            p.block(xh - s * 0.6, xh - s * 0.1, by0 - 0.1, by1 + 0.1, zq + 1.2, zq + 1.4, DARK, nseed(), axis="y")
    # The footbridges over the canal (where no boat sails): planked across on two stringers, railed both sides.
    for x0, x1, y0, y1 in H.fixed_bridges():
        ya, yb = y0 - 0.3, y1 + 0.3
        for xs in (x0 + 0.2, x1 - 0.2):
            p.beam(T.straight((xs, ya, zq - 0.25), (xs, yb, zq - 0.25), X, UP), yb - ya, 0.24, 0.32, DARK, nseed(), seg=1.0, wear=1.0)
        xx = x0
        rng = random.Random(nseed())
        while xx < x1 - 1e-6:
            xb = min(xx + 0.3, x1)
            p.board(T.straight((0.5 * (xx + xb), ya, zq - 0.04), (0.5 * (xx + xb), yb, zq - 0.04), X, UP), yb - ya, xb - xx - 0.01, 0.08,
                    rng.choice(DOCK), nseed(), seg=1.0, wear=1.0)
            xx = xb
        for xr in (x0 + 0.06, x1 - 0.06):
            rail(p, "x", xr, y0, y1, zq)
    # The pillars under the galleries' edges, from the quays up to the gun deck, iron-banded where they pass a gallery.
    pl = pr["pillars"]
    hs_ = pl["size"] * 0.5
    for px, py in pl["at"]:
        p.beam(T.straight((px, py, zq), (px, py, Z["gun"] - SLAB), X, Y), Z["gun"] - SLAB - zq, pl["size"], pl["size"], "MI_Wood_TeakDark", nseed(),
               seg=1.0, wear=1.0, bend=0.01)
        for lv in ("lower", "middle", "upper"):
            for zz in (Z[lv] - SLAB - 0.25, Z[lv] + 0.25):
                p.block(px - hs_ - 0.02, px + hs_ + 0.02, py - hs_ - 0.02, py + hs_ + 0.02, zz - 0.05, zz + 0.05, "MI_Metal_Iron", nseed(), axis="x")
        p.block(px - hs_ - 0.06, px + hs_ + 0.06, py - hs_ - 0.06, py + hs_ + 0.06, zq, zq + 0.12, "MI_Wood_TeakDark", nseed(), axis="x")
    # Bollards along the quays' edges (shared with the blockout's); the rung ladders up from the water.
    bo = pr["bollards"]
    for bx, by in B.bollard_spots():
        p.lathe([(0.0, 0.0), (bo["r"] + 0.05, 0.0), (bo["r"], 0.08), (bo["r"] * 0.82, 0.3), (bo["r"] * 0.85, bo["h"] - 0.14),
                 (bo["r"] * 1.15, bo["h"] - 0.08), (bo["r"] * 1.12, bo["h"]), (0.0, bo["h"] + 0.04)], Vector((bx, by, zq)), UP, "MI_Metal_Iron", sides=12,
                wear=1.0)
    for lx, ly, yaw in pr["rung_ladders"]["at"]:
        d = Vector((math.cos(math.radians(yaw)), math.sin(math.radians(yaw)), 0.0))        # from the water to the quay
        t = Vector((-d.y, d.x, 0.0))
        e = Vector((lx, ly, 0.0))
        zr = -1.8
        while zr < zq - 0.1:
            for dt in (-0.25, 0.25):
                a = e + t * dt + UP * zr
                p.beam(T.straight(a, a - d * 0.16, UP, t), 0.16, 0.04, 0.04, "MI_Metal_Iron", nseed(), seg=1.0)
            a = e - d * 0.16 + UP * zr
            p.beam(T.straight(a - t * 0.27, a + t * 0.27, UP, -d), 0.54, 0.045, 0.045, "MI_Metal_Iron", nseed(), seg=1.0)
            zr += 0.3
        for dt in (-0.3, 0.3):                                                   # grab handles over the kerb
            p.beam(T.arc(e + d * 0.15 + t * dt + UP * (zq + 0.02), -d, UP, 0.28, 0.0, math.pi), 0.9, 0.05, 0.05, "MI_Metal_Iron", nseed(), seg=0.1)
    # The docking stations (hub.docking): a board on a post at the berth's bow (the aft one is the Slip's berth board) and a
    # lantern post where the gangway lands.
    for d in HUB["docking"]:
        x, y, _ = H.docking_pose(d)
        face = math.copysign(H.canal_half(x), y)
        s = math.copysign(1.0, y)
        berth_board(p, d["x1"] - 1.0, face + s * 0.35, s)                     # (the aft one is the Slip's: spaces.slip.e)
        post_lantern(p, d["x0"] + 2.5, face + s * 0.8)
    # Lanterns: under the decks over the quays, and under the next gallery at the galleries' edges (none over the water: masts).
    for x, y, zz in pr["lanterns"]["at"]:
        over = min((Z[lv] for lv in ("lower", "middle", "upper", "gun") if Z[lv] > zq + zz), default=Z["gun"])
        deck_lantern(p, x, y, over)
    export(p)


def berth_board(p, x, y, s):
    """A painted board on a post at a quay's edge (s: the quay's side of the canal, +1 starboard), read from the quay."""
    zq = Z["quay"]
    c = Vector((x, y, zq))
    p.beam(T.straight(c, c + UP * 1.6, X, Y), 1.6, 0.16, 0.16, DARK, nseed())
    yb = y + s * 0.12
    p.block(x - 0.5, x + 0.5, yb - 0.04, yb + 0.04, zq + 1.0, zq + 1.6, "MI_Paint_Cream", nseed(), axis="x")
    for zz in (zq + 1.18, zq + 1.3, zq + 1.42):
        p.block(x - 0.38, x + 0.3 - random.random() * 0.2, yb + s * 0.05 - 0.01, yb + s * 0.05 + 0.01, zz - 0.025, zz + 0.025, "MI_Paint_HullDark",
                nseed(), axis="x")


def post_lantern(p, x, y):
    """A lantern on a post with an arm, at a gangway's landing."""
    zq = Z["quay"]
    p.beam(T.straight((x, y, zq), (x, y, zq + 2.3), X, Y), 2.3, 0.18, 0.18, DARK, nseed(), wear=1.0)
    p.block(x - 0.22, x + 0.22, y - 0.22, y + 0.22, zq, zq + 0.12, "MI_Wood_TeakDark", nseed(), axis="x")
    s = -math.copysign(1.0, y)                                                   # the arm out over the water's side
    p.beam(T.straight((x, y, zq + 2.2), (x, y + s * 0.6, zq + 2.2), X, UP), 0.6, 0.1, 0.1, DARK, nseed())
    lantern(p, Vector((x, y + s * 0.55, zq + 2.2 - LANTERN_H(0.8))), chain_to=(x, y + s * 0.55, zq + 2.15), r=0.18, s=0.8)


def build_bridge():
    """One leaf of a lifting bridge (hub.bridges.lift), built from its hinge (the pivot, on the quay's deck at the basin's
    edge) out along +X: oak deck planks across two girders, a railed balustrade both sides, the iron hinge straps, a counter
    bracket. Its own collision (it moves: ASaltHubBridge raises it to hub.bridges.lift.open degrees)."""
    p = T.Part("SM_Hub_BridgeLeaf")
    w = HUB["bridges"]["lift"]["w"] * 0.5
    L = LEAF_L - 0.03                                                            # the leaves meet with a seam in the middle
    rng = random.Random(70)
    for yg in (-w + 0.3, w - 0.3):                                               # the girders
        p.beam(T.straight((0.0, yg, -LEAF_T * 0.5 - 0.04), (L, yg, -LEAF_T * 0.5 - 0.04), Y, UP), L, 0.28, LEAF_T - 0.08, DARK, nseed(), seg=1.0,
               wear=1.0)
    x = 0.0
    while x < L - 1e-6:                                                          # the deck: planks across
        xb = min(x + 0.32, L)
        p.board(T.straight((0.5 * (x + xb), -w, -0.04), (0.5 * (x + xb), w, -0.04), X, UP), 2 * w, xb - x - 0.012, 0.08, rng.choice(DOCK), nseed(),
                seg=1.0, wear=1.0)
        x = xb
    for yr in (-w + 0.06, w - 0.06):                                             # balustrades
        rail(p, "y", yr, 0.15, L - 0.05, 0.0)
    for yg in (-w + 0.3, w - 0.3):                                               # the hinge straps round the axle
        p.block(-0.05, 0.9, yg - 0.17, yg - 0.15, -LEAF_T, 0.0, "MI_Metal_Iron", nseed(), axis="x")
        p.block(-0.05, 0.9, yg + 0.15, yg + 0.17, -LEAF_T, 0.0, "MI_Metal_Iron", nseed(), axis="x")
        p.lathe([(0.0, -0.18), (0.16, -0.18), (0.16, 0.18), (0.0, 0.18)], Vector((0.0, yg, -0.2)), Y, "MI_Metal_Iron", sides=12)
    p.collide_box(0.0, L, -w, w, -LEAF_T, 0.0)
    for yr in (-w, w - 0.12):
        p.collide_box(0.15, L - 0.05, yr, yr + 0.12, 0.0, B.RAIL_H)
    export_pivoted(p, (0.0, 0.0, 0.0))


# --- the Slip -------------------------------------------------------------------------------------------------------------
def build_slip():
    """The Slip (spaces.slip, D70: the shipwright's shop on the harbour's starboard quay, no slipway): the counter across the
    shop, the plans pinned on its aft wall, the half-models, the workbench, the timber racks, the work on the quay in front (a
    spar on trestles, the steam box, coils, a grindstone), the 14 m cutter moored at her berth along the quay, lanterns, the
    SHIPWRIGHT sign over the shop's open front. (Floors, walls and ceilings are the floors'.)"""
    p = T.Part("SM_Hub_Slip")
    zq = Z["quay"]
    sl = {pr["id"]: pr for pr in H.space("slip")["props"]}
    rng = random.Random(9)
    c = sl["counter"]
    hgt = c["h"]
    Fc = (Vector((c["x0"], 0.0, 0.0)), Y, UP, -X)
    HE.wall_boards(p, Fc, c["y0"], c["y1"], zq, zq + hgt - 0.06, None, mats=("MI_Wood_Varnish", "MI_Wood_TeakDark"))
    p.block(c["x0"] - 0.12, c["x1"] + 0.08, c["y0"] - 0.05, c["y1"] + 0.05, zq + hgt - 0.07, zq + hgt, "MI_Wood_TeakLight", nseed(), axis="y")
    for yy in (c["y0"] + 0.05, 0.5 * (c["y0"] + c["y1"]), c["y1"] - 0.05):
        p.block(c["x0"], c["x1"], yy - 0.06, yy + 0.06, zq, zq + hgt - 0.07, "MI_Wood_TeakDark", nseed(), axis="z")
    ym = 0.5 * (c["y0"] + c["y1"])
    p.block(c["x0"] + 0.15, c["x0"] + 0.6, ym - 0.4, ym + 0.2, zq + hgt, zq + hgt + 0.06, "MI_Paint_HullDark", nseed(), axis="y")        # the ledger
    p.lathe([(0.0, 0.0), (0.05, 0.0), (0.05, 0.06), (0.02, 0.08), (0.0, 0.08)], Vector((c["x0"] + 0.4, ym + 0.6, zq + hgt)), UP, "MI_Glass_Dark", sides=8)
    # The plans pinned on the shop's aft wall, past the dock stair (D70: the quay's side is the shop's open front).
    pb = sl["plans_board"]
    xe = float(pb["wall"].split("=")[1]) + WALL * 0.5
    y0, y1 = pb["y0"], pb["y1"]
    p.block(xe, xe + 0.05, y0, y1, zq + pb["z0"], zq + pb["z1"], "MI_Wood_Teak", nseed(), axis="y")
    p.block(xe, xe + 0.12, y0 - 0.05, y1 + 0.05, zq + pb["z0"] - 0.06, zq + pb["z0"], "MI_Wood_TeakDark", nseed(), axis="y")     # its ledge
    for k in range(4):
        ya = y0 + 0.2 + k * 0.7
        za = zq + pb["z0"] + 0.15 + rng.uniform(0.0, 0.4)
        p.polygon([(xe + 0.06, ya, za), (xe + 0.06, ya + 0.6, za - 0.02), (xe + 0.06, ya + 0.62, za + 0.7), (xe + 0.06, ya + 0.02, za + 0.68)],
                  "MI_Paint_CreamAged", tone=rng.random(), facing=X)
    # The half-models over the shop's forward wall; the workbench.
    hm = sl["half_models"]
    xw = float(hm["wall"].split("=")[1]) - WALL * 0.5
    for k in range(3):
        ya = hm["y0"] + 0.3 + k * 2.0
        p.block(xw - 0.04, xw, ya - 0.1, ya + 1.6, zq + hm["z0"] - 0.05, zq + hm["z1"] + 0.05, "MI_Wood_TeakDark", nseed(), axis="y")
        pts = []
        for i in range(13):
            t = i / 12
            pts.append(Vector((xw - 0.05 - 0.22 * math.sin(math.pi * t) ** 0.7, ya + 1.5 * t, zq + hm["z1"] - 0.05 - 0.35 * math.sin(math.pi * t) ** 0.8)))
        f = T.polyline(pts, lambda q: UP, lambda q: -X)
        p.beam(f, T.length_of(f), 0.12, 0.14, ("MI_Paint_Hull", "MI_Paint_Cream", "MI_Wood_Varnish")[k], nseed(), seg=0.1)
    wb = sl["workbench"]
    p.block(wb["x0"], wb["x1"], wb["y0"], wb["y1"], zq + wb["h"] - 0.12, zq + wb["h"], "MI_Wood_TeakDark", nseed(), axis="x")
    for xx in (wb["x0"] + 0.15, wb["x1"] - 0.15):
        for yy in (wb["y0"] + 0.15, wb["y1"] - 0.15):
            p.block(xx - 0.08, xx + 0.08, yy - 0.08, yy + 0.08, zq, zq + wb["h"] - 0.12, "MI_Wood_TeakDark", nseed(), axis="z")
    p.block(wb["x0"] + 0.1, wb["x1"] - 0.1, wb["y0"] + 0.1, wb["y1"] - 0.1, zq + 0.18, zq + 0.24, "MI_Wood_Teak", nseed(), axis="x")
    p.block(wb["x0"] + 0.6, wb["x0"] + 1.0, wb["y0"] + 0.3, wb["y0"] + 0.8, zq + wb["h"], zq + wb["h"] + 0.14, "MI_Metal_Iron", nseed(), axis="y")    # a vice
    # The timber racks on the shop's forward wall.
    tr = sl["timber_racks"]
    xw = float(tr["wall"].split("=")[1]) - WALL * 0.5
    ra, rb = tr["y0"], tr["y1"]
    for k in range(4):
        z = zq + tr["z0"] + (tr["z1"] - tr["z0"]) * k / 3
        for yb in (ra + 0.3, rb - 0.3):
            p.block(xw - 0.6, xw, yb - 0.04, yb + 0.04, z - 0.06, z, "MI_Metal_Iron", nseed(), axis="x")
        for j in range(3):
            ya = ra + rng.uniform(-0.1, 0.4)
            yb_ = rb - rng.uniform(0.0, 0.8)
            p.board(T.straight((xw - 0.15 - 0.15 * j, ya, z + 0.05 + 0.08 * j), (xw - 0.15 - 0.15 * j, yb_, z + 0.05 + 0.08 * j), X, UP),
                    yb_ - ya, 0.14, 0.07, rng.choice(("MI_Wood_TeakLight", "MI_Wood_Teak", "MI_Wood_Planks")), nseed(), seg=1.0)
    # The work on the quay in front of the shop: a spar on two trestles, the steam box on its stove, coils, a grindstone.
    qw = sl["quay_work"]
    sy = 0.5 * (qw["y0"] + qw["y1"])
    for tx in (qw["x0"] + 0.5, qw["x0"] + 3.5):
        for dy in (-0.45, 0.45):
            a, b = Vector((tx - 0.3, sy + dy * 1.4, zq)), Vector((tx, sy + dy * 0.2, zq + 0.8))
            p.beam(T.straight(a, b, X, (b - a).normalized().cross(X)), (b - a).length, 0.12, 0.12, DARK, nseed())
        p.block(tx - 0.12, tx + 0.12, sy - 0.5, sy + 0.5, zq + 0.62, zq + 0.8, "MI_Wood_TeakDark", nseed(), axis="y")
    a, b = Vector((qw["x0"] - 0.6, sy, zq + 0.98)), Vector((qw["x0"] + 4.8, sy, zq + 0.96))
    p.round(T.straight(a, b, Y, UP), (b - a).length, lambda t: 0.18 - 0.06 * t, "MI_Wood_Varnish", nseed(), sides=10, seg=1.0, wear=0.6)
    bx0, bx1, by0, by1 = qw["x1"] - 2.6, qw["x1"] - 0.4, qw["y1"] - 1.2, qw["y1"] - 0.2
    p.block(bx0, bx1, by0, by1, zq + 0.45, zq + 1.1, "MI_Wood_TeakDark", nseed(), axis="x")                  # the steam box
    p.block(bx0 + 0.3, bx0 + 0.9, by0 + 0.1, by1 - 0.1, zq, zq + 0.45, "MI_Metal_Iron", nseed(), axis="z")     # its stove
    p.round(T.straight((bx0 + 0.6, 0.5 * (by0 + by1), zq + 1.1), (bx0 + 0.6, 0.5 * (by0 + by1), Z["lower"] - SLAB - 0.05), X, Y),
            Z["lower"] - SLAB - 1.15 - zq, 0.09, "MI_Metal_Iron", nseed(), sides=8)                         # its flue
    for k, (cx_, cy_) in enumerate(((qw["x0"] + 5.4, sy), (qw["x0"] + 6.2, sy + 0.1))):                  # coils
        for j in range(4):
            p.beam(T.arc(Vector((cx_, cy_, zq + 0.05 + 0.07 * j)), X, Y, 0.38 - 0.03 * (j % 2), 0.0, 2 * math.pi), 2.3, 0.07, 0.07, "MI_Rope_Hemp",
                   nseed(), seg=0.2, wear=0.0)
    gx, gy = qw["x0"] + 6.9 + 0.3, sy                                                                       # the grindstone
    for dx in (-0.35, 0.35):
        p.block(gx + dx - 0.05, gx + dx + 0.05, gy - 0.05, gy + 0.05, zq, zq + 0.75, DARK, nseed(), axis="z")
    p.lathe([(0.0, -0.1), (0.42, -0.1), (0.45, -0.06), (0.45, 0.06), (0.42, 0.1), (0.0, 0.1)], Vector((gx, gy, zq + 0.75)), X, "MI_Hub_AncientStone", sides=18)
    # The 14 m cutter (D51) moored at her berth along the quay (harbour.fleet_berths), riding at her waterline.
    fb = next(b for b in H.prop("harbour", "fleet_berths")["berths"] if b["boat"] == "14 m")
    _, cy, _ = H.docking_pose(dict(fb, heading=0))
    cutter(p, {"x0": fb["x0"], "x1": fb["x1"], "y": cy, "beam": 4.4, "keel_z": -2.9}, cradles=False)
    # Lanterns under the Tally House in the shop; the sign under the lower deck's edge over the shop's open front.
    for x, y, zz in sl["lanterns"]["at"]:
        deck_lantern(p, x, y, Z["lower"])
    sg = sl["sign"]
    zc = zq + sg["z"]
    ys = sg["y"] - 0.05
    p.block(sg["x"] - 3.3, sg["x"] + 3.3, ys - 0.3, ys, zc - 0.4, zc + 0.4, "MI_Paint_HullDark", nseed(), axis="x")
    p.block(sg["x"] - 3.4, sg["x"] + 3.4, ys - 0.35, ys, zc - 0.47, zc - 0.37, "MI_Hub_Ochre", nseed(), axis="x")
    p.block(sg["x"] - 3.4, sg["x"] + 3.4, ys - 0.35, ys, zc + 0.37, zc + 0.47, "MI_Hub_Ochre", nseed(), axis="x")
    for xx in (sg["x"] - 2.8, sg["x"] + 2.8):                                    # hung from the lower deck's beams on iron straps
        p.block(xx - 0.04, xx + 0.04, ys - 0.2, ys - 0.1, zc + 0.45, Z["lower"] - SLAB + 0.05, "MI_Metal_Iron", nseed(), axis="z")
    sign_on_y(p, sg["text"], sg["x"], ys - 0.31, zc, 0.45, -1.0)
    export(p)


def mast_foot(mast_id):
    """A mast (hub.masts) and its radius at the main deck's partners (the rig's thickness, D62)."""
    m = next(m for m in HUB["masts"] if m["id"] == mast_id)
    return m, HUB["rig"]["thick"][0] * 0.5 * (0.85 + 0.15 * m["h"] / 76.0)


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
    # The cradles (display_boat note: at 0.21, 0.5 and 0.79 of her length): timber saddles shaped to her bottom, shores.
    for cx in (B.cradle_xs(db) if cradles else ()):
        t = (cx - x0) / L
        p.block(cx - 0.18, cx + 0.18, yc - 1.4, yc + 1.4, zq, zk - 0.25, "MI_Wood_TeakDark", nseed(), axis="y")
        for side in (1.0, -1.0):
            q = pt(t, 0.25, side)
            p.beam(T.straight(Vector((cx, yc + side * 1.3, zq)), q + Vector((0, 0, -0.05)), X, Vector((0, side, 0.3)).normalized()),
                   (q - Vector((cx, yc + side * 1.3, zq))).length, 0.16, 0.16, "MI_Wood_TeakDark", nseed())
            q2 = pt(t, 0.7, side) + Vector((0, side * 0.05, 0))
            a = Vector((cx + 0.3, yc + side * (hb + 0.3), zq))
            p.beam(T.straight(a, q2, X, (q2 - a).normalized().cross(X)), (q2 - a).length, 0.14, 0.14, "MI_Wood_Planks", nseed())


# --- stairs, the hull's inner side -----------------------------------------------------------------------------------------
def stairs(p, x0, x1, y0, y1, up, z0, z1, rails=True, soffit=True, sides=(True, True)):
    """A flight on the blockout's steps (17 risers of 17.6 cm on 30 cm treads from the bottom end, the rest a landing): oak
    treads with a nosing, risers, stringers along its sides, a boarded soffit under it, balustrades (rails: both, none, or
    (low side, high side) across it)."""
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
    rail_on = rails if isinstance(rails, tuple) else (rails, rails)
    for c, on in zip((lo_c, hi_c), rail_on):
        if on:
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
    """The dock stair up from the shop and the Tally House (spaces.tally): the stair with its balustrade on the shop's side and a
    handrail bracketed to the hull, the job board with its header and slate, the pay booth with its grilled hatch, ledge, door and
    PAY, the weighing bench, sacks, salt cakes, the long table and its benches, lanterns. (Floors, walls, windows and ceilings are
    the floors'.)"""
    p = T.Part("SM_Hub_Tally")
    zq, zl = Z["quay"], Z["lower"]
    ceil = zl + B.DECK_H - SLAB
    st = H.R(HUB["stair"])
    tl = {pr["id"]: pr for pr in H.space("tally")["props"]}
    rng = random.Random(77)
    # D70: up along the shop's aft wall (+y), its balustrade on the shop's side, a handrail bracketed to the wall.
    stairs(p, st[0], st[1], st[2], st[3], HUB["stair"].get("up", "+x"), zq, zl, rails=(False, True), soffit=True)
    run = round((zl - zq) / B.RISER) * B.TREAD
    a, b = Vector((st[0] + 0.12, st[2], zq + 0.95)), Vector((st[0] + 0.12, st[2] + run, zl + 0.95))
    p.round(T.straight(a, b, X, (b - a).normalized().cross(X)), (b - a).length, 0.045, "MI_Wood_Varnish", nseed(), sides=8)
    xwall = round(st[0]) + WALL * 0.5                                            # the shop's aft wall's face
    for k in range(5):                                                           # its brackets to the wall
        q = a + (b - a) * ((k + 0.5) / 5)
        if q.x - xwall > 0.05:
            p.beam(T.straight(q - UP * 0.04, Vector((xwall, q.y, q.z - 0.04)), UP, Y), q.x - xwall, 0.03, 0.05, "MI_Metal_Iron", nseed())
    # The job board on the forward wall: six slips, the TALLY header over it, a chalk slate under it.
    jb = tl["job_board"]
    xw = float(jb["wall"].split("=")[1]) - WALL * 0.5
    p.block(xw - 0.06, xw, jb["y0"], jb["y1"], zl + jb["z0"], zl + jb["z1"], "MI_Wood_TeakDark", nseed(), axis="y")
    for yy in (jb["y0"] + 0.04, jb["y1"] - 0.04):
        p.block(xw - 0.1, xw, yy - 0.06, yy + 0.06, zl + jb["z0"] - 0.05, zl + jb["z1"] + 0.05, "MI_Wood_Varnish", nseed(), axis="z")
    chalk_slips(p, xw - 0.06, jb["y0"] + 0.3, jb["y1"] - 0.3, zl + jb["z0"] + 0.45, zl + jb["z1"] - 0.08, 6, -1.0, rng)
    p.block(xw - 0.05, xw - 0.02, jb["y0"] + 1.0, jb["y1"] - 1.0, zl + jb["z0"] + 0.04, zl + jb["z0"] + 0.4, "MI_Glass_Dark", nseed(), axis="y")  # the slate
    hy0, hy1 = jb["y0"] + 0.6, jb["y1"] - 0.6
    hz0, hz1 = zl + jb["z1"] + 0.07, min(zl + jb["z1"] + 0.42, ceil - 0.33)
    p.block(xw - 0.08, xw, hy0, hy1, hz0, hz1, "MI_Paint_HullDark", nseed(), axis="y")
    sign_on_x(p, "TALLY", xw - 0.09, 0.5 * (hy0 + hy1), 0.5 * (hz0 + hz1), (hz1 - hz0) * 0.72, -1.0)
    # The pay booth on the hull wall: timber walls, a grilled hatch over a ledge, a door in its forward side, PAY over the hatch.
    pb = tl["pay_booth"]
    dy0, dy1 = pb["door"]
    booth_h = pb["h"]
    door_h = min(B.DOOR_H, booth_h - 0.1)
    hatch = (pb["x0"] + 0.9, pb["x1"] - 1.1, zl + 1.0, zl + 1.8)                     # (its note: the hatch x and z 1.0..1.8)
    Fb = (Vector((0.0, pb["y0"] + 0.08, 0.0)), X, UP, -Y)
    HE.wall_boards(p, Fb, pb["x0"], pb["x1"], zl, zl + booth_h, lambda u, v: not (hatch[0] < u < hatch[1] and hatch[2] < v < hatch[3]),
                   mats=("MI_Wood_Varnish", "MI_Wood_TeakDark"))
    for xx in (pb["x0"] + 0.06, hatch[0] - 0.06, hatch[1] + 0.06, pb["x1"] - 0.06):
        p.block(xx - 0.07, xx + 0.07, pb["y0"] - 0.08, pb["y0"] + 0.04, zl, zl + booth_h, "MI_Wood_TeakDark", nseed(), axis="z")
    for k in range(9):
        xx = hatch[0] + (hatch[1] - hatch[0]) * (k + 0.5) / 9
        p.beam(T.straight((xx, pb["y0"] - 0.02, hatch[2]), (xx, pb["y0"] - 0.02, hatch[3]), X, Y), hatch[3] - hatch[2], 0.025, 0.025, "MI_Metal_Iron", nseed())
    p.block(hatch[0] - 0.1, hatch[1] + 0.1, pb["y0"] - 0.4, pb["y0"], zl + 0.95, zl + 1.0, "MI_Wood_TeakLight", nseed(), axis="x")          # the ledge
    Fs = (Vector((pb["x0"] - 0.08, 0.0, 0.0)), -Y, UP, -X)
    HE.wall_boards(p, Fs, -pb["y1"], -pb["y0"], zl, zl + booth_h, None, mats=("MI_Wood_Varnish", "MI_Wood_TeakDark"))
    Fd = (Vector((pb["x1"] - 0.08, 0.0, 0.0)), Y, UP, X)
    HE.wall_boards(p, Fd, pb["y0"], pb["y1"], zl, zl + booth_h, lambda u, v: not (dy0 < u < dy1 and v < zl + door_h), mats=("MI_Wood_Varnish", "MI_Wood_TeakDark"))
    for k in range(3):                                                           # the door, shut, on strap hinges
        u = dy0 + (dy1 - dy0) * (k + 0.5) / 3
        p.block(pb["x1"] - 0.02, pb["x1"] + 0.04, u - (dy1 - dy0) / 6 + 0.01, u + (dy1 - dy0) / 6 - 0.01, zl + 0.02, zl + door_h - 0.02, "MI_Wood_TeakDark",
                nseed(), axis="z")
    for zz in (zl + 0.4, zl + door_h - 0.4):
        p.block(pb["x1"] + 0.04, pb["x1"] + 0.06, dy0 + 0.05, dy0 + 0.7, zz - 0.04, zz + 0.04, "MI_Metal_Iron", nseed(), axis="y")
    p.block(pb["x0"] - 0.15, pb["x1"] + 0.15, pb["y0"] - 0.2, pb["y1"] - 0.1, zl + booth_h, zl + booth_h + 0.1, "MI_Wood_TeakDark", nseed(), axis="x")
    hxm = 0.5 * (hatch[0] + hatch[1])
    p.block(hxm - 0.7, hxm + 0.7, pb["y0"] - 0.09, pb["y0"] - 0.05, zl + 1.9, zl + 2.24, "MI_Paint_HullDark", nseed(), axis="x")
    sign_on_y(p, "PAY", hxm, pb["y0"] - 0.1, zl + 2.07, 0.24, -1.0)
    # The weighing bench with a big balance; sacks beside it; the salt cakes.
    wb = tl["weighing_bench"]
    p.block(wb["x0"], wb["x1"], wb["y0"], wb["y1"], zl + wb["h"] - 0.1, zl + wb["h"], "MI_Wood_TeakDark", nseed(), axis="x")
    for xx in (wb["x0"] + 0.15, wb["x1"] - 0.15):
        p.block(xx - 0.1, xx + 0.1, wb["y0"] + 0.1, wb["y1"] - 0.1, zl, zl + wb["h"] - 0.1, "MI_Wood_TeakDark", nseed(), axis="z")
    bx, by = 0.5 * (wb["x0"] + wb["x1"]), 0.5 * (wb["y0"] + wb["y1"])
    zt = zl + wb["h"]
    beam_h = min(0.95, ceil - 0.3 - zt - 0.1)
    p.lathe([(0.0, 0.0), (0.22, 0.0), (0.18, 0.06), (0.06, 0.1), (0.05, beam_h - 0.05), (0.08, beam_h), (0.0, beam_h + 0.05)], Vector((bx, by, zt)), UP,
            "MI_Metal_Brass", sides=10)
    p.beam(T.straight((bx - 0.7, by, zt + beam_h), (bx + 0.7, by, zt + beam_h), Y, UP), 1.4, 0.05, 0.06, "MI_Metal_Brass", nseed())
    for xx in (bx - 0.65, bx + 0.65):
        for dy in (-0.12, 0.12):
            a, b = Vector((xx, by, zt + beam_h - 0.02)), Vector((xx, by + dy, zt + 0.35))
            p.beam(T.straight(a, b, X, (b - a).normalized().cross(X)), (b - a).length, 0.01, 0.01, "MI_Metal_Brass", nseed())
        p.lathe([(0.0, 0.0), (0.2, 0.02), (0.22, 0.08), (0.0, 0.06)], Vector((xx, by, zt + 0.3)), UP, "MI_Metal_Brass", sides=12)
    for k in range(3):
        c = Vector((wb["x0"] + 0.5 + 0.8 * k, wb["y0"] - 0.45, zl))
        p.lathe([(0.0, 0.0), (0.26, 0.0), (0.32, 0.2), (0.3, 0.45), (0.2, 0.6), (0.08, 0.68), (0.0, 0.7)], c, UP, "MI_Canvas", sides=10, wear=0.0)
    sc = tl["salt_cakes"]
    zz = zl
    while zz < zl + sc["h"] - 0.05:
        for xx in (sc["x0"] + 0.2, sc["x1"] - 0.2):
            for yy in (sc["y0"] + 0.2, sc["y1"] - 0.2):
                p.block(xx - 0.18, xx + 0.18, yy - 0.18, yy + 0.18, zz, zz + 0.14, "MI_Paint_Cream", nseed(), axis="x", wear=0.3)
        zz += 0.15
    # The long table and its benches (table note: benches at y 14.2..14.6 and 11.8..12.2); tankards on it.
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
    for f in (0.15, 0.45, 0.85):
        xx = tb["x0"] + (tb["x1"] - tb["x0"]) * f
        p.lathe([(0.0, 0.0), (0.07, 0.0), (0.075, 0.14), (0.07, 0.16), (0.0, 0.16)], Vector((xx, 0.5 * (tb["y0"] + tb["y1"]) + rng.uniform(-0.2, 0.2), zl + tb["h"])),
                UP, "MI_Metal_Brass" if f > 0.8 else "MI_Wood_Varnish", sides=10)
    for x, y, zz in tl["lanterns"]["at"]:
        deck_lantern(p, x, y, Z["middle"])
    export(p)


# --- the Well and the Keelgate ----------------------------------------------------------------------------------------------
def wall_run_at(level, axis, line, a, half=0.4):
    """Is there a plain wall (no door, no gate) on the line at `a` (+/- half) on this floor? (For things hung on walls.)"""
    pl = PL.plan(level)
    spans = B.merge([(w["a0"], w["a1"]) for w in pl.walls if w["kind"] == "wall" and w["axis"] == axis and abs(w["line"] - line) < 1e-6] or [(0.0, 0.0)])
    if not any(a0 <= a - half and a + half <= a1 for a0, a1 in spans):
        return False
    for d in list(pl.doors) + list(pl.gates):
        if d["axis"] == axis and abs((d["x"] if axis == "x" else d["y"]) - line) < 1e-6:
            mid = d["y"] if axis == "x" else d["x"]
            if abs(mid - a) < d["w"] / 2 + half + 0.3:
                return False
    return True


def build_well():
    """The Well (spaces.well): its five flights (railed on the open side), the hoist's fixed parts, a lantern on each gallery's
    corners on a bracket from the outer wall, the ring lantern in the shaft on a chain from the gantry. (The galleries' floors,
    walls, rails and ceilings are the floors'.)"""
    p = T.Part("SM_Hub_Well")
    ring_ = H.R(HUB["ring"])
    for f in H.flights():
        r = H.R(f)
        along_x = f["up"][1] == "x"
        lo, hi = (r[2], r[3]) if along_x else (r[0], r[1])
        edges = (ring_[2], ring_[3]) if along_x else (ring_[0], ring_[1])
        walled = [any(abs(c - e) < 1e-6 for e in edges) and f["from"] != "quay" for c in (lo, hi)]      # a side on the gallery's wall
        stairs(p, r[0], r[1], r[2], r[3], f["up"], Z[f["from"]], Z[f["to"]], rails=(not walled[0], not walled[1]), soffit=True,
               sides=(True, True))
    gantry_and_hoist(p)
    for lv in ("lower", "middle", "upper", "gun"):
        z = Z[lv]
        for x in (ring_[0] + 0.8, ring_[1] - 0.8):
            for sy in (1.0, -1.0):
                yl = sy * ring_[3]
                if not wall_run_at(lv, "y", yl, x):
                    continue
                face = yl - sy * (WALL * 0.5 + 0.12)
                p.beam(T.straight((x, face + sy * 0.06, z), (x, face + sy * 0.06, z + B.DECK_H - SLAB), X, Y), B.DECK_H - SLAB, 0.22, 0.12,
                       "MI_Wood_TeakDark", nseed())                                     # the post it hangs from
                bracket_lantern(p, Vector((x, face, z)), (0.0, -sy, 0.0), 2.3, drop=0.0, arm=0.6, s=0.7)
    # The ring lantern: an iron hoop with six lanterns, at the middle deck, on a chain from under the gantry's apex.
    zr = Z["middle"] + 1.6
    c = Vector((84.0, 0.0, zr))
    hoop = 1.3
    # D82: its one light hangs inside the hoop, so the hoop, its six lanterns and their chains cast no shadow from it (six dark
    # wedges ran across the shaft's walls).
    iron = T.no_shadow("MI_Metal_Iron")
    p.beam(T.arc(c, X, UP, hoop, 0.0, 2 * math.pi), 2 * math.pi * hoop, 0.08, 0.06, iron, nseed(), seg=0.2)
    for k in range(6):
        a = 2 * math.pi * k / 6
        d = Vector((math.cos(a), math.sin(a), 0.0))
        lantern(p, c + d * hoop - UP * (0.05 + LANTERN_H(0.8)), chain_to=c + d * hoop, r=0.18, s=0.8, lit=False, shadow=False)
        if k % 2 == 0:
            lantern_chain(p, c + d * hoop, c + UP * 2.2, iron)
    lamp(c - UP * 0.4)
    top = Z["main"] + 7.5 - 1.0                                                  # under the brake drum at the apex (gantry note)
    lantern_chain(p, c + UP * 2.2, Vector((84.0, 0.0, top)), iron)
    export(p)


def lantern_chain(p, a, b, mat="MI_Metal_Iron"):
    """An iron chain from a to b."""
    a, b = Vector(a), Vector(b)
    L = (b - a).length
    d = (b - a).normalized()
    k = 0
    while k * 0.12 < L:
        q = a + (b - a) * (k * 0.12 / L)
        p.beam(T.arc(q + d * 0.06, X if k % 2 else Y, d, 0.05, 0.0, 2 * math.pi), 0.33, 0.016, 0.016, mat, nseed(), seg=0.08)
        k += 1


def gantry_and_hoist(p):
    """The hoist's fixed parts (well.hoist and its notes): the gantry an A-frame over the opening on the main deck with guys
    to the deck, its beam out over the hoist with the two sheaves, the brake drum at its head; the guide posts at the cage's
    aft corners, braced to each gallery's edge; the counterweight's railed bay. The cage and the counterweight move
    (SM_Hub_HoistCage, SM_Hub_HoistWeight; ASaltHubHoist hangs their ropes)."""
    zq, zm = Z["quay"], Z["main"]
    hs = H.R(HUB["hoist"])
    top = zm + 7.5
    apex = Vector((84.0, 0.0, top))
    for fx in (76.5, 91.5):
        a = Vector((fx, 0.0, zm))
        p.beam(T.straight(a, apex, Y, (apex - a).normalized().cross(Y)), (apex - a).length, 0.45, 0.45, DARK, nseed(), seg=1.0, wear=1.0)
        p.block(fx - 0.5, fx + 0.5, -0.5, 0.5, zm, zm + 0.25, "MI_Metal_Iron", nseed(), axis="x")
    for gx, gy in GANTRY_GUYS:                                                   # guys to ring bolts on the deck
        b = Vector((gx, gy, zm))
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
    head = zm + GUIDE_OVER_MAIN                                                 # the guides' head: over the cage at its top stop
    for gy in (hs[2], hs[3] - 0.1):
        p.beam(T.straight((hs[0] - 0.08, gy + 0.05, zq), (hs[0] - 0.08, gy + 0.05, head), X, Y), head - zq, 0.16, 0.12, "MI_Wood_TeakDark", nseed(), seg=1.0)
        a, b = Vector((hs[0] - 0.08, gy + 0.05, head - 0.4)), Vector((H.R(HUB["shaft"])[0] - 1.2, gy + 0.05, zm))      # a brace to the deck
        p.beam(T.straight(a, b, Y, (b - a).normalized().cross(Y)), (b - a).length, 0.14, 0.12, "MI_Wood_TeakDark", nseed())
        for lv in ("lower", "middle", "upper", "gun", "main"):
            zz = Z[lv] - 0.4
            a, b = Vector((hs[0] - 0.08, gy + 0.05, zz)), Vector((H.R(HUB["shaft"])[0] - 0.05, gy + 0.05, zz))
            if (a - b).length > 0.05:
                p.beam(T.straight(a, b, UP, Y), (a - b).length, 0.1, 0.08, "MI_Metal_Iron", nseed())
    p.beam(T.straight((hs[0] - 0.08, hs[2] - 0.1, head + 0.08), (hs[0] - 0.08, hs[3] + 0.1, head + 0.08), X, UP), hs[3] - hs[2] + 0.2, 0.18, 0.16,
           "MI_Wood_TeakDark", nseed())                                         # their head beam
    for (ax, ln, a0, a1) in (("y", cw["y0"] - 0.2, cw["x0"] - 0.2, cw["x1"] + 0.3), ("x", cw["x1"] + 0.3, cw["y0"] - 0.2, cw["y1"]),
                             ("x", cw["x0"] - 0.2, cw["y0"] - 0.2, cw["y1"])):
        rail(p, ax, ln, a0, a1, zq)


GUIDE_OVER_MAIN = B.GUIDE_OVER_MAIN
GANTRY_GUYS = B.GANTRY_GUYS     # the gantry's guys' feet on the main deck (clear of the companion over the last flight)


def hoist_pivots():
    """The moving parts' pivots (ship metres): the cage's floor centre at the quay stop, the counterweight's bottom centre
    in its bay. ASaltHubHoist stands at the cage's pivot (build_hub_level.py)."""
    hs = H.R(HUB["hoist"])
    cw = H.prop("well", "counterweight")
    return ((0.5 * (hs[0] + hs[1]), 0.5 * (hs[2] + hs[3]), Z["quay"]),
            (0.5 * (cw["x0"] + cw["x1"]), 0.5 * (cw["y0"] + cw["y1"]), Z["quay"]))


def export_pivoted(p, pivot):
    objs = p.finish(HH.material, ucx=bool(p.ucx), pivot=pivot)
    sb.save_blend(p.name)
    sb.export_fbx(objs, os.path.join("Hub", p.name + ".fbx"))


def build_hoist():
    """The cage (well.hoist): four posts, a floor, rails to the shoulder on three sides, a roof frame, its aft side open (the
    door); the four ropes up to a yoke; the brake's lever by the door. The counterweight (well.counterweight): stone blocks in
    an iron frame with an eye for its rope. Both built where they stand with the cage at the quay stop and the weight in its
    bay, then pivoted there."""
    cage_pivot, weight_pivot = hoist_pivots()
    zq = Z["quay"]
    hs = H.R(HUB["hoist"])
    hx, hy = cage_pivot[0], cage_pivot[1]
    p = T.Part("SM_Hub_HoistCage")
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
        p.beam(T.straight((hs[1] - 0.06, hs[2] + 0.06, zz), (hs[1] - 0.06, hs[3] - 0.06, zz), UP, X), hs[3] - hs[2] - 0.12, 0.05, 0.04,
               "MI_Wood_TeakDark", nseed())
    for cx in (hs[0] + 0.06, hs[1] - 0.06):                                       # the ropes from the posts' heads up to the yoke
        for cy in (hs[2] + 0.06, hs[3] - 0.06):
            a, b = Vector((cx, cy, cz + 0.02)), Vector((hx + (0.25 if cx > hx else -0.25), hy, cz + 0.66))
            p.round(T.straight(a, b, X, Y), (b - a).length, 0.025, "MI_Rope_Hemp", nseed(), sides=5, seg=2.0, wear=0.0)
    p.block(hx - 0.3, hx + 0.3, hy - 0.08, hy + 0.08, cz + 0.62, cz + 0.8, "MI_Metal_Iron", nseed(), axis="x")
    p.beam(T.straight((hx + 0.3, hy - 0.5, z0 + 0.15), (hx + 0.3, hy - 0.45, z0 + 1.2), X, Y), 1.05, 0.05, 0.05, "MI_Metal_Iron", nseed())  # the lever
    # Collision: the floor and the three railed sides (shoulder high); the aft side open.
    p.collide_box(hs[0], hs[1], hs[2], hs[3], z0, z0 + 0.12)
    p.collide_box(hs[1] - 0.12, hs[1], hs[2], hs[3], z0, z0 + 1.1)
    p.collide_box(hs[0], hs[1], hs[2], hs[2] + 0.12, z0, z0 + 1.1)
    p.collide_box(hs[0], hs[1], hs[3] - 0.12, hs[3], z0, z0 + 1.1)
    export_pivoted(p, cage_pivot)

    cw = H.prop("well", "counterweight")
    w = T.Part("SM_Hub_HoistWeight")
    wz = weight_pivot[2]
    w.block(cw["x0"], cw["x1"], cw["y0"], cw["y1"], wz, wz + cw["h"], STONE, nseed(), axis="z")
    for zz in (wz + 0.1, wz + cw["h"] - 0.1):
        w.block(cw["x0"] - 0.04, cw["x1"] + 0.04, cw["y0"] - 0.04, cw["y1"] + 0.04, zz - 0.05, zz + 0.05, "MI_Metal_Iron", nseed(), axis="x")
    wx, wy = weight_pivot[0], weight_pivot[1]
    w.beam(T.arc(Vector((wx, wy, wz + cw["h"] + 0.12)), X, UP, 0.1, 0.0, 2 * math.pi), 0.63, 0.035, 0.035, "MI_Metal_Iron", nseed(), seg=0.1)  # its eye
    export_pivoted(w, weight_pivot)


KEEL_LINE = 0.07              # the gate's glowing lines' width (m)


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
    """The main deck's own things (spaces.main_deck; its planking is the floors'): the bulwarks' inner side from the quarterdeck's
    front to the forecastle's (waterway, stanchions, lining up to the cap rail), the Well's coaming and rails (open at the hoist's
    stop), the companion's hood over the last flight, fife rails round the masts, the cargo hatches, the ship's boats, the capstan,
    the galley's chimney, lanterns."""
    p = T.Part("SM_Hub_MainDeck")
    zm = Z["main"]
    sh = H.R(HUB["shaft"])
    md = {pr["id"]: pr for pr in H.space("main_deck")["props"]}
    xa, xb = HUB["qdX"] + 0.3, HUB["fcX"] - 0.1
    xs = [xa + k * 2.0 for k in range(int((xb - xa) / 2.0) + 1)] + [xb]
    for side in (1.0, -1.0):
        pts = [Vector((xx, side * (H.half_width(xx, zm) - 0.2), zm + 0.06)) for xx in xs]
        f = T.polyline(pts, lambda q: UP, lambda q, s=side: Vector((0.0, -s, 0.0)))
        p.beam(f, T.length_of(f, 80), 0.12, 0.3, "MI_Wood_TeakDark", nseed(), seg=1.0)                       # the waterway
        xx = xa + 0.3
        while xx < xb:
            q0 = Vector((xx, side * (H.half_width(xx, zm) - 0.2), zm))
            q1 = Vector((xx, side * (H.half_width(xx, zm + 0.85) - 0.2), zm + 0.86))
            p.beam(T.straight(q0, q1, X, Vector((0.0, -side, 0.0))), (q1 - q0).length, 0.16, 0.12, "MI_Wood_TeakDark", nseed())
            xx += 1.2
        for zr in (zm + 0.12, zm + 0.42, zm + 0.72):
            pts = [Vector((xx, side * (H.half_width(xx, zr) - 0.1), zr)) for xx in xs]
            f = T.polyline(pts, lambda q: UP, lambda q, s=side: Vector((0.0, -s, 0.0)))
            p.board(f, T.length_of(f, 80), 0.29, 0.06, random.choice(("MI_Wood_Planks", "MI_Wood_TeakDark")), nseed(), seg=1.0, wear=0.8)
    # The Well's opening: the coaming and the rails on it, both open where the hoist's cage stops (its door is aft).
    hs = H.R(HUB["hoist"])
    co = H.prop("well", "coaming")
    for (ax, ln, a0, a1) in (("x", sh[0], sh[2], sh[3]), ("x", sh[1], sh[2], sh[3]), ("y", sh[2], sh[0], sh[1]), ("y", sh[3], sh[0], sh[1])):
        coam, rails_ = [(a0 - 0.2, a1 + 0.2)], [(a0, a1)]
        if ax == "x" and abs(ln - sh[0]) < 1e-6:
            coam, rails_ = [(a0 - 0.2, hs[2]), (hs[3], a1 + 0.2)], [(a0, hs[2]), (hs[3], a1)]
        for pa, pb in coam:
            if ax == "x":
                p.block(ln - 0.18, ln + 0.18, pa, pb, zm, zm + co["h"], "MI_Wood_TeakDark", nseed(), axis="y")
            else:
                p.block(pa, pb, ln - 0.18, ln + 0.18, zm, zm + co["h"], "MI_Wood_TeakDark", nseed(), axis="x")
        for pa, pb in rails_:
            rail(p, ax, ln, pa, pb, zm + co["h"], height=B.RAIL_H - co["h"])
    p.block(sh[0] - 0.18, sh[0] + 0.18, hs[2], hs[3], zm - 0.02, zm + 0.03, "MI_Metal_Iron", nseed(), axis="y")    # the sill at the stop
    # The companion: a hood over the head of the last flight (the floors rail the hatch), open where one comes up.
    f = next(f for f in H.flights() if f["to"] == "main")
    cx0, cx1, cy0, cy1 = H.R(f)
    hx0 = cx1 - 3.0
    eave, ridge = zm + 2.3, zm + 2.8
    ym = 0.5 * (cy0 + cy1)
    for xx in (hx0, cx1):
        for yy in (cy0, cy1):
            p.beam(T.straight((xx, yy, zm), (xx, yy, eave), X, Y), eave - zm, 0.16, 0.16, "MI_Wood_Varnish", nseed())
        p.beam(T.straight((xx, cy0 - 0.2, eave + 0.07), (xx, cy1 + 0.2, eave + 0.07), X, UP), cy1 - cy0 + 0.4, 0.16, 0.14, "MI_Wood_TeakDark", nseed())
    for yy in (cy0, cy1):
        p.beam(T.straight((hx0 - 0.1, yy, eave - 0.08), (cx1 + 0.1, yy, eave - 0.08), Y, UP), cx1 - hx0 + 0.2, 0.14, 0.16, "MI_Wood_TeakDark", nseed())
    for s in (-1.0, 1.0):
        e = Vector((0.0, (cy0 - 0.3) if s < 0 else (cy1 + 0.3), eave + 0.1))
        r_ = Vector((0.0, ym, ridge + 0.1))
        sl_ = r_ - e
        Ls = sl_.length
        sd = sl_ / Ls
        nrm = X.cross(sd)
        if nrm.z < 0:
            nrm = -nrm
        nb = max(2, int(Ls / 0.3))
        for k in range(nb):
            q = e + sd * (Ls * (k + 0.5) / nb) + nrm * 0.03
            p.board(T.straight((hx0 - 0.3, q.y, q.z), (cx1 + 0.3, q.y, q.z), sd, nrm), cx1 - hx0 + 0.6, Ls / nb - 0.01, 0.05,
                    random.choice(("MI_Wood_TeakDark", "MI_Hub_HullTar")), nseed(), seg=1.0, wear=0.8)
    p.beam(T.straight((hx0 - 0.35, ym, ridge + 0.16), (cx1 + 0.35, ym, ridge + 0.16), Y, UP), cx1 - hx0 + 0.7, 0.2, 0.08, "MI_Wood_Varnish", nseed())
    p.polygon([(hx0 - 0.02, cy0 - 0.1, eave), (hx0 - 0.02, cy1 + 0.1, eave), (hx0 - 0.02, ym, ridge)], "MI_Wood_Planks", tone=0.4, facing=-X)  # its gable
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
    # Lanterns: by the companion from its hood's post, at the main mast from a bracket on it (the Chandlery's are its own).
    hook = md["lanterns"]["hook_z"]
    for x, y, zz in md["lanterns"]["at"]:
        if abs(y - cy0) < 1.0 and hx0 - 1.5 < x < cx1:
            post = Vector((hx0, cy0, zm))
            bracket_lantern(p, post, (x - post.x, y - post.y, 0.0), hook, drop=0.05, arm=Vector((x - post.x, y - post.y)).length)
        elif any(abs(x - mx) < 0.5 and abs(y - my) < 3.0 for mx, my in md["masts"]["at"]):
            m, r = mast_foot(next(mm["id"] for mm in HUB["masts"] if abs(mm["x"] - x) < 0.5))
            sy = 1.0 if y > 0 else -1.0
            bracket_lantern(p, Vector((x, sy * r, zm)), (0.0, sy, 0.0), hook, drop=0.0, arm=abs(y) - r)
    export(p)



# --- the Chandlery ------------------------------------------------------------------------------------------------------------
def build_chandlery():
    """The Chandlery in the forecastle's front (spaces.chandlery, D69: owner "implement the Chandler better into the ship, it just
    stands around ... at the front of the ship"): a shop two storeys high at the main deck. Outside, on the forecastle's front: two
    shop windows of small panes either side of the door (lit inside), the CHANDLER board over them, a striped awning on iron arms,
    lanterns by the door, goods stacked outside. Inside (its floor, walls and ceiling are the floors' build): the counter across
    the shop facing the door, shelves of stock up to the beams with a ladder on a brass rail, the beams hung with lanterns, coils
    and a net, the display table with the compass, the spyglass on its tripod."""
    p = T.Part("SM_Hub_Chandlery")
    zm = Z["main"]
    cd = {pr["id"]: pr for pr in H.space("chandlery")["props"]}
    sh = cd["shop"]
    x0, x1, y0, y1 = sh["x0"], sh["x1"], sh["y0"], sh["y1"]
    rng = random.Random(41)
    door = next(d for d in PL.plan("main").doors if d["kind"] == "link" and {d["a"][1], d["b"][1]} == {"deck", "chandlery"})
    dy, dw = door["y"], door["w"]
    gates_back = [g for g in PL.plan("main").gates if abs(g["x"] - x1) < 0.3]
    # --- the front, outside (x0, facing aft) ---
    xf = x0 - 0.02
    F = (Vector((xf, 0.0, zm)), -Y, UP, -X)                                         # t along -y so u = -y, n aft
    fr = cd["front"]
    wins = fr["spans"]

    def keep(u, v):
        y = -u
        if abs(y - dy) < dw * 0.5 + 0.35 and v < 2.75:
            return False
        for wa, wb in wins:
            if wa - 0.2 < y < wb + 0.2 and fr["z0"] - 0.15 < v < fr["z1"] + 0.15:
                return False
        return True
    sg_top = cd["sign"]["z1"] + 0.2                                               # the shopfront's boards run up behind the board
    HE.wall_boards(p, F, -(y1 + 0.2), -(y0 - 0.2), 0.0, sg_top, keep, mats=("MI_Paint_Hull", "MI_Paint_Hull", "MI_Paint_HullFaded"))
    for wa, wb in wins:                                                           # the shop windows: small panes, lit inside
        u = -0.5 * (wa + wb)
        Fw = (HH.lpt(F, u, 0.5 * (fr["z0"] + fr["z1"]), 0.0), F[1], F[2], F[3])
        hw_, hh = 0.5 * (wb - wa), 0.5 * (fr["z1"] - fr["z0"])
        p.polygon([HH.lpt(Fw, a, b, -0.14) for a, b in ((-hw_, -hh), (hw_, -hh), (hw_, hh), (-hw_, hh))], "MI_LanternGlass", tone=0.5, facing=F[3])
        nv, nh = int(round(2 * hw_ / 0.42)), int(round(2 * hh / 0.4))
        for k in range(1, nv):
            HH.lbeam(p, Fw, -hw_ + 2 * hw_ * k / nv, -hh, -hw_ + 2 * hw_ * k / nv, hh, 0.05, 0.06, -0.12, "MI_Paint_Cream", wear=0.6)
        for k in range(1, nh):
            HH.lbeam(p, Fw, -hw_, -hh + 2 * hh * k / nh, hw_, -hh + 2 * hh * k / nh, 0.05, 0.06, -0.12, "MI_Paint_Cream", wear=0.6)
        for a in (-hw_ - 0.1, hw_ + 0.1):
            HH.lbeam(p, Fw, a, -hh - 0.1, a, hh + 0.12, 0.22, 0.18, 0.0, "MI_Paint_Cream", wear=0.8)
        HH.lbeam(p, Fw, -hw_ - 0.25, -hh - 0.14, hw_ + 0.25, -hh - 0.14, 0.24, 0.42, 0.06, "MI_Hub_Ochre", wear=1.0)     # the sill shelf
        HH.lbeam(p, Fw, -hw_ - 0.25, hh + 0.2, hw_ + 0.25, hh + 0.2, 0.2, 0.3, 0.04, "MI_Paint_Cream", wear=0.8)
        for k in range(int(2 * hw_ / 0.5)):                                       # stock on the sill: tins and jars
            c = HH.lpt(Fw, -hw_ + 0.25 + 0.5 * k, -hh - 0.02, 0.18)
            r = rng.uniform(0.06, 0.11)
            p.lathe([(0.0, 0.0), (r, 0.0), (r * 1.05, r * 1.8), (r * 0.7, r * 2.1), (0.0, r * 2.1)], c, UP,
                    rng.choice(("MI_Metal_Brass", "MI_Glass_Dark", "MI_Paint_Hull", "MI_Metal_Iron")), sides=8)
    # The door: its frame, a fanlight, the leaf standing open inward against the jamb.
    Fd = (HH.lpt(F, -dy, 0.0, 0.0), F[1], F[2], F[3])
    for a in (-dw * 0.5 - 0.12, dw * 0.5 + 0.12):
        HH.lbeam(p, Fd, a, 0.0, a, 2.6, 0.24, 0.24, 0.0, "MI_Paint_Cream", wear=1.0)
    HH.lbeam(p, Fd, -dw * 0.5 - 0.3, 2.68, dw * 0.5 + 0.3, 2.68, 0.2, 0.3, 0.02, "MI_Hub_Ochre", wear=1.0)
    for k in range(5):                                                            # the fanlight's rays
        a = math.pi * (k + 1) / 6
        c = HH.lpt(Fd, 0.0, 2.35, -0.05)
        q = c + (F[1] * math.cos(a) + UP * math.sin(a)) * 0.28
        p.beam(T.straight(c, q, F[3], (q - c).normalized().cross(F[3])), 0.28, 0.03, 0.03, "MI_Paint_Cream", nseed())
    p.polygon([HH.lpt(Fd, a, b, -0.08) for a, b in ((-dw * 0.5, 2.3), (dw * 0.5, 2.3), (dw * 0.5, 2.6), (-dw * 0.5, 2.6))], "MI_LanternGlass", tone=0.5, facing=F[3])
    hinge = Vector((x0 + 0.05, dy - dw * 0.5, zm))
    for k in range(3):
        xx = hinge.x + dw * (k + 0.5) / 3
        p.block(xx - dw / 6 + 0.01, xx + dw / 6 - 0.01, hinge.y - 0.08, hinge.y - 0.02, zm + 0.02, zm + 2.28,
                "MI_Paint_Hull" if k != 1 else "MI_Paint_HullFaded", nseed(), axis="z")
    # The CHANDLER board and the awning on two iron arms, striped canvas with a scalloped valance.
    sg = cd["sign"]
    p.block(x0 - 0.3, x0 - 0.16, sg["y0"], sg["y1"], zm + sg["z0"], zm + sg["z1"], "MI_Paint_HullDark", nseed(), axis="y")
    for zz in (sg["z0"] - 0.05, sg["z1"] + 0.05):
        p.block(x0 - 0.34, x0 - 0.16, sg["y0"] - 0.1, sg["y1"] + 0.1, zm + zz - 0.04, zm + zz + 0.04, "MI_Hub_Ochre", nseed(), axis="y")
    sign_on_x(p, sg["text"], x0 - 0.305, 0.5 * (sg["y0"] + sg["y1"]), zm + 0.5 * (sg["z0"] + sg["z1"]), 0.55, -1.0)
    aw = cd["awning"]
    (axa, aza), (axb, azb) = aw["from"], aw["to"]
    n = 14
    for k in range(n):
        ya = aw["y0"] + (aw["y1"] - aw["y0"]) * k / n
        yb = aw["y0"] + (aw["y1"] - aw["y0"]) * (k + 1) / n
        mat = "MI_Hub_CanvasRed" if k % 2 == 0 else "MI_Canvas"
        sag = 0.07
        p.polygon([(axa, ya, zm + aza), (axa, yb, zm + aza), (axb, yb, zm + azb - sag), (axb, ya, zm + azb - sag)], mat, tone=0.5, facing=UP)
        p.polygon([(axa, ya, zm + aza - 0.01), (axb, ya, zm + azb - sag - 0.01), (axb, yb, zm + azb - sag - 0.01), (axa, yb, zm + aza - 0.01)], mat, tone=0.5,
                  facing=-UP)
        p.polygon([(axb, ya, zm + azb - sag), (axb, yb, zm + azb - sag), (axb - 0.01, 0.5 * (ya + yb), zm + azb - sag - 0.26)], mat, tone=0.5, facing=-X)
    for yy in (aw["y0"] + 0.3, aw["y1"] - 0.3):
        a, b = Vector((x0 - 0.05, yy, zm + aza - 0.9)), Vector((axb + 0.05, yy, zm + azb - 0.05))
        p.beam(T.straight(a, b, Y, (b - a).normalized().cross(Y)), (b - a).length, 0.07, 0.07, "MI_Metal_Iron", nseed())
        p.beam(T.straight(Vector((x0 - 0.05, yy, zm + aza)), b, Y, UP), (b - Vector((x0 - 0.05, yy, zm + aza))).length, 0.05, 0.05, "MI_Metal_Iron", nseed())
    for yy in (dy - dw * 0.5 - 0.7, dy + dw * 0.5 + 0.7):                         # lanterns either side of the door
        bracket_lantern(p, Vector((x0 - 0.04, yy, zm)), (-1.0, 0.0, 0.0), 2.45, drop=0.05, arm=0.5, s=0.8)
    # Goods outside: barrels, crates, coils, an anchor, oars leaning on the front.
    for gx0, gx1, gy0, gy1 in cd["goods"]["boxes"]:
        bx, by = gx0 + 0.45, gy0 + 0.45
        for k, (ox, oy) in enumerate(((0.0, 0.0), (0.0, 0.8), (0.8, 0.0))):
            if bx + ox > gx1 - 0.3 or by + oy > gy1 - 0.3:
                continue
            c = Vector((bx + ox, by + oy, zm))
            p.lathe([(0.0, 0.0), (0.3, 0.0), (0.36, 0.25), (0.38, 0.45), (0.36, 0.65), (0.3, 0.9), (0.0, 0.9)], c, UP, "MI_Wood_Planks", sides=14)
            for zz in (0.15, 0.75):
                p.lathe([(0.34, zz - 0.04), (0.36, zz), (0.34, zz + 0.04)], c, UP, "MI_Metal_Iron", sides=14, caps=False)
        p.block(gx1 - 0.85, gx1 - 0.05, gy1 - 0.8, gy1 - 0.05, zm, zm + 0.6, "MI_Wood_Planks", nseed(), axis="x")
        p.block(gx1 - 0.75, gx1 - 0.15, gy1 - 0.7, gy1 - 0.15, zm + 0.6, zm + 0.95, "MI_Wood_TeakLight", nseed(), axis="y")
        c = Vector((gx0 + 1.2, 0.5 * (gy0 + gy1), zm))                            # a coil of new rope
        for j in range(5):
            p.beam(T.arc(c + UP * (0.04 + 0.06 * j), X, Y, 0.32 - 0.015 * j, 0.0, 2 * math.pi), 2.0, 0.05, 0.05, "MI_Rope_Hemp", nseed(), seg=0.08, wear=0.0)
    for k, yy in enumerate((dy + dw * 0.5 + 1.25, dy + dw * 0.5 + 1.45)):          # two oars leaning on the front
        a, b = Vector((x0 - 0.75, yy, zm)), Vector((x0 - 0.08, yy + 0.12 * k, zm + 2.9))
        p.round(T.straight(a, b, Y, X), (b - a).length, 0.04, "MI_Wood_Varnish", nseed(), sides=8)
        p.beam(T.straight(a, a + (b - a).normalized() * 0.9, Y, X), 0.9, 0.16, 0.03, "MI_Wood_Varnish", nseed())
    c = Vector((x0 - 0.7, dy - dw * 0.5 - 1.4, zm))                             # a small anchor against the front
    p.beam(T.straight(c, c + UP * 1.3 + X * 0.35, Y, X), 1.35, 0.1, 0.1, "MI_Metal_Iron", nseed(), wear=0.5)
    p.beam(T.arc(c + UP * 0.35 + X * 0.1, Y, UP, 0.45, math.pi * 1.1, math.pi * 1.9), 1.0, 0.09, 0.09, "MI_Metal_Iron", nseed(), wear=0.5)
    p.lathe([(0.12, -0.03), (0.15, 0.0), (0.12, 0.03)], c + UP * 1.4 + X * 0.38, X, "MI_Metal_Iron", sides=10, caps=False)
    # --- inside ---
    cn = cd["counter"]
    Fc = (Vector((cn["x0"], 0.0, 0.0)), Y, UP, -X)
    HE.wall_boards(p, Fc, cn["y0"], cn["y1"], zm, zm + cn["h"], None, mats=("MI_Wood_Varnish", "MI_Wood_TeakDark"))
    p.block(cn["x0"] - 0.12, cn["x1"] + 0.05, cn["y0"] - 0.05, cn["y1"] + 0.05, zm + cn["h"], zm + cn["top"], "MI_Wood_TeakLight", nseed(), axis="y")
    p.block(cn["x0"] + 0.1, cn["x1"], cn["y0"], cn["y0"] + 0.1, zm, zm + cn["h"], "MI_Wood_TeakDark", nseed(), axis="z")
    p.block(cn["x0"] + 0.1, cn["x1"], cn["y1"] - 0.1, cn["y1"], zm, zm + cn["h"], "MI_Wood_TeakDark", nseed(), axis="z")
    tz = zm + cn["top"]                                                          # on the counter: the ledger, scales, a lamp
    p.block(cn["x0"] + 0.15, cn["x0"] + 0.55, 4.6, 5.2, tz, tz + 0.05, "MI_Paint_CreamAged", nseed(), axis="y")
    p.lathe([(0.0, 0.0), (0.12, 0.0), (0.04, 0.05), (0.03, 0.4), (0.0, 0.42)], Vector((cn["x0"] + 0.4, 7.0, tz)), UP, "MI_Metal_Brass", sides=8)
    p.beam(T.straight((cn["x0"] + 0.4, 6.7, tz + 0.4), (cn["x0"] + 0.4, 7.3, tz + 0.4), X, UP), 0.6, 0.02, 0.02, "MI_Metal_Brass", nseed())
    lantern(p, Vector((cn["x0"] + 0.4, 3.2, tz)), r=0.12, lit=True)
    shv = cd["shelves"]
    gap_ys = [(g["y"] - 1.0, g["y"] + 1.0) for g in gates_back]
    for (sx0, sx1, sy0, sy1) in shv["runs"]:
        along_x = (sx1 - sx0) > (sy1 - sy0)
        spans = [(sx0, sx1) if along_x else (sy0, sy1)]
        if not along_x:                                                          # open where a gate stands in the back wall
            for ga, gb in gap_ys:
                nxt = []
                for a, b in spans:
                    if ga < b and gb > a:
                        if ga - a > 0.4:
                            nxt.append((a, ga))
                        if b - gb > 0.4:
                            nxt.append((gb, b))
                    else:
                        nxt.append((a, b))
                spans = nxt
        for a, b in spans:
            bx0, bx1, by0, by1 = (a, b, sy0, sy1) if along_x else (sx0, sx1, a, b)
            for k in range(8):
                zz = zm + 0.2 + (shv["h"] - 0.3) * k / 7
                p.block(bx0, bx1, by0, by1, zz - 0.04, zz, "MI_Wood_Teak", nseed(), axis="x" if along_x else "y")
                if k == 7:
                    continue
                u = a + 0.15
                while u < b - 0.2:
                    w = rng.uniform(0.12, 0.35)
                    kind = rng.random()
                    c = Vector(((u + w / 2) if along_x else 0.5 * (bx0 + bx1), 0.5 * (by0 + by1) if along_x else (u + w / 2), zz))
                    if kind < 0.35:
                        p.lathe([(0.0, 0.0), (w * 0.4, 0.0), (w * 0.42, w * 0.9), (w * 0.3, w * 1.0), (0.0, w * 1.0)], c, UP,
                                rng.choice(("MI_Metal_Brass", "MI_Metal_Iron", "MI_Glass_Dark", "MI_Paint_Hull")), sides=8)
                    elif kind < 0.6:
                        for j in range(3):
                            p.beam(T.arc(c + UP * (0.04 + 0.05 * j), X, Y, w * 0.38, 0.0, 2 * math.pi), w * 2.4, 0.04, 0.04, "MI_Rope_Hemp", nseed(), seg=0.08,
                                   wear=0.0)
                    else:
                        p.block(c.x - (w * 0.45 if along_x else 0.2), c.x + (w * 0.45 if along_x else 0.2), c.y - (0.2 if along_x else w * 0.45),
                                c.y + (0.2 if along_x else w * 0.45), zz, zz + rng.uniform(0.15, 0.35),
                                rng.choice(("MI_Wood_Planks", "MI_Wood_TeakLight", "MI_Paint_CreamAged")), nseed(), axis="x")
                    u += w + rng.uniform(0.05, 0.15)
            for uu in [a + (b - a) * j / max(1, int((b - a) / 1.6)) for j in range(int((b - a) / 1.6) + 1)]:
                if along_x:
                    p.block(uu - 0.04, uu + 0.04, by0, by1, zm, zm + shv["h"], "MI_Wood_TeakDark", nseed(), axis="z")
                else:
                    p.block(bx0, bx1, uu - 0.04, uu + 0.04, zm, zm + shv["h"], "MI_Wood_TeakDark", nseed(), axis="z")
        if not along_x:                                                          # the ladder on its brass rail, along the back
            rail_x = sx0 - 0.3
            p.round(T.straight((rail_x, sy0 + 0.2, zm + shv["h"] - 0.2), (rail_x, sy1 - 0.2, zm + shv["h"] - 0.2), X, UP), sy1 - sy0 - 0.4, 0.03,
                    "MI_Metal_Brass", nseed(), sides=6)
            ly = sy0 + (sy1 - sy0) * 0.22
            for dyy in (-0.25, 0.25):
                p.beam(T.straight((rail_x - 1.3, ly + dyy, zm), (rail_x, ly + dyy, zm + shv["h"] - 0.15), Y, X), 5.3, 0.06, 0.08, "MI_Wood_Varnish", nseed())
            for j in range(1, 16):
                t = j / 16
                q = Vector((rail_x - 1.3 + 1.3 * t, ly, zm + (shv["h"] - 0.15) * t))
                p.round(T.straight(q - Y * 0.25, q + Y * 0.25, X, UP), 0.5, 0.025, "MI_Wood_Varnish", nseed(), sides=6)
    dt = cd["display_table"]
    p.block(dt["x0"], dt["x1"], dt["y0"], dt["y1"], zm + dt["h"] - 0.06, zm + dt["h"], "MI_Wood_Varnish", nseed(), axis="x")
    for xx in (dt["x0"] + 0.12, dt["x1"] - 0.12):
        for yy in (dt["y0"] + 0.12, dt["y1"] - 0.12):
            p.lathe([(0.06, 0.0), (0.05, 0.1), (0.07, dt["h"] * 0.5), (0.05, dt["h"] - 0.06)], Vector((xx, yy, zm)), UP, "MI_Wood_TeakDark", sides=8)
    tz = zm + dt["h"]
    cc = Vector((0.5 * (dt["x0"] + dt["x1"]), dt["y0"] + 0.5, tz))
    p.lathe([(0.0, 0.0), (0.16, 0.0), (0.16, 0.08), (0.14, 0.1), (0.0, 0.1)], cc, UP, "MI_Metal_Brass", sides=14)            # the compass
    p.polygon([cc + Vector((-0.12, -0.12, 0.105)), cc + Vector((0.12, -0.12, 0.105)), cc + Vector((0.12, 0.12, 0.105)), cc + Vector((-0.12, 0.12, 0.105))],
              "MI_Paint_CreamAged", facing=UP)
    p.polygon([Vector((dt["x0"] + 0.1, dt["y1"] - 0.75, tz + 0.005)), Vector((dt["x1"] - 0.1, dt["y1"] - 0.75, tz + 0.005)),
               Vector((dt["x1"] - 0.1, dt["y1"] - 0.1, tz + 0.005)), Vector((dt["x0"] + 0.1, dt["y1"] - 0.1, tz + 0.005))], "MI_Paint_CreamAged", facing=UP)  # a chart
    tp = cd["spyglass_stand"]
    c = Vector((tp["x"], tp["y"], zm))
    for k in range(3):
        a = 2 * math.pi * k / 3 + 0.3
        foot = c + Vector((math.cos(a) * 0.45, math.sin(a) * 0.45, 0.0))
        p.beam(T.straight(foot, c + UP * (tp["h"] - 0.15), X, Y), (c + UP * (tp["h"] - 0.15) - foot).length, 0.05, 0.05, "MI_Wood_Varnish", nseed())
    p.round(T.straight(c + UP * tp["h"] + Vector((0.45, 0.2, -0.03)), c + UP * tp["h"] + Vector((-0.45, -0.2, 0.06)), Y, UP), 0.99,
            lambda s: 0.045 + 0.035 * s, "MI_Metal_Brass", nseed(), sides=10)
    bm = cd["beams"]
    zb = zm + bm["z"]
    for bx in bm["at_x"]:
        p.beam(T.straight((bx, y0 + 0.2, zb), (bx, y1 - 0.2, zb), X, UP), y1 - y0 - 0.4, 0.26, 0.28, "MI_Wood_TeakDark", nseed())
        for k, yy in enumerate([y0 + 1.6 + 2.4 * j for j in range(5)]):
            if k % 3 == 0:
                lantern(p, Vector((bx, yy, zb - 1.3)), chain_to=(bx, yy, zb - 0.14), r=0.16, lit=(k == 0 or bx == bm["at_x"][1]))
            elif k % 3 == 1:
                for j in range(4):
                    p.beam(T.arc(Vector((bx, yy, zb - 0.55 - 0.04 * j)), X, UP, 0.28 - 0.01 * j, -math.pi * 0.4, math.pi * 1.4), 1.4, 0.04, 0.04, "MI_Rope_Hemp",
                           nseed(), seg=0.08, wear=0.0)
            else:
                pts = [Vector((bx, yy - 0.6 + 1.2 * t, zb - 0.15 - 0.9 * math.sin(math.pi * t))) for t in (j / 8 for j in range(9))]
                for dz in (0.0, -0.2, -0.4):
                    f = T.polyline([q + UP * dz for q in pts], lambda q: X, lambda q: UP)
                    p.round(f, T.length_of(f), 0.012, "MI_Rope_Hemp", nseed(), sides=4, wear=0.0)
    export(p)


# --- the gates ---------------------------------------------------------------------------------------------------------------
def seen_side(level, axis, line, along):
    """Which face of a gate's wall line looks into the open space or onto the open deck (+1: towards +x / +y)."""
    pl = PL.plan(level)
    for s in (1.0, -1.0):
        px, py = (line + s * 0.25, along) if axis == "x" else (along, line + s * 0.25)
        lab = pl.lab.get(PL.cell_of(px, py))
        if lab and lab[0] in ("open", "air"):
            return s
    return 1.0


def build_gates():
    """Every gate (hub_plan.all_gate_marks: the corridors' mouths the room layout found and the data's own marks) in art. In a wall
    it fills the opening the floors leave for it (door high, the corridor's width; the floors frame it): G1 a barred double door,
    G2 a rope and a board across the way, G4 shutters, G6 an iron grille with the store's goods stacked behind it; on the forecastle's
    front G3 a ladder chained shut. On the deck: the cargo rope (G2), a chained plate at the shrouds (G3), a padlocked hatch (G3).
    G5 (the Keelgate) is out of the hub for now (D70)."""
    p = T.Part("SM_Hub_Gates")
    rng = random.Random(66)
    for gm in PL.all_gate_marks():
        z = H.level_z(gm["level"])
        g, x, y = gm["g"], gm["x"], gm["y"]
        kind, box = H.gate_box(gm)
        if kind == "seal":
            continue
        if kind == "cargo":
            w = box[3]
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
        if kind == "plate":
            # The shrouds' ratlines start out of reach: a chained plate at the bulwark.
            side = 1.0 if y > 0 else -1.0
            yy = side * (H.half_width(x, z + 1.0) - 0.25)
            p.block(x - 0.75, x + 0.75, yy - 0.04, yy + 0.04, z + 0.2, z + 2.5, "MI_Wood_TeakDark", nseed(), axis="z")
            for zz in (z + 0.9, z + 1.7):
                cl = [Vector((x - 0.8 + 1.6 * t, yy - side * 0.06, zz - 0.15 * math.sin(math.pi * t))) for t in (j / 10 for j in range(11))]
                for q in cl:
                    p.beam(T.arc(q, X if int(q.x * 10) % 2 else UP, Y, 0.04, 0.0, 2 * math.pi), 0.25, 0.012, 0.012, "MI_Metal_Iron", nseed(), seg=0.05)
            continue
        if kind == "hatch":
            for (x0_, x1_, y0_, y1_) in ((x - 0.8, x + 0.8, y - 0.8, y - 0.68), (x - 0.8, x + 0.8, y + 0.68, y + 0.8), (x - 0.8, x - 0.68, y - 0.8, y + 0.8),
                                         (x + 0.68, x + 0.8, y - 0.8, y + 0.8)):
                p.block(x0_, x1_, y0_, y1_, z, z + 0.3, "MI_Wood_TeakDark", nseed(), axis="x")
            floor(p, (x - 0.68, x + 0.68, y - 0.68, y + 0.68), z + 0.3, width=0.22, thick=0.06)
            p.block(x + 0.62, x + 0.7, y - 0.1, y + 0.1, z + 0.12, z + 0.3, "MI_Metal_Iron", nseed(), axis="y")
            continue
        # In a wall line.
        if "axis" in gm:
            ax, wdt, hgt = gm["axis"], gm["w"], B.DOOR_H
            ln, along = (x, y) if ax == "x" else (y, x)
        else:
            ax, ln, _, a0, a1 = H.gate_axis(gm)
            wdt, hgt = H.GATE_SIZE[g]
            along = y if ax == "x" else x
            if a1 - a0 >= wdt:
                along = min(max(along, a0 + wdt / 2), a1 - wdt / 2)
        s = seen_side(gm["level"], ax, ln, along)
        n = (X if ax == "x" else Y) * s
        t = Y if ax == "x" else X
        org = Vector((ln, along, z)) if ax == "x" else Vector((along, ln, z))
        F = (org, t, UP, n)
        hw = wdt / 2
        front = WALL * 0.5                                                      # the wall's seen face
        right = n.cross(UP)
        if g == "G1":                                                            # a barred double door
            for k in range(6):
                u = -hw + wdt * (k + 0.5) / 6
                HH.lbeam(p, F, u, 0.02, u, hgt - 0.02, wdt / 6 - 0.015, 0.1, -0.05, rng.choice(("MI_Wood_TeakDark", "MI_Hub_HullTar")), wear=1.0)
            HH.lbeam(p, F, -hw, hgt + 0.25, hw, hgt + 0.25, 0.48, 0.1, -0.05, "MI_Wood_TeakDark", wear=0.9)   # a transom up to a taller frame's head
            for v in (0.45, hgt - 0.45):                                          # the leaves' ledges
                for sg in (-1.0, 1.0):
                    HH.lbeam(p, F, sg * 0.05, v, sg * (hw - 0.08), v, 0.14, 0.04, 0.05, "MI_Wood_TeakDark", wear=0.9)
            for v in (hgt * 0.35, hgt * 0.7):                                     # the bars in their staples
                HH.lbeam(p, F, -hw - 0.3, v, hw + 0.3, v, 0.16, 0.1, front + 0.12, "MI_Wood_Varnish", wear=1.0)
                for u in (-hw - 0.2, hw + 0.2):
                    HH.lbeam(p, F, u, v - 0.12, u, v + 0.12, 0.08, 0.16, front + 0.1, "MI_Metal_Iron", wear=0.6)
            q = HH.lpt(F, 0.3, hgt * 0.35 - 0.16, front + 0.3)                    # the padlock
            p.block(q.x - 0.07, q.x + 0.07, q.y - 0.07, q.y + 0.07, q.z - 0.08, q.z + 0.06, "MI_Metal_Iron", nseed(), axis="z")
            p.beam(T.arc(q + UP * 0.08, right, UP, 0.05, 0.0, math.pi), 0.16, 0.015, 0.015, "MI_Metal_Iron", nseed(), seg=0.05)
        elif g == "G2":                                                          # a rope across the way, a board on it
            for u in (-hw + 0.08, hw - 0.08):
                HH.lbeam(p, F, u, 0.0, u, 1.05, 0.12, 0.12, -0.06, "MI_Wood_Varnish")
            a, b = HH.lpt(F, -hw + 0.08, 0.95, 0.0), HH.lpt(F, hw - 0.08, 0.95, 0.0)
            pts = [a + (b - a) * tt - UP * (0.12 * math.sin(math.pi * tt)) for tt in (j / 8 for j in range(9))]
            f = T.polyline(pts, lambda q: UP, lambda q, n=n: n)
            p.round(f, T.length_of(f), 0.03, "MI_Rope_Hemp", nseed(), sides=6, wear=0.0)
            HH.lbeam(p, F, -0.45, 0.62, 0.45, 0.62, 0.32, 0.03, 0.05, "MI_Paint_CreamAged", wear=0.6)
            sign_text_plane(p, "CLOSED", HH.lpt(F, 0.0, 0.62, 0.085), right, n, 0.13)
        elif g == "G3":                                                          # a ladder up the castle's front, chained shut
            top = Z["fcTop"] - z + 1.0
            F = (org + n * front, t, UP, n)
            for u in (-hw, hw):
                HH.lbeam(p, F, u, 0.0, u, top, 0.08, 0.1, 0.1, "MI_Wood_Varnish")
            v = 0.3
            while v < top - 0.1:
                HH.lbeam(p, F, -hw, v, hw, v, 0.06, 0.06, 0.12, "MI_Wood_Varnish")
                v += 0.3
            for k in range(14):
                tt = k / 13
                q = HH.lpt(F, -hw + wdt * tt, 1.2 + 0.4 * tt, 0.2)
                p.beam(T.arc(q, t if k % 2 else UP, n, 0.035, 0.0, 2 * math.pi), 0.22, 0.012, 0.012, "MI_Metal_Iron", nseed(), seg=0.05)
            q = HH.lpt(F, hw, 1.6, 0.22)
            p.block(q.x - 0.06, q.x + 0.06, q.y - 0.06, q.y + 0.06, q.z - 0.08, q.z + 0.08, "MI_Metal_Iron", nseed(), axis="z")
        elif g == "G4":                                                          # shutters, closed from inside
            for k in range(6):
                u = -hw + wdt * (k + 0.5) / 6
                HH.lbeam(p, F, u, 0.03, u, hgt - 0.03, wdt / 6 - 0.015, 0.06, -0.03, rng.choice(("MI_Paint_Hull", "MI_Paint_HullFaded", "MI_Wood_TeakDark")),
                         wear=1.0)
            for v in (0.3, hgt * 0.5, hgt - 0.3):
                for sg in (-1.0, 1.0):
                    HH.lbeam(p, F, sg * 0.04, v, sg * (hw - 0.06), v, 0.1, 0.03, 0.03, "MI_Wood_TeakDark", wear=0.9)
            HH.lbeam(p, F, -hw - 0.15, hgt * 0.5 + 0.12, hw + 0.15, hgt * 0.5 + 0.12, 0.08, 0.05, front + 0.1, "MI_Metal_Iron")
        elif g == "G6":                                                          # an iron grille, the store's goods behind it
            for k in range(int(wdt / 0.16) + 1):
                u = -hw + 0.06 + k * 0.16
                if u > hw - 0.05:
                    break
                HH.lbeam(p, F, u, 0.0, u, hgt, 0.035, 0.035, -0.02, "MI_Metal_Iron", wear=0.5)
            for v in (0.1, hgt * 0.5, hgt - 0.05):
                HH.lbeam(p, F, -hw, v, hw, v, 0.06, 0.03, 0.02, "MI_Metal_Iron", wear=0.5)
            q = HH.lpt(F, hw - 0.25, hgt * 0.5 - 0.15, 0.1)
            p.block(q.x - 0.06, q.x + 0.06, q.y - 0.06, q.y + 0.06, q.z - 0.08, q.z + 0.06, "MI_Metal_Iron", nseed(), axis="z")
            if gm.get("zone") == "L4":                                           # the timber store: planks stacked across the way
                for k in range(9):
                    v = 0.08 + k * 0.2
                    HH.lbeam(p, F, -hw + rng.uniform(0.0, 0.2), v, hw - rng.uniform(0.0, 0.2), v, 0.18, 0.5, -2.2 - rng.uniform(0.0, 0.3),
                             rng.choice(("MI_Wood_TeakLight", "MI_Wood_Planks", "MI_Wood_Teak")), wear=1.0)
                for k in range(3):
                    v = 0.1 + k * 0.2
                    HH.lbeam(p, F, -hw + 0.1, v, -0.1, v, 0.18, 0.3, -1.1, rng.choice(("MI_Wood_TeakLight", "MI_Wood_Planks")), wear=1.0)
            else:                                                                # the holds: crates, barrels and sacks
                for k, (u0, u1, d0, d1, h_) in enumerate(((-hw + 0.05, -0.1, -2.6, -1.8, 1.6), (0.05, hw - 0.05, -2.6, -1.9, 1.1), (-0.3, 0.5, -2.6, -2.0, 2.1))):
                    q0, q1 = HH.lpt(F, u0, 0.0, d0), HH.lpt(F, u1, h_, d1)
                    p.block(min(q0.x, q1.x), max(q0.x, q1.x), min(q0.y, q1.y), max(q0.y, q1.y), z, z + h_, rng.choice(("MI_Wood_Planks", "MI_Wood_TeakDark")),
                            nseed(), axis="z", wear=1.0)
                for u, d in ((0.45, -1.2), (-0.5, -1.4)):
                    p.lathe([(0.0, 0.0), (0.28, 0.0), (0.33, 0.25), (0.34, 0.45), (0.33, 0.65), (0.28, 0.9), (0.0, 0.9)], HH.lpt(F, u, 0.0, d), UP,
                            "MI_Wood_Teak", sides=12, wear=0.8)
                p.lathe([(0.0, 0.0), (0.26, 0.0), (0.32, 0.2), (0.3, 0.45), (0.2, 0.6), (0.0, 0.66)], HH.lpt(F, 0.0, 0.0, -0.8), UP, "MI_Canvas", sides=10, wear=0.0)
    export(p)


BUILDERS = {"dock": build_dock, "bridge": build_bridge, "slip": build_slip, "tally": build_tally, "well": build_well, "hoist": build_hoist,
            "main": build_main, "chandlery": build_chandlery, "gates": build_gates}

if __name__ == "__main__":
    random.seed(62)
    wanted = sb.script_args() or list(BUILDERS)
    for key in wanted:
        sb.reset_scene()
        BUILDERS[key]()
    print("SB_HUBINSIDE_OK", wanted)
