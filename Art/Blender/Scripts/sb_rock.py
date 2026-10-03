"""Stylised rock generator (chunky, faceted, layered; see Docs/ArtDirection.md).

Pipeline (all deterministic per seed):
  1. Blob from overlapping convex-hull chunks, voxel-remeshed into one closed surface.
  2. Chiselled facets: planar cuts slice caps off the blob (flat faces with crisp-ish edges).
  3. Fine voxel remesh, then vectorised displacement along the normal:
       - fractured plates: 3D Voronoi cells pushed in/out by a random amount (steps at the cell borders),
       - cracks: grooves along the cell borders,
       - strata: stepped horizontal ledges on steep faces (slightly dipping layers),
       - small lumps.
  4. Light smoothing (soft stylised edges), decimation to a face budget (Nanite meshes).
  5. Vertex colour masks for M_SB_Rock: R convex edges, G cavities, B per-plate tone value.
"""
import math
import random

import bmesh
import bpy
import numpy as np
from mathutils import Vector

import sb_blender as sb


# --- vectorised helpers -------------------------------------------------------------------------

def _hash3(ix, iy, iz, seed):
    """Integer lattice hash -> three floats in [0, 1)."""
    h = (ix.astype(np.int64) * 73856093) ^ (iy.astype(np.int64) * 19349663) ^ (iz.astype(np.int64) * 83492791) ^ (seed * 2654435761)
    h = h & 0xFFFFFFFF
    out = []
    for k in range(3):
        h = (h * 1103515245 + 12345 + k * 977) & 0x7FFFFFFF
        out.append((h % 100003) / 100003.0)
    return out


def worley3(p, cell, seed):
    """3D Worley on points p (N, 3) with cell size `cell`. Returns f1, f2 (distances) and the id hash of
    the nearest cell (float in [0, 1))."""
    q = p / cell
    base = np.floor(q).astype(np.int64)
    f1 = np.full(len(p), 1e9)
    f2 = np.full(len(p), 1e9)
    cid = np.zeros(len(p))
    for dx in (-1, 0, 1):
        for dy in (-1, 0, 1):
            for dz in (-1, 0, 1):
                c = base + np.array([dx, dy, dz])
                jx, jy, jz = _hash3(c[:, 0], c[:, 1], c[:, 2], seed)
                fp = np.stack([c[:, 0] + jx, c[:, 1] + jy, c[:, 2] + jz], -1)
                d = np.linalg.norm(q - fp, axis=1)
                closer = d < f1
                f2 = np.where(closer, f1, np.minimum(f2, d))
                cid = np.where(closer, (jx * 7.31 + jy * 3.17 + jz) % 1.0, cid)
                f1 = np.where(closer, d, f1)
    return f1 * cell, f2 * cell, cid


def value_noise3(p, scale, seed):
    """Smooth 3D value noise in [-1, 1] (trilinear with smoothstep weights)."""
    q = p / scale
    b = np.floor(q).astype(np.int64)
    f = q - b
    w = f * f * (3 - 2 * f)
    acc = np.zeros(len(p))
    for dx in (0, 1):
        for dy in (0, 1):
            for dz in (0, 1):
                v, _, _ = _hash3(b[:, 0] + dx, b[:, 1] + dy, b[:, 2] + dz, seed)
                wx = w[:, 0] if dx else 1 - w[:, 0]
                wy = w[:, 1] if dy else 1 - w[:, 1]
                wz = w[:, 2] if dz else 1 - w[:, 2]
                acc += v * wx * wy * wz
    return acc * 2 - 1


def fbm3(p, scale, seed, octaves=3):
    out = np.zeros(len(p))
    amp, tot = 1.0, 0.0
    for o in range(octaves):
        out += value_noise3(p, scale / (2 ** o), seed + o * 31) * amp
        tot += amp
        amp *= 0.5
    return out / tot


def mesh_arrays(me):
    n = len(me.vertices)
    co = np.empty(n * 3)
    me.vertices.foreach_get("co", co)
    nor = np.empty(n * 3)
    me.vertex_normals.foreach_get("vector", nor)
    e = np.empty(len(me.edges) * 2, dtype=np.int64)
    me.edges.foreach_get("vertices", e)
    return co.reshape(-1, 3), nor.reshape(-1, 3), e.reshape(-1, 2)


