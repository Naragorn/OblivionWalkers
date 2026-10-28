#!/usr/bin/env bash
# Packs the two Nexus downloads (Nadi, 2026-10-28: "für den nexusmods release
# bieten wir beides: esl als erstes mit requirements zu oblivionesl und als
# esp") into dist/:
#
#   OblivionWalkers-ESL-<version>.zip  the ESL-flagged ESP - needs OblivionESL
#                                      and Cell Lookup Robustness
#   OblivionWalkers-ESP-<version>.zip  the plain ESP - no requirement beyond xOBSE
#
# Both hold the same DLL, INI and animations, laid out as a game's Data
# folder. The ESP and the animations are written from the game's own data
# first (tools/esp-write.py, tools/grab-anims.py), so this needs the game:
#
#   tools/package-release.sh <version> [<Oblivion Data folder>]
#
# The DLL is the one in build-linux/ (tools/build-linux.sh); a release should
# be built with MSVC on Windows - this script takes whatever is there and
# says which.
set -euo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
version="${1:?usage: tools/package-release.sh <version> [<Data folder>]}"
data="${2:-$HOME/.local/share/Steam/steamapps/common/Oblivion/Data}"
python="${PYTHON_PYFFI:-$HOME/obvr-venv/bin/python}"
dll="$root/build-linux/OblivionWalkers.dll"
[[ -f "$dll" ]] || { echo "no $dll - run tools/build-linux.sh first" >&2; exit 1; }

work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT
python3 "$root/tools/esp-write.py" "$data/Oblivion.esm" "$data/Maskar's Oblivion Overhaul.esp" \
	--out "$work/plain/OblivionWalkers.esp" >/dev/null
python3 "$root/tools/esp-write.py" "$data/Oblivion.esm" "$data/Maskar's Oblivion Overhaul.esp" --esl \
	--out "$work/esl/OblivionWalkers.esp" >/dev/null
"$python" "$root/tools/grab-anims.py" "$data/Oblivion - Meshes.bsa" --out "$work/meshes" >/dev/null

readme() {
	local variant="$1"
	{
		echo "Oblivion Walkers $version - the $variant download"
		echo
		echo "Install: copy the Data folder into Oblivion's Data folder (or install the archive"
		echo "with your mod manager) and activate OblivionWalkers.esp."
		echo
		echo "Needs: Oblivion 1.2.416, xOBSE 22.13 or later."
		if [[ "$variant" == ESL ]]; then
			echo "       OblivionESL (Nexus 56578) - loads the ESP as a light plugin, so it takes no load"
			echo "       order slot; and Cell Lookup Robustness (Nexus 55924), because the ESP has a cell."
			echo "       OBL Mod Limit Fix (Nexus 50066) is recommended for large load orders."
		fi
		echo
		echo "ESL or ESP: the same mod; take the ESL one if you use OblivionESL. Switching an existing"
		echo "game from one to the other keeps the game, but the walkers that already rose from the"
		echo "fallen are gone, with what they carried; the spawned ones come again as cells reset."
		echo
		echo "Settings: Data/OBSE/Plugins/OblivionWalkers.ini, and in the game with LINK (Nexus 50144)."
		echo "Source and licence (GPL-3.0): https://github.com/Naragorn/OblivionWalkers"
	} > "$2"
}

mkdir -p "$root/dist"
for variant in ESL ESP; do
	stage="$work/stage-$variant"
	mkdir -p "$stage/Data/OBSE/Plugins"
	cp "$dll" "$stage/Data/OBSE/Plugins/"
	cp "$root/OblivionWalkers.ini" "$stage/Data/OBSE/Plugins/"
	if [[ "$variant" == ESL ]]; then
		cp "$work/esl/OblivionWalkers.esp" "$stage/Data/"
	else
		cp "$work/plain/OblivionWalkers.esp" "$stage/Data/"
	fi
	mkdir -p "$stage/Data/Meshes"
	cp -r "$work/meshes/." "$stage/Data/Meshes/"
	readme "$variant" "$stage/OblivionWalkers-README.txt"
	zip_path="$root/dist/OblivionWalkers-$variant-$version.zip"
	rm -f "$zip_path"
	(cd "$stage" && zip -qr "$zip_path" .)
	echo "$zip_path: $(unzip -l "$zip_path" | tail -1 | awk '{print $2}') files"
done
echo "DLL: $dll ($(stat -c %y "$dll" | cut -d. -f1)) - built with clang-cl on Linux unless replaced by an MSVC build"
