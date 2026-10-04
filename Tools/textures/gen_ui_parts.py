"""Render the painted UI parts (D55) with the approved design's own drawing code, each alone on a transparent canvas.

  python Tools/textures/gen_ui_parts.py [PartName ...]

The design pages (Docs/Design/UI/UI_Satchel.html, UI_Prompts.html) draw every screen with a small canvas toolkit. This
script pulls that toolkit and the named drawing functions out of them verbatim, puts Tools/textures/ui_parts.js (the
part list) after them, renders the page in headless Chrome with the UI fonts from Art/Fonts, and writes each part to
Art/Textures/UI/<name>.png (import: python Tools/ue_exec.py Tools/ue_scripts/import_textures.py UI). So the parts look
exactly like the pictures the owner approved; the 3D pieces (brass, rope, wax, the crate) are Blender renders instead.
"""
import base64
import json
import os
import re
import subprocess
import sys
import tempfile

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
DESIGN = os.path.join(ROOT, "Docs", "Design", "UI")
OUT = os.path.join(ROOT, "Art", "Textures", "UI")
FONTS = os.path.join(ROOT, "Art", "Fonts")
CHROME = [r"C:\Program Files\Google\Chrome\Application\chrome.exe", r"C:\Program Files (x86)\Microsoft\Edge\Application\msedge.exe"]

# Named functions taken from the design pages (in this order; later pages override earlier ones of the same name).
FROM_SATCHEL = ["leatherAt", "woodAt", "poly", "ironPlate", "burn", "countCoin", "itemImg", "selGlow", "ringSlot", "ropeRing",
                "lashing", "medallion", "toolCompass", "toolSpyglass", "toolChart", "toolLantern", "toolSatchel", "toolHand"]
FROM_PROMPTS = ["scrapPath", "paperFill", "scrap", "inkText", "measure", "inkCircle", "fancy", "waxSeal", "sealGlyph", "keyScrap"]


def read(name):
    return open(os.path.join(DESIGN, name), encoding="utf-8").read()


def function_source(src, name):
    """A top-level `function name(` up to the next top-level statement."""
    m = re.search(r"^(async )?function %s\(" % re.escape(name), src, re.M)
    if not m:
        raise SystemExit("design function not found: " + name)
    rest = src[m.start():]
    end = re.search(r"\n(?=(async )?function |const |let |/\*|\(async|</script>)", rest[1:])
    return rest[: end.start() + 1 if end else len(rest)]


