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
  0x805  LVLC WalkersZombieVariant - VARIANT_SLOTS entries at level 1, each
         the any-walker list (0x94A) at counts 1..10, which the plugin
         rewrites from [Replace] MinCount and MaxCount at the start; no
         chance none, flags 0, like LL0ZombieVariant (00034095): what 4.6
         puts in place of a creature.
  0x94A  LVLC WalkersAnyWalker - every walker at level 1, count 1, twice, a
         headless one once (half as often), the vanilla looks first; flags
         2, "calculate for each item in count", so the engine draws it anew
         for each walker of a count (Nadi, 2026-10-27: ten at once were one
         look; spawn-mixed.txt).
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
  0x90E..0x91D  CREA WalkersZombie5..20 "Walker" - more looks (Nadi,
         2026-10-27: "brauchen mer diversität an zombies"): the other sixteen
         body part sets (NIFZ) Oblivion.esm's zombies wear, read off its
         CREA records whose skeleton is Creatures\\Zombie\\Skeleton.NIF on
         2026-10-27 (twenty sets, four of them the walkers above). Each is
         CreatureZombie1, 2 or 3 - the one missing the same arm (KFFZ, MODB,
         MODT and the ACBS arm flags go with the arms) - with the donor's
         NIFZ and, without a head among the parts, ACBS's No Head (0x8000,
         UESP Tes4Mod:Mod_File_Format/CREA): the walkers' stats stay one.
  0x94D..0x952  IDLE WalkersGrab{Hold,Bite,LetGo}{Zombie,Moo} - the grab's
         animations (4.19, tools/grab-anims.py) beneath each zombie
         skeleton's IdleAnims, after the zombie's IdleSway (0002ACCD) in the
         idle tree, each with a condition never true (GetRandomPercent < 0)
         so no zombie plays them but by the plugin.
  0x91E..0x949  CREA WalkersZombie21..64 "Walker" - MOO's looks (Nadi,
         2026-10-27): the part sets on Maskar's Oblivion Overhaul's zombie
         skeleton, its default 0x91E..0x925, Argonian 0x926..0x92F, Orc
         0x930..0x939, High Elf 0x93A..0x941, Dark Elf 0x942..0x949
         (src/WalkerLooks.h) - built as the sixteen above, with the donor's
         MODL, MODB, MODT, NIFZ and KFFZ: mesh paths only, no MOO master; the
         plugin uses them only with MOO loaded, and the variant list holds
         them after the vanilla twenty so it can cut them off without MOO.

    python3 tools/esp-write.py ~/.local/share/Steam/steamapps/common/Oblivion/Data/Oblivion.esm \\
        ~/.local/share/Steam/steamapps/common/Oblivion/Data/"Maskar's Oblivion Overhaul.esp" \\
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
import re
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
# The donors of the sixteen other part sets (RiseLogic.h kWalkerLocalIds
# 0x90E..0x91D, in this order): CreatureZombieHeadless1, SEUncleLeo,
# XPEbroccaWife, MQ12KingsZombie03, MQ12KingsZombie02, Dark05Zombie1,
# MG01ZombieHeadless14, MG01ZombieHeadless8, ARTESTZombie01, TestZombieGrant,
# TestCombatZombie02, TestZombie06, TestZombie05, TestZombie04, TestZombie03,
# TestZombie02.
PART_DONORS = [0x00025E4E, 0x0009661A, 0x0008238C, 0x000C5B40, 0x000C5B3F, 0x00071B98, 0x00047503, 0x00047500,
               0x00026076, 0x0002CD02, 0x0002C7EC, 0x0002AAC6, 0x0002AAC5, 0x0002AAC4, 0x0002AAC3, 0x0002B81C]
