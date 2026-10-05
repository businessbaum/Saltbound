"""Render the hub ship's plan sheets (design proposal) to images.

  python Tools/design/render_hub_ship.py [sheet-id ...]   (all sheets and the data)
  python Tools/design/render_hub_ship.py --data           (only Docs/Design/HubShip/HubShip_Data.json)

Opens Docs/Design/HubShip/HubShip_Plans.html#export in headless Chrome (the sheets are canvas drawings made by
hub_kit.js, hub_data.js and hub_sheets*.js, with the UI fonts from Art/Fonts), and writes each sheet to
Docs/Design/HubShip/HubShip_<nn>_<id>.jpg (one Chrome run per sheet: a page holding all of them chokes on the data). Also prints the floor areas (open / planned) that HubShip_Design.md quotes, and writes HubShip_Data.json: every
number of hub_data.js as plain JSON for the build's Blender and Unreal scripts.
A review drawing, not game content.
"""
import base64
import json
import os
import re
import subprocess
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
DIR = os.path.join(ROOT, "Docs", "Design", "HubShip")
PAGE = os.path.join(DIR, "HubShip_Plans.html")
CHROME = [r"C:\Program Files\Google\Chrome\Application\chrome.exe", r"C:\Program Files (x86)\Microsoft\Edge\Application\msedge.exe"]


SHEET_IDS = ["concept", "approach", "section", "exploded", "cross", "plans", "zones", "slip", "tally", "keelgate", "chandlery"]


def chrome_dump(url):
    exe = next(p for p in CHROME if os.path.exists(p))
    res = subprocess.run([exe, "--headless=new", "--disable-gpu", "--allow-file-access-from-files", "--virtual-time-budget=180000",
                          "--run-all-compositor-stages-before-draw", "--dump-dom", url], capture_output=True, text=True, encoding="utf-8",
                         timeout=900)
    m = re.search(r'<pre id="sb-out">(.*?)</pre>', res.stdout, re.S)
    if not m:
        print(res.stdout[:3000], res.stderr[:3000])
        raise SystemExit("nothing rendered: " + url)
    return json.loads(m.group(1).replace("&amp;", "&").replace("&lt;", "<").replace("&gt;", ">").replace("&quot;", '"'))


def write_data():
    data = chrome_dump("file:///" + PAGE.replace("\\", "/") + "#data")
    path = os.path.join(DIR, "HubShip_Data.json")
    with open(path, "w", encoding="utf-8", newline="\n") as f:
        json.dump(data, f, indent=1, ensure_ascii=False)
    print("SB_DATA", path, os.path.getsize(path))


def main(names):
    write_data()
    if names == ["--data"]:
        return
    errors = 0
    for i, name in enumerate(names or SHEET_IDS):
        errors += render(name, report_areas=(i == 0))
    if errors:
        sys.exit(1)


def render(sheet, report_areas):
    out = chrome_dump("file:///" + PAGE.replace("\\", "/") + "#export:" + sheet)
    areas = out.pop("areas", None)
    errors = {k: v for k, v in out.items() if k.startswith("error_")}
    for key, value in out.items():
        if key.startswith("time_"):
            print("SB_TIME", key[5:], value, "ms")
        elif not key.startswith("error_"):
            data = base64.b64decode(value.split(",", 1)[1])
            open(os.path.join(DIR, key + ".jpg"), "wb").write(data)
            print("SB_SHEET", key, len(data))
    if areas and report_areas:
        print("SB_AREAS total %.0f open %.0f (%.1f %%)" % (areas["total"], areas["openTotal"], 100 * areas["openTotal"] / areas["total"]))
        for k, v in areas["levels"].items():
            print("  level %-7s total %7.0f open %7.0f" % (k, v["total"], v["open"]))
        for k, v in sorted(areas["open"].items()):
            print("  open   %-10s %7.0f" % (k, v))
        for k, v in sorted(areas["locked"].items(), key=lambda kv: (len(kv[0]), kv[0])):
            print("  locked %-10s %7.0f" % (k, v))
    for k, v in errors.items():
        print("SB_ERROR", k, v)
    return len(errors)


if __name__ == "__main__":
    main(sys.argv[1:])
