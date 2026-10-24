#!/usr/bin/env python3
"""Writes [ReplaceCreatures] of OblivionWalkers.ini from the game's own files.

Every creature and person a leveled creature list (LVLC) of the given files
can spawn gets its line, as the plugin writes one at the game's start
(src/ReplaceLogic.h FormatReplaceKey, ClassifyCreature, DefaultOn): the name,
the file that made the record and its local ID, the note, 1 or 0. A mod's
creatures are not in the shipped list - the plugin appends their lines at
the first start with the mod.

    python3 tools/ini-creatures.py ~/.local/share/Steam/steamapps/common/Oblivion/Data \\
        --ini OblivionWalkers.ini

The files: Oblivion.esm (Shivering Isles is inside it in the GOTY copies; the
DLCShiveringIsles.esp beside it is an empty stub), Knights.esp and the
official DLC*.esp, in that order - a later file's record overrides an
earlier one, as in the game. A form ID's top byte indexes the file's master
list (MAST in its TES4 header); the index past the last master is the file
itself (UESP "Oblivion Mod:Mod File Format", FormID).

The record fields, as the plugin reads them off the loaded records:
  ACBS flags (uint32 first)    swims 0x10, flies 0x20, walks 0x40
  AIDT byte 1                  confidence
  CREA DATA byte 0             type (0 creature, 1 daedra, 2 undead)
  SCRI                         a script
  FULL                         the name
"""

from __future__ import annotations

import argparse
import importlib.util
import re
import struct
import sys
from pathlib import Path

_spec = importlib.util.spec_from_file_location("esm_spawns", Path(__file__).with_name("esm-spawns.py"))
esm = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(esm)

OFFICIAL = ["Oblivion.esm", "Knights.esp", "DLCBattlehornCastle.esp", "DLCFrostcrag.esp", "DLCHorseArmor.esp",
            "DLCMehrunesRazor.esp", "DLCOrrery.esp", "DLCSpellTomes.esp", "DLCThievesDen.esp", "DLCVileLair.esp"]
SECTION = "[ReplaceCreatures]"

# src/ReplaceLogic.h
SWIMS, FLIES, WALKS = 0x10, 0x20, 0x40
TYPE_CREATURE = 0
HARMLESS_CONFIDENCE_MOST = 25
NOTES = {"replace": "", "harmless": " harmless animal", "swims": " swims only", "scripted": " has a script",
         "person": " person"}


def classify(rec: dict) -> str:
    if rec["person"]:
        return "person"
    if rec["scripted"]:
        return "scripted"
    if rec["flags"] & SWIMS and not rec["flags"] & (WALKS | FLIES):
        return "swims"
    if rec["type"] == TYPE_CREATURE and rec["confidence"] <= HARMLESS_CONFIDENCE_MOST:
        return "harmless"
    return "replace"


def key_of(name: str, origin: str, local: int, kind: str) -> str:
    clean = "".join(" " if c in "=;[]" else c for c in name[:95])
    return f"{clean or 'Creature'} [{origin} {local:06X}]{NOTES[kind]}"


def read_file(path: Path, records: dict, lists: dict) -> None:
    buf = path.read_bytes()
    masters: list[str] = []

    def origin_of(form_id: int) -> tuple[str, int]:
        index = form_id >> 24
        name = masters[index] if index < len(masters) else path.name
        return name, form_id & 0x00FFFFFF

    def on_record(typ, flags, form_id, offset, size, _cell):
        if typ == b"TES4":
            for st, sd in esm.subrecords(esm.record_data(buf, offset, size, flags)):
                if st == b"MAST":
                    masters.append(sd.rstrip(b"\0").decode("latin-1"))
            return
        if typ not in (b"CREA", b"NPC_", b"LVLC"):
            return
        data = esm.record_data(buf, offset, size, flags)
        if typ == b"LVLC":
            entries = []
            for st, sd in esm.subrecords(data):
                if st == b"LVLO":
                    entries.append(origin_of(struct.unpack_from("<I", sd, 4)[0]))
            lists[origin_of(form_id)] = entries
            return
        rec = {"person": typ == b"NPC_", "name": "", "flags": 0, "confidence": 0, "type": TYPE_CREATURE,
               "scripted": False}
        for st, sd in esm.subrecords(data):
            if st == b"FULL":
                rec["name"] = sd.rstrip(b"\0").decode("latin-1")
            elif st == b"ACBS" and len(sd) >= 4:
                rec["flags"] = struct.unpack_from("<I", sd, 0)[0]
            elif st == b"AIDT" and len(sd) >= 2:
                rec["confidence"] = sd[1]
            elif st == b"DATA" and typ == b"CREA" and len(sd) >= 1:
                rec["type"] = sd[0]
            elif st == b"SCRI" and len(sd) >= 4:
                rec["scripted"] = struct.unpack_from("<I", sd, 0)[0] != 0
        records[origin_of(form_id)] = rec

    esm.walk(buf, 0, len(buf), [], on_record)


def main() -> int:
    p = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    p.add_argument("data", type=Path, help="the game's Data folder")
    p.add_argument("--ini", type=Path, help="write the section into this INI (everything after its header)")
    args = p.parse_args()

    records: dict = {}
    lists: dict = {}
    used = []
    for name in OFFICIAL:
        path = args.data / name
        if not path.exists():
            print(f"{name}: not there, skipped", file=sys.stderr)
            continue
        read_file(path, records, lists)
        used.append(name)

    spawned = {target for entries in lists.values() for target in entries if target in records}
    order = {name: i for i, name in enumerate(used)}
    lines = []
    kinds: dict[str, int] = {}
    for origin, local in sorted(spawned, key=lambda k: (order.get(k[0], 99), records[k]["name"].lower(), k[1])):
        rec = records[(origin, local)]
        kind = classify(rec)
        kinds[kind] = kinds.get(kind, 0) + 1
        lines.append(f"{key_of(rec['name'], origin, local, kind)}={1 if kind == 'replace' else 0}")
    print(f"{len(lists)} leveled lists in {', '.join(used)}; {len(lines)} creatures and people they spawn: " +
          ", ".join(f"{k} {v}" for k, v in sorted(kinds.items())), file=sys.stderr)

    if args.ini is None:
        print("\n".join(lines))
        return 0
    text = args.ini.read_bytes().decode("latin-1")
    # The header line itself: the comments above it name the section too.
    found = re.search(r"^" + re.escape(SECTION) + r"[ \t]*\r?$", text, re.MULTILINE)
    at = found.start() if found else -1
    if at < 0:
        print(f"{args.ini}: no {SECTION}", file=sys.stderr)
        return 1
    newline = "\r\n" if "\r\n" in text else "\n"
    text = text[:at] + SECTION + newline + newline.join(lines) + newline
    args.ini.write_bytes(text.encode("latin-1"))
    print(f"written into {args.ini}", file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main())
