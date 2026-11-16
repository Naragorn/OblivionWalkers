#!/usr/bin/env python3
"""Writes src/DaedricArmor.h: the pieces of Daedric armor in Oblivion.esm.

Idea 7 of docs/walkers-spec.md 4.22 (the Daedric disguise): in a full set of
Daedric armor the Daedra take the player for one of theirs. A piece is Daedric
when its model (ARMO MODL) lies under Armor\\Daedric\\ - the plain pieces,
the enchanted ones, the Dremora's own and the bound armor (conjured Daedric
armor, the same meshes). Its body slots are BMDT's low bits: 0x01 head
(0x02 hair beside it), 0x04 upper body, 0x08 lower body, 0x10 hands, 0x20
feet. Shields (0x2000 only) and the Daedric bracers (Armor\\DaedricBracers)
are left out.

    python3 tools/daedric-armor.py <Data>/Oblivion.esm [--out src/DaedricArmor.h] [--check]

--check writes nothing and fails when the header differs from what it would
write.
"""

from __future__ import annotations

import argparse
import importlib.util
import struct
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location("esm_spawns", HERE / "esm-spawns.py")
esm = importlib.util.module_from_spec(spec)
spec.loader.exec_module(esm)

SLOTS = 0x3D  # head, upper, lower, hands, feet


def read(path: Path) -> list[tuple[int, str, int]]:
    buf = path.read_bytes()
    found: list[tuple[int, str, int]] = []

    def on_record(typ, flags, form_id, offset, size, _cell):
        if typ != b"ARMO":
            return
        data = esm.record_data(buf, offset, size, flags)
        edid, model, slots = "", "", 0
        for st, sd in esm.subrecords(data):
            if st == b"EDID":
                edid = sd.rstrip(b"\0").decode("latin-1")
            elif st == b"MODL" and not model:
                model = sd.rstrip(b"\0").decode("latin-1")
            elif st == b"BMDT" and len(sd) >= 4:
                slots = struct.unpack_from("<I", sd, 0)[0] & SLOTS
        if model.lower().replace("/", "\\").startswith("armor\\daedric\\") and slots:
            found.append((form_id, edid, slots))

    esm.walk(buf, 0, len(buf), [], on_record)
    return sorted(found)


def header(found) -> str:
    lines = [
        "#pragma once",
        "",
        "// Written by tools/daedric-armor.py off Oblivion.esm - do not edit; run it again.",
        "// The pieces of Daedric armor (docs/walkers-spec.md 4.22 idea 7, the Daedric",
        "// disguise): every ARMO whose model lies under Armor\\Daedric\\ - the plain,",
        "// the enchanted, the Dremora's and the bound pieces - with its body slots",
        "// (BMDT: 0x01 head, 0x04 upper body, 0x08 lower body, 0x10 hands, 0x20",
        "// feet). Shields and the Daedric bracers are left out.",
        "",
        '#include "Types.h"',
        "",
        "namespace walkers {",
        "",
        "struct DaedricPiece {",
        "\tUInt32 formId;  // Oblivion.esm's",
        "\tUInt8 slots;",
        "};",
        "",
        "inline constexpr DaedricPiece kDaedricPieces[] = {",
    ]
    for form_id, edid, slots in found:
        lines.append(f"\t{{0x{form_id:08X}, 0x{slots:02X}}},  // {edid}")
    lines += [
        "};",
        "inline constexpr UInt32 kDaedricPieceCount = sizeof(kDaedricPieces) / sizeof(kDaedricPieces[0]);",
        "",
        "}  // namespace walkers",
        "",
    ]
    return "\n".join(lines)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("esm", type=Path)
    parser.add_argument("--out", type=Path, default=HERE.parent / "src" / "DaedricArmor.h")
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    found = read(args.esm)
    text = header(found)
    if args.check:
        old = args.out.read_text() if args.out.exists() else ""
        if old != text:
            print(f"{args.out} differs from what Oblivion.esm gives", file=sys.stderr)
            return 1
        print(f"{args.out}: up to date")
        return 0
    args.out.write_text(text)
    print(f"{args.out}: {len(found)} pieces written")
    return 0


if __name__ == "__main__":
    sys.exit(main())
