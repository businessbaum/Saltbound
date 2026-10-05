"""Preview the hub ship's art meshes together (D62), before they are imported: every Art/Blender/Source/SM_Hub_*.blend (or
the ones named) loaded into one scene and rendered with the workbench (material colours, cavity, shadows), like
preview_blend.py.

  blender -b --factory-startup -P Tools/preview_hub.py -- <out.png> <view> [<view> ...] [--parts A,B] [--blk]
  a view is  x,y,z,dist,az,el  in ship metres (x from the stern face, +y starboard; az 0 = looking from the bow side,
  90 = from starboard; el up from the horizon). --blk adds the blockout parts that have no art yet (grey).
"""
import glob
import math
import os
import sys

import bpy
from mathutils import Vector

args = sys.argv[sys.argv.index("--") + 1:]
out = args[0]
names = None
blk = "--blk" in args
views = []
i = 1
while i < len(args):
    if args[i] == "--parts":
        names = args[i + 1].split(",")
        i += 2
        continue
    if args[i] != "--blk":
        views.append(tuple(float(v) for v in args[i].split(",")))
    i += 1
src = os.path.join(os.path.dirname(__file__), "..", "Art", "Blender", "Source")
files = sorted(glob.glob(os.path.join(src, "SM_Hub_*.blend")))
if names:
    files = [f for f in files if os.path.basename(f)[:-6] in names]
bpy.ops.wm.read_factory_settings(use_empty=True)
scene = bpy.context.scene
for f in files:
    with bpy.data.libraries.load(f) as (src_data, dst):
        dst.objects = [n for n in src_data.objects if not n.startswith("UCX_")]
    for o in dst.objects:
        if o is not None:
            scene.collection.objects.link(o)
print("SB_PREVIEW_PARTS", [os.path.basename(f) for f in files])

cam = bpy.data.objects.new("cam", bpy.data.cameras.new("cam"))
scene.collection.objects.link(cam)
cam.data.lens = 35
cam.data.clip_end = 3000.0
scene.camera = cam
scene.render.engine = "BLENDER_WORKBENCH"
shading = scene.display.shading
shading.light = "STUDIO"
shading.color_type = "MATERIAL"
shading.show_cavity = True
shading.cavity_type = "BOTH"
shading.show_shadows = True
scene.render.resolution_x, scene.render.resolution_y = 1200, 760
scene.world = bpy.data.worlds.new("w")
scene.world.color = (0.55, 0.62, 0.68)

tiles = []
for k, (x, y, z, dist, az, el) in enumerate(views):
    target = Vector((x - 80.0, -y, z))
    a, e = math.radians(az), math.radians(el)
    cam.location = target + Vector((math.cos(a) * math.cos(e), -math.sin(a) * math.cos(e), math.sin(e))) * dist
    cam.rotation_euler = (target - cam.location).to_track_quat("-Z", "Y").to_euler()
    path = out.replace(".png", "_%d.png" % k)
    scene.render.filepath = path
    bpy.ops.render.render(write_still=True)
    tiles.append(path)

imgs = [bpy.data.images.load(p) for p in tiles]
w, h = imgs[0].size
cols = 2 if len(imgs) > 1 else 1
rows = (len(imgs) + cols - 1) // cols
import numpy as np  # noqa: E402
px = np.zeros((h * rows, w * cols, 4), dtype=np.float32)
for k, im in enumerate(imgs):
    a = np.array(im.pixels[:], dtype=np.float32).reshape(h, w, 4)
    r, c = rows - 1 - k // cols, k % cols
    px[r * h:(r + 1) * h, c * w:(c + 1) * w] = a
sheet = bpy.data.images.new("sheet", w * cols, h * rows)
sheet.pixels[:] = px.ravel()
sheet.filepath_raw = out
sheet.file_format = "PNG"
sheet.save()
for p in tiles:
    os.remove(p)
print("SB_PREVIEW", out, len(views))
