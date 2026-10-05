"""Every floor of the hub ship (D64): floors, walls with their doors, rails, ceilings and the hull's lining of every deck, built from
the room layout (hub_plan.py), one mesh per floor: SM_Hub_Lvl_<level> (/Game/Saltbound/Hub/Art, ship pivot, Nanite, no collision:
the blockout's SM_HubBlk_Deck_<level> collides under it).

Owner (D64): "Build all the decks and rooms, you don't need to design every room. Make the decks cozy and not too big." The open
spaces and what is seen from them (the Water Hall's galleries, the main deck) get the hand-made kit of build_hub_inside.py (boards
swept one by one, frame posts, skirting, turned balusters, beams, knees); the locked rooms and corridors, seen at most through a
grille, a lighter finish of the same look: planks as flat faces over a dark backing, beams as blocks, plank doors shut.
The designed spaces' own things (quays, stairs, counters, the Keelgate, the Chandlery) are build_hub_inside.py's; the castles'
fronts and weather decks and the hull's outside are the outside's (build_hub_ends.py, build_hub_hull.py).

Run: Tools/Build-BlenderAsset.ps1 build_hub_rooms [level ...]
"""
import math
import os
import random
import sys

from mathutils import Vector

sys.path.insert(0, os.path.dirname(__file__))
import build_hub_blockout as B  # noqa: E402
import build_hub_inside as I  # noqa: E402
import hub_layout as H  # noqa: E402
import hub_plan as PL  # noqa: E402
import hub_timber as T  # noqa: E402
import sb_blender as sb  # noqa: E402

X, Y, UP = Vector((1.0, 0.0, 0.0)), Vector((0.0, 1.0, 0.0)), Vector((0.0, 0.0, 1.0))
SLAB, WALL, DOOR_H, DECK_H = B.SLAB, B.WALL, B.DOOR_H, B.DECK_H
ROW_H = 0.3                    # the light finish's wall planks
LEVEL_ORDER = sorted(H.LEVELS, key=lambda lv: H.level_z(lv))
OUTSIDES = {"poop", "fcTop"}   # floors the outside builds (the castles' weather decks)


def weather(label):
    """Open to the sky: the main deck's waist and the cargo deck, the castles' tops."""
    return label is not None and (label[0] == "air" or (label[0] == "open" and label[1] == "deck"))


def style(plan, c, label):
    """'fine' (the hand-made kit), 'light' (planks as faces) or None (someone else builds it)."""
    if label is None or label[0] in ("water", "void", "slot"):
        return None
    if label[0] == "air" and (plan.level in OUTSIDES or plan.level == "sc2"):
        return None                                   # the quarterdeck, the poop, the forecastle deck: the outside's
    if label[0] in ("open", "air") or c in plan.walk:
        return "fine"
    return "light"


# --- the light finish ---------------------------------------------------------------------------------------------------------
def plank_faces(p, rect, z, facing, rng, mats=I.TEAK, width=I.FLOOR_W):
    """Planks as flat faces fore and aft over rect at z (facing up: a floor; down: a ceiling), over a dark backing."""
    x0, x1, y0, y1 = rect
    back = z - 0.01 if facing.z > 0 else z + 0.01
    p.polygon([(x0, y0, back), (x1, y0, back), (x1, y1, back), (x0, y1, back)], "MI_Caulk_Dark", tone=0.2, facing=tuple(facing))
    y = y0
    while y < y1 - 0.02:
        yb = min(y + width, y1)
        a = x0
        while a < x1 - 1e-6:
            ae = min(a + rng.uniform(2.5, 6.0), x1)
            if x1 - ae < 0.6:
                ae = x1
            p.polygon([(a + 0.006, y + 0.006, z), (ae - 0.006, y + 0.006, z), (ae - 0.006, yb - 0.006, z), (a + 0.006, yb - 0.006, z)],
                      rng.choice(mats), tone=rng.random(), facing=tuple(facing))
            a = ae
        y = yb


