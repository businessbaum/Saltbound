"""Shared helpers for Saltbound Blender asset scripts.

Conventions (see Docs/Conventions.md):
  * 1 Blender unit = 1 metre, scene unit scale 1.0. Unreal receives centimetres (1 m -> 100 uu).
  * Forward = +X, Up = +Z. Starboard (right) = -Y in Blender, which becomes +Y in Unreal.
  * Object origin = logical pivot (props: bottom centre; boat: midship on the design waterline).
  * Material names are the Unreal material slot names and should match an MI_ in /Game/Saltbound/Materials.
  * Custom collision objects are named UCX_<MeshName>_## (convex) and are exported next to their mesh.
"""
import math
import os
import random
import sys

import bmesh
import bpy
from mathutils import Vector

REPO_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", ".."))
EXPORT_ROOT = os.path.join(REPO_ROOT, "Art", "Export")
SOURCE_ROOT = os.path.join(REPO_ROOT, "Art", "Blender", "Source")


def script_args():
    """Arguments after '--' on the Blender command line."""
    return sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []


def reset_scene():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    scene = bpy.context.scene
    scene.unit_settings.system = "METRIC"
    scene.unit_settings.scale_length = 1.0
    return scene


def get_material(name, color=(0.8, 0.8, 0.8, 1.0)):
    """Material whose name becomes the Unreal slot name. Viewport color is only a preview aid."""
    mat = bpy.data.materials.get(name) or bpy.data.materials.new(name)
    mat.diffuse_color = color
    return mat


def new_object(name, mesh, collection=None):
    obj = bpy.data.objects.new(name, mesh)
    (collection or bpy.context.scene.collection).objects.link(obj)
    return obj


def box_mesh(name, size, center=(0, 0, 0)):
    """Axis-aligned box mesh with the given size (x, y, z) in metres."""
    bm = bmesh.new()
    bmesh.ops.create_cube(bm, size=1.0)
    for v in bm.verts:
        v.co = Vector((v.co.x * size[0] + center[0], v.co.y * size[1] + center[1], v.co.z * size[2] + center[2]))
    mesh = bpy.data.meshes.new(name)
    bm.to_mesh(mesh)
    bm.free()
    return mesh


def add_box(name, size, center=(0, 0, 0), material=None, collection=None):
    obj = new_object(name, box_mesh(name, size, center), collection)
    if material:
        obj.data.materials.append(material)
    return obj


def soften(obj, width=0.004, segments=2, harden=True):
    """Small rounded bevel plus weighted normals: the 'slightly softened edge' look."""
    bev = obj.modifiers.new("SoftBevel", "BEVEL")
    bev.width = width
    bev.segments = segments
    bev.limit_method = "ANGLE"
    bev.angle_limit = math.radians(40)
    bev.harden_normals = harden
    bev.miter_outer = "MITER_ARC"
    wn = obj.modifiers.new("WeightedNormals", "WEIGHTED_NORMAL")
    wn.keep_sharp = True
    return obj


def soften_marked(obj, width=0.004, segments=2, harden=True):
    """soften() for subdivided meshes: the new bevel faces are tagged in the face attribute 'sb_bevel'
    (and every face in 'sb_tagged'), so bake_vertex_masks() puts edge wear on the real bevels only.
    The edge-length rule cannot tell bevels from the narrow faces of a densely subdivided board.
    Applies the modifiers. Assumes the object uses material slot 0 only."""
    marker = bpy.data.materials.get("SB_BevelMarker") or bpy.data.materials.new("SB_BevelMarker")
    obj.data.materials.append(marker)
    mark_index = len(obj.data.materials) - 1
    soften(obj, width, segments, harden)
    obj.modifiers["SoftBevel"].material = mark_index
    apply_modifiers(obj)
    me = obj.data
    n = len(me.polygons)
    mats = [0] * n
    me.polygons.foreach_get("material_index", mats)
    bevel = [1 if m == mark_index else 0 for m in mats]
    for name, values in (("sb_bevel", bevel), ("sb_tagged", [1] * n)):
        attr = me.attributes.get(name) or me.attributes.new(name, "INT", "FACE")
        attr.data.foreach_set("value", values)
    me.polygons.foreach_set("material_index", [0] * n)
    me.materials.pop(index=mark_index)
    return obj


def apply_modifiers(obj):
    bpy.context.view_layer.objects.active = obj
    for m in list(obj.modifiers):
        try:
            bpy.ops.object.modifier_apply(modifier=m.name)
        except RuntimeError:
            # Disabled modifiers (e.g. a zero-width bevel) have no effect; drop them.
            obj.modifiers.remove(m)


