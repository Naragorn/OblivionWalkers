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
scenario="${CAMP_SCENARIO:-defend}"
if [[ "$scenario" == escort || "$scenario" == escort-dead ]]; then
	keys='TestEscort=7\nAttackChancePercent=0'
	[[ "${ESCORT_BEHIND:-0}" == 1 ]] && keys="$keys\\nTestEscortBehind=1"
	# CAMP_KEYS: more [Survivors] keys, separated by \n (e.g. EscortFollowSeconds=0).
	[[ -n "${CAMP_KEYS:-}" ]] && keys="$keys\\n$CAMP_KEYS"
elif [[ "$scenario" == leash ]]; then
	keys='TestLeash=7\nAttackChancePercent=0'
elif [[ "$scenario" == daedra ]]; then
	keys='TestCamp=7\nTestDaedra=1\nAttackWalkersMin=4\nAttackWalkersMax=4'
	if grep -q '^\[Invaders\]' "$ini"; then
		sed -i '0,/^\[Invaders\]\r\?$/s//[Invaders]\nBanishSeconds=5/' "$ini"
	else
		printf "\n[Invaders]\nBanishSeconds=5\n" >> "$ini"
	fi
else
	keys='TestCamp=7\nAttackWalkersMin=6\nAttackWalkersMax=6'
fi
if grep -q '^\[Survivors\]' "$ini"; then
	sed -i "s/^\[Survivors\]\r\?$/[Survivors]\n$keys/" "$ini"
else
	printf "\n[Survivors]\n$keys\n" >> "$ini"
fi
# CAMP_ENDS=1: [Walkers] DieOnlyBeheaded=0 too, so the refugees' kills end
# the walkers and the attack reaches its end (with OBVR cutting heads only
# the player's cut ends one - the run of 14:56).
# The held dungeons off for the run: their runner's news opens the journal's
# box, which holds the world while the camp's test clock runs on (the escort
# run of 2026-11-14 07:23 - nobody moved for its twenty seconds).
if grep -q '^\[Dungeons\]' "$ini"; then
	sed -i '0,/^\[Dungeons\]\r\?$/s//[Dungeons]\nEnabled=0/' "$ini"
else
	printf "\n[Dungeons]\nEnabled=0\n" >> "$ini"
fi
# escort-dead: the dead stay dead ([Rise] Enabled=0 - harness/camp-escort-dead.txt).
if [[ "$scenario" == escort-dead ]]; then
	grep -q '^\[Rise\]' "$ini" || { echo "no [Rise] section in $ini"; exit 1; }
	sed -i '0,/^\[Rise\]\r\?$/s//[Rise]\nEnabled=0/' "$ini"
fi
if [[ "${CAMP_ENDS:-0}" == 1 ]]; then
	# [Walkers] (named [Zombies] until 2026-11-16; the plugin reads the new
	# name first - IniNames.h): added when the INI has no such section yet.
	if grep -q '^\[Walkers\]' "$ini"; then
		sed -i '0,/^\[Walkers\]\r\?$/s//[Walkers]\nDieOnlyBeheaded=0/' "$ini"
	else
		printf "\n[Walkers]\nDieOnlyBeheaded=0\n" >> "$ini"
	fi
fi
(cd "$obvr" && python3 tools/hand-script-run.py "$root/harness/camp-$scenario.txt" --headless ${WALKERS_RUNNER_ARGS---no-steamvr} \
	--artifact-root "$root/artifacts/hand-script")
run="$(ls -td "$root"/artifacts/hand-script/camp-$scenario/*/ | head -1)"
cp "$game/OblivionWalkers.log" "$run/OblivionWalkers.log"
echo "run: $run"
grep -E '^(Camps:|Co-save: the camps|Death: |End: walker|Placer:|Banish:)' "$game/OblivionWalkers.log" | head -${CAMP_LINES:-150} \
	|| echo "no camp line"
