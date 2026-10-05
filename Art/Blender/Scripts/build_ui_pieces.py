"""The UI's hero pieces as real 3D (D55, UI_Design.md section 11: "real highlights and real rope twist are what make it
look made, not drawn"): modelled here, rendered orthographically top-down with one light setup, transparent, and laid
into the UI parts by Tools/textures/ui_parts.js (which keeps the design's own drawing as the fallback).

  Tools/Build-BlenderAsset.ps1 build_ui_pieces [piece ...]      writes Art/Textures/UI/Render/R_<Piece>.png

  RopeRing   the tool wheel's three-strand hemp ring: radius 262, 46 thick (1 px = 1 mm at 1080p), a 57 degree lay,
             frame 720 x 720 centred (the ring's soft shadow and the dark disc behind it are drawn by the parts page)

Scene units: metres, 1 mm = 1 px at 1080p. The camera looks straight down (-Z); the key light comes from the upper left
of the screen, as the design's bevels do; a cooler fill from the lower right.
"""
import math
import os
import sys

import bmesh
import bpy
from mathutils import Vector

sys.path.insert(0, os.path.dirname(__file__))
import sb_blender as sb  # noqa: E402

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", ".."))
OUT = os.path.join(ROOT, "Art", "Textures", "UI", "Render")
SUPERSAMPLE = 2


def studio(frame_px, samples=96):
    """Orthographic top-down camera framing frame_px x frame_px mm, a soft key from the upper left, a fill, Cycles."""
    scene = bpy.context.scene
    scene.render.engine = "CYCLES"
    scene.cycles.device = "CPU"
    scene.cycles.samples = samples
    scene.cycles.use_denoising = True
    scene.render.film_transparent = True
    scene.render.resolution_x = scene.render.resolution_y = frame_px * SUPERSAMPLE
    scene.render.image_settings.file_format = "PNG"
    scene.render.image_settings.color_mode = "RGBA"
    scene.view_settings.view_transform = "Standard"
    scene.view_settings.look = "None"
    cam_data = bpy.data.cameras.new("cam")
    cam_data.type = "ORTHO"
    cam_data.ortho_scale = frame_px / 1000.0
    cam = bpy.data.objects.new("cam", cam_data)
    scene.collection.objects.link(cam)
    cam.location = (0.0, 0.0, 2.0)
    scene.camera = cam
    # Screen up is +Y in the scene (the camera looks down -Z with its up along +Y).
    key = bpy.data.lights.new("key", "AREA")
    key.energy = 34.0
    key.size = 0.35
    key.color = (1.0, 0.95, 0.86)
    k = bpy.data.objects.new("key", key)
    scene.collection.objects.link(k)
    k.location = (-1.1, 1.1, 1.4)
    k.rotation_euler = (Vector((0, 0, 0)) - k.location).to_track_quat("-Z", "Y").to_euler()
    fill = bpy.data.lights.new("fill", "AREA")
    fill.energy = 3.0
    fill.size = 2.0
    fill.color = (0.82, 0.88, 1.0)
    f = bpy.data.objects.new("fill", fill)
    scene.collection.objects.link(f)
    f.location = (1.0, -1.0, 1.0)
    f.rotation_euler = (Vector((0, 0, 0)) - f.location).to_track_quat("-Z", "Y").to_euler()
    world = bpy.data.worlds.new("w")
    world.use_nodes = True
    world.node_tree.nodes["Background"].inputs[0].default_value = (0.32, 0.3, 0.28, 1.0)
    world.node_tree.nodes["Background"].inputs[1].default_value = 0.12
    scene.world = world


