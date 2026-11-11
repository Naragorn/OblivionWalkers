#!/usr/bin/env bash
# Runs harness/horde-stuck-farm.txt twice on OBVR's harness - [Horde]
# MarkerOnGround=0 (the marker at the leader's height, as before 2026-11-11)
# and =1 (on the land) - and prints, per run, how far each group's leader
# got and how high its marker hung over the land. The plugin INI is put back
# whatever happens. Announce "busy" to the other sessions first.
set -euo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
obvr="${OBVR_DIR:-$root/../OBVR}"
game="${WALKERS_LINUX_GAME:-$HOME/.local/share/Steam/steamapps/common/Oblivion}"
log="$game/OblivionWalkers.log"
ini="$game/Data/OBSE/Plugins/OblivionWalkers.ini"
backup="$(mktemp)"
cp "$ini" "$backup"
trap 'cp "$backup" "$ini"; rm -f "$backup"' EXIT
for on in 0 1; do
	cp "$backup" "$ini"
	python3 - "$ini" "$on" <<'INI'
import re, sys
path, on = sys.argv[1], sys.argv[2]
text = open(path, encoding="latin-1", newline="").read()
head = re.search(r"^\[Horde\]\s*$", text, re.M)
if head is None:
    sys.exit("no [Horde] section in " + path)
end = re.search(r"^\[", text[head.end():], re.M)
stop = head.end() + (end.start() if end else len(text) - head.end())
section = re.sub(r"^MarkerOnGround=.*\r?\n", "", text[head.end():stop], flags=re.M)
section = "\r\nMarkerOnGround=" + on + section
open(path, "w", encoding="latin-1", newline="").write(text[:head.end()] + section + text[stop:])
INI
	(cd "$obvr" && python3 tools/hand-script-run.py "$root/harness/horde-stuck-farm.txt" --headless \
		--artifact-root "$root/artifacts/hand-script") | tee "$root/artifacts/horde-stuck-farm-runner-$on.txt"
	run="$(ls -td "$root"/artifacts/hand-script/horde-stuck-farm/*/ | head -1)"
	cp "$log" "$run/OblivionWalkers.log"
	echo "== MarkerOnGround=$on - $run"
	python3 - "$run/OblivionWalkers.log" <<'PY'
import re, sys, math
pat = re.compile(r"^Horde: group (\d+) - leader (\w+) at (-?\d+) (-?\d+) (-?\d+) \(marker z (-?\d+), land there (unknown )?(-?\d+)\)")
first, last, gaps = {}, {}, {}
for l in open(sys.argv[1], encoding="latin-1"):
    m = pat.match(l)
    if not m:
        continue
    key = (m.group(1), m.group(2))
    p = (float(m.group(3)), float(m.group(4)))
    first.setdefault(key, p)
    last[key] = p
    if not m.group(7):
        gaps.setdefault(key, []).append(float(m.group(6)) - float(m.group(8)))
for key in sorted(first):
    a, b = first[key], last[key]
    g = gaps.get(key, [])
    print("  group %s leader %s: moved %.0f units; marker over the land %s" % (
        key[0], key[1], math.hypot(b[0] - a[0], b[1] - a[1]),
        ("%.0f..%.0f" % (min(g), max(g))) if g else "unknown"))
PY
done
