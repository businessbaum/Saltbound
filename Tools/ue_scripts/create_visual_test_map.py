"""Milestone 3 visual benchmark: /Game/Saltbound/Maps/L_VisualTest (script-owned, recreated on each run).

A small cove: coastal rocks, a two-section pier, the boat deck section moored alongside (riding the waves),
lanterns, a distant landmark in the fog, the ocean with a calm zone, and a lighting rig (Day / Evening).
"""
import math
import os
import sys

import unreal

sys.path.insert(0, os.path.dirname(__file__))
import sb_ue  # noqa: E402

MAP = "/Game/Saltbound/Maps/L_VisualTest"
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
    """rot = (pitch, yaw, roll) in degrees."""
    a = eas.spawn_actor_from_class(cls, unreal.Vector(*loc), unreal.Rotator(roll=rot[2], pitch=rot[0], yaw=rot[1]))
    if label:
        a.set_actor_label(label)
    return a


def mesh(label, path, loc, rot=(0, 0, 0), scale=1.0, mat=None):
    a = spawn(unreal.StaticMeshActor, loc, rot, label)
    a.static_mesh_component.set_static_mesh(unreal.load_asset(path))
    s = scale if isinstance(scale, (tuple, list)) else (scale, scale, scale)
    a.set_actor_scale3d(unreal.Vector(*s))
    if mat:
        a.static_mesh_component.set_material(0, unreal.load_asset(mat))
    return a


# --- sky & lighting ---------------------------------------------------------------------------------
sun = spawn(unreal.DirectionalLight, (0, 0, 2000), (-38, -60, 0), "Sun")
sc = sun.get_component_by_class(unreal.DirectionalLightComponent)
sc.set_mobility(unreal.ComponentMobility.MOVABLE)
sc.set_editor_property("atmosphere_sun_light", True)
sc.set_editor_property("intensity", 90000.0)
spawn(unreal.SkyAtmosphere, label="SkyAtmosphere")
sky = spawn(unreal.SkyLight, (0, 0, 800), label="SkyLight")
skc = sky.get_component_by_class(unreal.SkyLightComponent)
skc.set_mobility(unreal.ComponentMobility.MOVABLE)
skc.set_editor_property("real_time_capture", True)
spawn(unreal.ExponentialHeightFog, (0, 0, -200), label="HeightFog")
spawn(unreal.VolumetricCloud, label="Clouds")
pp = spawn(unreal.PostProcessVolume, label="GlobalPostProcess")
pp.set_editor_property("unbound", True)
rig = spawn(unreal.SaltLightingRig, (0, 0, 300), label="LightingRig")

# --- ocean -------------------------------------------------------------------------------------------
ocean = spawn(unreal.SaltOcean, (0, 0, 0), label="Ocean")
zone = unreal.SaltCalmZone()
zone.set_editor_property("center", unreal.Vector2D(500, 300))
zone.set_editor_property("inner_radius", 2500.0)
zone.set_editor_property("outer_radius", 15000.0)
zone.set_editor_property("min_factor", 0.5)
ocean.set_editor_property("calm_zones", [zone])
ocean.set_editor_property("amplitude_scale", 1.5)

# --- seabed --------------------------------------------------------------------------------------------
CUBE = "/Engine/BasicShapes/Cube"
mesh("Seabed", CUBE, (0, 0, -1800), scale=(1200, 1200, 1), mat="/Game/Saltbound/Materials/Instances/MI_Sand")
mesh("ShoreSlope", CUBE, (-200, 0, -700), (-10.0, 0, 0), scale=(40, 50, 1), mat="/Game/Saltbound/Materials/Instances/MI_Sand")

# --- coast -------------------------------------------------------------------------------------------
R = "/Game/Saltbound/Environment/Rocks/"
rocks = [
    ("Rock_A1", "SM_Rock_Coast_A", (-750, 150, -20), 30, 1.25),
    ("Rock_A2", "SM_Rock_Coast_A", (-550, -750, -40), 160, 0.95),
    ("Rock_A3", "SM_Rock_Coast_A", (-1100, -250, 60), 250, 1.5),
    ("Rock_B1", "SM_Rock_Coast_B", (-260, 520, -50), 70, 1.0),
    ("Rock_B2", "SM_Rock_Coast_B", (-220, -380, -70), 10, 1.1),
    ("Rock_B3", "SM_Rock_Coast_B", (-600, 850, -30), 200, 1.3),
    ("Rock_C1", "SM_Rock_Coast_C", (-90, 230, -60), 15, 1.0),
    ("Rock_C2", "SM_Rock_Coast_C", (-40, -190, -70), 120, 0.9),
    ("Rock_C3", "SM_Rock_Coast_C", (160, 640, -80), 300, 1.2),
    ("Rock_C4", "SM_Rock_Coast_C", (-380, 820, -40), 60, 1.4),
    ("Rock_C5", "SM_Rock_Coast_C", (320, -660, -90), 210, 1.1),
    ("Headland_A", "SM_Rock_Coast_A", (900, 3900, -150), 80, 2.6),
    ("Headland_B", "SM_Rock_Coast_B", (2000, 3300, -100), 140, 2.2),
    ("Headland_C", "SM_Rock_Coast_A", (3400, 4300, -250), 20, 2.0),
]
for label, m, loc, yaw, s in rocks:
    mesh(label, R + m, loc, (0, yaw, 0), s)
mesh("Landmark_Spire", R + "SM_Landmark_Spire", (42000, 15000, -600), (0, 35, 0), 2.0)

# --- pier --------------------------------------------------------------------------------------------
H = "/Game/Saltbound/Environment/Harbor/"
mesh("Dock_1", H + "SM_DockSection", (200, 0, 0))
mesh("Dock_2", H + "SM_DockSection", (1000, 0, 0), (0, 0, 0))

# --- moored boat section (rides the waves; walkable) ----------------------------------------------
boat = spawn(unreal.SaltDevFloater, (900, 330, 75), (0, 0, 0), "BoatSection")
boat.mesh.set_static_mesh(unreal.load_asset("/Game/Saltbound/Dev/VisualTest/SM_TestDeckSection"))


def attach(actor, parent):
    actor.attach_to_actor(parent, "", unreal.AttachmentRule.KEEP_WORLD, unreal.AttachmentRule.KEEP_WORLD,
                          unreal.AttachmentRule.KEEP_WORLD, False)


def local_to_world(parent_loc, local):
    return (parent_loc[0] + local[0], parent_loc[1] + local[1], parent_loc[2] + local[2])


bl = (900, 330, 75)
lantern_boat = spawn(unreal.SaltLantern, local_to_world(bl, (-42, 42, 103)), (0, 20, 0), "Lantern_Boat")
attach(lantern_boat, boat)
crate = mesh("Crate_Boat", "/Game/Saltbound/Props/SM_Crate_Small", local_to_world(bl, (60, 95, 0)), (0, 8, 0))
crate.static_mesh_component.set_mobility(unreal.ComponentMobility.MOVABLE)
attach(crate, boat)
spawn(unreal.SaltLantern, (1310, -70, 110), (0, -30, 0), "Lantern_Dock")
mesh("Crate_Dock", "/Game/Saltbound/Props/SM_Crate_Small", (1180, -60, 110), (0, 25, 0))
mesh("Crate_Dock2", "/Game/Saltbound/Props/SM_Crate_Small", (1185, -62, 150.5), (0, 12, 0), 0.85)

spawn(unreal.PlayerStart, (380, -10, 110 + 92), (0, 0, 0), "PlayerStart")
rig.apply_preset("Day")
les.save_current_level()
print("SB_MAP_OK", MAP)
