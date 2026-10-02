"""Creates /Game/Saltbound/Dev/L_OceanTest: ocean rendering and CPU/GPU wave consistency test (script-owned)."""
import os
import sys

import unreal

sys.path.insert(0, os.path.dirname(__file__))
import sb_ue  # noqa: E402

MAP = "/Game/Saltbound/Dev/L_OceanTest"
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

if sb_ue.eal.does_asset_exist(MAP):
    les.load_level(MAP)
    for actor in eas.get_all_level_actors():
        if not isinstance(actor, unreal.WorldSettings) and actor.get_class() != unreal.Brush.static_class():
            eas.destroy_actor(actor)
else:
    les.new_level(MAP)


def spawn(cls, loc=(0, 0, 0), rot=(0, 0, 0), label=None):
    a = eas.spawn_actor_from_class(cls, unreal.Vector(*loc), unreal.Rotator(*rot))
    if label:
        a.set_actor_label(label)
    return a


def block(label, loc, size, rot=(0, 0, 0), mat=None):
    a = spawn(unreal.StaticMeshActor, loc, rot, label)
    a.static_mesh_component.set_static_mesh(unreal.load_asset("/Engine/BasicShapes/Cube"))
    a.set_actor_scale3d(unreal.Vector(size[0] / 100.0, size[1] / 100.0, size[2] / 100.0))
    if mat:
        a.static_mesh_component.set_material(0, unreal.load_asset(mat))
    return a


sun = spawn(unreal.DirectionalLight, (0, 0, 1000), (0, -32, -50), "Sun")
sc = sun.get_component_by_class(unreal.DirectionalLightComponent)
sc.set_editor_property("intensity", 10.0)
sc.set_editor_property("atmosphere_sun_light", True)
sc.set_mobility(unreal.ComponentMobility.MOVABLE)
spawn(unreal.SkyAtmosphere, label="SkyAtmosphere")
sky = spawn(unreal.SkyLight, (0, 0, 500), label="SkyLight")
skc = sky.get_component_by_class(unreal.SkyLightComponent)
skc.set_editor_property("real_time_capture", True)
skc.set_mobility(unreal.ComponentMobility.MOVABLE)
fog = spawn(unreal.ExponentialHeightFog, (0, 0, 0), label="HeightFog")
fog.get_component_by_class(unreal.ExponentialHeightFogComponent).set_editor_property("fog_density", 0.008)
spawn(unreal.VolumetricCloud, label="Clouds")
spawn(unreal.PostProcessVolume, label="GlobalPostProcess").set_editor_property("unbound", True)

ocean = spawn(unreal.SaltOcean, (0, 0, 0), label="Ocean")
ocean.set_editor_property("parameter_collection", unreal.load_asset("/Game/Saltbound/Water/MPC_Ocean"))
ocean.set_editor_property("material", unreal.load_asset("/Game/Saltbound/Water/MI_Ocean"))
ocean.set_editor_property("tile_mesh", unreal.load_asset("/Game/Saltbound/Water/SM_OceanTile32"))
ocean.set_editor_property("quarter_tile_mesh", unreal.load_asset("/Game/Saltbound/Water/SM_OceanTile16"))

grid_mat = "/Game/LevelPrototyping/Materials/MI_PrototypeGrid_Gray"
# Seabed sloping up toward +X so depth colour and shore foam can be judged; a rock-like block at the shore.
block("Seabed", (0, 0, -900), (40000, 40000, 100), mat=grid_mat)
block("ShelfSlope", (4500, 0, -420), (8000, 6000, 100), rot=(0, 6.5, 0), mat=grid_mat)
block("ShoreBlock", (3200, -800, 0), (900, 600, 700), rot=(0, 0, 25), mat=grid_mat)
block("Post_1m", (900, 400, 0), (40, 40, 300))

# CPU-surface probes: 5 x 5 grid, 6 m apart.
for ix in range(-2, 3):
    for iy in range(-2, 3):
        spawn(unreal.SaltDevWaterProbe, (ix * 600, iy * 600, 0), label="Probe_%d_%d" % (ix, iy))

spawn(unreal.PlayerStart, (-1500, 0, 300), label="PlayerStart")
les.save_current_level()
print("SB_MAP_OK", MAP)
