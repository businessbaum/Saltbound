"""The Gale (D87): the storm wall round the sea, the storms that break out of it, their rain and lightning.

  SM_Gale_Wall_A/B/C  three segments of the wall, each about 1.3 km along the ring: a wall of chunky, soft cloud masses (metaballs,
                      like Sea of Thieves' storm clouds), one
                      with an anvil leaning inward. ASaltGale sets about 50 of them round the ring (9.5 km out), turned and scaled.
                      Frame: the pivot on the sea at the ring; +x points out of the sea (away from its middle), the face seen from
                      the sea is the -x side; y runs along the ring.
  SM_Gale_Cell        a storm that broke out of the wall: a cumulonimbus, a dark column 1.8 km across with towers and an anvil
                      3.2 km high; the pivot on the sea under its middle.
  SM_Gale_RainShaft   a unit open cylinder (radius 1, height 1, pivot at the bottom middle) for the rain under a cell, scaled.
  SM_Gale_RainSheet   a unit sheet (1 x 1 in y and z, pivot at the bottom middle, facing -x) for the rain curtains at the wall.
  SM_Gale_Bolt_A/B/C  forked lightning: thin tubes along a jagged path, 1.4-2.4 km from the cloud down to the sea; the pivot is
                      the strike point on the sea.
Vertex colour (the clouds): R the height in the mass (0 base, 1 top), G how open the surface is (0 deep in a crease, 1 on a
bulge: the material darkens creases), B a per-mass random value (the material varies the grey between masses).
Run: Tools/Build-BlenderAsset.ps1 build_gale
"""
import math
import os
import random
import sys

import bmesh
import bpy
from mathutils import Vector

sys.path.insert(0, os.path.dirname(__file__))
import sb_blender as sb  # noqa: E402

PREVIEW = "--preview" in sys.argv
# Half a segment along the ring: 52 segments on the 9.5 km ring are 1147.6 m apart (chord), so each is cut at +-574 m.
WALL_HALF = 574.0


def U(x, y, z):
    """Unreal's frame (x forward, y right) to Blender's (y mirrored)."""
    return Vector((x, -y, z))


def metaballs(name, balls, resolution, threshold=0.45):
    """A mesh from metaball elements: (centre (Unreal frame, m), radius, (sx, sy, sz) stretch). The radius given is the mass's
    visible one: at this threshold and stiffness a lone element's surface stands at about 0.62 of its own radius."""
    mb = bpy.data.metaballs.new(name + "_mb")
    mb.resolution = resolution
    mb.render_resolution = resolution
    mb.threshold = threshold
    for c, r, s in balls:
        e = mb.elements.new()
        e.type = "ELLIPSOID"
        e.co = U(*c)
        e.radius = r / 0.62
        e.size_x, e.size_y, e.size_z = s
        e.stiffness = 2.0
    obj = bpy.data.objects.new(name + "_obj", mb)
    bpy.context.scene.collection.objects.link(obj)
    bpy.ops.object.select_all(action="DESELECT")
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    bpy.context.view_layer.update()
    bpy.ops.object.convert(target="MESH")
    mesh_obj = bpy.context.view_layer.objects.active
    mesh_obj.name = name
    mesh_obj.data.name = name
    return mesh_obj


def cut_ends(obj, half):
    """Cut a wall segment flush at +-half along the ring (Unreal y; Blender's is mirrored, the cut is symmetric) and drop what lies
    beyond: the segments abut on the ring instead of overlapping (translucent overlaps showed as vertical bands)."""
    bm = bmesh.new()
    bm.from_mesh(obj.data)
    for sign in (1.0, -1.0):
        bmesh.ops.bisect_plane(bm, geom=bm.verts[:] + bm.edges[:] + bm.faces[:], plane_co=(0.0, sign * half, 0.0),
                               plane_no=(0.0, sign, 0.0), clear_outer=True)
    bm.to_mesh(obj.data)
    bm.free()
    return obj


