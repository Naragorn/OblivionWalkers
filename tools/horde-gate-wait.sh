#!/usr/bin/env bash
# Runs harness/horde-gate-wait.txt on OBVR's harness and checks
# OblivionWalkers.log (4.11c) - the runner itself reads OBVR.log only.
# Exit 0: every check held. Announce "busy" to the other sessions first.
set -euo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
obvr="${OBVR_DIR:-$root/../OBVR}"
game="${WALKERS_LINUX_GAME:-$HOME/.local/share/Steam/steamapps/common/Oblivion}"
log="$game/OblivionWalkers.log"
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
check(len(having("moves up to hold at Anvil Main Gate")) >= 1, "a waiting group moves up to hold again")
for l in (waits + having("moves up to hold"))[:20]:
    print("        " + l)
sys.exit(1 if failures else 0)
PY
