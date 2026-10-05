"""The hub ship's blockout (D61; phase 1 of Docs/Design/HubShip/HubShip_Agent_Prompt.md): plain shapes and flat colours
to walk, sail and measure the design before any art. Only for its own review map, L_HubBlockout.

Every number comes from Docs/Design/HubShip/HubShip_Data.json through hub_layout.py. The few build choices that are not
in the data are named constants below (slab and wall thickness, the 0.5 m wall grid) or come from the prompt's scale rules
(section 3: doors 2.2 m, 18 cm risers on 30 cm treads, rails 1.0 m).

Meshes (pivot at the ship's x 80 on the waterline, bow to +X; /Game/Saltbound/Hub/Blockout):
  SM_HubBlk_ShellStern     the hull from the stern face to the sterncastle's front (x 0..44.6): the stern face with the dock
                           mouth and the arch over it, the wings' inner faces, the poop, the sterncastle's front (complex)
  SM_HubBlk_ShellWaistS/P  the hull's sides and bottom from x 44.6 to 134, starboard and port (complex)
  SM_HubBlk_ShellBow       the bow, the forecastle's aft face and top, the bowsprit (complex)
  SM_HubBlk_Basin          the Stern Dock: basin floor and walls, the quays, the slipway (complex: the boat hits them)
  SM_HubBlk_Quay / Lower / Middle / Upper / Main   each deck's open floors, walls (along the open zones' edges, with
                           the doors and windows of the data), ceilings and rails (UCX boxes)
  SM_HubBlk_Stairs         the dock stair and the Well's four flights: visual steps, ramp collision
  SM_HubBlk_Gates          every gate of gate_marks as a marked block (G5: the Keelgate)
  SM_HubBlk_Props          the spaces' props as plain boxes at their sizes, the hoist cage, the gantry, the masts' fife
                           rails, hatches, boats, capstan, chimney
  SM_HubBlk_Masts          the three masts with tops, crosstrees, yards and furled sails
  SM_HubBlk_Markers        NPC spots (a disc and an arrow the way they face), interaction points, the berths (no collision)

Colours (MI_HubBlk_*): open warm, open walls lighter, locked grey, the ancient hull and the basin pale stone, the timber hull
dark, props brown, gates red, markers bright.
Run: Tools/Build-BlenderAsset.ps1 build_hub_blockout [shell basin decks stairs gates props masts markers]
"""
import math
import os
import sys

import bmesh
import bpy
from mathutils import Vector

sys.path.insert(0, os.path.dirname(__file__))
import hub_layout as H  # noqa: E402
import sb_blender as sb  # noqa: E402

CELL = 0.5            # the wall grid (m): walls run along the open zones' edges on it
SLAB = 0.3            # floor slab thickness (its top is the floor's z)
WALL = 0.3            # wall thickness (centred on the zone edge)
RAIL_H = 1.0          # rails round the Well and the dock head (spaces.well.shaft / spaces.dock.dock_head_rail notes)
RAIL_T = 0.12
DOOR_H = 2.2          # prompt section 3: doors 2.2 x 1.2 m (double 3 x 2.4 m)
RISER, TREAD = 4.0 / 22.0, 0.30   # prompt section 3: 22 risers of about 18 cm per 4 m storey on 30 cm treads
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


def hull_clip(rect, z):
    """Clamp a rectangle's y to the hull's inside at height z (sampled at its middle)."""
    x0, x1, y0, y1 = rect
    w = H.half_width(0.5 * (x0 + x1), z + 0.5) - 0.05
    return (x0, x1, max(y0, -w), min(y1, w))


# --- the shell --------------------------------------------------------------------------------------------------------
KNOTS = [-12.0, -10.0, -8.0, -6.0, -4.0, -2.0, 0.0, 1.5, H.HUB["ancientTop"], 4.5, 6.0, 8.0, 10.0, 12.0, 14.0, 16.0, 17.5, 18.7,
         21.0, 23.5, 26.0, 28.5, 31.0, 33.5, 34.7]


def ring(x):
    """The hull's section at x: [(y, z)] from the starboard top down to the keel's middle and up to the port top."""
    zb, zt = H.profile_bottom(x), H.profile_top(x)
    zs = [min(max(k, zb), zt) for k in KNOTS]
    stbd = [(H.half_width(x, z), z) for z in reversed(zs)]
    port = [(-H.half_width(x, z), z) for z in zs]
    return stbd + [(0.0, zb)] + port


def shell_stations(x0, x1):
    xs = {float(x) for x in range(int(math.ceil(x0)), int(math.floor(x1)) + 1)}
    xs |= {x0, x1}
    for k in (44.0, 44.6, 134.0, 134.6, 161.0, 161.5):
        if x0 <= k <= x1:
            xs.add(k)
    return sorted(xs)


def shell(part, x0, x1, side=None):
    """Skin the sections between x0 and x1 (side: None both, +1 starboard half, -1 port half)."""
    stations = shell_stations(x0, x1)
    rings = [ring(x) for x in stations]
    n = len(rings[0])
    mid = n // 2
    rng = range(0, n - 1) if side is None else (range(0, mid) if side > 0 else range(mid, n - 1))
    for (xa, ra), (xb, rb) in zip(zip(stations, rings), zip(stations[1:], rings[1:])):
        for k in rng:
            (ya0, za0), (ya1, za1) = ra[k], ra[k + 1]
            (yb0, zb0), (yb1, zb1) = rb[k], rb[k + 1]
            zc = 0.25 * (za0 + za1 + zb0 + zb1)
            mat = "MI_HubBlk_Stone" if zc < H.HUB["ancientTop"] else "MI_HubBlk_Hull"
            part.quad([(xa, ya0, za0), (xb, yb0, zb0), (xb, yb1, zb1), (xa, ya1, za1)], mat)


def arch_z(y):
    """The dock mouth's top (outside.stern): an arch from z 21.5 at |y| 10 to 25.5 in the middle (a circle through
    them); below the springing the wings' inner faces."""
    a = H.HUB["arch"]
    top, spring, half = a["z0"], Z["sc1"], H.HUB["basin"]["y1"]
    rise = top - spring
    r = (half * half + rise * rise) / (2.0 * rise)
    zc = top - r
    return zc + math.sqrt(max(r * r - y * y, 0.0)) if abs(y) <= half else spring


def face_cells(part, x, z0, z1, keep, w_of_z, mat, dz=0.25, dy=0.25):
    """A vertical face at x over |y| < w_of_z(z), z0..z1, where keep(y, z) (rows of runs, a plane fill)."""
    z = z0
    while z < z1 - 1e-6:
        zt = min(z + dz, z1)
        zm = 0.5 * (z + zt)
        w = w_of_z(zm)
        y = -w
        run = None
        while y < w - 1e-6:
            yt = min(y + dy, w)
            ok = keep(0.5 * (y + yt), zm)
            if ok and run is None:
                run = y
            if (not ok or yt >= w - 1e-6) and run is not None:
                end = yt if ok else y
                part.quad([(x, run, z), (x, end, z), (x, end, zt), (x, run, zt)], mat)
                run = None
            y = yt
        z = zt


