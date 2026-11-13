#!/usr/bin/env bash
# Runs harness/dungeon-sites.txt on OBVR's harness and checks
# OblivionWalkers.log. Exit 0: every check held. Announce "busy" first.
set -euo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
obvr="${OBVR_DIR:-$root/../OBVR}"
game="${WALKERS_LINUX_GAME:-$HOME/.local/share/Steam/steamapps/common/Oblivion}"
(cd "$obvr" && python3 tools/hand-script-run.py "$root/harness/dungeon-sites.txt" --headless \
	--artifact-root "$root/artifacts/hand-script") | tee "$root/artifacts/dungeon-sites-runner.txt"
run="$(ls -td "$root"/artifacts/hand-script/dungeon-sites/*/ | head -1)"
cp "$game/OblivionWalkers.log" "$run/OblivionWalkers.log"
python3 - "$run/OblivionWalkers.log" <<'PY'
import re, sys
lines = open(sys.argv[1], encoding="latin-1").read().splitlines()
failures = 0

def check(ok, what):
    global failures
    print(("  ok    " if ok else "  FAIL  ") + what)
    failures += 0 if ok else 1

ev = [l for l in lines if l.startswith("Dungeons: site ")]
reads = [l for l in lines if l.startswith("Co-save: ") and "held dungeons read" in l]
writes = [l for l in lines if l.startswith("Co-save: ") and "held dungeons written" in l]
rolled = [l for l in ev if " rolled - now " in l]
check(len(rolled) >= 3, f"three or more places rolled ({len(rolled)})")
written = int(re.search(r"Co-save: (\d+) held", writes[0]).group(1)) if writes else -1
read = [int(re.search(r"Co-save: (\d+) held", l).group(1)) for l in reads]
check(written >= 3 and written in read, f"the save wrote {written}, a load read them back ({read})")
kinds = {k: sum(1 for l in ev if f") {k} - now" in l) for k in
         ("attacked", "held", "overrun", "given up", "one of its people lost")}
check(kinds["given up"] >= 1, f"eight game days unseen: a place given up ({kinds})")
check(len(rolled) > written, "and another rolled for it")
for l in ev[:40]:
    print("        " + l)
sys.exit(1 if failures else 0)
PY
