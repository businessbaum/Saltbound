"""Digging (D73, M6 step 2): the signs of something buried, the hole and the heap, the finds, and the shovel.

  SM_Dig_Mound    a low mound of loose sand, 1.1 m across and 16 cm high, a few lumps on it: something was buried here
  SM_Dig_Hole     what digging leaves: a ring of thrown-up sand round a dark, damp patch (the ground hides anything below its
                  surface, so the hole reads by its rim and its dark middle); ASaltDigSpot grows it with the digging
  SM_Dig_Heap     the heap of thrown sand beside the hole, 70 cm long and 26 cm high; it grows with the digging
  SM_Dig_Stake    a weathered stake leaning in the sand, a faded red rag tied round its head (a sailor's mark)
  SM_Dig_Bundle   the Cartographer's cache: an oilcloth bundle tied with rope, 52 cm long
  SM_Dig_Box      a sailor's cache: a small planked box with iron corners, a lid and a rope handle, 44 x 30 x 26 cm
  SM_Tool_Shovel  the shovel: an ash shaft with a T-grip, a socket and a rounded iron blade with two rivets (held, D55)

The sand pieces carry the island's ground material (MI_HomeGround, D71: projected from the top in world space) with the
ground's own vertex code (R grass, G litter, B path earth, A tone): dry sand on the mound and the heap, the hole's middle
and the heap's inside damp and dark (earth). Their edges sink a few centimetres below zero so they melt into the ground.
Origins on the ground; Unreal axes (+X forward). Run: Tools/Build-BlenderAsset.ps1 build_dig [mound hole heap stake bundle box shovel]
"""
import math
import os
import random
import sys

import bmesh
import bpy
from mathutils import Vector

sys.path.insert(0, os.path.dirname(__file__))
import build_boat as bb  # noqa: E402  (pieces: rod, ring, block, lathe, finish, materials)
import sb_blender as sb  # noqa: E402
import sb_timber as tb  # noqa: E402

GROUND = "MI_HomeGround"


def mats():
    bb.mats()
    bb.M[GROUND] = sb.get_material(GROUND, (0.78, 0.68, 0.5, 1))


def noise2(x, y, seed, scale):
    """Smooth value noise in [-1, 1] (a few sines, enough for lumps)."""
    rng = random.Random(seed)
    s = 0.0
    for k in range(4):
        a = rng.uniform(0, 2 * math.pi)
        f = rng.uniform(0.7, 1.4) / scale * (1.7 ** k)
        s += math.sin((x * math.cos(a) + y * math.sin(a)) * f + rng.uniform(0, 6.28)) / (1.6 ** k)
    return s / 1.8


def radial_patch(name, radius, height_fn, earth_fn, rings=14, segs=40, center=(0.0, 0.0), stretch=(1.0, 1.0)):
    """A round patch of ground: rings round the middle, z = height_fn(x, y, r01), vertex B = earth_fn(r01, x, y).
    Its UV0 is the ground's top projection (1 m a tile), UV1 the material's flat code (0.5, 0.5)."""
    bm = bmesh.new()
    verts = [[bm.verts.new(Vector((center[0], -center[1], height_fn(center[0], center[1], 0.0))))]]
    for i in range(1, rings + 1):
        r01 = i / rings
        row = []
        for j in range(segs):
            a = 2 * math.pi * j / segs
            x = center[0] + radius * r01 * math.cos(a) * stretch[0]
            y = center[1] + radius * r01 * math.sin(a) * stretch[1]
            row.append(bm.verts.new(Vector((x, -y, height_fn(x, y, r01)))))
        verts.append(row)
    for j in range(segs):
        bm.faces.new([verts[0][0], verts[1][j], verts[1][(j + 1) % segs]])
    for i in range(1, rings):
        for j in range(segs):
            a, b = verts[i][j], verts[i][(j + 1) % segs]
            c, d = verts[i + 1][(j + 1) % segs], verts[i + 1][j]
            bm.faces.new([a, d, c, b])
    bm.normal_update()
    if sum(f.normal.z for f in bm.faces) < 0.0:
        bmesh.ops.reverse_faces(bm, faces=list(bm.faces))   # a patch of ground faces up
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    for p in me.polygons:
        p.use_smooth = True
    obj = sb.new_object(name, me)
    obj.data.materials.append(bb.M[GROUND])
    # Vertex code of the island's ground and the material's second UV.
    col = obj.data.color_attributes.new("Col", "FLOAT_COLOR", "POINT")
    for v in obj.data.vertices:
        x, y = v.co.x, -v.co.y
        r01 = math.hypot((x - center[0]) / (radius * stretch[0]), (y - center[1]) / (radius * stretch[1]))
        col.data[v.index].color = (0.0, 0.0, max(0.0, min(1.0, earth_fn(min(r01, 1.0), x, y))), 0.5)
    obj.data.color_attributes.active_color = col
    # As SM_Home_Ground (build_home_island.ground_object): UV0 the top projection in metres (tangents), UV1 "Data" (ripple
    # 0.5 none, curvature 0.5 flat).
    uv0 = obj.data.uv_layers.new(name="UVMap")
    uv1 = obj.data.uv_layers.new(name="Data")
    for loop in obj.data.loops:
        co = obj.data.vertices[loop.vertex_index].co
        uv0.data[loop.index].uv = (co.x, co.y)
        uv1.data[loop.index].uv = (0.5, 0.5)
    return obj


