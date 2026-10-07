"""One editor, several agents: a lock with a fair queue round every use of the running Unreal Editor.

Several agent sessions share one editor and one working tree. Each takes the lock before anything that uses the editor
(ue_exec.py, ue_mcp.py captures, imports, PIE tests, loading or saving maps, Rebuild-Editor.ps1) and gives it back as soon
as that step is done and the map is saved. Blender headless builds, host Python, C++ written aside and docs need no lock.

    python Tools/editor_lock.py take <owner> "<what>"      exit 0 taken, 1 busy (someone holds it or is ahead in the queue)
    python Tools/editor_lock.py wait <owner> "<what>" [s]  queues (first come, first served) and waits up to s seconds
                                                           (default 570; call it with a Bash timeout of 600000)
    python Tools/editor_lock.py give <owner>               gives it back (only its owner can) and names who is next
    python Tools/editor_lock.py status                     holder and queue
    python Tools/editor_lock.py decision <owner> "<topic>" claims the next free decision number (D79, D80, ...)
    python Tools/editor_lock.py break manager              frees a lock whose holder crashed (the managing session only)
    python Tools/editor_lock.py done <owner>               before a shutdown: saved, committed, idle (taking the lock clears it)

Rules (the managing session, 2026-10-07): sessions do not message each other; this tool is the only coordination.
Do non-editor work while waiting instead of polling. Never hold the lock while writing code, building in Blender or
writing docs. Test efficiently (owner): prepare scripts, C++ and assets before queuing, run all checks of a hold in one
script, test only what the change touches, never debug while holding, and aim for holds under 10 minutes.

Everything lives under Saltbound/Saved (not in git). The lock is a directory (mkdir is atomic).
"""
import json
import os
import re
import sys
import time

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
SAVED = os.path.join(ROOT, "Saltbound", "Saved")
LOCK = os.path.join(SAVED, "EditorLock")
INFO = os.path.join(LOCK, "owner.json")
QUEUE = os.path.join(SAVED, "EditorQueue")
CLAIMS = os.path.join(SAVED, "DecisionClaims.json")
CLAIMS_LOCK = os.path.join(SAVED, "DecisionClaims.lock")
DECISIONS = os.path.join(ROOT, "Docs", "Decisions.md")
DONE = os.path.join(SAVED, "SessionsDone")

QUEUE_STALE_S = 180.0   # a waiter whose wait call ended this long ago has left the queue
POLL_S = 5.0
FAIR_HOLD_MIN = 10.0


def read_json(path):
    try:
        with open(path, encoding="utf-8") as fh:
            return json.load(fh)
    except (OSError, ValueError):
        return None


def write_json(path, data):
    tmp = path + ".tmp"
    with open(tmp, "w", encoding="utf-8") as fh:
        json.dump(data, fh, indent=1)
    os.replace(tmp, path)


def holder():
    h = read_json(INFO)
    if h is None and os.path.isdir(LOCK):
        return {"owner": "?", "what": "?", "since": 0}
    return h


def describe(h):
    return "%s (%s) for %.1f min" % (h.get("owner"), h.get("what"), (time.time() - h.get("since", 0)) / 60.0)


# --- queue: one file per waiting owner; its 'since' keeps the place, its 'seen' shows the waiter is still there ---

def queue_path(owner):
    return os.path.join(QUEUE, re.sub(r"[^A-Za-z0-9_-]", "_", owner) + ".json")


def queued():
    """Live waiters, oldest first; stale entries are removed."""
    out = []
    if not os.path.isdir(QUEUE):
        return out
    now = time.time()
    for name in os.listdir(QUEUE):
        path = os.path.join(QUEUE, name)
        e = read_json(path) if name.endswith(".json") else None
        if not e:
            continue
        if now - e.get("seen", 0) > QUEUE_STALE_S:
            try:
                os.remove(path)
            except OSError:
                pass
            continue
        out.append(e)
    return sorted(out, key=lambda e: e.get("since", 0))


def enqueue(owner, what):
    os.makedirs(QUEUE, exist_ok=True)
    path = queue_path(owner)
    e = read_json(path)
    now = time.time()
    if not e or now - e.get("seen", 0) > QUEUE_STALE_S:
        e = {"owner": owner, "what": what, "since": now}
    e["what"], e["seen"] = what, now
    write_json(path, e)


def dequeue(owner):
    try:
        os.remove(queue_path(owner))
    except OSError:
        pass


def describe_queue(q):
    return ", ".join("%s (%s, %.0f min)" % (e["owner"], e.get("what", ""), (time.time() - e["since"]) / 60.0) for e in q)


def first_in_line(owner):
    q = queued()
    return not q or q[0]["owner"] == owner


# --- the lock ---

def take(owner, what):
    """True if the owner holds the lock now (taken, or held already)."""
    h = holder()
    if h is not None:
        return h.get("owner") == owner
    if not first_in_line(owner):
        return False
    os.makedirs(SAVED, exist_ok=True)
    try:
        os.mkdir(LOCK)
    except FileExistsError:
        return False
    write_json(INFO, {"owner": owner, "what": what, "since": time.time()})
    dequeue(owner)
    try:
        os.remove(os.path.join(DONE, re.sub(r"[^A-Za-z0-9_-]", "_", owner) + ".json"))
    except OSError:
        pass
    return True


