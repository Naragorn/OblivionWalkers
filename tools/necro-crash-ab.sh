#!/usr/bin/env bash
# The pool-12 crash (docs/walkers-spec.md 4.23 B): tools/necro-harness.sh
# daedra N times with TestFight=daedra (the plugin's MoveTo, StartCombat and
# StopCombat lines on a Dremora and the necromancer) and N times without
# (NECRO_NOFIGHT=1: the Dremora only put down), alternating; [Debug]
# PoolProbe=1 in both. A line a run: the arm, crashed or not, the probe's
# first broken link, OBNG's three routes. Needs tools/linux-walkers.sh on.
#
#   tools/necro-crash-ab.sh [N]    (default 5 of each)
#   NECRO_ARMS="fight" NECRO_OBNG_OFF="SoundFileCache AnimationNameArena" tools/necro-crash-ab.sh 5
#     only the fight arm, with those OBNG [Route] keys set to 0 for the runs
#     (OBNG.ini put back after, md5 checked; OBNG asked for it 2026-11-22);
#     NECRO_OBNG_OFF=all: all five.
set -uo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
game="${WALKERS_LINUX_GAME:-$HOME/.local/share/Steam/steamapps/common/Oblivion}"
n="${1:-5}"
obng="$game/Data/OBSE/Plugins/OBNG.ini"
off="${NECRO_OBNG_OFF:-}"
[[ "$off" == all ]] && off="SoundFileCache AnimationNameArena FaceGenLoadOutside BodyTexturePremake AnimationLazyAppend"
if [[ -n "$off" ]]; then
	backup="$(mktemp)"; cp "$obng" "$backup"; sum="$(md5sum "$obng" | cut -c1-32)"
	trap 'cp "$backup" "$obng"; [[ "$(md5sum "$obng" | cut -c1-32)" == "$sum" ]] && echo "necro-crash-ab: OBNG.ini restored" || echo "necro-crash-ab: OBNG.ini NOT restored" >&2; rm -f "$backup"' EXIT
	for key in $off; do sed -i -E "s/^($key)=1/\\1=0/" "$obng"; done
fi
routes="$(grep -E '^(FaceGenLoadOutside|BodyTexturePremake|AnimationLazyAppend|SoundFileCache|AnimationNameArena)=' "$game/Data/OBSE/Plugins/OBNG.ini" 2>/dev/null | tr '\n' ' ')"
echo "necro-crash-ab: $n of each arm; OBNG routes: ${routes:-none read}"
for i in $(seq 1 "$n"); do
	for arm in ${NECRO_ARMS:-fight nofight}; do
		if [[ "$arm" == nofight ]]; then export NECRO_NOFIGHT=1; else unset NECRO_NOFIGHT; fi
		"$root/tools/necro-harness.sh" daedra > "$root/artifacts/necro-crash-ab-last.txt" 2>&1
		run="$(ls -td "$root"/artifacts/hand-script/necro-daedra/*/ | head -1)"
		crash="no"
		grep -q '^CRASH:' "$run/OBVR.log" 2>/dev/null && crash="CRASH $(grep -m1 '^CRASH: access' "$run/OBVR.log" | cut -c1-90)"
		probe="$(grep -m1 'Pool probe: BROKEN' "$run/OblivionWalkers.log" 2>/dev/null | cut -c1-120)"
		checked="$(grep -c 'Pool probe: on' "$run/OblivionWalkers.log" 2>/dev/null)"
		echo "run $i $arm: $crash; probe on=$checked ${probe:-no broken link}; $(basename "$run")"
	done
done