def build_shell():
    basin = H.HUB["basin"]
    half, mouth = basin["y1"], H.HUB["quayY"]
    # The stern block: hull sides and bottom, the stern face with the dock mouth, the arch, the wings, the poop.
    p = Part("SM_HubBlk_ShellStern", ucx=False)
    shell(p, 0.0, 44.6)

    def open_mouth(y, z):
        if abs(y) < half:
            return basin["floor"] < z < arch_z(y)
        return abs(y) < mouth and Z["quay"] < z < Z["lower"]

    face_cells(p, 0.0, H.profile_bottom(0.0), H.profile_top(0.0), lambda y, z: not open_mouth(y, z),
               lambda z: H.half_width(0.0, z), "MI_HubBlk_Hull")
    a = H.HUB["arch"]
    ys = [-half + i * 0.5 for i in range(int(2 * half / 0.5) + 1)]
    for ya, yb in zip(ys[:-1], ys[1:]):
        # The arch's vault, its forward face and top.
        p.quad([(a["x0"], ya, arch_z(ya)), (a["x1"], ya, arch_z(ya)), (a["x1"], yb, arch_z(yb)), (a["x0"], yb, arch_z(yb))], "MI_HubBlk_Locked")
        p.quad([(a["x1"], ya, arch_z(ya)), (a["x1"], yb, arch_z(yb)), (a["x1"], yb, a["z1"]), (a["x1"], ya, a["z1"])], "MI_HubBlk_Locked")
    p.quad([(a["x0"], -half, a["z1"]), (a["x1"], -half, a["z1"]), (a["x1"], half, a["z1"]), (a["x0"], half, a["z1"])], "MI_HubBlk_Locked")
    for s in (-1.0, 1.0):
        y = s * half
        # The wings' inner faces over the basin: from the wing's lower deck (over the arcade) up to the poop rail.
        p.quad([(0.0, y, Z["lower"]), (a["x1"], y, Z["lower"]), (a["x1"], y, Z["sc1"]), (0.0, y, Z["sc1"])], "MI_HubBlk_Locked")
        px = H.HUB["poopX"]                       # the galleon's tiers (D62): the poop aft of poopX, the quarterdeck forward
        for xa_, xb_ in ((a["x1"], px + 0.3), (px + 0.3, basin["x1"])):
            zt_ = H.profile_top(0.5 * (xa_ + xb_))
            p.quad([(xa_, y, Z["lower"]), (xb_, y, Z["lower"]), (xb_, y, zt_), (xa_, y, zt_)], "MI_HubBlk_Locked")
        # The poop and the quarterdeck over each wing.
        for xa in range(0, int(basin["x1"]), 2):
            xb = min(xa + 2, basin["x1"])
            zd = Z["poop"] if xb <= px else Z["sc2"]
            w = H.half_width(0.5 * (xa + xb), zd)
            ya, yb = sorted((y, s * w))
            p.quad([(xa, ya, zd), (xb, ya, zd), (xb, yb, zd), (xa, yb, zd)], "MI_HubBlk_Locked")
    # Twin rudders on the wings' sternposts (the wheel's ropes run to their tillers) and the stern moorings.
    ru = H.HUB["rudders"]
    for sg in (-1.0, 1.0):
        p.box(ru["x0"], ru["x1"], sg * ru["y"] - 0.25, sg * ru["y"] + 0.25, ru["z0"], ru["z1"], "MI_HubBlk_Hull")
    for cb in H.HUB["cables"]:
        if cb["id"] == "stern":
            pole(p, cb["from"], cb["to"], 0.22, 0.22, "MI_HubBlk_Rail", sides=6)
    # The sterncastle's front over the main deck (its doors are gates).
    xf = basin["x1"] + 0.3
    face_cells(p, xf, Z["main"], H.profile_top(xf - 1.0), lambda y, z: abs(y) >= half, lambda z: H.half_width(xf, z), "MI_HubBlk_Locked")
    xp = H.HUB["poopX"] + 0.3                     # the poop's front over the quarterdeck
    face_cells(p, xp, Z["sc2"], H.profile_top(1.0), lambda y, z: abs(y) >= half, lambda z: H.half_width(xp, z), "MI_HubBlk_Locked")
    p.finish()

    for side, name in ((1, "SM_HubBlk_ShellWaistS"), (-1, "SM_HubBlk_ShellWaistP")):
        q = Part(name, ucx=False)
        shell(q, 44.6, 134.6, side)
        q.finish()

    b = Part("SM_HubBlk_ShellBow", ucx=False)
    shell(b, 134.6, 161.5)
    xa = H.HUB["fcX"] + 0.3
    face_cells(b, xa, Z["main"], H.profile_top(150.0), lambda y, z: True, lambda z: H.half_width(xa, z), "MI_HubBlk_Locked")
    x = H.HUB["fcX"] + 0.6
    while x < 161.0:
        xb = min(x + 1.0, 161.0)
        w = H.half_width(0.5 * (x + xb), Z["fcTop"]) - 0.05
        if w > 0.1:
            b.quad([(x, -w, Z["fcTop"]), (xb, -w, Z["fcTop"]), (xb, w, Z["fcTop"]), (x, w, Z["fcTop"])], "MI_HubBlk_Locked")
        x = xb
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


# --- the basin ---------------------------------------------------------------------------------------------------------
def build_basin():
    p = Part("SM_HubBlk_Basin", ucx=False)
    bs = H.HUB["basin"]
    sw = H.prop("dock", "slipway")
    half, quay = bs["y1"], H.HUB["quayY"]
    fl = bs["floor"]
    # The floor (ancient stone, 5 m down), beside the slipway up to the dock head.
    p.quad([(bs["x0"], -half, fl), (sw["x0"], -half, fl), (sw["x0"], half, fl), (bs["x0"], half, fl)], "MI_HubBlk_Stone")
    for s in (-1, 1):
        ya, yb = sorted((s * sw["y1"], s * half))
        p.quad([(sw["x0"], ya, fl), (bs["x1"], ya, fl), (bs["x1"], yb, fl), (sw["x0"], yb, fl)], "MI_HubBlk_Stone")
        # The quays: stone walls from the floor, timber decking on top (the arcade above is the Quay part).
        p.box(bs["x0"], bs["x1"], min(s * half, s * quay), max(s * half, s * quay), fl, Z["quay"] - 0.1, "MI_HubBlk_Stone")
        p.box(bs["x0"], bs["x1"], min(s * half, s * quay), max(s * half, s * quay), Z["quay"] - 0.1, Z["quay"], "MI_HubBlk_Open")
        # The dock head beside the slipway, up to the hall floor.
        p.box(bs["x1"], sw["x1"], ya, yb, fl, Z["quay"] - SLAB, "MI_HubBlk_Stone")
    # The slipway: from 3 m under the water up into the hall floor (a beach the boat runs up on).
    w = sw["y1"]
    p.hull([(sw["x0"], -w, fl), (sw["x0"], w, fl), (sw["x0"], -w, sw["z_at_x0"]), (sw["x0"], w, sw["z_at_x0"]),
            (sw["x1"], -w, fl), (sw["x1"], w, fl), (sw["x1"], -w, sw["z_at_x1"]), (sw["x1"], w, sw["z_at_x1"])], "MI_HubBlk_Stone")
    p.finish()


