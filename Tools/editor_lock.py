"""One editor, several agents: a lock round every use of the running Unreal Editor.

Two agents working at once (the hub and the home island, D70/D71) share one editor. Each takes the lock before anything that
uses it (ue_exec.py, ue_mcp.py captures, imports, PIE tests, loading or saving maps, Rebuild-Editor.ps1) and gives it back
when the map is saved. Blender builds and host-Python work need no lock.

    python Tools/editor_lock.py take <owner> "<what>"      exit 0 taken, 1 held by someone else (prints who and since when)
    python Tools/editor_lock.py wait <owner> "<what>" [s]  waits (up to s seconds, default 540) until it can take it
    python Tools/editor_lock.py give <owner>               gives it back (only its owner can)
    python Tools/editor_lock.py status

The lock is a directory (mkdir is atomic) under Saltbound/Saved (not in git).
"""
import json
import os
import sys
import time

LOCK = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "Saltbound", "Saved", "EditorLock")
INFO = os.path.join(LOCK, "owner.json")


def holder():
    try:
        with open(INFO, encoding="utf-8") as fh:
            return json.load(fh)
    except (OSError, ValueError):
        return {"owner": "?", "what": "?", "since": 0} if os.path.isdir(LOCK) else None


def describe(h):
    age = (time.time() - h.get("since", 0)) / 60.0
    return "%s (%s) for %.1f min" % (h.get("owner"), h.get("what"), age)


def take(owner, what):
    os.makedirs(os.path.dirname(LOCK), exist_ok=True)
    try:
        os.mkdir(LOCK)
    except FileExistsError:
        h = holder()
        if h and h.get("owner") == owner:
            return True
        return False
    with open(INFO, "w", encoding="utf-8") as fh:
        json.dump({"owner": owner, "what": what, "since": time.time()}, fh)
    return True


def give(owner):
    h = holder()
    if h is None:
        return True
    if h.get("owner") != owner:
        print("EDITOR_LOCK held by", describe(h), "- not yours")
        return False
    try:
        os.remove(INFO)
    except OSError:
        pass
    os.rmdir(LOCK)
    return True


def main(argv):
    if len(argv) < 2:
        print(__doc__)
        return 2
    cmd = argv[1]
    if cmd == "status":
        h = holder()
        print("EDITOR_LOCK free" if h is None else "EDITOR_LOCK held by " + describe(h))
        return 0
    owner = argv[2]
    if cmd == "take":
        ok = take(owner, argv[3] if len(argv) > 3 else "")
        print("EDITOR_LOCK taken by " + owner if ok else "EDITOR_LOCK held by " + describe(holder()))
        return 0 if ok else 1
    if cmd == "wait":
        what = argv[3] if len(argv) > 3 else ""
        limit = float(argv[4]) if len(argv) > 4 else 540.0
        t0 = time.time()
        while not take(owner, what):
            if time.time() - t0 > limit:
                print("EDITOR_LOCK still held by " + describe(holder()))
                return 1
            time.sleep(10.0)
        print("EDITOR_LOCK taken by %s after %.0f s" % (owner, time.time() - t0))
        return 0
    if cmd == "give":
        ok = give(owner)
        if ok:
            print("EDITOR_LOCK free")
        return 0 if ok else 1
    print(__doc__)
    return 2


if __name__ == "__main__":
    sys.exit(main(sys.argv))
