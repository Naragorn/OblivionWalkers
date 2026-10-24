#!/usr/bin/env bash
# Builds OblivionWalkers.dll on Linux with clang-cl and lld-link against
# xwin's winsysroot (cmake/toolchain-linux-clang-cl.cmake), after running the
# unit tests natively (clang++ -m32). Releases are built with MSVC on Windows;
# this is the build for working on Linux.
#
#   tools/build-linux.sh            tests, then the DLL into build-linux/
#   tools/build-linux.sh --deploy   also copy the DLL (and the INI, if none is
#                                   there yet) into every game's Data/OBSE/Plugins
#                                   - refused while Oblivion runs
#
# The games: WALKERS_GAME_DIRS (colon-separated) or the two Steam copies on
# this machine, the Linux one and the Windows one.
set -euo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
deploy=0
if [[ "${1:-}" == "--deploy" ]]; then
	deploy=1
fi

# The game, if it runs: the process whose program is Oblivion.exe (under
# Proton its name reads "Main", so pgrep -x does not find it).
game_running() {
	local cmdline
	for cmdline in /proc/[0-9]*/cmdline; do
		local argv0
		argv0="$(tr '\0' '\n' < "$cmdline" 2>/dev/null | head -n 1)" || continue
		argv0="${argv0//\\//}"
		if [[ "${argv0##*/}" =~ ^[Oo][Bb][Ll][Ii][Vv][Ii][Oo][Nn]\.[Ee][Xx][Ee]$ ]]; then
			return 0
		fi
	done
	return 1
}

cmake -S "$root/tests" -B "$root/build-linux-tests" -G Ninja -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_CXX_FLAGS=-m32 >/dev/null
cmake --build "$root/build-linux-tests"
ctest --test-dir "$root/build-linux-tests" --output-on-failure

cmake -S "$root" -B "$root/build-linux" -G Ninja -DCMAKE_BUILD_TYPE=Release \
	--toolchain "$root/cmake/toolchain-linux-clang-cl.cmake" >/dev/null
cmake --build "$root/build-linux"
ls -la "$root/build-linux/OblivionWalkers.dll"

if [[ $deploy -eq 1 ]]; then
	if game_running; then
		echo "Oblivion is running - nothing deployed" >&2
		exit 1
	fi
	games="${WALKERS_GAME_DIRS:-$HOME/.local/share/Steam/steamapps/common/Oblivion:/run/media/nadi/Programs/Steam/steamapps/common/Oblivion}"
	IFS=':' read -r -a game_list <<< "$games"
	for game in "${game_list[@]}"; do
		plugins="$game/Data/OBSE/Plugins"
		if [[ ! -d "$plugins" ]]; then
			echo "No plugin folder at $plugins - skipped" >&2
			continue
		fi
		cp "$root/build-linux/OblivionWalkers.dll" "$plugins/"
		[[ -f "$plugins/OblivionWalkers.ini" ]] || cp "$root/OblivionWalkers.ini" "$plugins/"
		echo "Deployed to $plugins"
	done
fi
