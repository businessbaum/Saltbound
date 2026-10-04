"""Capture a UI screen in PIE (the engine's own `shot showui`, never the desktop) and put it next to its reference.

  python Tools/ui_capture.py <out.png> [--ref Docs/Design/UI/UI_prompt.png] [--wait 2] [--keep-pie] -- <console cmd> [<console cmd> ...]

Starts PIE (if needed), runs the console commands, waits, takes `shot showui`, scales the shot to 1920 x 1080 and writes
an A/B sheet (shot on top, reference below; each half size) to <out.png>, or just the shot without --ref.
"""
import argparse
import glob
import os
import sys
import time

from PIL import Image

TOOLS = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, TOOLS)
import run_pie_test as pie  # noqa: E402

SHOTS = os.path.join(TOOLS, "..", "Saltbound", "Saved", "Screenshots")


def shot():
    before = set(glob.glob(os.path.join(SHOTS, "**", "*.png"), recursive=True))
    pie.pie_console("shot showui")
    for _ in range(40):
        time.sleep(0.25)
        new = sorted(set(glob.glob(os.path.join(SHOTS, "**", "*.png"), recursive=True)) - before, key=os.path.getmtime)
        if new:
            time.sleep(0.5)
            return new[-1]
    raise SystemExit("no screenshot")


def main():
    argv = sys.argv[1:]
    cmds = []
    if "--" in argv:
        cmds = argv[argv.index("--") + 1:]
        argv = argv[: argv.index("--")]
    ap = argparse.ArgumentParser()
    ap.add_argument("out")
    ap.add_argument("--ref")
    ap.add_argument("--wait", type=float, default=2.0)
    ap.add_argument("--keep-pie", action="store_true")
    ns = ap.parse_args(argv)
    # A floating 1920 x 1080 PIE window (Editor > Level Editor > Play In: NewWindowWidth/Height), so `shot showui` takes
    # exactly the game frame (in the editor viewport it would take the whole editor window).
    if not pie.pie_running():
        opts = {"options": {"bSimulate": False, "playMode": "PlayMode_InEditorFloating", "warmupSeconds": 2.5}}
        res = pie.ue_mcp.call_tool("EditorToolset.EditorAppToolset", "StartPIE", opts)
        if res.get("isError"):
            raise SystemExit("StartPIE failed: " + pie.ue_mcp.text_of(res))
    try:
        for c in cmds:
            pie.pie_console(c)
            time.sleep(0.2)
        time.sleep(ns.wait)
        path = shot()
    finally:
        if not ns.keep_pie:
            pie.stop_pie()
    im = Image.open(path).convert("RGB")
    # The floating window's shot includes its frame (title bar and borders): keep the 1920 x 1080 client area.
    w, h = im.size
    if 1920 <= w < 1960 and 1080 <= h < 1160:
        b = (w - 1920) // 2
        im = im.crop((b, h - b - 1080, b + 1920, h - b))
    # The viewport need not be 16:9: crop its middle to 16:9 (UMG scales by the short side), then scale to 1080p.
    w, h = im.size
    if w * 9 > h * 16:
        cw = h * 16 // 9
        im = im.crop(((w - cw) // 2, 0, (w - cw) // 2 + cw, h))
    else:
        ch = w * 9 // 16
        im = im.crop((0, (h - ch) // 2, w, (h - ch) // 2 + ch))
    im = im.resize((1920, 1080), Image.LANCZOS)
    if ns.ref:
        ref = Image.open(ns.ref).convert("RGB").resize((1920, 1080), Image.LANCZOS)
        sheet = Image.new("RGB", (1920, 2160))
        sheet.paste(im, (0, 0))
        sheet.paste(ref, (0, 1080))
        im = sheet
    im.save(ns.out)
    print("SB_UI_CAPTURE", ns.out, "from", path)


if __name__ == "__main__":
    main()
