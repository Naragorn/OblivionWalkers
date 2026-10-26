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
  0x806..0x815  PACK WalkersHordeTravel00..15 - the horde's leaders (4.11):
         Travel to marker NN, must reach it, 256 units near; a copy of
         SE32HlovalReturnStart (0009718B) but EDID and the location.
  0x816..0x825  PACK WalkersHordeFollow00..15 - the horde's followers: Follow,
         must complete, 300 units behind; a copy of FollowPlayer (0009828A)
         but EDID, flags (its defensive combat dropped - a walker attacks)
         and the distance. The plugin sets the target, the group's leader.
  0x826  CELL WalkersHordeMarkers - an interior holding cell, as TestCave04
         (000183F0): EDID and DATA (interior).
  0x827..0x836  REFR WalkersHordeMarker00..15 - persistent XMarkers (0000003B)
         in that cell, as MarkSpellTestMarker4 (00040BB8): the plugin moves
         marker NN ahead of group NN's leader (MoveTo), and the leader's
         travel package goes there.
  0x837..0x83E  NPC_ WalkersGuardAnvil..ImperialCity - one guard per city
         (4.11b), a copy of a city guard (Anvil's stables post 0000A0AE and
         the like) but EDID, its AI packages and its script, and never
         essential (Nadi, 2026-10-26): it stands and fights where it is put.
  0x83F..0x8FB  ACHR WalkersGuardPoolCityN - 21 persistent, initially
         disabled references of each city's guard in the holding cell, as
         an Oblivion.esm ACHR (000171CB: NAME and DATA), in banks of six
         per city (GuardLogic.h PoolLocalId: slots 0-5 at 0x83F + city x 6,
         where the six-pool had them, 6-11 at 0x86F + city x 6, where the
         pool of twelve had them, 12-17 at 0x89F + city x 6, 18-20 at
         0x8CF + city x 6): the
         plugin moves them out to a gate and back - a guard that dies is
         raised and sent out again, so no reference is ever made at runtime
         (PlaceAtMe's actors stay in the save for good, the CS wiki's
         PlaceAtMe page).
  0x8FC..0x90D  BOOK WalkersCourier* - the walkers' news (4.9), 18 editions of
         the Black Horse Courier in NewsLogic.h EditionIndex order
         (tools/courier-editions.py), each a copy of Broadsheet01VampireNest
         (0006D6F3) but EDID, FULL and DESC: the plugin puts the edition that
         fits on the vanilla broadsheets of the inns and the Courier's office
         (src/CourierRefs.h). 0x8FC is past the last pool guard and no pool
         slot (GuardLogic.h PoolSlotOf: bank 3, slot 21).

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
HORDE_GROUPS = 16  # HordeLogic.h kHordeGroups
LOCAL_TRAVEL_IDS = [0x806 + i for i in range(HORDE_GROUPS)]
LOCAL_FOLLOW_IDS = [0x816 + i for i in range(HORDE_GROUPS)]
LOCAL_CELL_ID = 0x826
LOCAL_MARKER_IDS = [0x827 + i for i in range(HORDE_GROUPS)]
TRAVEL_TEMPLATE = 0x0009718B  # SE32HlovalReturnStart: Travel, near a reference, must reach
FOLLOW_TEMPLATE = 0x0009828A  # FollowPlayer: Follow the player, must complete
CELL_TEMPLATE = 0x000183F0    # TestCave04: an interior cell, EDID and DATA only
MARKER_TEMPLATE = 0x00040BB8  # MarkSpellTestMarker4: a persistent XMarker
XMARKER = 0x0000003B
TRAVEL_RADIUS = 256
FOLLOW_UNITS = 300
PACKAGE_DEFENSIVE_COMBAT = 0x00400000
RECORD_PERSISTENT = 0x400
CELL_EDID = "WalkersHordeMarkers"
CITIES = ["Anvil", "Bravil", "Bruma", "Cheydinhal", "Chorrol", "Leyawiin", "Skingrad", "ImperialCity"]  # CityGates.h City
GUARD_TEMPLATES = [0x0000A0AE, 0x0000A0B0, 0x0000A0B2, 0x0000A0B4, 0x0000A0B6, 0x0003554B, 0x0000A0BA, 0x000752F2]
LOCAL_GUARD_IDS = [0x837 + i for i in range(len(CITIES))]
GUARD_POOL = 21  # GuardLogic.h kGuardPool
GUARD_BANK = 6  # GuardLogic.h kGuardBank
ACBS_ESSENTIAL = 0x2
POOL_FIRST_ID = 0x83F  # GuardLogic.h kGuardPoolFirstLocalId


