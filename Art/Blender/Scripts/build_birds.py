"""The seagull (D76; owner: "fix the birds. I want detailed birds with good animation when flying"): a skinned herring gull
for ASaltBirds, stylised like the rest of the game but with the detail of a bird seen up close.

  SK_Gull   one skinned mesh on a small skeleton (SK_Gull_Rig): spine; neck, head; tail; for each wing upper (the arm, from
            the shoulder), fore (the forearm, from the elbow) and hand (from the wrist, the primaries on it). ASaltBirds poses
            the bones every frame (the wingbeat, the crooked glide, the banking, the head looking about), so there is no
            animation asset.
            The body and head one smooth loft (a plump breast, a steep forehead), the beak hooked with the red spot on the
            lower mandible, pale eyes with a black pupil, a fan of seven tail feathers, tucked pink feet. The wings in their
            rest pose spread level: a thick rounded leading edge, grey above and white below, a white trailing edge along the
            secondaries, seven separate primaries on the hand fanned out like fingers, the outer five black with white tips.
Built in Unreal's frame (x forward, y right, z up, metres; U() mirrors y for Blender), about 0.62 m from bill to tail and 1.42 m
across the wings (ASaltBirds' BirdScale scales it). The pivot is the body's centre.
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

MATS = [("MI_Bird_White", (0.93, 0.93, 0.91)), ("MI_Bird_Grey", (0.5, 0.55, 0.6)), ("MI_Bird_Dark", (0.06, 0.06, 0.07)),
        ("MI_Bird_Beak", (0.95, 0.76, 0.18)), ("MI_Bird_Red", (0.78, 0.14, 0.08)), ("MI_Bird_Leg", (0.86, 0.6, 0.52)),
        ("MI_Bird_Iris", (0.92, 0.88, 0.62))]
WHITE, GREY, DARK, BEAK, RED, LEG, IRIS = range(7)

# The skeleton (Unreal frame, metres): name, parent, head, tail.
SHOULDER, ELBOW, WRIST, HAND_END = 0.05, 0.20, 0.38, 0.62
BONES = [
    ("spine", None, (-0.02, 0.0, 0.0), (0.09, 0.0, 0.0)),
    ("neck", "spine", (0.11, 0.0, 0.015), (0.17, 0.0, 0.045)),
    ("head", "neck", (0.17, 0.0, 0.045), (0.26, 0.0, 0.05)),
    ("tail", "spine", (-0.17, 0.0, 0.01), (-0.33, 0.0, 0.012)),
]
for _s, _n in ((1, "R"), (-1, "L")):
    BONES += [
        ("upper_" + _n, "spine", (0.035, _s * SHOULDER, 0.03), (0.04, _s * ELBOW, 0.03)),
        ("fore_" + _n, "upper_" + _n, (0.04, _s * ELBOW, 0.03), (0.02, _s * WRIST, 0.03)),
        ("hand_" + _n, "fore_" + _n, (0.02, _s * WRIST, 0.03), (-0.03, _s * HAND_END, 0.03)),
    ]


def U(p):
    return Vector((p[0], -p[1], p[2]))


class Builder:
    """One bmesh; every vertex remembers its bone weights (by name)."""

    def __init__(self):
        self.bm = bmesh.new()
        self.weights = {}

    def vert(self, p, w):
        v = self.bm.verts.new(U(p))
        self.weights[v] = w
        return v

    def loft(self, rings, mat, weight_fn, cap=(True, True), mat_fn=None):
        """Faces through rings of points (equal counts, each a closed loop); weight_fn(p) -> {bone: w}; mat_fn(face centre, ring
        index, point index, outward) -> material, else mat."""
        vs = [[self.vert(p, weight_fn(p)) for p in ring] for ring in rings]
        n = len(rings[0])
        for a in range(len(vs) - 1):
            for i in range(n):
                j = (i + 1) % n
                f = self.bm.faces.new((vs[a][i], vs[a][j], vs[a + 1][j], vs[a + 1][i]))
                f.material_index = mat if mat_fn is None else mat_fn(rings, a, i)
        if cap[0]:
            f = self.bm.faces.new(list(reversed(vs[0])))
            f.material_index = mat
        if cap[-1]:
            f = self.bm.faces.new(vs[-1])
            f.material_index = mat
        return vs

    def sphere(self, c, r, mat, w, segs=(14, 9), squash=(1.0, 1.0, 1.0)):
        res = bmesh.ops.create_uvsphere(self.bm, u_segments=segs[0], v_segments=segs[1], radius=r)
        for v in res["verts"]:
            v.co = Vector((v.co.x * squash[0], v.co.y * squash[1], v.co.z * squash[2])) + U(c)
            self.weights[v] = w
        for f in {f for v in res["verts"] for f in v.link_faces}:
            f.material_index = mat
        return res["verts"]


def superellipse(cx, cy_half, cz, top, bottom, n=24, e=2.4, y_shift=0.0):
    """A ring across the body at x: half-width cy_half, heights above and below cz, a soft box (exponent e)."""
    pts = []
    for i in range(n):
        a = 2 * math.pi * i / n
        c, s = math.cos(a), math.sin(a)
        y = cy_half * math.copysign(abs(c) ** (2.0 / e), c)
        h = top if s > 0 else bottom
        z = cz + h * math.copysign(abs(s) ** (2.0 / e), s)
        pts.append((cx, y + y_shift, z))
    return pts


def catmull(points, k):
    """Points resampled with k steps between each pair (Catmull-Rom on tuples)."""
    out = []
    P = [points[0]] + points + [points[-1]]
    for i in range(1, len(P) - 2):
        p0, p1, p2, p3 = P[i - 1], P[i], P[i + 1], P[i + 2]
        for s in range(k):
            t = s / k
            out.append(tuple(0.5 * ((2 * b) + (-a + c) * t + (2 * a - 5 * b + 4 * c - d) * t * t + (-a + 3 * b - 3 * c + d) * t ** 3)
                             for a, b, c, d in zip(p0, p1, p2, p3)))
    out.append(points[-1])
    return out


def smooth_w(x, a, b):
    t = min(max((x - a) / (b - a), 0.0), 1.0)
    return t * t * (3 - 2 * t)


# --- weights ------------------------------------------------------------------------------------------------------------------
def body_weight(p):
    x = p[0]
    neck = smooth_w(x, 0.08, 0.13)
    head = smooth_w(x, 0.15, 0.19)
    tail = 1.0 - smooth_w(x, -0.2, -0.14)
    w = {"spine": max(1.0 - neck - tail, 0.0), "neck": max(neck - head, 0.0), "head": head, "tail": tail}
    return {k: v for k, v in w.items() if v > 1e-3}


def wing_weight(p):
    y = abs(p[1])
    side = "R" if p[1] > 0 else "L"
    root = smooth_w(y, SHOULDER - 0.01, SHOULDER + 0.035)
    fore = smooth_w(y, ELBOW - 0.025, ELBOW + 0.025)
    hand = smooth_w(y, WRIST - 0.025, WRIST + 0.025)
    w = {"spine": 1.0 - root, "upper_" + side: root * (1.0 - fore), "fore_" + side: root * fore * (1.0 - hand), "hand_" + side: root * hand}
    return {k: v for k, v in w.items() if v > 1e-3}


# --- the parts ----------------------------------------------------------------------------------------------------------------
def body(B):
    # (x, half width, top, bottom, centre z): tail root, back, belly, breast, neck, head, cheeks, the beak's base.
    prof = [(-0.205, 0.03, 0.016, 0.016, 0.012), (-0.16, 0.05, 0.034, 0.036, 0.008), (-0.09, 0.066, 0.05, 0.058, 0.001),
            (-0.02, 0.074, 0.056, 0.068, -0.003), (0.045, 0.071, 0.055, 0.066, -0.002), (0.095, 0.056, 0.046, 0.05, 0.01),
            (0.13, 0.04, 0.036, 0.036, 0.028), (0.16, 0.035, 0.038, 0.032, 0.045), (0.19, 0.038, 0.042, 0.034, 0.053),
            (0.22, 0.034, 0.036, 0.03, 0.053), (0.243, 0.024, 0.024, 0.02, 0.05), (0.256, 0.014, 0.014, 0.012, 0.048)]
    prof = catmull(prof, 4)
    rings = [superellipse(x, w, zc, t, b, n=28, e=2.3) for x, w, t, b, zc in prof]

    def mat(rings_, a, i):
        x = rings_[a][0][0]
        z = rings_[a][i][2]
        zc = sum(q[2] for q in rings_[a]) / len(rings_[a])
        # The grey mantle on the back between the wings (the folded wings' grey goes on from it).
        return GREY if (-0.17 < x < 0.08 and z > zc + 0.035) else WHITE
    B.loft(rings, WHITE, body_weight, mat_fn=mat)


def beak(B):
    """Hooked, deep at the gonys; yellow, the red spot on the lower mandible near the angle."""
    n = 16
    rings = []
    L = 0.078
    for k in range(13):
        t = k / 12
        x = 0.25 + L * t
        w = 0.0125 * (1 - t) + 0.0035 * t
        top = 0.011 * (1 - t) + 0.004 * t
        gonys = 0.0045 * math.exp(-((t - 0.7) / 0.12) ** 2)                    # the lower mandible's angle
        bottom = 0.01 * (1 - t) + 0.003 * t + gonys
        zc = 0.048 - 0.006 * t - (0.008 * ((t - 0.78) / 0.22) ** 2 if t > 0.78 else 0.0)       # the hooked tip
        rings.append(superellipse(x, w, zc, top, bottom, n=n, e=2.0))

    def mat(rings_, a, i):
        t = a / 12
        z = rings_[a][i][2]
        zc = sum(q[2] for q in rings_[a]) / n
        return RED if (0.6 < t < 0.82 and z < zc - 0.002) else BEAK
    B.loft(rings, BEAK, lambda p: {"head": 1.0}, mat_fn=mat)


def eyes(B):
    for s in (1, -1):
        B.sphere((0.211, s * 0.029, 0.064), 0.0105, IRIS, {"head": 1.0}, segs=(14, 9), squash=(1.0, 0.8, 1.0))
        B.sphere((0.2135, s * 0.0362, 0.0648), 0.0052, DARK, {"head": 1.0}, segs=(10, 7), squash=(1.0, 0.6, 1.0))


def tail(B):
    """Seven feathers fanned from the tail's root, the middle ones on top."""
    for k in range(7):
        a = math.radians((k - 3) * 6.5)
        z0 = 0.014 + 0.0012 * (3 - abs(k - 3))
        L = 0.135 - 0.004 * abs(k - 3)
        rings = []
        for j in range(10):
            t = j / 9
            r = L * t
            half = (0.012 + 0.012 * math.sin(math.pi * min(t / 0.85, 1.0) * 0.5)) * (1.0 if t < 0.85 else math.sqrt(max(1 - ((t - 0.85) / 0.15) ** 2, 0.02)))
            cx, cy = -0.185 - r * math.cos(a), -r * math.sin(a)
            ring = []
            for u, h in ((-1.0, 0.0), (-0.5, 0.0018), (0.0, 0.0024), (0.5, 0.0018), (1.0, 0.0), (0.5, -0.0012), (0.0, -0.0016), (-0.5, -0.0012)):
                ring.append((cx - u * half * math.sin(a), cy + u * half * math.cos(a), z0 + h - 0.01 * t * t))
            rings.append(ring)
        B.loft(rings, WHITE, lambda p: {"tail": 1.0})


