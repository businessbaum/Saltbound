"""Characters (D80: the player's arms; D81: the Harbour Keeper): one skinned mesh built from lofted tubes and blobs, every vertex
carrying its bone weights from the part it belongs to, a plain armature, and the FBX export the gull uses (D76).

Everything is built in Unreal's frame (x forward, y right, z up, metres); U() mirrors y for Blender. A part's weights come from
a chain of bones: a vertex goes to the bone whose segment it is nearest, blended with the next bone over a short band round each
joint, so fingers bend at their knuckles and nowhere else.
"""
import math
import os

import bmesh
import bpy
from mathutils import Matrix, Vector

import sb_blender as sb


def U(p):
    return Vector((p[0], -p[1], p[2]))


def V(p):
    return Vector((float(p[0]), float(p[1]), float(p[2])))


def smooth(x, a, b):
    t = min(max((x - a) / (b - a), 0.0), 1.0) if b != a else (1.0 if x >= a else 0.0)
    return t * t * (3 - 2 * t)


class Builder:
    """One bmesh; every vertex remembers its bone weights (by name)."""

    def __init__(self):
        self.bm = bmesh.new()
        self.weights = {}

    def vert(self, p, w):
        v = self.bm.verts.new(U(p))
        self.weights[v] = w
        return v

    def rings(self, rings, mat, weight_fn, cap=(True, True), closed=True, mat_fn=None):
        """Faces through rings of points (equal counts); weight_fn(p) -> {bone: w}; mat_fn(a, i) -> material."""
        vs = [[self.vert(p, weight_fn(p)) for p in ring] for ring in rings]
        n = len(rings[0])
        for a in range(len(vs) - 1):
            for i in range(n if closed else n - 1):
                j = (i + 1) % n
                f = self.bm.faces.new((vs[a][i], vs[a][j], vs[a + 1][j], vs[a + 1][i]))
                f.material_index = mat if mat_fn is None else mat_fn(a, i)
        if cap[0]:
            c = self.vert(sum((V(p) for p in rings[0]), Vector()) / n, weight_fn(rings[0][0]))
            for i in range(n):
                f = self.bm.faces.new((vs[0][(i + 1) % n], vs[0][i], c))
                f.material_index = mat
        if cap[1]:
            c = self.vert(sum((V(p) for p in rings[-1]), Vector()) / n, weight_fn(rings[-1][0]))
            for i in range(n):
                f = self.bm.faces.new((vs[-1][i], vs[-1][(i + 1) % n], c))
                f.material_index = mat
        return vs

    def blob(self, centre, radii, axes, mat, weight_fn, segs=(16, 10), bend=None):
        """An ellipsoid: radii along the three axes (unit vectors); bend(local) -> local displaces its points."""
        res = bmesh.ops.create_uvsphere(self.bm, u_segments=segs[0], v_segments=segs[1], radius=1.0)
        c = V(centre)
        ax = [V(a).normalized() for a in axes]
        for v in res["verts"]:
            l = Vector((v.co.x * radii[0], v.co.y * radii[1], v.co.z * radii[2]))
            if bend:
                l = bend(l)
            p = c + ax[0] * l.x + ax[1] * l.y + ax[2] * l.z
            v.co = U(p)
            self.weights[v] = weight_fn(p)
        for f in {f for v in res["verts"] for f in v.link_faces}:
            f.material_index = mat
        return res["verts"]


def frames(path, up_hint):
    """A frame (tangent, side, up) at each point of a path, the up kept near up_hint and turned smoothly along it."""
    out = []
    up = V(up_hint).normalized()
    for i in range(len(path)):
        a = V(path[max(i - 1, 0)])
        b = V(path[min(i + 1, len(path) - 1)])
        t = (b - a).normalized()
        u = (up - t * up.dot(t))
        if u.length < 1e-6:
            u = t.orthogonal()
        u.normalize()
        s = u.cross(t).normalized()   # side: up x tangent (right-handed: tangent x side = up in Unreal's frame)
        out.append((t, s, u))
        up = u
    return out


def tube(B, path, section, mat, weight_fn, up_hint=(0, 0, 1), n=20, cap=(True, True), mat_fn=None):
    """A lofted tube along path; section(k, t, angle) -> (side radius, up radius) for ring k (t 0..1 along) at angle (0 = side),
    or a (radius_side, radius_up, exponent) triple for a soft box."""
    fr = frames(path, up_hint)
    rings = []
    m = len(path)
    for k, (p, (t, s, u)) in enumerate(zip(path, fr)):
        tt = k / max(m - 1, 1)
        ring = []
        for i in range(n):
            a = 2 * math.pi * i / n
            sec = section(k, tt, a)
            rs, ru = sec[0], sec[1]
            e = sec[2] if len(sec) > 2 else 2.0
            c, sn = math.cos(a), math.sin(a)
            x = rs * math.copysign(abs(c) ** (2.0 / e), c)
            y = ru * math.copysign(abs(sn) ** (2.0 / e), sn)
            ring.append(tuple(V(p) + s * x + u * y))
        rings.append(ring)
    return B.rings(rings, mat, weight_fn, cap=cap, mat_fn=mat_fn)


