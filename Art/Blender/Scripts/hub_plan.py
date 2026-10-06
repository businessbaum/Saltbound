"""The hub ship's rooms (D64): every floor of every deck laid out from the data, on a 0.5 m grid.

Owner (D64): "Build all the decks and rooms, you don't need to design every room. Make the decks cozy and not too big." Outside
the open spaces (hub_data.js OPEN) every floor becomes corridors and rooms (hub.rooms): two corridors fore and aft, corridors
across at fixed stations, rooms of about 5 m between them, every room with a door to a corridor or to the room in front of it.
Where a locked zone's corridor meets an open space a gate stands (its type from the zone).

Pure Python (the editor's Python has no numpy). Ship metres; cell (i, j) covers x X0 + i * CELL .. + CELL, y Y0 + j * CELL ..
Used by the layout review (Tools/design/hub_review.py), the blockout (collision) and the art.

    plan(level_id) -> Plan: .lab {(i, j): label}, .void (cells open to below), .rooms {rid: Room}, .doors, .gates, .walls
    labels: ("water",), ("open", zone id), ("corr", locked zone id), ("room", rid), ("air", locked zone id): open deck under
    the sky (the quarterdeck, the poop, the forecastle deck, the cargo deck)
"""
import math

import hub_layout as H

CELL = 0.5
GATE_ROOM = 1.2             # the open floor a gate needs in front of it (m): where a player stands to try it
X0, X1, Y0, Y1 = -3.0, 163.0, -21.0, 21.0
NX = int(round((X1 - X0) / CELL))
NY = int(round((Y1 - Y0) / CELL))
RM = H.HUB["rooms"]
LINKS = {}
for _a, _b, _how in H.HUB.get("links", []):
    LINKS[(_a, _b)] = LINKS[(_b, _a)] = _how


def cx(i):
    return X0 + (i + 0.5) * CELL


def cy(j):
    return Y0 + (j + 0.5) * CELL


def cell_of(x, y):
    return int(math.floor((x - X0) / CELL)), int(math.floor((y - Y0) / CELL))


class Room:
    def __init__(self, rid, zone, band):
        self.rid, self.zone, self.band = rid, zone, band
        self.cells = set()
        self.door = None            # (x, y, axis, to label)

    def bounds(self):
        xs = [cx(i) for i, _ in self.cells]
        ys = [cy(j) for _, j in self.cells]
        return min(xs) - CELL / 2, max(xs) + CELL / 2, min(ys) - CELL / 2, max(ys) + CELL / 2


