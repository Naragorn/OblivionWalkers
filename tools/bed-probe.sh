#!/usr/bin/env bash
# Runs harness/bed-probe.txt on OBVR's harness (the Linux game, headless)
# with the held dungeons off ([Dungeons] Enabled=0 in the live plugin INI,
# put back whatever happens), so the plugin leaves its beds to the console.
# Prints the run's folder; the eye pictures a..f are the result. Announce
# "busy" to the other sessions first.
set -euo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
obvr="${OBVR_DIR:-$root/../OBVR}"
game="${WALKERS_LINUX_GAME:-$HOME/.local/share/Steam/steamapps/common/Oblivion}"
ini="$game/Data/OBSE/Plugins/OblivionWalkers.ini"
backup="$(mktemp)"
cp "$ini" "$backup"
trap 'cp "$backup" "$ini"; rm -f "$backup"' EXIT
if grep -q '^\[Dungeons\]' "$ini"; then
	sed -i '0,/^\[Dungeons\]\r\?$/s//[Dungeons]\nEnabled=0/' "$ini"
else
	printf "\n[Dungeons]\nEnabled=0\n" >> "$ini"
fi
(cd "$obvr" && python3 tools/hand-script-run.py "$root/harness/bed-probe.txt" --headless \
	--artifact-root "$root/artifacts/hand-script") > "$root/artifacts/bed-probe-runner.txt" 2>&1 || true
tail -1 "$root/artifacts/bed-probe-runner.txt"
run="$(ls -td "$root"/artifacts/hand-script/bed-probe/*/ | head -1)"
cp "$game/OblivionWalkers.log" "$run/OblivionWalkers.log"
echo "run: $run"
grep -E 'Console line: ".*(A8F|A90|A91|placeatme)' "$run/OBVR.log" | cut -c1-140 || true
grep -E '^Dungeons: beds' "$run/OblivionWalkers.log" || echo "the plugin said nothing of the beds"
