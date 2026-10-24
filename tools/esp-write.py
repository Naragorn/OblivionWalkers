#!/usr/bin/env python3
"""Writes OblivionWalkers.esp: the records the plugin needs beside Oblivion.esm,
built from the ESM's own records so that nothing is typed by hand.

The ESP holds, at local IDs the plugin knows (Rise.cpp finds the ESP's load
index at runtime):
  0x800  RACE WalkersZombieRace "Walker" - a copy of the Imperial race
         (00000907), every subrecord verbatim but EDID, FULL and DESC; the
         race path of docs/walkers-spec.md 4.1, kept as the fallback.
  0x801..0x804  CREA WalkersZombie1..4 "Walker" - copies of CreatureZombie1..4
         (00025E6E, 00025E5E, 00025E55, 00025E4F), every subrecord verbatim
         but EDID and FULL: the walkers the risen become (4.1, the creature
         path decided 2026-10-23) and the spawns turn into (4.6); their own
         animation folder (4.4) comes later on these records.
  0x805  LVLC WalkersZombieVariant - each of the four three times at level 1
         (counts 1, 2, 3, which the plugin rewrites from [Replace] MinCount
         and MaxCount at the start), no chance none, flags 0, like
         LL0ZombieVariant (00034095): what 4.6 puts in place of a creature.

    python3 tools/esp-write.py ~/.local/share/Steam/steamapps/common/Oblivion/Data/Oblivion.esm \\
        --out OblivionWalkers.esp

File layout, read off DLCShiveringIsles.esp and Oblivion.esm on 2026-10-23 and
cross-checked with UESP's TES4 page: a record is type(4) dataSize(4) flags(4)
formID(4) vcs(4) data; a GRUP is 'GRUP' size(4, the header included) label(4)
type(4) stamp(4); the TES4 record's data is HEDR (version 1.0f, numRecords,
nextObjectId), CNAM (author), SNAM (description), then MAST (master name) and
DATA (u64 0) per master. A new record's mod index is the count of masters: with
Oblivion.esm alone, 01.
"""

from __future__ import annotations

import argparse
import importlib.util
import struct
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
RACE_EDID = "WalkersZombieRace"
RACE_NAME = "Walker"
RACE_DESC = "One of the walking dead."
IMPERIAL_RACE = 0x00000907
LOCAL_RACE_ID = 0x800
WALKER_NAME = "Walker"
ZOMBIE_SOURCES = [0x00025E6E, 0x00025E5E, 0x00025E55, 0x00025E4F]  # CreatureZombie1..4
LOCAL_WALKER_IDS = [0x801, 0x802, 0x803, 0x804]
LOCAL_LIST_ID = 0x805
VARIANT_SLOTS = 10  # ReplaceLogic.h kVariantSlots
LIST_EDID = "WalkersZombieVariant"
AUTHOR = "Naragorn"
DESCRIPTION = "Oblivion Walkers - the records the plugin needs: the Walker race for the risen."


def load_esm_module():
    spec = importlib.util.spec_from_file_location("esm_spawns", HERE / "esm-spawns.py")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def zstring(text: str) -> bytes:
    return text.encode("latin-1") + b"\0"


def subrecord(kind: bytes, data: bytes) -> bytes:
    if len(data) > 0xFFFF:
        raise ValueError(f"subrecord {kind!r} too long for a 16-bit size: {len(data)}")
    return kind + struct.pack("<H", len(data)) + data


def record(kind: bytes, form_id: int, data: bytes, flags: int = 0) -> bytes:
    return kind + struct.pack("<IIII", len(data), flags, form_id, 0) + data


def group(label: bytes, records: bytes) -> bytes:
    return b"GRUP" + struct.pack("<I", 20 + len(records)) + label + struct.pack("<II", 0, 0) + records


def header(num_records: int, next_object_id: int, masters: list[str]) -> bytes:
    data = subrecord(b"HEDR", struct.pack("<fiI", 1.0, num_records, next_object_id))
    data += subrecord(b"CNAM", zstring(AUTHOR))
    data += subrecord(b"SNAM", zstring(DESCRIPTION))
    for master in masters:
        data += subrecord(b"MAST", zstring(master))
        data += subrecord(b"DATA", struct.pack("<Q", 0))
    return record(b"TES4", 0, data)


def find_record(esm, buf: bytes, kind: bytes, form_id: int) -> list[tuple[bytes, bytes]]:
    found: list[tuple[bytes, bytes]] = []

    def on_record(typ, flags, fid, offset, size, cell):
        if typ == kind and fid == form_id:
            found.extend(esm.subrecords(esm.record_data(buf, offset, size, flags)))

    esm.walk(buf, 0, len(buf), [], on_record)
    if not found:
        raise SystemExit(f"no {kind.decode()} {form_id:08X} in the ESM")
    return found


def build_race(template: list[tuple[bytes, bytes]]) -> bytes:
    """The Imperial race's subrecords with its own name and description."""
    out = b""
    for kind, data in template:
        if kind == b"EDID":
            data = zstring(RACE_EDID)
        elif kind == b"FULL":
            data = zstring(RACE_NAME)
        elif kind == b"DESC":
            data = zstring(RACE_DESC)
        out += subrecord(kind, data)
    return out