def finish_cloud(obj, keep, seed, masses):
    """Decimate to a light mesh, shade smooth, and write the vertex colours (height, openness, mass)."""
    dec = obj.modifiers.new("dec", "DECIMATE")
    dec.ratio = keep
    sb.apply_modifiers(obj)
    me = obj.data
    for p in me.polygons:
        p.use_smooth = True
    bm = bmesh.new()
    bm.from_mesh(me)
    bm.verts.ensure_lookup_table()
    zs = [v.co.z for v in bm.verts]
    z0, z1 = min(zs), max(zs)
    # openness: how far a vertex stands out of the mean of its neighbourhood along its normal, smoothed (creases < 0 < bulges)
    raw = {}
    for v in bm.verts:
        if not v.link_edges:
            raw[v.index] = 0.0
            continue
        mean = sum((e.other_vert(v).co for e in v.link_edges), Vector()) / len(v.link_edges)
        raw[v.index] = (v.co - mean).dot(v.normal)
    for _ in range(4):
        raw = {v.index: (raw[v.index] + sum(raw[e.other_vert(v).index] for e in v.link_edges)) / (1 + len(v.link_edges)) for v in bm.verts}
    vals = sorted(raw.values())
    lo, hi = vals[int(len(vals) * 0.05)], vals[int(len(vals) * 0.95)]
    rng = random.Random(seed)
    tint = [rng.random() for _ in masses]
    col = bm.loops.layers.color.new("Col")
    for f in bm.faces:
        for loop in f.loops:
            v = loop.vert
            h = (v.co.z - z0) / max(z1 - z0, 1.0)
            o = min(max((raw[v.index] - lo) / max(hi - lo, 1e-6), 0.0), 1.0)
            # the mass this vertex belongs to: the nearest centre (scaled by its radius)
            best, bi = 1e18, 0
            p = Vector((v.co.x, -v.co.y, v.co.z))
            for i, (c, r, s) in enumerate(masses):
                d = (p - Vector(c)).length / r
                if d < best:
                    best, bi = d, i
            loop[col] = (h, o, tint[bi], 1.0)
    bm.to_mesh(me)
    bm.free()
    sb.world_box_uv(obj, metres_per_uv=200.0)
    return obj


def heap(rng, out, cx, cy, cz, r, n):
    """A cauliflower head: a central mass with n smaller ones round its upper half."""
    out.append(((cx, cy, cz), r, (1.0, 1.0, 0.95)))
    for k in range(n):
        a = rng.uniform(0, math.tau)
        up = rng.uniform(0.15, 0.75)
        d = r * rng.uniform(0.55, 0.8)
        out.append(((cx + math.cos(a) * d * (1 - up * 0.5), cy + math.sin(a) * d * (1 - up * 0.5), cz + r * up * 0.8), r * rng.uniform(0.45, 0.65), (1.0, 1.0, 1.0)))


def wall_masses(seed, anvil=False):
    """The masses of one wall segment (Unreal frame, m): one continuous wall of storm from the sea to a rolling top (owner: "no
    pillars, just a big wall of a storm"). Rows of overlapping masses fill the wall 500 m deep; its top line rolls between about 2.0
    and 2.5 km and comes back to 2.2 km at both ends, so neighbouring segments meet at the same height; billows on the face seen from
    the sea break it up."""
    rng = random.Random(seed)
    half = WALL_HALF
    ends = 2200.0
    ph1, ph2 = rng.uniform(0, math.tau), rng.uniform(0, math.tau)

    def top(y):
        t = (y + half) / (2 * half)                      # 0 .. 1 along the segment
        bulge = math.sin(math.pi * min(max(t, 0.0), 1.0))  # 0 at the ends
        return ends + bulge * (180.0 * math.sin(2.7 * math.tau * t + ph1) + 120.0 * math.sin(5.3 * math.tau * t + ph2))

    out = []
    # the body: rows of masses from the sea up, two deep (the face and behind it)
    for row_x in (0.0, 380.0):
        z = 250.0
        while True:
            y = -half - 250.0
            r_row = rng.uniform(380, 460)
            while y < half + 250.0:
                h = top(y)
                if z < h - r_row * 0.6:
                    r = r_row * rng.uniform(0.9, 1.1)
                    out.append(((row_x + rng.uniform(-70, 70), y + rng.uniform(-60, 60), z + rng.uniform(-60, 60)), r, (1.0, 1.0, 0.85)))
                y += r_row * rng.uniform(0.75, 0.95)
            z += r_row * 0.8
            if z > ends + 400.0:
                break
    # the rolling top: a row of broad, low heads along the top line
    y = -half - 200.0
    while y < half + 200.0:
        r = rng.uniform(330, 420)
        h = top(y)
        out.append(((rng.uniform(100, 300), y, h - r * 0.55), r, (1.15, 1.15, 0.7)))
        y += r * rng.uniform(0.8, 1.0)
    # billows on the face seen from the sea, at every height
    for _ in range(40):
        y = rng.uniform(-half, half)
        z = rng.uniform(150.0, top(y) - 250.0)
        out.append(((rng.uniform(-300, -200), y, z), rng.uniform(170, 290), (1.0, 1.0, 0.85)))
    return out