# --- decks: floors, walls, rails ----------------------------------------------------------------------------------------
def flight_holes(level):
    """Footprints of the flights that arrive at this level (holes in its floor) and the edge they arrive over."""
    out = []
    for f in H.flights():
        if f["to"] == level:
            r = (f["x0"], f["x1"], f["y0"], f["y1"])
            arrive = {"-x": ("x", f["x0"]), "+x": ("x", f["x1"]), "-y": ("y", f["y0"]), "+y": ("y", f["y1"])}[f["up"]]
            out.append((r, arrive))
    return out


def rasterise(z, opens, holes, rail_voids, wall_voids, water, heights, rail_gaps):
    """Walls along the open zones' edges, and rails where they meet the Well's shaft or a stair hole: runs of (kind, axis,
    line, from, to, height). opens: the zone rectangles; holes: stair holes in this floor (they close the outer walls like
    the floor round them, and are railed from it); heights(x, y): the zone's clear height."""
    def inside_any(rs, x, y):
        return any(r[0] <= x < r[1] and r[2] <= y < r[3] for r in rs)

    def is_open(x, y):
        return inside_any(opens, x, y) and not inside_any(holes, x, y) and not inside_any(rail_voids, x, y) \
            and not inside_any(wall_voids, x, y)

    edges = {}
    nx = int(round(162.0 / CELL))
    ny = int(round(42.0 / CELL))
    for i in range(nx):
        cx = (i + 0.5) * CELL
        for j in range(ny):
            cy = -21.0 + (j + 0.5) * CELL
            o = is_open(cx, cy)
            if not o and not inside_any(holes, cx, cy):
                continue
            hgt = heights(cx, cy)
            for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                nxp, nyp = cx + dx * CELL, cy + dy * CELL
                if is_open(nxp, nyp):
                    continue
                n_hole, n_void = inside_any(holes, nxp, nyp), inside_any(rail_voids, nxp, nyp)
                if n_hole or n_void:
                    if not o or (n_hole and inside_any(holes, cx, cy)):
                        continue
                    kind = "rail"
                elif inside_any(water, nxp, nyp) or abs(nyp) >= H.half_width(nxp, z + 1.0):
                    continue
                else:
                    kind = "wall"
                if dx:
                    axis, line, a = "x", cx + dx * CELL * 0.5, cy - CELL * 0.5
                else:
                    axis, line, a = "y", cy + dy * CELL * 0.5, cx - CELL * 0.5
                key = (kind, axis, round(line, 3), hgt if kind == "wall" else RAIL_H)
                edges.setdefault(key, []).append(round(a, 3))
    runs = []
    for (kind, axis, line, hgt), starts in edges.items():
        starts = sorted(set(starts))
        a0 = prev = starts[0]
        for st in starts[1:] + [None]:
            if st is not None and abs(st - prev - CELL) < 1e-6:
                prev = st
                continue
            runs.append((kind, axis, line, a0, prev + CELL, hgt))
            if st is not None:
                a0 = prev = st
    # Gaps in the rails: the hoist's stops, the edge a flight arrives over.
    out = []
    for kind, axis, line, a0, a1, hgt in runs:
        pieces = [(a0, a1)]
        if kind == "rail":
            for g in rail_gaps:
                if g[0] == axis and abs(g[1] - line) < 0.01:
                    pieces = [q for pc in pieces for q in ((pc[0], min(pc[1], g[2])), (max(pc[0], g[3]), pc[1])) if q[1] - q[0] > 1e-3]
        out += [(kind, axis, line, pa, pb, hgt) for pa, pb in pieces]
    return out


def wall_box(part, axis, line, a0, a1, z0, z1, mat, thick):
    if axis == "x":
        part.box(line - thick / 2, line + thick / 2, a0, a1, z0, z1, mat)
    else:
        part.box(a0, a1, line - thick / 2, line + thick / 2, z0, z1, mat)


def draw_runs(part, z, runs, openings):
    """Walls (with their openings: (axis, line, from, to, bottom, top) above the floor) and rails."""
    for kind, axis, line, a0, a1, hgt in runs:
        if kind == "rail":
            wall_box(part, axis, line, a0, a1, z, z + RAIL_H, "MI_HubBlk_Rail", RAIL_T)
            continue
        holes = [o for o in openings if o[0] == axis and abs(o[1] - line) < 0.01 and o[3] > a0 and o[2] < a1]
        cuts = sorted(set([a0, a1] + [max(a0, o[2]) for o in holes] + [min(a1, o[3]) for o in holes]))
        for ca, cb in zip(cuts[:-1], cuts[1:]):
            mid = 0.5 * (ca + cb)
            o = next((o for o in holes if o[2] <= mid <= o[3]), None)
            if o is None:
                wall_box(part, axis, line, ca, cb, z, z + hgt, "MI_HubBlk_Wall", WALL)
            else:
                wall_box(part, axis, line, ca, cb, z, z + o[4], "MI_HubBlk_Wall", WALL)
                wall_box(part, axis, line, ca, cb, z + o[5], z + hgt, "MI_HubBlk_Wall", WALL)