def build_page():
    satchel, prompts = read("UI_Satchel.html"), read("UI_Prompts.html")
    toolkit = satchel[satchel.index("/* ===== toolkit ===== */"): satchel.index("/* the game view behind the screens */")]
    funcs = [function_source(satchel, n) for n in FROM_SATCHEL] + [function_source(prompts, n) for n in FROM_PROMPTS]
    pre = """const FL='"IM Fell English",Georgia,serif',FSC='"IM Fell English SC",Georgia,serif',FT='Alegreya,Georgia,serif',INK='rgba(48,30,16,.94)';
let PARCH,PARCH_BIG,SPECK,ICON={},LEATHER_BIG,LEATHER_DISC,LEATHER_SMALL,PARCH_SMALL,PARCH_TAG,WOOD;
const KS=58,KSEED=12;
"""
    font_url = lambda rel: "file:///" + os.path.join(FONTS, rel).replace("\\", "/")
    faces = [("IM Fell English", "normal", 400, "IMFellEnglish/IMFeENrm28P.ttf"), ("IM Fell English", "italic", 400, "IMFellEnglish/IMFeENit28P.ttf"),
             ("IM Fell English SC", "normal", 400, "IMFellEnglishSC/IMFeENsc28P.ttf"), ("Alegreya", "normal", 400, "Alegreya/Alegreya-Regular.ttf"),
             ("Alegreya", "normal", 700, "Alegreya/Alegreya-Bold.ttf"), ("Alegreya", "italic", 400, "Alegreya/Alegreya-Italic.ttf")]
    css = "\n".join("@font-face{font-family:'%s';font-style:%s;font-weight:%d;src:url('%s')}" % (f, s, w, font_url(p)) for f, s, w, p in faces)
    parts_js = open(os.path.join(os.path.dirname(__file__), "ui_parts.js"), encoding="utf-8").read()
    runner = """
(async()=>{
  const want = (location.hash.slice(1) || '').split(',').filter(Boolean);
  await Promise.all(['400 40px "IM Fell English"','italic 400 40px "IM Fell English"','400 40px "IM Fell English SC"','400 20px Alegreya','700 20px Alegreya','italic 400 20px Alegreya'].map(f=>document.fonts.load(f)));
  setupShared();
  const out = {};
  for (const p of PARTS) {
    if (want.length && !want.includes(p.name)) continue;
    const cv = canvas(Math.round(p.w * p.scale), Math.round(p.h * p.scale)), c = cv.getContext('2d');
    c.scale(p.scale, p.scale); await p.draw(c);
    out[p.name] = cv.toDataURL('image/png');
  }
  document.body.innerHTML = '<pre id="sb-out">' + JSON.stringify(out) + '</pre>';
})();
"""
    return "<!doctype html><meta charset=utf-8><style>%s</style><body><script>\n%s\n%s\n%s\n%s\n%s</script>" % (
        css, pre, toolkit, "\n".join(funcs), parts_js, runner)


def main(names):
    page = build_page()
    tmp = os.path.join(tempfile.gettempdir(), "sb_ui_parts.html")
    open(tmp, "w", encoding="utf-8").write(page)
    exe = next(p for p in CHROME if os.path.exists(p))
    url = "file:///" + tmp.replace("\\", "/") + ("#" + ",".join(names) if names else "")
    res = subprocess.run([exe, "--headless=new", "--disable-gpu", "--allow-file-access-from-files", "--virtual-time-budget=120000",
                          "--run-all-compositor-stages-before-draw", "--dump-dom", url], capture_output=True, text=True, encoding="utf-8",
                         timeout=600)
    m = re.search(r'<pre id="sb-out">(.*?)</pre>', res.stdout, re.S)
    if not m:
        print(res.stdout[:2000], res.stderr[:2000])
        raise SystemExit("no parts rendered")
    parts = json.loads(m.group(1).replace("&amp;", "&").replace("&lt;", "<").replace("&gt;", ">").replace("&quot;", '"'))
    os.makedirs(OUT, exist_ok=True)
    for name, url in parts.items():
        data = base64.b64decode(url.split(",", 1)[1])
        open(os.path.join(OUT, name + ".png"), "wb").write(data)
        print("SB_UI_PART", name, len(data))
    print("SB_UI_PARTS_DONE", len(parts))
    icon_shadows()


def icon_shadows():
    """Each item icon's soft shadow (its silhouette, blurred, 70 % black), as the design's canvas drop shadow on the icon:
    T_Icon_X.png -> T_UI_IconShadow_X.png, same frame. The widgets offset it (3, 6) px under the icon."""
    import glob
    from PIL import Image, ImageFilter
    for path in sorted(glob.glob(os.path.join(OUT, "T_Icon_*.png"))):
        icon = Image.open(path).convert("RGBA")
        w, h = icon.size
        alpha = icon.split()[3].filter(ImageFilter.GaussianBlur(radius=9 * w / 96))
        shadow = Image.new("RGBA", (w, h), (0, 0, 0, 0))
        shadow.putalpha(alpha.point(lambda v: int(v * 0.7)))
        name = os.path.basename(path).replace("T_Icon_", "T_UI_IconShadow_")
        shadow.save(os.path.join(OUT, name))
        print("SB_UI_PART", name[:-4])


if __name__ == "__main__":
    main(sys.argv[1:])