LOCAL_MORE_WALKER_IDS = [0x90E + i for i in range(len(PART_DONORS))]
VANILLA_KEYS = (b"NIFZ",)
# MOO's looks (Nadi, 2026-10-27: its race zombies and its default set, one
# ESP, used only with MOO loaded - WalkerLooks.h): the part sets on MOO's
# zombie skeleton (MOO\zombie\skeleton.nif), one donor each, read off
# Maskar's Oblivion Overhaul.esp on 2026-10-27 (IDs as in its file, its own
# records under index 01). Taken: the skeleton, bound, textures, parts and
# animations - mesh paths only, no MOO form, so no MOO master.
MOO_KEYS = (b"MODL", b"MODB", b"MODT", b"NIFZ", b"KFFZ")
MOO_DONORS = [
    # default: MOOZombie1Default01..04, MOOZombie2Default01..04
    0x0110B23C, 0x0110B23D, 0x0110B23E, 0x0110B23F, 0x0110B240, 0x0110B241, 0x0110B242, 0x0110B243,
    # Argonian: MOOSummonZombie1Argonian01..04, MOOSummonZombie2Argonian01..04, MOOZombie3Argonian03..04
    0x0110B939, 0x0110B93A, 0x0110B93B, 0x0110B93C, 0x0110B94D, 0x0110B94E, 0x0110B94F, 0x0110B950,
    0x011096A4, 0x011096A5,
    # Orc: MOOSummonZombie1Orc01..04, MOOSummonZombie2Orc01..04, MOOZombie3Orc03..04
    0x0110B949, 0x0110B94A, 0x0110B94B, 0x0110B94C, 0x0110B95D, 0x0110B95E, 0x0110B95F, 0x0110B960,
    0x011096B0, 0x011096B1,
    # High Elf: MOOZombie2HighElf01, MOOZombie3HighElf01..04, MOOZombie2HighElf02..04
    0x011096C2, 0x011096C3, 0x011096C4, 0x011096C5, 0x011096C6, 0x011096C7, 0x011096C8, 0x011096C9,
    # Dark Elf: MOOZombie2DarkElf02..04, MOOSummonZombie1DarkElf01..04, MOOSummonZombie2DarkElf01
    0x011096BF, 0x011096C0, 0x011096C1, 0x0110B93D, 0x0110B93E, 0x0110B93F, 0x0110B940, 0x0110B951,
]
LOCAL_MOO_WALKER_IDS = [LOCAL_MORE_WALKER_IDS[-1] + 1 + i for i in range(len(MOO_DONORS))]  # WalkerLooks.h
ACBS_NO_HEAD = 0x8000
ACBS_NO_RIGHT_ARM = 0x10000
ACBS_NO_LEFT_ARM = 0x20000
LOCAL_LIST_ID = 0x805
LOCAL_ANY_WALKER_ID = 0x94A  # ReplaceLogic.h kAnyWalkerLocalId: right after the last walker
LOCAL_LINK_QUEST_ID = 0x94B  # LinkLogic.h kLinkQuestLocalId
LOCAL_LINK_SCRIPT_ID = 0x94C  # LinkLogic.h kLinkScriptLocalId
LINK_QUEST_EDID = "Walkers"  # LinkLogic.h kLinkQuestEditorId
LINK_SCRIPT_EDID = "WalkersLinkScript"
# IDLE WalkersGrab* (4.19, GrabLogic.h): the grab's three animations
# (tools/grab-anims.py) for each zombie skeleton, the vanilla one's first.
GRAB_ANIMS = [("Hold", "WalkersGrabHold.kf"), ("Bite", "WalkersGrabBite.kf"), ("LetGo", "WalkersGrabLetGo.kf")]
GRAB_SKELETONS = [("Zombie", "Creatures\\Zombie\\IdleAnims"), ("Moo", "MOO\\zombie\\IdleAnims")]
LOCAL_GRAB_IDLE_FIRST = 0x94D  # Grab.cpp kGrabIdleLocalFirst: hold, bite, let go; then MOO's three
# The zombie skeleton's last idle in Oblivion.esm (CreaturesZombieIdleAnimsIdleSway,
# read 2026-10-28): the grab's idles follow it in the idle tree.
ZOMBIE_LAST_IDLE = 0x0002ACCD
# A condition never true, GetRandomPercent (77) < 0, so the idle manager never
# picks them for a zombie on its own - the plugin plays them (CTDA as
# Oblivion.esm's: type and compare (0x80 less than), value, function, two
# parameters, unused).
NEVER = struct.pack("<B3xfIIII", 0x80, 0.0, 77, 0, 0, 0)
IDLE_SPECIAL = 0x04  # ANAM: the animation group section Special Idle, as the zombie's IdleSway
LOCAL_TEST_POOL_FIRST = LOCAL_GRAB_IDLE_FIRST + len(GRAB_ANIMS) * len(GRAB_SKELETONS)
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
# LVLF bit 2, "calculate for each item in count": TESLeveledList's
# CalcItemForContainer (0x0046CDE0) draws such a list once per count, read
# with REA 2026-10-27; without it one draw is copied count times.
LIST_CALC_EACH = 0x02
LIST_EDID = "WalkersZombieVariant"
ANY_WALKER_EDID = "WalkersAnyWalker"
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


