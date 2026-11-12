#!/usr/bin/env bash
# Runs harness/slow-aggression-rouse.txt on OBVR's harness and checks
# OblivionWalkers.log - the runner itself reads OBVR.log only. Exit 0: every
# check held. Announce "busy" to the other sessions first.
set -euo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
obvr="${OBVR_DIR:-$root/../OBVR}"
game="${WALKERS_LINUX_GAME:-$HOME/.local/share/Steam/steamapps/common/Oblivion}"
log="$game/OblivionWalkers.log"
ini="$game/Data/OBSE/Plugins/OblivionWalkers.ini"
# The spawn replacement off for the run (put back whatever happens): with it
# dozens of walkers stand about, and the harness's {actor} - the actor
# nearest the player - after a push is often another one.
backup="$(mktemp)"
cp "$ini" "$backup"
trap 'cp "$backup" "$ini"; rm -f "$backup"' EXIT
python3 - "$ini" <<'INI'
import re, sys
path = sys.argv[1]
text = open(path, encoding="latin-1", newline="").read()
head = re.search(r"^\[Replace\]\s*$", text, re.M)
if head is None:
    sys.exit("no [Replace] section in " + path)
end = re.search(r"^\[", text[head.end():], re.M)
stop = head.end() + (end.start() if end else len(text) - head.end())
section = re.sub(r"^Enabled=1", "Enabled=0", text[head.end():stop], count=1, flags=re.M)
open(path, "w", encoding="latin-1", newline="").write(text[:head.end()] + section + text[stop:])
INI
grep -n -A3 "^\[Replace\]" "$ini" | head -3
(cd "$obvr" && python3 tools/hand-script-run.py "$root/harness/slow-aggression-rouse.txt" --headless \
	--artifact-root "$root/artifacts/hand-script") | tee "$root/artifacts/slow-aggression-rouse-runner.txt"
run="$(ls -td "$root"/artifacts/hand-script/slow-aggression-rouse/*/ | head -1)"
cp "$log" "$run/OblivionWalkers.log"
python3 - "$run/OblivionWalkers.log" <<'PY'
import re, sys
lines = open(sys.argv[1], encoding="latin-1").read().splitlines()
failures = 0

def check(ok, what):
    global failures
    print(("  ok    " if ok else "  FAIL  ") + what)
    failures += 0 if ok else 1

roused = [l for l in lines if "roused - the engine decides" in l]
calmed = [l for l in lines if " calms down " in l]
check(len(roused) >= 1, "a walker roused after its time near ran out")
check(any("answered 1" in l for l in roused), "its ModAV Aggression handed to the console")
m = re.search(r"now (-?\d+)", calmed[0]) if calmed else None
check(len(calmed) >= 1, "it calms down with nobody near")
check(m is not None and int(m.group(1)) <= 1, "calmed: its Aggression back at the record's (now 0)")
check(not any("turns on" in l for l in lines), "no fight sent again every second (the old Keep)")
others = sorted({m.group(1) for l in lines for m in [re.search(r"time near ([0-9A-F]{8}) ran out", l)] if m and m.group(1) != "00000014"})
# A note, not a check: mods (MOO) may put people where Oblivion.esm has none.
print("  note  " + ("near the walker besides the player: " + ", ".join(others) if others else "nobody but the player near the walker"))
for l in (roused + calmed)[:10]:
    print("        " + l)
sys.exit(1 if failures else 0)
PY