def export(obj, folder="Dig"):
    sb.save_blend(obj.name)
    sb.export_fbx([obj], os.path.join(folder, obj.name + ".fbx"))
    print("SB_DIG", obj.name, len(obj.data.polygons))


def bell(r01, sink=0.03):
    """1 in the middle, 0 at the edge, with the edge sunk a little below the ground."""
    t = max(0.0, 1.0 - r01 * r01)
    return t * t, -sink * (r01 ** 6)


def lumpy_radius(x, y, seed, amount):
    """The outline's wobble: a round patch would read as made by a machine."""
    a = math.atan2(y, x)
    return 1.0 + amount * (0.6 * math.sin(3 * a + seed) + 0.4 * math.sin(5 * a + 2 * seed))


def build_mound():
    # Low and broad with a soft, uneven top and a few lumps, its outline wobbling.
    def h(x, y, r01):
        rr = r01 / lumpy_radius(x, y, 1.3, 0.12)
        b = max(0.0, 1.0 - rr * rr) ** 1.5
        lumps = 0.03 * noise2(x, y, 3, 0.12) * b + 0.015 * noise2(x, y, 8, 0.05) * b
        return 0.13 * b + lumps - 0.03 * (r01 ** 6)
    obj = radial_patch("SM_Dig_Mound", 0.6, h, lambda r01, x, y: 0.12 * (1 - r01), rings=16, stretch=(1.0, 0.85))
    export(obj)


def build_hole():
    # A ring of thrown sand (crest at 0.3 m, 9 cm high) round a dark, damp middle that dips just below the ground.
    def h(x, y, r01):
        r = r01 * 0.5
        rim = 0.09 * math.exp(-((r - 0.3) / 0.075) ** 2) * (1 + 0.25 * noise2(x, y, 5, 0.1))
        dip = -0.012 * max(0.0, 1 - r / 0.24)
        edge = -0.03 * (r01 ** 6)
        return rim + dip + edge + 0.004
    obj = radial_patch("SM_Dig_Hole", 0.5, h, lambda r01, x, y: 1.0 if r01 < 0.42 else max(0.0, 1.0 - (r01 - 0.42) / 0.3) * 0.6,
                       rings=18, segs=48)
    export(obj)


def build_heap():
    # Thrown spadefuls: a broad, lumpy pile, flatter than a cone, with clods on its flanks.
    def h(x, y, r01):
        rr = r01 / lumpy_radius(x, y, 4.1, 0.15)
        b = max(0.0, 1.0 - rr * rr) ** 1.3
        clods = (0.045 * noise2(x, y, 9, 0.07) + 0.02 * noise2(x, y, 13, 0.035)) * min(1.0, b * 2.0)
        return 0.2 * b + clods - 0.03 * (r01 ** 6)
    obj = radial_patch("SM_Dig_Heap", 0.4, h, lambda r01, x, y: 0.75 * (1 - r01) + 0.15, rings=16, stretch=(1.0, 0.8))
    export(obj)


def flat_band(name, radius, width, center_z, mat, squash=1.0):
    """A strip of cloth or rope wrapped round something upright: a torus pressed flat against it."""
    band = bb.ring(name, radius, width * 0.5, (0.0, 0.0, center_z), (0, 0, 1), mat, 18)
    for v in band.data.vertices:
        d = Vector((v.co.x, v.co.y, 0.0))
        r = d.length
        v.co.z = center_z + (v.co.z - center_z) * 1.6
        if r > 1e-6:
            # Pull the tube's outer half in: a flat band rather than a round tube.
            k = radius + (r - radius) * 0.35 * squash
            v.co.x, v.co.y = v.co.x / r * k, v.co.y / r * k
    return band


