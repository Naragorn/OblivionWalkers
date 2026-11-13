#!/usr/bin/env python3
"""Checks the shipped OblivionWalkers.ini as the game's reader takes it.

Every line is blank, a ";" comment, a section header alone on its line, or
"key=value"; no section twice; the keys named below sit in their sections. A
comment that lost its ";" and begins with "[" would open a section and move
the keys after it (2026-11-13: "[ReplaceCreatures] for those ..." took
[Replace]'s keys).

    tools/ini-check.py OblivionWalkers.ini
"""
import re
import sys

HEADER = re.compile(r"^\[[A-Za-z]+\]$")
# One key a section, enough to show its keys are still its own.
EXPECTED = {
    "Rise": "Enabled",
    "Replace": "Enabled",
    "ReplaceCategories": "Skeletons",
    "Horde": "Enabled",
    "Survivors": "Enabled",
    "Dungeons": "OnlyChosen",
    "News": "Courier",
    "Grab": "Enabled",
    "Debug": "StepTiming",
}


def check(path):
    """The faults of the file at `path`, each a line of text."""
    faults = []
    sections = {}
    section = None
    with open(path, encoding="utf-8") as handle:
        for number, raw in enumerate(handle, 1):
            line = raw.rstrip("\r\n")
            if not line.strip() or line.startswith(";"):
                continue
            if line.startswith("["):
                if not HEADER.match(line):
                    faults.append(f"line {number}: not a section header alone on its line: {line[:60]}")
                    continue
                section = line[1:-1]
                if section in sections:
                    faults.append(f"line {number}: section [{section}] a second time")
                sections.setdefault(section, set())
                continue
            if "=" not in line:
                faults.append(f"line {number}: neither comment, section nor key=value: {line[:60]}")
                continue
            if section is None:
                faults.append(f"line {number}: a key before the first section: {line[:60]}")
                continue
            sections[section].add(line.split("=", 1)[0].strip())
    for name, key in EXPECTED.items():
        if name not in sections:
            faults.append(f"section [{name}] is missing")
        elif key not in sections[name]:
            faults.append(f"[{name}] has no {key}")
    return faults


def main():
    if len(sys.argv) != 2:
        print(__doc__)
        return 2
    faults = check(sys.argv[1])
    for fault in faults:
        print(f"ini-check: {fault}")
    print(f"ini-check: {sys.argv[1]} - {'FAIL' if faults else 'PASS'} ({len(faults)} faults)")
    return 1 if faults else 0


if __name__ == "__main__":
    sys.exit(main())
