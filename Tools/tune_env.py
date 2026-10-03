"""Live tuning of the environment rig in the open editor level (iteration helper; final values belong in
ASaltLightingRig's constructor / setup scripts). Runs inside the editor via ue_exec.py.

  python Tools/ue_exec.py Tools/tune_env.py region.ShoresOfPlenty.water_absorption=1.3,0.2,0.16 \
         sea.Rough.wave_height=1.3 tod.Day.fog_density=0.01 mi.RippleStrength=0.16 apply=Region:ShoresOfPlenty
Keys: region.<Name>.<field>, sea.<Name>.<field>, weather.<Name>.<field>, tod.<Name>.<field>, mi.<scalar>,
      apply=Region:<Name> | Weather:<Name> | SeaState:<Name> | Tod:<Name>
"""
import sys

import unreal

ARRAYS = {"region": "regions", "sea": "sea_states", "weather": "weathers", "tod": "presets"}


def value(v):
    vals = [float(x) for x in v.split(",")]
    if len(vals) == 1:
        return vals[0]
    return unreal.LinearColor(*(vals + [1.0] * (4 - len(vals))))


def main():
    eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    rig = [a for a in eas.get_all_level_actors() if a.get_class().get_name() == "SaltLightingRig"][0]
    mel = unreal.MaterialEditingLibrary
    mi = unreal.load_asset("/Game/Saltbound/Water/MI_Ocean")
    applies = []
    for arg in sys.argv[1:]:
        key, v = arg.split("=", 1)
        if key == "apply":
            applies.append(v)
            continue
        parts = key.split(".")
        if parts[0] == "mi":
            mel.set_material_instance_scalar_parameter_value(mi, parts[1], float(v))
            continue
        arr_name = ARRAYS[parts[0]]
        arr = list(rig.get_editor_property(arr_name))
        item = [x for x in arr if str(x.get_editor_property("name")) == parts[1]][0]
        item.set_editor_property(parts[2], value(v))
        rig.set_editor_property(arr_name, arr)
    mel.update_material_instance(mi)
    # Re-select the current layers so the edited presets take effect.
    rig.set_region(rig.get_editor_property("current_region") or "ShoresOfPlenty", 0.0)
    rig.set_weather(rig.get_editor_property("current_weather") or "Clear", 0.0)
    rig.set_sea_state(rig.get_editor_property("current_sea_state") or "Rough", 0.0)
    for a in applies:
        kind, name = a.split(":")
        {"Region": lambda n: rig.set_region(n, 0.0), "Weather": lambda n: rig.set_weather(n, 0.0),
         "SeaState": lambda n: rig.set_sea_state(n, 0.0), "Tod": lambda n: rig.apply_preset(n)}[kind](name)
    print("SB_TUNED", rig.describe_environment() if hasattr(rig, "describe_environment") else "")


main()