def light_wall(p, axis, line, a0, a1, z0, z1, side, openings, rng):
    """A wall face of planks in rows (flat faces over a dark backing), a skirting and a top plate as blocks; openings cut out."""
    face = line + side * WALL * 0.5
    n = (X if axis == "x" else Y) * side

    def at(u, z, d=0.0):
        return (face + side * d, u, z) if axis == "x" else (u, face + side * d, z)
    p.polygon([at(a0, z0, -0.01), at(a1, z0, -0.01), at(a1, z1 + 0.15, -0.01), at(a0, z1 + 0.15, -0.01)], "MI_Caulk_Dark", tone=0.2, facing=tuple(n))
    z = z0
    while z < z1 - 0.02:
        zt = min(z + ROW_H, z1)
        segs = [(a0, a1)]
        for b0, b1, oz0, oz1 in openings:
            if oz0 - 0.01 < 0.5 * (z + zt) < oz1 + 0.01:
                segs = [s for sg in segs for s in ((sg[0], min(sg[1], b0)), (max(sg[0], b1), sg[1])) if s[1] - s[0] > 0.05]
        for sa, sb_ in segs:
            u = sa
            while u < sb_ - 1e-6:
                ue = min(u + rng.uniform(2.0, 4.5), sb_)
                if sb_ - ue < 0.5:
                    ue = sb_
                p.polygon([at(u + 0.005, z + 0.005), at(ue - 0.005, z + 0.005), at(ue - 0.005, zt - 0.005), at(u + 0.005, zt - 0.005)],
                          rng.choice(I.WALL_MATS), tone=rng.random(), facing=tuple(n))
                u = ue
        z = zt
    for b0, b1, oz0, oz1 in openings:                                    # a frame round each opening
        for u in (b0 - 0.06, b1 + 0.06):
            box_on(p, axis, face, side, u - 0.06, u + 0.06, oz0, oz1 + 0.1, 0.06, "MI_Wood_TeakDark")
        box_on(p, axis, face, side, b0 - 0.12, b1 + 0.12, oz1, oz1 + 0.12, 0.06, "MI_Wood_TeakDark")
    segs = [(a0, a1)]
    for b0, b1, oz0, oz1 in openings:
        if oz0 <= z0 + 0.05:
            segs = [s for sg in segs for s in ((sg[0], min(sg[1], b0 - 0.12)), (max(sg[0], b1 + 0.12), sg[1])) if s[1] - s[0] > 0.1]
    for sa, sb_ in segs:
        box_on(p, axis, face, side, sa, sb_, z0, z0 + 0.14, 0.04, "MI_Wood_TeakDark")
    box_on(p, axis, face, side, a0, a1, z1 - 0.12, z1, 0.06, "MI_Wood_TeakDark")


def box_on(p, axis, face, side, u0, u1, z0, z1, depth, mat):
    """A block standing out `depth` from a wall face."""
    lo, hi = sorted((face, face + side * depth))
    if axis == "x":
        p.block(lo, hi, u0, u1, z0, z1, mat, I.nseed(), axis="z")
    else:
        p.block(u0, u1, lo, hi, z0, z1, mat, I.nseed(), axis="z")


def light_rail(p, axis, line, a0, a1, z):
    """A plain rail: posts every 1.6 m under a cap rail."""
    n = max(1, int(round((a1 - a0) / 1.6)))
    for k in range(n + 1):
        u = a0 + (a1 - a0) * k / n
        if axis == "x":
            p.block(line - 0.06, line + 0.06, u - 0.06, u + 0.06, z, z + 1.0, "MI_Wood_TeakDark", I.nseed(), axis="z")
        else:
            p.block(u - 0.06, u + 0.06, line - 0.06, line + 0.06, z, z + 1.0, "MI_Wood_TeakDark", I.nseed(), axis="z")
    if axis == "x":
        p.block(line - 0.08, line + 0.08, a0, a1, z + 0.94, z + 1.04, "MI_Wood_Varnish", I.nseed(), axis="y")
    else:
        p.block(a0, a1, line - 0.08, line + 0.08, z + 0.94, z + 1.04, "MI_Wood_Varnish", I.nseed(), axis="x")