def build_stake():
    stake = tb.rough_post("stake", 0.045, 0.95, (0, 0, -0.25), material=bb.M["MI_Wood_DockWeathered"], seed=11)
    rag = [flat_band("rag_turn", 0.047, 0.03, 0.6 - k * 0.04, "MI_EnsignRed") for k in range(2)]
    rag.append(bb.block("rag_knot", (0.04, 0.03, 0.035), (0.05, 0.0, 0.59), "MI_EnsignRed", 0.012))
    # Two loose ends, flat strips hanging from the knot.
    for k, (dx, dy, dz) in enumerate(((0.03, 0.025, -0.16), (0.045, -0.02, -0.12))):
        strip = bb.block("rag_end", (0.012, 0.03, abs(dz)), (0.06 + dx * 0.4, dy, 0.59 + dz / 2), "MI_EnsignRed", 0.004)
        sb.transform_obj(strip, (math.radians(8 + 6 * k), math.radians(-12), 0), (0.06, 0.0, 0.59))
        rag.append(strip)
    parts = [stake] + rag
    for p in parts:
        sb.transform_obj(p, (math.radians(9), math.radians(-4), 0), (0, 0, 0))
    obj = bb.finish(parts, "SM_Dig_Stake", wear=0.9, chips=0.4)
    export(obj)


def build_bundle():
    # The oilcloth: a roll along +X, flattened, its ends gathered into short twists; three rope ties that hug it.
    def radius_at(x):
        e = abs(x) / 0.27
        return 0.12 * (1.0 - 0.55 * max(0.0, (e - 0.72) / 0.28) ** 1.5) if e <= 1.0 else 0.0
    profile = [(radius_at(x) + 0.004 * math.sin(x * 41.0), x) for x in [(-0.27 + 0.54 * k / 30) for k in range(31)]]
    profile = [(0.0, -0.275)] + profile + [(0.0, 0.275)]
    body = bb.lathe("bundle_body", profile, (0.0, 0.0, 0.0), "MI_Canvas", 22, axis=(1, 0, 0))
    squash = 0.7
    for v in body.data.vertices:
        v.co.z = v.co.z * squash + 0.12 * squash
    sb.jitter_vertices(body, 0.005, 4)
    parts = [body]
    for x in (-0.17, 0.0, 0.17):
        r = radius_at(x)
        tie = bb.ring("bundle_tie", r + 0.002, 0.011, (x, 0.0, 0.0), (1, 0, 0), "MI_Rope_Hemp", 24)
        for v in tie.data.vertices:
            v.co.z = v.co.z * squash + 0.12 * squash
        parts.append(tie)
    # The gathered ends, twisted and tied off with a turn of rope.
    for side in (-1, 1):
        twist = bb.lathe("bundle_twist", [(0.0, 0.0), (0.035, 0.0), (0.028, 0.03), (0.04, 0.06), (0.0, 0.065)],
                         (side * 0.25, 0.0, 0.12 * squash), "MI_Canvas", 12, axis=(side, 0, 0))
        parts.append(twist)
        parts.append(bb.ring("bundle_endtie", 0.032, 0.008, (side * 0.27, 0.0, 0.12 * squash), (1, 0, 0), "MI_Rope_Hemp", 14))
    parts.append(bb.block("bundle_knot", (0.035, 0.045, 0.025), (0.0, 0.0, 0.24 * squash + 0.012), "MI_Rope_Hemp", 0.01))
    parts += bb.rope("bundle_tail", [(0.0, 0.02, 0.24 * squash + 0.01), (0.04, 0.07, 0.15), (0.06, 0.1, 0.07)], 0.009)
    obj = bb.finish(parts, "SM_Dig_Bundle", wear=0.7, chips=0.2)
    export(obj)


