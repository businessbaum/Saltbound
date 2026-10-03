"""Controlled ocean captures for diagnostics and before/after comparisons (editor must be running).

Sets the ocean clock, freeze state, temporary console variables and temporary MI_Ocean parameters, captures
one shot of the visual-test set (see capture_shots.py), then restores every changed setting.

  python Tools/ocean_diag.py <label> --shot 10_open_sea_sun [--time 100] [--freeze 1] [--settle 2.5]
         [--cvar r.AntiAliasingMethod=0 ...] [--mi RippleStrength=0 ...] [--preset Day] [--out DIR]

--freeze 1  waves, ripples and foam stand still (TSR converges: no temporal effects)
--freeze 0  the sea runs from --time for --settle seconds before the capture (temporal effects visible)
"""
import argparse
import os
import sys

sys.path.insert(0, os.path.dirname(__file__))
import capture_shots  # noqa: E402
import ue_exec  # noqa: E402
import ue_mcp  # noqa: E402

MI = "/Game/Saltbound/Water/MI_Ocean"


def run(code):
    return ue_exec.run(code)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("label")
    ap.add_argument("--shot", required=True)
    ap.add_argument("--time", type=float, default=100.0)
    ap.add_argument("--freeze", type=int, default=1)
    ap.add_argument("--settle", type=float, default=2.5)
    ap.add_argument("--cvar", nargs="*", default=[])
    ap.add_argument("--mi", nargs="*", default=[])
    ap.add_argument("--ocean", nargs="*", default=[], help="ocean actor properties, e.g. ripple_scroll=0,0,0,0")
    ap.add_argument("--mitex", nargs="*", default=[], help="MI_Ocean texture parameters, e.g. RippleTex=/Game/.../T_X")
    ap.add_argument("--preset", default="Day")
    ap.add_argument("--out", default=os.path.join(os.environ.get("TEMP", "."), "saltbound_diag"))
    ns = ap.parse_args()
    os.makedirs(ns.out, exist_ok=True)
    shot = {n: (loc, rot) for n, loc, rot in capture_shots.SHOTS["visualtest"]}[ns.shot]
    cvars = dict(c.split("=", 1) for c in ns.cvar)
    mis = {k: float(v) for k, v in (m.split("=", 1) for m in ns.mi)}
    ocean = {k: [float(x) for x in v.split(",")] for k, v in (o.split("=", 1) for o in ns.ocean)}
    mitex = dict(t.split("=", 1) for t in ns.mitex)
    run("import unreal\n"
        "unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_set_game_view(True)\n")
    capture_shots.apply_preset(ns.preset)
    # Remember and apply cvars / MI parameters.
    setup = (
        "import unreal, json\n"
        "mel = unreal.MaterialEditingLibrary\n"
        "mi = unreal.load_asset(%r)\n"
        "old = {'cvar': {}, 'mi': {}}\n"
        "for k, v in %r.items():\n"
        "    old['cvar'][k] = unreal.SystemLibrary.get_console_variable_float_value(k)\n"
        "    unreal.SystemLibrary.execute_console_command(None, k + ' ' + v)\n"
        "for k, v in %r.items():\n"
        "    old['mi'][k] = mel.get_material_instance_scalar_parameter_value(mi, k)\n"
        "    mel.set_material_instance_scalar_parameter_value(mi, k, v)\n"
        "old['mitex'] = {}\n"
        "for k, v in %r.items():\n"
        "    cur = mel.get_material_instance_texture_parameter_value(mi, k)\n"
        "    old['mitex'][k] = cur.get_path_name() if cur else ''\n"
        "    mel.set_material_instance_texture_parameter_value(mi, k, unreal.load_asset(v))\n"
        "mel.update_material_instance(mi)\n"
        "eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)\n"
        "for a in eas.get_all_level_actors():\n"
        "    if a.get_class().get_name() == 'SaltOcean':\n"
        "        old['ocean'] = {}\n"
        "        for k, v in %r.items():\n"
        "            cur = a.get_editor_property(k)\n"
        "            old['ocean'][k] = [cur.x, cur.y, cur.z, cur.w] if hasattr(cur, 'w') else [float(cur)]\n"
        "            a.set_editor_property(k, unreal.Vector4(*v) if len(v) == 4 else v[0])\n"
        "        a.set_editor_property('freeze_time', %s)\n"
        "        a.set_ocean_time(%f)\n"
        "print('SB_DIAG_OLD', json.dumps(old))\n" % (MI, cvars, mis, mitex, ocean, "True" if ns.freeze else "False", ns.time))
    out = run(setup)
    text = "".join(o["output"] for o in out.get("output", []))
    if not out.get("success"):
        raise SystemExit("setup failed: %s %s" % (text, out.get("result")))
    old_line = [l for l in text.splitlines() if "SB_DIAG_OLD" in l]
    path = os.path.join(ns.out, "%s.png" % ns.label)
    try:
        ue_mcp.capture(path, (*shot[0], *shot[1]), settle=ns.settle)
        print("SB_DIAG_SHOT", path)
    finally:
        restore = (
            "import unreal, json\n"
            "mel = unreal.MaterialEditingLibrary\n"
            "mi = unreal.load_asset(%r)\n"
            "old = json.loads(%r)\n"
            "for k, v in old['cvar'].items():\n"
            "    unreal.SystemLibrary.execute_console_command(None, '%%s %%g' %% (k, v))\n"
            "for k, v in old['mi'].items():\n"
            "    mel.set_material_instance_scalar_parameter_value(mi, k, v)\n"
            "for k, v in old.get('mitex', {}).items():\n"
            "    mel.set_material_instance_texture_parameter_value(mi, k, unreal.load_asset(v) if v else None)\n"
            "mel.update_material_instance(mi)\n"
            "eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)\n"
            "for a in eas.get_all_level_actors():\n"
            "    if a.get_class().get_name() == 'SaltOcean':\n"
            "        for k, v in old.get('ocean', {}).items():\n"
            "            a.set_editor_property(k, unreal.Vector4(*v) if len(v) == 4 else v[0])\n"
            "        a.set_editor_property('freeze_time', False)\n" % (MI, old_line[0].split("SB_DIAG_OLD", 1)[1].strip() if old_line else "{\"cvar\": {}, \"mi\": {}}"))
        run(restore)


if __name__ == "__main__":
    main()
