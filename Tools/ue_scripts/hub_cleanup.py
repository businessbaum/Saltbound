"""D64 tidy-up in the editor, after the hub's maps are rebuilt: the visual test map's hub berth gets the current calm zones (the
water hall's moved: hub_place.calm_zones) and sheltered water (D70: the harbour's three boxes), its ship her inside level (D70:
L_HubInside), and the hub assets the builds no longer make are deleted when nothing uses them any more (the D61 blockout's old
floors, the ancient band, the sea gate, D70: the Keelgate).

Run (the editor open, no PIE):  python Tools/ue_exec.py Tools/ue_scripts/hub_cleanup.py
"""
import importlib
import os
import sys

import unreal

sys.path.insert(0, os.path.dirname(__file__))
import hub_place  # noqa: E402
importlib.reload(hub_place)

STALE = (["/Game/Saltbound/Hub/Blockout/SM_HubBlk_" + n for n in ("Basin", "Quay", "Lower", "Middle", "Upper", "Main")]
         + ["/Game/Saltbound/Hub/Art/SM_Hub_" + n for n in ("Ancient", "SeaGate", "SeaGateFrame", "Galleries", "Keelgate")])     # D69: no quarter galleries; D70: no Keelgate
VISUAL_TEST = "/Game/Saltbound/Maps/L_VisualTest"

les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
eal = unreal.EditorAssetLibrary

les.load_level(VISUAL_TEST)
berths = 0
for a in eas.get_all_level_actors():
    if isinstance(a, unreal.SaltHubBerth):
        a.set_editor_property("calm_zones", hub_place.calm_zones(local=True))
        a.set_editor_property("shelter", hub_place.shelter_box(local=True))
        if "shelters" in [str(n) for n in dir(a)]:
            a.set_editor_property("shelters", hub_place.shelter_boxes(local=True))
        berths += 1
    if isinstance(a, unreal.SaltHubShip):
        hub_place.set_inside_level(a)
les.save_current_level()

deleted = 0
for path in STALE:
    if not eal.does_asset_exist(path):
        continue
    refs = [r for r in eal.find_package_referencers_for_asset(path, False) if r != path.rsplit(".", 1)[0]]
    if refs:
        print("SB_HUB_CLEANUP keep", path, "used by", refs)
        continue
    if eal.delete_asset(path):
        deleted += 1
        print("SB_HUB_CLEANUP deleted", path)
print("SB_HUB_CLEANUP_OK berths", berths, "deleted", deleted)
