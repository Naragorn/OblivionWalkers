#!/usr/bin/env bash
# Runs harness/headshots.txt on OBVR's harness and checks OblivionWalkers.log
# (4.17b). Exit 0: every deterministic check held; the arrows' case (d) is
# reported, not judged. Announce "busy" to the other sessions first.
set -euo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
obvr="${OBVR_DIR:-$root/../OBVR}"
game="${WALKERS_LINUX_GAME:-$HOME/.local/share/Steam/steamapps/common/Oblivion}"
log="$game/OblivionWalkers.log"
ini="$game/Data/OBSE/Plugins/OblivionWalkers.ini"
# The spawn replacement off for the run (put back whatever happens), as in
# rise-twitch.sh: {actor} must be the walker just placed.
backup="$(mktemp)"
cp "$ini" "$backup"
trap 'cp "$backup" "$ini"; rm -f "$backup"' EXIT
python3 - "$ini" <<'INI'
import re, sys
path = sys.argv[1]
text = open(path, encoding="latin-1", newline="").read()
head = re.search(r"^\[Replace\]\s*$", text, re.M)
if head is None:
    sys.exit("no [Replace] section in " + path)
end = re.search(r"^\[", text[head.end():], re.M)
stop = head.end() + (end.start() if end else len(text) - head.end())
section = re.sub(r"^Enabled=1", "Enabled=0", text[head.end():stop], count=1, flags=re.M)
open(path, "w", encoding="latin-1", newline="").write(text[:head.end()] + section + text[stop:])
INI
mkdir -p "$root/artifacts"
(cd "$obvr" && python3 tools/hand-script-run.py "$root/harness/headshots.txt" --headless \
	--artifact-root "$root/artifacts/hand-script") | tee "$root/artifacts/headshots-runner.txt"
run="$(ls -td "$root"/artifacts/hand-script/headshots/*/ | head -1)"
cp "$log" "$run/OblivionWalkers.log"
python3 - "$run/OblivionWalkers.log" <<'PY'
import sys
lines = open(sys.argv[1], encoding="latin-1").read().splitlines()
failures = 0

def check(ok, what):
    global failures
    print(("  ok    " if ok else "  FAIL  ") + what)
    failures += 0 if ok else 1

def show(prefix):
    found = [l for l in lines if prefix in l]
    for l in found[:12]:
        print("        " + l[:220])
    return found

print("Watch:")
watch = show("Headshot watch:")
check(bool(watch) and "magic on, damage on" in watch[-1], "both events registered")
print("a burning:")
burnt = show("dies burning")
check(len(burnt) >= 1, "a walker hit by fire and killed dies burning (%d)" % len(burnt))
print("b overkill / bolt:")
over = show("a spell took")
bolts = show("Headshot: spell projectile")
check(len(over) >= 1 or len(burnt) >= 2, "the 110 fire bolt ends its walker (overkill %d, burning %d)" % (len(over), len(burnt)))
print("c control:")
up = [l for l in lines if l.startswith("End: walker") and "gets up again" in l]
for l in up[:4]:
    print("        " + l[:200])
check(len(up) >= 1, "the walker killed without fire gets up again (%d)" % len(up))
print("arrows (reported, not judged):")
arrows = show("Headshot: arrow")
hits = [l for l in lines if l.startswith("Hit: ")]
print("        %d arrow judgements, %d hits" % (len(arrows), len(hits)))
print("Corpses:")
show("Corpses:")
sys.exit(1 if failures else 0)
PY
