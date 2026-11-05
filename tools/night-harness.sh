#!/usr/bin/env bash
# Runs harness/night-pace.txt with the nights made plain to measure in the
# live plugin INI of the Linux game: [Night] SpeedMultiplier=2, RampHours=0
# (the section added) and [Rise] AfterSeconds=10, JitterSeconds=0 (set at the
# top of the INI's own [Rise] section - the first of a key is the one read).
# Puts the INI back whatever happens, then prints the night's, the pace's
# and the counts' lines of OblivionWalkers.log.
set -euo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
obvr="${OBVR_DIR:-$root/../OBVR}"
game="${WALKERS_LINUX_GAME:-$HOME/.local/share/Steam/steamapps/common/Oblivion}"
ini="$game/Data/OBSE/Plugins/OblivionWalkers.ini"
backup="$(mktemp)"
cp "$ini" "$backup"
trap 'cp "$backup" "$ini"; rm -f "$backup"' EXIT
grep -q '^\[Rise\]' "$ini" || { echo "no [Rise] section in $ini"; exit 1; }
sed -i '0,/^\[Rise\]\r\?$/s//[Rise]\nAfterSeconds=10\nJitterSeconds=0/' "$ini"
if grep -q '^\[Night\]' "$ini"; then
	sed -i 's/^\[Night\]\r\?$/[Night]\nSpeedMultiplier=2\nRampHours=0/' "$ini"
else
	printf '\n[Night]\nSpeedMultiplier=2\nRampHours=0\n' >> "$ini"
fi
(cd "$obvr" && python3 tools/hand-script-run.py "$root/harness/night-pace.txt" --headless \
	--artifact-root "$root/artifacts/hand-script")
grep -E '^(Night:|Walk: walker|Walkers: [0-9A-F]{8} speed|Spawns: WalkersZombieVariant|Rise: \[Rise\])' \
	"$game/OblivionWalkers.log" || echo "no night line"
