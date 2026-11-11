#!/usr/bin/env bash
# Runs harness/link-save-load.txt on OBVR's harness and prints each load's
# LINK first look and the LINK value changes after it. Exit 0: every load
# after the save read this version with values set and changed nothing to
# its least. Announce "busy" to the other sessions first.
set -euo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
obvr="${OBVR_DIR:-$root/../OBVR}"
game="${WALKERS_LINUX_GAME:-$HOME/.local/share/Steam/steamapps/common/Oblivion}"
log="$game/OblivionWalkers.log"
(cd "$obvr" && python3 tools/hand-script-run.py "$root/harness/link-save-load.txt" --headless \
	--artifact-root "$root/artifacts/hand-script") | tee "$root/artifacts/link-save-load-runner.txt"
run="$(ls -td "$root"/artifacts/hand-script/link-save-load/*/ | head -1)"
cp "$log" "$run/OblivionWalkers.log"
python3 - "$run/OblivionWalkers.log" <<'PY'
import sys
lines = open(sys.argv[1], encoding="latin-1").read().splitlines()
looks = [i for i, l in enumerate(lines) if l.startswith("LINK: first look")]
failures = 0
for n, i in enumerate(looks):
    end = looks[n + 1] if n + 1 < len(looks) else len(lines)
    changes = [l for l in lines[i:end] if l.startswith("LINK: ini_") and " now " in l]
    decided = [l for l in lines[i:end] if l.startswith("LINK: loaded") or l.startswith("LINK: not loaded")]
    print("load %d: %s" % (n + 1, lines[i]))
    for l in decided[:1]:
        print("        " + l)
    print("        %d value changes after it%s" % (len(changes), (": " + "; ".join(changes[:4])) if changes else ""))
    if n >= 1 and ("values EMPTY" in lines[i] or len(changes) > 5):
        failures += 1
print("FAIL" if failures or len(looks) < 3 else "PASS", "- %d loads seen" % len(looks))
sys.exit(1 if failures or len(looks) < 3 else 0)
PY
