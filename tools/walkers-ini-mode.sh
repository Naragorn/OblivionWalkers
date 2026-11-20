#!/usr/bin/env bash
# Sourced by the harness wrappers (docs/walkers-spec.md 4.24): the runs that
# need the apocalypse - the dead rise, hordes, sieges, camps, held dungeons,
# every category replaced - put [Walkers] Mode=2 into the INI at `$1`; the
# code's default is Mode 1 (the game's zombies only). The wrapper backs the
# INI up and puts it back itself. Mode=2 lands right under the header, so it
# is read first when the INI has a Mode already.
walkers_apocalypse() {
	local ini="$1"
	if grep -q '^\[Walkers\]' "$ini"; then
		sed -i '0,/^\[Walkers\]\r\?$/s//[Walkers]\nMode=2/' "$ini"
	else
		printf '\n[Walkers]\nMode=2\n' >> "$ini"
	fi
}
