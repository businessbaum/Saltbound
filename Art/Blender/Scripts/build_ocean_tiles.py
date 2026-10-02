"""Ocean CDLOD tiles: flat grids whose vertices sit on integer local coordinates (Unreal units).

SM_OceanTile32: 32 x 32 cells, local -16..16;  SM_OceanTile16: 16 x 16 cells, local -8..8.
Blender units are metres, so one cell = 0.01 m (= 1 uu); ASaltOcean scales instances by the cell size.
The material morphs odd vertices onto even ones, so the grid must be regular and centred on an even index.
"""
import os
import sys

import bmesh
import bpy

sys.path.insert(0, os.path.dirname(__file__))
import sb_blender as sb  # noqa: E402

CELL = 0.01  # metres per cell -> 1 uu


def make_tile(name, cells):
    half = cells // 2
    bm = bmesh.new()
    uv_layer = bm.loops.layers.uv.new("UVMap")
    verts = {}
    for j in range(-half, half + 1):
        for i in range(-half, half + 1):
            verts[(i, j)] = bm.verts.new((i * CELL, j * CELL, 0.0))
    for j in range(-half, half):
        for i in range(-half, half):
            quad = [verts[(i, j)], verts[(i + 1, j)], verts[(i + 1, j + 1)], verts[(i, j + 1)]]
            # Alternate the diagonal per cell so morphing to the coarse grid stays symmetric.
            if (i + j) % 2 == 0:
                tris = [(quad[0], quad[1], quad[2]), (quad[0], quad[2], quad[3])]
            else:
                tris = [(quad[0], quad[1], quad[3]), (quad[1], quad[2], quad[3])]
            for t in tris:
                f = bm.faces.new(t)
                for loop in f.loops:
                    co = loop.vert.co
                    loop[uv_layer].uv = ((co.x / CELL + half) / cells, (co.y / CELL + half) / cells)
    mesh = bpy.data.meshes.new(name)
    bm.to_mesh(mesh)
    bm.free()
    obj = sb.new_object(name, mesh)
    obj.data.materials.append(sb.get_material("MI_Ocean", (0.1, 0.3, 0.4, 1)))
    return obj


sb.reset_scene()
for name, cells in (("SM_OceanTile32", 32), ("SM_OceanTile16", 16)):
    obj = make_tile(name, cells)
    sb.export_fbx([obj], os.path.join("Water", name + ".fbx"))
    bpy.data.objects.remove(obj)
print("SB_OCEAN_TILES_OK")
