#!/usr/bin/env bash
# Runs a held dungeon's walkers on OBVR's harness (harness/dungeon-walkers.txt
# or dungeon-fight.txt), the live INI set for the run and put back whatever
# happens. Mode: siege (a siege counted off by the console's kills), taken
# (a walker dungeon cleared so), fight (a siege fought by the game - a look),
# clear (a place to clear: the people wait outside, killall, they move in).
# Exit 0: every check held. Announce "busy" first.
set -euo pipefail
mode="${1:-siege}"
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
obvr="${OBVR_DIR:-$root/../OBVR}"
game="${WALKERS_LINUX_GAME:-$HOME/.local/share/Steam/steamapps/common/Oblivion}"
ini="$game/Data/OBSE/Plugins/OblivionWalkers.ini"
case "$mode" in
	siege) state=2; walkers=8; behead=0; scenario=dungeon-walkers; site="Fort Ash" ;;
	taken) state=4; walkers=8; behead=0; scenario=dungeon-walkers; site="Fort Ash" ;;
	fight) state=2; walkers=10; behead=1; scenario=dungeon-fight; site="Fort Ash" ;;
	clear) state=3; walkers=8; behead=0; scenario=dungeon-clear; site="Echo Mine" ;;
	*) echo "mode: siege | taken | fight | clear" >&2; exit 2 ;;
esac
backup="$(mktemp)"
cp "$ini" "$backup"
trap 'cp "$backup" "$ini"; rm -f "$backup"' EXIT
python3 - "$ini" "$state" "$walkers" "$behead" "$site" <<'INI'
import re, sys
path, state, walkers, behead, site = sys.argv[1:6]
text = open(path, encoding="latin-1", newline="").read()

def put(text, section, key, value):
    head = re.search(r"^\[" + section + r"\]\s*$", text, re.M)
    if head is None:
        return text + "\r\n[" + section + "]\r\n" + key + "=" + value + "\r\n"
    end = re.search(r"^\[", text[head.end():], re.M)
    stop = head.end() + (end.start() if end else len(text) - head.end())
    body = text[head.end():stop]
    if re.search(r"^" + key + r"=", body, re.M):
        body = re.sub(r"^" + key + r"=.*$", key + "=" + value, body, count=1, flags=re.M)
    else:
        body = "\r\n" + key + "=" + value + body
    return text[:head.end()] + body + text[stop:]

for key, value in (("TestSite", site), ("TestSiteState", state), ("Enabled", "1"),
                   ("AttackWalkersMin", walkers), ("AttackWalkersMax", walkers), ("WalkersAtOnce", "6"),
                   ("PeopleMin", "8"), ("PeopleMax", "8")):
    text = put(text, "Dungeons", key, value)
text = put(text, "Zombies", "DieOnlyBeheaded", behead)
text = put(text, "Debug", "StepTiming", "1")
open(path, "w", encoding="latin-1", newline="").write(text)
INI
(cd "$obvr" && python3 tools/hand-script-run.py "$root/harness/$scenario.txt" --headless \
	--artifact-root "$root/artifacts/hand-script") > "$root/artifacts/dungeon-walkers-runner.txt" 2>&1 || true
tail -1 "$root/artifacts/dungeon-walkers-runner.txt"
run="$(ls -td "$root"/artifacts/hand-script/$scenario/*/ | head -1)"
cp "$game/OblivionWalkers.log" "$run/OblivionWalkers.log"
echo "run: $run ($mode)"
python3 - "$run/OblivionWalkers.log" "$mode" <<'PY'
import re, sys
lines = open(sys.argv[1], encoding="latin-1").read().splitlines()
mode = sys.argv[2]
failures = 0

def check(ok, what):
    global failures
    print(("  ok    " if ok else "  FAIL  ") + what)
    failures += 0 if ok else 1

