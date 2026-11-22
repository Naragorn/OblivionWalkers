#!/usr/bin/env bash
# The pool-12 crash (docs/walkers-spec.md 4.23 B): tools/necro-harness.sh
# daedra N times with TestFight=daedra (the plugin's MoveTo, StartCombat and
# StopCombat lines on a Dremora and the necromancer) and N times without
# (NECRO_NOFIGHT=1: the Dremora only put down), alternating; [Debug]
# PoolProbe=1 in both. A line a run: the arm, crashed or not, the probe's
# first broken link, OBNG's three routes. Needs tools/linux-walkers.sh on.
#
#   tools/necro-crash-ab.sh [N]    (default 5 of each)
set -uo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
game="${WALKERS_LINUX_GAME:-$HOME/.local/share/Steam/steamapps/common/Oblivion}"
n="${1:-5}"
routes="$(grep -E '^(FaceGenLoadOutside|BodyTexturePremake|AnimationLazyAppend|SoundFileCache|AnimationNameArena)=' "$game/Data/OBSE/Plugins/OBNG.ini" 2>/dev/null | tr '\n' ' ')"
echo "necro-crash-ab: $n of each arm; OBNG routes: ${routes:-none read}"
for i in $(seq 1 "$n"); do
	for arm in fight nofight; do
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
