"""The hub ship's blockout (D61; rebuilt for the D64 layout): plain shapes and flat colours to walk, sail and measure the
design, and under the art the hidden collision (L_HubReview, L_Hub: the art draws, these collide).

Every number comes from Docs/Design/HubShip/HubShip_Data.json through hub_layout.py; the floors, walls, doors and gates of every
deck come from hub_plan.py (the D64 room layout). The few build choices that are not in the data are named constants below
(slab and wall thickness) or come from the prompt's scale rules (doors 2.2 m, rails 1.0 m).

Meshes (pivot at the ship's x 80 on the waterline, bow to +X; /Game/Saltbound/Hub/Blockout):
  SM_HubBlk_ShellStern / ShellWaistS / ShellWaistP / ShellBow   the hull's skin (complex collision); open at the hidden doors
  SM_HubBlk_Dock           the Water Hall's water floor, the quays' faces and the stone under them, the slipway (complex)
  SM_HubBlk_Deck_<level>   one per floor: its slabs, walls (door high openings), rails, the hull's lining (UCX boxes)
  SM_HubBlk_Stairs         the Well's five flights and the dock stair: visual steps, ramp collision; D69 the grand stairs up
                           the sterncastle (solid, a ramp to walk) and the rails along the quarterdeck's and the poop's edges
  SM_HubBlk_Gates          every gate (hub_plan.all_gate_marks) as a marked block (G5: the Keelgate)
  SM_HubBlk_Props          the designed spaces' props as boxes, the hoist's guide posts, the gantry, the main deck's things
  SM_HubBlk_Masts          the three masts with tops, crosstrees, yards and furled sails
  SM_HubBlk_Markers        NPC spots, interaction points, the berths (no collision)

Run: Tools/Build-BlenderAsset.ps1 build_hub_blockout [shell dock decks stairs gates props masts markers]
"""
import math
import os
import sys

import bmesh
import bpy
from mathutils import Vector

sys.path.insert(0, os.path.dirname(__file__))
import hub_layout as H  # noqa: E402
import hub_plan as PL  # noqa: E402
import sb_blender as sb  # noqa: E402

SLAB = H.HUB["slab"]          # floor slab thickness (its top is the floor's z)
WALL = 0.3                    # wall thickness (centred on the cell edge)
RAIL_H = 1.0                  # rails round voids and galleries
RAIL_T = 0.12
LINING_T = 0.2                # the hull's lining at a floor's edge (stops the capsule slipping between floor and skin)
DOOR_H = 2.2                  # prompt section 3: doors 2.2 m
DECK_H = H.HUB["deckH"]
RISERS = 17                   # 3 m storeys: 17 risers of 17.6 cm
TREAD = 0.30
RISER = DECK_H / RISERS
Z = H.Z

COLOURS = {
    "MI_HubBlk_Open": (0.78, 0.56, 0.3), "MI_HubBlk_Wall": (0.86, 0.75, 0.56), "MI_HubBlk_Locked": (0.45, 0.45, 0.47),
    "MI_HubBlk_Stone": (0.86, 0.85, 0.79), "MI_HubBlk_Hull": (0.24, 0.16, 0.1), "MI_HubBlk_Prop": (0.5, 0.33, 0.19),
    "MI_HubBlk_Rail": (0.32, 0.2, 0.11), "MI_HubBlk_Gate": (0.75, 0.12, 0.1), "MI_HubBlk_Seal": (0.35, 0.85, 0.78),
    "MI_HubBlk_MarkNPC": (0.85, 0.15, 0.75), "MI_HubBlk_MarkE": (0.1, 0.8, 0.95), "MI_HubBlk_MarkBerth": (0.95, 0.85, 0.1),
    "MI_HubBlk_Canvas": (0.8, 0.76, 0.66),
}


def bl(p):
    """Ship metres -> Blender (hub_layout.to_blender)."""
    return Vector(H.to_blender(*p))


class Part:
    """One mesh: faces with material slots, and UCX hulls for its collision (unless it collides complex)."""

    def __init__(self, name, ucx=True):
        self.name = name
        self.ucx_on = ucx
        self.bm = bmesh.new()
        self.slots = []
        self.ucx = []

    def slot(self, mat):
        if mat not in self.slots:
            self.slots.append(mat)
        return self.slots.index(mat)

    def _merge(self, src, mat):
        idx = self.slot(mat)
        vmap = {v: self.bm.verts.new(v.co) for v in src.verts}
        for f in src.faces:
            try:
                nf = self.bm.faces.new([vmap[v] for v in f.verts])
            except ValueError:
                continue
            nf.material_index = idx
        src.free()

    def box(self, x0, x1, y0, y1, z0, z1, mat, collide=True):
        """An axis-aligned box in ship metres."""
        if x1 - x0 < 1e-3 or y1 - y0 < 1e-3 or z1 - z0 < 1e-3:
            return
        a, b = bl((x0, y1, z0)), bl((x1, y0, z1))       # Blender min / max (y mirrored)
        tmp = bmesh.new()
        bmesh.ops.create_cube(tmp, size=1.0)
        for v in tmp.verts:
            v.co = Vector((a.x + (v.co.x + 0.5) * (b.x - a.x), a.y + (v.co.y + 0.5) * (b.y - a.y), a.z + (v.co.z + 0.5) * (b.z - a.z)))
        if collide:
            self.ucx.append([v.co.copy() for v in tmp.verts])
        self._merge(tmp, mat)

    def hull(self, pts, mat, collide=True):
        """A convex solid round points in ship metres (ramps, slanted boards, poles)."""
        tmp = bmesh.new()
        for p in pts:
            tmp.verts.new(bl(p))
        res = bmesh.ops.convex_hull(tmp, input=list(tmp.verts))
        bmesh.ops.delete(tmp, geom=[g for g in res["geom_interior"] + res["geom_unused"] if isinstance(g, bmesh.types.BMVert)], context="VERTS")
        bmesh.ops.recalc_face_normals(tmp, faces=tmp.faces)
        if collide:
            self.ucx.append([v.co.copy() for v in tmp.verts])
        self._merge(tmp, mat)

    def quad(self, pts, mat):
        """A face through points in ship metres (open surfaces of complex meshes; the materials are two-sided)."""
        idx = self.slot(mat)
        vs = [self.bm.verts.new(bl(p)) for p in pts]
        try:
            f = self.bm.faces.new(vs)
            f.material_index = idx
        except ValueError:
            pass

    def finish(self):
        bmesh.ops.remove_doubles(self.bm, verts=self.bm.verts, dist=0.0005)
        bmesh.ops.dissolve_degenerate(self.bm, edges=self.bm.edges, dist=0.0005)
        me = bpy.data.meshes.new(self.name)
        self.bm.normal_update()
        self.bm.to_mesh(me)
        self.bm.free()
        obj = sb.new_object(self.name, me)
        for s in self.slots:
            obj.data.materials.append(sb.get_material(s, (*COLOURS[s], 1.0)))
        sb.world_box_uv(obj, 1.0)
        objs = [obj]
        if self.ucx_on:
            for k, pts in enumerate(self.ucx):
                hb_ = bmesh.new()
                for p in pts:
                    hb_.verts.new(p)
                bmesh.ops.convex_hull(hb_, input=list(hb_.verts))
                hm = bpy.data.meshes.new("UCX_%s_%03d" % (self.name, k))
                hb_.to_mesh(hm)
                hb_.free()
                u = sb.new_object("UCX_%s_%03d" % (self.name, k), hm)
                u.display_type = "WIRE"
                objs.append(u)
        print("SB_HUBBLK", self.name, "faces", len(me.polygons), "ucx", len(self.ucx) if self.ucx_on else "complex")
        sb.save_blend(self.name)
        sb.export_fbx(objs, os.path.join("Hub", self.name + ".fbx"))
        return obj


