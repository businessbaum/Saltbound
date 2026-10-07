"""A/B sheets of the hub's lantern light in play (D82): PIE in the open map, the hub's lantern lights ("HubLantern") and its
interior exposure volumes changed live per variant, the same camera poses captured for each (pie_wait.settled_shot), one labelled
sheet (rows: preset and variant, columns: poses). Nothing is saved: the variants live only in the PIE world.

  python Tools/hub_light_ab.py <out.jpg> --poses "x y z pitch yaw; x y z pitch yaw" [--presets Day Night]
         [--variant "now"] [--variant "K=2800 shadow=0"] ... [--max 6]

  Poses in ship metres (x from the stern face, +y starboard, z up from the waterline); yaw in the ship's frame (0 = to the bow).
  A variant is a label of key=value pairs over today's lanterns (hub_place.py, D82): K temperature (2800), cd intensity
  (240), radius cm (1100), src source radius cm (12), shadow 1|0, gi indirect lighting intensity (1), minev / maxev the
  interior exposure volumes' range (0 / 7.5), bias their exposure bias (-1). "now" is today's values. The lanterns' flicker is off
  in every variant (it caches the intensity it starts with), so the rows compare like with like.
  In L_VisualTest the hub is shown and her inside loaded first (sb.Hub.Appear show).
"""
import argparse
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "Art", "Blender", "Scripts"))
sys.path.insert(0, os.path.dirname(__file__))
import hub_layout as H  # noqa: E402
import pie_wait as W  # noqa: E402
import run_pie_test as R  # noqa: E402
import ue_exec  # noqa: E402

NOW = {"K": 2800.0, "cd": 240.0, "radius": 1100.0, "src": 12.0, "shadow": 1.0, "gi": 1.0, "minev": 0.0, "maxev": 7.5, "bias": -1.0}

APPLY = r'''
import unreal
w = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
assert w is not None, "no PIE world"
V = %r
for r in unreal.GameplayStatics.get_all_actors_of_class(w, unreal.SaltHubRopes):
    r.set_editor_property("flicker_amount", 0.0)
n = 0
for a in unreal.GameplayStatics.get_all_actors_with_tag(w, "HubLantern"):
    lc = a.get_component_by_class(unreal.PointLightComponent)
    lc.set_use_temperature(True)
    lc.set_temperature(V["K"])
    lc.set_intensity(V["cd"])
    lc.set_attenuation_radius(V["radius"])
    lc.set_source_radius(V["src"])
    lc.set_cast_shadows(V["shadow"] > 0.5)
    lc.set_indirect_lighting_intensity(V["gi"])
    n += 1
m = 0
for a in unreal.GameplayStatics.get_all_actors_of_class(w, unreal.PostProcessVolume):
    if a.get_editor_property("unbound") or abs(a.get_editor_property("priority") - 1.0) > 1e-3:
        continue
    s = a.get_editor_property("settings")
    s.set_editor_property("auto_exposure_min_brightness", V["minev"])
    s.set_editor_property("auto_exposure_max_brightness", V["maxev"])
    if "bias" in V:
        s.set_editor_property("override_auto_exposure_bias", True)
        s.set_editor_property("auto_exposure_bias", V["bias"])
    else:
        s.set_editor_property("override_auto_exposure_bias", False)
    a.set_editor_property("settings", s)
    m += 1
print("SB_AB lanterns=%%d volumes=%%d" %% (n, m))
'''

MAP_NAME = r'''
import unreal
w = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
print("SB_AB map=" + (w.get_name() if w else "none"))
'''


def parse_variant(text):
    label = text.strip()
    v = dict(NOW)
    if label != "now":
        for kv in label.split():
            k, val = kv.split("=", 1)
            if k not in NOW:
                raise SystemExit("unknown key %s in variant %r" % (k, label))
            v[k] = float(val)
    return label, v


def output_of(res):
    return "".join(o.get("output", "") for o in (res or {}).get("output", [])) if isinstance(res, dict) else str(res)


def sheet(out, rows, labels, width=640):
    from PIL import Image, ImageDraw, ImageFont
    first = next(p for row in rows for p in row if p)
    w0, h0 = Image.open(first).size
    h = int(width * h0 / w0)
    pad = 26
    cols = max(len(r) for r in rows)
    S = Image.new("RGB", (width * cols, (h + pad) * len(rows)), (18, 18, 18))
    d = ImageDraw.Draw(S)
    try:
        font = ImageFont.truetype("arial.ttf", 18)
    except OSError:
        font = ImageFont.load_default()
    for r, (row, label) in enumerate(zip(rows, labels)):
        y = r * (h + pad)
        d.text((8, y + 3), label, fill=(235, 225, 200), font=font)
        for c, p in enumerate(row):
            if p:
                S.paste(Image.open(p).convert("RGB").resize((width, h)), (c * width, y + pad))
    S.save(out, quality=90)
    print("SB_SHEET", out)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("out")
    ap.add_argument("--poses", required=True)
    ap.add_argument("--presets", nargs="+", default=["Day"])
    ap.add_argument("--variant", action="append", default=[])
    ap.add_argument("--max", type=float, default=6.0, help="longest wait for a settled image per shot (s)")
    ap.add_argument("--keep-pie", action="store_true")
    ns = ap.parse_args()
    poses = [[float(v) for v in p.split()] for p in ns.poses.split(";") if p.strip()]
    variants = [parse_variant(t) for t in (ns.variant or ["now"])]
    _, byaw, _ = H.berth_world()
    base = os.path.splitext(ns.out)[0]
    rows, labels = [], []
    R.start_pie()
    try:
        if "L_VisualTest" in output_of(ue_exec.run(MAP_NAME)):
            offset = R.log_size()
            R.pie_console("sb.Hub.Appear show")
            W.wait_log("SB_HUB appear", offset, timeout=20.0)
        R.pie_console("sb.Hub.Doors snapclosed")
        for preset in ns.presets:
            R.pie_console("sb.Lighting " + preset)
            for vi, (label, v) in enumerate(variants):
                print("SB_AB", preset, label, output_of(ue_exec.run(APPLY % v)).strip())
                row = []
                for k, (x, y, z, pitch, yaw) in enumerate(poses):
                    wx, wy, wz = H.ship_to_world(x, y, z)
                    R.pie_console("sb.Dev.Camera %.1f %.1f %.1f %.1f %.1f 80" % (wx, wy, wz, pitch, byaw + yaw))
                    row.append(W.settled_shot("%s_%s_v%d_p%d.png" % (base, preset, vi, k), max_s=ns.max))
                rows.append(row)
                labels.append("%s  |  %s" % (preset, label))
    finally:
        if not ns.keep_pie:
            R.stop_pie()
    if any(p for row in rows for p in row):
        sheet(ns.out, rows, labels)


if __name__ == "__main__":
    main()