# The ESL flag OblivionESL (Nexus 56578, github.com/googlepox/OblivionESL
# ESLHooks.cpp, read 2026-10-28) reads off the TES4 record's flags: 0x200 as
# the TES4Edit custom build sets it (0x80000 is its own, also taken). With
# it OblivionESL loads the ESP as a light plugin, its forms at 0xFE |
# light index << 12 | local ID - every local ID must lie in 0x800..0xFFF and
# there may be 2048 records at most.
ESL_FLAG = 0x200
LIGHT_LOCAL_LAST = 0xFFF
LIGHT_RECORDS_MOST = 2048


def header(num_records: int, next_object_id: int, masters: list[str], flags: int = 0) -> bytes:
    data = subrecord(b"HEDR", struct.pack("<fiI", 1.0, num_records, next_object_id))
    data += subrecord(b"CNAM", zstring(AUTHOR))
    data += subrecord(b"SNAM", zstring(DESCRIPTION))
    for master in masters:
        data += subrecord(b"MAST", zstring(master))
        data += subrecord(b"DATA", struct.pack("<Q", 0))
    return record(b"TES4", 0, data, flags)


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


def part_names(nifz: bytes) -> list[str]:
    return sorted(p.decode("latin-1").lower() for p in nifz.split(b"\0") if p)


def arm_template(parts: list[str], walker_templates: list[list[tuple[bytes, bytes]]]):
    """CreatureZombie1 with both arms, 2 without the right, 3 without the
    left - the one whose arm flags, animations and bound fit the parts."""
    right = any("rightarm" in p for p in parts)
    left = any("leftarm" in p for p in parts)
    if right and left:
        return walker_templates[0]
    if left:
        return walker_templates[1]
    if right:
        return walker_templates[2]
    raise SystemExit(f"a part set without either arm: {parts}")


def headless(parts: list[str]) -> bool:
    """No head among the parts (vanilla head01, MOO's dehead01, arg01head,
    orc01head and the like)."""
    return not any("head" in p for p in parts)


def build_more_walker(template: list[tuple[bytes, bytes]], donor: dict, keys: tuple, edid: str) -> bytes:
    """The arm template under the Walker's names with the donor's `keys`
    (its parts, and for MOO's its skeleton, bound, textures and
    animations), No Head set when no head is among the parts."""
    no_head = headless(part_names(donor[b"NIFZ"]))
    out = b""
    for kind, data in template:
        if kind == b"EDID":
            data = zstring(edid)
        elif kind == b"FULL":
            data = zstring(WALKER_NAME)
        elif kind in keys and kind in donor:
            data = donor[kind]
        elif kind == b"ACBS" and no_head:
            data = struct.pack("<I", struct.unpack_from("<I", data, 0)[0] | ACBS_NO_HEAD) + data[4:]
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


def variant_entries(any_walker: int) -> list[tuple[int, int]]:
    """VARIANT_SLOTS entries, each the any-walker list, at the counts
    1..slots - the plugin writes them anew from MinCount and MaxCount."""
    return [(any_walker, slot + 1) for slot in range(VARIANT_SLOTS)]


def any_walker_entries(walkers: list[int], no_heads: list[bool]) -> list[tuple[int, int]]:
    """Every walker at count 1, twice, a headless one once (ReplaceLogic.h
    AnyWalkerTimes; Nadi, 2026-10-27: half as often), in the walkers' order -
    the vanilla looks first, so the plugin cuts the MOO ones with one link."""
    return [(form, 1) for form, no_head in zip(walkers, no_heads) for _ in range(1 if no_head else 2)]