def join(objects, name):
    """Join objects into one mesh (keeps per-face materials)."""
    for o in objects:
        apply_modifiers(o)
    bpy.ops.object.select_all(action="DESELECT")
    for o in objects:
        o.select_set(True)
    bpy.context.view_layer.objects.active = objects[0]
    bpy.ops.object.join()
    result = bpy.context.view_layer.objects.active
    result.name = name
    result.data.name = name
    return result


def world_box_uv(obj, metres_per_uv=1.0, layer="UVMap"):
    """Per-face box projection in object space: 1 UV unit = metres_per_uv metres.

    Gives every mesh a valid UV0 (required for tangents) with consistent texel density,
    so tiling materials line up across separately built parts.
    """
    me = obj.data
    bm = bmesh.new()
    bm.from_mesh(me)
    uv = bm.loops.layers.uv.get(layer) or bm.loops.layers.uv.new(layer)
    s = 1.0 / metres_per_uv
    for f in bm.faces:
        n = f.normal
        ax, ay, az = abs(n.x), abs(n.y), abs(n.z)
        for loop in f.loops:
            co = loop.vert.co
            if az >= ax and az >= ay:
                u, v = co.x, co.y if n.z >= 0 else -co.y
            elif ax >= ay:
                u, v = (co.y if n.x >= 0 else -co.y), co.z
            else:
                u, v = (-co.x if n.y >= 0 else co.x), co.z
            loop[uv].uv = (u * s, v * s)
    bm.to_mesh(me)
    bm.free()
    return obj


def offset_uv(obj, du, dv, layer="UVMap"):
    """Shifts all UVs of an object (tiling materials): gives each part its own slice of the texture."""
    uv = obj.data.uv_layers.get(layer) or (obj.data.uv_layers[0] if obj.data.uv_layers else None)
    if uv is None:
        return obj
    for d in uv.data:
        d.uv = (d.uv[0] + du, d.uv[1] + dv)
    return obj


def add_cylinder(name, radius, depth, center=(0, 0, 0), axis="Z", segments=16, material=None, radius_top=None, collection=None):
    """Capped cylinder (optionally tapered) along an axis, centred at center."""
    bm = bmesh.new()
    r2 = radius if radius_top is None else radius_top
    bmesh.ops.create_cone(bm, cap_ends=True, cap_tris=False, segments=segments, radius1=radius, radius2=r2, depth=depth, calc_uvs=True)
    rot = {"Z": None, "X": ("Y", math.pi / 2), "Y": ("X", -math.pi / 2)}[axis]
    if rot:
        from mathutils import Matrix
        bmesh.ops.rotate(bm, verts=bm.verts, cent=(0, 0, 0), matrix=Matrix.Rotation(rot[1], 3, rot[0]))
    bmesh.ops.translate(bm, verts=bm.verts, vec=Vector(center))
    for f in bm.faces:
        f.smooth = len(f.verts) == 4
    mesh = bpy.data.meshes.new(name)
    bm.to_mesh(mesh)
    bm.free()
    obj = new_object(name, mesh, collection)
    if material:
        obj.data.materials.append(material)
    return obj


def add_torus(name, major, minor, center=(0, 0, 0), rotation=(0, 0, 0), major_segments=24, minor_segments=8, material=None):
    bpy.ops.mesh.primitive_torus_add(major_radius=major, minor_radius=minor, major_segments=major_segments,
                                     minor_segments=minor_segments, location=center, rotation=rotation, generate_uvs=True)
    bpy.ops.object.shade_smooth()
    obj = bpy.context.active_object
    obj.name = name
    obj.data.name = name
    bpy.ops.object.transform_apply(location=False, rotation=True, scale=True)
    # Bake location into vertices so joins keep positions; origin stays at world 0.
    for v in obj.data.vertices:
        v.co += obj.location
    obj.location = (0, 0, 0)
    if material:
        obj.data.materials.append(material)
    return obj


def transform_obj(obj, rotation=(0, 0, 0), pivot=(0, 0, 0)):
    """Rotate mesh data (radians, XYZ euler) around a pivot; keeps the object origin at world 0."""
    from mathutils import Euler, Matrix
    m = Matrix.Translation(Vector(pivot)) @ Euler(rotation).to_matrix().to_4x4() @ Matrix.Translation(-Vector(pivot))
    obj.data.transform(m)
    return obj