def tube_xy(B, path, section_xy, mat, weight_fn, up_hint=(0, 0, 1), n=20, cap=(True, True), mat_fn=None, frames_=None):
    """A lofted tube whose rings are free shapes: section_xy(k, d, angle) -> (x, y) in the ring's (side, up) plane, d the distance
    along the path. frames_ may give the frames (from frames())."""
    fr = frames_ or frames(path, up_hint)
    d = 0.0
    rings = []
    for k, (p, (t, s, u)) in enumerate(zip(path, fr)):
        if k:
            d += (V(p) - V(path[k - 1])).length
        ring = []
        for i in range(n):
            a = 2 * math.pi * i / n
            x, y = section_xy(k, d, a)
            ring.append(tuple(V(p) + s * x + u * y))
        rings.append(ring)
    return B.rings(rings, mat, weight_fn, cap=cap, mat_fn=mat_fn)


def soft_box(rs, ru, a, e=2.0):
    """A point of a soft box (superellipse) of half sizes rs, ru at angle a."""
    c, sn = math.cos(a), math.sin(a)
    return rs * math.copysign(abs(c) ** (2.0 / e), c), ru * math.copysign(abs(sn) ** (2.0 / e), sn)


def resample(points, step):
    """Points along a polyline about step apart (keeps the ends)."""
    pts = [V(p) for p in points]
    out = [pts[0]]
    for a, b in zip(pts, pts[1:]):
        L = (b - a).length
        k = max(int(math.ceil(L / step)), 1)
        for i in range(1, k + 1):
            out.append(a.lerp(b, i / k))
    return [tuple(p) for p in out]


def chain_weight(chain, band=0.012):
    """weight_fn for a part on a chain of bones [(name, head, tail), ...]: the nearest segment's bone, blended with its parent over
    band round the joint at its head and with its child round its tail."""
    segs = [(n, V(h), V(t)) for n, h, t in chain]

    def fn(p):
        p = V(p)
        best = None
        for i, (n, h, t) in enumerate(segs):
            d = t - h
            L2 = max(d.length_squared, 1e-12)
            s = max(0.0, min(1.0, (p - h).dot(d) / L2))
            dist = (p - (h + d * s)).length
            along = (p - h).dot(d) / math.sqrt(L2)
            if best is None or dist < best[0] - 1e-9:
                best = (dist, i, along, math.sqrt(L2))
        _, i, along, L = best
        w = {segs[i][0]: 1.0}
        if i > 0 and along < band:
            k = 0.5 * (1.0 - smooth(along, -band, band))
            w = {segs[i][0]: 1.0 - k, segs[i - 1][0]: k}
        elif i < len(segs) - 1 and along > L - band:
            k = 0.5 * smooth(along, L - band, L + band)
            w = {segs[i][0]: 1.0 - k, segs[i + 1][0]: k}
        return w
    return fn


def mix(*parts):
    """Adds weight dicts scaled: mix((w1, a), (w2, b))."""
    out = {}
    for w, k in parts:
        for n, v in w.items():
            out[n] = out.get(n, 0.0) + v * k
    return out


def armature(name, bones):
    """bones: [(name, parent, head, tail)] in Unreal's frame."""
    arm = bpy.data.armatures.new(name)
    obj = bpy.data.objects.new(name, arm)
    bpy.context.scene.collection.objects.link(obj)
    bpy.context.view_layer.objects.active = obj
    bpy.ops.object.mode_set(mode="EDIT")
    made = {}
    for bname, parent, head, tail in bones:
        b = arm.edit_bones.new(bname)
        b.head = U(head)
        b.tail = U(tail)
        b.roll = 0.0
        if parent:
            b.parent = made[parent]
            b.use_connect = False
        made[bname] = b
    bpy.ops.object.mode_set(mode="OBJECT")
    return obj


def finish(B, name, mats, bones, folder, uv_metres=0.1):
    """The mesh from the builder, its weights, the armature, saved as <name>.blend and exported to Art/Export/<folder>/<name>.fbx."""
    bmesh.ops.recalc_face_normals(B.bm, faces=B.bm.faces)
    for f in B.bm.faces:
        f.smooth = True
    me = bpy.data.meshes.new(name)
    order = list(B.bm.verts)
    B.bm.verts.index_update()
    weights = [B.weights[v] for v in order]
    B.bm.to_mesh(me)
    B.bm.free()
    obj = sb.new_object(name, me)
    for n, c in mats:
        obj.data.materials.append(sb.get_material(n, (*c, 1.0)))
    sb.world_box_uv(obj, uv_metres)
    rig = armature(name + "_Rig", bones)
    names = {b[0] for b in bones}
    groups = {b[0]: obj.vertex_groups.new(name=b[0]) for b in bones}
    for i, w in enumerate(weights):
        w = {k: v for k, v in w.items() if v > 1e-4}
        bad = set(w) - names
        assert not bad, "weights on unknown bones %s" % bad
        total = sum(w.values()) or 1.0
        for bone, val in w.items():
            groups[bone].add([i], val / total, "REPLACE")
    obj.parent = rig
    mod = obj.modifiers.new("Rig", "ARMATURE")
    mod.object = rig
    sb.save_blend(name)
    path = os.path.join(sb.EXPORT_ROOT, folder, name + ".fbx")
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
    print("SB_CHAR", name, "faces", len(me.polygons), "verts", len(me.vertices), "bones", len(bones), path)
    return obj, rig
