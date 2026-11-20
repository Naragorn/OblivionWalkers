#!/usr/bin/env bash
# Sourced by the harness wrappers (docs/walkers-spec.md 4.24, 4.24a). The
# wrapper backs the INI up and puts it back itself.
#
# walkers_onboarding_off INI   [Onboarding] Show=0: no box over a run (the
#                              code's default is 1, the chain of boxes).
# walkers_apocalypse INI       the runs that need the apocalypse - the dead
#                              rise, hordes, sieges, camps, held dungeons,
#                              every category replaced - put [Walkers] Mode=2
#                              in (the code's default is Mode 1, the game's
#                              zombies only), and the onboarding off.
#
# Each key lands right under its header, so it is read first when the INI has
# one already.
walkers_onboarding_off() {
	local ini="$1"
	if grep -q '^\[Onboarding\]' "$ini"; then
		sed -i '0,/^\[Onboarding\]\r\?$/s//[Onboarding]\nShow=0/' "$ini"
	else
		printf '\n[Onboarding]\nShow=0\n' >> "$ini"
	fi
}

walkers_apocalypse() {
	local ini="$1"
	if grep -q '^\[Walkers\]' "$ini"; then
		sed -i '0,/^\[Walkers\]\r\?$/s//[Walkers]\nMode=2/' "$ini"
	else
		printf '\n[Walkers]\nMode=2\n' >> "$ini"
	fi
	walkers_onboarding_off "$ini"
}