def bake_vertex_masks(obj, wear=1.0, crevice=1.0, waterline=None, wet_height=0.4, seed=0, island_variation=True,
                      mode="bevel", bevel_max=0.012):
    """Vertex colour masks used by M_SB_Surface (stored in the 'Col' corner attribute, linear):
    R = edge wear, G = crevice dirt + wetness below waterline+wet_height, B = random per connected part.

    mode "bevel": hard-surface parts built with soften(); faces with an edge shorter than bevel_max are the
    rounded edges and get full wear, flat faces get none (low-poly flat faces have no interior vertices,
    so vertex convexity would smear wear across them). mode "convexity": dense organic meshes (rocks)."""
    import random
    rnd = random.Random(seed)
    me = obj.data
    bm = bmesh.new()
    bm.from_mesh(me)
    bm.verts.ensure_lookup_table()
    bm.normal_update()
    convex = {}
    for v in bm.verts:
        if not v.link_edges:
            convex[v.index] = 0.0
            continue
        acc = 0.0
        for e in v.link_edges:
            o = e.other_vert(v)
            d = v.co - o.co
            if d.length > 1e-6:
                acc += v.normal.dot(d.normalized())
        convex[v.index] = acc / len(v.link_edges)
    # Connected islands -> per-part random value.
    island_val = {}
    if island_variation:
        seen = set()
        for f in bm.faces:
            if f.index in seen:
                continue
            val = rnd.random()
            stack = [f]
            while stack:
                cur = stack.pop()
                if cur.index in seen:
                    continue
                seen.add(cur.index)
                island_val[cur.index] = val
                for e in cur.edges:
                    for nf in e.link_faces:
                        if nf.index not in seen:
                            stack.append(nf)
    layer = bm.loops.layers.float_color.get("Col") or bm.loops.layers.float_color.new("Col")
    tagged = bm.faces.layers.int.get("sb_tagged")
    bevel_tag = bm.faces.layers.int.get("sb_bevel")
    for f in bm.faces:
        b = island_val.get(f.index, 0.5)
        if tagged is not None and f[tagged]:
            is_bevel = mode == "bevel" and bool(f[bevel_tag])
        else:
            is_bevel = mode == "bevel" and min(e.calc_length() for e in f.edges) < bevel_max
        for loop in f.loops:
            c = convex[loop.vert.index]
            if mode == "bevel":
                r = min(wear, 1.0) if is_bevel else 0.0
            else:
                r = min(max(c * 4.0 * wear, 0.0), 1.0)
            g = min(max(-c * 4.0 * crevice, 0.0), 1.0)
            if waterline is not None:
                z = loop.vert.co.z
                g = max(g, min(max((waterline + wet_height - z) / max(wet_height, 1e-3), 0.0), 1.0))
            loop[layer] = (r, g, b, 1.0)
    bm.to_mesh(me)
    bm.free()
    me.color_attributes.active_color = me.color_attributes["Col"]
    return obj


def jitter_vertices(obj, amount=0.002, seed=1):
    """Subtle irregularity so hand-made objects do not look machine-perfect."""
    rnd = random.Random(seed)
    for v in obj.data.vertices:
        v.co += Vector((rnd.uniform(-amount, amount), rnd.uniform(-amount, amount), rnd.uniform(-amount, amount)))


def add_ucx_box(mesh_name, index, size, center=(0, 0, 0), collection=None):
    obj = add_box("UCX_%s_%02d" % (mesh_name, index), size, center, collection=collection)
    obj.display_type = "WIRE"
    return obj


def export_fbx(objects, rel_path):
    """Export objects (mesh + UCX_ collision) to Art/Export/<rel_path>. Returns the absolute path."""
    path = os.path.join(EXPORT_ROOT, rel_path)
    os.makedirs(os.path.dirname(path), exist_ok=True)
    for o in objects:
        if o.type == "MESH" and not o.name.startswith(("UCX_", "UBX_", "USP_", "UCP_")) and not o.data.uv_layers:
            raise RuntimeError("%s has no UV map; call world_box_uv() or unwrap before export" % o.name)
    bpy.ops.object.select_all(action="DESELECT")
    for o in objects:
        o.select_set(True)
    bpy.context.view_layer.objects.active = objects[0]
    bpy.ops.export_scene.fbx(
        filepath=path,
        use_selection=True,
        object_types={"MESH", "EMPTY"},
        global_scale=1.0,
        apply_unit_scale=True,
        apply_scale_options="FBX_SCALE_NONE",
        axis_forward="-Z",
        axis_up="Y",
        use_space_transform=True,
        bake_space_transform=False,
        use_mesh_modifiers=True,
        mesh_smooth_type="FACE",
        use_tspace=True,
        use_triangles=False,
        colors_type="LINEAR",
        add_leaf_bones=False,
        bake_anim=False,
        path_mode="STRIP",
        embed_textures=False,
    )
    print("SB_EXPORTED", path)
    return path


def save_blend(name):
    os.makedirs(SOURCE_ROOT, exist_ok=True)
    path = os.path.join(SOURCE_ROOT, name + ".blend")
    bpy.ops.wm.save_as_mainfile(filepath=path, compress=True)
    print("SB_SAVED", path)
    return path