def feet(B):
    """Tucked under the tail, pale pink."""
    for s in (1, -1):
        pts = [(-0.08, s * 0.025, -0.045), (-0.13, s * 0.028, -0.05), (-0.18, s * 0.03, -0.03), (-0.205, s * 0.032, -0.02)]
        rings = []
        for k, (x, y, z) in enumerate(pts):
            r = 0.007 if k < 2 else 0.011
            rings.append([(x, y + r * math.cos(2 * math.pi * i / 8), z + 0.5 * r * math.sin(2 * math.pi * i / 8)) for i in range(8)])
        B.loft(rings, LEG, lambda p: {"tail": 0.6, "spine": 0.4})


def lerp_curve(pts, y):
    """Piecewise-smooth (Catmull-Rom) value of a curve given as [(y, v)] at y."""
    ys = [q[0] for q in pts]
    if y <= ys[0]:
        return pts[0][1]
    if y >= ys[-1]:
        return pts[-1][1]
    k = max(i for i in range(len(ys) - 1) if ys[i] <= y)
    t = (y - ys[k]) / (ys[k + 1] - ys[k])
    p0 = pts[max(k - 1, 0)][1]
    p1, p2 = pts[k][1], pts[k + 1][1]
    p3 = pts[min(k + 2, len(pts) - 1)][1]
    return 0.5 * ((2 * p1) + (-p0 + p2) * t + (2 * p0 - 5 * p1 + 4 * p2 - p3) * t * t + (-p0 + 3 * p1 - 3 * p2 + p3) * t ** 3)


