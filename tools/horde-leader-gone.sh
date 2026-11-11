#!/usr/bin/env bash
# Runs harness/horde-leader-gone.txt on OBVR's harness and checks
# OblivionWalkers.log - the runner itself reads OBVR.log only. Exit 0: every
# check held. Announce "busy" to the other sessions first.
set -euo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
obvr="${OBVR_DIR:-$root/../OBVR}"
game="${WALKERS_LINUX_GAME:-$HOME/.local/share/Steam/steamapps/common/Oblivion}"
log="$game/OblivionWalkers.log"
(cd "$obvr" && python3 tools/hand-script-run.py "$root/harness/horde-leader-gone.txt" --headless \
	--artifact-root "$root/artifacts/hand-script") | tee "$root/artifacts/horde-leader-gone-runner.txt"
run="$(ls -td "$root"/artifacts/hand-script/horde-leader-gone/*/ | head -1)"
cp "$log" "$run/OblivionWalkers.log"
python3 - "$run/OblivionWalkers.log" <<'PY'
import sys
lines = open(sys.argv[1], encoding="latin-1").read().splitlines()
failures = 0

def check(ok, what):
    global failures
    print(("  ok    " if ok else "  FAIL  ") + what)
    failures += 0 if ok else 1

def count(text):
    return sum(1 for l in lines if text in l)

check(count("follow package aimed at its leader") >= 1, "a group's follow package aimed at its leader")
check(count("its follow package aimed at the group's marker (1)") >= 1, "the dead leaders' packages aimed at the markers")
check(count("its follow package aimed at the group's marker (0)") == 0, "no aim at a marker failed")
check(count("gets up again") >= 1, "the walkers get up again")
stuck = [l for l in lines if l.startswith("Risen: ") and " not up " in l]
print("  info  %d raise(s) still on the spot after the watch's time" % len(stuck))
for l in stuck[:20]:
    print("        " + l)
sys.exit(1 if failures else 0)
PY
