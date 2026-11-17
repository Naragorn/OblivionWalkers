#!/usr/bin/env python3
"""Writes src/DaedraLists.h: the Daedra's summoners and their NOSUMMON twins, read off Oblivion.esm.

Also the Dremora rank lists' own entries, as the file has them (kRankListActors):
MOO's dynamic lists rewrite those lists in the game (daedra-leader.txt
2026-11-17: LL0Dremora2Kynval100 placed gave two clannfear runts, its
NOSUMMON twin a creature of MOO's), so a band's leader and an outpost's boss
are placed as one of these actors, not through the list.

Idea 11 of docs/walkers-spec.md 4.22 (the summoners held back): Oblivion.esm
carries, beside the Dremora's ranks, the spider daedra and the xivilai, a
twin of each whose editor ID ends in NOSUMMON. This reads both sides - the
actors (NPC_, CREA) and the leveled lists (LVLC) - and keeps a pair only when
the file shows it: the original's spells (SPLO, its leveled spell lists walked through) hold a summoning effect (a
magic effect code beginning with 'Z', Oblivion's summons: ZDRE, ZSPD, ZXIV
...), the twin's hold none. The lists pair by name; an actor's twin is its
name less a trailing number plus NOSUMMON (Dremora0ChurlMelee1, 2 and 3 share
Dremora0ChurlMeleeNOSUMMON).

    python3 tools/daedra-lists.py <Data>/Oblivion.esm [--out src/DaedraLists.h] [--check]

--check writes nothing and fails when the header differs from what it would
write.
"""

from __future__ import annotations

import argparse
import importlib.util
import re
import struct
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location("esm_spawns", HERE / "esm-spawns.py")
esm = importlib.util.module_from_spec(spec)
spec.loader.exec_module(esm)


def read(path: Path):
    buf = path.read_bytes()
    records: dict[int, dict] = {}

    def on_record(typ, flags, form_id, offset, size, _cell):
        if typ not in (b"NPC_", b"CREA", b"LVLC", b"SPEL", b"LVSP"):
            return
        data = esm.record_data(buf, offset, size, flags)
        rec = {"type": typ.decode(), "edid": "", "spells": [], "effects": [], "entries": []}
        for st, sd in esm.subrecords(data):
            if st == b"EDID":
                rec["edid"] = sd.rstrip(b"\0").decode("latin-1")
            elif st == b"SPLO" and len(sd) >= 4:
                rec["spells"].append(struct.unpack_from("<I", sd, 0)[0])
            elif st == b"EFID" and len(sd) >= 4:
                rec["effects"].append(sd[:4].decode("latin-1"))
            elif st == b"LVLO" and len(sd) >= 8:
                rec["entries"].append(struct.unpack_from("<I", sd, 4)[0])
        records[form_id] = rec

    esm.walk(buf, 0, len(buf), [], on_record)
    return records


def spell_summons(records, form_id, seen) -> list[str]:
    """The summoning effects of a spell, or of every spell a leveled spell list (LVSP) can give."""
    s = records.get(form_id)
    if not s or form_id in seen:
        return []
    seen.add(form_id)
    if s["type"] == "SPEL":
        return [e for e in s["effects"] if e.startswith("Z")]
    if s["type"] == "LVSP":
        return [e for entry in s["entries"] for e in spell_summons(records, entry, seen)]
    return []


def summons(records, form_id) -> list[str]:
    """The summoning effects among an actor's spells, its leveled spell lists walked through."""
    seen: set[int] = set()
    return sorted({e for spell in records[form_id]["spells"] for e in spell_summons(records, spell, seen)})


def pairs(records):
    by_name = {r["edid"]: f for f, r in records.items() if r["edid"]}
    found = []
    for twin_id, twin in sorted(records.items()):
        name = twin["edid"]
        if not name.endswith("NOSUMMON") or twin["type"] not in ("NPC_", "CREA", "LVLC"):
            continue
        stem = name[: -len("NOSUMMON")]
        if twin["type"] == "LVLC":
            originals = [by_name[stem]] if stem in by_name else []
        else:
            pattern = re.compile(re.escape(stem) + r"\d*$")
            originals = [f for n, f in by_name.items() if pattern.fullmatch(n) and records[f]["type"] == twin["type"]]
        for original in sorted(originals):
            found.append((original, twin_id))
    return found


def verify(records, found):
    """Splits the actor pairs: an original that summons and a twin that does not (a summoner, kept); an original
    that summons nothing (the Dremora's melee and archers - read 2026-11-15: their spell lists are the twins'
    own, the twin only carries less; dropped, nothing to hold back). A twin that still summons is a problem. Each
    list pair: entry for entry, the twin holds the original's entry or its twin."""
    twin_of = dict(found)
    problems = []
    kept = []
    dropped = []
    for original, twin in found:
        o, t = records[original], records[twin]
        if o["type"] in ("NPC_", "CREA"):
            if summons(records, twin):
                problems.append(f"{t['edid']} still summons {summons(records, twin)}")
            elif summons(records, original):
                kept.append((original, twin))
            else:
                dropped.append(o["edid"])
            continue
        kept.append((original, twin))
        if len(o["entries"]) != len(t["entries"]):
            problems.append(f"{o['edid']} and {t['edid']} differ in length")
            continue
        for a, b in zip(o["entries"], t["entries"]):
            if twin_of.get(a, a) != b:
                problems.append(f"{o['edid']}: {records.get(a, {}).get('edid', hex(a))} -> "
                                f"{records.get(b, {}).get('edid', hex(b))} is not its twin")
    return problems, kept, dropped