# --- rectangles ----------------------------------------------------------------------------------------------------
def subtract(rect, holes):
    """A rectangle minus holes, as rectangles (guillotine cuts)."""
    out = [rect]
    for h in holes:
        nxt = []
        for r in out:
            x0, x1, y0, y1 = r
            hx0, hx1, hy0, hy1 = max(x0, h[0]), min(x1, h[1]), max(y0, h[2]), min(y1, h[3])
            if hx0 >= hx1 or hy0 >= hy1:
                nxt.append(r)
                continue
            if x0 < hx0:
                nxt.append((x0, hx0, y0, y1))
            if hx1 < x1:
                nxt.append((hx1, x1, y0, y1))
            if y0 < hy0:
                nxt.append((hx0, hx1, y0, hy0))
            if hy1 < y1:
                nxt.append((hx0, hx1, hy1, y1))
        out = nxt
    return out


def slab(part, rect, z, mat, holes=(), thick=SLAB):
    for r in subtract(rect, holes):
        part.box(r[0], r[1], r[2], r[3], z - thick, z, mat)


def rects_of(cells):
    """Cells merged into rectangles (hub_plan.rects_of)."""
    return PL.rects_of(cells)


# --- the shell --------------------------------------------------------------------------------------------------------
def knots():
    ks = {float(z) for z in range(-12, 32, 1)} | {H.FORM["stern"]["counter"], H.FORM["stern"]["counter"] - 0.05}
    for z in Z.values():
        ks.add(float(z) + H.HUB["rail"])
    return sorted(ks)


KNOTS = knots()
DOORS = H.HUB["doors"]


def ring(x):
    """The hull's section at x: [(y, z)] from the starboard top down to the keel's middle and up to the port top."""
    zb, zt = H.profile_bottom(x), H.profile_top(x)
    zs = sorted({min(max(k, zb), zt) for k in KNOTS})
    stbd = [(H.half_width(x, z), z) for z in reversed(zs)]
    port = [(-H.half_width(x, z), z) for z in zs]
    return stbd + [(0.0, zb)] + port


def in_door(x, z):
    """In one of the hidden doors' openings (D70: a station at each basin)."""
    return DOORS["z0"] < z < DOORS["z1"] and any(a < x < b for a, b in H.door_stations())


def shell(part, x0, x1, side=None):
    """Skin the sections between x0 and x1 (side: None both, +1 starboard half, -1 port half); open at the hidden doors."""
    edges = {e for st in H.door_stations() for e in st}
    xs = sorted({x0, x1} | edges | {float(x) for x in range(int(math.ceil(x0)), int(math.floor(x1)) + 1)})
    xs = [x for x in xs if x0 <= x <= x1]
    rings = [ring(x) for x in xs]
    for (xa, ra), (xb, rb) in zip(zip(xs, rings), zip(xs[1:], rings[1:])):
        n = min(len(ra), len(rb))
        mid = n // 2
        ks = range(0, n - 1) if side is None else (range(0, mid) if side > 0 else range(mid, n - 1))
        for k in ks:
            (ya0, za0), (ya1, za1) = ra[k], ra[k + 1]
            (yb0, zb0), (yb1, zb1) = rb[k], rb[k + 1]
            zc = 0.25 * (za0 + za1 + zb0 + zb1)
            if in_door(0.5 * (xa + xb), zc):
                continue
            part.quad([(xa, ya0, za0), (xb, yb0, zb0), (xb, yb1, zb1), (xa, ya1, za1)], "MI_HubBlk_Hull")