def principled(name, color, roughness, metallic=0.0, sheen=0.0, bump=None):
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    nt = m.node_tree
    bsdf = nt.nodes["Principled BSDF"]
    bsdf.inputs["Base Color"].default_value = (*color, 1.0)
    bsdf.inputs["Roughness"].default_value = roughness
    bsdf.inputs["Metallic"].default_value = metallic
    if sheen and "Sheen Weight" in bsdf.inputs:
        bsdf.inputs["Sheen Weight"].default_value = sheen
    if bump:
        # Fine fibres / grain: a stretched noise into a bump.
        scale, strength, stretch = bump
        tc = nt.nodes.new("ShaderNodeTexCoord")
        mp = nt.nodes.new("ShaderNodeMapping")
        mp.inputs["Scale"].default_value = (scale * stretch[0], scale * stretch[1], scale * stretch[2])
        nz = nt.nodes.new("ShaderNodeTexNoise")
        nz.inputs["Detail"].default_value = 8.0
        bp = nt.nodes.new("ShaderNodeBump")
        bp.inputs["Strength"].default_value = strength
        nt.links.new(tc.outputs["Object"], mp.inputs["Vector"])
        nt.links.new(mp.outputs["Vector"], nz.inputs["Vector"])
        nt.links.new(nz.outputs["Fac"], bp.inputs["Height"])
        nt.links.new(bp.outputs["Normal"], bsdf.inputs["Normal"])
    return m


def tube(name, points, radius, sides, material):
    """A closed tube along a closed loop of points (a strand of rope)."""
    bm = bmesh.new()
    n = len(points)
    rings = []
    for i in range(n):
        p = points[i]
        t = (points[(i + 1) % n] - points[i - 1]).normalized()
        a = Vector((0, 0, 1)).cross(t)
        if a.length < 1e-6:
            a = Vector((1, 0, 0))
        a.normalize()
        b = t.cross(a).normalized()
        rings.append([bm.verts.new(p + (a * math.cos(2 * math.pi * k / sides) + b * math.sin(2 * math.pi * k / sides)) * radius)
                      for k in range(sides)])
    for i in range(n):
        r0, r1 = rings[i], rings[(i + 1) % n]
        for k in range(sides):
            bm.faces.new((r0[k], r0[(k + 1) % sides], r1[(k + 1) % sides], r1[k]))
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    obj = sb.new_object(name, me)
    obj.data.materials.append(material)
    for poly in obj.data.polygons:
        poly.use_smooth = True
    return obj


def rope_ring():
    """Three strands laid round a ring: radius 262 mm, rope 46 mm thick, a 57 degree lay; each strand a little uneven."""
    studio(720)
    R, T = 0.262, 0.046
    rh = (T / 2) / (1 + 0.866)                     # strand centres round the rope's core
    rs = rh * 0.9                                  # strands touch at 0.866 rh; a little fuller so they press together
    lay = 2 * math.pi * rh / math.tan(math.radians(57.0))
    turns = round(2 * math.pi * R / lay)
    hemp = principled("hemp", (0.52, 0.33, 0.13), 0.6, sheen=0.3, bump=(380.0, 0.5, (1.0, 1.0, 6.0)))
    steps = 2400
    for s in range(3):
        pts = []
        for i in range(steps):
            u = i / steps
            a = 2 * math.pi * u
            radial = Vector((math.cos(a), math.sin(a), 0.0))
            up = Vector((0.0, 0.0, 1.0))
            ph = 2 * math.pi * turns * u + s * 2 * math.pi / 3
            wob = 1.0 + 0.04 * math.sin(7 * a + s) + 0.02 * math.sin(23 * a + 2 * s)
            core = radial * R
            pts.append(core + (radial * math.cos(ph) + up * math.sin(ph)) * rh * wob)
        tube("strand%d" % s, pts, rs, 14, hemp)
    render("RopeRing")


def render(name):
    os.makedirs(OUT, exist_ok=True)
    path = os.path.join(OUT, "R_%s.png" % name)
    bpy.context.scene.render.filepath = path
    bpy.ops.render.render(write_still=True)
    print("SB_RENDERED", path)


PIECES = {"RopeRing": rope_ring}


def main():
    for name in sb.script_args() or list(PIECES):
        sb.reset_scene()
        PIECES[name]()


main()
