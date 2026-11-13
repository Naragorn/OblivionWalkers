#!/usr/bin/env python3
"""Lists the dungeons refugees could hold (docs/walkers-spec.md 4.20.7), quest places out.

Reads Oblivion.esm (and any further plugin given after it, for the quest
names only) and writes
  <out>/candidates.tsv  every dungeon with a door in Tamriel: its kind, name,
                        cells, door, arrival point, the room there, the
                        occupants, and why it is out (empty: a candidate)
  <out>/summary.md      the counts by kind and by reason

A dungeon is the set of interior cells reached from one door in the Tamriel
worldspace (0000003C) through doors (XTEL) between interior cells. Its kind
is the type (TNAM) of the map marker named as its first cell (the nearest such),
else of the nearest within --marker-units of that door: 2 cave, 4 Ayleid ruin ("Elven Ruin"), 5 fort, 6 mine; any other or
none, and it is no dungeon of ours. Out of the pool when:
  quest     a quest names one of its references or cells - in its script,
            a stage's result script, a target (QSTA), a condition (CTDA,
            GetInCell and the like) or a dialogue line of it (INFO QSTI) -
            or an object script that runs a quest (SetStage, StartQuest ...)
            names one, or a quest (its own records, not a dialogue line)
            names the record of a person placed once in the world and
            there, or a quest item (flag 0x400) lies in it, or one of its references hangs on an enable parent
            so named; the door outside and its map marker count too
            ... or MOO holds its map marker back until a quest is done
            (--moo-quests), or UESP's place page names a related quest or
            says "quest-related" (--uesp, tools/dungeon-uesp.py)
  owned     a cell of it is owned (XOWN) - someone lives there
  room      fewer than --room-points path grid points (PGRD) within
            --room-units of the arrival point in its first cell
  spots     fewer than --spots-least path grid points of the first cell for
            its people to stand on, or the door inside is no persistent
            reference (the plugin moves them by it)
  several   more than one door from Tamriel leads into it - kept, marked
            (a refuge with two ways in), not out

Record layout as tools/esm-spawns.py (UESP "Oblivion Mod:Mod File Format");
REFR XTEL: door(4) x y z rx ry rz (floats), TNAM: type(1) pad(1) (UESP
"Oblivion Mod:Mod File Format/REFR", read 2026-11-13); XESP: parent(4)
flags(4); PGRD PGRP: x y z (floats) connections(1) pad(3) per point.

    python3 tools/dungeon-candidates.py ~/.local/share/Steam/steamapps/common/Oblivion/Data/Oblivion.esm \\
        --out artifacts/dungeons
"""

from __future__ import annotations

import argparse
import math
import re
import struct
import sys
import zlib
from collections import defaultdict
from pathlib import Path

HEADER = 20
COMPRESSED = 0x00040000
TAMRIEL = 0x0000003C
QUEST_ITEM = 0x00000400
ITEM_TYPES = {b"ARMO", b"CLOT", b"WEAP", b"MISC", b"BOOK", b"INGR", b"ALCH", b"KEYM", b"SGST", b"SLGM",
              b"LIGH", b"AMMO", b"APPA"}
KINDS = {2: "cave", 4: "ayleid", 5: "fort", 6: "mine"}
MARKER_TYPES = {0: "none", 1: "camp", 2: "cave", 3: "city", 4: "ayleid", 5: "fort", 6: "mine", 7: "landmark",
                8: "tavern", 9: "settlement", 10: "shrine", 11: "gate", 12: "door"}
QUEST_CALLS = re.compile(r"\b(setstage|getstage|startquest|stopquest|completequest|getquestrunning|"
                         r"getstagedone|setquestobject)\b", re.I)


def subrecords(data: bytes):
    i = 0
    big = None
    while i + 6 <= len(data):
        typ = data[i:i + 4]
        size = struct.unpack_from("<H", data, i + 4)[0]
        i += 6
        if typ == b"XXXX":
            big = struct.unpack_from("<I", data, i)[0]
            i += size
            continue
        if big is not None:
            size = big
            big = None
        yield typ, data[i:i + size]
        i += size


def u32(data: bytes, at: int = 0) -> int:
    return struct.unpack_from("<I", data, at)[0] if len(data) >= at + 4 else 0


def zstr(data: bytes) -> str:
    return data.split(b"\0", 1)[0].decode("latin-1")


