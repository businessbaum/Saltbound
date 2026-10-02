"""Creates /Game/Saltbound/Maps/L_DevTest: neutral daylight test map for pipeline and movement tests.

Re-running recreates the map from scratch (the map is fully script-owned).
"""
import os
import sys

import unreal

sys.path.insert(0, os.path.dirname(__file__))
import sb_ue  # noqa: E402

MAP = "/Game/Saltbound/Maps/L_DevTest"
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

if sb_ue.eal.does_asset_exist(MAP):
    les.load_level(MAP)
    for actor in eas.get_all_level_actors():
        # Keep only the world settings and the level's default brush (exact class; volumes derive from Brush).
        if not isinstance(actor, unreal.WorldSettings) and actor.get_class() != unreal.Brush.static_class():
            eas.destroy_actor(actor)
else:
    les.new_level(MAP)


def spawn(cls, loc=(0, 0, 0), rot=(0, 0, 0), label=None):
    a = eas.spawn_actor_from_class(cls, unreal.Vector(*loc), unreal.Rotator(*rot))
    if label:
        a.set_actor_label(label)
    return a


sun = spawn(unreal.DirectionalLight, (0, 0, 1000), (0, -40, -35), "Sun")
sun_comp = sun.get_component_by_class(unreal.DirectionalLightComponent)
sun_comp.set_editor_property("intensity", 10.0)
sun_comp.set_editor_property("atmosphere_sun_light", True)
sun_comp.set_mobility(unreal.ComponentMobility.MOVABLE)

spawn(unreal.SkyAtmosphere, (0, 0, 0), label="SkyAtmosphere")
sky = spawn(unreal.SkyLight, (0, 0, 500), label="SkyLight")
sky_comp = sky.get_component_by_class(unreal.SkyLightComponent)
sky_comp.set_editor_property("real_time_capture", True)
sky_comp.set_mobility(unreal.ComponentMobility.MOVABLE)
spawn(unreal.ExponentialHeightFog, (0, 0, 0), label="HeightFog")
spawn(unreal.VolumetricCloud, (0, 0, 0), label="Clouds")
spawn(unreal.PostProcessVolume, (0, 0, 0), label="GlobalPostProcess").set_editor_property("unbound", True)

floor = spawn(unreal.StaticMeshActor, (0, 0, 0), label="Floor")
floor.static_mesh_component.set_static_mesh(unreal.load_asset("/Engine/BasicShapes/Plane"))
floor.set_actor_scale3d(unreal.Vector(100, 100, 1))
floor.static_mesh_component.set_material(0, unreal.load_asset("/Game/LevelPrototyping/Materials/MI_PrototypeGrid_Gray"))

crate = spawn(unreal.StaticMeshActor, (300, 0, 0), label="PipelineTest_Crate")
crate.static_mesh_component.set_static_mesh(unreal.load_asset("/Game/Saltbound/Props/SM_Crate_Small"))

# Axis reference: 1 m engine cube at +X 2 m, small cube at +Y 1 m (right side when facing +X)
ref_x = spawn(unreal.StaticMeshActor, (300 + 200, 0, 50), label="AxisRef_PlusX_1m")
ref_x.static_mesh_component.set_static_mesh(unreal.load_asset("/Engine/BasicShapes/Cube"))
ref_y = spawn(unreal.StaticMeshActor, (300, 100, 10), label="AxisRef_PlusY")
ref_y.static_mesh_component.set_static_mesh(unreal.load_asset("/Engine/BasicShapes/Cube"))
ref_y.set_actor_scale3d(unreal.Vector(0.2, 0.2, 0.2))

spawn(unreal.PlayerStart, (0, 0, 100), label="PlayerStart")

# Milestone 2: kinematic moving deck with a helm (player/station/moving-base tests).
if hasattr(unreal, "SaltDevMovingPlatform"):
    platform = spawn(unreal.SaltDevMovingPlatform, (-1200, 600, 150), (0, 0, 0), "DevMovingPlatform")
    grid = unreal.load_asset("/Game/LevelPrototyping/Materials/MI_PrototypeGrid_Gray_02")
    accent = unreal.load_asset("/Game/LevelPrototyping/Materials/MI_PrototypeGrid_TopDark")
    for comp in platform.get_components_by_class(unreal.StaticMeshComponent):
        comp.set_material(0, grid if comp.get_name() == "Deck" else accent)
    # Boarding ramp from the floor to the open stern.
    ramp = spawn(unreal.StaticMeshActor, (-1780, 600, 0), (0, 0, 0), "BoardingRamp")
    ramp.static_mesh_component.set_static_mesh(unreal.load_asset("/Game/LevelPrototyping/Meshes/SM_Ramp"))

les.save_current_level()
print("SB_MAP_OK", MAP)