def build_shell():
    """The skin in four parts; the stern and the bow close themselves (the form narrows to nothing there). The castles' fronts
    and decks are the floors' walls (Deck_* parts)."""
    p = Part("SM_HubBlk_ShellStern", ucx=False)
    shell(p, -1.0, H.HUB["qdX"] + 0.6)
    rd = H.HUB["rudder"]
    p.hull([(rd["x0"], -0.3, rd["z0"]), (rd["x1"], -0.3, rd["z0"]), (rd["x0"], 0.3, rd["z0"]), (rd["x1"], 0.3, rd["z0"]),
            (rd["x1"] - rd["head"], -0.3, rd["z1"]), (rd["x1"], -0.3, rd["z1"]), (rd["x1"] - rd["head"], 0.3, rd["z1"]), (rd["x1"], 0.3, rd["z1"])],
           "MI_HubBlk_Hull", collide=False)
    for cb in H.HUB["cables"]:
        if cb["id"] == "stern":
            pole(p, cb["from"], cb["to"], 0.22, 0.22, "MI_HubBlk_Rail", sides=6)
    p.finish()
    for side, name in ((1, "SM_HubBlk_ShellWaistS"), (-1, "SM_HubBlk_ShellWaistP")):
        sb.reset_scene()
        q = Part(name, ucx=False)
        shell(q, H.HUB["qdX"] + 0.6, H.HUB["fcX"] + 0.6, side)
        q.finish()
    sb.reset_scene()
    b = Part("SM_HubBlk_ShellBow", ucx=False)
    shell(b, H.HUB["fcX"] + 0.6, 161.5)
    sp = H.prop("main_deck", "bowsprit")
    pole(b, sp["from"], sp["to"], 0.9, 0.4, "MI_HubBlk_Prop")
    bs_ = H.prop("main_deck", "bobstay")
    pole(b, bs_["from"], bs_["to"], 0.12, 0.12, "MI_HubBlk_Rail", sides=6)
    for cb in H.HUB["cables"]:
        if cb["id"] == "bow":
            pole(b, cb["from"], cb["to"], 0.25, 0.25, "MI_HubBlk_Rail", sides=6)
    b.finish()


def pole(part, a, b, r0, r1, mat, sides=8, collide=True):
    """A tapered pole from a to b (ship metres) as a convex solid."""
    a, b = Vector(a), Vector(b)
    d = (b - a).normalized()
    u = d.cross(Vector((0.0, 0.0, 1.0)) if abs(d.z) < 0.9 else Vector((1.0, 0.0, 0.0))).normalized()
    v = d.cross(u)
    pts = []
    for k in range(sides):
        t = 2 * math.pi * k / sides
        off = u * math.cos(t) + v * math.sin(t)
        pts += [tuple(a + off * r0), tuple(b + off * r1)]
    part.hull(pts, mat, collide)


# --- the harbour (D70) -----------------------------------------------------------------------------------------------------
QUAY_FACE = 0.4                                   # the quays' faces: this thick, on the quay's side of the water's edge


def build_dock():
    """The harbour's water bed (the canal and the basins), the quays' faces (from the bed up to the quay deck: the boats meet
    them) and the fixed footbridges over the canal. The lifting bridges are actors of their own (they move)."""
    p = Part("SM_HubBlk_Dock", ucx=False)
    fl = H.HUB["dock"]["floor"]
    zq = Z["quay"]
    for x0, x1, y0, y1 in H.water_rects():
        xs = [x0 + k * 0.5 for k in range(int(round((x1 - x0) / 0.5)) + 1)]
        for xa, xb in zip(xs[:-1], xs[1:]):
            hw = min(H.half_width(xa, fl), H.half_width(xb, fl)) - 0.05
            ya, yb = max(y0, -hw), min(y1, hw)
            if yb > ya:
                p.quad([(xa, ya, fl), (xb, ya, fl), (xb, yb, fl), (xa, yb, fl)], "MI_HubBlk_Stone")
    for axis, line, a0, a1, side in PL.water_edges():
        t0, t1 = sorted((line, line + side * QUAY_FACE))
        if axis == "x":
            p.box(t0, t1, a0, a1, fl, zq - SLAB, "MI_HubBlk_Stone")
        else:
            p.box(a0, a1, t0, t1, fl, zq - SLAB, "MI_HubBlk_Stone")
    for x0, x1, y0, y1 in H.fixed_bridges():
        p.box(x0, x1, y0 - 0.3, y1 + 0.3, zq - 0.3, zq, "MI_HubBlk_Open")
        for xr in (x0, x1 - RAIL_T):
            p.box(xr, xr + RAIL_T, y0, y1, zq, zq + RAIL_H, "MI_HubBlk_Rail")
    p.finish()


# --- the floors: slabs, walls, rails ------------------------------------------------------------------------------------------
def floor_mat(label):
    return "MI_HubBlk_Open" if label[0] == "open" else "MI_HubBlk_Locked"


def build_deck(level_id):
    plan = PL.plan(level_id)
    z = plan.z
    p = Part("SM_HubBlk_Deck_" + level_id)
    by_mat = {}
    for c, l in plan.lab.items():
        if l[0] == "water":
            continue
        by_mat.setdefault(floor_mat(l), []).append(c)
    for mat, cells in by_mat.items():
        for r in rects_of(cells):
            p.box(r[0], r[1], r[2], r[3], z - SLAB, z, mat)
    top = z + DECK_H - SLAB
    # Openings on each wall line: doors (2.2 m high, a lintel over them) and gates (the gate's blocker fills them).
    openings = {}
    for d in plan.doors:
        key = (d["axis"], round(d["x"] if d["axis"] == "x" else d["y"], 3))
        mid = d["y"] if d["axis"] == "x" else d["x"]
        openings.setdefault(key, []).append((mid - d["w"] / 2, mid + d["w"] / 2))
    for g in plan.gates:
        key = (g["axis"], round(g["x"] if g["axis"] == "x" else g["y"], 3))
        mid = g["y"] if g["axis"] == "x" else g["x"]
        openings.setdefault(key, []).append((mid - g["w"] / 2, mid + g["w"] / 2))
    # D69: a gate mark of the data standing in one of this floor's walls (the command hall's doors) gets its doorway too.
    for gm in H.GATE_MARKS:
        if gm["level"] != level_id or gm["g"] != "G1":
            continue
        found = H.gate_axis(gm)
        if found:
            ax, ln = found[0], found[1]
            mid = gm["y"] if ax == "x" else gm["x"]
            w = H.GATE_SIZE["G1"][0] + 0.4
            openings.setdefault((ax, round(ln, 3)), []).append((mid - w / 2, mid + w / 2))
    lines = {}
    for w in plan.walls:
        if w["kind"] == "bulwark":
            continue                                                 # the shell's own top
        lines.setdefault((w["kind"], w["axis"], round(w["line"], 3)), []).append((w["a0"], w["a1"]))
    for (kind, axis, line), spans in lines.items():
        for a0, a1 in merge(spans):
            if kind == "rail":
                wall_box(p, axis, line, a0, a1, z, z + RAIL_H, "MI_HubBlk_Rail", RAIL_T)
                continue
            if kind == "hull":
                wall_box(p, axis, line, a0, a1, z, top, "MI_HubBlk_Hull", LINING_T)
                continue
            holes = [(max(o0, a0), min(o1, a1)) for o0, o1 in openings.get((axis, line), []) if o1 > a0 and o0 < a1]
            for s0, s1 in cut(a0, a1, holes):
                wall_box(p, axis, line, s0, s1, z, top, "MI_HubBlk_Wall", WALL)
            for h0, h1 in holes:
                wall_box(p, axis, line, h0, h1, z + DOOR_H, top, "MI_HubBlk_Wall", WALL)
    p.finish()


