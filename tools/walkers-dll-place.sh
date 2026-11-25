# Where a game folder keeps the walkers' DLL - sourced by tools/build-linux.sh,
# tested by tests/walkers-dll-place-test.sh:
#
#   walkers_dll_place <game>   Data/OBSE/Plugins while the walkers are in the
#                              game (the DLL is there), else
#                              Data/OBSE/WalkersDisabled (parked, or never in)
#
# Whether the walkers are in a game is Nadi's to say, never a deploy's: the
# deploy renews the DLL where it lies. Until 2026-11-25 it copied into Plugins
# always, and from 2026-11-20 that put the walkers back into both games after
# Nadi had taken them out on 2026-11-15.
walkers_dll_place() {
	local obse="$1/Data/OBSE"
	if [[ -f "$obse/Plugins/OblivionWalkers.dll" ]]; then
		printf '%s\n' "$obse/Plugins"
	else
		printf '%s\n' "$obse/WalkersDisabled"
	fi
}