def build_walker(template: list[tuple[bytes, bytes]], edid: str) -> bytes:
    """A zombie creature's subrecords under the Walker's editor ID and name."""
    out = b""
    for kind, data in template:
        if kind == b"EDID":
            data = zstring(edid)
        elif kind == b"FULL":
            data = zstring(WALKER_NAME)
        out += subrecord(kind, data)
    return out


def variant_entries(walkers: list[int]) -> list[tuple[int, int]]:
    """Every walker VARIANT_SLOTS times, with the counts 1..VARIANT_SLOTS."""
    return [(form, slot + 1) for form in walkers for slot in range(VARIANT_SLOTS)]


def build_list(entries: list[tuple[int, int]]) -> bytes:
    """A leveled creature list of the (form, count) entries at level 1: EDID,
    LVLD (chance none), LVLF (flags), then LVLO level(2) pad(2) form(4)
    count(2) pad(2)."""
    out = subrecord(b"EDID", zstring(LIST_EDID))
    out += subrecord(b"LVLD", bytes([0]))
    out += subrecord(b"LVLF", bytes([0]))
    for form, count in entries:
        out += subrecord(b"LVLO", struct.pack("<HHIHH", 1, 0, form, count, 0))
    return out


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("esm", help="Oblivion.esm")
    parser.add_argument("--out", default=str(HERE.parent / "OblivionWalkers.esp"))
    args = parser.parse_args()
    esm = load_esm_module()
    buf = Path(args.esm).read_bytes()

    masters = ["Oblivion.esm"]
    index = len(masters) << 24

    template = find_record(esm, buf, b"RACE", IMPERIAL_RACE)
    race_form = index | LOCAL_RACE_ID
    races = record(b"RACE", race_form, build_race(template))

    walker_templates = [find_record(esm, buf, b"CREA", source) for source in ZOMBIE_SOURCES]
    walker_forms = [index | local for local in LOCAL_WALKER_IDS]
    creatures = b"".join(record(b"CREA", form, build_walker(tpl, f"WalkersZombie{i + 1}"))
                         for i, (form, tpl) in enumerate(zip(walker_forms, walker_templates)))
    list_form = index | LOCAL_LIST_ID
    lists = record(b"LVLC", list_form, build_list(variant_entries(walker_forms)))

    body = group(b"RACE", races) + group(b"CREA", creatures) + group(b"LVLC", lists)
    groups = 3
    records = 1 + len(walker_forms) + 1
    plugin = header(num_records=groups + records, next_object_id=LOCAL_LIST_ID + 1, masters=masters) + body
    out = Path(args.out)
    out.write_bytes(plugin)

    # Read it back with the ESM parser: every record under its new IDs, every
    # other subrecord as the original's.
    written = find_record(esm, plugin, b"RACE", race_form)
    kept = [(k, d) for k, d in written if k not in (b"EDID", b"FULL", b"DESC")]
    original = [(k, d) for k, d in template if k not in (b"EDID", b"FULL", b"DESC")]
    names = {k.decode(): d.rstrip(b"\0").decode("latin-1") for k, d in written if k in (b"EDID", b"FULL", b"DESC")}
    if kept != original:
        raise SystemExit("the written race differs from the Imperial beyond its names")
    if names != {"EDID": RACE_EDID, "FULL": RACE_NAME, "DESC": RACE_DESC}:
        raise SystemExit(f"the written race's names are off: {names}")
    for i, (form, tpl) in enumerate(zip(walker_forms, walker_templates)):
        back = find_record(esm, plugin, b"CREA", form)
        if [(k, d) for k, d in back if k not in (b"EDID", b"FULL")] != \
                [(k, d) for k, d in tpl if k not in (b"EDID", b"FULL")]:
            raise SystemExit(f"walker {i + 1} differs from its zombie beyond the names")
        back_names = {k.decode(): d.rstrip(b"\0").decode("latin-1") for k, d in back if k in (b"EDID", b"FULL")}
        if back_names != {"EDID": f"WalkersZombie{i + 1}", "FULL": WALKER_NAME}:
            raise SystemExit(f"walker {i + 1}'s names are off: {back_names}")
    back_list = find_record(esm, plugin, b"LVLC", list_form)
    entries = [struct.unpack_from("<HHIHH", d, 0) for k, d in back_list if k == b"LVLO"]
    if [(e[2], e[3]) for e in entries] != variant_entries(walker_forms) or any(e[0] != 1 for e in entries):
        raise SystemExit(f"the list's entries are off: {[(f'{e[2]:08X}', e[3]) for e in entries]}")
    print(f"{out}: {len(plugin)} bytes - RACE {race_form:08X} {RACE_EDID}, CREA "
          + ", ".join(f"{f:08X}" for f in walker_forms)
          + f" WalkersZombie1..4 '{WALKER_NAME}', LVLC {list_form:08X} {LIST_EDID} with the four x{VARIANT_SLOTS}; "
          "every other subrecord as the originals'")
    return 0


if __name__ == "__main__":
    sys.exit(main())