def merge(spans):
    spans = sorted(spans)
    out = [list(spans[0])]
    for a0, a1 in spans[1:]:
        if a0 <= out[-1][1] + 1e-6:
            out[-1][1] = max(out[-1][1], a1)
        else:
            out.append([a0, a1])
    return [tuple(s) for s in out]


def cut(a0, a1, holes):
    out = []
    s = a0
    for h0, h1 in sorted(holes):
        if h0 > s:
            out.append((s, h0))
        s = max(s, h1)
    if s < a1:
        out.append((s, a1))
    return out


def wall_box(part, axis, line, a0, a1, z0, z1, mat, thick):
    if axis == "x":
        part.box(line - thick / 2, line + thick / 2, a0, a1, z0, z1, mat)
    else:
        part.box(a0, a1, line - thick / 2, line + thick / 2, z0, z1, mat)


def build_decks():
    for lv in H.LEVELS:
        sb.reset_scene()
        build_deck(lv)


# --- stairs --------------------------------------------------------------------------------------------------------------
def flight(part, x0, x1, y0, y1, up, z0, z1, rails=True):
    """A flight on its rectangle rising along `up` ('+x', '-x', '+y', '-y'): steps from the bottom end, the rest a landing at
    the top; visual steps, a ramp to walk (UCX)."""
    n = RISERS
    run = n * TREAD
    length = (x1 - x0) if up[1] == "x" else (y1 - y0)
    sign = 1.0 if up[0] == "+" else -1.0
    start = (x0 if sign > 0 else x1) if up[1] == "x" else (y0 if sign > 0 else y1)

    def at(t):
        return start + sign * t
    rise = (z1 - z0) / n
    for k in range(n):
        a, b = sorted((at(k * TREAD), at((k + 1) * TREAD)))
        top = z0 + (k + 1) * rise
        if up[1] == "x":
            part.box(a, b, y0, y1, z0, top, "MI_HubBlk_Open", collide=False)
        else:
            part.box(x0, x1, a, b, z0, top, "MI_HubBlk_Open", collide=False)
    a, b = sorted((at(run), at(length)))
    lo, hi = at(0.0), at(run)
    if up[1] == "x":
        part.box(a, b, y0, y1, z0, z1, "MI_HubBlk_Open")
        part.hull([(lo, y0, z0), (lo, y1, z0), (hi, y0, z0), (hi, y1, z0), (hi, y0, z1), (hi, y1, z1), (lo, y0, z0 + 0.02), (lo, y1, z0 + 0.02)],
                  "MI_HubBlk_Open")
        sides = [("y", yy) for yy in (y0, y1)]
    else:
        part.box(x0, x1, a, b, z0, z1, "MI_HubBlk_Open")
        part.hull([(x0, lo, z0), (x1, lo, z0), (x0, hi, z0), (x1, hi, z0), (x0, hi, z1), (x1, hi, z1), (x0, lo, z0 + 0.02), (x1, lo, z0 + 0.02)],
                  "MI_HubBlk_Open")
        sides = [("x", xx) for xx in (x0, x1)]
    if rails:
        t = RAIL_T / 2
        for ax, c in sides:
            if ax == "y":
                part.hull([(lo, c - t, z0), (lo, c + t, z0), (lo, c - t, z0 + RAIL_H), (lo, c + t, z0 + RAIL_H),
                           (hi, c - t, z1), (hi, c + t, z1), (hi, c - t, z1 + RAIL_H), (hi, c + t, z1 + RAIL_H)], "MI_HubBlk_Rail")
            else:
                part.hull([(c - t, lo, z0), (c + t, lo, z0), (c - t, lo, z0 + RAIL_H), (c + t, lo, z0 + RAIL_H),
                           (c - t, hi, z1), (c + t, hi, z1), (c - t, hi, z1 + RAIL_H), (c + t, hi, z1 + RAIL_H)], "MI_HubBlk_Rail")


