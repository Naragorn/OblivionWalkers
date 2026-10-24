#!/usr/bin/env python3
"""Lists Oblivion's leveled creature lists (LVLC) and the spawn points that use them.

Reads Oblivion.esm (and any further .esm/.esp given) and writes
  <out>/lists.tsv    one line per LVLC: form ID, editor ID, chance none, flags,
                     entries as level:formID:count:editorID
  <out>/forms.tsv    every NPC_ and CREA record: form ID, type, editor ID
  <out>/placed.tsv   every placed actor with a direct NPC_ or CREA base (no list)
  <out>/spawns.tsv   one line per placed reference whose base is a LVLC: cell,
                     grid (exteriors), reference form ID, list form ID, list editor ID,
                     level modifier, position
  <out>/summary.md   counts by list and by cell, for choosing harness cells

    python3 tools/esm-spawns.py ~/.local/share/Steam/steamapps/common/Oblivion/Data/Oblivion.esm --out artifacts/esm

Record layout (TES4 format, read off the file, cross-checked with UESP's
"Oblivion Mod:Mod File Format"): a record header is type(4) dataSize(4)
flags(4) formID(4) vc(4); a GRUP header is 'GRUP' size(4) label(4) type(4)
stamp(4), the size including the header; flag 0x40000 marks zlib-compressed
data behind a 4-byte decompressed size; a subrecord is type(4) size(2) data,
with XXXX giving the next subrecord's real size. LVLO is level(2) pad(2)
formID(4) count(2) pad(2), or the first 8 bytes alone in some old records.
"""

from __future__ import annotations

import argparse
import os
import struct
import sys
import zlib
from collections import defaultdict
from pathlib import Path

HEADER = 20
COMPRESSED = 0x00040000


def subrecords(data: bytes):
    i = 0
    big = None
    while i + 6 <= len(data):
        typ = data[i:i + 4]
        size = struct.unpack_from("<H", data, i + 4)[0]
        i += 6
        if typ == b"XXXX":
            big = struct.unpack_from("<I", data, i)[0]
            i += size
            continue
        if big is not None:
            size = big
            big = None
        yield typ, data[i:i + size]
        i += size


def record_data(buf: bytes, offset: int, size: int, flags: int) -> bytes:
    raw = buf[offset + HEADER:offset + HEADER + size]
    if flags & COMPRESSED:
        return zlib.decompress(raw[4:])
    return raw


def walk(buf: bytes, start: int, end: int, cell_stack: list, on_record):
    """Walks records and groups between start and end, tracking the enclosing cell."""
    i = start
    while i + HEADER <= end:
        typ = buf[i:i + 4]
        if typ == b"GRUP":
            size, label, gtype = struct.unpack_from("<IIi", buf, i + 4)
            if size < HEADER:
                raise ValueError(f"bad GRUP size at {i}")
            pushed = False
            if gtype == 6:  # cell children: the label is the cell's form ID
                cell_stack.append(label)
                pushed = True
            walk(buf, i + HEADER, i + size, cell_stack, on_record)
            if pushed:
                cell_stack.pop()
            i += size
        else:
            size, flags, form_id = struct.unpack_from("<III", buf, i + 4)
            on_record(typ, flags, form_id, i, size, cell_stack[-1] if cell_stack else 0)
            i += HEADER + size