def build_decks():
    sh, ring_, ch = H.R(H.HUB["shaft"]), H.R(H.HUB["ring"]), H.R(H.HUB["chamber"])
    hoist = H.R(H.HUB["hoist"])
    bs = H.HUB["basin"]
    hall, shop, tally, lane = H.R(H.HUB["hall"]), H.R(H.HUB["shop"]), H.R(H.HUB["tally"]), H.R(H.HUB["lane"])
    stair = H.R(H.HUB["stair"])
    sw = H.prop("dock", "slipway")
    hoist_gap = ("x", sh[0], hoist[2], hoist[3])          # the cage's aft side opens on to the galleries
    water = [(bs["x0"], bs["x1"], bs["y0"], bs["y1"])]

    # Quay deck: the quays (under the wings), the Slip (hall 12 m high, shop), the Keelgate chamber.
    q = Part("SM_HubBlk_Quay")
    zq = Z["quay"]
    hall_h = H.prop("slip", "hall")["h"]
    opens = [r for zid, r in H.open_rects("quay") if zid != "stair"]

    def heights_q(x, y):
        return hall_h if H.inside(hall, x, y) else Z["lower"] - zq

    land = next(r for zid, r in H.open_rects("lower") if zid == "tally" and r[1] - r[0] < 5.0)
    foot = land[0] - 22 * TREAD                                                     # where the dock stair's steps begin
    openings_q = [("y", stair[2], stair[0], foot, 0.0, 2.6)]                        # the arcade's opening into the dock stair
    runs = rasterise(zq, opens, [], [], [], water, heights_q, [])
    draw_runs(q, zq, runs, openings_q)
    for r in subtract((hall[0], hall[1], hall[2], shop[3]), [(hall[0], sw["x1"], sw["y0"], sw["y1"])]):
        slab(q, r, zq, "MI_HubBlk_Open")
    slab(q, ch, zq, "MI_HubBlk_Open")
    for zid, r in H.open_rects("quay"):
        if zid == "dock" and r[0] >= H.HUB["basin"]["x1"]:
            slab(q, r, zq, "MI_HubBlk_Open")                                             # the port landing
    for s in (-1, 1):
        ya, yb = sorted((s * bs["y1"], s * H.HUB["quayY"]))
        slab(q, (bs["x0"], bs["x1"], ya, yb), Z["lower"], "MI_HubBlk_Locked")          # the wing over the arcade
        post = H.prop("dock", "arcade_posts")
        x = post["from"]
        while x <= post["to"]:
            hp = post["size"] / 2
            q.box(x - hp, x + hp, s * post["y"] - hp, s * post["y"] + hp, zq, zq + post["h"], "MI_HubBlk_Prop")
            x += post["every"]
    slab(q, hall, Z["upper"], "MI_HubBlk_Locked")                                        # the hall's ceiling
    q.finish()

    # Lower deck: the Tally House (with the dock stair's landing), the lane, the Well's lower gallery.
    lw = Part("SM_HubBlk_Lower")
    zl = Z["lower"]
    holes = flight_holes("lower")
    opens = [r for zid, r in H.open_rects("lower")]
    win = H.prop("tally", "hall_windows")
    openings_l = [("x", land[0], land[2], land[3], 0.0, Z["middle"] - zl)]                # where the dock stair arrives
    openings_l += [("y", tally[2], a, b, win["z0"], win["z1"]) for a, b in win["spans"]]
    gaps = [hoist_gap] + [(ax, ln, h[2] if ax == "x" else h[0], h[3] if ax == "x" else h[1]) for h, (ax, ln) in holes]
    runs = rasterise(zl, opens, [h for h, _ in holes], [sh], [hall], water, lambda x, y: Z["middle"] - zl, gaps)
    draw_runs(lw, zl, runs, openings_l)
    slab(lw, hull_clip(tally, zl), zl, "MI_HubBlk_Open")
    slab(lw, lane, zl, "MI_HubBlk_Open")
    slab(lw, ch, zl, "MI_HubBlk_Open", holes=[sh] + [h for h, _ in holes])               # the gallery over the chamber
    slab(lw, hull_clip(tally, Z["middle"]), Z["middle"], "MI_HubBlk_Locked")             # ceilings
    slab(lw, lane, Z["middle"], "MI_HubBlk_Locked")
    lw.finish()

    # Middle and upper decks: the Well's galleries only; the hall's walls between them.
    for level, name in (("middle", "SM_HubBlk_Middle"), ("upper", "SM_HubBlk_Upper")):
        pt = Part(name)
        zz = Z[level]
        holes = flight_holes(level)
        opens = [r for zid, r in H.open_rects(level)]
        openings = []
        for gm in H.GATE_MARKS:
            if gm["level"] == level and gm["g"] == "G2":                                   # a roped doorway (Market closed)
                openings.append(("x", gm["x"], gm["y"] - 1.2, gm["y"] + 1.2, 0.0, 2.6))
        gaps = [(ax, ln, h[2] if ax == "x" else h[0], h[3] if ax == "x" else h[1]) for h, (ax, ln) in holes]
        nxt = "upper" if level == "middle" else "main"
        runs = rasterise(zz, opens, [h for h, _ in holes], [sh], [], [], lambda x, y: Z[nxt] - zz, gaps)
        draw_runs(pt, zz, runs, openings)
        slab(pt, ring_, zz, "MI_HubBlk_Open", holes=[sh] + [h for h, _ in holes])
        if level == "middle":
            for s in (-1, 1):                                                              # the hall's sides above the Tally
                wall_box(pt, "y", s * hall[3], hall[0], hall[1], zz, Z["upper"] - SLAB, "MI_HubBlk_Locked", WALL)
        else:
            slab(pt, hall, zz, "MI_HubBlk_Locked")                                           # (the hall's ceiling is in Quay)
        pt.finish()

    # Main deck: the deck from the dock head to the forecastle, the Well's opening and the companion, rails.
    m = Part("SM_HubBlk_Main")
    zm = Z["main"]
    comp = next(h for h, _ in flight_holes("main"))
    cargo_x = H.prop("main_deck", "cargo_rope")["x"]
    x = bs["x1"]
    while x < H.HUB["fcX"] - 1e-6:
        xb = min(x + 2.0, H.HUB["fcX"])
        if x < 108.0 and xb <= 108.0:
            xb = 108.0
        w = H.half_width(0.5 * (x + xb), zm) - 0.05
        for xa_, xb_ in ((x, min(xb, cargo_x)), (max(x, cargo_x), xb)):
            if xb_ - xa_ > 1e-3:
                slab(m, (xa_, xb_, -w, w), zm, "MI_HubBlk_Open" if xb_ <= cargo_x else "MI_HubBlk_Locked", holes=[sh, comp])
        x = xb
    half = bs["y1"]
    wall_box(m, "x", bs["x1"], -half, half, zm, zm + RAIL_H, "MI_HubBlk_Rail", RAIL_T)          # the dock head rail
    m.box(bs["x1"], bs["x1"] + 0.3, -half, half, Z["upper"], zm - SLAB, "MI_HubBlk_Locked")     # over the hall's mouth
    for s in (-1, 1):
        ya, yb = sorted((s * hall[3], s * half))
        m.box(bs["x1"], bs["x1"] + 0.3, ya, yb, zl, zm - SLAB, "MI_HubBlk_Locked")             # the corner by the wings
    for (ax, ln, a0, a1) in (("x", sh[0], sh[2], sh[3]), ("x", sh[1], sh[2], sh[3]), ("y", sh[2], sh[0], sh[1]), ("y", sh[3], sh[0], sh[1])):
        pieces = [(a0, a1)]
        if ax == hoist_gap[0] and abs(ln - hoist_gap[1]) < 0.01:
            pieces = [(a0, hoist_gap[2]), (hoist_gap[3], a1)]
        for pa, pb in pieces:
            wall_box(m, ax, ln, pa, pb, zm, zm + RAIL_H, "MI_HubBlk_Rail", RAIL_T)
    co = H.prop("well", "coaming")
    for (ax, ln, a0, a1) in (("x", sh[0], sh[2], sh[3]), ("x", sh[1], sh[2], sh[3]), ("y", sh[2], sh[0], sh[1]), ("y", sh[3], sh[0], sh[1])):
        wall_box(m, ax, ln, a0 - 0.2, a1 + 0.2, zm, zm + co["h"], "MI_HubBlk_Rail", 0.35)
    f_main = next(f for f in H.flights() if f["to"] == "main")
    for (ax, ln, a0, a1) in (("x", comp[0], comp[2], comp[3]), ("x", comp[1], comp[2], comp[3]), ("y", comp[2], comp[0], comp[1])):
        wall_box(m, ax, ln, a0, a1, zm, zm + RAIL_H, "MI_HubBlk_Rail", RAIL_T)
    m.finish()