def grand_flight(part, g):
    """D69: one grand flight (H.grand_flights): solid from the deck up to its steps (nobody stands under it), visual steps, a ramp
    to walk, a rail on its open side (towards the deck's middle); its other side is the bulwark."""
    x0, x1, y0, y1, z0, z1, n = g["x0"], g["x1"], g["y0"], g["y1"], g["z0"], g["z1"], g["n"]
    for k in range(n - 1):
        top = z0 + (k + 1) * g["rise"]
        part.box(x1 - (k + 1) * g["tread"], x1 - k * g["tread"], y0, y1, z0, top, "MI_HubBlk_Open", collide=False)
    part.hull([(x1, y0, z0), (x1, y1, z0), (x0, y0, z0), (x0, y1, z0), (x0, y0, z1), (x0, y1, z1), (x1, y0, z0 + 0.02), (x1, y1, z0 + 0.02)],
              "MI_HubBlk_Open")
    yi = y0 if g["side"] > 0 else y1
    t = RAIL_T / 2
    part.hull([(x1, yi - t, z0), (x1, yi + t, z0), (x1, yi - t, z0 + RAIL_H), (x1, yi + t, z0 + RAIL_H),
               (x0, yi - t, z1), (x0, yi + t, z1), (x0, yi - t, z1 + RAIL_H), (x0, yi + t, z1 + RAIL_H)], "MI_HubBlk_Rail")


def edge_rails(part):
    """D69: rails along the forward edges of the open decks the grand stairs reach (the quarterdeck's at qdX, the poop's at
    poopX), open where the flights arrive."""
    for gid in sorted({g["id"] for g in H.grand_flights()}):
        fl = [g for g in H.grand_flights() if g["id"] == gid]
        x, z = fl[0]["x0"], fl[0]["z1"]
        w = H.half_width(x - 0.2, z + 0.5) - 0.2
        cuts = sorted((g["y0"] - 0.05, g["y1"] + 0.05) for g in fl)
        y = -w
        for a, b in cuts + [(w, w)]:
            if a - y > 0.2:
                part.box(x - 0.2 - RAIL_T, x - 0.2, y, a, z, z + RAIL_H, "MI_HubBlk_Rail")
            y = max(y, b)


def build_stairs():
    p = Part("SM_HubBlk_Stairs")
    st = H.HUB["stair"]
    flight(p, st["x0"], st["x1"], st["y0"], st["y1"], st.get("up", "+x"), Z["quay"], Z["lower"])     # the dock stair (in the shop)
    for f in H.flights():
        flight(p, f["x0"], f["x1"], f["y0"], f["y1"], f["up"], Z[f["from"]], Z[f["to"]])
    for g in H.grand_flights():
        grand_flight(p, g)
    edge_rails(p)
    p.finish()


# --- gates ---------------------------------------------------------------------------------------------------------------
def build_gates():
    p = Part("SM_HubBlk_Gates")
    for gm in PL.all_gate_marks():
        z = H.level_z(gm["level"])
        g = gm["g"]
        x, y = gm["x"], gm["y"]
        if g == "G5":
            continue                                # D70: the Keelgate is out of the hub for now
        kind, box = H.gate_box(gm)
        if kind == "cargo":
            # The cargo rope across the whole deck on stanchions, a board in the middle: it stops the player.
            p.box(*box, "MI_HubBlk_Gate")
            step = H.prop("main_deck", "cargo_rope")["every"]
            w = box[3]
            k = -math.floor(w / step)
            while k * step <= w:
                p.box(x - 0.08, x + 0.08, k * step - 0.08, k * step + 0.08, z, z + 1.05, "MI_HubBlk_Rail")
                k += 1
            p.box(x - 0.15, x + 0.15, y - 0.6, y + 0.6, z + 0.5, z + 1.3, "MI_HubBlk_Gate", collide=False)
            continue
        p.box(*box, "MI_HubBlk_Gate")
    p.finish()


def disc(part, cx, cy, r, z0, z1, mat, sides=24, collide=True):
    pts = []
    for k in range(sides):
        t = 2 * math.pi * k / sides
        pts += [(cx + r * math.cos(t), cy + r * math.sin(t), z0), (cx + r * math.cos(t), cy + r * math.sin(t), z1)]
    part.hull(pts, mat, collide)


# --- props, masts, markers ---------------------------------------------------------------------------------------------------
SPACE_LEVEL = {"slip": "quay", "tally": "lower", "chandlery": "main", "harbour": "quay"}


def wall_prop(part, space_name, pr, floor, depth=0.12, mat="MI_HubBlk_Prop"):
    """A board on a wall (plans, rubbings, racks, shelves): 'wall' is 'x=64' or 'y=19'; it stands on the open floor's side."""
    ax, val = pr["wall"].split("=")
    line = float(val)
    z0, z1 = floor + pr.get("z0", 0.0), floor + pr.get("z1", 2.0)
    d = pr.get("depth", depth)
    rects = [r for _, r in H.open_rects(SPACE_LEVEL[space_name])]
    if ax == "x":
        mid = 0.5 * (pr["y0"] + pr["y1"])
        inner = 1.0 if any(H.inside(r, line + 0.5, mid) for r in rects) else -1.0
        x0 = line - WALL / 2 - d if inner < 0 else line + WALL / 2
        part.box(x0, x0 + d, pr["y0"], pr["y1"], z0, z1, mat)
    else:
        mid = 0.5 * (pr["x0"] + pr["x1"])
        inner = 1.0 if any(H.inside(r, mid, line + 0.5) for r in rects) else -1.0
        y0 = line - WALL / 2 - d if inner < 0 else line + WALL / 2
        part.box(pr["x0"], pr["x1"], y0, y0 + d, z0, z1, mat)


def lanterns(part, pr, floor):
    for lx, ly, lz in pr["at"]:
        part.box(lx - 0.2, lx + 0.2, ly - 0.2, ly + 0.2, floor + lz - 0.3, floor + lz + 0.3, "MI_HubBlk_MarkBerth", collide=False)
        part.box(lx - 0.02, lx + 0.02, ly - 0.02, ly + 0.02, floor + lz + 0.3, floor + pr["hook_z"], "MI_HubBlk_Rail", collide=False)


def cradle_xs(db):
    """Where a boat on display stands on her cradles (at 0.21, 0.5 and 0.79 of her length)."""
    return [db["x0"] + (db["x1"] - db["x0"]) * f for f in (0.21, 0.5, 0.79)]


