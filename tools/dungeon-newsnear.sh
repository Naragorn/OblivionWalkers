#!/usr/bin/env bash
# Runs harness/dungeon-newsnear.txt (4.20.7a A: the news names the nearest
# held place) with [Dungeons] set for the run, the INI put back whatever
# happens; checks each "news - the site now" line against the places active
# in the log at that moment, by the candidates' doors in
# src/DungeonCandidates.h. Needs tools/linux-walkers.sh on; announce "busy"
# first.
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
# eight places anywhere, no runner; the rumors and the journal on (the news is only heard with both)
for key, value in (("Enabled", "1"), ("TestSite", ""), ("NewPlacesWithin", "0"), ("MessengerHours", "0"),
                   ("Journal", "1"), ("Rumors", "1"), ("ActiveMin", "8"), ("ActiveMax", "8")):
    text = put(text, "Dungeons", key, value)
open(path, "w", encoding="latin-1", newline="").write(text)
INI
(cd "$obvr" && python3 tools/hand-script-run.py "$root/harness/dungeon-newsnear.txt" --headless ${WALKERS_RUNNER_ARGS---no-steamvr} \
	--artifact-root "$root/artifacts/hand-script") > "$root/artifacts/dungeon-newsnear-runner.txt" 2>&1 || true
tail -1 "$root/artifacts/dungeon-newsnear-runner.txt"
run="$(ls -td "$root"/artifacts/hand-script/dungeon-newsnear/*/ | head -1)"
cp "$game/OblivionWalkers.log" "$run/OblivionWalkers.log"
echo "run: $run"
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
# the active places by site, as the log's events leave them (a free one has no news)
active = {}
looks = []
for l in lines:
    m = re.search(r"Dungeons: site (\d+) (.+?) \([\w ]+\) [\w ]+ - now ([\w ]+), ", l)
    if m:
        site, name, state = int(m.group(1)), m.group(2), m.group(3)
        if state == "free":
            active.pop(site, None)
        else:
            active[site] = name
    m = re.search(r"Dungeons: news - the site now (.+?) \(site (\d+), (\d+) units from the player's place (-?\d+) (-?\d+)(;[^)]*)?\)", l)
    if m:
        looks.append((m.group(1), int(m.group(2)), float(m.group(3)), float(m.group(4)), float(m.group(5)),
                      m.group(6) is None, dict(active)))
    if l.startswith("Dungeons: news - now"):
        print("  look  " + l[:200])
print("        the active places at the end: %s" % sorted(active.values()))
known = [x for x in looks if x[5]]
for name, site, d, px, py, ok, act in looks:
    print("  look  news of %-24s site %d, %6.0f units from %7.0f %7.0f%s" % (name, site, d, px, py, "" if ok else " (the place not known)"))
check(len(looks) >= 1, "the news names its site (%d lines)" % len(looks))
places = sorted({(round(x[3] / 4096), round(x[4] / 4096)) for x in known})
check(len(places) >= 2, "the news looked at from two places of the player (cells %s)" % places)
for name, site, d, px, py, ok, act in known:
    dist = sorted((math.hypot(doors[n][0] - px, doors[n][1] - py), n) for n in act.values() if n in doors)
    near = dist[0] if dist else (-1.0, "-")
    check(bool(dist) and abs(near[0] - d) < 2.0,
          "at %.0f %.0f the news names %s (%.0f) - the nearest active place %s (%.0f); next %s" %
          (px, py, name, d, near[1], near[0], ["%s %.0f" % (n, x) for x, n in dist[1:3]]))
check(len({x[0] for x in known}) >= 2, "the two places' news differ (%s)" % [x[0] for x in known])
print("dungeon-newsnear: %s" % ("PASS" if failures == 0 else "FAIL (%d)" % failures))
sys.exit(1 if failures else 0)
PY
