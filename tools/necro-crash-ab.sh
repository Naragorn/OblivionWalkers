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
#   NECRO_BGPATH_OFF=1: Oblivion.ini [Pathfinding] bUseBackgroundPathing=0 for
#     the runs (the second crash's stack is the background A* search's list,
#     read 2026-11-22), the file put back after (md5 checked).
set -uo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
game="${WALKERS_LINUX_GAME:-$HOME/.local/share/Steam/steamapps/common/Oblivion}"
n="${1:-5}"
obng="$game/Data/OBSE/Plugins/OBNG.ini"
oini="${WALKERS_LINUX_OBLIVION_INI:-$HOME/.local/share/Steam/steamapps/compatdata/22330/pfx/drive_c/users/steamuser/Documents/My Games/Oblivion/Oblivion.ini}"
kept=()
keep() {  # a file put back at the end, md5 checked
	local b; b="$(mktemp)"; cp "$1" "$b"; kept+=("$1|$b|$(md5sum "$1" | cut -c1-32)")
}
restore() {
	local e f b s
	for e in "${kept[@]}"; do
		IFS='|' read -r f b s <<< "$e"
		cp "$b" "$f"; rm -f "$b"
		[[ "$(md5sum "$f" | cut -c1-32)" == "$s" ]] && echo "necro-crash-ab: $(basename "$f") restored" || echo "necro-crash-ab: $(basename "$f") NOT restored" >&2
	done
}
trap restore EXIT
off="${NECRO_OBNG_OFF:-}"
[[ "$off" == all ]] && off="SoundFileCache AnimationNameArena FaceGenLoadOutside BodyTexturePremake AnimationLazyAppend"
if [[ -n "$off" ]]; then
	keep "$obng"
	for key in $off; do sed -i -E "s/^($key)=1/\\1=0/" "$obng"; done
fi
if [[ "${NECRO_BGPATH_OFF:-}" == 1 ]]; then
	keep "$oini"
	sed -i -E 's/^(bUseBackgroundPathing)=1/\1=0/' "$oini"
fi
echo "necro-crash-ab: Oblivion.ini $(grep -m1 -E '^bUseBackgroundPathing=' "$oini" | tr -d '\r')"
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
