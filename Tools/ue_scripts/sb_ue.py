"""Shared helpers for Saltbound editor scripts (run inside Unreal via Tools/ue_exec.py)."""
import unreal

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
mel = unreal.MaterialEditingLibrary
eal = unreal.EditorAssetLibrary


def ensure_folder(path):
    if not eal.does_directory_exist(path):
        eal.make_directory(path)


def recreate_asset(name, folder, cls, factory):
    """Delete (if present) and create an asset. Use only for fully script-owned assets."""
    path = "%s/%s" % (folder, name)
    if eal.does_asset_exist(path):
        eal.delete_asset(path)
    ensure_folder(folder)
    return asset_tools.create_asset(name, folder, cls, factory)


def reset_material(name, folder):
    """Script-owned master material: reuse the asset if it exists and clear its graph (keeps references
    from instances and placed meshes intact; deleting a referenced material fails while it is loaded)."""
    path = "%s/%s" % (folder, name)
    if eal.does_asset_exist(path):
        m = unreal.load_asset(path)
        # delete_all_material_expressions removes only part of the graph per call (5.8); repeat until empty.
        for _ in range(16):
            if mel.get_num_material_expressions(m) == 0:
                break
            mel.delete_all_material_expressions(m)
        if mel.get_num_material_expressions(m) != 0:
            raise RuntimeError("Could not clear material graph of " + path)
        return m
    ensure_folder(folder)
    return asset_tools.create_asset(name, folder, unreal.Material, unreal.MaterialFactoryNew())


def get_or_create_asset(name, folder, cls, factory):
    path = "%s/%s" % (folder, name)
    if eal.does_asset_exist(path):
        return unreal.load_asset(path)
    ensure_folder(folder)
    return asset_tools.create_asset(name, folder, cls, factory)


def expr(material, cls, x, y, **props):
    e = mel.create_material_expression(material, cls, x, y)
    for k, v in props.items():
        e.set_editor_property(k, v)
    return e


def scalar_param(material, name, default, x, y, group="", priority=0):
    return expr(material, unreal.MaterialExpressionScalarParameter, x, y,
                parameter_name=name, default_value=default, group=group, sort_priority=priority)


def vector_param(material, name, rgba, x, y, group="", priority=0):
    return expr(material, unreal.MaterialExpressionVectorParameter, x, y,
                parameter_name=name, default_value=unreal.LinearColor(*rgba), group=group, sort_priority=priority)


def static_switch(material, name, default, x, y, group=""):
    return expr(material, unreal.MaterialExpressionStaticSwitchParameter, x, y,
                parameter_name=name, default_value=default, group=group)


def connect(a, a_out, b, b_in):
    if not mel.connect_material_expressions(a, a_out, b, b_in):
        raise RuntimeError("Failed to connect %s.%s -> %s.%s" % (a.get_name(), a_out, b.get_name(), b_in))


def to_prop(e, out, prop):
    if not mel.connect_material_property(e, out, prop):
        raise RuntimeError("Failed to connect %s.%s -> %s" % (e.get_name(), out, prop))


def make_mi(name, folder, parent, scalars=None, vectors=None, textures=None, switches=None):
    mi = get_or_create_asset(name, folder, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mel.set_material_instance_parent(mi, parent)
    for k, v in (scalars or {}).items():
        mel.set_material_instance_scalar_parameter_value(mi, k, v)
    for k, v in (vectors or {}).items():
        mel.set_material_instance_vector_parameter_value(mi, k, unreal.LinearColor(*v))
    for k, v in (textures or {}).items():
        mel.set_material_instance_texture_parameter_value(mi, k, unreal.load_asset(v) if isinstance(v, str) else v)
    for k, v in (switches or {}).items():
        mel.set_material_instance_static_switch_parameter_value(mi, k, v)
    mel.update_material_instance(mi)
    eal.save_loaded_asset(mi)
    return mi