# --- stairs ----------------------------------------------------------------------------------------------------------------
def flight(part, x0, x1, y0, y1, up, z0, z1, rails=True):
    """22 steps of 30 cm from the bottom end, the rest a landing at the top; solid under; ramp collision."""
    n = int(round((z1 - z0) / RISER))
    run = n * TREAD
    if up in ("+x", "-x"):
        length = x1 - x0
    else:
        length = y1 - y0
    sign = 1.0 if up[0] == "+" else -1.0
    start = (x0 if sign > 0 else x1) if up[1] == "x" else (y0 if sign > 0 else y1)

    def at(t):                       # distance t from the bottom end along the way up -> coordinate
        return start + sign * t

    for k in range(n):
        a, b = sorted((at(k * TREAD), at((k + 1) * TREAD)))
        top = z0 + (k + 1) * RISER
        if up[1] == "x":
            part.box(a, b, y0, y1, z0, top, "MI_HubBlk_Open", collide=False)
        else:
            part.box(x0, x1, a, b, z0, top, "MI_HubBlk_Open", collide=False)
    a, b = sorted((at(run), at(length)))
    if up[1] == "x":
        part.box(a, b, y0, y1, z0, z1, "MI_HubBlk_Open")
        e0, e1 = sorted((at(0.0), at(run)))
        lo = at(0.0)
        hi = at(run)
        part.hull([(lo, y0, z0), (lo, y1, z0), (hi, y0, z0), (hi, y1, z0), (hi, y0, z1), (hi, y1, z1), (lo, y0, z0 + 0.02), (lo, y1, z0 + 0.02)],
                  "MI_HubBlk_Open")
        if rails:
            for yy in (y0, y1):
                part.hull([(lo, yy - RAIL_T / 2, z0), (lo, yy + RAIL_T / 2, z0), (lo, yy - RAIL_T / 2, z0 + RAIL_H), (lo, yy + RAIL_T / 2, z0 + RAIL_H),
                           (hi, yy - RAIL_T / 2, z1), (hi, yy + RAIL_T / 2, z1), (hi, yy - RAIL_T / 2, z1 + RAIL_H), (hi, yy + RAIL_T / 2, z1 + RAIL_H)],
                          "MI_HubBlk_Rail")
    else:
        part.box(x0, x1, a, b, z0, z1, "MI_HubBlk_Open")
        lo = at(0.0)
        hi = at(run)
        part.hull([(x0, lo, z0), (x1, lo, z0), (x0, hi, z0), (x1, hi, z0), (x0, hi, z1), (x1, hi, z1), (x0, lo, z0 + 0.02), (x1, lo, z0 + 0.02)],
                  "MI_HubBlk_Open")
        if rails:
            for xx in (x0, x1):
                part.hull([(xx - RAIL_T / 2, lo, z0), (xx + RAIL_T / 2, lo, z0), (xx - RAIL_T / 2, lo, z0 + RAIL_H), (xx + RAIL_T / 2, lo, z0 + RAIL_H),
                           (xx - RAIL_T / 2, hi, z1), (xx + RAIL_T / 2, hi, z1), (xx - RAIL_T / 2, hi, z1 + RAIL_H), (xx + RAIL_T / 2, hi, z1 + RAIL_H)],
                          "MI_HubBlk_Rail")


def build_stairs():
    p = Part("SM_HubBlk_Stairs")
    st = H.R(H.HUB["stair"])
    ds = H.prop("dock", "dock_stair")
    land = next(r for zid, r in H.open_rects("lower") if zid == "tally" and r[1] - r[0] < 5.0)
    zq, zl, zm = ds["from_floor"], ds["to_floor"], Z["middle"]
    # The dock stair: 22 steps up to the landing in the Tally House's aft wall; the rest of its run a landing at the foot
    # (entered from the quay through the arcade's opening). Enclosed, roofed by the middle deck.
    foot = land[0] - 22 * TREAD
    slab(p, (st[0], foot, st[2], st[3]), zq, "MI_HubBlk_Open")
    flight(p, foot, land[0], st[2], st[3], "+x", zq, zl, rails=False)
    p.box(land[0], st[1], st[2], st[3], zq, zl, "MI_HubBlk_Open")                         # under the landing
    wall_box(p, "y", st[3], st[0], st[1], zq, zm, "MI_HubBlk_Wall", WALL)
    wall_box(p, "x", st[0], st[2], st[3], zq, zm, "MI_HubBlk_Wall", WALL)
    wall_box(p, "y", st[2], st[0], st[1], zl, zm, "MI_HubBlk_Wall", WALL)                    # over the arcade wall (Quay part)
    slab(p, st, zm, "MI_HubBlk_Locked")
    # The Well's four flights.
    for f in H.flights():
        flight(p, f["x0"], f["x1"], f["y0"], f["y1"], f["up"], Z[f["from"]], Z[f["to"]])
    p.finish()


# --- gates ---------------------------------------------------------------------------------------------------------------
GATE_SIZE = {"G1": (1.2, DOOR_H), "G2": (2.4, 1.0), "G3": (0.9, 3.0), "G4": (1.6, 1.4), "G6": (3.0, 2.5)}


def gate_axis(gm):
    """Which wall line the gate stands on: ('x', line) spans y, ('y', line) spans x, or None (on the deck)."""
    lines = []
    for zid, r in H.open_rects(gm["level"]):
        lines += [("x", r[0], r[2], r[3]), ("x", r[1], r[2], r[3]), ("y", r[2], r[0], r[1]), ("y", r[3], r[0], r[1])]
    if gm["level"] == "main":
        lines += [("x", H.HUB["fcX"], -20.0, 20.0)]
    best = None
    for ax, ln, a0, a1 in lines:
        along, across = (gm["y"], gm["x"]) if ax == "x" else (gm["x"], gm["y"])
        if a0 - 0.3 <= along <= a1 + 0.3 and abs(across - ln) < 1.0:
            if best is None or abs(across - ln) < best[2]:
                best = (ax, ln, abs(across - ln), a0, a1)
    return best


def build_gates():
    p = Part("SM_HubBlk_Gates")
    for gm in H.GATE_MARKS:
        z = H.level_z(gm["level"])
        g = gm["g"]
        x, y = gm["x"], gm["y"]
        if g == "G5":
            kg = H.HUB["keelgate"]
            disc(p, kg["x"], kg["y"], kg["ring"], z, z + H.prop("keelgate", "keelgate")["raised"] * 0.6, "MI_HubBlk_Stone")
            disc(p, kg["x"], kg["y"], kg["r"], z, z + H.prop("keelgate", "keelgate")["raised"], "MI_HubBlk_Seal")
            continue
        if g == "G2" and gm["level"] == "main":
            # The cargo rope across the whole deck, a board in the middle: it stops the player.
            w = H.half_width(x, z) - 0.1
            p.box(x - 0.05, x + 0.05, -w, w, z, z + 1.0, "MI_HubBlk_Gate")
            step = H.prop("main_deck", "cargo_rope")["every"]
            k = -math.floor(w / step)
            while k * step <= w:
                p.box(x - 0.08, x + 0.08, k * step - 0.08, k * step + 0.08, z, z + 1.05, "MI_HubBlk_Rail")
                k += 1
            p.box(x - 0.15, x + 0.15, y - 0.6, y + 0.6, z + 0.5, z + 1.3, "MI_HubBlk_Gate", collide=False)
            continue
        found = gate_axis(gm)
        wdt, hgt = GATE_SIZE[g]
        z0 = z + (1.0 if g == "G4" else 0.0)
        if found is None and gm["level"] == "main" and abs(y) > 15.0:
            # Shrouds whose ratlines start out of reach: a chained plate at the bulwark.
            p.box(x - 0.75, x + 0.75, y - 0.1, y + 0.1, z, z + 2.5, "MI_HubBlk_Gate")
            continue
        if found is None:
            # A padlocked hatch on the deck.
            p.box(x - 0.8, x + 0.8, y - 0.8, y + 0.8, z, z + 0.3, "MI_HubBlk_Gate")
            continue
        ax, ln, _, a0, a1 = found
        along = y if ax == "x" else x
        if a1 - a0 >= wdt:
            along = min(max(along, a0 + wdt / 2), a1 - wdt / 2)
        if ax == "x":
            p.box(ln - WALL / 2 - 0.08, ln + WALL / 2 + 0.08, along - wdt / 2, along + wdt / 2, z0, z0 + hgt, "MI_HubBlk_Gate")
        else:
            p.box(along - wdt / 2, along + wdt / 2, ln - WALL / 2 - 0.08, ln + WALL / 2 + 0.08, z0, z0 + hgt, "MI_HubBlk_Gate")
    p.finish()