def light_lining(p, side, x0, x1, z0, z1, rng):
    """The hull's inside in planks as faces following its curve, frames every 1.5 m as blocks."""
    def yin(x, z):
        return side * (H.half_width(x, z) - 0.08)
    z = z0
    while z < z1 - 0.02:
        zt = min(z + ROW_H, z1)
        xa = x0
        while xa < x1 - 1e-6:
            xb = min(xa + rng.uniform(2.0, 4.0), x1)
            if x1 - xb < 0.5:
                xb = x1
            xs = [xa + (xb - xa) * k / 4 for k in range(5)]
            mat = rng.choice(I.WALL_MATS)
            tone = rng.random()
            for xx0, xx1 in zip(xs[:-1], xs[1:]):
                p.polygon([(xx0, yin(xx0, z), z), (xx1, yin(xx1, z), z), (xx1, yin(xx1, zt), zt), (xx0, yin(xx0, zt), zt)], mat, tone=tone,
                          facing=(0.0, -side, 0.0))
            xa = xb
        z = zt
    x = x0 + 0.5
    while x < x1 - 0.2:
        pts = [Vector((x, yin(x, zz) - side * 0.1, zz)) for zz in [z0 + (z1 - z0) * k / 4 for k in range(5)]]
        f = T.polyline(pts, lambda q: X, lambda q, s=side: Vector((0.0, -s, 0.0)))
        p.beam(f, T.length_of(f), 0.2, 0.14, "MI_Wood_TeakDark", I.nseed(), seg=0.7, wear=0.6)
        x += 1.5


def light_ceiling(p, rect, z_top, rng):
    zb = z_top - SLAB
    plank_faces(p, rect, zb, -UP, rng)
    x0, x1, y0, y1 = rect
    n = max(1, int(round((x1 - x0) / I.BEAM_EVERY)))
    for k in range(n + 1):
        x = min(max(x0 + (x1 - x0) * k / n, x0 + 0.12), x1 - 0.12)
        p.block(x - 0.12, x + 0.12, y0, y1, zb - 0.24, zb, "MI_Wood_TeakDark", I.nseed(), axis="y")


def door_leaf(p, d, z):
    """A plank door shut in its opening (the wall's middle), two iron straps."""
    hw = d["w"] / 2 - 0.02
    if d["axis"] == "x":
        p.block(d["x"] - 0.04, d["x"] + 0.04, d["y"] - hw, d["y"] + hw, z + 0.01, z + DOOR_H - 0.02, "MI_Wood_TeakDark", I.nseed(), axis="z")
        for zz in (z + 0.4, z + DOOR_H - 0.45):
            p.block(d["x"] - 0.06, d["x"] + 0.06, d["y"] - hw, d["y"] + hw * 0.4, zz - 0.04, zz + 0.04, "MI_Metal_Iron", I.nseed(), axis="y")
    else:
        p.block(d["x"] - hw, d["x"] + hw, d["y"] - 0.04, d["y"] + 0.04, z + 0.01, z + DOOR_H - 0.02, "MI_Wood_TeakDark", I.nseed(), axis="z")
        for zz in (z + 0.4, z + DOOR_H - 0.45):
            p.block(d["x"] - hw, d["x"] + hw * 0.4, d["y"] - 0.06, d["y"] + 0.06, zz - 0.04, zz + 0.04, "MI_Metal_Iron", I.nseed(), axis="x")


# --- a floor ---------------------------------------------------------------------------------------------------------------
def above(level_id, c):
    """What is over cell c of a level: (z of the floor above, its plan) when a floor closes it, else None (open to the sky)."""
    k = LEVEL_ORDER.index(level_id)
    for lv in LEVEL_ORDER[k + 1:]:
        q = PL.plan(lv)
        if c in q.lab:
            return q.z
        if c in q.void or c in q.slot:
            continue
        if H.LEVELS[lv].get("only") and not any(H.inside(H.R(r), PL.cx(c[0]), PL.cy(c[1])) for r in H.LEVELS[lv]["only"]):
            continue
        return None
    return None


