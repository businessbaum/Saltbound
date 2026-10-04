"""Close-up check of a generated asset before it is imported (owner, D53: "check your Blender objects before you use
them, and also check for physics").

Renders one or more views of a .blend with Blender's workbench (material colours, cavity, shadows) and can overlay the
UCX collision hulls as red wireframes, so both the shape and its collision can be checked.

  Tools/Build-BlenderAsset.ps1 is not needed; run Blender directly:
  blender -b --factory-startup -P Tools/preview_blend.py -- <file.blend> <out.png> <view> [<view> ...] [--ucx]
  a view is  x,y,z,dist,az,el  (target in Unreal metres: +Y = starboard; dist in m; az/el in degrees, az 0 = from +X)
  Several views are tiled into one sheet (2 columns).
"""
import math
import sys

import bpy
from mathutils import Vector

args = sys.argv[sys.argv.index("--") + 1:]
blend, out = args[0], args[1]
show_ucx = "--ucx" in args
views = [tuple(float(v) for v in a.split(",")) for a in args[2:] if a != "--ucx"]
bpy.ops.wm.open_mainfile(filepath=blend)
scene = bpy.context.scene

wire = bpy.data.materials.new("ucx_wire")
wire.diffuse_color = (1.0, 0.1, 0.05, 1.0)
for o in list(scene.objects):
    if o.name.startswith("UCX_"):
        if show_ucx:
            mod = o.modifiers.new("wire", "WIREFRAME")
            mod.thickness = 0.008
            o.data.materials.clear()
            o.data.materials.append(wire)
            o.hide_render = False
        else:
            bpy.data.objects.remove(o)

cam = bpy.data.objects.new("cam", bpy.data.cameras.new("cam"))
scene.collection.objects.link(cam)
cam.data.lens = 40
scene.camera = cam
scene.render.engine = "BLENDER_WORKBENCH"
shading = scene.display.shading
shading.light = "STUDIO"
shading.color_type = "MATERIAL"
shading.show_cavity = True
shading.cavity_type = "BOTH"
shading.show_shadows = True
scene.render.resolution_x, scene.render.resolution_y = 900, 640
scene.render.film_transparent = False
scene.world = scene.world or bpy.data.worlds.new("w")
scene.world.color = (0.55, 0.62, 0.68)

tiles = []
for k, (x, y, z, dist, az, el) in enumerate(views):
    target = Vector((x, -y, z))                       # Unreal -> Blender (Y mirrored)
    a, e = math.radians(az), math.radians(el)
    cam.location = target + Vector((math.cos(a) * math.cos(e), -math.sin(a) * math.cos(e), math.sin(e))) * dist
    cam.rotation_euler = (target - cam.location).to_track_quat("-Z", "Y").to_euler()
    path = out.replace(".png", "_%d.png" % k)
    scene.render.filepath = path
    bpy.ops.render.render(write_still=True)
    tiles.append(path)

# Tile the views into one sheet.
imgs = [bpy.data.images.load(p) for p in tiles]
w, h = imgs[0].size
cols = 2 if len(imgs) > 1 else 1
rows = (len(imgs) + cols - 1) // cols
sheet = bpy.data.images.new("sheet", w * cols, h * rows)
import numpy as np  # noqa: E402
px = np.zeros((h * rows, w * cols, 4), dtype=np.float32)
for k, im in enumerate(imgs):
    a = np.array(im.pixels[:], dtype=np.float32).reshape(h, w, 4)
    r, c = rows - 1 - k // cols, k % cols
    px[r * h:(r + 1) * h, c * w:(c + 1) * w] = a
sheet.pixels[:] = px.ravel()
sheet.filepath_raw = out
sheet.file_format = "PNG"
sheet.save()
print("SB_PREVIEW", out, len(views))
