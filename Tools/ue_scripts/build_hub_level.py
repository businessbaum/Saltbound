"""The hub ship's own levels (D63, D70; phase 3 of Docs/Design/HubShip/HubShip_Agent_Prompt.md): /Game/Saltbound/Maps/L_Hub, her
outside, and /Game/Saltbound/Maps/L_HubInside, everything inside her.

Script-owned, recreated on each run. The ship alone, her pivot (the data's x 80, the waterline) at the levels' origin and her
bow to +X (hub_place.py, shared with the art review map, which holds both in one). L_Hub: the hull, the castles, the rig and
its ropes, the doors, the shell's collision. L_HubInside: every floor and room as hidden collision under the art, the harbour
and its bridges, the gates, the E points, the hoist, the quay ladders, the moorings, the lanterns' lights, the exposure volumes
and the interiors' fog. No sea, sky or light: ASaltHubShip streams L_Hub into the game's world at a berth
(ULevelStreamingDynamic) and L_HubInside there too while the player is near her (D70).

    python Tools/ue_exec.py Tools/ue_scripts/build_hub_level.py
"""
import importlib
import os
import sys

import unreal

sys.path.insert(0, os.path.dirname(__file__))
import hub_place  # noqa: E402
importlib.reload(hub_place)          # the editor keeps modules (and hub_place reloads the data)

MAPS = (("/Game/Saltbound/Maps/L_Hub", hub_place.outside), ("/Game/Saltbound/Maps/L_HubInside", hub_place.inside))

les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
eal = unreal.EditorAssetLibrary
for path, fill in MAPS:
    if eal.does_asset_exist(path):
        les.load_level(path)
        for actor in eas.get_all_level_actors():
            if not isinstance(actor, unreal.WorldSettings) and actor.get_class() != unreal.Brush.static_class():
                eas.destroy_actor(actor)
    else:
        les.new_level(path)
    p = hub_place.Placer(eas, (0.0, 0.0, 0.0), 0.0, "HubShip")
    counts = fill(p)
    les.save_current_level()
    print("SB_HUB_LEVEL_OK", path, counts)
