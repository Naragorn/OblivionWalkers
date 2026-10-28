#!/usr/bin/env bash
# Builds OblivionWalkers.dll on Linux with clang-cl and lld-link against
# xwin's winsysroot (cmake/toolchain-linux-clang-cl.cmake), after running the
# unit tests natively (clang++ -m32). Releases are built with MSVC on Windows;
# this is the build for working on Linux.
#
#   tools/build-linux.sh            tests, then the DLL into build-linux/
#   tools/build-linux.sh --deploy   also copy the DLL (and the INI, if none is
#                                   there yet) into every game's Data/OBSE/Plugins,
#                                   the ESP (tools/esp-write.py, its file time in
#                                   the game kept - the load order goes by it) and
#                                   the grab's animations (tools/grab-anims.py,
#                                   build-linux/meshes) into its Data - refused
#                                   while Oblivion runs
#
# The games: WALKERS_GAME_DIRS (colon-separated) or the two Steam copies on
# this machine, the Linux one and the Windows one.
set -euo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
deploy=0
if [[ "${1:-}" == "--deploy" ]]; then
	deploy=1
fi

# A folder beneath `base`, each part matched without regard to case (the
# games hold both Meshes and meshes; Windows sees one): the part as written
# if it is there, else any spelling of it, else made as written.
resolve_dir() {
	local base="$1" rel="$2" part found
	IFS='/' read -r -a parts <<< "$rel"
	for part in "${parts[@]}"; do
		if [[ -d "$base/$part" ]]; then
			base="$base/$part"
			continue
		fi
		found="$(find "$base" -mindepth 1 -maxdepth 1 -type d -iname "$part" -print -quit)"
		if [[ -z "$found" ]]; then
			mkdir -p "$base/$part"
			found="$base/$part"
		fi
		base="$found"
	done
	printf '%s\n' "$base"
}

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
		if [[ -f "$root/OblivionWalkers.esp" ]]; then
			esp="$game/Data/OblivionWalkers.esp"
			stamp=""
			[[ -f "$esp" ]] && stamp="$(stat -c %Y "$esp")"
			cp "$root/OblivionWalkers.esp" "$esp"
			[[ -n "$stamp" ]] && touch -d "@$stamp" "$esp"
			echo "ESP to $esp (sha256 $(sha256sum "$esp" | cut -c1-8))"
		fi
		if [[ -d "$root/build-linux/meshes" ]]; then
			while IFS= read -r -d '' kf; do
				rel="${kf#"$root/build-linux/meshes/"}"
				dir="$(resolve_dir "$game/Data" "Meshes/$(dirname "$rel")")"
				cp "$kf" "$dir/"
				echo "  $dir/$(basename "$kf")"
			done < <(find "$root/build-linux/meshes" -type f -name '*.kf' -print0)
		fi
	done
fi
