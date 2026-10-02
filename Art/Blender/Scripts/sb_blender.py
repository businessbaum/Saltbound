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


def apply_modifiers(obj):
    bpy.context.view_layer.objects.active = obj
    for m in list(obj.modifiers):
        bpy.ops.object.modifier_apply(modifier=m.name)


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
        colors_type="SRGB",
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