def cell_masses(seed):
    rng = random.Random(seed)
    out = []
    # the dark base, wide and low
    for k in range(10):
        a = k / 10 * math.tau
        out.append(((math.cos(a) * 560, math.sin(a) * 560, rng.uniform(300, 420)), rng.uniform(440, 540), (1.0, 1.0, 0.75)))
    out.append(((0, 0, 450), 650, (1.0, 1.0, 0.75)))
    # two columns of heaps leaning together, broad at the foot
    for cx, cy, top in ((-150.0, -220.0, 2700.0), (220.0, 260.0, 2300.0)):
        z, r = 850.0, 640.0
        while z < top:
            heap(rng, out, cx + rng.uniform(-120, 120), cy + rng.uniform(-120, 120), z, r, 7)
            z += r * 0.8
            r *= 0.88
    # the anvil, blown downwind (+x): flattened masses spreading to one side from the top of the column
    for k in range(11):
        d = rng.uniform(0, 1)
        out.append(((-200 + d * 1300 + rng.uniform(-150, 150), rng.uniform(-550, 550) * (0.6 + 0.6 * d), rng.uniform(2550, 2750) + d * 150),
                    rng.uniform(420, 520) * (1.1 - 0.35 * d), (1.4, 1.3, 0.62)))
    # billows round the column
    for _ in range(20):
        a = rng.uniform(0, math.tau)
        out.append(((math.cos(a) * 650, math.sin(a) * 650, rng.uniform(300, 2200)), rng.uniform(200, 300), (1.0, 1.0, 0.9)))
    return out


def rain_shaft():
    bpy.ops.mesh.primitive_cylinder_add(vertices=32, radius=1.0, depth=1.0, end_fill_type="NOTHING", location=(0, 0, 0.5))
    obj = bpy.context.active_object
    obj.name = obj.data.name = "SM_Gale_RainShaft"
    bm = bmesh.new()
    bm.from_mesh(obj.data)
    uv = bm.loops.layers.uv.new("UVMap")
    for f in bm.faces:
        for loop in f.loops:
            co = loop.vert.co
            a = math.atan2(co.y, co.x) / math.tau + 0.5
            loop[uv].uv = (a * 6.0, co.z)
    # seam: faces that wrap get their low side moved up by one turn
    for f in bm.faces:
        us = [l[uv].uv.x for l in f.loops]
        if max(us) - min(us) > 3.0:
            for l in f.loops:
                if l[uv].uv.x < 3.0:
                    l[uv].uv.x += 6.0
    bm.to_mesh(obj.data)
    bm.free()
    return obj


def rain_sheet():
    """A unit sheet in the y-z plane facing -x (Unreal), gently bowed, 16 x 4 quads."""
    me = bpy.data.meshes.new("SM_Gale_RainSheet")
    bm = bmesh.new()
    nu, nv = 16, 4
    verts = [[bm.verts.new(U(-0.04 * (1 - (2 * i / nu - 1) ** 2), i / nu - 0.5, j / nv)) for j in range(nv + 1)] for i in range(nu + 1)]
    uv = bm.loops.layers.uv.new("UVMap")
    for i in range(nu):
        for j in range(nv):
            f = bm.faces.new((verts[i][j], verts[i + 1][j], verts[i + 1][j + 1], verts[i][j + 1]))
            for loop, (a, b) in zip(f.loops, ((i, j), (i + 1, j), (i + 1, j + 1), (i, j + 1))):
                loop[uv].uv = (a / nu * 4.0, b / nv)
    bm.normal_update()
    bm.to_mesh(me)
    bm.free()
    obj = sb.new_object("SM_Gale_RainSheet", me)
    # face -x: flip if the normals point +x
    if obj.data.polygons[0].normal.x > 0:
        bpy.context.view_layer.objects.active = obj
        for p in obj.data.polygons:
            p.flip()
    return obj