def windows(level_id):
    """Windows the spaces' data cuts into their walls (a prop with `wall` 'x=..' / 'y=..', `spans` and z0..z1 over the floor):
    {(axis, line): [(b0, b1, z0, z1)]} in absolute z."""
    out = {}
    z = H.level_z(level_id)
    for name, sp in H.SPACES.items():
        if sp.get("level") != level_id:
            continue
        for pr in sp.get("props", []):
            if "wall" in pr and "spans" in pr and "z0" in pr:
                ax, val = pr["wall"].split("=")
                for b0, b1 in pr["spans"]:
                    out.setdefault((ax, round(float(val), 3)), []).append((b0, b1, z + pr["z0"], z + pr["z1"]))
    return out


def under_keelgate(level_id, c):
    """The Keelgate's stone ring and apron stand in the chamber's floor (build_hub_inside builds them): no boards there."""
    if level_id != "quay":
        return False
    kg = H.HUB["keelgate"]
    r = kg["ring"] + 0.3
    return abs(PL.cx(c[0]) - kg["x"]) < r and abs(PL.cy(c[1]) - kg["y"]) < r


def floor_style(label):
    if label[0] == "open" and label[1] in ("dock", "dock_g"):
        return dict(mats=I.DOCK, width=I.QUAY_W)
    return dict(mats=I.TEAK, width=I.FLOOR_W)


def build_level(level_id):
    plan = PL.plan(level_id)
    z = plan.z
    rng = random.Random(hash(level_id) & 0xffff)
    p = T.Part("SM_Hub_Lvl_" + level_id)
    # Floors.
    groups = {}
    for c, l in plan.lab.items():
        st = style(plan, c, l)
        if st is None or under_keelgate(level_id, c):
            continue
        key = (st, l[1] if st == "fine" and l[0] == "open" else None)
        groups.setdefault(key, []).append(c)
    for (st, zone), cells in groups.items():
        for r in B.rects_of(cells):
            if st == "fine":
                kw = floor_style(("open", zone)) if zone else dict(mats=I.TEAK, width=I.FLOOR_W)
                I.floor(p, r, z, mats=kw["mats"], width=kw["width"])
            else:
                plank_faces(p, r, z, UP, rng)
    # Walls, doors, rails.
    openings = {}
    for d in plan.doors:
        key = (d["axis"], round(d["x"] if d["axis"] == "x" else d["y"], 3))
        mid = d["y"] if d["axis"] == "x" else d["x"]
        openings.setdefault(key, []).append((mid - d["w"] / 2, mid + d["w"] / 2, z, z + DOOR_H))
        if d["kind"] in ("room", "zone"):
            door_leaf(p, d, z)
    for g in plan.gates:
        key = (g["axis"], round(g["x"] if g["axis"] == "x" else g["y"], 3))
        mid = g["y"] if g["axis"] == "x" else g["x"]
        openings.setdefault(key, []).append((mid - g["w"] / 2, mid + g["w"] / 2, z, z + DOOR_H))
    for key, ws in windows(level_id).items():
        openings.setdefault(key, []).extend(ws)
    top = z + DECK_H - SLAB
    for w in plan.walls:
        axis, line, a0, a1 = w["axis"], w["line"], w["a0"], w["a1"]
        if w["kind"] in ("hull", "bulwark"):
            continue                                                 # the lining below; the bulwarks are the main deck's
        cl, cr = side_cells(axis, line, a0, a1)
        if w["kind"] == "rail":
            lab = w["left"] if w["left"] not in (("void",), ("slot",)) else w["right"]
            c_ = cl if w["left"] is lab else cr
            st = style(plan, c_, lab)
            if shaft_edge(level_id, w):
                st = None                                            # the Well's coaming and rails on the main deck: build_main's
            if st == "fine":
                I.rail(p, axis, line, a0, a1, z)
            elif st == "light":
                light_rail(p, axis, line, a0, a1, z)
            fascia(p, axis, line, a0, a1, z, toward_void=(-1.0 if w["left"] in (("void",), ("slot",)) else 1.0))
            continue
        ops = [o for o in openings.get((axis, round(line, 3)), []) if o[1] > a0 and o[0] < a1]
        for lab, c_, s in ((w["left"], cl, -1.0), (w["right"], cr, 1.0)):
            if lab is None or weather(lab):
                continue
            if lab == ("void",) and level_id in ("hold", "deep"):
                continue                                             # behind the quays and the slipway's sides (build_hub_inside)
            st = "fine" if lab == ("void",) else style(plan, c_, lab)       # a wall's face over a void is seen from the hall
            if st == "fine":
                I.wall(p, axis, line, a0, a1, z, top, s, ops)
            elif st == "light":
                light_wall(p, axis, line, a0, a1, z, top, s, ops, rng)
    # The hull's lining where the floor meets it under a deck (not on the weather decks).
    for side in (1.0, -1.0):
        runs = []
        for i in range(PL.NX):
            js = [j for (ii, j) in ((i, j) for j in range(PL.NY)) if (ii, j) in plan.lab]
            if not js:
                runs.append(None)
                continue
            j = max(js) if side > 0 else min(js)
            l = plan.lab[(i, j)]
            if weather(l) or above(level_id, (i, j)) is None:
                runs.append(None)
                continue
            runs.append(style(plan, (i, j), l))
        i = 0
        while i < PL.NX:
            st = runs[i]
            if st is None:
                i += 1
                continue
            k = i
            while k < PL.NX and runs[k] == st:
                k += 1
            x0, x1 = PL.X0 + i * PL.CELL, PL.X0 + k * PL.CELL
            if st == "fine":
                I.hull_lining(p, side, x0, x1, z, top)
            else:
                light_lining(p, side, x0, x1, z, top, rng)
            i = k
    # Ceilings: under whatever floor closes this level from above, over its floors, water and slots (a void's ceiling is the
    # floor's below it, which looks up through it: drawn once).
    cells = {}
    for c in list(plan.lab) + list(plan.slot):
        zt = above(level_id, c)
        if zt is None:
            continue
        lab = plan.lab.get(c)
        st = "fine" if lab is None or lab[0] == "water" else style(plan, c, lab)
        if lab is not None and weather(lab):
            st = None
        if st:
            cells.setdefault((zt, st), []).append(c)
    for (zt, st), cs in cells.items():
        for r in B.rects_of(cs):
            if st == "fine":
                I.ceiling(p, r, zt, knees=False)
            else:
                light_ceiling(p, r, zt, rng)
    I.export(p)


