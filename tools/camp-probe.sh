#!/usr/bin/env bash
# Runs harness/camp-probe.txt on OBVR's harness (the Linux game, headless)
# with [Survivors] TestProbe=7 in the live plugin INI (the key put at the
# section's top - the first of a key is the one read). Puts the INI back
# whatever happens, then prints the probe's lines of OblivionWalkers.log.
set -euo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
obvr="${OBVR_DIR:-$root/../OBVR}"
game="${WALKERS_LINUX_GAME:-$HOME/.local/share/Steam/steamapps/common/Oblivion}"
ini="$game/Data/OBSE/Plugins/OblivionWalkers.ini"
backup="$(mktemp)"
cp "$ini" "$backup"
trap 'cp "$backup" "$ini"; rm -f "$backup"' EXIT
if grep -q '^\[Survivors\]' "$ini"; then
	sed -i "s/^\[Survivors\]\r\?$/[Survivors]\nTestProbe=7/" "$ini"
else
	printf "\n[Survivors]\nTestProbe=7\n" >> "$ini"
fi
(cd "$obvr" && python3 tools/hand-script-run.py "$root/harness/camp-probe.txt" --headless \
	--artifact-root "$root/artifacts/hand-script") || true
grep -E '^Camps: (probe|\[Survivors\])' "$game/OblivionWalkers.log" || echo "no probe line"
