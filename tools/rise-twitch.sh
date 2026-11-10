#!/usr/bin/env bash
# Runs harness/rise-twitch.txt on OBVR's harness and checks
# OblivionWalkers.log - the runner itself reads OBVR.log only. Exit 0: every
# check held. Announce "busy" to the other sessions first.
set -euo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
obvr="${OBVR_DIR:-$root/../OBVR}"
game="${WALKERS_LINUX_GAME:-$HOME/.local/share/Steam/steamapps/common/Oblivion}"
log="$game/OblivionWalkers.log"
(cd "$obvr" && python3 tools/hand-script-run.py "$root/harness/rise-twitch.txt" --headless \
	--artifact-root "$root/artifacts/hand-script") | tee "$root/artifacts/rise-twitch-runner.txt"
run="$(ls -td "$root"/artifacts/hand-script/rise-twitch/*/ | head -1)"
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

deaths = [l for l in lines if l.startswith("End: walker") and "gets up again" in l]
raises = [l for l in lines if l.startswith("Rise: ") and " gets up at " in l]
stuck = [l for l in lines if "still on its spot" in l]
check(len(deaths) >= 4, "four walkers down, each to get up again (%d)" % len(deaths))
check(len(raises) >= 4, "each raised (%d)" % len(raises))
for l in deaths + raises:
    print("        " + l[:200])
check(not stuck, "none still on its spot 20 s after its raise (%d)" % len(stuck))
for l in stuck:
    print("        " + l)
sys.exit(1 if failures else 0)
PY