def neighbour_mean(values, edges, n):
    acc = np.zeros((n,) + values.shape[1:])
    cnt = np.zeros(n)
    np.add.at(acc, edges[:, 0], values[edges[:, 1]])
    np.add.at(acc, edges[:, 1], values[edges[:, 0]])
    np.add.at(cnt, edges[:, 0], 1)
    np.add.at(cnt, edges[:, 1], 1)
    cnt = np.maximum(cnt, 1)
    return acc / (cnt[:, None] if values.ndim > 1 else cnt)


# --- geometry steps -----------------------------------------------------------------------------

def _remesh(obj, voxel):
    m = obj.modifiers.new("Remesh", "REMESH")
    m.mode = "VOXEL"
    m.voxel_size = voxel
    m.use_smooth_shade = True
    sb.apply_modifiers(obj)


def _blob(name, size, chunks, rng):
    objs = []
    for c in range(chunks):
        bm = bmesh.new()
        if c == 0:
            cx, cy, cz = 0.0, 0.0, 0.25 * size[2]
            sx, sy, sz = size[0] * 0.5, size[1] * 0.5, size[2] * 0.55
        else:
            cx = rng.uniform(-0.28, 0.28) * size[0]
            cy = rng.uniform(-0.28, 0.28) * size[1]
            cz = rng.uniform(0.05, 0.35) * size[2]
            sx, sy, sz = (size[0] * rng.uniform(0.32, 0.55), size[1] * rng.uniform(0.32, 0.55), size[2] * rng.uniform(0.35, 0.62))
        for _ in range(22):
            v = Vector((rng.gauss(0, 1), rng.gauss(0, 1), rng.gauss(0, 1))).normalized() * rng.uniform(0.7, 1.0)
            bm.verts.new((cx + v.x * sx, cy + v.y * sy, cz + v.z * sz))
        bmesh.ops.convex_hull(bm, input=bm.verts)
        me = bpy.data.meshes.new(name + "_chunk")
        bm.to_mesh(me)
        bm.free()
        objs.append(sb.new_object("%s_chunk%d" % (name, c), me))
    return sb.join(objs, name)


def _planar_cuts(obj, cuts, rng, keep=(0.72, 0.9)):
    """Slice caps off the blob with random planes (flat chiselled facets). Never cuts the underside."""
    bm = bmesh.new()
    bm.from_mesh(obj.data)
    center = sum((v.co for v in bm.verts), Vector()) / max(len(bm.verts), 1)
    for _ in range(cuts):
        az = rng.uniform(0, 2 * math.pi)
        el = rng.uniform(-0.15, 0.9)          # mostly sideways, some upward-facing flats
        n = Vector((math.cos(az) * math.cos(el), math.sin(az) * math.cos(el), math.sin(el))).normalized()
        reach = max((v.co - center).dot(n) for v in bm.verts)
        co = center + n * reach * rng.uniform(*keep)
        geom = bm.verts[:] + bm.edges[:] + bm.faces[:]
        bmesh.ops.bisect_plane(bm, geom=geom, plane_co=co, plane_no=n, clear_outer=True)
        boundary = [e for e in bm.edges if len(e.link_faces) == 1]
        if boundary:
            bmesh.ops.holes_fill(bm, edges=boundary, sides=0)
    bm.to_mesh(obj.data)
    bm.free()


