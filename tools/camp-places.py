#!/usr/bin/env python3
"""Prints src/CampCandidates.h: the places the dynamic camps are rolled from (docs/walkers-spec.md 4.20.6).

Reads the Tamriel worldspace (0000003C): its ROAD record's points (PGRP), every
exterior cell's grid (XCLC) and land heights (LAND VHGT), every door and map
marker reference (a DOOR base, an XMRK subrecord) and every reference of a
solid base (SOLID_TYPES). A candidate is a spot
  - by a road (a road point moved --side-units off at a right angle, to the
    flatter side) or on the land off them (every --grid-units),
  - at least --gate-units from every city gate of src/CityGates.h,
  - at least --door-units from every door (a house, an inn, a fort, a cave),
  - at least --marker-units from every map marker (a place of its own),
  - at least --clear-units from every tree, rock and other solid reference,
    and each of its props --prop-clear-units,
  - above the water (the land's height over --water-z, under the walker spots
    too),
  - where the land within --flat-units rises or falls no more than
    --slope-units,
  - within --road-units of a road point (the roads run only through the land
    the player can reach - Tamriel's land goes on past the border and up
    the peaks),
  - at least --apart-units from the candidates taken before it.
Each is named after the nearest map marker that is no Oblivion gate's, and
carries its props' and people's spots (tools/esp-write.py camp_layout,
camp_person_spot) and the land's height under its walker spots.

    python3 tools/camp-places.py ~/.local/share/Steam/steamapps/common/Oblivion/Data/Oblivion.esm \\
        > src/CampCandidates.h

PGRP is x y z (floats) and a connection count
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


# Bases whose references stand in the way of a camp's tents (trees, rocks,
# walls, furniture, doors, containers, activators).
SOLID_TYPES = (b"TREE", b"STAT", b"ACTI", b"FURN", b"DOOR", b"CONT")


def read_world(esm, buf: bytes, sample_units: float):
    """Tamriel's roads (sampled every `sample_units`), land, doors, map
    markers and the references of solid bases (SOLID_TYPES)."""
    start, end = tamriel_range(buf)
    door_bases: set[int] = set()
    solid_bases: set[int] = set()

    def on_base(typ, flags, form_id, offset, size, cell):
        if typ == b"DOOR":
            door_bases.add(form_id)
        if typ in SOLID_TYPES:
            solid_bases.add(form_id)

    esm.walk(buf, 0, start, [], on_base)

    grid: dict[int, tuple[int, int]] = {}
    land: dict[tuple[int, int], list[list[float]]] = {}
    road: list[tuple[float, float, float, float, float]] = []
    doors: list[tuple[float, float]] = []
    markers: list[tuple[float, float, str]] = []
    solids: list[tuple[float, float]] = []

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
                    steps = max(1, int(math.hypot(tx - x, ty - y) // sample_units))
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
        base = struct.unpack("<I", data[b"NAME"])[0]
        if b"XMRK" in data:
            markers.append((x, y, data.get(b"FULL", b"?").rstrip(b"\0").decode("latin-1")))
        elif base in door_bases:
            doors.append((x, y))
        if base in solid_bases:
            solids.append((x, y))

    esm.walk(buf, start, end, [], on_record)
    return road, land, doors, markers, solids


class Points:
    """Points in buckets of `units`: whether any lies within `units` of a spot."""

    def __init__(self, points, units: float):
        self.units = units
        self.buckets: dict[tuple[int, int], list[tuple[float, float]]] = {}
        for p in points:
            self.buckets.setdefault((math.floor(p[0] / units), math.floor(p[1] / units)), []).append(p)

    def near(self, x: float, y: float) -> bool:
        bx, by = math.floor(x / self.units), math.floor(y / self.units)
        r2 = self.units * self.units
        for dx in (-1, 0, 1):
            for dy in (-1, 0, 1):
                for p in self.buckets.get((bx + dx, by + dy), ()):
                    if (x - p[0]) ** 2 + (y - p[1]) ** 2 < r2:
                        return True
        return False


def flat_side(args, ground, x, y, wx, wy):
    """The flatter side of the road at (x, y) going (wx, wy), moved
    --side-units off: (x, y, z, slope, facing to the road) or None."""
    length = math.hypot(wx, wy) or 1.0
    sx, sy = -wy / length, wx / length
    best = None
    for side in (1.0, -1.0):
        cx, cy = x + side * sx * args.side_units, y + side * sy * args.side_units
        s = slope_at(args, ground, cx, cy)
        h = ground(cx, cy)
        if s is None or h is None or h <= args.water_z or s > args.slope_units:
            continue
        if best is None or s < best[3]:
            best = (cx, cy, h, s, math.degrees(math.atan2(x - cx, y - cy)) % 360.0)
    return best


def slope_at(args, ground, x: float, y: float) -> float | None:
    values = []
    for dx in (-args.flat_units, 0.0, args.flat_units):
        for dy in (-args.flat_units, 0.0, args.flat_units):
            h = ground(x + dx, y + dy)
            if h is None:
                return None
            values.append(h)
    return max(values) - min(values)


def marker_name(markers, x: float, y: float):
    """The nearest map marker that is no Oblivion gate's: (x, y, name)."""
    return min((m for m in markers if "Oblivion" not in m[2]), key=lambda m: (m[0] - x) ** 2 + (m[1] - y) ** 2)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("esm", help="Oblivion.esm")
    parser.add_argument("--gate-units", type=float, default=9000.0)
    parser.add_argument("--door-units", type=float, default=2500.0)
    parser.add_argument("--marker-units", type=float, default=2000.0)
    parser.add_argument("--water-z", type=float, default=150.0)
    parser.add_argument("--side-units", type=float, default=450.0)
    parser.add_argument("--flat-units", type=float, default=400.0)
    parser.add_argument("--slope-units", type=float, default=120.0)
    parser.add_argument("--sample-units", type=float, default=300.0)
    parser.add_argument("--clear-units", type=float, default=450.0)
    parser.add_argument("--prop-clear-units", type=float, default=300.0)
    parser.add_argument("--grid-units", type=float, default=1024.0)
    parser.add_argument("--apart-units", type=float, default=6000.0)
    parser.add_argument("--road-units", type=float, default=12000.0)
    args = parser.parse_args()
    esm = load_esm_module()
    buf = Path(args.esm).read_bytes()
    return print_candidates(args, esm, buf)