def give(owner):
    h = holder()
    if h is None:
        return True
    if h.get("owner") != owner and owner != "manager":
        print("EDITOR_LOCK held by", describe(h), "- not yours")
        return False
    try:
        os.remove(INFO)
    except OSError:
        pass
    try:
        os.rmdir(LOCK)
    except OSError:
        pass
    return True


def report_taken(owner, waited=None):
    msg = "EDITOR_LOCK taken by " + owner
    if waited is not None:
        msg += " after %.0f s" % waited
    print(msg)
    print("  one prepared script, only the checks your change needs, no debugging while holding; aim for < %.0f min" % FAIR_HOLD_MIN)
    others = [e for e in queued() if e["owner"] != owner]
    if others:
        print("  waiting: %s - give it back within %.0f min, then queue again" % (describe_queue(others), FAIR_HOLD_MIN))


def report_busy(owner):
    h = holder()
    if h is not None:
        print("EDITOR_LOCK held by " + describe(h))
    q = queued()
    if q:
        print("  queue: " + describe_queue(q))
    print("  do non-editor work meanwhile, then: python Tools/editor_lock.py wait %s \"<what>\"" % owner)


# --- decision numbers ---

def claim_decision(owner, topic):
    t0 = time.time()
    while True:
        try:
            os.mkdir(CLAIMS_LOCK)
            break
        except FileExistsError:
            if time.time() - t0 > 30:
                os.rmdir(CLAIMS_LOCK)   # a crashed claimer; the claim itself is atomic below
            time.sleep(0.2)
    try:
        claims = read_json(CLAIMS) or {}
        top = 0
        try:
            with open(DECISIONS, encoding="utf-8") as fh:
                top = max([int(n) for n in re.findall(r"^#+\s*D(\d+)", fh.read(), re.M)] or [0])
        except OSError:
            pass
        top = max([top] + [int(k[1:]) for k in claims])
        key = "D%d" % (top + 1)
        claims[key] = {"owner": owner, "topic": topic, "since": time.time()}
        write_json(CLAIMS, claims)
        return key
    finally:
        os.rmdir(CLAIMS_LOCK)


def main(argv):
    if len(argv) < 2:
        print(__doc__)
        return 2
    cmd = argv[1]
    if cmd == "status":
        h = holder()
        print("EDITOR_LOCK free" if h is None else "EDITOR_LOCK held by " + describe(h))
        q = queued()
        if q:
            print("  queue: " + describe_queue(q))
        claims = read_json(CLAIMS) or {}
        if claims:
            print("  decisions claimed: " + ", ".join("%s %s (%s)" % (k, v["owner"], v["topic"]) for k, v in
                                                      sorted(claims.items(), key=lambda kv: int(kv[0][1:]))))
        done = sorted(os.listdir(DONE)) if os.path.isdir(DONE) else []
        if done:
            print("  done (saved, committed, idle): " + ", ".join(d[:-5] for d in done if d.endswith(".json")))
        return 0
    if cmd == "done" and len(argv) > 2:
        # Before a shutdown: a session marks itself done once everything is saved and committed; taking the lock clears it.
        os.makedirs(DONE, exist_ok=True)
        write_json(os.path.join(DONE, re.sub(r"[^A-Za-z0-9_-]", "_", argv[2]) + ".json"), {"since": time.time()})
        print("DONE " + argv[2])
        return 0
    if len(argv) < 3:
        print(__doc__)
        return 2
    owner = argv[2]
    what = argv[3] if len(argv) > 3 else ""
    if cmd == "take":
        if take(owner, what):
            report_taken(owner)
            return 0
        report_busy(owner)
        return 1
    if cmd == "wait":
        limit = float(argv[4]) if len(argv) > 4 else 570.0
        t0 = time.time()
        while True:
            if holder() is None or holder().get("owner") != owner:
                enqueue(owner, what)
            if take(owner, what):
                report_taken(owner, time.time() - t0)
                return 0
            if time.time() - t0 > limit:
                print("EDITOR_LOCK not yet yours after %.0f s (your place in the queue is kept for %.0f s)"
                      % (time.time() - t0, QUEUE_STALE_S))
                report_busy(owner)
                return 1
            time.sleep(POLL_S)
    if cmd == "give":
        if not give(owner):
            return 1
        q = queued()
        print("EDITOR_LOCK free" + ("; next: " + q[0]["owner"] if q else ""))
        return 0
    if cmd == "break":
        if owner != "manager":
            print("only the managing session breaks a lock")
            return 1
        h = holder()
        give("manager")
        print("EDITOR_LOCK broken" + (" (was " + describe(h) + ")" if h else ""))
        return 0
    if cmd == "decision":
        print("DECISION %s claimed by %s (%s)" % (claim_decision(owner, what), owner, what))
        return 0
    print(__doc__)
    return 2


if __name__ == "__main__":
    sys.exit(main(sys.argv))