def stylised_rock(name, size, seed, chunks=5, cuts=9, voxel=None, plate_cell=None, plate_amount=None,
                  crack_depth=None, strata_height=None, strata_amount=None, lump=None, bottom=-2.5,
                  target_faces=60000, smooth=0.6, smooth_iterations=3, material=None):
    """Builds the rock object (origin = water level) and returns it. Sizes in metres.

    Default style (owner feedback 2026-10-03, Sea of Thieves reference): simple chunky shapes with flat
    chiselled facets and soft bevels; surface detail comes from the hand-painted material, not from
    carved geometry. Plates, cracks and strata stay available but are off unless amounts are passed."""
    rng = random.Random(seed)
    s = max(size)
    voxel = voxel or s / 150.0
    plate_cell = plate_cell or s * 0.3
    plate_amount = plate_amount if plate_amount is not None else 0.0
    crack_depth = crack_depth if crack_depth is not None else 0.0
    strata_height = strata_height or max(size[2] * 0.16, 0.2)
    strata_amount = strata_amount if strata_amount is not None else 0.0
    lump = lump if lump is not None else s * 0.004

    rock = _blob(name, size, chunks, rng)
    # Sink the base below the water so rocks never float.
    for v in rock.data.vertices:
        if v.co.z < 0:
            v.co.z = v.co.z * 0.3 + bottom * 0.7
    _remesh(rock, s / 45.0)
    _planar_cuts(rock, cuts, rng)
    _remesh(rock, voxel)

    me = rock.data
    co, nor, edges = mesh_arrays(me)
    n = len(co)
    # Domain warp so cells and layers are not too regular.
    warp = np.stack([fbm3(co, s * 0.35, seed + k * 7, 2) for k in range(3)], -1) * (s * 0.04)
    pw = co + warp
    # Fractured plates + cracks along their borders.
    f1, f2, cid = worley3(pw, plate_cell, seed + 101)
    edge = f2 - f1
    plates = (cid - 0.5) * 2.0 * plate_amount
    crack_w = plate_cell * 0.04
    # Plate offsets fade to zero inside the crack, so neighbouring plates meet at the groove bottom
    # (a hard step there aliases into zipper seams on the voxel mesh).
    t_edge = np.clip(edge / (crack_w * 1.3), 0, 1)
    plates = plates * t_edge * t_edge * (3 - 2 * t_edge)
    groove = -crack_depth * np.clip(1.0 - edge / crack_w, 0, 1) ** 1.2
    # A few secondary cracks (sparse; a dense network reads as a turtle shell).
    g1, g2, _ = worley3(pw * np.array([1.0, 1.0, 1.6]), plate_cell * 0.5, seed + 202)
    sparse = fbm3(co, s * 0.3, seed + 303, 2) > 0.3
    fine = -crack_depth * 0.7 * np.clip(1.0 - (g2 - g1) / (crack_w * 0.7), 0, 1) ** 1.5 * sparse
    # Strata: terraces on steep faces (a sharp shelf at the base of each layer, the face receding
    # towards the layer top), layers dipping slightly.
    dip = co[:, 0] * 0.06 + co[:, 1] * 0.03
    t = (co[:, 2] + dip + warp[:, 2] * 0.5) / strata_height
    frac = t - np.floor(t)
    steep = np.clip(1.0 - np.abs(nor[:, 2]) * 1.3, 0, 1)
    shelf = np.clip(frac / 0.12, 0, 1)
    shelf = shelf * shelf * (3 - 2 * shelf)
    strata = (shelf * (1.0 - 0.6 * frac) - 0.5) * strata_amount * steep
    lumps = fbm3(co, s * 0.12, seed + 404, 3) * lump
    disp = plates + groove + fine + strata + lumps
    co = co + nor * disp[:, None]
    me.vertices.foreach_set("co", co.ravel())
    me.update()

    if smooth > 0:
        m = rock.modifiers.new("Smooth", "SMOOTH")
        m.factor = smooth
        m.iterations = smooth_iterations
    faces = len(me.polygons)
    if faces > target_faces:
        d = rock.modifiers.new("Decimate", "DECIMATE")
        d.decimate_type = "COLLAPSE"
        d.ratio = target_faces / faces
    sb.apply_modifiers(rock)
    for p in rock.data.polygons:
        p.use_smooth = True

    # Vertex masks (recomputed on the final mesh).
    me = rock.data
    co, nor, edges = mesh_arrays(me)
    n = len(co)
    lap = co - neighbour_mean(co, edges, n)
    curv = np.einsum("ij,ij->i", lap, nor)
    for _ in range(3):
        curv = 0.5 * curv + 0.5 * neighbour_mean(curv, edges, n)
    k = np.percentile(np.abs(curv), 92) + 1e-9
    convex = np.clip(curv / k, 0, 1)
    concave = np.clip(-curv / k, 0, 1)
    _, _, cid = worley3(co + np.stack([fbm3(co, s * 0.35, seed + k2 * 7, 2) for k2 in range(3)], -1) * (s * 0.04), plate_cell, seed + 101)
    _write_colors(me, convex, concave, cid)

    rock.data.materials.clear()
    if material is not None:
        rock.data.materials.append(material)
    sb.world_box_uv(rock, 4.0)
    return rock


def _write_colors(me, r, g, b):
    """Per-vertex values written to the face-corner 'Col' attribute (what the FBX exporter and
    sb_blender.bake_vertex_masks use)."""
    if "Col" in me.color_attributes:
        me.color_attributes.remove(me.color_attributes["Col"])
    attr = me.color_attributes.new("Col", "FLOAT_COLOR", "CORNER")
    loop_vi = np.empty(len(me.loops), dtype=np.int64)
    me.loops.foreach_get("vertex_index", loop_vi)
    col = np.stack([r, g, b, np.ones_like(r)], -1).astype(np.float32)[loop_vi]
    attr.data.foreach_set("color", col.ravel())
    me.color_attributes.active_color = attr