def link_variables() -> list[str]:
    """The LINK settings' variable names, read off src/LinkLogic.h's
    kLinkSettings in its order, then ini_version - one list for the plugin
    and the ESP."""
    text = (Path(__file__).resolve().parent.parent / "src" / "LinkLogic.h").read_text()
    table = text[text.index("kLinkSettings[] = {"):text.index("kLinkSettingCount")]
    names = re.findall(r'\{"(ini_[a-z_]+)"', table)
    if len(names) != len(set(names)) or not names:
        raise SystemExit("LinkLogic.h's settings are not readable")
    return names + ["ini_version"]


def build_link_script(names: list[str]) -> bytes:
    """A quest script that only declares LINK's variables, all floats: SCHR
    (unused, refs 0, compiled size 4, the last variable's index, type 1
    quest), SCDA the bare script name opcode 1D 00 00 00, SCTX the source,
    and per variable SLSD (index, 12 unused, flags 0 = float, 7 unused) and
    SCVR (its name) - as Ultimate Leveling.esp's variables-only scripts and
    ULVLScript have them (read 2026-10-27)."""
    source = "scn " + LINK_SCRIPT_EDID + "\r\n\r\n" + "".join(f"float {n}\r\n" for n in names)
    out = subrecord(b"EDID", zstring(LINK_SCRIPT_EDID))
    out += subrecord(b"SCHR", struct.pack("<IIIII", 0, 0, 4, len(names), 1))
    out += subrecord(b"SCDA", bytes([0x1D, 0, 0, 0]))
    out += subrecord(b"SCTX", source.encode("latin-1"))
    for i, name in enumerate(names, start=1):
        out += subrecord(b"SLSD", struct.pack("<I12sB7s", i, bytes(12), 0, bytes(7)))
        out += subrecord(b"SCVR", zstring(name))
    return out


def build_grab_idle(edid: str, path: str, previous: int) -> bytes:
    """EDID, MODL (the KF beneath the skeleton's folder), MODB, the never-true
    condition, ANAM, DATA (no parent, the sibling before it) - as Oblivion.esm's
    zombie idles."""
    return (subrecord(b"EDID", zstring(edid)) + subrecord(b"MODL", zstring(path))
            + subrecord(b"MODB", struct.pack("<f", 0.0)) + subrecord(b"CTDA", NEVER)
            + subrecord(b"ANAM", bytes([IDLE_SPECIAL])) + subrecord(b"DATA", struct.pack("<II", 0, previous)))


def build_link_quest(script_form: int) -> bytes:
    """The quest LINK's settings live in: EDID, SCRI its script, DATA flags
    1 (start game enabled) and priority 0, like ULVLOBSEInit."""
    return (subrecord(b"EDID", zstring(LINK_QUEST_EDID)) + subrecord(b"SCRI", struct.pack("<I", script_form))
            + subrecord(b"DATA", bytes([1, 0])))


