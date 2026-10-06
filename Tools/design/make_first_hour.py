"""Builds Docs/Design/UI/UI_FirstHour.html (D75, M6's new screens as ideas for the owner).

The screens are drawn on 1920 x 1080 canvases with the approved pages' own drawing kit: the toolkit and the named drawing
functions are pulled verbatim out of UI_Satchel.html and UI_Prompts.html (as Tools/textures/gen_ui_parts.py does), the screens
themselves are Tools/design/first_hour_screens.js. Headless Chrome renders them with the UI fonts from Art/Fonts into
Docs/Design/UI/FirstHour/screen_<name>.jpg; the page (the menus page's style, Tools/design/first_hour_body.html) shows them
with the notes and questions, self-contained (pictures as data URIs). UI_FirstHour.jpg is a contact sheet of the screens.

    python Tools/design/make_first_hour.py            render the screens, then build the page and the sheet
    python Tools/design/make_first_hour.py page       only the page and the sheet (from the screens on disk)
"""
import base64
import json
import os
import re
import subprocess
import sys
import tempfile

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(ROOT, "Tools", "textures"))
from gen_ui_parts import CHROME, DESIGN, FONTS, function_source, read  # noqa: E402

MENUS = os.path.join(ROOT, "Docs", "Design", "Menu_Ideas", "Menus.html")
BODY = os.path.join(HERE, "first_hour_body.html")
SCREENS_JS = os.path.join(HERE, "first_hour_screens.js")
FOLDER = os.path.join(DESIGN, "FirstHour")
OUT = os.path.join(DESIGN, "UI_FirstHour.html")
SHEET = os.path.join(DESIGN, "UI_FirstHour.jpg")

SCREENS = ["speak", "speakb", "plans", "log", "board"]
BACKDROPS = {"day": "bg_pier_day.jpg", "evening": "bg_pier_evening.jpg", "night": "bg_pier_night.jpg"}
ICONS = ["Canvas", "Driftwood", "GlassFloat", "IronNails", "PaleFloat", "RopeCoil", "Seashell", "Tar"]
FROM_SATCHEL = ["backdrop", "leatherAt", "woodAt", "poly", "ironPlate", "burn", "countCoin", "itemImg", "selGlow", "ringSlot"]
FROM_PROMPTS = ["scrapPath", "paperFill", "scrap", "inkText", "measure", "inkCircle", "fancy", "waxSeal", "sealGlyph"]


def data_uri(path):
    kind = "image/png" if path.endswith(".png") else "image/jpeg"
    return "data:%s;base64,%s" % (kind, base64.b64encode(open(path, "rb").read()).decode("ascii"))


def render_page(names):
    satchel, prompts = read("UI_Satchel.html"), read("UI_Prompts.html")
    toolkit = satchel[satchel.index("/* ===== toolkit ===== */"): satchel.index("/* the game view behind the screens */")]
    funcs = [function_source(satchel, n) for n in FROM_SATCHEL] + [function_source(prompts, n) for n in FROM_PROMPTS]
    font_url = lambda rel: "file:///" + os.path.join(FONTS, rel).replace("\\", "/")
    faces = [("IM Fell English", "normal", 400, "IMFellEnglish/IMFeENrm28P.ttf"), ("IM Fell English", "italic", 400, "IMFellEnglish/IMFeENit28P.ttf"),
             ("IM Fell English SC", "normal", 400, "IMFellEnglishSC/IMFeENsc28P.ttf"), ("Alegreya", "normal", 400, "Alegreya/Alegreya-Regular.ttf"),
             ("Alegreya", "normal", 700, "Alegreya/Alegreya-Bold.ttf"), ("Alegreya", "italic", 400, "Alegreya/Alegreya-Italic.ttf")]
    css = "\n".join("@font-face{font-family:'%s';font-style:%s;font-weight:%d;src:url('%s')}" % (f, s, w, font_url(p)) for f, s, w, p in faces)
    icons = "const ICONS={%s};" % ",".join("%s:'%s'" % (k, data_uri(os.path.join(FOLDER, "icon_%s.png" % k))) for k in ICONS)
    bgs = "const BG={%s};" % ",".join("%s:'%s'" % (k, data_uri(os.path.join(FOLDER, v))) for k, v in BACKDROPS.items())
    canvases = "".join('<canvas id="c-%s" width="1920" height="1080"></canvas>' % n for n in names)
    done = """
const sbErr=m=>{const e=document.createElement('pre');e.id='sb-err';e.textContent=m;document.documentElement.appendChild(e)};
window.addEventListener('error',e=>sbErr(e.message+' @'+e.lineno+':'+e.colno));window.addEventListener('unhandledrejection',e=>sbErr('rejected: '+(e.reason&&e.reason.stack||e.reason)));
window.sbDone=()=>{const out={};document.querySelectorAll('canvas').forEach(cv=>out[cv.id.slice(2)]=cv.toDataURL('image/jpeg',.92));
  const pre=document.createElement('pre');pre.id='sb-out';pre.textContent=JSON.stringify(out);document.body.innerHTML='';document.body.appendChild(pre)};
"""
    js = open(SCREENS_JS, encoding="utf-8").read()
    return "<!doctype html><meta charset=utf-8><style>%s</style><body>%s<script>\n%s\n%s\n%s\n%s\n%s\n%s</script>" % (
        css, canvases, icons, bgs, done, toolkit, "\n".join(funcs), js)


