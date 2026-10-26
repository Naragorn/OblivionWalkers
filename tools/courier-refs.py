#!/usr/bin/env python3
"""Writes src/CourierRefs.h: the Black Horse Courier broadsheets the plugin turns into the walkers' news.

Oblivion's broadsheets are BOOK records whose editor IDs start with
"Broadsheet" (Clutter\\Books\\Broadsheet01.NIF); its references to them lie in
45 cells (76 of them, read 2026-10-26). The walkers' news (docs/walkers-spec.md
4.9; Nadi, 2026-10-26: the vanilla places, no cell of the ESP) take only those
in the inns and taverns and in the Courier's own office in the Imperial City -
the ones in people's houses keep the old news. The cells are named below by
their editor IDs; each broadsheet reference found in one of them becomes a row.

    python3 tools/courier-refs.py ~/.local/share/Steam/steamapps/common/Oblivion/Data/Oblivion.esm
"""

from __future__ import annotations

import argparse
import importlib.util
import struct
from pathlib import Path

HERE = Path(__file__).resolve().parent

# The inns and taverns with a broadsheet of their own, and the Courier's office.
NEWS_CELLS = [
    "AnvilTheCountsArms",
    "AnvilTheFlowingBowl",
    "AnvilTheFocsle",
    "BravilLonelySuitorLodge",
    "BravilSilverHomeonthewater",
    "ChorrolTheOakandCrosierTavern",
    "GottshawInnTavern",
    "ICElvenGardensLutherBroadsBoardingHouse",
    "ICMarketDistrictTheFeedBag",
    "ICMarketDistrictTheMerchantsInnUpstairs",
    "ICTalosPlazaTheTiberSeptimHotel",
    "ICWaterfrontTheBloatedFloat",
    "LeyawiinFiveClawsLodge",
    "LeyawiinThreeSistersInn",
    "TheDrunkenDragonInn",
    "ICMarketDistrictBlackHorseCourier",
    "ICMarketDistrictBlackHorseCourierUpstairs",
]


def load_esm_module():
    spec = importlib.util.spec_from_file_location("esm_spawns", HERE / "esm-spawns.py")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("esm", help="Oblivion.esm")
    parser.add_argument("--out", default=str(HERE.parent / "src" / "CourierRefs.h"))
    args = parser.parse_args()
    esm = load_esm_module()
    buf = Path(args.esm).read_bytes()
    sheets: dict[int, str] = {}
    cells: dict[int, str] = {}
    refs: list[tuple[int, int, int]] = []

    def on_record(typ, flags, form_id, offset, size, cell):
        if typ not in (b"BOOK", b"CELL", b"REFR"):
            return
        data = esm.record_data(buf, offset, size, flags)
        if typ == b"REFR":
            for kind, sub in esm.subrecords(data):
                if kind == b"NAME":
                    refs.append((form_id, struct.unpack_from("<I", sub, 0)[0], cell))
            return
        edid = ""
        for kind, sub in esm.subrecords(data):
            if kind == b"EDID":
                edid = sub.rstrip(b"\0").decode("latin-1")
                break
        if typ == b"BOOK" and edid.startswith("Broadsheet"):
            sheets[form_id] = edid
        elif typ == b"CELL":
            cells[form_id] = edid

    esm.walk(buf, 0, len(buf), [], on_record)
    wanted = {form: edid for form, edid in cells.items() if edid in NEWS_CELLS}
    missing = sorted(set(NEWS_CELLS) - set(wanted.values()))
    if missing:
        raise SystemExit(f"no cell {', '.join(missing)} in the ESM")
    rows = sorted((cell, ref, base) for ref, base, cell in refs if base in sheets and cell in wanted)
    lines = [
        "#pragma once",
        "",
        "// Written by tools/courier-refs.py from Oblivion.esm - do not edit by hand.",
        "// The Black Horse Courier broadsheets in the inns, taverns and the Courier's",
        "// office that carry the walkers' news (docs/walkers-spec.md 4.9): each",
        "// reference, its cell and the broadsheet it holds in Oblivion.esm.",
        "",
        '#include "Types.h"',
        "",
        "namespace walkers {",
        "",
        "struct CourierRef {",
        "\tUInt32 ref;",
        "\tUInt32 cell;",
        "\tUInt32 sheet;  // the vanilla broadsheet (BOOK)",
        "};",
        "",
        "inline constexpr CourierRef kCourierRefs[] = {",
    ]
    for cell, ref, base in rows:
        lines.append(f"\t{{0x{ref:08X}, 0x{cell:08X}, 0x{base:08X}}},  // {wanted[cell]}, {sheets[base]}")
    lines += [
        "};",
        "inline constexpr UInt32 kCourierRefCount = sizeof(kCourierRefs) / sizeof(kCourierRefs[0]);",
        "",
        "}  // namespace walkers",
        "",
    ]
    Path(args.out).write_text("\n".join(lines))
    print(f"{args.out}: {len(rows)} broadsheets in {len({r[0] for r in rows})} of {len(NEWS_CELLS)} cells")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
