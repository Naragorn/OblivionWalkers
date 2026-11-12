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
python3 "$root/tools/link-save-load-check.py" "$run/OblivionWalkers.log"
