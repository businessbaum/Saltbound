"""Seagull for ASaltBirds (ambient life over the cove), stylised and chunky like the rest of the game.

  SM_Gull_Body    white body and head, grey back, yellow beak, dark eyes; pivot at the body centre, facing +X.
                  About 0.45 m long (BirdScale scales it in the game).
  SM_Gull_WingL   left wing (towards -Y in Unreal), pivot at the shoulder: grey on top, white underneath, dark tip.
                  Gently gull-shaped: rising inner half, drooping and swept-back outer half.
  SM_Gull_WingR   the mirror image (towards +Y).
The shoulder sits at (4, +-5.5, 3) cm on the body (SaltBirds.cpp). Coordinates here are Unreal (x forward, y right,
z up) in metres; U() converts to Blender (y mirrored).
Run: Tools/Build-BlenderAsset.ps1 build_birds
"""
import math
import os
import sys

import bmesh
import bpy
from mathutils import Vector

sys.path.insert(0, os.path.dirname(__file__))
import sb_blender as sb  # noqa: E402

M = {}


def U(p):
    return Vector((p[0], -p[1], p[2]))


def mats():
    for n, c in {"MI_Bird_White": (0.92, 0.92, 0.9), "MI_Bird_Grey": (0.55, 0.6, 0.65), "MI_Bird_Dark": (0.08, 0.08, 0.09),
                 "MI_Bird_Beak": (0.95, 0.75, 0.2)}.items():
        M[n] = sb.get_material(n, (*c, 1))


def finish(obj, name, materials):
    for m in materials:
        obj.data.materials.append(M[m])
    sb.world_box_uv(obj, 0.25)
    obj.name = name
    obj.data.name = name
    return obj


def loft(bm, sections):
    """Closed loft through rings of points (each ring the same count), capped at both ends."""
    rings = [[bm.verts.new(U(p)) for p in ring] for ring in sections]
    faces = []
    n = len(rings[0])
    for r0, r1 in zip(rings[:-1], rings[1:]):
        for i in range(n):
            j = (i + 1) % n
            faces.append(bm.faces.new((r0[i], r0[j], r1[j], r1[i])))
    faces.append(bm.faces.new(rings[0]))
    faces.append(bm.faces.new(list(reversed(rings[-1]))))
    return faces


def body():
    """Plump teardrop body with a round head on a short neck, a flat tail wedge and a hooked beak."""
    bm = bmesh.new()
    n = 14
    # (x, half width, half height, centre z) along the body, tail to neck.
    prof = [(-0.25, 0.012, 0.006, 0.012), (-0.2, 0.035, 0.012, 0.01), (-0.14, 0.05, 0.04, 0.0), (-0.07, 0.062, 0.062, -0.005),
            (0.0, 0.066, 0.068, -0.005), (0.07, 0.06, 0.06, 0.0), (0.12, 0.045, 0.045, 0.012), (0.16, 0.034, 0.036, 0.025)]
    sections = []
    for x, w, h, zc in prof:
        ring = []
        for i in range(n):
            a = 2 * math.pi * i / n
            ring.append((x, w * math.cos(a), zc + h * math.sin(a)))
        sections.append(ring)
    loft(bm, sections)
    head = bmesh.ops.create_uvsphere(bm, u_segments=16, v_segments=10, radius=0.043)
    bmesh.ops.translate(bm, verts=head["verts"], vec=U((0.185, 0.0, 0.045)))
    beak = bmesh.ops.create_cone(bm, cap_ends=True, segments=10, radius1=0.014, radius2=0.004, depth=0.075)
    from mathutils import Matrix
    bmesh.ops.rotate(bm, verts=beak["verts"], cent=(0, 0, 0), matrix=Matrix.Rotation(math.pi / 2, 3, "Y"))
    bmesh.ops.translate(bm, verts=beak["verts"], vec=U((0.255, 0.0, 0.036)))
    for v in beak["verts"]:
        if v.co.x > 0.27:
            v.co.z -= (v.co.x - 0.27) * 0.35          # slight hook
    eyes = []
    for s in (-1, 1):
        e = bmesh.ops.create_uvsphere(bm, u_segments=8, v_segments=6, radius=0.009)
        bmesh.ops.translate(bm, verts=e["verts"], vec=U((0.215, s * 0.038, 0.058)))
        eyes += e["verts"]
    bm.normal_update()
    beak_set, eye_set = set(beak["verts"]), set(eyes)
    for f in bm.faces:
        f.smooth = True
        vs = set(f.verts)
        if vs & beak_set:
            f.material_index = 2
        elif vs & eye_set:
            f.material_index = 3
        else:
            c = f.calc_center_median()
            # Grey back between the wings (Blender y is mirrored, only z and x matter here).
            f.material_index = 1 if (c.z > 0.03 and -0.17 < c.x < 0.09) else 0
    me = bpy.data.meshes.new("SM_Gull_Body")
    bm.to_mesh(me)
    bm.free()
    obj = sb.new_object("SM_Gull_Body", me)
    return finish(obj, "SM_Gull_Body", ["MI_Bird_White", "MI_Bird_Grey", "MI_Bird_Beak", "MI_Bird_Dark"])


def wing(side):
    """Wing towards side * Y from the shoulder: chord 0.18 m at the root to 0.07 m at the tip, span 0.66 m."""
    bm = bmesh.new()
    n_span = 12
    span = 0.66
    sections = []
    for k in range(n_span + 1):
        t = k / n_span
        y = side * span * t
        chord = 0.18 * (1.0 - t) + 0.07 * t - 0.03 * math.sin(math.pi * t) * 0.0
        # Gull shape: the inner half rises, the outer half droops and sweeps back.
        z = 0.06 * math.sin(math.pi * min(t / 0.55, 1.0) * 0.5) - 0.07 * max(t - 0.5, 0.0) ** 1.5
        lead = 0.03 - 0.12 * max(t - 0.45, 0.0) ** 1.4
        thick = 0.026 * (1.0 - t) + 0.008 * t
        # Airfoil-ish ring: rounded leading edge, thin trailing edge.
        ring = []
        pts = [(0.0, 0.0), (0.15, 0.6), (0.4, 0.75), (0.75, 0.45), (1.0, 0.0), (0.75, -0.25), (0.4, -0.35), (0.15, -0.3)]
        for u, h in pts:
            ring.append((lead - u * chord, y, z + h * thick))
        sections.append(ring)
    faces = loft(bm, sections)
    bm.normal_update()
    if side > 0:
        bmesh.ops.reverse_faces(bm, faces=bm.faces)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    for f in bm.faces:
        f.smooth = True
        c = f.calc_center_median()
        t = abs(c.y) / span                       # Blender y mirrored; the span fraction is symmetric
        if t > 0.74:
            f.material_index = 2                  # dark tip
        elif f.normal.z < -0.2:
            f.material_index = 0                  # white underside
        else:
            f.material_index = 1                  # grey top
    name = "SM_Gull_WingL" if side < 0 else "SM_Gull_WingR"
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    obj = sb.new_object(name, me)
    return finish(obj, name, ["MI_Bird_White", "MI_Bird_Grey", "MI_Bird_Dark"])


def main():
    for build in (body, lambda: wing(-1), lambda: wing(1)):
        sb.reset_scene()
        mats()
        obj = build()
        sb.save_blend(obj.name)
        sb.export_fbx([obj], os.path.join("Birds", obj.name + ".fbx"))
        print("SB_BIRD", obj.name, "faces", len(obj.data.polygons))


main()