def disc(part, cx, cy, r, z0, z1, mat, sides=24, collide=True):
    pts = []
    for k in range(sides):
        t = 2 * math.pi * k / sides
        pts += [(cx + r * math.cos(t), cy + r * math.sin(t), z0), (cx + r * math.cos(t), cy + r * math.sin(t), z1)]
    part.hull(pts, mat, collide)


# --- props, masts, markers ---------------------------------------------------------------------------------------------------
def wall_prop(part, space_name, pr, floor, depth=0.12, mat="MI_HubBlk_Prop"):
    """A board on a wall (plans, rubbings, racks, shelves): 'wall' is 'x=64' or 'y=19'."""
    ax, val = pr["wall"].split("=")
    line = float(val)
    z0, z1 = floor + pr.get("z0", 0.0), floor + pr.get("z1", 2.0)
    d = pr.get("depth", depth)
    level = {"slip": "quay", "keelgate": "quay", "tally": "lower", "chandlery": "main"}[space_name]
    rects = [r for _, r in H.open_rects(level)]
    if ax == "x":
        mid = 0.5 * (pr["y0"] + pr["y1"])
        inner = 1.0 if any(H.inside(r, line + 0.5, mid) for r in rects) else -1.0          # the board faces the open floor
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


def simple_hull(part, x0, x1, yc, beam, zk, depth, mat):
    """A boat as a plain shape: pointed ends in plan, narrower at the keel."""
    L = x1 - x0
    top = [(x0, yc, zk + depth), (x0 + 0.2 * L, yc - beam / 2, zk + depth), (x1 - 0.25 * L, yc - beam / 2, zk + depth),
           (x1, yc, zk + depth * 1.05), (x1 - 0.25 * L, yc + beam / 2, zk + depth), (x0 + 0.2 * L, yc + beam / 2, zk + depth)]
    bot = [(x0 + 0.1 * L, yc, zk + 0.3 * depth), (x0 + 0.25 * L, yc - beam / 5, zk), (x1 - 0.3 * L, yc - beam / 5, zk),
           (x1 - 0.08 * L, yc, zk + 0.3 * depth), (x1 - 0.3 * L, yc + beam / 5, zk), (x0 + 0.25 * L, yc + beam / 5, zk)]
    part.hull(top + bot, mat)