class Plugin:
    """One plugin's records, with the world and cell each lies in. Form IDs
    are made global by the plugin's masters (index -> load position)."""

    def __init__(self, path: Path, order: list[str]):
        self.name = path.name
        self.buf = path.read_bytes()
        size = u32(self.buf, 4)
        masters = [zstr(d) for t, d in subrecords(self.buf[HEADER:HEADER + size]) if t == b"MAST"]
        self.remap = [order.index(m.lower()) if m.lower() in order else 0xFF for m in masters]
        self.own = order.index(self.name.lower())
        self.body = HEADER + size

    def glob(self, form_id: int) -> int:
        index = form_id >> 24
        new = self.remap[index] if index < len(self.remap) else self.own
        return (new << 24) | (form_id & 0xFFFFFF)

    def records(self):
        stack: list[tuple[int, int, int]] = []  # (end, kind, label)
        i = self.body
        end = len(self.buf)
        while i + HEADER <= end:
            while stack and i >= stack[-1][0]:
                stack.pop()
            typ = self.buf[i:i + 4]
            if typ == b"GRUP":
                size, label, gtype = struct.unpack_from("<IIi", self.buf, i + 4)
                stack.append((i + size, gtype, label))
                i += HEADER
                continue
            size, flags, form_id = struct.unpack_from("<III", self.buf, i + 4)
            world = next((self.glob(l) for _, k, l in reversed(stack) if k == 1), 0)
            cell = next((self.glob(l) for _, k, l in reversed(stack) if k == 6), 0)
            raw = self.buf[i + HEADER:i + HEADER + size]
            yield typ, flags, self.glob(form_id), world, cell, raw
            i += HEADER + size


def data_of(flags: int, raw: bytes) -> bytes:
    return zlib.decompress(raw[4:]) if flags & COMPRESSED else raw


