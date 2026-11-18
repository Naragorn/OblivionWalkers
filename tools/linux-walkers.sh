#!/usr/bin/env bash
# Puts the walkers into the Linux game for harness runs and takes them out
# again (Nadi, 2026-11-15: the walkers stay out of the game while Nadi tests
# VR; harness runs allowed).
#
#   tools/linux-walkers.sh on    the built DLL into OBSE/Plugins, the built
#                                OblivionWalkers.esp into Data, the ESP's line
#                                into Plugins.txt (at its old place)
#   tools/linux-walkers.sh off   all three as they were before "on"
#
# "on" keeps what it replaces in Data/OBSE/WalkersDisabled/harness-before/;
# "off" puts that back and removes the folder. Refused while Oblivion runs.
set -euo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
game="${WALKERS_LINUX_GAME:-$HOME/.local/share/Steam/steamapps/common/Oblivion}"
plugins_txt="${WALKERS_LINUX_PLUGINS:-$HOME/.local/share/Steam/steamapps/compatdata/22330/pfx/drive_c/users/steamuser/AppData/Local/Oblivion/Plugins.txt}"
disabled="$game/Data/OBSE/WalkersDisabled"
before="$disabled/harness-before"
dll="$game/Data/OBSE/Plugins/OblivionWalkers.dll"
esp="$game/Data/OblivionWalkers.esp"

# The game by its own argv[0] (as tools/build-linux.sh): pgrep -f also
# matches other shells whose command line merely names Oblivion.exe (a peer
# session's wait loop, 2026-11-16).
game_running() {
	local cmdline argv0
	for cmdline in /proc/[0-9]*/cmdline; do
		argv0="$(tr '\0' '\n' < "$cmdline" 2>/dev/null | head -n 1)" || continue
		argv0="${argv0//\\//}"
		if [[ "${argv0##*/}" =~ ^[Oo][Bb][Ll][Ii][Vv][Ii][Oo][Nn]\.[Ee][Xx][Ee]$ ]]; then
			return 0
		fi
	done
	return 1
}

# A harness run's game takes some seconds to quit after its runner ends: wait
# up to WALKERS_WAIT_SECONDS (60) for it before refusing (2026-11-18: an "off"
# right after a run refused, a FREE went out with the walkers still in).
waited=0
while game_running && (( waited < ${WALKERS_WAIT_SECONDS:-60} )); do
	sleep 2
	waited=$((waited + 2))
done
if game_running; then
	echo "linux-walkers: Oblivion runs - nothing changed (waited ${waited} s)" >&2
	exit 1
fi

case "${1:-}" in
on)
	if [[ -d "$before" ]]; then
		echo "linux-walkers: already on ($before exists)" >&2
		exit 1
	fi
	[[ -f "$root/build-linux/OblivionWalkers.dll" && -f "$root/OblivionWalkers.esp" ]] || {
		echo "linux-walkers: build the DLL and the ESP first" >&2
		exit 1
	}
	mkdir -p "$before"
	cp -p "$plugins_txt" "$before/Plugins.txt"
	[[ -f "$esp" ]] && cp -p "$esp" "$before/OblivionWalkers.esp"
	[[ -f "$dll" ]] && cp -p "$dll" "$before/OblivionWalkers.dll"
	cp "$root/build-linux/OblivionWalkers.dll" "$dll"
	cp "$root/OblivionWalkers.esp" "$esp"
	if ! grep -qi '^OblivionWalkers.esp' "$plugins_txt"; then
		# its old place: after the line before it in the backup of the day it went out
		python3 - "$plugins_txt" "$disabled/linux-Plugins.txt" <<'PY'
import sys
path, old = sys.argv[1], sys.argv[2]
text = open(path, encoding="latin-1", newline="").read()
nl = "\r\n" if "\r\n" in text else "\n"
lines = text.split(nl)
after = None
try:
    old_lines = open(old, encoding="latin-1", newline="").read().replace("\r\n", "\n").split("\n")
    at = [l.lower() for l in old_lines].index("oblivionwalkers.esp")
    after = old_lines[at - 1] if at > 0 else None
except (OSError, ValueError):
    pass
if after is not None and after in lines:
    lines.insert(lines.index(after) + 1, "OblivionWalkers.esp")
else:
    while lines and lines[-1] == "":
        lines.pop()
    lines += ["OblivionWalkers.esp", ""]
open(path, "w", encoding="latin-1", newline="").write(nl.join(lines))
PY
	fi
	echo "linux-walkers: on - DLL $(md5sum "$dll" | cut -c1-8), ESP $(md5sum "$esp" | cut -c1-8)"
	;;
off)
	[[ -d "$before" ]] || { echo "linux-walkers: not on" >&2; exit 1; }
	cp -p "$before/Plugins.txt" "$plugins_txt"
	if [[ -f "$before/OblivionWalkers.esp" ]]; then cp -p "$before/OblivionWalkers.esp" "$esp"; else rm -f "$esp"; fi
	if [[ -f "$before/OblivionWalkers.dll" ]]; then cp -p "$before/OblivionWalkers.dll" "$dll"; else rm -f "$dll"; fi
	rm -rf "$before"
	echo "linux-walkers: off - Plugins.txt $(md5sum "$plugins_txt" | cut -c1-8), DLL $([[ -f "$dll" ]] && echo present || echo absent)"
	;;
*)
	echo "usage: $0 on|off" >&2
	exit 2
	;;
esac