def pool_local_id(city: int, slot: int) -> int:
    """GuardLogic.h PoolLocalId."""
    return POOL_FIRST_ID + (slot // GUARD_BANK) * len(CITIES) * GUARD_BANK + city * GUARD_BANK + slot % GUARD_BANK


LOCAL_POOL_IDS = sorted(pool_local_id(c, n) for c in range(len(CITIES)) for n in range(GUARD_POOL))
BROADSHEET_TEMPLATE = 0x0006D6F3  # Broadsheet01VampireNest
EDITION_COUNT = 18  # NewsLogic.h kEditionCount
LOCAL_EDITION_IDS = [0x8FC + i for i in range(EDITION_COUNT)]
ACHR_TEMPLATE = 0x000171CB
RECORD_INITIALLY_DISABLED = 0x800
VARIANT_SLOTS = 10  # ReplaceLogic.h kVariantSlots
LIST_EDID = "WalkersZombieVariant"
AUTHOR = "Naragorn"
DESCRIPTION = "Oblivion Walkers - the records the plugin needs: the Walker race for the risen."


def load_editions_module():
    spec = importlib.util.spec_from_file_location("courier_editions", HERE / "courier-editions.py")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


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


def build_travel(template: list[tuple[bytes, bytes]], edid: str, marker: int) -> bytes:
    """A leader's package: the template's, its location near `marker`."""
    out = b""
    for kind, data in template:
        if kind == b"EDID":
            data = zstring(edid)
        elif kind == b"PLDT":
            data = struct.pack("<iIi", 0, marker, TRAVEL_RADIUS)
        out += subrecord(kind, data)
    return out


def build_follow(template: list[tuple[bytes, bytes]], edid: str) -> bytes:
    """A follower's package: FollowPlayer's without defensive combat, its
    distance FOLLOW_UNITS; the target (the player) the plugin replaces."""
    out = b""
    for kind, data in template:
        if kind == b"EDID":
            data = zstring(edid)
        elif kind == b"PKDT":
            flags = struct.unpack_from("<I", data, 0)[0] & ~PACKAGE_DEFENSIVE_COMBAT
            data = struct.pack("<I", flags) + data[4:]
        elif kind == b"PTDT":
            target_type, target = struct.unpack_from("<iI", data, 0)
            data = struct.pack("<iIi", target_type, target, FOLLOW_UNITS)
        out += subrecord(kind, data)
    return out


def build_cell(template: list[tuple[bytes, bytes]]) -> bytes:
    out = b""
    for kind, data in template:
        out += subrecord(kind, zstring(CELL_EDID) if kind == b"EDID" else data)
    return out


def build_marker(template: list[tuple[bytes, bytes]], edid: str, slot: int) -> bytes:
    """An XMarker in the holding cell, a row of them 64 units apart."""
    out = b""
    for kind, data in template:
        if kind == b"EDID":
            data = zstring(edid)
        elif kind == b"NAME":
            data = struct.pack("<I", XMARKER)
        elif kind == b"DATA":
            data = struct.pack("<6f", slot * 64.0, 0.0, 0.0, 0.0, 0.0, 0.0)
        out += subrecord(kind, data)
    return out


def guard_acbs(data: bytes) -> bytes:
    """The guard's ACBS without the essential flag."""
    flags = struct.unpack_from("<I", data, 0)[0] & ~ACBS_ESSENTIAL
    return struct.pack("<I", flags) + data[4:]


def build_guard(template: list[tuple[bytes, bytes]], edid: str) -> bytes:
    """A city guard without its packages, its script and essential."""
    out = b""
    for kind, data in template:
        if kind in (b"PKID", b"SCRI"):
            continue
        if kind == b"EDID":
            data = zstring(edid)
        elif kind == b"ACBS":
            data = guard_acbs(data)
        out += subrecord(kind, data)
    return out


def build_pool_ref(template: list[tuple[bytes, bytes]], edid: str, base: int, slot: int) -> bytes:
    """A guard waiting in the holding cell, a row of them."""
    out = subrecord(b"EDID", zstring(edid))
    for kind, data in template:
        if kind == b"NAME":
            data = struct.pack("<I", base)
        elif kind == b"DATA":
            data = struct.pack("<6f", slot * 64.0, 512.0, 0.0, 0.0, 0.0, 0.0)
        elif kind == b"EDID":
            continue
        out += subrecord(kind, data)
    return out


def build_edition(template: list[tuple[bytes, bytes]], edid: str, title: str, text: str) -> bytes:
    """A broadsheet with the walkers' news."""
    out = b""
    for kind, data in template:
        if kind == b"EDID":
            data = zstring(edid)
        elif kind == b"FULL":
            data = zstring(title)
        elif kind == b"DESC":
            data = text.encode("latin-1") + b"\0"
        out += subrecord(kind, data)
    return out


def typed_group(label: int, gtype: int, records: bytes) -> bytes:
    return b"GRUP" + struct.pack("<IIiI", 20 + len(records), label, gtype, 0) + records


def cell_groups(cell_form: int, cell: bytes, markers: bytes) -> bytes:
    """The top CELL group: interior block (the form ID's last decimal digit)
    and sub-block (the one before it), the CELL record, then its children
    group (6) with the persistent children (8) - the nesting Oblivion.esm
    has (TestCave04: block 2, sub-block 1)."""
    block = cell_form % 10
    sub_block = (cell_form // 10) % 10
    children = typed_group(cell_form, 6, typed_group(cell_form, 8, markers))
    inner = typed_group(sub_block, 3, cell + children)
    return group(b"CELL", typed_group(block, 2, inner))


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

    travel_template = find_record(esm, buf, b"PACK", TRAVEL_TEMPLATE)
    follow_template = find_record(esm, buf, b"PACK", FOLLOW_TEMPLATE)
    marker_forms = [index | local for local in LOCAL_MARKER_IDS]
    travel_forms = [index | local for local in LOCAL_TRAVEL_IDS]
    follow_forms = [index | local for local in LOCAL_FOLLOW_IDS]
    packages = b"".join(record(b"PACK", form, build_travel(travel_template, f"WalkersHordeTravel{i:02d}", marker))
                        for i, (form, marker) in enumerate(zip(travel_forms, marker_forms)))
    packages += b"".join(record(b"PACK", form, build_follow(follow_template, f"WalkersHordeFollow{i:02d}"))
                         for i, form in enumerate(follow_forms))
    cell_form = index | LOCAL_CELL_ID
    cell = record(b"CELL", cell_form, build_cell(find_record(esm, buf, b"CELL", CELL_TEMPLATE)))
    marker_template = find_record(esm, buf, b"REFR", MARKER_TEMPLATE)
    markers = b"".join(record(b"REFR", form, build_marker(marker_template, f"WalkersHordeMarker{i:02d}", i),
                              flags=RECORD_PERSISTENT)
                       for i, form in enumerate(marker_forms))
    guard_forms = [index | local for local in LOCAL_GUARD_IDS]
    guard_templates = [find_record(esm, buf, b"NPC_", t) for t in GUARD_TEMPLATES]
    guards = b"".join(record(b"NPC_", form, build_guard(tpl, f"WalkersGuard{city}"))
                      for form, tpl, city in zip(guard_forms, guard_templates, CITIES))
    achr_template = find_record(esm, buf, b"ACHR", ACHR_TEMPLATE)
    pool_forms = [index | local for local in LOCAL_POOL_IDS]
    pool_bases = {}
    pool = b""
    for local in LOCAL_POOL_IDS:
        c, n = next((c, n) for c in range(len(CITIES)) for n in range(GUARD_POOL) if pool_local_id(c, n) == local)
        pool_bases[index | local] = guard_forms[c]
        pool += record(b"ACHR", index | local,
                       build_pool_ref(achr_template, f"WalkersGuardPool{CITIES[c]}{n}", guard_forms[c],
                                      local - POOL_FIRST_ID),
                       flags=RECORD_PERSISTENT | RECORD_INITIALLY_DISABLED)

    edition_texts = load_editions_module().editions()
    if len(edition_texts) != EDITION_COUNT:
        raise SystemExit(f"{len(edition_texts)} editions, NewsLogic.h has {EDITION_COUNT}")
    broadsheet = find_record(esm, buf, b"BOOK", BROADSHEET_TEMPLATE)
    edition_forms = [index | local for local in LOCAL_EDITION_IDS]
    books = b"".join(record(b"BOOK", form, build_edition(broadsheet, edid, title, text))
                     for form, (edid, title, text) in zip(edition_forms, edition_texts))
    body = (group(b"RACE", races) + group(b"BOOK", books) + group(b"NPC_", guards) + group(b"CREA", creatures)
            + group(b"LVLC", lists)
            + cell_groups(cell_form, cell, markers + pool) + group(b"PACK", packages))
    groups = 5 + 5 + 1
    records = (1 + len(edition_forms) + len(guard_forms) + len(walker_forms) + 1 + 1 + len(marker_forms)
               + len(pool_forms) + len(travel_forms) + len(follow_forms))
    plugin = header(num_records=groups + records, next_object_id=LOCAL_EDITION_IDS[-1] + 1, masters=masters) + body
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
    for i, (form, marker) in enumerate(zip(travel_forms, marker_forms)):
        back = dict(find_record(esm, plugin, b"PACK", form))
        if struct.unpack("<iIi", back[b"PLDT"]) != (0, marker, TRAVEL_RADIUS) or back[b"PKDT"] != dict(travel_template)[b"PKDT"]:
            raise SystemExit(f"travel package {i} is off")
    for i, form in enumerate(follow_forms):
        back = dict(find_record(esm, plugin, b"PACK", form))
        if struct.unpack_from("<I", back[b"PKDT"], 0)[0] & PACKAGE_DEFENSIVE_COMBAT or back[b"PKDT"][4] != 1:
            raise SystemExit(f"follow package {i} is off")
        if struct.unpack("<iIi", back[b"PTDT"])[2] != FOLLOW_UNITS:
            raise SystemExit(f"follow package {i}'s distance is off")
    back_cell = dict(find_record(esm, plugin, b"CELL", cell_form))
    if back_cell[b"EDID"].rstrip(b"\0").decode() != CELL_EDID or back_cell[b"DATA"][0] & 1 == 0:
        raise SystemExit("the holding cell is off")
    for i, form in enumerate(marker_forms):
        back = dict(find_record(esm, plugin, b"REFR", form))
        if struct.unpack("<I", back[b"NAME"])[0] != XMARKER:
            raise SystemExit(f"marker {i} is no XMarker")
    for form, tpl, city in zip(guard_forms, guard_templates, CITIES):
        back = find_record(esm, plugin, b"NPC_", form)
        kept = [(k, d) for k, d in back if k != b"EDID"]
        wanted = [(k, guard_acbs(d) if k == b"ACBS" else d) for k, d in tpl if k not in (b"EDID", b"PKID", b"SCRI")]
        if kept != wanted:
            raise SystemExit(f"the {city} guard differs from its template beyond EDID, packages, script and essential")
    for form in pool_forms:
        back = dict(find_record(esm, plugin, b"ACHR", form))
        if struct.unpack("<I", back[b"NAME"])[0] != pool_bases[form]:
            raise SystemExit(f"pool guard {form:08X} has the wrong base")
    for form, (edid, title, text) in zip(edition_forms, edition_texts):
        back = dict(find_record(esm, plugin, b"BOOK", form))
        if back[b"FULL"].rstrip(b"\0").decode("latin-1") != title or back[b"MODL"] != dict(broadsheet)[b"MODL"]:
            raise SystemExit(f"edition {edid} reads back wrong")
    if LOCAL_EDITION_IDS[0] <= LOCAL_POOL_IDS[-1]:
        raise SystemExit("the editions overlap the guard pool")
    if (pool_local_id(0, 0) != 0x83F or pool_local_id(7, 5) != 0x86E or pool_local_id(0, 6) != 0x86F
            or pool_local_id(7, 11) != 0x89E):
        raise SystemExit("the pool's first banks moved: a save made with a smaller pool would lose its guards")
    print(f"{out}: {len(plugin)} bytes - RACE {race_form:08X} {RACE_EDID}, CREA "
          + ", ".join(f"{f:08X}" for f in walker_forms)
          + f" WalkersZombie1..4 '{WALKER_NAME}', LVLC {list_form:08X} {LIST_EDID} with the four x{VARIANT_SLOTS}; "
          + f"PACK {travel_forms[0]:08X}.. travel and {follow_forms[0]:08X}.. follow x{HORDE_GROUPS}, CELL {cell_form:08X} "
          + f"{CELL_EDID} with markers {marker_forms[0]:08X}..{marker_forms[-1]:08X} and {len(pool_forms)} pool guards "
          + f"{pool_forms[0]:08X}..{pool_forms[-1]:08X} of NPC_ {guard_forms[0]:08X}..{guard_forms[-1]:08X}; "
          "every other subrecord as the originals'")
    return 0


if __name__ == "__main__":
    sys.exit(main())
