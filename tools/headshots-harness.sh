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
# The spawn replacement and the dungeons off for the run (put back whatever
# happens), as in rise-twitch.sh: {actor} must be the walker just placed. The
# dungeons' runner (the ESP's A78, WalkersDungeonRunnerRef) otherwise comes
# with news and stands nearest the player - he was {actor} in the runs of
# 2026-11-14 19:20-19:28.
backup="$(mktemp)"
cp "$ini" "$backup"
trap 'cp "$backup" "$ini"; rm -f "$backup"' EXIT
python3 - "$ini" <<'INI'
import re, sys
path = sys.argv[1]
text = open(path, encoding="latin-1", newline="").read()
for name in ("Replace", "Dungeons"):
    head = re.search(r"^\[" + name + r"\]\s*$", text, re.M)
    if head is None:
        # an INI without the section runs on the default (on): it is added
        newline = "\r\n" if "\r\n" in text else "\n"
        text = text.rstrip("\r\n") + newline + newline + "[" + name + "]" + newline + "Enabled=0" + newline
        continue
    end = re.search(r"^\[", text[head.end():], re.M)
    stop = head.end() + (end.start() if end else len(text) - head.end())
    section = re.sub(r"^Enabled=1", "Enabled=0", text[head.end():stop], count=1, flags=re.M)
    text = text[:head.end()] + section + text[stop:]
open(path, "w", encoding="latin-1", newline="").write(text)
INI
mkdir -p "$root/artifacts"
(cd "$obvr" && python3 tools/hand-script-run.py "$root/harness/headshots.txt" --headless \
	--artifact-root "$root/artifacts/hand-script") | tee "$root/artifacts/headshots-runner.txt"
run="$(ls -td "$root"/artifacts/hand-script/headshots/*/ | head -1)"
cp "$log" "$run/OblivionWalkers.log"
python3 - "$run/OblivionWalkers.log" "$run/OBVR.log" <<'PY'
import re, sys
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

# The console lines aimed at {actor} must have hit a placed walker (FF...),
# not one of the plugin's people (25...) - else the case proves nothing.
obvr_log = open(sys.argv[2], encoding="latin-1").read().splitlines()
aimed = [re.search(r'console "([0-9A-F]{8})\.(setrestrained|kill)', l) for l in obvr_log]
aimed = [m.group(1) for m in aimed if m]
print("Aimed at: " + " ".join(aimed))
check(len(aimed) == 4 and all(a.startswith("FF") for a in aimed), "{actor} was a placed walker each time")
print("Magic hits (reported):")
show("Magic hit:")
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
