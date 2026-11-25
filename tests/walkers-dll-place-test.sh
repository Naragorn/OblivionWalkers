#!/usr/bin/env bash
# tools/walkers-dll-place.sh: where the deploy renews the walkers' DLL, for
# every way a game folder can hold it. Run by ctest (WalkersDllPlaceTest).
set -euo pipefail
here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=../tools/walkers-dll-place.sh
source "$here/../tools/walkers-dll-place.sh"

tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT
fails=0

# A game folder with the DLL in the named places (plugins, parked).
game() {
	local dir="$tmp/$1"
	shift
	mkdir -p "$dir/Data/OBSE/Plugins"
	for where in "$@"; do
		case "$where" in
		plugins) touch "$dir/Data/OBSE/Plugins/OblivionWalkers.dll" ;;
		parked)
			mkdir -p "$dir/Data/OBSE/WalkersDisabled"
			touch "$dir/Data/OBSE/WalkersDisabled/OblivionWalkers.dll"
			;;
		esac
	done
	printf '%s\n' "$dir"
}

expect() {
	local name="$1" dir="$2" want="$3" got
	got="$(walkers_dll_place "$dir")"
	if [[ "$got" == "$dir/$want" ]]; then
		echo "PASS $name: $want"
	else
		echo "FAIL $name: ${got#"$dir/"}, not $want"
		fails=$((fails + 1))
	fi
}

# The walkers are in: the DLL renewed where the game loads it.
expect in "$(game in plugins)" Data/OBSE/Plugins
# Parked (out): renewed beside the parked one, the game left without it.
expect parked "$(game parked parked)" Data/OBSE/WalkersDisabled
# Never put in: a deploy does not put the walkers into a game either.
expect never-in "$(game never)" Data/OBSE/WalkersDisabled
# In, with an older parked copy beside it: in wins.
expect in-and-parked "$(game both plugins parked)" Data/OBSE/Plugins

((fails == 0))
