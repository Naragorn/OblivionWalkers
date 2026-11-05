#!/usr/bin/env python3
"""Proposes the survivors' camp places (docs/walkers-spec.md 4.20.6) off Oblivion.esm's roads.

Reads the Tamriel worldspace (0000003C): its ROAD record's points (PGRP), every
exterior cell's grid (XCLC) and land heights (LAND VHGT), every door and map
marker reference (a DOOR base, an XMRK subrecord). A place is a road point
  - at least --gate-units from every city gate of src/CityGates.h,
  - at least --door-units from every door (a house, an inn, a fort, a cave),
  - at least --marker-units from every map marker (a place of its own),
  - above the water (the land's height over --water-z),
then moved --side-units off the road at a right angle, to the side whose land
is the flatter; there the land within --flat-units must rise or fall no more
than --slope-units. The places are picked one after another, each the farthest
from those picked already (the first the farthest from the Imperial City), so
they spread over the map. Each is named after the nearest map marker
that is no Oblivion gate's.

    python3 tools/camp-places.py ~/.local/share/Steam/steamapps/common/Oblivion/Data/Oblivion.esm

Prints src/CampPlaces.h's rows. PGRP is x y z (floats) and a connection count
(1 byte) and 3 bytes padding per point, PGRR each point's connections in turn as
the linked point's x y z - the roads are sampled every --sample-units along them; VHGT a float offset and 33x33 signed
bytes, each the step from the one before (the row's first from the row's first
before it), times 8 for units; a cell is 4096 units, its land 32 steps of 128
(UESP "Oblivion Mod:Mod File Format/LAND", read 2026-11-05 off the file).
"""

from __future__ import annotations

import argparse
import importlib.util
import math
import re
import struct
from pathlib import Path

HERE = Path(__file__).resolve().parent
TAMRIEL = 0x0000003C
CELL_UNITS = 4096.0
LAND_STEP = 128.0
HEADER = 20


def load_esm_module():
    spec = importlib.util.spec_from_file_location("esm_spawns", HERE / "esm-spawns.py")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def gate_positions() -> list[tuple[float, float]]:
    text = (HERE.parent / "src" / "CityGates.h").read_text()
    return [(float(x), float(y)) for x, y in
            re.findall(r"\{City::\w+, 0x[0-9A-Fa-f]{8}, (-?[\d.]+)f, (-?[\d.]+)f,", text)]


def tamriel_range(buf: bytes) -> tuple[int, int]:
    """The Tamriel world's children group (type 1, label 0000003C)."""
    i = 0
    while i + HEADER <= len(buf):
        if buf[i:i + 4] == b"GRUP":
            size, label, gtype = struct.unpack_from("<IIi", buf, i + 4)
            if gtype == 1 and label == TAMRIEL:
                return i + HEADER, i + size
            if gtype == 0 and buf[i + 8:i + 12] != b"WRLD":
                i += size
                continue
            i += HEADER
            continue
        size = struct.unpack_from("<I", buf, i + 4)[0]
        i += HEADER + size
    raise SystemExit("no Tamriel world group")


def heights(vhgt: bytes) -> list[list[float]]:
    offset = struct.unpack_from("<f", vhgt, 0)[0]
    rows = []
    row_start = offset
    for r in range(33):
        row_start += struct.unpack_from("<b", vhgt, 4 + r * 33)[0]
        value = row_start
        row = [value * 8.0]
        for c in range(1, 33):
            value += struct.unpack_from("<b", vhgt, 4 + r * 33 + c)[0]
            row.append(value * 8.0)
        rows.append(row)
    return rows


def read_land(esm, buf: bytes) -> dict[tuple[int, int], list[list[float]]]:
    """Every Tamriel exterior cell's land heights by grid (tools/esp-write.py lays the camps out on it)."""
    start, end = tamriel_range(buf)
    grid: dict[int, tuple[int, int]] = {}
    land: dict[tuple[int, int], list[list[float]]] = {}

    def on_record(typ, flags, form_id, offset, size, cell):
        if typ == b"CELL":
            data = dict(esm.subrecords(esm.record_data(buf, offset, size, flags)))
            if b"XCLC" in data:
                grid[form_id] = struct.unpack_from("<ii", data[b"XCLC"], 0)
        elif typ == b"LAND" and cell in grid:
            data = dict(esm.subrecords(esm.record_data(buf, offset, size, flags)))
            if b"VHGT" in data:
                land[grid[cell]] = heights(data[b"VHGT"])

    esm.walk(buf, start, end, [], on_record)
    return land


