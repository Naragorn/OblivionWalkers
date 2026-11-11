#!/usr/bin/env bash
# Runs harness/horde-gate-wait.txt on OBVR's harness and checks
# OblivionWalkers.log (4.11c) - the runner itself reads OBVR.log only.
# Exit 0: every check held. Announce "busy" to the other sessions first.
set -euo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
obvr="${OBVR_DIR:-$root/../OBVR}"
game="${WALKERS_LINUX_GAME:-$HOME/.local/share/Steam/steamapps/common/Oblivion}"
log="$game/OblivionWalkers.log"
ini="$game/Data/OBSE/Plugins/OblivionWalkers.ini"
# The siege off for the run (put back whatever happens): a group holding with
# the player near begins one (4.12), and its walkers - joining the groups,
# sent back to the pool when the player leaves - stood in for the hordes in
# the first run (2026-11-11 11:12).
backup="$(mktemp)"
cp "$ini" "$backup"
trap 'cp "$backup" "$ini"; rm -f "$backup"' EXIT
python3 - "$ini" <<'INI'
import re, sys
path = sys.argv[1]
text = open(path, encoding="latin-1", newline="").read()
head = re.search(r"^\[Siege\]\s*$", text, re.M)
if head is None:
    # A deployed INI older than 4.12 has none (the plugin's default is on).
    open(path, "w", encoding="latin-1", newline="").write(text.rstrip("\r\n") + "\r\n\r\n[Siege]\r\nEnabled=0\r\n")
    sys.exit(0)
end = re.search(r"^\[", text[head.end():], re.M)
stop = head.end() + (end.start() if end else len(text) - head.end())
section = re.sub(r"^Enabled=1", "Enabled=0", text[head.end():stop], count=1, flags=re.M)
open(path, "w", encoding="latin-1", newline="").write(text[:head.end()] + section + text[stop:])
INI
grep -n -A2 "^\[Siege\]" "$ini" | head -3
(cd "$obvr" && python3 tools/hand-script-run.py "$root/harness/horde-gate-wait.txt" --headless \
	--artifact-root "$root/artifacts/hand-script") | tee "$root/artifacts/horde-gate-wait-runner.txt"
run="$(ls -td "$root"/artifacts/hand-script/horde-gate-wait/*/ | head -1)"
cp "$log" "$run/OblivionWalkers.log"
python3 - "$run/OblivionWalkers.log" <<'PY'
import sys
lines = open(sys.argv[1], encoding="latin-1").read().splitlines()
failures = 0

def check(ok, what):
    global failures
    print(("  ok    " if ok else "  FAIL  ") + what)
    failures += 0 if ok else 1

def having(text):
    return [l for l in lines if text in l]

on = having("groups a gate at most")
check(len(on) >= 1 and "1 groups a gate at most, only with the player within 8000" in on[0],
      "Horde: on names GateHordesMost=1 and GateNearUnits=8000")
holding = having("holding there")
check(any("Anvil Main Gate" in l for l in holding), "a group holds at Anvil Main Gate")
waits = having("waits out of the player's grid near Anvil Main Gate")
check(len(waits) >= 1, "a group waits out of the player's grid near Anvil Main Gate")
check(any("the player " in l and "units from the gate" in l for l in waits), "the wait line names the player's distance")
import re
far = [l for l in waits if re.search(r"the player (\d+) units", l) and int(re.search(r"the player (\d+) units", l).group(1)) > 8000]
check(len(far) >= 1, "with the player beyond GateNearUnits a group waits (the holding one goes off)")
check(not any("Siege: a horde holds" in l for l in lines), "no siege begun (off for the run)")
check(len(having("moves up to hold at Anvil Main Gate")) >= 1, "a waiting group moves up to hold again")
for l in (waits + having("moves up to hold"))[:20]:
    print("        " + l)
sys.exit(1 if failures else 0)
PY