d = [l for l in lines if l.startswith("Dungeons: ")]
pool = [l for l in d if "the pool to site" in l and "Fort Ash" in l]
into = [l for l in d if re.search(r"walker \w+ into Fort Ash", l)]
ended = [l for l in d if "Fort Ash - walker" in l and "ended" in l]
if mode == "clear":
    kit = [l for l in d if "the kit to site 0 Echo Mine" in l]
    check(len(kit) >= 1 and "(mine; to clear, 8 people) - waiting outside" in kit[0], "the kit waits outside Echo Mine, eight of them")
    stands = [m for l in d for m in [re.search(r"stands - (\d+) shown, (\d+) enabled, (\d+) in its first cell \w+, (\d+) with their 3D; the farthest (\d+) units from its spot, the lowest (-?\d+) beneath it", l)] if m]
    check(len(stands) >= 1 and stands[0].group(1, 2, 4) == ("8", "8", "8") and int(stands[0].group(6)) < 150,
          "outside the eight stand enabled with their 3D on the land" + (f" ({stands[0].group(0)})" if stands else ""))
    noted = [l for l in d if "nobody living left in it" in l]
    check(len(noted) == 1 and "cells cleared 01 - the place is cleared" in noted[0], "after the killall the one cell is noted: cleared")
    check(not any("nobody living" in l for l in lines[:lines.index(noted[0])] if "Echo Mine" in l) if noted else False,
          "not before - its occupants lived")
    check(any("site 0 Echo Mine (mine) taken - now refuge, 8 people" in l for l in d), "the place taken: a refuge of the eight")
    check(any("move in by the door (9)" in l for l in d), "those who waited move in by the door, the trader with them")
    check(len(stands) >= 2 and stands[-1].group(1, 2, 3) == ("9", "9", "9"),
          "three seconds on the nine stand in its cell" + (f" ({stands[-1].group(0)})" if len(stands) >= 2 else ""))
elif mode == "fight":
    died = [l for l in d if "one of its people" in l and "died" in l]
    print(f"  look  the pool: {len(pool)}, walkers into the place: {len(into)}, walkers ended: {len(ended)}, "
          f"people died: {len(died)}")
    for l in [l for l in d if " defended " in l or " overrun " in l or " held " in l]:
        print("  look  " + l[:200])
    for l in [l for l in lines if l.startswith("Slow aggression: walker") and "roused" in l][:3]:
        print("  look  " + l[:200])
else:
    name = "besieged" if mode == "siege" else "walker dungeon"
    check(len(pool) >= 1 and f"({name}; 8 walkers, 6 at once)" in pool[0], f"the pool goes to Fort Ash, a {name} of 8, six at once")
    first = into[:6]
    where = "by the door" if mode == "siege" else "among its spots"
    check(len(first) == 6 and all(where in l and "answered 1" in l for l in first), f"six come into the place {where}")
    check(len(into) == 8 and all("by the door" in l for l in into[6:]), "the last two come in by the door as the six fall")
    check(len(ended) == 8 and "0 walkers left" in ended[-1], f"all eight counted off ({len(ended)})")
    out = [int(m.group(1)) for l in d for m in [re.search(r"is held - (\d+) occupants taken out", l)] if m]
    check(len(out) >= 1 and out[0] >= 1, f"the game's occupants out on entering ({out})")
    if mode == "siege":
        kit = [l for l in d if "the kit to site" in l and "besieged, 8 people" in l]
        check(len(kit) >= 1, "the kit stands in the besieged place")
        check(any("site 0 Fort Ash (fort) defended - now refuge, 8 people" in l for l in d), "the last walker ended: defended, a refuge again")
    else:
        taken = [l for l in d if "site 0 Fort Ash (fort) taken - now refuge" in l]
        check(len(taken) == 1, "the last walker ended: the place taken, a refuge" + (f" ({taken[0][-40:]})" if taken else ""))
        check(any("the kit to site 0 Fort Ash" in l and "refuge" in l for l in d), "and the kit of people moves in")
for l in d[2:44]:
    print("        " + l[:210])
for l in [l for l in lines if l.startswith("Timing:   dungeons")][-8:]:
    print("        " + l)
sys.exit(1 if failures else 0)
PY
