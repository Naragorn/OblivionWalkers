#!/usr/bin/env python3
"""Prints where one stands outside each city gate of src/CityGates.h.

For every Tamriel-side door in the table, the city-side door whose
teleport (XTEL) leads back to it, and the spot that teleport puts the
player on: in front of the gate, on the ground, facing away from it - where
the gate guards come out (docs/walkers-spec.md 4.11b). The door's own
position (DATA) is printed beside it as a check against the table.

    python3 tools/city-gates.py ~/.local/share/Steam/steamapps/common/Oblivion/Data/Oblivion.esm

XTEL is the destination door's form ID (4) and the arrival position and
rotation (6 floats: x y z, rx ry rz), DATA a reference's position and
rotation (UESP "Oblivion Mod:Mod File Format/REFR").
"""

from __future__ import annotations

import argparse
import importlib.util
import math
import re
import struct
from pathlib import Path

HERE = Path(__file__).resolve().parent


def load_esm_module():
    spec = importlib.util.spec_from_file_location("esm_spawns", HERE / "esm-spawns.py")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def gate_doors() -> list[tuple[int, str, str]]:
    text = (HERE.parent / "src" / "CityGates.h").read_text()
    return [(int(door, 16), city, name) for city, door, name in
            re.findall(r"\{City::(\w+), 0x([0-9A-Fa-f]{8}),[^\"]*\"([^\"]+)\"\}", text)]


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("esm", help="Oblivion.esm")
    args = parser.parse_args()
    esm = load_esm_module()
    buf = Path(args.esm).read_bytes()
    doors = gate_doors()
    wanted = {door for door, _, _ in doors}
    arrivals: dict[int, tuple[int, tuple[float, ...]]] = {}
    positions: dict[int, tuple[float, ...]] = {}

    def on_record(typ, flags, form_id, offset, size, cell):
        if typ != b"REFR":
            return
        data = esm.record_data(buf, offset, size, flags)
        for kind, sub in esm.subrecords(data):
            if kind == b"XTEL" and len(sub) >= 28:
                target = struct.unpack_from("<I", sub, 0)[0]
                if target in wanted:
                    arrivals[target] = (form_id, struct.unpack_from("<6f", sub, 4))
            elif kind == b"DATA" and form_id in wanted and len(sub) >= 24:
                positions[form_id] = struct.unpack_from("<6f", sub, 0)

    esm.walk(buf, 0, len(buf), [], on_record)
    rows = []
    for door, city, name in doors:
        if door not in arrivals or door not in positions:
            print(f"{door:08X} {name}: no way back found")
            continue
        back, (x, y, z, _rx, _ry, rz) = arrivals[door]
        dx, dy, dz = positions[door][:3]
        print(f"{door:08X} {name:40s} out {x:9.0f} {y:9.0f} {z:7.0f} facing {math.degrees(rz):5.0f} "
              f"(door at {dx:.0f} {dy:.0f} {dz:.0f}, {math.hypot(x - dx, y - dy):.0f} units off; back door {back:08X})")
        rows.append(f"\t{{City::{city}, 0x{door:08X}, {dx:.1f}f, {dy:.1f}f, {dz:.1f}f, {x:.1f}f, {y:.1f}f, {z:.1f}f, "
                    f"\"{name}\"}},")
    print("\nThe rows of src/CityGates.h's kCityGates:")
    print("\n".join(rows))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