def render(names):
    tmp = os.path.join(tempfile.gettempdir(), "sb_first_hour.html")
    open(tmp, "w", encoding="utf-8").write(render_page(names))
    exe = next(p for p in CHROME if os.path.exists(p))
    res = subprocess.run([exe, "--headless=new", "--disable-gpu", "--allow-file-access-from-files", "--virtual-time-budget=180000",
                          "--run-all-compositor-stages-before-draw", "--dump-dom", "file:///" + tmp.replace("\\", "/")],
                         capture_output=True, text=True, encoding="utf-8", timeout=900)
    m = re.search(r'<pre id="sb-out">(.*?)</pre>', res.stdout, re.S)
    for err in re.findall(r'<pre id="sb-err">(.*?)</pre>', res.stdout, re.S):
        print("SB_DESIGN_ERROR", err[:1500])
    if not m:
        raise SystemExit("no screens rendered")
    shots = json.loads(m.group(1).replace("&amp;", "&").replace("&lt;", "<").replace("&gt;", ">").replace("&quot;", '"'))
    for name, url in shots.items():
        path = os.path.join(FOLDER, "screen_%s.jpg" % name)
        open(path, "wb").write(base64.b64decode(url.split(",", 1)[1]))
        print("SB_DESIGN_SCREEN", name, os.path.getsize(path))


def page():
    lines = open(MENUS, encoding="utf-8").read().split("\n")
    end_style = next(i for i, l in enumerate(lines) if l.strip() == "</style>")
    head = "\n".join(lines[:end_style + 1]).replace("<title>Saltbound Menus</title>", "<title>Saltbound First Hour</title>")
    html = head + "\n</head>\n<body>\n" + open(BODY, encoding="utf-8").read() + "\n</body>\n</html>\n"
    html = re.sub(r"</head>\s*</head>", "</head>", html)
    html = re.sub(r"FirstHour/([A-Za-z0-9_]+\.(?:png|jpg))", lambda m: data_uri(os.path.join(FOLDER, m.group(1))), html)
    open(OUT, "w", encoding="utf-8").write(html)
    print("SB_DESIGN", OUT, len(html))
    # The contact sheet: the five screens, two across.
    from PIL import Image
    tw, th, gap = 960, 540, 16
    sheet = Image.new("RGB", (2 * tw + 3 * gap, 3 * th + 4 * gap), (22, 17, 12))
    for i, name in enumerate(SCREENS):
        im = Image.open(os.path.join(FOLDER, "screen_%s.jpg" % name)).convert("RGB").resize((tw, th), Image.LANCZOS)
        col, row = (i % 2, i // 2) if i < 4 else (0.5, 2)
        sheet.paste(im, (int(gap + col * (tw + gap)), gap + row * (th + gap)))
    sheet.save(SHEET, quality=88)
    print("SB_DESIGN_SHEET", SHEET)


if __name__ == "__main__":
    args = sys.argv[1:]
    if not args or args[0] != "page":
        render([a for a in args if a in SCREENS] or SCREENS)
    if not args or args[0] in ("page", "all"):
        page()
