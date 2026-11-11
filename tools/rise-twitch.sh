#!/usr/bin/env bash
# Runs harness/rise-twitch.txt on OBVR's harness and checks
# OblivionWalkers.log - the runner itself reads OBVR.log only. Exit 0: every
# check held. Announce "busy" to the other sessions first.
set -euo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
obvr="${OBVR_DIR:-$root/../OBVR}"
game="${WALKERS_LINUX_GAME:-$HOME/.local/share/Steam/steamapps/common/Oblivion}"
log="$game/OblivionWalkers.log"
ini="$game/Data/OBSE/Plugins/OblivionWalkers.ini"
# The spawn replacement off for the run (put back whatever happens): with it
# dozens of walkers stand about, and the harness's {actor} - the actor
# nearest the player - after a push is often another one.
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
grep -n -A3 "^\[Replace\]" "$ini" | head -3
(cd "$obvr" && python3 tools/hand-script-run.py "$root/harness/rise-twitch.txt" --headless \
	--artifact-root "$root/artifacts/hand-script") | tee "$root/artifacts/rise-twitch-runner.txt"
run="$(ls -td "$root"/artifacts/hand-script/rise-twitch/*/ | head -1)"
cp "$log" "$run/OblivionWalkers.log"
python3 - "$run/OblivionWalkers.log" <<'PY'
import sys
lines = open(sys.argv[1], encoding="latin-1").read().splitlines()
failures = 0

def check(ok, what):
    global failures
    print(("  ok    " if ok else "  FAIL  ") + what)
    failures += 0 if ok else 1

def count(text):
    return sum(1 for l in lines if text in l)

deaths = [l for l in lines if l.startswith("End: walker") and "gets up again" in l]
raises = [l for l in lines if l.startswith("Rise: ") and " gets up at " in l]
stuck = [l for l in lines if "still on its spot" in l]
check(len(deaths) >= 4, "four walkers down, each to get up again (%d)" % len(deaths))
check(len(raises) >= 4, "each raised (%d)" % len(raises))
for l in deaths + raises:
    print("        " + l[:200])
npc = [l for l in lines if l.startswith("End: walker") and "killer 0002C0B9" in l]
check(any("felled by someone else" in l for l in npc),
      "e: the walker an NPC killed ends (%s)" % (npc[0][:160] if npc else "no line"))
npc_ids = {l[12:20] for l in npc}
check(not any(l[6:14] in npc_ids for l in raises), "e: and is not raised")
check(not stuck, "none still on its spot 20 s after its raise (%d)" % len(stuck))
for l in stuck:
    print("        " + l)
sys.exit(1 if failures else 0)
PY