def build_box():
    l, w, h = 0.44, 0.30, 0.22
    parts = []
    for side in (-1, 1):
        for k in range(2):
            z = 0.03 + h * (k + 0.5) / 2
            parts.append(tb.rough_plank("box_side", l, h / 2 - 0.004, 0.022, bb.U((0.0, side * (w / 2 - 0.011), z)), (math.pi / 2, 0, 0),
                                        bb.M["MI_Wood_DockWeathered"], seed=20 + k + side, wear=0.8, grooves=(1, 2)))
            parts.append(tb.rough_plank("box_end", w - 0.04, h / 2 - 0.004, 0.022, bb.U((side * (l / 2 - 0.011), 0.0, z)),
                                        (math.pi / 2, 0, math.pi / 2), bb.M["MI_Wood_DockWeathered"], seed=30 + k + side, wear=0.8, grooves=(1, 1)))
    parts.append(bb.block("box_bottom", (l - 0.02, w - 0.02, 0.025), (0.0, 0.0, 0.02), "MI_Wood_DockWeathered", 0.006))
    for k in range(3):
        parts.append(tb.rough_plank("box_lid", l + 0.03, (w + 0.03) / 3 - 0.004, 0.024, bb.U((0.0, -w / 3 + k * w / 3, h + 0.045)), (0, 0, 0),
                                    bb.M["MI_Wood_DockWeathered"], seed=40 + k, wear=0.9, grooves=(1, 2)))
    for dx in (-1, 1):
        for dy in (-1, 1):
            parts.append(bb.block("box_corner", (0.04, 0.04, h + 0.03), (dx * (l / 2 - 0.006), dy * (w / 2 - 0.006), 0.03 + h / 2),
                                  "MI_Metal_Iron", 0.006))
    parts.append(bb.block("box_hasp", (0.045, 0.012, 0.08), (l / 2 + 0.004, 0.0, h - 0.01), "MI_Metal_Iron", 0.004))
    parts.append(bb.ring("box_handle", 0.045, 0.01, (0.0, w / 2 + 0.02, h * 0.62), (0, 1, 0), "MI_Rope_Hemp", 14))
    obj = bb.finish(parts, "SM_Dig_Box", wear=0.9, chips=0.6)
    export(obj)


def build_shovel():
    # Along +X from the grip (x 0) to the blade's tip (x 1.05); +Z is the blade's face. Pivot at the grip.
    parts = []
    shaft = bb.rod("shovel_shaft", (0.06, 0.0, 0.0), (0.78, 0.0, 0.0), 0.021, "MI_Wood_Teak", 10)
    sb.jitter_vertices(shaft, 0.0015, 3)
    parts.append(shaft)
    # The T-grip: a short crosspiece pegged through the shaft's end.
    parts.append(bb.rod("shovel_grip", (0.035, -0.08, 0.0), (0.035, 0.08, 0.0), 0.024, "MI_Wood_TeakDark", 10))
    parts.append(bb.block("shovel_grip_cheek", (0.07, 0.04, 0.04), (0.055, 0.0, 0.0), "MI_Wood_TeakDark", 0.01))
    # The socket: an iron sleeve tapering onto the shaft, two rivets.
    parts.append(bb.lathe("shovel_socket", [(0.0, 0.0), (0.021, 0.0), (0.024, 0.09), (0.03, 0.16), (0.0, 0.16)], (0.74, 0.0, 0.0),
                          "MI_Metal_Iron", 14, axis=(1, 0, 0)))
    for x in (0.77, 0.83):
        parts.append(bb.disc("shovel_rivet", 0.006, 0.006, (x, 0.0, 0.024), (0, 0, 1), "MI_Metal_Iron", 8))
    # The blade: a rounded spade, a little dished, thick enough to read.
    bm = bmesh.new()
    outline = []
    for k in range(25):
        t = k / 24
        a = math.pi * t
        x = 0.9 + 0.19 * math.sin(a) ** 0.9
        y = 0.13 * math.cos(a) * (1.0 - 0.15 * (math.sin(a) ** 4))
        outline.append((x, y))
    outline = [(0.88, 0.0)] + outline
    top = [bm.verts.new(bb.U((x, y, 0.006 + 0.012 * (y / 0.13) ** 2))) for x, y in outline]
    bot = [bm.verts.new(bb.U((x, y, -0.006 + 0.012 * (y / 0.13) ** 2))) for x, y in outline]
    bm.faces.new(top)
    bm.faces.new(list(reversed(bot)))
    n = len(outline)
    for i in range(n):
        j = (i + 1) % n
        bm.faces.new([top[i], bot[i], bot[j], top[j]])
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    blade = bb.mesh_object("shovel_blade", bm, ["MI_Metal_Iron"])
    sb.world_box_uv(blade, 0.5)
    parts.append(blade)
    obj = bb.finish(parts, "SM_Tool_Shovel", wear=0.8, chips=0.3)
    sb.save_blend(obj.name)
    sb.export_fbx([obj], os.path.join("Tools", obj.name + ".fbx"))
    print("SB_DIG", obj.name, len(obj.data.polygons))


BUILDERS = {"mound": build_mound, "hole": build_hole, "heap": build_heap, "stake": build_stake, "bundle": build_bundle,
            "box": build_box, "shovel": build_shovel}


def main():
    names = sb.script_args() or list(BUILDERS)
    for n in names:
        sb.reset_scene()
        mats()
        BUILDERS[n]()


if __name__ == "__main__":
    main()