def load_esp_module():
    spec = importlib.util.spec_from_file_location("esp_write", HERE / "esp-write.py")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def print_candidates(args, esm, buf: bytes) -> int:
    """The module's docstring: by the roads first, then off them; off the
    roads a camp faces the nearest road point within a cell or so."""
    road, land, doors, markers, solids = read_world(esm, buf, args.sample_units)
    esp = load_esp_module()

    def ground(x: float, y: float) -> float | None:
        return ground_at(land, x, y)

    gates = Points(gate_positions(), args.gate_units)
    door_points = Points(doors, args.door_units)
    marker_points = Points(markers, args.marker_units)
    solid_points = Points(solids, args.clear_units)
    prop_points = Points(solids, args.prop_clear_units)
    road_points = Points([(r[0], r[1]) for r in road], 4096.0)
    road_reach = Points([(r[0], r[1]) for r in road], args.road_units)

    def allowed(x: float, y: float) -> bool:
        return not (gates.near(x, y) or door_points.near(x, y) or marker_points.near(x, y) or solid_points.near(x, y))

    raw = []
    for x, y, z, wx, wy in road:
        if allowed(x, y):
            best = flat_side(args, ground, x, y, wx, wy)
            if best is not None and allowed(best[0], best[1]):
                raw.append(best + (True,))
    for (gx, gy) in sorted(land):
        steps = int(CELL_UNITS // args.grid_units)
        for i in range(steps):
            for j in range(steps):
                x = gx * CELL_UNITS + (i + 0.5) * args.grid_units
                y = gy * CELL_UNITS + (j + 0.5) * args.grid_units
                h = ground(x, y)
                if h is None or h <= args.water_z or not allowed(x, y) or not road_reach.near(x, y):
                    continue
                s = slope_at(args, ground, x, y)
                if s is None or s > args.slope_units:
                    continue
                # Facing the nearest road point within a cell or so, else north.
                bx, by = math.floor(x / 4096.0), math.floor(y / 4096.0)
                nearest = None
                for dx in (-1, 0, 1):
                    for dy in (-1, 0, 1):
                        for p in road_points.buckets.get((bx + dx, by + dy), ()):
                            d = (p[0] - x) ** 2 + (p[1] - y) ** 2
                            if nearest is None or d < nearest[0]:
                                nearest = (d, p)
                facing = math.degrees(math.atan2(nearest[1][0] - x, nearest[1][1] - y)) % 360.0 if nearest else 0.0
                raw.append((x, y, h, s, facing, False))
    taken = []
    apart = Points([], args.apart_units)
    for c in raw:
        if apart.near(c[0], c[1]):
            continue
        place = (c[0], c[1], c[2], c[4], "")
        try:
            props = esp.camp_layout(place, lambda px, py: must(ground(px, py), px, py))
            people = [esp.camp_person_spot(place, slot, lambda px, py: must(ground(px, py), px, py))
                      for slot in range(13)]
            spots = []
            for attack in (True, False):
                for slot in range(WALKERS_AT_ONCE):
                    dx, dy = walker_spot(c[4], attack, slot)
                    spots.append(must(ground(c[0] + dx, c[1] + dy), c[0] + dx, c[1] + dy))
        except LookupError:
            continue
        if any(h <= args.water_z for h in spots) or any(prop_points.near(x, y) for _, x, y, _, _, _ in props):
            continue
        taken.append((c, props, people, spots))
        apart.buckets.setdefault((math.floor(c[0] / args.apart_units), math.floor(c[1] / args.apart_units)),
                                 []).append((c[0], c[1]))
    names = sorted({marker_name(markers, c[0], c[1])[2] for c, _, _, _ in taken})
    by_road = sum(1 for c, _, _, _ in taken if c[5])
    out = []
    out.append("#pragma once")
    out.append("")
    out.append("// The dynamic camps' places to roll from (docs/walkers-spec.md 4.20.6), printed by")
    out.append("// tools/camp-places.py --candidates off Oblivion.esm. Do not edit: run it again.")
    out.append(f"// {len(taken)} candidates ({by_road} by the roads, {len(taken) - by_road} off them), "
               f"{len(raw)} spots passed;")
    out.append(f"// --road-units {args.road_units:.0f} --clear-units {args.clear_units:.0f} "
               f"--prop-clear-units {args.prop_clear_units:.0f} --apart-units {args.apart_units:.0f} "
               f"--slope-units {args.slope_units:.0f} --sample-units {args.sample_units:.0f} "
               f"--grid-units {args.grid_units:.0f}.")
    out.append("")
    out.append('#include "CampLogic.h"')
    out.append("")
    out.append("namespace walkers {")
    out.append("")
    out.append("inline constexpr const char* kCampNames[] = {")
    for n in names:
        out.append(f'	"near {n}",')
    out.append("};")
    out.append("")
    out.append("inline constexpr CampCandidate kCampCandidates[] = {")
    for c, props, people, spots in taken:
        name = names.index(marker_name(markers, c[0], c[1])[2])
        out.append(f"	{{{c[0]:.0f}.0f, {c[1]:.0f}.0f, {c[2]:.0f}.0f, {c[4]:.0f}.0f, {name}, {1 if c[5] else 0},")
        out.append("	 {" + ", ".join(f"{{{x:.0f}.0f, {y:.0f}.0f, {z:.0f}.0f, {rx:.4f}f, {rz:.4f}f}}"
                                       for _, x, y, z, rx, rz in props) + "},")
        out.append("	 {" + ", ".join(f"{{{x:.0f}.0f, {y:.0f}.0f, {z:.0f}.0f, 0.0f, {r:.4f}f}}"
                                       for x, y, z, r in people) + "},")
        out.append("	 {" + ", ".join(f"{h:.0f}.0f" for h in spots) + "}},")
    out.append("};")
    out.append("inline constexpr UInt32 kCampCandidateCount = sizeof(kCampCandidates) / sizeof(kCampCandidates[0]);")
    out.append("")
    out.append("}  // namespace walkers")
    print("\n".join(out))
    return 0


def must(h, x, y):
    if h is None:
        raise LookupError(f"no land at {x:.0f},{y:.0f}")
    return h


if __name__ == "__main__":
    raise SystemExit(main())