def build_props():
    p = Part("SM_HubBlk_Props")
    zq, zl, zm = Z["quay"], Z["lower"], Z["main"]
    # The Slip.
    sl = {pr["id"]: pr for pr in H.space("slip")["props"]}
    c = sl["counter"]
    p.box(c["x0"], c["x1"], c["y0"], c["y1"], zq, zq + c["h"], "MI_HubBlk_Prop")
    wall_prop(p, "slip", sl["plans_board"], zq)
    wall_prop(p, "slip", sl["half_models"], zq, depth=0.4)
    wb = sl["workbench"]
    p.box(wb["x0"], wb["x1"], wb["y0"], wb["y1"], zq, zq + wb["h"], "MI_HubBlk_Prop")
    wall_prop(p, "slip", sl["timber_racks"], zq, depth=0.6)
    db = sl["display_boat"]
    simple_hull(p, db["x0"], db["x1"], db["y"], db["beam"], zq + db["keel_z"], 2.2, "MI_HubBlk_Gate")
    for cx in (50.0, 54.0, 58.0):                                                    # the cradles (display_boat note)
        p.box(cx - 0.2, cx + 0.2, db["y"] - 1.2, db["y"] + 1.2, zq, zq + db["keel_z"] + 0.2, "MI_HubBlk_Prop")
    mt = sl["mast_on_trestles"]
    p.box(mt["x0"], mt["x1"], mt["y"] - 0.2, mt["y"] + 0.2, zq + mt["z"], zq + mt["z"] + 0.4, "MI_HubBlk_Prop")
    for tx in (mt["x0"] + 2.0, mt["x1"] - 2.0):
        p.box(tx - 0.1, tx + 0.1, mt["y"] - 0.6, mt["y"] + 0.6, zq, zq + mt["z"], "MI_HubBlk_Prop")
    cr = sl["crane"]
    zb = zq + cr["bracket_z"]
    p.box(cr["x"] - 0.3, cr["x"] + 0.3, cr["y"] - 0.5, cr["y"], zb - 1.6, zb + 0.4, "MI_HubBlk_Prop", collide=False)   # the bracket
    jt = cr["jib_to"]
    pole(p, (cr["x"], cr["y"] - 0.3, zb), (jt[0], jt[1], zq + jt[2]), 0.2, 0.15, "MI_HubBlk_Prop", collide=False)
    pole(p, (cr["x"], cr["y"] - 1.2, zb - 1.4), (cr["x"], cr["y"] - 0.4, zb - 0.2), 0.12, 0.12, "MI_HubBlk_Prop", collide=False)  # its strut
    p.box(jt[0] - 0.02, jt[0] + 0.02, jt[1] - 0.02, jt[1] + 0.02, zq + 3.0, zq + jt[2], "MI_HubBlk_Rail", collide=False)   # the fall
    p.box(jt[0] - 0.15, jt[0] + 0.15, jt[1] - 0.15, jt[1] + 0.15, zq + 2.7, zq + 3.0, "MI_HubBlk_Rail", collide=False)    # the hook block
    wx, wy = cr["winch"]
    p.box(wx - 0.5, wx + 0.5, wy - 0.4, wy + 0.4, zq, zq + 0.9, "MI_HubBlk_Prop")                                         # the winch
    lanterns(p, sl["lanterns"], zq)
    sg = sl["sign"]
    p.box(sg["x"] - 0.3, sg["x"], sg["y"] - 3.0, sg["y"] + 3.0, zq + sg["z"] - 0.5, zq + sg["z"] + 0.5, "MI_HubBlk_Canvas", collide=False)
    # The Tally House.
    tl = {pr["id"]: pr for pr in H.space("tally")["props"]}
    pb = tl["pay_booth"]
    p.box(pb["x0"], pb["x1"], pb["y0"], pb["y0"] + 0.1, zl, zl + 1.0, "MI_HubBlk_Prop")          # the front under the hatch
    p.box(pb["x0"], pb["x1"], pb["y0"], pb["y0"] + 0.1, zl + 2.0, zl + pb["h"], "MI_HubBlk_Prop")
    p.box(pb["x0"], 51.5, pb["y0"], pb["y0"] + 0.1, zl + 1.0, zl + 2.0, "MI_HubBlk_Prop")      # (hatch x 51.5..54.5, its note)
    p.box(54.5, pb["x1"], pb["y0"], pb["y0"] + 0.1, zl + 1.0, zl + 2.0, "MI_HubBlk_Prop")
    p.box(pb["x0"], pb["x0"] + 0.1, pb["y0"], pb["y1"], zl, zl + pb["h"], "MI_HubBlk_Prop")
    dy0, dy1 = pb["door"]                                                                  # the door in its forward side
    for ya, yb in ((pb["y0"], dy0), (dy1, pb["y1"])):
        p.box(pb["x1"] - 0.1, pb["x1"], ya, yb, zl, zl + pb["h"], "MI_HubBlk_Prop")
    p.box(pb["x1"] - 0.1, pb["x1"], dy0, dy1, zl + DOOR_H, zl + pb["h"], "MI_HubBlk_Prop")
    p.box(pb["x0"], pb["x1"], pb["y0"], pb["y1"], zl + 2.7, zl + 2.8, "MI_HubBlk_Prop")
    p.box(pb["x0"], pb["x1"], 17.0, pb["y0"], zl + 0.95, zl + 1.0, "MI_HubBlk_Prop")          # the ledge (its note)
    for key in ("weighing_bench", "salt_cakes", "table"):
        r = tl[key]
        p.box(r["x0"], r["x1"], r["y0"], r["y1"], zl, zl + r["h"], "MI_HubBlk_Prop")
    t = tl["table"]
    for by0, by1 in ((14.2, 14.6), (11.8, 12.2)):                                         # the benches (table note)
        p.box(t["x0"], t["x1"], by0, by1, zl, zl + 0.45, "MI_HubBlk_Prop")
    wall_prop(p, "tally", tl["job_board"], zl)
    lanterns(p, tl["lanterns"], zl)
    # The Keelgate chamber.
    kg = {pr["id"]: pr for pr in H.space("keelgate")["props"]}
    dk = kg["desk"]
    p.box(dk["x0"], dk["x1"], dk["y0"], dk["y1"], zq, zq + dk["h"], "MI_HubBlk_Prop")
    ch_ = dk["chair"]
    p.box(ch_[0], ch_[1], ch_[2], ch_[3], zq, zq + 0.9, "MI_HubBlk_Prop")                   # the chair behind it
    wall_prop(p, "keelgate", kg["shelves"], zq)
    wall_prop(p, "keelgate", kg["rubbings"], zq, depth=0.05, mat="MI_HubBlk_Canvas")
    lanterns(p, kg["lanterns"], zq)
    # The hoist's cage at its quay stop, the gantry over the Well on the main deck.
    hs = H.R(H.HUB["hoist"])
    for cx in (hs[0], hs[1] - 0.1):
        for cy in (hs[2], hs[3] - 0.1):
            p.box(cx, cx + 0.1, cy, cy + 0.1, zq, zq + 2.4, "MI_HubBlk_Rail")
    p.box(hs[0], hs[1], hs[2], hs[3], zq + 2.4, zq + 2.5, "MI_HubBlk_Rail", collide=False)
    for gy in (hs[2], hs[3] - 0.1):                                                         # the guide posts (aft corners)
        p.box(hs[0] - 0.12, hs[0], gy, gy + 0.1, zq, Z["main"] + 7.5, "MI_HubBlk_Rail", collide=False)
    cw = H.prop("well", "counterweight")
    top_ = Z["main"] + 7.5 - 1.0
    p.box(cw["x0"], cw["x1"], cw["y0"], cw["y1"], top_ - cw["h"], top_, "MI_HubBlk_Prop", collide=False)   # up while the cage is down
    for (ax, ln, a0, a1) in (("y", cw["y0"] - 0.2, cw["x0"] - 0.2, cw["x1"] + 0.3), ("x", cw["x1"] + 0.3, cw["y0"] - 0.2, cw["y1"]),
                             ("x", cw["x0"] - 0.2, cw["y0"] - 0.2, cw["y1"])):
        wall_box(p, ax, ln, a0, a1, zq, zq + RAIL_H, "MI_HubBlk_Rail", RAIL_T)                # its bay on the chamber floor
    sh = H.R(H.HUB["shaft"])
    top = zm + 7.5                                                                          # gantry note: 7.5 m above the deck
    apex = (0.5 * (76.5 + 91.5), 0.0, top)
    for fx in (76.5, 91.5):
        pole(p, (fx, 0.0, zm), apex, 0.25, 0.2, "MI_HubBlk_Prop")
    hx, hy = 0.5 * (hs[0] + hs[1]), 0.5 * (hs[2] + hs[3])
    pole(p, apex, (hx, hy, top), 0.2, 0.18, "MI_HubBlk_Prop", collide=False)
    p.box(hx - 0.03, hx + 0.03, hy - 0.03, hy + 0.03, zq + 2.5, top, "MI_HubBlk_Rail", collide=False)
    # The Chandlery: the deckhouse (front = the counter, posts, door), pitched roof, shelves, table, the try-it spots.
    cd = {pr["id"]: pr for pr in H.space("chandlery")["props"]}
    dh = cd["deckhouse"]
    eav, ridge = dh["h"], dh["ridge"]
    p.box(dh["x0"], dh["x1"], dh["y0"], dh["y0"] + 0.3, zm, zm + eav, "MI_HubBlk_Wall")
    win = cd["window"]
    for (wx0, wx1) in ((dh["x0"], dh["x0"] + 0.3),):
        p.box(wx0, wx1, dh["y0"], dh["y1"], zm, zm + eav, "MI_HubBlk_Wall")
    p.box(dh["x1"] - 0.3, dh["x1"], dh["y0"], win["y0"], zm, zm + eav, "MI_HubBlk_Wall")
    p.box(dh["x1"] - 0.3, dh["x1"], win["y1"], dh["y1"], zm, zm + eav, "MI_HubBlk_Wall")
    p.box(dh["x1"] - 0.3, dh["x1"], win["y0"], win["y1"], zm, zm + win["z0"], "MI_HubBlk_Wall")
    p.box(dh["x1"] - 0.3, dh["x1"], win["y0"], win["y1"], zm + win["z1"], zm + eav, "MI_HubBlk_Wall")
    cf = cd["counter_front"]
    p.box(cf["x0"], cf["x1"], cf["y0"], cf["y1"], zm, zm + cf["h"], "MI_HubBlk_Prop")
    p.box(cf["x0"], cf["x1"], cf["y0"] - 0.2, cf["y1"], zm + cf["h"], zm + cf["top"], "MI_HubBlk_Prop")   # the top board
    for px0, px1 in ((98.0, 99.5), (110.0, 111.0), (113.0, 114.0)):                         # the posts (counter_front note)
        p.box(px0, px1, cf["y0"], cf["y1"], zm, zm + eav, "MI_HubBlk_Wall")
    door = cd["door"]
    p.box(door["x0"], door["x1"], cf["y0"], cf["y1"], zm + DOOR_H + 0.2, zm + eav, "MI_HubBlk_Wall")    # over the door
    aw = cd["awning"]
    p.box(cf["x0"], cf["x1"], cf["y0"], cf["y1"], zm + aw["from"][1], zm + eav, "MI_HubBlk_Wall")     # the fascia over the opening
    for s in (-1, 1):                                                                         # the pitched roof
        yo = dh["y0"] if s < 0 else dh["y1"]
        yr = 0.5 * (dh["y0"] + dh["y1"])
        p.hull([(dh["x0"] - 0.3, yo, zm + eav), (dh["x1"] + 0.3, yo, zm + eav), (dh["x0"] - 0.3, yr, zm + ridge), (dh["x1"] + 0.3, yr, zm + ridge),
                (dh["x0"] - 0.3, yo, zm + eav + 0.2), (dh["x1"] + 0.3, yo, zm + eav + 0.2), (dh["x0"] - 0.3, yr, zm + ridge + 0.2),
                (dh["x1"] + 0.3, yr, zm + ridge + 0.2)], "MI_HubBlk_Locked", collide=False)
        p.hull([(xg, yo, zm + eav), (xg, yr, zm + ridge), (xg, yo, zm + eav - 0.01), (xg + 0.3, yo, zm + eav), (xg + 0.3, yr, zm + ridge),
                (xg + 0.3, yr, zm + eav)] if False else [(dh["x0"], yo, zm + eav), (dh["x0"], yr, zm + ridge), (dh["x0"], yr, zm + eav),
                                                         (dh["x0"] + 0.3, yo, zm + eav), (dh["x0"] + 0.3, yr, zm + ridge), (dh["x0"] + 0.3, yr, zm + eav)],
               "MI_HubBlk_Wall", collide=False)
        p.hull([(dh["x1"] - 0.3, yo, zm + eav), (dh["x1"] - 0.3, yr, zm + ridge), (dh["x1"] - 0.3, yr, zm + eav),
                (dh["x1"], yo, zm + eav), (dh["x1"], yr, zm + ridge), (dh["x1"], yr, zm + eav)], "MI_HubBlk_Wall", collide=False)
    # The awning on two posts.
    (ay0, az0), (ay1, az1) = aw["from"], aw["to"]
    p.hull([(aw["x0"], ay0, zm + az0), (aw["x1"], ay0, zm + az0), (aw["x0"], ay1, zm + az1), (aw["x1"], ay1, zm + az1),
            (aw["x0"], ay0, zm + az0 + 0.05), (aw["x1"], ay0, zm + az0 + 0.05), (aw["x0"], ay1, zm + az1 + 0.05), (aw["x1"], ay1, zm + az1 + 0.05)],
           "MI_HubBlk_Canvas", collide=False)
    for ax_ in (aw["x0"], aw["x1"]):
        p.box(ax_ - 0.06, ax_ + 0.06, ay1 - 0.06, ay1 + 0.06, zm, zm + az1, "MI_HubBlk_Prop")
    sgn = cd["sign"]
    p.box(sgn["x0"], sgn["x1"], -8.0 + 0.0, -8.0 + 0.12, zm + sgn["z0"], zm + sgn["z1"], "MI_HubBlk_Canvas", collide=False)
    shv = cd["shelves"]
    for sx0, sx1, sy0, sy1 in shv["runs"]:
        p.box(sx0, sx1, sy0, sy1, zm, zm + shv["h"], "MI_HubBlk_Prop")
    dt = cd["display_table"]
    p.box(dt["x0"], dt["x1"], dt["y0"], dt["y1"], zm, zm + dt["h"], "MI_HubBlk_Prop")
    for bx in cd["beams"]["at_x"]:
        p.box(bx - 0.15, bx + 0.15, dh["y0"], dh["y1"], zm + cd["beams"]["z"] - 0.15, zm + cd["beams"]["z"] + 0.15, "MI_HubBlk_Prop", collide=False)
    for key in ("tripod_spyglass", "binnacle"):
        r = cd[key]
        p.box(r["x"] - 0.25, r["x"] + 0.25, r["y"] - 0.25, r["y"] + 0.25, zm, zm + r["h"], "MI_HubBlk_Prop")
    bc = cd["barrels_crates"]
    p.box(bc["x0"], bc["x1"], bc["y0"], bc["y1"], zm, zm + 1.0, "MI_HubBlk_Prop")
    # The main deck: fife rails round the masts, hatches, the ship's boats, capstan, the galley's chimney.
    md = {pr["id"]: pr for pr in H.space("main_deck")["props"]}
    for mx, my in md["masts"]["at"]:
        for (fx0, fx1, fy0, fy1) in ((mx - 2, mx + 2, my - 2, my - 1.85), (mx - 2, mx + 2, my + 1.85, my + 2), (mx - 2, mx - 1.85, my - 2, my + 2),
                                     (mx + 1.85, mx + 2, my - 2, my + 2)):
            zb = zm if mx < H.HUB["fcX"] else Z["fcTop"]
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
    scale = {"main": 1.0, "fore": 0.92, "mizzen": 0.8}           # outside.masts: yards (main; fore x 0.92, mizzen x 0.8)
    for m in H.HUB["masts"]:
        x, h = m["x"], m["h"]
        pole(p, (x, 0.0, zm - 0.5), (x, 0.0, zm + h), r_deck, 0.35, "MI_HubBlk_Prop", sides=12)
        p.box(x - 2.0, x + 2.0, -7.0, 7.0, zm + 0.38 * h - 0.25, zm + 0.38 * h + 0.25, "MI_HubBlk_Prop", collide=False)   # the top, 14 m
        p.box(x - 0.2, x + 0.2, -3.2, 3.2, zm + 0.68 * h - 0.2, zm + 0.68 * h + 0.2, "MI_HubBlk_Prop", collide=False)     # crosstrees 6.4 m
        for frac, half in ((0.27, 0.42), (0.47, 0.35), (0.62, 0.26), (0.80, 0.18)):
            hl = half * H.HUB["B"] * scale[m["id"]]
            zy = zm + frac * h
            p.box(x - 0.35, x + 0.35, -hl, hl, zy - 0.35, zy + 0.35, "MI_HubBlk_Prop", collide=False)
            p.box(x - 0.1, x + 0.9, -hl * 0.92, hl * 0.92, zy - 1.2, zy - 0.3, "MI_HubBlk_Canvas", collide=False)       # furled
    p.finish()