def build_list(entries: list[tuple[int, int]], edid: str = "", flags: int = 0) -> bytes:
    """A leveled creature list of the (form, count) entries at level 1: EDID,
    LVLD (chance none), LVLF (flags), then LVLO level(2) pad(2) form(4)
    count(2) pad(2)."""
    out = subrecord(b"EDID", zstring(edid or LIST_EDID))
    out += subrecord(b"LVLD", bytes([0]))
    out += subrecord(b"LVLF", bytes([flags]))
    for form, count in entries:
        out += subrecord(b"LVLO", struct.pack("<HHIHH", 1, 0, form, count, 0))
    return out


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("esm", help="Oblivion.esm")
    parser.add_argument("moo", help="Maskar's Oblivion Overhaul.esp - the parts of the walkers' MOO looks")
    parser.add_argument("--out", default=str(HERE.parent / "OblivionWalkers.esp"))
    parser.add_argument("--esl", action="store_true",
                        help="ESL-flag the ESP for OblivionESL (Nadi, 2026-10-28: the release offers it first, "
                             "the plain ESP beside it); the same records and IDs")
    parser.add_argument("--test-walker-pool", type=int, default=0, metavar="N",
                        help="a measurement only (4.12, 2026-10-26): N persistent, initially disabled walker "
                             "references per city in the holding cell, after the last walker - what such a pool "
                             "costs the game before one is built; never for a deployed ESP")
    args = parser.parse_args()
    esm = load_esm_module()
    buf = Path(args.esm).read_bytes()
    moo_buf = Path(args.moo).read_bytes()

    masters = ["Oblivion.esm"]
    index = len(masters) << 24

    template = find_record(esm, buf, b"RACE", IMPERIAL_RACE)
    race_form = index | LOCAL_RACE_ID
    races = record(b"RACE", race_form, build_race(template))

    walker_templates = [find_record(esm, buf, b"CREA", source) for source in ZOMBIE_SOURCES]
    first_forms = [index | local for local in LOCAL_WALKER_IDS]
    creatures = b"".join(record(b"CREA", form, build_walker(tpl, f"WalkersZombie{i + 1}"))
                         for i, (form, tpl) in enumerate(zip(first_forms, walker_templates)))
    donors = [dict(find_record(esm, buf, b"CREA", donor)) for donor in PART_DONORS]
    donor_keys = [VANILLA_KEYS] * len(donors)
    for donor in MOO_DONORS:
        found = dict(find_record(esm, moo_buf, b"CREA", donor))
        if b"moo\\zombie\\skeleton.nif" not in found[b"MODL"].lower():
            raise SystemExit(f"MOO's {donor:08X} is not on MOO's zombie skeleton")
        donors.append(found)
        donor_keys.append(MOO_KEYS)
    donor_parts = [d[b"NIFZ"] for d in donors]
    more_forms = [index | local for local in LOCAL_MORE_WALKER_IDS + LOCAL_MOO_WALKER_IDS]
    more_templates = [arm_template(part_names(nifz), walker_templates) for nifz in donor_parts]
    creatures += b"".join(
        record(b"CREA", form, build_more_walker(tpl, donor, keys, f"WalkersZombie{len(first_forms) + i + 1}"))
        for i, (form, tpl, donor, keys) in enumerate(zip(more_forms, more_templates, donors, donor_keys)))
    walker_forms = first_forms + more_forms
    all_parts = [(dict(t)[b"MODL"].lower(), tuple(part_names(dict(t)[b"NIFZ"]))) for t in walker_templates] + \
        [((d[b"MODL"] if k == MOO_KEYS else dict(t)[b"MODL"]).lower(), tuple(part_names(d[b"NIFZ"])))
         for d, k, t in zip(donors, donor_keys, more_templates)]
    if len(set(all_parts)) != len(all_parts):
        raise SystemExit("two walkers wear the same parts")
    list_form = index | LOCAL_LIST_ID
    walker_no_heads = [headless(list(p)) for _, p in all_parts]
    any_walker_form = index | LOCAL_ANY_WALKER_ID
    lists = (record(b"LVLC", list_form, build_list(variant_entries(any_walker_form)))
             + record(b"LVLC", any_walker_form,
                      build_list(any_walker_entries(walker_forms, walker_no_heads), ANY_WALKER_EDID, LIST_CALC_EACH)))

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
    test_pool = 0
    for c in range(len(CITIES)):
        for n in range(args.test_walker_pool):
            local = LOCAL_TEST_POOL_FIRST + test_pool
            pool += record(b"ACRE", index | local,
                           build_pool_ref(achr_template, f"WalkersTestPool{CITIES[c]}{n}", walker_forms[n % 4],
                                          test_pool),
                           flags=RECORD_PERSISTENT | RECORD_INITIALLY_DISABLED)
            test_pool += 1

    edition_texts = load_editions_module().editions()
    if len(edition_texts) != EDITION_COUNT:
        raise SystemExit(f"{len(edition_texts)} editions, NewsLogic.h has {EDITION_COUNT}")
    broadsheet = find_record(esm, buf, b"BOOK", BROADSHEET_TEMPLATE)
    edition_forms = [index | local for local in LOCAL_EDITION_IDS]
    books = b"".join(record(b"BOOK", form, build_edition(broadsheet, edid, title, text))
                     for form, (edid, title, text) in zip(edition_forms, edition_texts))
    idle_forms = []
    idles = b""
    for k, (skeleton, folder) in enumerate(GRAB_SKELETONS):
        previous = ZOMBIE_LAST_IDLE if k == 0 else 0
        for a, (anim, kf) in enumerate(GRAB_ANIMS):
            form = index | (LOCAL_GRAB_IDLE_FIRST + k * len(GRAB_ANIMS) + a)
            idles += record(b"IDLE", form, build_grab_idle(f"WalkersGrab{anim}{skeleton}", f"{folder}\\{kf}", previous))
            idle_forms.append(form)
            previous = form
    link_names = link_variables()
    link_script_form = index | LOCAL_LINK_SCRIPT_ID
    link_quest_form = index | LOCAL_LINK_QUEST_ID
    body = (group(b"RACE", races) + group(b"SCPT", record(b"SCPT", link_script_form, build_link_script(link_names)))
            + group(b"BOOK", books) + group(b"NPC_", guards) + group(b"CREA", creatures)
            + group(b"LVLC", lists)
            + cell_groups(cell_form, cell, markers + pool)
            + group(b"QUST", record(b"QUST", link_quest_form, build_link_quest(link_script_form)))
            + group(b"PACK", packages) + group(b"IDLE", idles))
    groups = 5 + 5 + 1 + 2 + 1
    records = (1 + len(edition_forms) + len(guard_forms) + len(walker_forms) + 1 + 1 + len(marker_forms)
               + len(pool_forms) + test_pool + len(travel_forms) + len(follow_forms) + 1 + 2 + len(idle_forms))
    next_id = LOCAL_TEST_POOL_FIRST + test_pool
    if args.esl and (next_id - 1 > LIGHT_LOCAL_LAST or records > LIGHT_RECORDS_MOST):
        raise SystemExit(f"--esl: the last local ID {next_id - 1:03X} or {records} records do not fit a light plugin")
    if next_id - 1 > LIGHT_LOCAL_LAST and not args.test_walker_pool:
        raise SystemExit(f"the last local ID {next_id - 1:03X} is past 0xFFF: the ESL variant (and the plugin's "
                         "0xFFF mask, RiseLogic.h kEspLocalMask) would break")
    plugin = header(num_records=groups + records,
                    next_object_id=next_id,
                    masters=masters, flags=ESL_FLAG if args.esl else 0) + body
    out = Path(args.out)
    out.parent.mkdir(parents=True, exist_ok=True)
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
    for i, (form, tpl) in enumerate(zip(first_forms, walker_templates)):
        back = find_record(esm, plugin, b"CREA", form)
        if [(k, d) for k, d in back if k not in (b"EDID", b"FULL")] != \
                [(k, d) for k, d in tpl if k not in (b"EDID", b"FULL")]:
            raise SystemExit(f"walker {i + 1} differs from its zombie beyond the names")
        back_names = {k.decode(): d.rstrip(b"\0").decode("latin-1") for k, d in back if k in (b"EDID", b"FULL")}
        if back_names != {"EDID": f"WalkersZombie{i + 1}", "FULL": WALKER_NAME}:
            raise SystemExit(f"walker {i + 1}'s names are off: {back_names}")
    for i, (form, tpl, donor, keys) in enumerate(zip(more_forms, more_templates, donors, donor_keys)):
        n = len(first_forms) + i + 1
        back = find_record(esm, plugin, b"CREA", form)
        no_head = headless(part_names(donor[b"NIFZ"]))
        keep = (b"EDID", b"FULL", b"ACBS") + keys
        if [(k, d) for k, d in back if k not in keep] != [(k, d) for k, d in tpl if k not in keep]:
            raise SystemExit(f"walker {n} differs from its arm template beyond names, donor's keys and flags")
        got = dict(back)
        flags = struct.unpack_from("<I", got[b"ACBS"], 0)[0]
        want = struct.unpack_from("<I", dict(tpl)[b"ACBS"], 0)[0] | (ACBS_NO_HEAD if no_head else 0)
        if any(got.get(k) != (donor[k] if k in donor else dict(tpl).get(k)) for k in keys) or flags != want \
                or got[b"ACBS"][4:] != dict(tpl)[b"ACBS"][4:]:
            raise SystemExit(f"walker {n}'s parts or flags are off")
        if got[b"EDID"].rstrip(b"\0").decode() != f"WalkersZombie{n}" or got[b"FULL"].rstrip(b"\0").decode() != WALKER_NAME:
            raise SystemExit(f"walker {n}'s names are off")
    if LOCAL_MORE_WALKER_IDS[0] <= LOCAL_EDITION_IDS[-1]:
        raise SystemExit("the new walkers overlap the editions")
    back_list = find_record(esm, plugin, b"LVLC", list_form)
    entries = [struct.unpack_from("<HHIHH", d, 0) for k, d in back_list if k == b"LVLO"]
    if [(e[2], e[3]) for e in entries] != variant_entries(any_walker_form) or any(e[0] != 1 for e in entries):
        raise SystemExit(f"the list's entries are off: {[(f'{e[2]:08X}', e[3]) for e in entries]}")
    back_quest = dict(find_record(esm, plugin, b"QUST", link_quest_form))
    back_script = find_record(esm, plugin, b"SCPT", link_script_form)
    if back_quest[b"SCRI"] != struct.pack("<I", link_script_form) or back_quest[b"DATA"] != bytes([1, 0]) \
            or [d.rstrip(b"\0").decode() for k, d in back_script if k == b"SCVR"] != link_names:
        raise SystemExit("the LINK quest or its script is off")
    if LOCAL_ANY_WALKER_ID != LOCAL_MOO_WALKER_IDS[-1] + 1:
        raise SystemExit("the any-walker list is not right after the last walker")
    back_any = find_record(esm, plugin, b"LVLC", any_walker_form)
    any_entries = [struct.unpack_from("<HHIHH", d, 0) for k, d in back_any if k == b"LVLO"]
    if [(e[2], e[3]) for e in any_entries] != any_walker_entries(walker_forms, walker_no_heads) \
            or any(e[0] != 1 for e in any_entries) or dict(back_any)[b"LVLF"] != bytes([LIST_CALC_EACH]):
        raise SystemExit("the any-walker list's entries or flags are off")
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
    for form in idle_forms:
        back = dict(find_record(esm, plugin, b"IDLE", form))
        if back[b"CTDA"] != NEVER or not back[b"MODL"].rstrip(b"\0").lower().endswith(b".kf") \
                or back[b"ANAM"] != bytes([IDLE_SPECIAL]):
            raise SystemExit(f"grab idle {form:08X} reads back wrong")
    if LOCAL_GRAB_IDLE_FIRST != LOCAL_LINK_SCRIPT_ID + 1:
        raise SystemExit("the grab's idles are not right after the LINK script")
    if LOCAL_EDITION_IDS[0] <= LOCAL_POOL_IDS[-1]:
        raise SystemExit("the editions overlap the guard pool")
    if (pool_local_id(0, 0) != 0x83F or pool_local_id(7, 5) != 0x86E or pool_local_id(0, 6) != 0x86F
            or pool_local_id(7, 11) != 0x89E):
        raise SystemExit("the pool's first banks moved: a save made with a smaller pool would lose its guards")
    if struct.unpack_from("<I", plugin, 8)[0] != (ESL_FLAG if args.esl else 0):
        raise SystemExit("the header's flags read back wrong")
    print(f"{out}: {len(plugin)} bytes{' (ESL-flagged)' if args.esl else ''} - RACE {race_form:08X} {RACE_EDID}, CREA "
          + ", ".join(f"{f:08X}" for f in walker_forms)
          + f" WalkersZombie1..{len(walker_forms)} '{WALKER_NAME}' ({len(MOO_DONORS)} of MOO's looks, "
          + f"{sum(1 for _, p in all_parts if headless(list(p)))} headless), "
          + f"LVLC {list_form:08X} {LIST_EDID} - {VARIANT_SLOTS} counts of LVLC {any_walker_form:08X} "
          + f"{ANY_WALKER_EDID} ({len(any_walker_entries(walker_forms, walker_no_heads))} entries, drawn per walker); "
          + f"PACK {travel_forms[0]:08X}.. travel and {follow_forms[0]:08X}.. follow x{HORDE_GROUPS}, CELL {cell_form:08X} "
          + f"{CELL_EDID} with markers {marker_forms[0]:08X}..{marker_forms[-1]:08X} and {len(pool_forms)} pool guards "
          + f"{pool_forms[0]:08X}..{pool_forms[-1]:08X} of NPC_ {guard_forms[0]:08X}..{guard_forms[-1]:08X}; "
          + f"IDLE {idle_forms[0]:08X}..{idle_forms[-1]:08X} the grab's animations; "
          "every other subrecord as the originals'")
    return 0


if __name__ == "__main__":
    sys.exit(main())
