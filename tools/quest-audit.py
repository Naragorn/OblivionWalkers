#!/usr/bin/env python3
"""Measures which quests the spawn replacement can break (docs/walkers-spec.md 7a).

The replacement turns leveled-list creatures and people into walkers by
rewriting the lists' entries (Replace.cpp), per base form, not per placed
reference. It keeps a base with a script of its own and one that carries a
quest-flagged item (ReplaceLogic.h ClassifyCreature). This tool reads the
load order's plugins and lists what that leaves open, by quest:

  gap 1  an item a quest needs - named by a quest, or scripted - that is not
         quest-flagged, carried by a base in a leveled list the rules do not
         keep (SQ06's bear fang the known case)
  gap 2  a placed reference whose base is a leveled list, named by a quest
         (a script, a stage, a target, a condition) or persistent - a "kill
         this one" boss like SQ03's SQ3BossRef
  gap 3  a base form or a leveled list named by a quest (GetDeadCount and
         the like), a base in a leveled list the rules do not keep

"Named by a quest": the form ID stands in a quest's own script (SCRO), its
stage result scripts, its targets (QSTA), its conditions (CTDA), or in a
dialogue line of that quest (INFO with QSTI). Other object scripts' names
count as "named by script <EDID>".

What it does not see: names built at run time (a script that walks an
inventory), leveled item lists in inventories (LVLI, counted, not opened),
and which replacement category takes a base - every leveled-list base is
taken as replaced unless kept by the two rules above. So the counts are an
upper bound for gap 3 and gap 1.

    python3 tools/quest-audit.py <Data> <Plugins.txt> --out artifacts/quest-audit

Record layout as tools/esm-spawns.py (UESP "Oblivion Mod:Mod File Format").
CTDA: type(1) pad(3) value(4) function(4) param1(4) param2(4) [pad(4)].
QSTA: target(4) flags(4). CNTO: item(4) count(4). SCHR: pad(4) refCount(4)
compiledSize(4) varCount(4) type(2) - type 0 object, 1 quest, 0x100 effect.
"""

from __future__ import annotations

import argparse
import struct
import sys
import zlib
from collections import defaultdict
from pathlib import Path

HEADER = 20
COMPRESSED = 0x00040000
PERSISTENT = 0x00000400
QUEST_ITEM = 0x00000400
ITEM_TYPES = {b"ARMO", b"CLOT", b"WEAP", b"MISC", b"BOOK", b"INGR", b"ALCH", b"KEYM", b"SGST", b"SLGM",
              b"LIGH", b"AMMO", b"APPA"}
ACTOR_TYPES = {b"CREA", b"NPC_"}
WANTED = ITEM_TYPES | ACTOR_TYPES | {b"LVLC", b"LVLI", b"SCPT", b"QUST", b"INFO", b"ACRE", b"ACHR", b"REFR"}


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


class Plugin:
    def __init__(self, name: str, buf: bytes):
        self.name = name
        self.buf = buf
        self.masters: list[str] = []
        size = struct.unpack_from("<I", buf, 4)[0]
        for typ, data in subrecords(buf[HEADER:HEADER + size]):
            if typ == b"MAST":
                self.masters.append(data.rstrip(b"\0").decode("latin-1"))
        self.body = HEADER + size

    def key(self, form_id: int) -> tuple[str, int]:
        index = form_id >> 24
        owner = self.masters[index] if index < len(self.masters) else self.name
        return owner.lower(), form_id & 0xFFFFFF

    def records(self):
        yield from self._walk(self.body, len(self.buf))

    def _walk(self, i: int, end: int):
        while i + HEADER <= end:
            typ = self.buf[i:i + 4]
            size = struct.unpack_from("<I", self.buf, i + 4)[0]
            if typ == b"GRUP":
                yield from self._walk(i + HEADER, i + size)
                i += size
                continue
            flags, form_id = struct.unpack_from("<II", self.buf, i + 8)
            if typ in WANTED:
                raw = self.buf[i + HEADER:i + HEADER + size]
                data = zlib.decompress(raw[4:]) if flags & COMPRESSED else raw
                yield typ, flags, form_id, data
            i += HEADER + size


