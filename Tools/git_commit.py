"""Commit only your own files while other sessions share the working tree and its index (manager, 2026-10-07).

    python Tools/git_commit.py <owner> -m "<message>" [-m "<more>"] -- <path> [<path> ...]

A plain `git commit` takes everything anyone has staged in the shared index (D82's commit took concept's D81 docs). This
takes a short git lock (Saltbound/Saved/GitLock), stages the paths into a private index built from HEAD, so nothing other
sessions staged can slip in, commits, and points the shared index's entries for those paths at the new commit.
Whole files are committed: if another session has edits in one of them, those go along. Leave such a file out or commit
only your hunks (CLAUDE.md, session 8, D70: a blob of HEAD plus your edits) while holding this lock: --lock-only takes it
and prints GIT_LOCK taken, `--unlock <owner>` gives it back.
"""
import os
import subprocess
import sys
import time

ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
LOCK = os.path.join(ROOT, "Saltbound", "Saved", "GitLock")
STALE_S = 300.0


def git(*args, env=None, check=True):
    r = subprocess.run(["git", *args], cwd=ROOT, env=env, capture_output=True, text=True)
    if check and r.returncode != 0:
        raise SystemExit("git %s failed:\n%s%s" % (" ".join(args), r.stdout, r.stderr))
    return r


def lock(owner):
    os.makedirs(os.path.dirname(LOCK), exist_ok=True)
    t0 = time.time()
    while True:
        try:
            os.mkdir(LOCK)
            with open(os.path.join(LOCK, "owner"), "w") as fh:
                fh.write(owner)
            return
        except FileExistsError:
            try:
                if time.time() - os.path.getmtime(LOCK) > STALE_S:
                    unlock(None)
                    continue
            except OSError:
                continue
            if time.time() - t0 > 2 * STALE_S:
                raise SystemExit("GIT_LOCK still held")
            time.sleep(0.2)


def unlock(owner):
    path = os.path.join(LOCK, "owner")
    try:
        if owner is not None and open(path).read() != owner:
            print("GIT_LOCK is not yours")
            return
        os.remove(path)
    except OSError:
        pass
    try:
        os.rmdir(LOCK)
    except OSError:
        pass


def main(argv):
    if len(argv) >= 3 and argv[1] == "--unlock":
        unlock(argv[2])
        print("GIT_LOCK free")
        return 0
    if len(argv) >= 3 and argv[2] == "--lock-only":
        lock(argv[1])
        print("GIT_LOCK taken by " + argv[1])
        return 0
    if len(argv) < 2 or "--" not in argv:
        print(__doc__)
        return 2
    owner = argv[1]
    cut = argv.index("--")
    opts, paths = argv[2:cut], argv[cut + 1:]
    msgs = [opts[i + 1] for i, o in enumerate(opts) if o == "-m" and i + 1 < len(opts)]
    if not msgs or not paths:
        print(__doc__)
        return 2
    lock(owner)
    gitdir = git("rev-parse", "--absolute-git-dir").stdout.strip()
    index = os.path.join(gitdir, "index-" + owner)
    env = dict(os.environ, GIT_INDEX_FILE=index)
    try:
        git("read-tree", "HEAD", env=env)
        git("add", "-A", "--", *paths, env=env)
        if git("diff", "--cached", "--quiet", env=env, check=False).returncode == 0:
            print("NOTHING TO COMMIT in", " ".join(paths))
            return 1
        args = ["commit", "-q"]
        for m in msgs:
            args += ["-m", m]
        git(*args, env=env)
        git("reset", "-q", "--", *paths, check=False)
        print(git("log", "--oneline", "-1").stdout.strip())
        print(git("show", "--stat", "--format=", "HEAD").stdout.rstrip())
        return 0
    finally:
        try:
            os.remove(index)
        except OSError:
            pass
        unlock(owner)


if __name__ == "__main__":
    sys.exit(main(sys.argv))