# The planform (one side, |y| out from the centreline, x forward): the leading edge runs forward to the bend of the wing at the
# wrist and sweeps back along the hand to the pointed tip; the trailing edge runs straight along the secondaries and curves
# out along the primaries.
TIP = 0.7
LEAD = [(SHOULDER - 0.01, 0.045), (0.2, 0.062), (0.37, 0.078), (0.47, 0.06), (0.57, 0.02), (0.64, -0.03), (TIP, -0.105)]
TRAIL = [(SHOULDER - 0.01, -0.152), (0.2, -0.142), (0.36, -0.125), (0.43, -0.138), (0.52, -0.135), (0.6, -0.12), (0.66, -0.112), (TIP, -0.105)]
HAND_BLACK = 0.535


def wing(B, s):
    """One piece from the shoulder to the tip: a thick rounded leading edge, the coverts standing a little proud, grey above and
    white below with a white trailing edge along the secondaries (gently scalloped, a feather every 3 cm), the hand black above
    and below, its trailing edge notched into the primaries' rounded tips (white), a white mirror near the tip."""
    airfoil = [(0.0, 0.0), (0.05, 0.5), (0.14, 0.82), (0.26, 0.92), (0.38, 0.86), (0.5, 0.66), (0.62, 0.48), (0.74, 0.32), (0.86, 0.17),
               (1.0, 0.0), (0.86, -0.06), (0.74, -0.1), (0.62, -0.14), (0.5, -0.18), (0.38, -0.23), (0.26, -0.27), (0.14, -0.27),
               (0.04, -0.2)]
    stations = []
    y = SHOULDER - 0.01
    while y < TIP - 0.004:
        stations.append(y)
        y += 0.0075 if y > 0.38 else 0.012
    stations.append(TIP - 0.004)
    rings = []
    for y in stations:
        le, te = lerp_curve(LEAD, y), lerp_curve(TRAIL, y)
        if y < 0.37:                                       # the secondaries' soft scallops
            te -= 0.0025 * (0.5 + 0.5 * math.cos(2 * math.pi * (y - SHOULDER) / 0.03)) ** 2
        elif y > 0.4:                                      # the primaries' tips: rounded notches, closer towards the tip
            ph = (y - 0.4) / (TIP - 0.4)
            te -= 0.009 * (1.0 - ph) * abs(math.sin(math.pi * ph * 6.0)) ** 0.6
        chord = max(le - te, 0.004)
        t = min(max((y - SHOULDER) / (TIP - SHOULDER), 0.0), 1.0)
        thick = 0.034 * (1.0 - t) ** 0.8 + 0.003
        z = 0.03 + 0.006 * math.sin(math.pi * min(t / 0.55, 1.0)) - 0.01 * max(t - 0.6, 0.0)
        ring = []
        for u, h in airfoil:
            cov = 0.0018 * (1.0 - smooth_w(u, 0.38, 0.5)) * (1.0 if h > 0 else 0.0) * (1.0 - t)     # the coverts, proud
            ring.append((le - u * chord, s * y, z + h * thick + cov))
        rings.append(ring)

    def mat(rings_, a, i):
        u, h = airfoil[i]
        un, hn = airfoil[(i + 1) % len(airfoil)]
        top = h > 0 or hn > 0
        yv = abs(rings_[a][i][1])
        um = 0.5 * (u + un)
        if yv >= HAND_BLACK - 0.035 + 0.13 * um:              # the black wedge: further out towards the trailing edge
            if um > 0.8 and yv > 0.6:
                return WHITE                                   # the primaries' white tips
            if ((yv - 0.648) / 0.02) ** 2 + ((um - 0.42) / 0.16) ** 2 < 1.0:
                return WHITE                                   # the mirror
            return DARK
        if not top:
            return WHITE
        if um > 0.8:
            return WHITE                                       # the white trailing edge
        return GREY
    B.loft(rings, GREY, wing_weight, mat_fn=mat)


