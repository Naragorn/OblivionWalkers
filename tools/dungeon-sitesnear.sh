#!/usr/bin/env bash
# Runs harness/dungeon-sitesnear.txt (4.20.7a B: new held places rolled near
# the player) with [Dungeons] set for the run, the INI put back whatever
# happens; checks each place rolled after the player was moved against
# NewPlacesWithin, by the candidates' doors in src/DungeonCandidates.h.
# Needs tools/linux-walkers.sh on; announce "busy" first.
set -euo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
obvr="${OBVR_DIR:-$root/../OBVR}"
game="${WALKERS_LINUX_GAME:-$HOME/.local/share/Steam/steamapps/common/Oblivion}"
ini="$game/Data/OBSE/Plugins/OblivionWalkers.ini"
backup="$(mktemp)"
cp "$ini" "$backup"
trap 'cp "$backup" "$ini"; rm -f "$backup"' EXIT
source "$root/tools/walkers-ini-mode.sh"
walkers_apocalypse "$ini"  # 4.24: these runs need the apocalypse
python3 - "$ini" <<'INI'
import re, sys
path = sys.argv[1]
text = open(path, encoding="latin-1", newline="").read()
nl = "\r\n" if "\r\n" in text else "\n"
def put(text, section, key, value):
    head = re.search(r"^\[" + section + r"\]\s*$", text, re.M)
    if head is None:
        return text.rstrip("\r\n") + nl + nl + "[" + section + "]" + nl + key + "=" + value + nl
    end = re.search(r"^\[", text[head.end():], re.M)
    stop = head.end() + (end.start() if end else len(text) - head.end())
    body = text[head.end():stop]
    if re.search(r"^" + key + r"=", body, re.M):
        body = re.sub(r"^" + key + r"=.*$", key + "=" + value, body, count=1, flags=re.M)
    else:
        body = nl + key + "=" + value + body
    return text[:head.end()] + body + text[stop:]
for key, value in (("Enabled", "1"), ("TestSite", ""), ("NewPlacesWithin", "60000"), ("LeaveAfterDays", "1"),
                   ("MessengerHours", "0"), ("Journal", "0"),
                   # one place at a time: each new roll must take one of the pool's places within 60000
                   ("ActiveMin", "1"), ("ActiveMax", "1")):
    text = put(text, "Dungeons", key, value)
open(path, "w", encoding="latin-1", newline="").write(text)
INI
(cd "$obvr" && python3 tools/hand-script-run.py "$root/harness/dungeon-sitesnear.txt" --headless ${WALKERS_RUNNER_ARGS---no-steamvr} \
	--artifact-root "$root/artifacts/hand-script") > "$root/artifacts/dungeon-sitesnear-runner.txt" 2>&1 || true
run="$(ls -td "$root"/artifacts/hand-script/dungeon-sitesnear/*/ | head -1)"
cp "$game/OblivionWalkers.log" "$run/OblivionWalkers.log"
python3 - "$run/OblivionWalkers.log" "$root/src/DungeonCandidates.h" <<'PY'
import math, re, sys
lines = open(sys.argv[1], encoding="latin-1").read().splitlines()
doors = {m.group(1): (float(m.group(2)), float(m.group(3))) for m in re.finditer(
    r'\{"([^"]+)", DungeonKind::\w+, \{[^}]*\}, \d+, 0x\w+, 0x\w+, (?:true|false), (-?[\d.]+)f, (-?[\d.]+)f,',
    open(sys.argv[2]).read())}
failures = 0
def check(ok, what):
    global failures
    print(("  ok    " if ok else "  FAIL  ") + what)
    failures += 0 if ok else 1
px, py = 20 * 4096 + 2048, -15 * 4096 + 2048
rolledAt = [(i, m.group(1)) for i, m in ((i, re.search(r"Dungeons: site \d+ (.+?) \(\w+\) rolled - now", l)) for i, l in enumerate(lines)) if m]
rolled = [n for _, n in rolledAt]
pools = [l for l in lines if "Dungeons:" in l and ("pool" in l and "candidates" in l)]
for l in pools[:3]:
    print("        " + l[:200])
dist = [(n, math.hypot(doors[n][0] - px, doors[n][1] - py)) for n in rolled if n in doors]
for n, d in dist:
    print("        rolled %-28s %6.0f units from the player's place" % (n, d))
gaveUp = [i for i, l in enumerate(lines) if " given up - now " in l]
check(bool(gaveUp), "places unseen for a day given up (%d)" % len(gaveUp))
later = [(n, math.hypot(doors[n][0] - px, doors[n][1] - py)) for i, n in rolledAt if gaveUp and i > gaveUp[0] and n in doors]
# in the pool unless its last word in the log is "out" (a quest's reference enabled since 2026-11-18:
# the main menu's look reads the file's flags, the load the save's - "back in the pool")
def lastOut(n):
    state = False
    for l in lines:
        if l.startswith("Dungeons: %s (" % n) and " out - " in l:
            state = True
        elif l.startswith("Dungeons: %s back in the pool" % n):
            state = False
    return state
inPool = sorted((math.hypot(x - px, y - py), n) for n, (x, y) in doors.items() if not lastOut(n))
near = [n for d, n in inPool if d <= 60000]
print("        the pool within 60000: %s" % near)
check(bool(near), "a candidate of the pool lies within NewPlacesWithin=60000 (else the rule rolls anywhere)")
check(bool(later) and all(n in near for n, _ in later),
      "every place rolled after the move one of those near (%s)" % ["%s %.0f" % x for x in later[:8]])
news = [l for l in lines if l.startswith("Dungeons: news - now")]
for l in news[-2:]:
    print("  look  " + l[:200])
print("dungeon-sitesnear: %s" % ("PASS" if failures == 0 else "FAIL (%d)" % failures))
sys.exit(1 if failures else 0)
PY