def u32(data: bytes, at: int = 0) -> int:
    return struct.unpack_from("<I", data, at)[0] if len(data) >= at + 4 else 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("data", help="the game's Data folder")
    parser.add_argument("plugins", help="Plugins.txt - the load order")
    parser.add_argument("--out", default="artifacts/quest-audit")
    args = parser.parse_args()

    order = [line.strip() for line in Path(args.plugins).read_text(encoding="latin-1").splitlines()
             if line.strip() and not line.startswith("#")]
    forms: dict = {}  # key -> dict(type, flags, edid, file, ...); the last plugin wins
    names: list = []  # (source label, quest key or None, named key, how)
    info_quest_lines = 0

    for name in order:
        path = Path(args.data) / name
        if not path.exists():
            print(f"missing {name}", file=sys.stderr)
            continue
        plugin = Plugin(name, path.read_bytes())
        for typ, flags, form_id, data in plugin.records():
            k = plugin.key(form_id)
            f = {"type": typ.decode(), "flags": flags, "edid": "", "file": name, "script": None, "items": [],
                 "entries": [], "base": None, "scro": [], "schr": 0, "quest": None}
            subs = list(subrecords(data))
            for st, sd in subs:
                if st == b"EDID":
                    f["edid"] = sd.rstrip(b"\0").decode("latin-1")
                elif st == b"SCRI" and len(sd) >= 4:
                    f["script"] = plugin.key(u32(sd))
                elif st == b"CNTO" and len(sd) >= 4:
                    f["items"].append(plugin.key(u32(sd)))
                elif st == b"LVLO" and len(sd) >= 8:
                    f["entries"].append(plugin.key(u32(sd, 4)))
                elif st == b"NAME" and typ in (b"ACRE", b"ACHR", b"REFR") and len(sd) >= 4:
                    f["base"] = plugin.key(u32(sd))
                elif st == b"SCHR" and len(sd) >= 18:
                    f["schr"] = struct.unpack_from("<H", sd, 16)[0]
                elif st == b"QSTI" and len(sd) >= 4:
                    f["quest"] = plugin.key(u32(sd))
            if typ in (b"QUST", b"INFO", b"SCPT"):
                label = f"{typ.decode()} {f['edid'] or hex(form_id)}"
                quest = k if typ == b"QUST" else (f["quest"] if typ == b"INFO" else None)
                if typ == b"INFO":
                    info_quest_lines += 1
                for st, sd in subs:
                    if st == b"SCRO" and len(sd) >= 4 and u32(sd) != 0:
                        f["scro"].append(plugin.key(u32(sd)))
                        if typ != b"SCPT":
                            names.append((label, quest, plugin.key(u32(sd)), "stage/result script"))
                    elif st == b"QSTA" and len(sd) >= 4:
                        names.append((label, quest, plugin.key(u32(sd)), "target"))
                    elif st == b"CTDA" and len(sd) >= 20:
                        for at in (12, 16):
                            if u32(sd, at) != 0:
                                names.append((label, quest, plugin.key(u32(sd, at)), "condition"))
            if typ == b"REFR" and forms.get(f["base"], {}).get("type") != "LVLC":
                continue  # only placed leveled lists matter here (Oblivion places them as REFR)
            forms[k] = f

    def edid(k) -> str:
        f = forms.get(k)
        return f"{f['edid']} [{f['file']} {k[1]:06X}]" if f else f"? [{k[0]} {k[1]:06X}]"

    # Scripts: a quest's own script names count for that quest.
    quest_of_script = {}
    for k, f in forms.items():
        if f["type"] == "QUST" and f["script"]:
            quest_of_script[f["script"]] = k
    for k, f in forms.items():
        if f["type"] == "SCPT":
            quest = quest_of_script.get(k)
            label = f"SCPT {f['edid']}"
            for n in f["scro"]:
                names.append((label, quest, n, "quest script" if quest else
                              ("object script" if f["schr"] == 0 else "effect script")))

    named_by: dict = defaultdict(set)  # key -> {(quest label, how)}
    for label, quest, n, how in names:
        if n not in forms:
            continue
        who = f"QUST {forms[quest]['edid']}" if quest in forms else label
        named_by[n].add((who, how))

    def by_quest(k) -> set:
        return {w for w, _ in named_by.get(k, ()) if w.startswith("QUST")}

    # Bases in leveled lists, through nested lists.
    in_list: dict = defaultdict(set)
    for k, f in forms.items():
        if f["type"] == "LVLC":
            for e in f["entries"]:
                in_list[e].add(k)

    def kept(k) -> str:
        f = forms[k]
        if f["script"]:
            return "scripted"
        for item in f["items"]:
            fi = forms.get(item)
            if fi and fi["type"] in {t.decode() for t in ITEM_TYPES} and fi["flags"] & QUEST_ITEM:
                return "quest item"
        return ""

    replaced = {k for k in in_list if k in forms and forms[k]["type"] in ("CREA", "NPC_") and not kept(k)}

    def walkers_in(lvlc, seen=None) -> list:
        """The replaced bases a leveled list can give, through nested lists."""
        seen = seen if seen is not None else set()
        if lvlc in seen:
            return []
        seen.add(lvlc)
        found = []
        for e in forms.get(lvlc, {}).get("entries", []):
            if e in replaced:
                found.append(e)
            elif forms.get(e, {}).get("type") == "LVLC":
                found += walkers_in(e, seen)
        return found

    def quest_prefix(quest_label: str, item) -> bool:
        """The item's editor ID starts with its quest's (SQ06BearFang, SQ06)."""
        q = quest_label.split(" ", 1)[1] if " " in quest_label else quest_label
        return len(q) >= 3 and forms[item]["edid"].lower().startswith(q.lower())

    out = Path(args.out)
    out.mkdir(parents=True, exist_ok=True)
    lines = ["# Quest audit - what the spawn replacement can break", "",
             f"Plugins read: {len(order)} (Plugins.txt order). Forms: {len(forms)}. Names collected: {len(names)}.",
             f"Leveled-list bases (CREA/NPC_): {sum(1 for k in in_list if k in forms and forms[k]['type'] in ('CREA', 'NPC_'))}, "
             f"of them kept by the two rules: "
             f"{sum(1 for k in in_list if k in forms and forms[k]['type'] in ('CREA', 'NPC_') and kept(k))}, "
             f"taken as replaced: {len(replaced)}.", ""]

    # Gap 1.
    gap1 = defaultdict(list)
    for k in sorted(replaced):
        for item in forms[k]["items"]:
            fi = forms.get(item)
            if not fi or fi["type"] not in {t.decode() for t in ITEM_TYPES} or fi["flags"] & QUEST_ITEM:
                continue
            quests = by_quest(item)
            if quests or fi["script"]:
                for q in sorted(quests) or [f"(scripted: {edid(fi['script'])})"]:
                    mark = "OWN ITEM " if q.startswith("QUST") and quest_prefix(q, item) else ""
                    gap1[q].append(f"{mark}{edid(k)} carries {fi['type']} {edid(item)}")
    lines += ["## Gap 1 - a needed item without the quest flag on a replaced base", ""]
    for q in sorted(gap1):
        lines.append(f"- {q}")
        lines += [f"  - {x}" for x in sorted(set(gap1[q]))]
    own = sorted({q for q, xs in gap1.items() if any(x.startswith("OWN ITEM") for x in xs)})
    lines += ["", f"Quests with an item of their own (editor ID with the quest's prefix): {len(own)}: {', '.join(own)}",
              f"Quests: {sum(1 for q in gap1 if q.startswith('QUST'))}; scripted items not named by a quest: "
              f"{sum(1 for q in gap1 if not q.startswith('QUST'))} groups", ""]

    # Gap 2.
    gap2 = defaultdict(list)
    persistent_only = []
    for k, f in forms.items():
        if f["type"] not in ("ACRE", "ACHR", "REFR") or not f["base"] or forms.get(f["base"], {}).get("type") != "LVLC":
            continue
        quests = by_quest(k)
        others = {w for w, _ in named_by.get(k, ()) if not w.startswith("QUST")}
        walk = walkers_in(f["base"])
        what = (f"{edid(k)} places {edid(f['base'])} - "
                + (f"{len(set(walk))} replaced bases in it" if walk else "all kept - safe"))
        for q in quests:
            gap2[q].append(what)
        if not quests and others:
            for o in others:
                gap2[o].append(what)
        if not quests and not others and f["flags"] & PERSISTENT:
            persistent_only.append(what)
    lines += ["## Gap 2 - a placed leveled list named by a quest or a script", ""]
    for q in sorted(gap2):
        lines.append(f"- {q}")
        lines += [f"  - {x}" for x in sorted(set(gap2[q]))]
    at_risk = sorted({q for q, xs in gap2.items() if q.startswith("QUST") and any("replaced bases" in x for x in xs)})
    lines += ["", f"Quests with a replaced base in a named placed list: {len(at_risk)}: {', '.join(at_risk)}",
              f"Quests: {sum(1 for q in gap2 if q.startswith('QUST'))}; other scripts/lines: "
              f"{sum(1 for q in gap2 if not q.startswith('QUST'))}",
              f"Persistent but named by nothing: {len(persistent_only)}", ""]
    lines += [f"  - {x}" for x in sorted(persistent_only)[:200]]
    lines.append("")

    # Gap 3.
    gap3 = defaultdict(list)
    for k in sorted(set(replaced) | {k for k, f in forms.items() if f["type"] == "LVLC"}):
        for who, how in sorted(named_by.get(k, ())):
            if (forms[k]["type"] == "LVLC" and walkers_in(k)) or k in replaced:
                gap3[who].append(f"{forms[k]['type']} {edid(k)} ({how})")
    lines += ["## Gap 3 - a replaced base or a leveled list named by a quest or a script", ""]
    for q in sorted(gap3):
        lines.append(f"- {q}")
        lines += [f"  - {x}" for x in sorted(set(gap3[q]))]
    lines += ["", f"Quests: {sum(1 for q in gap3 if q.startswith('QUST'))}; other scripts/lines: "
              f"{sum(1 for q in gap3 if not q.startswith('QUST'))}", ""]

    (out / "summary.md").write_text("\n".join(lines), encoding="utf-8")
    print("\n".join(l for l in lines if not l.startswith("  ")))
    return 0


if __name__ == "__main__":
    sys.exit(main())