def shaft_edge(level_id, w):
    """Is this wall run the Well's edge on the main deck?"""
    if level_id != "main":
        return False
    sh = H.R(H.HUB["shaft"])
    lines, span = (sh[:2], sh[2:]) if w["axis"] == "x" else (sh[2:], sh[:2])
    return any(abs(w["line"] - v) < 1e-6 for v in lines) and span[0] - 1e-6 <= w["a0"] and w["a1"] <= span[1] + 1e-6


def side_cells(axis, line, a0, a1):
    """A cell on each side of a wall run (at its middle)."""
    mid = 0.5 * (a0 + a1)
    if axis == "x":
        i = int(round((line - PL.X0) / PL.CELL))
        j = int(math.floor((mid - PL.Y0) / PL.CELL))
        return (i - 1, j), (i, j)
    j = int(round((line - PL.Y0) / PL.CELL))
    i = int(math.floor((mid - PL.X0) / PL.CELL))
    return (i, j - 1), (i, j)


def fascia(p, axis, line, a0, a1, z, toward_void):
    """The floor's edge over a void: a beam along it, its face where the slab ends."""
    if axis == "x":
        x = line + toward_void * 0.0
        p.block(min(x, x + toward_void * 0.25), max(x, x + toward_void * 0.25), a0, a1, z - SLAB - 0.15, z - 0.02, "MI_Wood_TeakDark", I.nseed(), axis="y")
    else:
        y = line
        p.block(a0, a1, min(y, y + toward_void * 0.25), max(y, y + toward_void * 0.25), z - SLAB - 0.15, z - 0.02, "MI_Wood_TeakDark", I.nseed(), axis="x")


if __name__ == "__main__":
    wanted = sb.script_args() or [lv for lv in LEVEL_ORDER if lv not in OUTSIDES]
    for lv in wanted:
        sb.reset_scene()
        build_level(lv)
    print("SB_HUBROOMS_OK", wanted)
