#!/usr/bin/env bash
# Runs harness/camp-defend.txt on OBVR's harness (the Linux game, headless)
# with camp 07 attacked at once: [Survivors] TestCamp=7, AttackWalkersMin=6,
# AttackWalkersMax=6 in the live plugin INI (the section added, or the keys
# put at its top - the first of a key is the one read). Puts the INI back
# whatever happens, then prints the camps' lines of OblivionWalkers.log.
set -euo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
obvr="${OBVR_DIR:-$root/../OBVR}"
game="${WALKERS_LINUX_GAME:-$HOME/.local/share/Steam/steamapps/common/Oblivion}"
ini="$game/Data/OBSE/Plugins/OblivionWalkers.ini"
backup="$(mktemp)"
cp "$ini" "$backup"
trap 'cp "$backup" "$ini"; rm -f "$backup"' EXIT
keys='TestCamp=7\nAttackWalkersMin=6\nAttackWalkersMax=6'
if grep -q '^\[Survivors\]' "$ini"; then
	sed -i "s/^\[Survivors\]\r\?$/[Survivors]\n$keys/" "$ini"
else
	printf "\n[Survivors]\n$keys\n" >> "$ini"
fi
# CAMP_ENDS=1: [Zombies] DieOnlyBeheaded=0 too, so the refugees' kills end
# the walkers and the attack reaches its end (with OBVR cutting heads only
# the player's cut ends one - the run of 14:56).
if [[ "${CAMP_ENDS:-0}" == 1 ]]; then
	grep -q '^\[Zombies\]' "$ini" || { echo "no [Zombies] section in $ini"; exit 1; }
	sed -i '0,/^\[Zombies\]\r\?$/s//[Zombies]\nDieOnlyBeheaded=0/' "$ini"
fi
(cd "$obvr" && python3 tools/hand-script-run.py "$root/harness/camp-defend.txt" --headless \
	--artifact-root "$root/artifacts/hand-script")
grep -E '^(Camps:|Co-save: the camps|Death: |End: walker)' "$game/OblivionWalkers.log" | head -150 \
	|| echo "no camp line"
