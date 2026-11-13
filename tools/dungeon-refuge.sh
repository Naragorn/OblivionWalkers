#!/usr/bin/env bash
# Runs harness/dungeon-refuge.txt with [Dungeons] TestSite=Fort Ash and
# [Debug] StepTiming=1 in the live INI (put back whatever happens) and
# checks OblivionWalkers.log. Exit 0: every check held. Announce "busy" first.
set -euo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
obvr="${OBVR_DIR:-$root/../OBVR}"
game="${WALKERS_LINUX_GAME:-$HOME/.local/share/Steam/steamapps/common/Oblivion}"
ini="$game/Data/OBSE/Plugins/OblivionWalkers.ini"
backup="$(mktemp)"
cp "$ini" "$backup"
trap 'cp "$backup" "$ini"; rm -f "$backup"' EXIT
python3 - "$ini" <<'INI'
import re, sys
path = sys.argv[1]
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

text = put(text, "Dungeons", "TestSite", "Fort Ash")
text = put(text, "Dungeons", "Enabled", "1")
text = put(text, "Debug", "StepTiming", "1")
open(path, "w", encoding="latin-1", newline="").write(text)
INI
(cd "$obvr" && python3 tools/hand-script-run.py "$root/harness/dungeon-refuge.txt" --headless \
	--artifact-root "$root/artifacts/hand-script") > "$root/artifacts/dungeon-refuge-runner.txt" 2>&1 || true
tail -3 "$root/artifacts/dungeon-refuge-runner.txt"
run="$(ls -td "$root"/artifacts/hand-script/dungeon-refuge/*/ | head -1)"
cp "$game/OblivionWalkers.log" "$run/OblivionWalkers.log"
echo "run: $run"
python3 - "$run/OblivionWalkers.log" <<'PY'
import re, sys
lines = open(sys.argv[1], encoding="latin-1").read().splitlines()
failures = 0

def check(ok, what):
    global failures
    print(("  ok    " if ok else "  FAIL  ") + what)
    failures += 0 if ok else 1

d = [l for l in lines if l.startswith("Dungeons: ") or (l.startswith("Co-save: ") and "dungeon" in l)]
kit = [l for l in d if "the kit to site" in l and "Fort Ash" in l]
check(len(kit) >= 1 and "8 people and the trader" in kit[0], "the kit goes to Fort Ash, a refuge of 8 and the trader")
out = [int(m.group(1)) for l in d for m in [re.search(r"is held - (\d+) occupants taken out", l)] if m]
check(len(out) >= 1 and out[0] >= 1, f"its occupants taken out on entering ({out})")
died = [l for l in d if "one of its people" in l and "died" in l]
check(len(died) == 1 and "7 people left" in died[0], "the one killed counted off: 7 left")
read = [l for l in d if "occupants taken out of held dungeons read" in l]
n_read = int(re.search(r"(\d+) occupants", read[-1]).group(1)) if read else -1
check(n_read >= 1 and n_read == sum(out[:1]), f"the save kept those taken out ({n_read})")
check(len(kit) >= 2 and "7 people and the trader" in kit[-1], "after the load the kit stands there again, 7 and the trader")
back = [m for l in d for m in [re.search(r"is held no more - (\d+) of (\d+) occupants put back", l)] if m]
check(len(back) >= 1 and int(back[-1].group(1)) >= 1 and back[-1].group(1) == back[-1].group(2),
      "Enabled=0: every occupant put back" + (f" ({back[-1].group(0)})" if back else ""))
check(any("the kit off Fort Ash" in l for l in d), "and the kit put away")
check(not any(l.startswith("Rise:") and "25000A47" in l and "raised again" in l for l in lines) and
      sum(1 for l in lines if "Resurrect" in l and "25000A47" in l) == 0, "the dead one is not raised by the load")
stands = [m for l in d for m in [re.search(r"stands - (\d+) shown, (\d+) enabled, (\d+) in its first cell \w+, (\d+) with their 3D", l)] if m]
check(len(stands) >= 2 and stands[0].group(1) == "9" and stands[0].group(2) == "9" and stands[0].group(3) == "9",
      "three seconds on, the nine stand enabled in Fort Ash's first cell" + (f" ({stands[0].group(0)})" if stands else ""))
check(len(stands) >= 2 and stands[-1].group(1) == "8" and stands[-1].group(2) == "8" and stands[-1].group(4) == "8",
      "after the load the eight left stand enabled with their 3D" + (f" ({stands[-1].group(0)})" if stands else ""))
for l in d[:60]:
    print("        " + l[:230])
for l in [l for l in lines if l.startswith("Timing:   dungeons") or l.startswith("Timing: ") and "frames" in l][-6:]:
    print("        " + l)
sys.exit(1 if failures else 0)
PY
