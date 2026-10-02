"""Import (or reimport) Blender exports into Unreal according to Art/asset_manifest.json.

Runs inside the Unreal Editor:  python Tools/ue_exec.py Tools/ue_scripts/import_assets.py [AssetName ...]
With no names, every manifest entry is imported.

Per entry:
  fbx       path relative to Art/Export
  dest      content folder, e.g. /Game/Saltbound/Props
  nanite    build Nanite data (default false)
  collision "ucx" (use UCX_ hulls from the file), "complex" (use complex as simple), or "none"
  bounds_extension  optional [x, y, z] added to the mesh bounds on both sides (local units), for meshes
                    displaced in the material (e.g. ocean tiles)
Material slots are matched by name to existing material instances anywhere under /Game/Saltbound.
Unreal never creates materials from the FBX; missing ones fall back to MI_Placeholder.
"""
import json
import os
import sys

import unreal

REPO_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
MANIFEST = os.path.join(REPO_ROOT, "Art", "asset_manifest.json")
EXPORT_ROOT = os.path.join(REPO_ROOT, "Art", "Export")
MATERIAL_ROOT = "/Game/Saltbound"
PLACEHOLDER = "/Game/Saltbound/Materials/Instances/MI_Placeholder"


def make_pipeline(entry):
    pipe = unreal.new_object(unreal.InterchangeGenericAssetsPipeline)
    common = pipe.get_editor_property("common_meshes_properties")
    common.set_editor_property("force_all_mesh_as_type", unreal.InterchangeForceMeshType.IFMT_STATIC_MESH)
    common.set_editor_property("recompute_normals", False)
    common.set_editor_property("recompute_tangents", True)
    common.set_editor_property("use_mikk_t_space", True)
    common.set_editor_property("vertex_color_import_option", unreal.InterchangeVertexColorImportOption.IVCIO_REPLACE)
    mesh = pipe.get_editor_property("mesh_pipeline")
    mesh.set_editor_property("import_static_meshes", True)
    mesh.set_editor_property("import_skeletal_meshes", False)
    mesh.set_editor_property("combine_static_meshes_behavior", unreal.InterchangeCombineStaticMeshesBehavior.ALL)
    mesh.set_editor_property("build_nanite", bool(entry.get("nanite", False)))
    mesh.set_editor_property("generate_lightmap_u_vs", False)
    use_ucx = entry.get("collision", "ucx") == "ucx"
    # 'collision' is bCollision (import custom collision); 'fallback_collision_type' applies when none is present.
    mesh.set_editor_property("collision", use_ucx)
    mesh.set_editor_property("import_collision_according_to_mesh_name", use_ucx)
    mesh.set_editor_property("one_convex_hull_per_ucx", True)
    mesh.set_editor_property("fallback_collision_type", unreal.InterchangeMeshCollision.NONE)
    mat = pipe.get_editor_property("material_pipeline")
    mat.set_editor_property("material_import", unreal.InterchangeMaterialImportOption.IMPORT_AS_MATERIAL_INSTANCES)
    mat.set_editor_property("import_materials", False)
    anim = pipe.get_editor_property("animation_pipeline")
    anim.set_editor_property("import_animations", False)
    return pipe


def find_material(slot_name):
    reg = unreal.AssetRegistryHelpers.get_asset_registry()
    flt = unreal.ARFilter(package_paths=[MATERIAL_ROOT], recursive_paths=True)
    for data in reg.get_assets(flt):
        if str(data.asset_name) == slot_name:
            return data.get_asset()
    return unreal.load_asset(PLACEHOLDER) if unreal.EditorAssetLibrary.does_asset_exist(PLACEHOLDER) else None


def import_entry(name, entry):
    fbx = os.path.join(EXPORT_ROOT, entry["fbx"])
    if not os.path.exists(fbx):
        unreal.log_error("SB_IMPORT missing export for %s: %s" % (name, fbx))
        return False
    dest = entry["dest"]
    pipe = make_pipeline(entry)
    params = unreal.ImportAssetParameters()
    params.set_editor_property("is_automated", True)
    params.set_editor_property("replace_existing", True)
    params.set_editor_property("override_pipelines", [unreal.SoftObjectPath(pipe.get_path_name())])
    source = unreal.InterchangeManager.create_source_data(fbx)
    mgr = unreal.InterchangeManager.get_interchange_manager_scripted()
    mgr.import_asset(dest, source, params)

    asset_path = "%s/%s" % (dest, name)
    sm = unreal.load_asset(asset_path)
    if not isinstance(sm, unreal.StaticMesh):
        unreal.log_error("SB_IMPORT %s did not produce a static mesh at %s" % (name, asset_path))
        return False

    # Materials by slot name
    mats = sm.get_editor_property("static_materials")
    for i, slot in enumerate(mats):
        slot_name = str(slot.get_editor_property("material_slot_name"))
        mi = find_material(slot_name)
        if mi:
            sm.set_material(i, mi)
    # Collision complexity
    body = sm.get_editor_property("body_setup")
    if body:
        mode = entry.get("collision", "ucx")
        flag = unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE if mode == "complex" else unreal.CollisionTraceFlag.CTF_USE_DEFAULT
        body.set_editor_property("collision_trace_flag", flag)
    if "bounds_extension" in entry:
        ext = unreal.Vector(*entry["bounds_extension"])
        sm.set_editor_property("positive_bounds_extension", ext)
        sm.set_editor_property("negative_bounds_extension", ext)
    if entry.get("collision") == "none" and body:
        body.set_editor_property("collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_SIMPLE_AS_COMPLEX)
    unreal.EditorAssetLibrary.save_asset(asset_path, only_if_is_dirty=False)

    bounds = sm.get_bounding_box()
    size = bounds.max - bounds.min
    agg = body.get_editor_property("agg_geom") if body else None
    n_convex = len(agg.get_editor_property("convex_elems")) if agg else 0
    n_box = len(agg.get_editor_property("box_elems")) if agg else 0
    slots = [str(m.get_editor_property("material_slot_name")) + "->" +
             (m.get_editor_property("material_interface").get_name() if m.get_editor_property("material_interface") else "None")
             for m in sm.get_editor_property("static_materials")]
    print("SB_IMPORTED %s size_cm=(%.1f, %.1f, %.1f) min=(%.1f, %.1f, %.1f) convex=%d box=%d tris=%d slots=%s" % (
        name, size.x, size.y, size.z, bounds.min.x, bounds.min.y, bounds.min.z, n_convex, n_box,
        sm.get_num_triangles(0), slots))
    return True


def main():
    with open(MANIFEST, encoding="utf-8") as f:
        manifest = {k: v for k, v in json.load(f).items() if not k.startswith("_")}
    names = sys.argv[1:] or list(manifest)
    ok = True
    for n in names:
        if n not in manifest:
            unreal.log_error("SB_IMPORT unknown asset %s" % n)
            ok = False
            continue
        ok = import_entry(n, manifest[n]) and ok
    print("SB_IMPORT_DONE ok=%s" % ok)


if __name__ == "__main__":
    main()
