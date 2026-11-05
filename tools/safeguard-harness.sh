#!/usr/bin/env bash
# Runs harness/safeguard-home.txt with [Safeguard] TestVictim=0002CA3B in the
# live plugin INI of the Linux game (the key set inside the INI's own
# [Safeguard] section, or the section added), puts the INI back whatever
# happens, and prints the safeguard's lines of OblivionWalkers.log.
set -euo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
obvr="${OBVR_DIR:-$root/../OBVR}"
game="${WALKERS_LINUX_GAME:-$HOME/.local/share/Steam/steamapps/common/Oblivion}"
ini="$game/Data/OBSE/Plugins/OblivionWalkers.ini"
backup="$(mktemp)"
cp "$ini" "$backup"
trap 'cp "$backup" "$ini"; rm -f "$backup"' EXIT
if grep -q '^\[Safeguard\]' "$ini"; then
	sed -i 's/^\[Safeguard\]\r\?$/[Safeguard]\r\nTestVictim=0002CA3B\r/' "$ini"
else
	printf '\r\n[Safeguard]\r\nTestVictim=0002CA3B\r\n' >> "$ini"
fi
(cd "$obvr" && python3 tools/hand-script-run.py "$root/harness/safeguard-home.txt" --headless \
	--artifact-root "$root/artifacts/hand-script")
grep -E '^Safeguard: (test|0002CA3B)' "$game/OblivionWalkers.log" || echo "no safeguard line for 0002CA3B"
