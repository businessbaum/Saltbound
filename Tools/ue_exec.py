"""Run Python inside a running Unreal Editor via the PythonScriptPlugin remote execution protocol.

Usage:
  python Tools/ue_exec.py -c "import unreal; print(unreal.SystemLibrary.get_engine_version())"
  python Tools/ue_exec.py path/to/script.py [--arg value ...]   (script args are exposed as sys.argv inside UE)
Requires bRemoteExecution=True in Saltbound/Config/DefaultEngine.ini (already set).
"""
import argparse, atexit, json, os, sys, time

ENGINE = os.environ.get("SALTBOUND_ENGINE", r"C:\Program Files\Epic Games\UE_5.8")
sys.path.insert(0, os.path.join(ENGINE, r"Engine\Plugins\Experimental\PythonScriptPlugin\Content\Python"))
import remote_execution as rex  # noqa: E402


def connect(timeout=30.0):
    cfg = rex.RemoteExecutionConfig()
    cfg.multicast_bind_address = "127.0.0.1"
    re = rex.RemoteExecution(cfg)
    re.start()
    t0 = time.time()
    while time.time() - t0 < timeout:
        nodes = re.remote_nodes
        if nodes:
            # Prefer a node for the Saltbound project
            node = next((n for n in nodes if "Saltbound" in n.get("project_name", "")), nodes[0])
            re.open_command_connection(node["node_id"])
            return re
        time.sleep(0.25)
    re.stop()
    raise SystemExit("No Unreal Editor remote execution node found (is the editor running?)")


_conn = None


def close():
    global _conn
    if _conn is not None:
        try:
            _conn.stop()
        except Exception:
            pass
        _conn = None


def run(code, mode=rex.MODE_EXEC_FILE, timeout=30.0):
    """One connection per process, kept open between calls: finding the node and connecting costs 0.3-1 s each time,
    and PIE scripts send dozens of console commands. A dead connection is replaced before sending, never after."""
    global _conn
    if _conn is not None and not _conn.has_command_connection():
        close()
    if _conn is None:
        _conn = connect(timeout)
        atexit.register(close)
    try:
        return _conn.run_command(code, unattended=True, exec_mode=mode)
    except Exception:
        close()
        raise


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("script", nargs="?")
    ap.add_argument("-c", "--code")
    ap.add_argument("--json", action="store_true", help="print raw JSON result")
    ns, rest = ap.parse_known_args()
    if ns.code:
        code = ns.code
    elif ns.script:
        path = os.path.abspath(ns.script)
        argv = [path] + rest
        code = ("import sys, runpy\nsys.argv = %r\nrunpy.run_path(%r, run_name='__main__')\n" % (argv, path))
    else:
        code = sys.stdin.read()
    res = run(code)
    if ns.json:
        print(json.dumps(res, indent=1))
    else:
        for o in res.get("output", []):
            print(("[%s] " % o["type"] if o["type"] != "Info" else "") + o["output"].rstrip("\n"))
        if not res.get("success"):
            print("RESULT:", res.get("result"))
            sys.exit(1)


if __name__ == "__main__":
    main()