def ground_at(land: dict[tuple[int, int], list[list[float]]], x: float, y: float) -> float | None:
    """The land's height at the vertex nearest (x, y); None where no land was read."""
    gx, gy = math.floor(x / CELL_UNITS), math.floor(y / CELL_UNITS)
    rows = land.get((gx, gy))
    if rows is None:
        return None
    col = min(32, max(0, round((x - gx * CELL_UNITS) / LAND_STEP)))
    row = min(32, max(0, round((y - gy * CELL_UNITS) / LAND_STEP)))
    return rows[row][col]


# src/CampLogic.h's kCampWalkersAtOnce, kCampAttackUnits, kCampNestUnits and
# CampWalkerSpot - the same turns, so the heights lie under the C++ spots.
WALKERS_AT_ONCE = 12
ATTACK_UNITS = 1600.0
NEST_UNITS = 350.0


def walker_spot(facing: float, attack: bool, slot: int) -> tuple[float, float]:
    away = math.radians(facing + 180.0)
    side = 1.0 if slot % 2 == 1 else -1.0
    at = away + side * ((slot + 1) // 2) * (2.0 * math.pi / 30.0) if attack \
        else slot * (2.0 * math.pi / WALKERS_AT_ONCE)
    r = ATTACK_UNITS if attack else NEST_UNITS
    return math.sin(at) * r, math.cos(at) * r


def print_spots(esm, buf: bytes) -> int:
    text = (HERE.parent / "src" / "CampPlaces.h").read_text()
    places = [(float(x), float(y), float(f)) for x, y, f in
              re.findall(r"\{(-?[\d.]+)f, (-?[\d.]+)f, -?[\d.]+f, (-?[\d.]+)f, \"", text)]
    land = read_land(esm, buf)
    print("\t// tools/camp-places.py --spots: per camp, the attack's ring, then the overrun camp's")
    for x, y, facing in places:
        rows = []
        for attack in (True, False):
            zs = []
            for slot in range(WALKERS_AT_ONCE):
                dx, dy = walker_spot(facing, attack, slot)
                h = ground_at(land, x + dx, y + dy)
                if h is None:
                    raise SystemExit(f"no land under {x + dx:.0f},{y + dy:.0f}")
                zs.append(f"{h:.0f}.0f")
            rows.append("{" + ", ".join(zs) + "}")
        print("\t{" + ", ".join(rows) + "},")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("esm", help="Oblivion.esm")
    parser.add_argument("--count", type=int, default=12)
    parser.add_argument("--gate-units", type=float, default=9000.0)
    parser.add_argument("--door-units", type=float, default=2500.0)
    parser.add_argument("--marker-units", type=float, default=2000.0)
    parser.add_argument("--water-z", type=float, default=150.0)
    parser.add_argument("--side-units", type=float, default=450.0)
    parser.add_argument("--flat-units", type=float, default=400.0)
    parser.add_argument("--slope-units", type=float, default=90.0)
    parser.add_argument("--sample-units", type=float, default=1000.0)
    parser.add_argument("--spots", action="store_true",
                        help="instead: src/CampSpots.h - the land's height under each place's walker spots")
    args = parser.parse_args()
    esm = load_esm_module()
    buf = Path(args.esm).read_bytes()
    if args.spots:
        return print_spots(esm, buf)
    start, end = tamriel_range(buf)

    door_bases: set[int] = set()

    def on_base(typ, flags, form_id, offset, size, cell):
        if typ == b"DOOR":
            door_bases.add(form_id)

    esm.walk(buf, 0, start, [], on_base)

    grid: dict[int, tuple[int, int]] = {}
    land: dict[tuple[int, int], list[list[float]]] = {}
    road: list[tuple[float, float, float]] = []
    doors: list[tuple[float, float]] = []
    markers: list[tuple[float, float, str]] = []

    def on_record(typ, flags, form_id, offset, size, cell):
        if typ == b"CELL":
            data = dict(esm.subrecords(esm.record_data(buf, offset, size, flags)))
            if b"XCLC" in data:
                grid[form_id] = struct.unpack_from("<ii", data[b"XCLC"], 0)
            return
        if typ == b"ROAD":
            data = dict(esm.subrecords(esm.record_data(buf, offset, size, flags)))
            pgrp, pgrr = data[b"PGRP"], data[b"PGRR"]
            at = 0
            for k in range(len(pgrp) // 16):
                x, y, z, links = struct.unpack_from("<fffB", pgrp, k * 16)
                for _ in range(links):
                    tx, ty, tz = struct.unpack_from("<fff", pgrr, at)
                    at += 12
                    steps = max(1, int(math.hypot(tx - x, ty - y) // args.sample_units))
                    for n in range(steps):
                        t = n / steps
                        road.append((x + (tx - x) * t, y + (ty - y) * t, z + (tz - z) * t, tx - x, ty - y))
            if at != len(pgrr):
                raise SystemExit(f"PGRR read {at} of {len(pgrr)} bytes")
            return
        if typ == b"LAND" and cell in grid:
            data = dict(esm.subrecords(esm.record_data(buf, offset, size, flags)))
            if b"VHGT" in data:
                land[grid[cell]] = heights(data[b"VHGT"])
            return
        if typ != b"REFR":
            return
        data = dict(esm.subrecords(esm.record_data(buf, offset, size, flags)))
        if b"DATA" not in data or b"NAME" not in data:
            return
        x, y = struct.unpack_from("<ff", data[b"DATA"], 0)
        if b"XMRK" in data:
            markers.append((x, y, data.get(b"FULL", b"?").rstrip(b"\0").decode("latin-1")))
        elif struct.unpack("<I", data[b"NAME"])[0] in door_bases:
            doors.append((x, y))

    esm.walk(buf, start, end, [], on_record)

    def ground(x: float, y: float) -> float | None:
        return ground_at(land, x, y)

    def slope(x: float, y: float) -> float | None:
        values = []
        for dx in (-args.flat_units, 0.0, args.flat_units):
            for dy in (-args.flat_units, 0.0, args.flat_units):
                h = ground(x + dx, y + dy)
                if h is None:
                    return None
                values.append(h)
        return max(values) - min(values)

    def near(x, y, points, units):
        return any((x - p[0]) ** 2 + (y - p[1]) ** 2 < units * units for p in points)

    gates = gate_positions()
    candidates = []
    for x, y, z, wx, wy in road:
        if near(x, y, gates, args.gate_units) or near(x, y, doors, args.door_units) \
                or near(x, y, markers, args.marker_units):
            continue
        # A right angle to the road's way here.
        length = math.hypot(wx, wy) or 1.0
        sx, sy = -wy / length, wx / length
        best = None
        for side in (1.0, -1.0):
            cx, cy = x + side * sx * args.side_units, y + side * sy * args.side_units
            s = slope(cx, cy)
            h = ground(cx, cy)
            if s is None or h is None or h <= args.water_z or s > args.slope_units:
                continue
            if best is None or s < best[3]:
                best = (cx, cy, h, s, math.degrees(math.atan2(x - cx, y - cy)) % 360.0)
        if best is not None:
            candidates.append(best)

    picked = []
    ic = (27000.0, 63000.0)
    while len(picked) < args.count and candidates:
        def spread(c):
            if not picked:
                return math.hypot(c[0] - ic[0], c[1] - ic[1])
            return min(math.hypot(c[0] - p[0], c[1] - p[1]) for p in picked)
        best = max(candidates, key=spread)
        picked.append(best)

    print(f"// {len(candidates)} spots passed, {len(road)} road points, {len(doors)} doors, {len(markers)} markers")
    for x, y, h, s, facing in picked:
        # Named after a place that stays: not an Oblivion gate's marker.
        name = min((m for m in markers if "Oblivion" not in m[2]), key=lambda m: (m[0] - x) ** 2 + (m[1] - y) ** 2)
        far = math.hypot(name[0] - x, name[1] - y)
        print(f'\t{{{x:.0f}.0f, {y:.0f}.0f, {h:.0f}.0f, {facing:.0f}.0f, "near {name[2]}"}},'
              f"  // the land within {args.flat_units:.0f} varies {s:.0f}, the marker {far:.0f} off")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