def build_markers():
    p = Part("SM_HubBlk_Markers", ucx=False)
    floors = {"slip": Z["quay"], "tally": Z["lower"], "keelgate": Z["quay"], "chandlery": Z["main"]}
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
    dock = {pr["id"]: pr for pr in H.space("dock")["props"]}
    fb = dock["fitting_berth"]
    berths = [(fb["x0"], fb["x1"], fb["y"])] + [(b["x0"], b["x1"], b["y"]) for b in dock["fleet_berths"]["berths"]]
    for x0, x1, y in berths:
        edge = math.copysign(H.HUB["basin"]["y1"], y)
        p.box(x0, x1, edge - 0.15, edge + 0.15, Z["quay"], Z["quay"] + 0.04, "MI_HubBlk_MarkBerth", collide=False)
        p.box(x0, x1, y - 0.05, y + 0.05, 0.02, 0.08, "MI_HubBlk_MarkBerth", collide=False)       # the berth's line on the water
    p.finish()


BUILDERS = {"shell": build_shell, "basin": build_basin, "decks": build_decks, "stairs": build_stairs, "gates": build_gates,
            "props": build_props, "masts": build_masts, "markers": build_markers}

if __name__ == "__main__":
    wanted = sb.script_args() or list(BUILDERS)
    for key in wanted:
        sb.reset_scene()
        BUILDERS[key]()
    print("SB_HUBBLK_OK", wanted)
