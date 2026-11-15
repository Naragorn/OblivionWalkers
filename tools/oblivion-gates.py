#!/usr/bin/env python3
"""Writes src/OblivionGates.h: the Oblivion gates standing in Tamriel, read off Oblivion.esm.

Step 3 of docs/walkers-spec.md 4.22 (the invaders come out of the open
gates) reads at run time which gates are open. A gate is a door reference
whose base carries one of the gate scripts; most hang on an enable parent of
their own (XESP), which the game enables when the gate opens (the random
gates' OblivionGateRandomScript, read 2026-11-13). This lists every such door
placed in an exterior cell of Tamriel, with its parent and where it stands:
the plugin looks them up by ID, so no search over the world at run time.

    python3 tools/oblivion-gates.py <Data>/Oblivion.esm [--out src/OblivionGates.h] [--check]
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

TAMRIEL = 0x0000003C
# The gate scripts, by editor ID, and the kind they make a gate: the random
# gates the main quest's count opens, the five cities' gates of MQ11 (and
# MS13's on its script), and the quests' own.
RANDOM = {"OblivionGateRandomScript"}
CITY = {"MQ11OblivionGateScript"}


def walk(buf: bytes, start: int, end: int, world: list, cell: list, on_record):
    i = start
    while i + esm.HEADER <= end:
        if buf[i:i + 4] == b"GRUP":
            size, label, gtype = struct.unpack_from("<IIi", buf, i + 4)
            push_world = gtype == 1
            push_cell = gtype == 6
            if push_world:
                world.append(label)
            if push_cell:
                cell.append(label)
            walk(buf, i + esm.HEADER, i + size, world, cell, on_record)
            if push_world:
                world.pop()
            if push_cell:
                cell.pop()
            i += size
        else:
            size, flags, form_id = struct.unpack_from("<III", buf, i + 4)
            on_record(buf[i:i + 4], flags, form_id, i, size, world[-1] if world else 0, cell[-1] if cell else 0)
            i += esm.HEADER + size


def read(path: Path):
    buf = path.read_bytes()
    doors, scripts, refs = {}, {}, []

    def on_record(typ, flags, form_id, offset, size, world, cell):
        if typ not in (b"DOOR", b"SCPT", b"REFR"):
            return
        subs = list(esm.subrecords(esm.record_data(buf, offset, size, flags)))
        d = {}
        for st, sd in subs:
            d.setdefault(st, sd)
        edid = d.get(b"EDID", b"").rstrip(b"\0").decode("latin-1")
        if typ == b"DOOR":
            doors[form_id] = (edid, struct.unpack_from("<I", d[b"SCRI"])[0] if b"SCRI" in d else 0)
        elif typ == b"SCPT":
            scripts[form_id] = edid
        else:
            base = struct.unpack_from("<I", d[b"NAME"])[0] if b"NAME" in d else 0
            parent = struct.unpack_from("<I", d[b"XESP"])[0] if b"XESP" in d else 0
            pos = struct.unpack_from("<fff", d[b"DATA"]) if b"DATA" in d else (0.0, 0.0, 0.0)
            refs.append({"form": form_id, "base": base, "parent": parent, "pos": pos, "world": world,
                         "disabled": bool(flags & 0x800)})

    walk(buf, 0, len(buf), [], [], on_record)
    gates = []
    for r in refs:
        door = doors.get(r["base"])
        if door is None or r["world"] != TAMRIEL:
            continue
        script = scripts.get(door[1], "")
        if "OblivionGate" not in script or "Inside" in script:
            continue
        kind = "Random" if script in RANDOM else ("City" if script in CITY else "Quest")
        gates.append({**r, "door": door[0], "script": script, "kind": kind})
    return sorted(gates, key=lambda g: g["form"])


def header(gates) -> str:
    counts = {k: sum(1 for g in gates if g["kind"] == k) for k in ("Random", "City", "Quest")}
    lines = [
        "#pragma once",
        "",
        "// Written by tools/oblivion-gates.py off Oblivion.esm - do not edit; run it again.",
        "// The Oblivion gates standing in Tamriel (docs/walkers-spec.md 4.22, step 3):",
        f"// {len(gates)} doors - {counts['Random']} random (OblivionGateRandomScript), {counts['City']} of the cities",
        f"// (MQ11OblivionGateScript), {counts['Quest']} the quests' own. Their Oblivion.esm IDs (load index 0).",
        "",
        '#include "Types.h"',
        "",
        "namespace walkers {",
        "",
        "enum class GateKind : UInt8 { Random, City, Quest };",
        "",
        "struct OblivionGateRef {",
        "\tUInt32 door;    // the door reference",
        "\tUInt32 parent;  // its enable parent, 0 none",
        "\tfloat x, y, z;",
        "\tGateKind kind;",
        "\tbool initiallyDisabled;",
        "};",
        "",
        "inline constexpr OblivionGateRef kOblivionGates[] = {",
    ]
    for g in gates:
        x, y, z = g["pos"]
        lines.append(f"\t{{0x{g['form']:08X}, 0x{g['parent']:08X}, {x:.0f}.0f, {y:.0f}.0f, {z:.0f}.0f, GateKind::{g['kind']}, "
                     f"{'true' if g['disabled'] else 'false'}}},  // {g['door']}")
    lines += [
        "};",
        "inline constexpr UInt32 kOblivionGateCount = sizeof(kOblivionGates) / sizeof(kOblivionGates[0]);",
        "",
        "}  // namespace walkers",
        "",
    ]
    return "\n".join(lines)


def main() -> int:
    p = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    p.add_argument("esm", type=Path)
    p.add_argument("--out", type=Path, default=HERE.parent / "src" / "OblivionGates.h")
    p.add_argument("--check", action="store_true")
    args = p.parse_args()
    gates = read(args.esm)
    text = header(gates)
    kinds = {k: sum(1 for g in gates if g["kind"] == k) for k in ("Random", "City", "Quest")}
    print(f"oblivion-gates: {len(gates)} gates in Tamriel {kinds}, "
          f"{sum(1 for g in gates if g['parent'])} with an enable parent")
    if args.check:
        same = args.out.exists() and args.out.read_text() == text
        print("oblivion-gates:", "up to date" if same else f"{args.out} differs")
        return 0 if same else 1
    args.out.write_text(text)
    return 0


if __name__ == "__main__":
    sys.exit(main())