# --- the skeleton, the mesh, the export ---------------------------------------------------------------------------------------
def armature():
    arm = bpy.data.armatures.new("SK_Gull_Rig")
    obj = bpy.data.objects.new("SK_Gull_Rig", arm)
    bpy.context.scene.collection.objects.link(obj)
    bpy.context.view_layer.objects.active = obj
    bpy.ops.object.mode_set(mode="EDIT")
    made = {}
    for name, parent, head, tail_ in BONES:
        b = arm.edit_bones.new(name)
        b.head = U(head)
        b.tail = U(tail_)
        b.roll = 0.0
        if parent:
            b.parent = made[parent]
            b.use_connect = False
        made[name] = b
    bpy.ops.object.mode_set(mode="OBJECT")
    return obj


def main():
    sb.reset_scene()
    mats = [sb.get_material(n, (*c, 1.0)) for n, c in MATS]
    B = Builder()
    body(B)
    beak(B)
    eyes(B)
    tail(B)
    feet(B)
    wing(B, 1)
    wing(B, -1)
    bmesh.ops.recalc_face_normals(B.bm, faces=B.bm.faces)
    for f in B.bm.faces:
        f.smooth = True
    me = bpy.data.meshes.new("SK_Gull")
    order = list(B.bm.verts)
    B.bm.verts.index_update()
    weights = [B.weights[v] for v in order]
    B.bm.to_mesh(me)
    B.bm.free()
    obj = sb.new_object("SK_Gull", me)
    for m in mats:
        obj.data.materials.append(m)
    sb.world_box_uv(obj, 0.25)
    rig = armature()
    groups = {name: obj.vertex_groups.new(name=name) for name, _, _, _ in BONES}
    for i, w in enumerate(weights):
        total = sum(w.values()) or 1.0
        for bone, val in w.items():
            groups[bone].add([i], val / total, "REPLACE")
    obj.parent = rig
    mod = obj.modifiers.new("Rig", "ARMATURE")
    mod.object = rig
    sb.save_blend("SK_Gull")
    path = os.path.join(sb.EXPORT_ROOT, "Birds", "SK_Gull.fbx")
    os.makedirs(os.path.dirname(path), exist_ok=True)
    bpy.ops.object.select_all(action="DESELECT")
    rig.select_set(True)
    obj.select_set(True)
    bpy.context.view_layer.objects.active = rig
    bpy.ops.export_scene.fbx(filepath=path, use_selection=True, object_types={"ARMATURE", "MESH"}, global_scale=1.0, apply_unit_scale=True,
                             apply_scale_options="FBX_SCALE_NONE", axis_forward="-Z", axis_up="Y", use_space_transform=True,
                             bake_space_transform=False, use_mesh_modifiers=False, mesh_smooth_type="FACE", use_tspace=True,
                             add_leaf_bones=False, primary_bone_axis="Y", secondary_bone_axis="X", armature_nodetype="NULL",
                             use_armature_deform_only=True, bake_anim=False, path_mode="STRIP", embed_textures=False)
    print("SB_BIRD SK_Gull faces", len(me.polygons), "verts", len(me.vertices), "bones", len(BONES), path)


main()
