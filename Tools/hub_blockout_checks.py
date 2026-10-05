"""Phase 1 measurements of the hub ship's blockout (D61) on L_HubBlockout (open it in the editor first).

  python Tools/hub_blockout_checks.py [headroom] [walk | walk:<slip|tally|keelgate|chandlery>] [boat] [water]

headroom runs in the editor world; the others start PIE, drive Tools/ue_scripts/hub_checks.py inside the editor and print
its SB_ lines. Short runs: each part alone (the owner finds long suites too slow).
"""
import os
import re
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import run_pie_test as pie  # noqa: E402
import ue_exec  # noqa: E402

PRE = "import sys, importlib\nsys.path.insert(0, r'%s')\nimport hub_checks\nimportlib.reload(hub_checks)\n" % os.path.join(
    os.path.dirname(os.path.abspath(__file__)), "ue_scripts")


def run(code):
    return ue_exec.run(PRE + code)


def wait_for(offset, pattern, timeout):
    t0 = time.time()
    while time.time() - t0 < timeout:
        time.sleep(1.0)
        text = pie.read_from(offset)
        if re.search(pattern, text):
            return text
    return pie.read_from(offset)


def show(text, prefix):
    for line in re.findall(r"(SB_%s[^\r\n]*)" % prefix, text):
        print(line[:400])


def headroom():
    off = pie.log_size()
    run("hub_checks.headroom()")
    show(wait_for(off, r"SB_HEADROOM DONE", 600), "HEADROOM")


def walks(names=("slip", "tally", "keelgate", "chandlery")):
    pie.start_pie(2.0)
    try:
        time.sleep(3.0)
        for name in names:
            off = pie.log_size()
            run("hub_checks.walk(%r)" % name)
            show(wait_for(off, r"SB_WALK DONE", 200), "WALK")
    finally:
        pie.stop_pie()


def boat():
    pie.start_pie(2.0)
    try:
        time.sleep(3.0)
        for name in ("in", "turn", "out"):
            off = pie.log_size()
            run("hub_checks.boat(%r)" % name)
            text = wait_for(off, r"SB_HUBBOAT DONE", 120)
            show(text, "HUBBOAT")
            contacts = [int(c) for c in re.findall(r"SB_BOAT .*?contacts=(\d+)", text)]
            print("   contacts per second:", contacts)
    finally:
        pie.stop_pie()


def water():
    pie.start_pie(2.0)
    try:
        time.sleep(3.0)
        pie.pie_console("sb.SeaState Storm 0")
        time.sleep(8.0)
        off = pie.log_size()
        run("hub_checks.water(60.0)")
        show(wait_for(off, r"SB_HUBWATER DONE", 120), "HUBWATER")
    finally:
        pie.stop_pie()


if __name__ == "__main__":
    parts = sys.argv[1:] or ["headroom", "walk", "boat", "water"]
    if "headroom" in parts:
        headroom()
    if "walk" in parts:
        walks()
    for name in ("slip", "tally", "keelgate", "chandlery"):
        if "walk:" + name in parts:
            walks((name,))
    if "boat" in parts:
        boat()
    if "water" in parts:
        water()
