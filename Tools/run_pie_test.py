"""Start PIE in the running editor, execute console commands, collect SB_TEST log lines, stop PIE.

Usage:
  python Tools/run_pie_test.py "sb.Dev.RunMovementTests 1 300 8" [--timeout 120] [--done "SB_TEST Suite DONE"]
  python Tools/run_pie_test.py "sb.Dev.RunMovementTests 1" "sb.Dev.RunMovementTests 2"   (run sequentially)
Exit code 0 when every SB_TEST line reports PASS.
"""
import argparse
import os
import sys
import time

sys.path.insert(0, os.path.dirname(__file__))
import ue_exec  # noqa: E402
import ue_mcp  # noqa: E402

LOG = os.path.join(os.path.dirname(__file__), "..", "Saltbound", "Saved", "Logs", "Saltbound.log")


def log_size():
    return os.path.getsize(LOG) if os.path.exists(LOG) else 0


def read_from(offset):
    with open(LOG, "rb") as f:
        f.seek(offset)
        return f.read().decode("utf-8", "replace")


def pie_running():
    res = ue_mcp.call_tool("EditorToolset.EditorAppToolset", "IsPIERunning")
    return '"returnValue":true' in ue_mcp.text_of(res).replace(" ", "")


def start_pie(warmup=1.5):
    if pie_running():
        return
    opts = {"options": {"bSimulate": False, "playMode": "PlayMode_InViewPort", "warmupSeconds": warmup}}
    res = ue_mcp.call_tool("EditorToolset.EditorAppToolset", "StartPIE", opts)
    if res.get("isError"):
        raise SystemExit("StartPIE failed: " + ue_mcp.text_of(res))


def stop_pie():
    if pie_running():
        ue_mcp.call_tool("EditorToolset.EditorAppToolset", "StopPIE")


def pie_console(cmd):
    code = (
        "import unreal\n"
        "w = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()\n"
        "assert w is not None, 'no PIE world'\n"
        "unreal.SystemLibrary.execute_console_command(w, %r)\n" % cmd
    )
    ue_exec.run(code)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("commands", nargs="+")
    ap.add_argument("--timeout", type=float, default=120.0)
    ap.add_argument("--done", default="SB_TEST Suite DONE")
    ap.add_argument("--keep-pie", action="store_true")
    ns = ap.parse_args()

    start_pie()
    all_lines = []
    try:
        for cmd in ns.commands:
            offset = log_size()
            pie_console(cmd)
            t0 = time.time()
            done = False
            while time.time() - t0 < ns.timeout and not done:
                time.sleep(0.1)
                text = read_from(offset)
                done = ns.done in text
            lines = [l.split("SB_TEST", 1)[1].strip() for l in read_from(offset).splitlines() if "SB_TEST" in l]
            print("== %s%s" % (cmd, "" if done else "  (TIMEOUT)"))
            for l in lines:
                print("   " + l)
            all_lines += lines
            if not done:
                all_lines.append("TIMEOUT FAIL")
    finally:
        if not ns.keep_pie:
            stop_pie()
    failed = [l for l in all_lines if " FAIL" in l or l.startswith("TIMEOUT")]
    print("RESULT: %s (%d failing lines)" % ("PASS" if not failed else "FAIL", len(failed)))
    sys.exit(1 if failed else 0)


if __name__ == "__main__":
    main()
