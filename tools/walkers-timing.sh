#!/usr/bin/env bash
# Runs harness/walkers-timing.txt with [Debug] StepTiming=1 in the live INI
# (put back whatever happens) and prints the log's timing lines. Announce
# "busy" first. Optional: a label for the run (before/after a change).
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
if re.search(r"^\[Debug\]\s*$", text, re.M):
    if re.search(r"^StepTiming=", text, re.M):
        text = re.sub(r"^StepTiming=.*$", "StepTiming=1", text, count=1, flags=re.M)
    else:
        text = re.sub(r"^\[Debug\]\s*$", "[Debug]\r\nStepTiming=1", text, count=1, flags=re.M)
else:
    text += "\r\n[Debug]\r\nStepTiming=1\r\n"
open(path, "w", encoding="latin-1", newline="").write(text)
INI
(cd "$obvr" && python3 tools/hand-script-run.py "$root/harness/walkers-timing.txt" --headless \
	--artifact-root "$root/artifacts/hand-script") > "$root/artifacts/walkers-timing-runner.txt" 2>&1 || true
run="$(ls -td "$root"/artifacts/hand-script/walkers-timing/*/ | head -1)"
cp "$game/OblivionWalkers.log" "$run/OblivionWalkers.log"
echo "run: $run ${1:-}"
grep -E "^Timing:|walkers on the process lists" "$run/OblivionWalkers.log" | tail -60
