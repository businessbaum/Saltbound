"""Held tools (D55, UI_Design.md section 4): what the player holds from the tool wheel.

  SM_Tool_Compass        a brass pocket compass, about 7 cm across: a turned case with a bezel, a pendant and a bow at
                         its north edge (+X), a cream dial with dark ticks (the four quarters longer) and a red north
                         mark; pivot at the dial's centre, the dial facing +Z
  SM_Tool_CompassNeedle  its needle, pivot at its centre, north half red along +X, south half dark, a brass cap; the game
                         turns it about +Z so the red end settles on world north

The lantern is the boat's (SM_Boat_Lantern). The spyglass is looked through (the view narrows), so it needs no mesh.
Plain shapes and the project's materials, no textures (D38). Run: Tools/Build-BlenderAsset.ps1 build_tools
"""
import math
import os
import sys

import bmesh

sys.path.insert(0, os.path.dirname(__file__))
import build_boat as bb  # noqa: E402  (the boat script's parts: lathe, ring, rod, block, finish)
import sb_blender as sb  # noqa: E402


def export(obj):
    sb.save_blend(obj.name)
    sb.export_fbx([obj], os.path.join("Tools", obj.name + ".fbx"))


def flat(name, outline, z0, z1, mat):
    """A flat piece: the outline [(x, y)] (Unreal) extruded from z0 to z1."""
    bm = bmesh.new()
    bottom = [bm.verts.new(bb.U((x, y, z0))) for x, y in outline]
    top = [bm.verts.new(bb.U((x, y, z1))) for x, y in outline]
    bm.faces.new(list(reversed(bottom)))
    bm.faces.new(top)
    n = len(outline)
    for i in range(n):
        j = (i + 1) % n
        bm.faces.new([bottom[i], bottom[j], top[j], top[i]])
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    return bb.mesh_object(name, bm, [mat])


def build_compass():
    dial_z = 0.0015
    parts = [
        # The case: a shallow turned dish, its rim rolled over, the dial sitting inside.
        bb.lathe("compass_case", [(0.0, -0.009), (0.027, -0.009), (0.031, -0.0075), (0.0335, -0.004), (0.034, 0.0005), (0.0325, 0.0045),
                                  (0.0298, 0.005), (0.0294, 0.0), (0.0, 0.0)], (0.0, 0.0, 0.0), "MI_Metal_Brass", 40),
        bb.ring("compass_bezel", 0.0306, 0.0021, (0.0, 0.0, 0.0047), (0, 0, 1), "MI_Metal_Brass", 40),
        bb.disc("compass_dial", 0.0294, 0.0015, (0.0, 0.0, dial_z / 2), (0, 0, 1), "MI_Paint_CreamAged", 40),
        # Pendant and bow at the north edge.
        bb.rod("compass_pendant", (0.033, 0.0, -0.002), (0.041, 0.0, -0.002), 0.0032, "MI_Metal_Brass", 10),
        bb.ring("compass_bow", 0.0075, 0.0017, (0.0475, 0.0, -0.002), (0, 0, 1), "MI_Metal_Brass", 20),
    ]
    # Sixteen ticks round the dial, the quarters longer; a red north mark inside the north tick.
    for k in range(16):
        a = 2 * math.pi * k / 16
        quarter = k % 4 == 0
        length = 0.0072 if quarter else 0.0034
        r = 0.0282 - length / 2
        tick = bb.block("compass_tick", (length, 0.0016 if quarter else 0.0011, 0.0007), (r, 0.0, dial_z + 0.00035), "MI_Metal_Iron", 0.0002)
        sb.transform_obj(tick, (0.0, 0.0, a), (0.0, 0.0, 0.0))
        parts.append(tick)
    parts.append(flat("compass_north", [(0.0175, -0.0028), (0.0205, 0.0), (0.0175, 0.0028)], dial_z, dial_z + 0.0008, "MI_Paint_Hull"))
    obj = bb.finish(parts, "SM_Tool_Compass", wear=0.5, chips=0.0)
    export(obj)

    sb.reset_scene()
    bb.mats()
    needle = [flat("needle_north", [(0.0, -0.0031), (0.0215, 0.0), (0.0, 0.0031)], 0.0, 0.0008, "MI_Paint_Hull"),
              flat("needle_south", [(0.0, 0.0031), (-0.019, 0.0), (0.0, -0.0031)], 0.0, 0.0008, "MI_Metal_Iron"),
              bb.lathe("needle_cap", [(0.0, 0.0), (0.0032, 0.0), (0.0028, 0.0011), (0.0012, 0.0017), (0.0, 0.0018)], (0.0, 0.0, 0.0006),
                       "MI_Metal_Brass", 16)]
    obj = bb.finish(needle, "SM_Tool_CompassNeedle", wear=0.3, chips=0.0)
    export(obj)


def main():
    sb.reset_scene()
    bb.mats()
    build_compass()


main()
