#!/usr/bin/env bash
# Runs harness/noise-draws.txt on OBVR's harness (the Linux game, headless)
# and prints the horde's and the noise's lines of OblivionWalkers.log. The
# live INI is left as it is: [Noise] takes its defaults when the section is
# missing (Enabled=1, Radius=1200, SearchSeconds=30).
set -euo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
obvr="${OBVR_DIR:-$root/../OBVR}"
game="${WALKERS_LINUX_GAME:-$HOME/.local/share/Steam/steamapps/common/Oblivion}"
(cd "$obvr" && python3 tools/hand-script-run.py "$root/harness/noise-draws.txt" --headless \
	--artifact-root "$root/artifacts/hand-script")
grep -E '^(Horde: (noise|group [0-9]+ (at the noise|done searching|.s marker to))|Hit: |Cast watch|Noise:)' \
	"$game/OblivionWalkers.log" || echo "no noise line"