def main() -> int:
    p = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    p.add_argument("files", nargs="+", help="Oblivion.esm first, then plugins whose quests count too")
    p.add_argument("--out", default="artifacts/dungeons")
    p.add_argument("--marker-units", type=float, default=2500.0)
    p.add_argument("--room-units", type=float, default=1024.0)
    p.add_argument("--room-points", type=int, default=6)
    p.add_argument("--uesp", help="tools/dungeon-uesp.py's TSV: a dungeon with a related quest or the "
                   "\"quest-related\" mark on its UESP page is out")
    p.add_argument("--chosen-room", type=int, default=10, help="the chosen ones: at least this many path grid "
                   "points at the arrival")
    p.add_argument("--chosen-cells", type=int, default=3, help="the chosen ones: at most this many cells")
    p.add_argument("--chosen-apart", type=float, default=12000.0, help="the chosen ones: doors this far apart")
    p.add_argument("--spot-least", type=float, default=160.0, help="its people's spots: this far from the arrival")
    p.add_argument("--spot-most", type=float, default=2048.0)
    p.add_argument("--spot-apart", type=float, default=110.0)
    p.add_argument("--spots-least", type=int, default=5, help="fewer spots than this: out")
    p.add_argument("--outside-rise", type=float, default=160.0,
                   help="its people's spots outside: the land this near the coming-out point's height")
    p.add_argument("--header", help="also write src/DungeonCandidates.h here: the pool's dungeons, their cells")
    p.add_argument("--moo-quests", help="MOO's \"Maskar's Oblivion Overhaul for quests.ini\": the map markers "
                   "it holds back until a quest is done count as quest places")
    args = p.parse_args()
    order = [Path(f).name.lower() for f in args.files]
    moo_held: dict[int, str] = {}
    uesp: dict[str, str] = {}
    if args.uesp:
        for line in Path(args.uesp).read_text(encoding="utf-8").splitlines()[1:]:
            f = line.split("\t")
            if len(f) >= 3 and (f[1] or f[2]):
                uesp[f[0]] = f[1] or "quest-related"
    if args.moo_quests:
        block: list[str] = []
        for line in Path(args.moo_quests).read_text(encoding="latin-1").splitlines() + [""]:
            if line.strip():
                block.append(line)
                continue
            m = re.search(r'ini_object_marker\s+to GetFormFromMod "Oblivion.esm" "([0-9A-Fa-f]+)"', "\n".join(block))
            q = re.search(r'ini_object_quest\s+to .*?; (.*)', "\n".join(block))
            if m:
                moo_held[int(m.group(1), 16)] = q.group(1).strip() if q else "no quest named"
            block = []

    # The land's heights, for the spots outside (the header only: it is a
    # second pass over Oblivion.esm).
    land = None
    places = None
    if args.header:
        import importlib.util
        here = Path(__file__).resolve().parent
        spec = importlib.util.spec_from_file_location("camp_places", here / "camp-places.py")
        places = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(places)
        land = places.read_land(places.load_esm_module(), Path(args.files[0]).read_bytes())
    cells: dict[int, dict] = {}
    refs: dict[int, dict] = {}
    doors: set[int] = set()
    lists: set[int] = set()
    actors: set[int] = set()
    quest_items: set[int] = set()
    names: dict[int, set[str]] = defaultdict(set)  # form -> who names it
    scripts: dict[int, dict] = {}
    quest_script: dict[int, str] = {}
    grids: dict[int, list[tuple[float, float, float]]] = {}

    for path in args.files:
        plugin = Plugin(Path(path), order)
        main_file = plugin.own == 0
        for typ, flags, form, world, cell, raw in plugin.records():
            if typ in (b"QUST", b"INFO", b"SCPT"):
                subs = list(subrecords(data_of(flags, raw)))
                edid = next((zstr(d) for t, d in subs if t == b"EDID"), f"{form:08X}")
                if typ == b"SCPT":
                    text = next((d.decode("latin-1", "replace") for t, d in subs if t == b"SCTX"), "")
                    scripts[form] = {"edid": edid, "scro": [plugin.glob(u32(d)) for t, d in subs
                                                            if t == b"SCRO" and len(d) >= 4],
                                     "runs_quest": bool(QUEST_CALLS.search(text))}
                    continue
                who = f"QUST {edid}"
                if typ == b"INFO":
                    q = next((plugin.glob(u32(d)) for t, d in subs if t == b"QSTI"), 0)
                    who = f"INFO of {q:08X}"
                for t, d in subs:
                    if t == b"SCRI" and typ == b"QUST":
                        quest_script[plugin.glob(u32(d))] = who
                    elif t in (b"SCRO", b"QSTA") and u32(d):
                        names[plugin.glob(u32(d))].add(who)
                    elif t == b"CTDA" and len(d) >= 20:
                        for at in (12, 16):
                            if u32(d, at):
                                names[plugin.glob(u32(d, at))].add(who)
                continue
            if not main_file:
                continue
            if typ == b"DOOR":
                doors.add(form)
            elif typ == b"LVLC":
                lists.add(form)
            elif typ in (b"NPC_", b"CREA"):
                actors.add(form)
            elif typ in ITEM_TYPES and flags & QUEST_ITEM:
                quest_items.add(form)
            elif typ == b"CELL":
                info = {"form": form, "edid": "", "name": "", "interior": True, "owned": False, "world": world}
                for t, d in subrecords(data_of(flags, raw)):
                    if t == b"EDID":
                        info["edid"] = zstr(d)
                    elif t == b"FULL":
                        info["name"] = zstr(d)
                    elif t == b"DATA" and d:
                        info["interior"] = bool(d[0] & 1)
                    elif t == b"XOWN":
                        info["owned"] = True
                cells[form] = info
            elif typ == b"PGRD":
                pgrp = dict(subrecords(data_of(flags, raw))).get(b"PGRP", b"")
                grids[cell] = [struct.unpack_from("<fff", pgrp, k * 16) for k in range(len(pgrp) // 16)]
            elif typ in (b"REFR", b"ACHR", b"ACRE"):
                r = {"form": form, "type": typ, "cell": cell, "world": world, "base": 0, "pos": None,
                     "persistent": bool(flags & 0x400),
                     "tel": None, "marker": None, "name": "", "parent": 0, "edid": ""}
                for t, d in subrecords(data_of(flags, raw)):
                    if t == b"NAME":
                        r["base"] = plugin.glob(u32(d))
                    elif t == b"EDID":
                        r["edid"] = zstr(d)
                    elif t == b"DATA" and len(d) >= 12:
                        r["pos"] = struct.unpack_from("<fff", d, 0)
                    elif t == b"XTEL" and len(d) >= 16:
                        r["tel"] = (plugin.glob(u32(d)), struct.unpack_from("<fff", d, 4))
                    elif t == b"TNAM" and d:
                        r["marker"] = d[0]
                    elif t == b"FULL":
                        r["name"] = zstr(d)
                    elif t == b"XESP" and len(d) >= 4:
                        r["parent"] = plugin.glob(u32(d))
                refs[form] = r

    # Object scripts that run a quest name things for it; a quest's own script for that quest.
    for form, s in scripts.items():
        who = quest_script.get(form) or (f"SCPT {s['edid']} (runs a quest)" if s["runs_quest"] else None)
        if who:
            for n in s["scro"]:
                names[n].add(who)

    def quest_names(form: int) -> set[str]:
        return {w for w in names.get(form, ()) if w.startswith(("QUST", "INFO", "SCPT"))}

    placed_count: dict[int, int] = defaultdict(int)
    for r in refs.values():
        placed_count[r["base"]] += 1
    by_cell: dict[int, list[dict]] = defaultdict(list)
    for r in refs.values():
        by_cell[r["cell"]].append(r)
    markers = [r for r in refs.values() if r["marker"] is not None and r["world"] == TAMRIEL and r["pos"]]

    def marker_near(x: float, y: float):
        best = None
        for m in markers:
            d = math.hypot(m["pos"][0] - x, m["pos"][1] - y)
            if d <= args.marker_units and (best is None or d < best[0]):
                best = (d, m)
        return best

    def interior(form: int) -> bool:
        c = cells.get(form)
        return bool(c and c["interior"])

    entrances: dict[int, list[dict]] = defaultdict(list)  # first interior cell -> outside doors
    for r in refs.values():
        if r["world"] != TAMRIEL or not r["tel"] or r["base"] not in doors:
            continue
        inside = refs.get(r["tel"][0])
        if inside and interior(inside["cell"]):
            entrances[inside["cell"]].append(r)

    def complex_of(first: int) -> list[int]:
        seen = [first]
        todo = [first]
        while todo:
            c = todo.pop()
            for r in by_cell.get(c, ()):
                if r["tel"]:
                    other = refs.get(r["tel"][0])
                    if other and interior(other["cell"]) and other["cell"] not in seen:
                        seen.append(other["cell"])
                        todo.append(other["cell"])
        return seen

    rows = []
    done: set[int] = set()
    for first, outside in sorted(entrances.items()):
        if first in done:
            continue
        group = complex_of(first)
        done.update(group)
        outside = [d for c in group for d in entrances.get(c, ())]
        door = outside[0]
        near = marker_near(door["pos"][0], door["pos"][1])
        if near is None or near[1]["name"] != cells[first]["name"]:
            # The marker can stand far from the door (Fort Wariel): one of the
            # same name as the first cell wins over a nearer one of another.
            same = [m for m in markers if m["name"] and m["name"] == cells[first]["name"]]
            if same:
                m = min(same, key=lambda m: math.hypot(m["pos"][0] - door["pos"][0], m["pos"][1] - door["pos"][1]))
                near = (math.hypot(m["pos"][0] - door["pos"][0], m["pos"][1] - door["pos"][1]), m)
        kind = KINDS.get(near[1]["marker"]) if near else None
        marker_kind = MARKER_TYPES.get(near[1]["marker"], "?") if near else "none"
        why = []
        if kind is None:
            why.append(f"no dungeon marker ({marker_kind})")
        quests: set[str] = set()
        for c in group:
            quests |= quest_names(c)
            for r in by_cell.get(c, ()):
                quests |= quest_names(r["form"])
                # The thing placed: a person a quest names by his record
                # (a lich of a quest), an item a quest names lying there.
                if r["base"] in quest_items:
                    quests.add(f"quest item {r['base']:08X} lies there")
                elif r["base"] in actors and placed_count[r["base"]] == 1:
                    quests |= {f"{w} (its one {r['type'].decode()})" for w in quest_names(r["base"])
                               if w.startswith("QUST")}
                if r["parent"]:
                    quests |= {f"{w} (enable parent)" for w in quest_names(r["parent"])}
        for r in outside + ([near[1]] if near else []):
            quests |= quest_names(r["form"])
        name0 = (near[1]["name"] if near else "") or cells[first]["name"]
        if name0 in uesp:
            quests.add(f"UESP: {uesp[name0]}")
        if near and near[1]["form"] in moo_held:
            quests.add(f"MOO holds it back ({moo_held[near[1]['form']]})")
        if quests:
            why.append("quest: " + ", ".join(sorted(quests)[:4]) + (" ..." if len(quests) > 4 else ""))
        if any(cells[c]["owned"] for c in group):
            why.append("owned")
        arrival = door["tel"][1]
        points = grids.get(first, [])
        room = sum(1 for x, y, z in points
                   if math.dist((x, y, z), arrival) <= args.room_units)
        if room < args.room_points:
            why.append(f"room ({room} points)")
        # Where its people stand (the kit's 13): path grid points of the first
        # cell, the nearest to the arrival first - off the door's way
        # (--spot-least from the arrival), no farther than --spot-most, each
        # --spot-apart from the others; as offsets from the door inside
        # (the plugin's MoveTo names that door).
        inside = refs.get(door["tel"][0])
        spots = []
        for x, y, z in sorted(points, key=lambda q: math.dist(q, arrival)):
            d = math.dist((x, y, z), arrival)
            if d < args.spot_least or d > args.spot_most:
                continue
            if any(math.dist((x, y, z), q) < args.spot_apart for q in spots):
                continue
            spots.append((x, y, z))
            if len(spots) == 13:
                break
        if not inside or not inside["persistent"] or inside["cell"] != first or not inside["pos"]:
            why.append("the door inside is no persistent reference of the first cell")
        elif len(spots) < args.spots_least:
            why.append(f"spots ({len(spots)} for its people)")
        # Where its people wait outside (a place to clear): a fan before the
        # door outside - from where one comes out of it (the door inside's
        # XTEL) away from the door, four rows of three, on the land's height
        # (LAND, tools/camp-places.py), each within --outside-rise of the
        # coming-out point's height; as offsets from the door outside.
        outside_spots = []
        if inside and inside["tel"] and door["pos"] and land is not None:
            ox, oy, oz = inside["tel"][1]
            ux, uy = ox - door["pos"][0], oy - door["pos"][1]
            length = math.hypot(ux, uy)
            if length >= 20.0:
                ux, uy = ux / length, uy / length
                for row in range(4):
                    for col in (0, -1, 1):
                        x = ox + ux * (220.0 + 110.0 * row) - uy * 120.0 * col
                        y = oy + uy * (220.0 + 110.0 * row) + ux * 120.0 * col
                        h = places.ground_at(land, x, y)
                        if h is not None and abs(h - oz) <= args.outside_rise:
                            outside_spots.append((x - door["pos"][0], y - door["pos"][1], h + 20.0 - door["pos"][2]))
        spawns = sum(1 for c in group for r in by_cell.get(c, ()) if r["base"] in lists)
        placed = sum(1 for c in group for r in by_cell.get(c, ()) if r["base"] in actors)
        name = (near[1]["name"] if near else "") or cells[first]["name"]
        rows.append({
            "first": first, "edid": cells[first]["edid"], "name": name, "kind": kind or marker_kind,
            "cells": len(group), "doors_in": len(outside), "door": door["form"], "x": door["pos"][0],
            "y": door["pos"][1], "arrival": arrival, "room": room, "spawns": spawns, "placed": placed,
            "marker": near[1]["form"] if near else 0, "why": "; ".join(why), "group": group,
            "inside": inside["form"] if inside else 0, "outside": outside_spots,
            "spots": [(x - inside["pos"][0], y - inside["pos"][1], z - inside["pos"][2]) for x, y, z in spots]
            if inside and inside["pos"] else []})

    out = Path(args.out)
    out.mkdir(parents=True, exist_ok=True)
    with open(out / "candidates.tsv", "w", encoding="utf-8") as f:
        f.write("cell\tedid\tname\tkind\tcells\tdoors_in\tdoor\tdoor_x\tdoor_y\tarrival\troom\t"
                "spawns\tplaced\tmarker\tout_because\n")
        for r in sorted(rows, key=lambda r: (r["why"] != "", r["kind"], r["name"])):
            a = r["arrival"]
            f.write(f"{r['first']:08X}\t{r['edid']}\t{r['name']}\t{r['kind']}\t{r['cells']}\t{r['doors_in']}\t"
                    f"{r['door']:08X}\t{r['x']:.0f}\t{r['y']:.0f}\t{a[0]:.0f},{a[1]:.0f},{a[2]:.0f}\t"
                    f"{r['room']}\t{r['spawns']}\t{r['placed']}\t{r['marker']:08X}\t{r['why']}\n")

    dungeons = [r for r in rows if r["kind"] in KINDS.values()]
    pool = [r for r in dungeons if not r["why"]]
    # The chosen ones (the INI's [Dungeons] OnlyChosen): room at the arrival,
    # few cells, spread over the map - the roomiest first, each kind its share.
    share = {"cave": 9, "ayleid": 8, "fort": 9, "mine": 6}
    chosen: list[dict] = []
    for r in sorted(pool, key=lambda r: (-r["room"], r["name"])):
        if r["room"] < args.chosen_room or r["cells"] > args.chosen_cells:
            continue
        if sum(1 for c in chosen if c["kind"] == r["kind"]) >= share[r["kind"]]:
            continue
        if any(math.hypot(r["x"] - c["x"], r["y"] - c["y"]) < args.chosen_apart for c in chosen):
            continue
        chosen.append(r)
    for r in pool:
        r["chosen"] = any(c is r for c in chosen)
    reasons = defaultdict(int)
    for r in dungeons:
        for w in r["why"].split("; ") if r["why"] else []:
            reasons[w.split(":")[0].split(" (")[0]] += 1
    lines = ["# Dungeon candidates (docs/walkers-spec.md 4.20.7)", "",
             f"Read: {', '.join(order)}. Interiors entered from Tamriel: {len(rows)}; "
             f"dungeons by their map marker: {len(dungeons)}; in the pool: {len(pool)}.", "",
             "| kind | dungeons | in the pool |", "|---|---|---|"]
    for k in KINDS.values():
        lines.append(f"| {k} | {sum(1 for r in dungeons if r['kind'] == k)} | "
                     f"{sum(1 for r in pool if r['kind'] == k)} |")
    lines += ["", "Out because (a dungeon may have several):", ""]
    lines += [f"- {w}: {n}" for w, n in sorted(reasons.items(), key=lambda kv: -kv[1])]
    lines += ["", f"Chosen: {len(chosen)} - " + ", ".join(sorted(r["name"] for r in chosen))]
    lines += ["", f"Several doors in (kept): {sum(1 for r in pool if r['doors_in'] > 1)}", ""]
    if args.header:
        enum = {"cave": "Cave", "ayleid": "Ayleid", "fort": "Fort", "mine": "Mine"}
        h = ["#pragma once", "",
             "// The refugee dungeons' candidates (docs/walkers-spec.md 4.20.7), printed by",
             "// tools/dungeon-candidates.py --header. Do not edit: run it again.",
             f"// Read: {', '.join(order)}.",
             f"// {len(pool)} of {len(dungeons)} dungeons with a door in Tamriel; the cells are",
             "// Oblivion.esm form IDs, the first the one the door outside leads into; then",
             "// the door outside, the map marker, whether it is one of the chosen, the door",
             "// outside's x and y, the door inside and its people's spots as offsets from",
             "// that door (path grid points near the arrival), and where they wait outside",
             "// (a fan before the door outside on the land's height, offsets from that door).",
             f"// ({len(chosen)}: at least {args.chosen_room} path grid points at the arrival, at most "
             f"{args.chosen_cells} cells, doors {args.chosen_apart:.0f} apart).", "",
             '#include "DungeonLogic.h"', "", "namespace walkers {", "",
             "inline constexpr DungeonCandidate kDungeonCandidates[] = {"]
        for r in sorted(pool, key=lambda r: r["name"]):
            cells = r["group"][:4] + [0] * (4 - min(4, len(r["group"])))
            if len(r["group"]) > 4:
                raise SystemExit(f"{r['name']}: {len(r['group'])} cells, more than DungeonLogic.h holds")
            name = r["name"].replace("\\", "\\\\").replace('"', '\\"')
            h.append(f'\t{{"{name}", DungeonKind::{enum[r["kind"]]}, {{'
                     + ", ".join(f"0x{c:08X}" for c in cells)
                     + f"}}, {len(r['group'])}, 0x{r['door']:08X}, 0x{r['marker']:08X}, "
                     + ("true" if r["chosen"] else "false") + f", {r['x']:.0f}.0f, {r['y']:.0f}.0f, "
                     + f"0x{r['inside']:08X}, {len(r['spots'])}, {{"
                     + ", ".join(f"{{{x:.0f}, {y:.0f}, {z:.0f}}}" for x, y, z in r["spots"]) + "}, "
                     + f"{len(r['outside'])}, {{"
                     + ", ".join(f"{{{x:.0f}, {y:.0f}, {z:.0f}}}" for x, y, z in r["outside"]) + "}},")
        h += ["};", "", "inline constexpr UInt32 kDungeonCandidateCount =",
              "    static_cast<UInt32>(sizeof kDungeonCandidates / sizeof kDungeonCandidates[0]);", "",
              "}  // namespace walkers", ""]
        Path(args.header).write_text("\n".join(h), encoding="utf-8")
    (out / "summary.md").write_text("\n".join(lines) + "\n", encoding="utf-8")
    print("\n".join(lines))
    return 0


if __name__ == "__main__":
    sys.exit(main())
