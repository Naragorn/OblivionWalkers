#!/usr/bin/env python3
"""Reads UESP's place page of every refugee dungeon candidate (docs/walkers-spec.md 4.20.7).

For each name in src/DungeonCandidates.h it fetches the page's wikitext
(https://en.uesp.net/w/index.php?title=Oblivion:<Name>&action=raw, one a
second) and writes <out> as TSV: the name, the quests of its "Related
Quests" section ({{Quest Link|...}}), whether its description says
"quest-related", and the page's state (ok, missing, redirect followed).
tools/dungeon-candidates.py --uesp reads it: a dungeon with a related
quest or the "quest-related" mark is out of the pool.

    python3 tools/dungeon-uesp.py --out tools/dungeon-uesp.tsv
"""

from __future__ import annotations

import argparse
import re
import sys
import time
import urllib.error
import urllib.parse
import urllib.request
from pathlib import Path

RAW = "https://en.uesp.net/w/index.php?title={}&action=raw"
AGENT = "OblivionWalkers-dungeon-check/1.0 (https://github.com/Naragorn/OblivionWalkers)"


def fetch(title: str) -> str | None:
    url = RAW.format(urllib.parse.quote(title.replace(" ", "_"), safe="_':"))
    req = urllib.request.Request(url, headers={"User-Agent": AGENT})
    try:
        with urllib.request.urlopen(req, timeout=30) as r:
            return r.read().decode("utf-8", "replace")
    except urllib.error.HTTPError as e:
        if e.code == 404:
            return None
        raise


def main() -> int:
    p = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    p.add_argument("--header", default="src/DungeonCandidates.h")
    p.add_argument("--out", default="tools/dungeon-uesp.tsv")
    args = p.parse_args()
    names = re.findall(r'^\t\{"((?:[^"\\]|\\.)*)"', Path(args.header).read_text(encoding="utf-8"), re.M)
    rows = ["name\trelated_quests\tquest_related\tpage"]
    for name in names:
        title = f"Oblivion:{name}"
        text = fetch(title)
        state = "ok"
        for _ in range(2):
            m = re.match(r"\s*#REDIRECT\s*\[\[([^\]|#]+)", text or "", re.I)
            if not m:
                break
            title = m.group(1).strip()
            text = fetch(title)
            state = f"redirect {title}"
            time.sleep(1)
        if text is None:
            rows.append(f"{name}\t\t\tmissing")
            time.sleep(1)
            continue
        related = ""
        sec = re.search(r"==\s*Related Quests?\s*==(.*?)(\n==[^=]|\Z)", text, re.S | re.I)
        if sec:
            related = ", ".join(sorted(set(q.strip() for q in re.findall(r"\{\{Quest Link\|([^}|]+)", sec.group(1)))))
        flagged = "yes" if re.search(r"quest-related", text, re.I) else ""
        rows.append(f"{name}\t{related}\t{flagged}\t{state}")
        print(rows[-1], flush=True)
        time.sleep(1)
    Path(args.out).write_text("\n".join(rows) + "\n", encoding="utf-8")
    return 0


if __name__ == "__main__":
    sys.exit(main())