class Plan:
    """One floor's layout."""

    def __init__(self, level_id):
        self.level = level_id
        self.z = H.level_z(level_id)
        self.lab = {}
        self.void = set()
        self.slot = set()           # cells where something else is the floor (the slipway): no floor, no walls round
        self.walk = set()           # corridor cells that are walkways in view (the Water Hall's galleries)
        self.hole = {}              # stair holes' cells: the edge the stair arrives over (axis, line), left open
        self.rooms = {}
        self.doors = []             # dicts: x, y, axis ('x': the wall runs along y at that x; 'y': along x), w, kind, a, b
        self.gates = []             # gate marks: level, x, y, g, zone, open (the open space it faces)
        self.gates_dropped = []     # corridors' mouths left walled: a stair in front of them
        self.walls = []             # segments: axis, line, a0, a1, kind ('wall' | 'rail' | 'hull'), left, right labels
        self._build()

    # --- cells ---------------------------------------------------------------------------------------------------------
    def _build(self):
        lv = H.LEVELS[self.level]
        only = [H.R(r) for r in lv.get("only", [])]
        voids = [H.R(r) for r in lv.get("voids", [])]
        holes = stair_holes(self.level)
        walks = [H.R(r) for w in RM.get("walks", []) if self.level in w["levels"] for r in w["rects"]]
        water = [H.R(r) for r in lv.get("water", [])]
        slots = [H.R(r) for r in lv.get("slots", [])]
        opens = [(o["id"], H.R(r)) for o in H.OPEN if o["level"] == self.level and not o.get("inside") for r in o["rects"]]
        locked = [(z["id"], H.R(r)) for z in H.LOCKED for r in z["levels"].get(self.level, [])]
        air = [(z["id"], H.R(r)) for z in H.LOCKED for r in z.get("air", {}).get(self.level, [])]
        s0, s1 = RM["spine"]
        cw = RM["cross_w"] * 0.5
        half = {}
        for i in range(NX):
            x = cx(i)
            half[i] = H.floor_half(self.level, x)
        for i in range(NX):
            x = cx(i)
            hw = half[i]
            if hw <= 0.0:
                continue
            for j in range(NY):
                y = cy(j)
                if abs(y) >= hw:
                    continue
                if only and not any(H.inside(r, x, y) for r in only):
                    continue
                if any(H.inside(r, x, y) for r in slots):
                    self.slot.add((i, j))
                    continue
                hole = next((h for h in holes if H.inside(h[0], x, y)), None)
                if hole:
                    self.void.add((i, j))
                    self.hole[(i, j)] = hole[1]
                    continue
                if any(H.inside(r, x, y) for r in voids):
                    self.void.add((i, j))
                    continue
                if any(H.inside(r, x, y) for r in water):
                    self.lab[(i, j)] = ("water",)
                    continue
                o = next((zid for zid, r in opens if H.inside(r, x, y)), None)
                if o:
                    self.lab[(i, j)] = ("open", o)
                    continue
                a = next((zid for zid, r in air if H.inside(r, x, y)), None)
                if a:
                    self.lab[(i, j)] = ("air", a)
                    continue
                zone = next((zid for zid, r in locked if H.inside(r, x, y)), "rest")
                walk = any(H.inside(r, x, y) for r in walks)
                corr = s0 <= abs(y) < s1 or any(abs(x - xk) < cw for xk in RM["cross"]) or walk
                self.lab[(i, j)] = ("corr", zone) if corr else ("todo", zone)
                if walk:
                    self.walk.add((i, j))
        self._rooms(half)
        self._prune_corridors()
        self._doors()
        self._gates()
        self._walls()
        self._hoist_gap()

    def _rooms(self, half):
        """The cells between the corridors: blocks by zone, band (outboard starboard, centre either side, outboard port) and
        the stretch between corridors across; each block cut into rooms of about 5 m, deep outboard blocks once more across."""
        s0, s1 = RM["spine"]
        lo, hi = RM["room"]
        target = 0.5 * (lo + hi)
        todo = {c for c, l in self.lab.items() if l[0] == "todo"}
        seen = set()
        rid = 0
        for start in sorted(todo):
            if start in seen:
                continue
            zone = self.lab[start][1]
            band = self._band(cy(start[1]))
            # The block: connected cells of the same zone and band.
            stack, block = [start], set()
            seen.add(start)
            while stack:
                c = stack.pop()
                block.add(c)
                for n in ((c[0] + 1, c[1]), (c[0] - 1, c[1]), (c[0], c[1] + 1), (c[0], c[1] - 1)):
                    if n in todo and n not in seen and self.lab[n][1] == zone and self._band(cy(n[1])) == band:
                        seen.add(n)
                        stack.append(n)
            xs = [cx(i) for i, _ in block]
            bx0, bx1 = min(xs) - CELL / 2, max(xs) + CELL / 2
            n = max(1, int(round((bx1 - bx0) / target)))
            step = (bx1 - bx0) / n
            split = band in ("S", "P")
            for c in block:
                k = min(int((cx(c[0]) - bx0) / step), n - 1)
                outer = False
                if split:
                    depth = half[c[0]] - s1
                    if depth > RM["deep"]:
                        outer = abs(cy(c[1])) >= s1 + depth * 0.5
                key = (zone, band, start, k, outer)
                room = self.rooms.get(key)
                if room is None:
                    rid += 1
                    room = self.rooms[key] = Room(rid, zone, band + ("o" if outer else ""))
                room.cells.add(c)
        # Re-key by rid, label the cells; slivers (under 2 m2) join a neighbour.
        rooms = {}
        for room in self.rooms.values():
            rooms[room.rid] = room
            for c in room.cells:
                self.lab[c] = ("room", room.rid)
        for room in sorted(rooms.values(), key=lambda r: len(r.cells)):
            if len(room.cells) * CELL * CELL >= 4.0:
                continue
            others = [self.lab.get(n) for c in room.cells for n in self._nbrs(c)]
            others = [l for l in others if l and l[0] == "room" and l[1] != room.rid and rooms[l[1]].cells]
            others.sort(key=lambda l: rooms[l[1]].zone != room.zone)
            if others:
                other = rooms[others[0][1]]
                for cc in room.cells:
                    self.lab[cc] = ("room", other.rid)
                other.cells |= room.cells
            else:
                for cc in room.cells:
                    self.lab[cc] = ("corr", room.zone)
            room.cells = set()
        self.rooms = {k: r for k, r in rooms.items() if r.cells}

    @staticmethod
    def _band(y):
        s0, s1 = RM["spine"]
        if y >= s1:
            return "S"
        if y <= -s1:
            return "P"
        return "C+" if y >= 0.0 else "C-"

    @staticmethod
    def _nbrs(c):
        return ((c[0] + 1, c[1]), (c[0] - 1, c[1]), (c[0], c[1] + 1), (c[0], c[1] - 1))

    def _prune_corridors(self):
        """Corridor cells that lead nowhere (no room or other corridor beside them, stubs under 1.5 m) become room."""
        changed = True
        while changed:
            changed = False
            for c, l in list(self.lab.items()):
                if l[0] != "corr":
                    continue
                ways = [n for n in self._nbrs(c) if self.lab.get(n, ("x",))[0] in ("corr", "open")]
                if len(ways) <= 1:
                    nb = [self.lab.get(n) for n in self._nbrs(c)]
                    room = next((x for x in nb if x and x[0] == "room"), None)
                    if room:
                        self.lab[c] = room
                        self.rooms[room[1]].cells.add(c)
                        changed = True

    # --- doors and gates -----------------------------------------------------------------------------------------------
    def _shared_edges(self, cells, want):
        """Edges between `cells` and neighbours whose label passes want(label): (x, y of the edge's middle, axis, label)."""
        out = []
        for c in cells:
            for n, axis in (((c[0] + 1, c[1]), "x"), ((c[0] - 1, c[1]), "x"), ((c[0], c[1] + 1), "y"), ((c[0], c[1] - 1), "y")):
                l = self.lab.get(n)
                if l is None or not want(l):
                    continue
                if axis == "x":
                    out.append((X0 + max(c[0], n[0]) * CELL, cy(c[1]), "x", l))
                else:
                    out.append((cx(c[0]), Y0 + max(c[1], n[1]) * CELL, "y", l))
        return out

    @staticmethod
    def _middle_run(edges):
        """The middle edge of the longest straight run of edges on one line (a door wants a straight piece of wall)."""
        lines = {}
        for e in edges:
            key = (e[2], round(e[0], 3) if e[2] == "x" else round(e[1], 3))
            lines.setdefault(key, []).append(e)
        best = None
        for (axis, _), es in lines.items():
            es.sort(key=lambda e: e[1] if axis == "x" else e[0])
            run = [es[0]]
            for e in es[1:]:
                prev = run[-1]
                gap = (e[1] - prev[1]) if axis == "x" else (e[0] - prev[0])
                if gap <= CELL + 1e-6:
                    run.append(e)
                else:
                    if best is None or len(run) > len(best):
                        best = run
                    run = [e]
            if best is None or len(run) > len(best):
                best = run
        return best[len(best) // 2] if best else None

    def _doors(self):
        dw = RM["door"]
        for room in self.rooms.values():
            # To its own zone's corridor first, then to the room in front of it (outer rooms), then any corridor.
            choices = (lambda l: l[0] == "corr" and l[1] == room.zone,
                       lambda l: l[0] == "room" and l[1] != room.rid and self.rooms[l[1]].zone == room.zone and not self.rooms[l[1]].band.endswith("o"),
                       lambda l: l[0] == "corr")
            for want in choices:
                e = self._middle_run(self._shared_edges(room.cells, want))
                if e and self._run_len(room.cells, want, e) >= dw:
                    room.door = e
                    self.doors.append({"x": e[0], "y": e[1], "axis": e[2], "w": dw, "kind": "room", "a": ("room", room.rid), "b": e[3]})
                    break
        # Where two zones' corridors meet: a door between them.
        done = set()
        for c, l in self.lab.items():
            if l[0] != "corr":
                continue
            for n in self._nbrs(c):
                m = self.lab.get(n)
                if m and m[0] == "corr" and m[1] != l[1]:
                    key = tuple(sorted((l[1], m[1])))
                    if key in done:
                        continue
                    edges = self._shared_edges({cc for cc, ll in self.lab.items() if ll == l}, lambda x, m=m: x == m)
                    e = self._middle_run(edges)
                    if e:
                        done.add(key)
                        self.doors.append({"x": e[0], "y": e[1], "axis": e[2], "w": 1.2, "kind": "zone", "a": l, "b": m})
        # Open spaces linked by a door.
        for (a, b), how in LINKS.items():
            if how != "door" or a > b:
                continue
            cells = {c for c, l in self.lab.items() if l == ("open", a)}
            e = self._middle_run(self._shared_edges(cells, lambda l, b=b: l == ("open", b)))
            if e:
                self.doors.append({"x": e[0], "y": e[1], "axis": e[2], "w": 1.2, "kind": "link", "a": ("open", a), "b": ("open", b)})

    def _run_len(self, cells, want, e):
        es = [x for x in self._shared_edges(cells, want) if x[2] == e[2] and abs((x[0] if e[2] == "x" else x[1]) - (e[0] if e[2] == "x" else e[1])) < 1e-6]
        return len(es) * CELL

    def _gates(self):
        """Where a locked zone's corridor meets an open space: one gate per opening (contiguous edges on one line)."""
        zones = {z["id"]: z for z in H.LOCKED}
        edges = {}
        for c, l in self.lab.items():
            if l[0] != "corr":
                continue
            for e in self._shared_edges({c}, lambda m: m[0] == "open"):
                key = (l[1], e[3][1], e[2], round(e[0], 3) if e[2] == "x" else round(e[1], 3))
                edges.setdefault(key, []).append(e)
        # A flight up from this floor (or the dock stair) in front of a corridor's mouth leaves no room to stand at a gate there:
        # the corridor ends in a wall instead (D64: the grille behind the Keelgate's stair, the rope behind the middle flight).
        stairs = [H.R(f) for f in H.flights() if f["from"] == self.level]
        if self.level == "quay":
            stairs.append(H.R(H.HUB["stair"]))
        for (zone, open_id, axis, _), es in edges.items():
            es.sort(key=lambda e: e[1] if axis == "x" else e[0])
            runs, run = [], [es[0]]
            for e in es[1:]:
                gap = (e[1] - run[-1][1]) if axis == "x" else (e[0] - run[-1][0])
                if gap <= CELL + 1e-6:
                    run.append(e)
                else:
                    runs.append(run)
                    run = [e]
            runs.append(run)
            for run in runs:
                mid = run[len(run) // 2]
                fronts = [(mid[0] + s * GATE_ROOM, mid[1]) if axis == "x" else (mid[0], mid[1] + s * GATE_ROOM) for s in (-1.0, 1.0)]
                if any(H.inside(r, fx, fy) for r in stairs for fx, fy in fronts):
                    self.gates_dropped.append((self.level, zone, mid[0], mid[1]))
                    continue
                g = zones.get(zone, {}).get("gate", "G1")
                if g in ("G3", "G5"):
                    g = "G1"
                self.gates.append({"level": self.level, "x": round(mid[0], 2), "y": round(mid[1], 2), "g": g, "zone": zone, "open": open_id,
                                   "axis": axis, "w": len(run) * CELL})

    # --- walls ---------------------------------------------------------------------------------------------------------
    def _kind(self, a, b):
        """What stands between two neighbouring cells' labels (None: nothing)."""
        if a == b:
            return None
        if a is None or b is None:
            other = b if a is None else a
            if other[0] == "water":
                return None
            return "edge"
        if a[0] == "water" or b[0] == "water":
            return None if (a[0] == "open" or b[0] == "open") else "wall"
        if a[0] == "open" and b[0] == "open":
            how = LINKS.get((a[1], b[1]))
            return None if how == "open" else "wall"
        if {a[0], b[0]} == {"air", "open"}:
            return None                     # deck meets deck: the gate there stands across it (the cargo rope)
        return "wall"

    def _walls(self):
        """Wall runs along cell edges where labels differ (merged on each line). kind 'wall', or 'edge' where a floor ends:
        at the hull (hull) or over a void (rail for open spaces, wall for rooms)."""
        segs = {}
        for i in range(NX + 1):
            for j in range(NY):
                a, b = self._label_at(i - 1, j), self._label_at(i, j)
                k = self._edge_kind(a, b, (i - 1, j), (i, j))
                if k:
                    segs.setdefault(("x", X0 + i * CELL, k, a, b), []).append(Y0 + j * CELL)
        for j in range(NY + 1):
            for i in range(NX):
                a, b = self._label_at(i, j - 1), self._label_at(i, j)
                k = self._edge_kind(a, b, (i, j - 1), (i, j))
                if k:
                    segs.setdefault(("y", Y0 + j * CELL, k, a, b), []).append(X0 + i * CELL)
        for (axis, line, kind, a, b), starts in segs.items():
            starts.sort()
            s = starts[0]
            prev = s
            for t in starts[1:] + [None]:
                if t is not None and abs(t - prev - CELL) < 1e-6:
                    prev = t
                    continue
                self.walls.append({"axis": axis, "line": line, "a0": s, "a1": prev + CELL, "kind": kind, "left": a, "right": b})
                if t is not None:
                    s = prev = t

    def _hoist_gap(self):
        """The hoist's stops (spaces.well.hoist): its cage's door (aft) opens through the shaft's rail there."""
        if self.level not in H.prop("well", "hoist")["stops"]:
            return
        hs, x0 = H.R(H.HUB["hoist"]), H.HUB["shaft"]["x0"]
        out = []
        for w in self.walls:
            if w["kind"] == "rail" and w["axis"] == "x" and abs(w["line"] - x0) < 1e-6 and w["a0"] < hs[3] and w["a1"] > hs[2]:
                out += [dict(w, a0=a, a1=b) for a, b in ((w["a0"], hs[2]), (hs[3], w["a1"])) if b - a > 1e-6]
            else:
                out.append(w)
        self.walls = out

    def _label_at(self, i, j):
        if (i, j) in self.slot:
            return ("slot",)
        if (i, j) in self.void:
            return ("void",)
        return self.lab.get((i, j))

    def _edge_kind(self, a, b, ca, cb):
        if a == b:
            return None
        if a == ("slot",) or b == ("slot",):
            return None
        if a is None and b is None:
            return None
        if a == ("void",) or b == ("void",):
            other = b if a == ("void",) else a
            if other is None or other == ("void",):
                return None
            hc = ca if a == ("void",) else cb
            if hc in self.hole:
                ax_, ln = self.hole[hc]
                edge = ("x", X0 + max(ca[0], cb[0]) * CELL) if ca[1] == cb[1] else ("y", Y0 + max(ca[1], cb[1]) * CELL)
                if edge[0] == ax_ and abs(edge[1] - ln) < CELL * 0.5 + 1e-6:
                    return None                     # where the stair arrives: open (the hole's cell edge at the stair's end)
                return "rail" if other[0] in ("open", "air") else "wall"     # (a locked corridor or room behind it: walled)
            if other[0] == "water":
                return None
            if other[0] == "open":
                o = next((z for z in H.OPEN if z["id"] == other[1]), {})
                return o.get("void_edge", "rail")
            if other[0] == "corr" and H.LEVELS[self.level].get("void_edge") == "wall":
                return "wall"                       # (the hold round the slipway: walled, the ramp is walkable)
            return "rail" if other[0] in ("air", "corr") else "wall"
        if a is None or b is None:
            other = b if a is None else a
            if other[0] == "water":
                return None
            if other[0] == "open" and next((z for z in H.OPEN if z["id"] == other[1]), {}).get("weather"):
                return "bulwark"                    # D69: an open deck under the sky (the quarterdeck, the poop): the bulwark
            return "bulwark" if other[0] in ("air",) or (other[0] == "open" and H.LEVELS[self.level].get("deck")) else "hull"
        return self._kind(a, b)

    # --- for the drawings and the builds ----------------------------------------------------------------------------------
    def area(self, kind):
        return sum(1 for l in self.lab.values() if l[0] == kind) * CELL * CELL


def stair_holes(level_id):
    """The holes stairs come up through on this floor: the Well's flights arriving here, the dock stair (HUB.stair, up to the
    lower deck)."""
    def arrive(r, up):
        return {"-x": ("x", r[0]), "+x": ("x", r[1]), "-y": ("y", r[2]), "+y": ("y", r[3])}[up]
    out = [(H.R(f), arrive(H.R(f), f["up"])) for f in H.flights() if f["to"] == level_id]
    if level_id == "lower":
        out.append((H.R(H.HUB["stair"]), arrive(H.R(H.HUB["stair"]), "+x")))
    return out


_CACHE = {}


def plan(level_id):
    if level_id not in _CACHE:
        _CACHE[level_id] = Plan(level_id)
    return _CACHE[level_id]


def rects_of(cells):
    """Cells (i, j) merged into rectangles (runs along y, then equal runs stacked along x): [(x0, x1, y0, y1)]."""
    rows = {}
    for i, j in cells:
        rows.setdefault(i, []).append(j)
    runs = {}
    for i, js in rows.items():
        js.sort()
        s = p = js[0]
        for j in js[1:] + [None]:
            if j is not None and j == p + 1:
                p = j
                continue
            runs.setdefault((s, p), []).append(i)
            if j is not None:
                s = p = j
    out = []
    for (j0, j1), iis in runs.items():
        iis.sort()
        s = p = iis[0]
        for i in iis[1:] + [None]:
            if i is not None and i == p + 1:
                p = i
                continue
            out.append((X0 + s * CELL, X0 + (p + 1) * CELL, Y0 + j0 * CELL, Y0 + (j1 + 1) * CELL))
            if i is not None:
                s = p = i
    return out


def open_floor_rects(level_id):
    """[(zone id, (x0, x1, y0, y1))] of the open floors as the layout has them (inside the hull, round holes and voids)."""
    pl = plan(level_id)
    zones = {}
    for c, lab in pl.lab.items():
        if lab[0] == "open":
            zones.setdefault(lab[1], []).append(c)
    return [(z, r) for z, cells in sorted(zones.items()) for r in rects_of(cells)]


def all_gate_marks():
    """Every gate: those the layout places (corridor mouths) and the data's own (GATE_MARKS)."""
    out = []
    for lv in H.LEVELS:
        out += plan(lv).gates
    return out + list(H.GATE_MARKS)