def main() -> int:
    p = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    p.add_argument("files", nargs="+", help="Oblivion.esm first, then any esp to overlay")
    p.add_argument("--out", default="artifacts/esm")
    args = p.parse_args()
    out = Path(args.out)
    out.mkdir(parents=True, exist_ok=True)

    editor_ids: dict[int, str] = {}
    types: dict[int, bytes] = {}
    lists: dict[int, dict] = {}
    cells: dict[int, dict] = {}
    refs: list[dict] = []

    for index, path in enumerate(args.files):
        buf = Path(path).read_bytes()
        print(f"{path}: {len(buf) / 1e6:.1f} MB", flush=True)

        def on_record(typ, flags, form_id, offset, size, cell):
            types[form_id] = typ
            if typ in (b"LVLC", b"CELL", b"REFR", b"ACRE", b"ACHR", b"NPC_", b"CREA", b"WRLD"):
                data = record_data(buf, offset, size, flags)
                if typ == b"LVLC":
                    entry = {"form": form_id, "edid": "", "chance_none": 0, "flags": 0, "entries": []}
                    for st, sd in subrecords(data):
                        if st == b"EDID":
                            entry["edid"] = sd.rstrip(b"\0").decode("latin-1")
                        elif st == b"LVLD":
                            entry["chance_none"] = sd[0]
                        elif st == b"LVLF":
                            entry["flags"] = sd[0]
                        elif st == b"LVLO":
                            level, _pad, ref = struct.unpack_from("<HHI", sd, 0)
                            count = struct.unpack_from("<H", sd, 8)[0] if len(sd) >= 10 else 1
                            entry["entries"].append((level, ref, count))
                    lists[form_id] = entry
                    editor_ids[form_id] = entry["edid"]
                elif typ == b"CELL":
                    info = {"form": form_id, "edid": "", "grid": None, "interior": True}
                    for st, sd in subrecords(data):
                        if st == b"EDID":
                            info["edid"] = sd.rstrip(b"\0").decode("latin-1")
                        elif st == b"XCLC" and len(sd) >= 8:
                            info["grid"] = struct.unpack_from("<ii", sd, 0)
                            info["interior"] = False
                        elif st == b"DATA" and len(sd) >= 1:
                            info["interior"] = bool(sd[0] & 1)
                    cells[form_id] = info
                elif typ in (b"REFR", b"ACRE", b"ACHR"):
                    base = 0
                    level_mod = 0
                    pos = None
                    for st, sd in subrecords(data):
                        if st == b"NAME":
                            base = struct.unpack_from("<I", sd, 0)[0]
                        elif st == b"XLCM" and len(sd) >= 4:
                            level_mod = struct.unpack_from("<i", sd, 0)[0]
                        elif st == b"DATA" and len(sd) >= 12:
                            pos = struct.unpack_from("<fff", sd, 0)
                    refs.append({"form": form_id, "type": typ.decode(), "base": base, "cell": cell,
                                 "level_mod": level_mod, "pos": pos})
                else:
                    for st, sd in subrecords(data):
                        if st == b"EDID":
                            editor_ids[form_id] = sd.rstrip(b"\0").decode("latin-1")
                            break

        walk(buf, 0, len(buf), [], on_record)

    spawn_refs = [r for r in refs if r["base"] in lists]
    print(f"{len(lists)} leveled creature lists, {len(refs)} placed references, "
          f"{len(spawn_refs)} of them leveled creature spawn points", flush=True)

    with open(out / "lists.tsv", "w", encoding="utf-8") as f:
        f.write("form\tedid\tchance_none\tflags\tentries\n")
        for form, entry in sorted(lists.items(), key=lambda kv: kv[1]["edid"]):
            entries = " ".join(f"{lv}:{ref:08X}:{count}:{editor_ids.get(ref, '?')}"
                               for lv, ref, count in entry["entries"])
            f.write(f"{form:08X}\t{entry['edid']}\t{entry['chance_none']}\t{entry['flags']}\t{entries}\n")

    with open(out / "forms.tsv", "w", encoding="utf-8") as f:
        f.write("form\ttype\tedid\n")
        for form, typ in sorted(types.items()):
            if typ in (b"NPC_", b"CREA"):
                f.write(f"{form:08X}\t{typ.decode()}\t{editor_ids.get(form, '')}\n")

    with open(out / "placed.tsv", "w", encoding="utf-8") as f:
        f.write("cell_form\tcell_edid\tgrid\tref\tref_type\tbase_form\tbase_edid\tx\ty\tz\n")
        for r in refs:
            if types.get(r["base"]) not in (b"NPC_", b"CREA"):
                continue
            cell = cells.get(r["cell"], {"edid": "?", "grid": None})
            grid = f"{cell['grid'][0]},{cell['grid'][1]}" if cell.get("grid") else ""
            pos = r["pos"] or (0.0, 0.0, 0.0)
            f.write(f"{r['cell']:08X}\t{cell['edid']}\t{grid}\t{r['form']:08X}\t{r['type']}\t{r['base']:08X}\t"
                    f"{editor_ids.get(r['base'], '')}\t{pos[0]:.0f}\t{pos[1]:.0f}\t{pos[2]:.0f}\n")

    by_cell: dict[int, list] = defaultdict(list)
    with open(out / "spawns.tsv", "w", encoding="utf-8") as f:
        f.write("cell_form\tcell_edid\tgrid\tref\tref_type\tlist_form\tlist_edid\tlevel_mod\tx\ty\tz\n")
        for r in spawn_refs:
            cell = cells.get(r["cell"], {"edid": "?", "grid": None})
            grid = f"{cell['grid'][0]},{cell['grid'][1]}" if cell.get("grid") else ""
            pos = r["pos"] or (0.0, 0.0, 0.0)
            f.write(f"{r['cell']:08X}\t{cell['edid']}\t{grid}\t{r['form']:08X}\t{r['type']}\t{r['base']:08X}\t"
                    f"{lists[r['base']]['edid']}\t{r['level_mod']}\t{pos[0]:.0f}\t{pos[1]:.0f}\t{pos[2]:.0f}\n")
            by_cell[r["cell"]].append(r)

    by_list: dict[int, int] = defaultdict(int)
    for r in spawn_refs:
        by_list[r["base"]] += 1
    counted = sum(1 for e in lists.values() if any(c > 1 for _, _, c in e["entries"]))

    with open(out / "summary.md", "w", encoding="utf-8") as f:
        f.write(f"# Leveled creature spawns\n\n{len(lists)} LVLC lists, {len(spawn_refs)} spawn points, "
                f"{counted} lists with an entry whose count is above 1.\n\n")
        f.write("## Lists with count > 1 entries\n\n")
        for form, entry in sorted(lists.items(), key=lambda kv: kv[1]["edid"]):
            big = [(lv, ref, c) for lv, ref, c in entry["entries"] if c > 1]
            if big:
                f.write(f"- {entry['edid']} ({form:08X}, {by_list[form]} spawn points): " +
                        ", ".join(f"L{lv} {editor_ids.get(ref, '?')} x{c}" for lv, ref, c in big) + "\n")
        f.write("\n## Most used lists\n\n")
        for form, n in sorted(by_list.items(), key=lambda kv: -kv[1])[:40]:
            f.write(f"- {lists[form]['edid']} ({form:08X}): {n} spawn points\n")
        f.write("\n## Interior cells with the most spawn points\n\n")
        interiors = [(c, rs) for c, rs in by_cell.items() if cells.get(c, {}).get("interior", True)]
        for c, rs in sorted(interiors, key=lambda kv: -len(kv[1]))[:40]:
            names = defaultdict(int)
            for r in rs:
                names[lists[r["base"]]["edid"]] += 1
            f.write(f"- {cells.get(c, {}).get('edid', '?')} ({c:08X}): {len(rs)} - " +
                    ", ".join(f"{k} x{v}" for k, v in sorted(names.items(), key=lambda kv: -kv[1])[:6]) + "\n")
    print(f"written to {out}/", flush=True)
    return 0


if __name__ == "__main__":
    sys.exit(main())