# The seven Dremora rank lists (KindLogic.h kDremoraRankLists).
RANK_LISTS = (0x00092DA6, 0x0003E9D1, 0x0003E9D2, 0x0003E9D3, 0x0003E9D4, 0x0003E9D5, 0x0003E9D6)


def rank_actors(records, found):
    """Each rank list and its twin with the actors the file's entries name; a nested list is a problem."""
    twin_of = dict(found)
    rows, problems = [], []
    for original in RANK_LISTS:
        for form in (original, twin_of.get(original)):
            if form is None or form not in records:
                problems.append(f"rank list {form} not in the file")
                continue
            actors = records[form]["entries"]
            if not actors or any(records.get(a, {}).get("type") not in ("NPC_", "CREA") for a in actors):
                problems.append(f"{records[form]['edid']}: an entry not an actor")
                continue
            rows.append((form, records[form]["edid"], actors))
    return rows, problems


def header(records, found) -> str:
    lines = [
        "#pragma once",
        "",
        "// Written by tools/daedra-lists.py off Oblivion.esm - do not edit; run it again.",
        "// The Daedra's summoners and their NOSUMMON twins (docs/walkers-spec.md 4.22,",
        "// idea 11): each original's spells (its leveled spell lists walked through)",
        "// hold a summoning effect, its twin's none; a list's twin holds, entry for",
        "// entry, the twins. The Dremora's melee and archers summon nothing (their",
        "// NOSUMMON twins differ only in what they carry) - left out.",
        "",
        '#include "Types.h"',
        "",
        "namespace walkers {",
        "",
        "struct SummonTwin {",
        "\tUInt32 original;  // Oblivion.esm's form ID",
        "\tUInt32 twin;",
        "\tconst char* name;  // the original's editor ID",
        "\tbool list;         // a leveled list (LVLC), else an actor",
        "};",
        "",
        "inline constexpr SummonTwin kSummonTwins[] = {",
    ]
    for original, twin in found:
        o = records[original]
        lines.append(f'\t{{0x{original:08X}, 0x{twin:08X}, "{o["edid"]}", {"true" if o["type"] == "LVLC" else "false"}}},'
                     f"  // {', '.join(summons(records, original)) or 'list'}")
    lines += [
        "};",
        "inline constexpr UInt32 kSummonTwinCount = sizeof(kSummonTwins) / sizeof(kSummonTwins[0]);",
        "",
        "// The Dremora rank lists and their twins with the actors their entries name in",
        "// the file - MOO's dynamic lists rewrite the lists in the game, so the plugin",
        "// places one of these actors itself (a band's leader, an outpost's boss).",
        "inline constexpr UInt32 kRankActorsMost = 8;",
        "struct RankListActors {",
        "\tUInt32 list;  // Oblivion.esm's form ID",
        "\tUInt32 count;",
        "\tUInt32 actors[kRankActorsMost];",
        "};",
        "",
        "inline constexpr RankListActors kRankListActors[] = {",
    ]
    rows, _ = rank_actors(records, found)
    for form, edid, actors in rows:
        lines.append(f"\t{{0x{form:08X}, {len(actors)}, {{{', '.join(f'0x{a:08X}' for a in actors)}}}}},  // {edid}")
    lines += [
        "};",
        "",
        "// One actor of `list`'s file entries, by `roll`; `list` itself when it is not",
        "// a rank list.",
        "inline UInt32 RankListActor(UInt32 list, UInt32 roll) {",
        "\tfor (const RankListActors& r : kRankListActors) {",
        "\t\tif (r.list == list && r.count > 0) {",
        "\t\t\treturn r.actors[roll % r.count];",
        "\t\t}",
        "\t}",
        "\treturn list;",
        "}",
        "",
        "// The twin of `formId` (Oblivion.esm's ID, load index 0), or itself.",
        "inline UInt32 NoSummonTwin(UInt32 formId) {",
        "\tfor (const SummonTwin& t : kSummonTwins) {",
        "\t\tif (t.original == formId) {",
        "\t\t\treturn t.twin;",
        "\t\t}",
        "\t}",
        "\treturn formId;",
        "}",
        "",
        "}  // namespace walkers",
        "",
    ]
    return "\n".join(lines)


def main() -> int:
    p = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    p.add_argument("esm", type=Path)
    p.add_argument("--out", type=Path, default=HERE.parent / "src" / "DaedraLists.h")
    p.add_argument("--check", action="store_true")
    args = p.parse_args()
    records = read(args.esm)
    found = pairs(records)
    problems, found, dropped = verify(records, found)
    rows, rank_problems = rank_actors(records, found)
    problems += rank_problems
    problems += [f"{edid}: {len(a)} entries, more than 8" for _, edid, a in rows if len(a) > 8]
    for line in problems:
        print("daedra-lists:", line, file=sys.stderr)
    if problems:
        return 1
    text = header(records, found)
    lists = sum(1 for o, _ in found if records[o]["type"] == "LVLC")
    print(f"daedra-lists: {len(found)} pairs ({lists} lists, {len(found) - lists} summoners), all verified; "
          f"{len(dropped)} twins of actors that summon nothing left out")
    if args.check:
        same = args.out.exists() and args.out.read_text() == text
        print("daedra-lists:", "up to date" if same else f"{args.out} differs")
        return 0 if same else 1
    args.out.write_text(text)
    return 0


if __name__ == "__main__":
    sys.exit(main())
