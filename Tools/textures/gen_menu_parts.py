"""Render the menu parts (D66) with the approved designs' own drawing code, each alone on a transparent canvas.

  python Tools/textures/gen_menu_parts.py [PartName ...]

Uses gen_ui_parts.py's page (the design pages' canvas toolkit and prompt functions) with Tools/textures/menu_parts.js as the
part list, and writes Art/Textures/UI/<name>.png (import: python Tools/ue_exec.py Tools/ue_scripts/import_textures.py UI).
T_UI_MenuTitle is cut from the launch intro's own title at its start-menu place (Docs/Design/Menu_Ideas/T_UI_Title_Menu.png,
exported from Docs/Design/UI/Launch_Intro.html#title), so the start menu's title is the intro's last frame, pixel for pixel.
"""
import base64
import json
import os
import re
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import gen_ui_parts as G  # noqa: E402

ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
OUT = os.environ.get("SB_UI_OUT", os.path.join(ROOT, "Art", "Textures", "UI"))
if os.environ.get("SB_FONTS"):
    G.FONTS = os.environ["SB_FONTS"]
TITLE_SRC = os.environ.get("SB_TITLE", os.path.join(ROOT, "Docs", "Design", "Menu_Ideas", "T_UI_Title_Menu.png"))
# Where the title's crop sits on the 1080p screen (the widget places T_UI_MenuTitle here).
TITLE_BOX = (101, 155, 833, 305)


def main(names):
    page = G.build_page()
    parts_js = open(os.path.join(HERE, "ui_parts.js"), encoding="utf-8").read()
    mine = open(os.path.join(HERE, "menu_parts.js"), encoding="utf-8").read()
    assert parts_js in page
    page = page.replace(parts_js, mine)
    tmp = os.path.join(tempfile.gettempdir(), "sb_menu_parts.html")
    open(tmp, "w", encoding="utf-8").write(page)
    exe = next(p for p in G.CHROME if os.path.exists(p))
    url = "file:///" + tmp.replace("\\", "/") + ("#" + ",".join(n for n in names if n not in ("T_UI_MenuTitle", "T_UI_MenuKnotMask")) if names else "")
    res = subprocess.run([exe, "--headless=new", "--disable-gpu", "--allow-file-access-from-files", "--virtual-time-budget=120000",
                          "--run-all-compositor-stages-before-draw", "--dump-dom", url], capture_output=True, text=True, encoding="utf-8",
                         timeout=600)
    m = re.search(r'<pre id="sb-out">(.*?)</pre>', res.stdout, re.S)
    if not m:
        print(res.stdout[:2000], res.stderr[:2000])
        raise SystemExit("no parts rendered")
    parts = json.loads(m.group(1).replace("&amp;", "&").replace("&lt;", "<").replace("&gt;", ">").replace("&quot;", '"'))
    os.makedirs(OUT, exist_ok=True)
    for name, data_url in parts.items():
        data = base64.b64decode(data_url.split(",", 1)[1])
        open(os.path.join(OUT, name + ".png"), "wb").write(data)
        print("SB_UI_PART", name, len(data))
    if not names or "T_UI_MenuTitle" in names:
        from PIL import Image
        Image.open(TITLE_SRC).convert("RGBA").crop(TITLE_BOX).save(os.path.join(OUT, "T_UI_MenuTitle.png"))
        print("SB_UI_PART T_UI_MenuTitle", TITLE_BOX)
    if not names or "T_UI_MenuKnotMask" in names:
        # The first start's brightness knots: the save sign's knot as plain white shapes, so the widget tints them exactly.
        from PIL import Image
        for layer in ("Ring", "Rose"):
            src = Image.open(os.path.join(OUT, "T_UI_SaveKnot_%s.png" % layer)).convert("RGBA")
            white = Image.new("RGBA", src.size, (255, 255, 255, 255))
            white.putalpha(src.getchannel("A"))
            white.save(os.path.join(OUT, "T_UI_MenuKnotMask_%s.png" % layer))
            print("SB_UI_PART T_UI_MenuKnotMask_%s" % layer)
    print("SB_UI_PARTS_DONE", len(parts) + 1)


if __name__ == "__main__":
    main(sys.argv[1:])