def bolt(name, seed, height):
    """A forked bolt: a jagged main channel from the cloud to the sea and two or three branches, as thin hexagonal tubes."""
    rng = random.Random(seed)
    paths = []
    p = Vector((0, 0, 0))
    main = [p.copy()]
    z = 0.0
    while z < height:
        step = rng.uniform(40, 110)
        z = min(z + step, height)
        p = Vector((main[-1].x + rng.uniform(-45, 45), main[-1].y + rng.uniform(-45, 45), z))
        main.append(p)
    paths.append((main, 6.0))
    for b in range(rng.choice([2, 3, 3])):
        k = rng.randrange(len(main) // 3, len(main) - 2)
        q = main[k].copy()
        br = [q.copy()]
        d = Vector((rng.uniform(-1, 1), rng.uniform(-1, 1), 0)).normalized()
        for s in range(rng.randrange(4, 9)):
            q = Vector((q.x + d.x * rng.uniform(30, 80) + rng.uniform(-25, 25), q.y + d.y * rng.uniform(30, 80) + rng.uniform(-25, 25), q.z - rng.uniform(30, 90)))
            br.append(q)
        paths.append((br, 3.5))
    me = bpy.data.meshes.new(name)
    bm = bmesh.new()
    uvl = bm.loops.layers.uv.new("UVMap")
    for pts, rad in paths:
        rings = []
        for i, c in enumerate(pts):
            t = (pts[min(i + 1, len(pts) - 1)] - pts[max(i - 1, 0)]).normalized()
            a = t.orthogonal().normalized()
            b = t.cross(a)
            r = rad * (1.0 - 0.6 * i / len(pts))
            rings.append([bm.verts.new(U(*(c + (a * math.cos(k / 6 * math.tau) + b * math.sin(k / 6 * math.tau)) * r))) for k in range(6)])
        for i in range(len(rings) - 1):
            for k in range(6):
                f = bm.faces.new((rings[i][k], rings[i][(k + 1) % 6], rings[i + 1][(k + 1) % 6], rings[i + 1][k]))
                for loop in f.loops:
                    loop[uvl].uv = (0.5, i / len(rings))
    bm.normal_update()
    bm.to_mesh(me)
    bm.free()
    return sb.new_object(name, me)


def preview(objs, png, dist, el=8.0, az=200.0):
    """A workbench render from the sea side (D53: look before importing)."""
    scene = bpy.context.scene
    scene.render.engine = "BLENDER_WORKBENCH"
    scene.display.shading.light = "STUDIO"
    scene.display.shading.color_type = "SINGLE"
    scene.display.shading.single_color = (0.55, 0.57, 0.6)
    scene.render.resolution_x, scene.render.resolution_y = 1280, 720
    for o in bpy.data.objects:
        o.hide_render = o not in objs
    lo = Vector((min(o.bound_box[i][0] + o.location.x for o in objs for i in range(8)), min(o.bound_box[i][1] + o.location.y for o in objs for i in range(8)), 0))
    hi = Vector((max(o.bound_box[i][0] + o.location.x for o in objs for i in range(8)), max(o.bound_box[i][1] + o.location.y for o in objs for i in range(8)),
                 max(o.bound_box[i][2] for o in objs for i in range(8))))
    tgt = (lo + hi) / 2
    cam = bpy.data.objects.get("PreviewCam") or bpy.data.objects.new("PreviewCam", bpy.data.cameras.new("PreviewCam"))
    if cam.name not in scene.collection.objects:
        scene.collection.objects.link(cam)
    cam.data.clip_end = 100000
    a, e = math.radians(az), math.radians(el)
    cam.location = tgt + Vector((math.cos(a) * math.cos(e), math.sin(a) * math.cos(e), math.sin(e))) * dist
    cam.rotation_euler = (tgt - cam.location).to_track_quat("-Z", "Y").to_euler()
    scene.camera = cam
    scene.render.filepath = png
    bpy.ops.render.render(write_still=True)
    print("SB_PREVIEW", png)


def main():
    """D87: only the rain round the camera and the bolts are in use; the wall's segments and the breakout's cloud (wall_masses,
    cell_masses) were the first look of the wall, rejected by the owner (pillars, then mesh edges); --walls still builds them."""
    sb.reset_scene()
    objs = [rain_shaft()] + [bolt("SM_Gale_Bolt_A", 3, 1900.0), bolt("SM_Gale_Bolt_B", 7, 1500.0), bolt("SM_Gale_Bolt_C", 13, 2300.0)]
    if "--walls" in sys.argv:
        for name, seed in (("SM_Gale_Wall_A", 11), ("SM_Gale_Wall_B", 23), ("SM_Gale_Wall_C", 37)):
            masses = wall_masses(seed)
            obj = metaballs(name, masses, resolution=34.0)
            cut_ends(obj, WALL_HALF)
            objs.append(finish_cloud(obj, keep=0.22, seed=seed, masses=masses))
    for o in objs:
        mat = "MI_Gale_Bolt" if o.name.startswith("SM_Gale_Bolt") else "MI_Gale_Rain" if o.name == "SM_Gale_RainShaft" else "MI_Gale_Cloud"
        o.data.materials.clear()
        o.data.materials.append(sb.get_material(mat))
        sb.export_fbx([o], os.path.join("Gale", o.name + ".fbx"))
    sb.save_blend("SM_Gale")


if __name__ == "__main__":
    main()