EASEL_OUT = 0.6                                   # the Slip's plans stand on an easel this far in from the quay's edge
GANTRY_GUYS = ((84.0, 10.5), (84.0, -10.5))      # the Well's gantry's guys' feet on the main deck, aft of the companion's hatch
GUIDE_OVER_MAIN = 3.2                             # the hoist's guide posts end this far over the main deck (the cage is 2.4 m tall)


def simple_hull(part, x0, x1, yc, beam, zk, depth, mat):
    """A boat as a plain shape: pointed ends in plan, narrower at the keel."""
    L = x1 - x0
    top = [(x0, yc, zk + depth), (x0 + 0.2 * L, yc - beam / 2, zk + depth), (x1 - 0.25 * L, yc - beam / 2, zk + depth),
           (x1, yc, zk + depth * 1.05), (x1 - 0.25 * L, yc + beam / 2, zk + depth), (x0 + 0.2 * L, yc + beam / 2, zk + depth)]
    bot = [(x0 + 0.1 * L, yc, zk + 0.3 * depth), (x0 + 0.25 * L, yc - beam / 5, zk), (x1 - 0.3 * L, yc - beam / 5, zk),
           (x1 - 0.08 * L, yc, zk + 0.3 * depth), (x1 - 0.3 * L, yc + beam / 5, zk), (x0 + 0.25 * L, yc + beam / 5, zk)]
    part.hull(top + bot, mat)


