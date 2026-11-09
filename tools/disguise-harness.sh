#!/usr/bin/env bash
# Runs harness/disguise.txt on OBVR's harness (the Linux game, headless) with
# [Disguise] Test=1 and Seconds=40 in the live plugin INI (the keys put at the
# top of the section, or the section added - the first of a key is the one
# read). Puts the INI back whatever happens, then prints the disguise's and
# the slow aggression's lines of OblivionWalkers.log.
set -euo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
obvr="${OBVR_DIR:-$root/../OBVR}"
game="${WALKERS_LINUX_GAME:-$HOME/.local/share/Steam/steamapps/common/Oblivion}"
ini="$game/Data/OBSE/Plugins/OblivionWalkers.ini"
backup="$(mktemp)"
cp "$ini" "$backup"
trap 'cp "$backup" "$ini"; rm -f "$backup"' EXIT
keys='Test=1\nSeconds=40'
if grep -q '^\[Disguise\]' "$ini"; then
	sed -i "s/^\[Disguise\]\r\?$/[Disguise]\n$keys/" "$ini"
else
	printf "\n[Disguise]\n$keys\n" >> "$ini"
fi
(cd "$obvr" && python3 tools/hand-script-run.py "$root/harness/disguise.txt" --headless \
	--artifact-root "$root/artifacts/hand-script") || true
grep -E '^(Disguise:|Activate watch|Slow aggression: walker)' "$game/OblivionWalkers.log" | head -60 || true