def bollard_spots():
    """D70: the bollards along the quays' edges (harbour.bollards: every `every` m along each line, 0.45 m in from the face on
    the quay's side, none within 0.8 m of a pillar or on a bridge's landing). Shared with the art (build_hub_inside)."""
    bo = H.prop("harbour", "bollards")
    pillars = H.prop("harbour", "pillars")["at"]
    landings = [(x0 - 0.8, x1 + 0.8, y0 - 0.8, y1 + 0.8) for x0, x1, y0, y1 in H.lift_bridges() + H.fixed_bridges()]
    out = []
    for x0, y0, x1, y1 in bo["lines"]:
        length = math.hypot(x1 - x0, y1 - y0)
        n = max(int(length // bo["every"]), 0)
        for k in range(n + 1):
            t = k / n if n else 0.5
            x, y = x0 + (x1 - x0) * t, y0 + (y1 - y0) * t
            if y0 == y1:                                    # a face along x: the quay away from the canal's middle
                y += math.copysign(0.45, y)
            else:                                           # a basin's end: the quay away from the basin
                x += 0.45 if any(abs(x - b["x1"]) < 0.01 for b in H.HUB["dock"]["basins"]) else -0.45
            if any(abs(x - px) < 0.8 and abs(y - py) < 0.8 for px, py in pillars):
                continue
            if any(H.inside(r, x, y) for r in landings):
                continue
            out.append((x, y))
    return out


def berths():
    """D70: every berth along the canal: the docking stations (hub.docking) and the fleet's (harbour.fleet_berths)."""
    fleet = H.prop("harbour", "fleet_berths")
    return list(H.HUB["docking"]) + [dict(b, heading=fleet["heading"]) for b in fleet["berths"]]


def build_props():
    p = Part("SM_HubBlk_Props")
    zq, zl, zm = Z["quay"], Z["lower"], Z["main"]
    # The harbour (D70): bollards along the quays' edges, the pillars under the galleries' edges, the lanterns.
    dk = {pr["id"]: pr for pr in H.space("harbour")["props"]}
    for bx, by in bollard_spots():
        disc(p, bx, by, dk["bollards"]["r"], zq, zq + dk["bollards"]["h"], "MI_HubBlk_Prop", sides=10)
    lanterns(p, dk["lanterns"], zq)
    pl = dk["pillars"]
    for px, py in pl["at"]:
        p.box(px - pl["size"] / 2, px + pl["size"] / 2, py - pl["size"] / 2, py + pl["size"] / 2, zq, Z["gun"] - SLAB, "MI_HubBlk_Prop")
    # The Slip.
    sl = {pr["id"]: pr for pr in H.space("slip")["props"]}
    c = sl["counter"]
    p.box(c["x0"], c["x1"], c["y0"], c["y1"], zq, zq + c["h"], "MI_HubBlk_Prop")
    wall_prop(p, "slip", sl["plans_board"], zq, depth=0.08)                             # D70: on the shop's aft wall
    wall_prop(p, "slip", sl["half_models"], zq, depth=0.4)
    wb = sl["workbench"]
    p.box(wb["x0"], wb["x1"], wb["y0"], wb["y1"], zq, zq + wb["h"], "MI_HubBlk_Prop")
    wall_prop(p, "slip", sl["timber_racks"], zq, depth=0.6)
    qw = sl["quay_work"]                                                                # the trestles and the steam box on the quay
    sy = 0.5 * (qw["y0"] + qw["y1"])
    p.box(qw["x0"] - 0.6, qw["x0"] + 4.8, sy - 0.6, sy + 0.6, zq, zq + 1.15, "MI_HubBlk_Prop")
    p.box(qw["x1"] - 2.6, qw["x1"] - 0.4, qw["y1"] - 1.2, qw["y1"] - 0.2, zq, zq + 1.1, "MI_HubBlk_Prop")
    lanterns(p, sl["lanterns"], zq)
    sg = sl["sign"]
    p.box(sg["x"] - 3.0, sg["x"] + 3.0, sg["y"] - 0.3, sg["y"], zq + sg["z"] - 0.35, zq + sg["z"] + 0.35, "MI_HubBlk_Canvas", collide=False)
    # The Tally House.
    tl = {pr["id"]: pr for pr in H.space("tally")["props"]}
    pb = tl["pay_booth"]
    hatch = (pb["x0"] + 0.9, pb["x1"] - 1.1)                                              # (its note: the hatch x and z 1.0..1.8)
    p.box(pb["x0"], pb["x1"], pb["y0"], pb["y0"] + 0.1, zl, zl + 1.0, "MI_HubBlk_Prop")
    p.box(pb["x0"], pb["x1"], pb["y0"], pb["y0"] + 0.1, zl + 1.8, zl + pb["h"], "MI_HubBlk_Prop")
    p.box(pb["x0"], hatch[0], pb["y0"], pb["y0"] + 0.1, zl + 1.0, zl + 1.8, "MI_HubBlk_Prop")
    p.box(hatch[1], pb["x1"], pb["y0"], pb["y0"] + 0.1, zl + 1.0, zl + 1.8, "MI_HubBlk_Prop")
    p.box(pb["x0"], pb["x0"] + 0.1, pb["y0"], pb["y1"], zl, zl + pb["h"], "MI_HubBlk_Prop")
    dy0, dy1 = pb["door"]
    for ya, yb in ((pb["y0"], dy0), (dy1, pb["y1"])):
        p.box(pb["x1"] - 0.1, pb["x1"], ya, yb, zl, zl + pb["h"], "MI_HubBlk_Prop")
    p.box(pb["x0"], pb["x1"], pb["y0"], pb["y1"], zl + pb["h"], zl + pb["h"] + 0.1, "MI_HubBlk_Prop")
    p.box(pb["x0"], pb["x1"], pb["y0"] - 0.4, pb["y0"], zl + 0.95, zl + 1.0, "MI_HubBlk_Prop")     # the ledge
    for key in ("weighing_bench", "salt_cakes", "table"):
        r = tl[key]
        p.box(r["x0"], r["x1"], r["y0"], r["y1"], zl, zl + r["h"], "MI_HubBlk_Prop")
    t = tl["table"]
    for by0, by1 in ((14.2, 14.6), (11.8, 12.2)):                                         # the benches (table note)
        p.box(t["x0"], t["x1"], by0, by1, zl, zl + 0.45, "MI_HubBlk_Prop")
    wall_prop(p, "tally", tl["job_board"], zl)
    lanterns(p, tl["lanterns"], zl)
    # The hoist's guide posts and the counterweight's bay (ASaltHubHoist draws and moves the cage and the weight), the gantry.
    hs = H.R(H.HUB["hoist"])
    top = zm + 7.5                                                                          # gantry note: 7.5 m above the deck
    for gy in (hs[2], hs[3] - 0.1):
        p.box(hs[0] - 0.12, hs[0], gy, gy + 0.1, zq, zm + GUIDE_OVER_MAIN, "MI_HubBlk_Rail", collide=False)
    cw = H.prop("well", "counterweight")
    for (ax, ln, a0, a1) in (("y", cw["y0"] - 0.2, cw["x0"] - 0.2, cw["x1"] + 0.3), ("x", cw["x1"] + 0.3, cw["y0"] - 0.2, cw["y1"]),
                             ("x", cw["x0"] - 0.2, cw["y0"] - 0.2, cw["y1"])):
        wall_box(p, ax, ln, a0, a1, zq, zq + RAIL_H, "MI_HubBlk_Rail", RAIL_T)
    apex = (0.5 * (76.5 + 91.5), 0.0, top)
    for fx in (76.5, 91.5):
        pole(p, (fx, 0.0, zm), apex, 0.25, 0.2, "MI_HubBlk_Prop")
    hx_, hy_ = 0.5 * (hs[0] + hs[1]), 0.5 * (hs[2] + hs[3])
    pole(p, apex, (hx_, hy_, top), 0.2, 0.18, "MI_HubBlk_Prop", collide=False)
    for gx, gy in GANTRY_GUYS:
        pole(p, apex, (gx, gy, zm), 0.06, 0.06, "MI_HubBlk_Rail", sides=6)
    co = H.prop("well", "coaming")
    sh = H.R(H.HUB["shaft"])
    for (ax, ln, a0, a1) in (("x", sh[0], sh[2], sh[3]), ("x", sh[1], sh[2], sh[3]), ("y", sh[2], sh[0], sh[1]), ("y", sh[3], sh[0], sh[1])):
        pieces = [(a0 - 0.2, a1 + 0.2)]
        if ax == "x" and abs(ln - sh[0]) < 1e-6:
            pieces = [(a0 - 0.2, hs[2]), (hs[3], a1 + 0.2)]                                # open where the hoist's cage stops
        for pa, pb in pieces:
            wall_box(p, ax, ln, pa, pb, zm, zm + co["h"], "MI_HubBlk_Rail", 0.35)
    # The Chandlery (D69: in the forecastle's front; its walls and door are the main deck's floor plan): the counter, the shelves
    # (open where a gate stands in the back wall), the display table, the spyglass's tripod, the goods outside the front.
    cd = {pr["id"]: pr for pr in H.space("chandlery")["props"]}
    cn = cd["counter"]
    p.box(cn["x0"], cn["x1"], cn["y0"], cn["y1"], zm, zm + cn["top"], "MI_HubBlk_Prop")
    shp = cd["shop"]
    gaps = [(g["y"] - 1.0, g["y"] + 1.0) for g in PL.plan("main").gates if abs(g["x"] - shp["x1"]) < 0.3]
    shv = cd["shelves"]
    for sx0, sx1, sy0, sy1 in shv["runs"]:
        if (sx1 - sx0) > (sy1 - sy0):
            p.box(sx0, sx1, sy0, sy1, zm, zm + shv["h"], "MI_HubBlk_Prop")
            continue
        a = sy0
        for ga, gb in sorted(gaps) + [(sy1, sy1)]:
            if ga - a > 0.4:
                p.box(sx0, sx1, a, min(ga, sy1), zm, zm + shv["h"], "MI_HubBlk_Prop")
            a = max(a, gb)
    dt = cd["display_table"]
    p.box(dt["x0"], dt["x1"], dt["y0"], dt["y1"], zm, zm + dt["h"], "MI_HubBlk_Prop")
    r = cd["spyglass_stand"]
    p.box(r["x"] - 0.25, r["x"] + 0.25, r["y"] - 0.25, r["y"] + 0.25, zm, zm + r["h"], "MI_HubBlk_Prop")
    for gx0, gx1, gy0, gy1 in cd["goods"]["boxes"]:
        p.box(gx0, gx1, gy0, gy1, zm, zm + 1.0, "MI_HubBlk_Prop")
    # The main deck: fife rails round the masts, hatches, the ship's boats, capstan, the galley's chimney.
    md = {pr["id"]: pr for pr in H.space("main_deck")["props"]}
    for mx, my in md["masts"]["at"]:
        zb = zm if mx < H.HUB["fcX"] else Z["fcTop"]
        for (fx0, fx1, fy0, fy1) in ((mx - 2, mx + 2, my - 2, my - 1.85), (mx - 2, mx + 2, my + 1.85, my + 2), (mx - 2, mx - 1.85, my - 2, my + 2),
                                     (mx + 1.85, mx + 2, my - 2, my + 2)):
            p.box(fx0, fx1, fy0, fy1, zb, zb + 0.9, "MI_HubBlk_Prop")
    for x0, x1, y0, y1 in md["cargo_hatches"]["boxes"]:
        p.box(x0, x1, y0, y1, zm, zm + 0.3, "MI_HubBlk_Prop")
    for x0, x1, y0, y1 in md["ships_boats"]["boxes"]:
        simple_hull(p, x0, x1, 0.5 * (y0 + y1), y1 - y0, zm + 0.4, 1.2, "MI_HubBlk_Prop")
    cp = md["capstan"]
    disc(p, cp["x"], cp["y"], cp["r"], zm, zm + 1.1, "MI_HubBlk_Prop", sides=12)
    gc = md["galley_chimney"]
    p.box(gc["x"] - 0.4, gc["x"] + 0.4, gc["y"] - 0.4, gc["y"] + 0.4, zm, zm + gc["h"], "MI_HubBlk_Rail")
    p.finish()


def build_masts():
    p = Part("SM_HubBlk_Masts")
    zm = Z["main"]
    r_deck = H.prop("main_deck", "masts")["r"]
    rig = H.HUB["rig"]
    for m in H.HUB["masts"]:
        x, h = m["x"], m["h"]
        sc = rig["yards"]["scale"][m["id"]]
        pole(p, (x, 0.0, zm - 0.5), (x, 0.0, zm + h), r_deck, 0.35, "MI_HubBlk_Prop", sides=12)
        tw = rig["top"]["w"] * h / 76.0
        p.box(x - 2.0, x + 2.0, -tw / 2, tw / 2, zm + rig["top"]["at"] * h - 0.25, zm + rig["top"]["at"] * h + 0.25, "MI_HubBlk_Prop", collide=False)
        cw_ = rig["cross"]["w"] * h / 76.0
        p.box(x - 0.2, x + 0.2, -cw_ / 2, cw_ / 2, zm + rig["cross"]["at"] * h - 0.2, zm + rig["cross"]["at"] * h + 0.2, "MI_HubBlk_Prop", collide=False)
        for frac, half in zip(rig["yards"]["at"], rig["yards"]["half"]):
            hl = half * H.HUB["B"] * sc
            zy = zm + frac * h
            p.box(x - 0.35, x + 0.35, -hl, hl, zy - 0.35, zy + 0.35, "MI_HubBlk_Prop", collide=False)
            p.box(x - 0.1, x + 0.9, -hl * 0.92, hl * 0.92, zy - 1.2, zy - 0.3, "MI_HubBlk_Canvas", collide=False)
    p.finish()


def build_markers():
    p = Part("SM_HubBlk_Markers", ucx=False)
    floors = {"slip": Z["quay"], "tally": Z["lower"], "chandlery": Z["main"]}
    for name, fl in floors.items():
        sp = H.space(name)
        npc = sp["npc"]
        disc(p, npc["x"], npc["y"], 0.5, fl + 0.01, fl + 0.05, "MI_HubBlk_MarkNPC", sides=12, collide=False)
        f = math.radians(npc["face"])
        dx, dy = math.cos(f), math.sin(f)
        tip = (npc["x"] + dx * 1.1, npc["y"] + dy * 1.1)
        l = (npc["x"] + dx * 0.4 - dy * 0.35, npc["y"] + dy * 0.4 + dx * 0.35)
        r = (npc["x"] + dx * 0.4 + dy * 0.35, npc["y"] + dy * 0.4 - dx * 0.35)
        p.hull([(tip[0], tip[1], fl + 0.01), (l[0], l[1], fl + 0.01), (r[0], r[1], fl + 0.01),
                (tip[0], tip[1], fl + 0.06), (l[0], l[1], fl + 0.06), (r[0], r[1], fl + 0.06)], "MI_HubBlk_MarkNPC", collide=False)
        p.box(npc["x"] - 0.04, npc["x"] + 0.04, npc["y"] - 0.04, npc["y"] + 0.04, fl, fl + 1.4, "MI_HubBlk_MarkNPC", collide=False)
        for e in sp["e"]:
            p.box(e["x"] - 0.12, e["x"] + 0.12, e["y"] - 0.12, e["y"] + 0.12, fl + 0.9, fl + 1.14, "MI_HubBlk_MarkE", collide=False)
    for b in berths():                                                                 # the berths' lines on the water
        _, y, _ = H.docking_pose(b)
        p.box(b["x0"], b["x1"], y - 0.05, y + 0.05, 0.02, 0.08, "MI_HubBlk_MarkBerth", collide=False)
    p.finish()


BUILDERS = {"shell": build_shell, "dock": build_dock, "decks": build_decks, "stairs": build_stairs, "gates": build_gates,
            "props": build_props, "masts": build_masts, "markers": build_markers}

if __name__ == "__main__":
    wanted = sb.script_args() or list(BUILDERS)
    for key in wanted:
        sb.reset_scene()
        BUILDERS[key]()
    print("SB_HUBBLK_OK", wanted)
