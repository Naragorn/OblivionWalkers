#!/usr/bin/env bash
# Runs harness/dungeon-questrefs.txt: the dungeon pool before, a radiant
# quest's reference (VampTown71) enabled and disabled by console. With
# QUESTREFS_SITE="<name>": [Dungeons] TestSite=<name> (a refuge), which must be
# given up once the quest's reference there is enabled. The INI is put back
# whatever happens. Needs tools/linux-walkers.sh on; announce "busy" first.
set -euo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
obvr="${OBVR_DIR:-$root/../OBVR}"
game="${WALKERS_LINUX_GAME:-$HOME/.local/share/Steam/steamapps/common/Oblivion}"
ini="$game/Data/OBSE/Plugins/OblivionWalkers.ini"
site="${QUESTREFS_SITE:-}"
backup="$(mktemp)"
cp "$ini" "$backup"
trap 'cp "$backup" "$ini"; rm -f "$backup"' EXIT
source "$root/tools/walkers-ini-mode.sh"
walkers_apocalypse "$ini"  # 4.24: these runs need the apocalypse
python3 - "$ini" "$site" <<'INI'
import re, sys
path, site = sys.argv[1], sys.argv[2]
text = open(path, encoding="latin-1", newline="").read()
nl = "\r\n" if "\r\n" in text else "\n"
def put(text, section, key, value):
    head = re.search(r"^\[" + section + r"\]\s*$", text, re.M)
    if head is None:
        return text.rstrip("\r\n") + nl + nl + "[" + section + "]" + nl + key + "=" + value + nl
    end = re.search(r"^\[", text[head.end():], re.M)
    stop = head.end() + (end.start() if end else len(text) - head.end())
    body = text[head.end():stop]
    if re.search(r"^" + key + r"=", body, re.M):
        body = re.sub(r"^" + key + r"=.*$", key + "=" + value, body, count=1, flags=re.M)
    else:
        body = nl + key + "=" + value + body
    return text[:head.end()] + body + text[stop:]
for key, value in (("Enabled", "1"), ("TestSite", site), ("TestSiteState", "1"), ("MessengerHours", "0"),
                   ("Journal", "0"), ("ActiveMin", "1"), ("ActiveMax", "1")):
    text = put(text, "Dungeons", key, value)
open(path, "w", encoding="latin-1", newline="").write(text)
INI
(cd "$obvr" && python3 tools/hand-script-run.py "$root/harness/dungeon-questrefs.txt" --headless ${WALKERS_RUNNER_ARGS---no-steamvr} \
	--artifact-root "$root/artifacts/hand-script") > "$root/artifacts/dungeon-questrefs-runner.txt" 2>&1 || true
run="$(ls -td "$root"/artifacts/hand-script/dungeon-questrefs/*/ | head -1)"
cp "$game/OblivionWalkers.log" "$run/OblivionWalkers.log"
echo "run: $run"
python3 - "$run/OblivionWalkers.log" "$site" <<'PY'
import re, sys
lines = open(sys.argv[1], encoding="latin-1").read().splitlines()
site = sys.argv[2]
failures = 0
def check(ok, what):
    global failures
    print(("  ok    " if ok else "  FAIL  ") + what)
    failures += 0 if ok else 1
pool = [l for l in lines if re.match(r"Dungeons: \d+ candidates, \d+ in the pool", l)]
for l in pool[:2]:
    print("        " + l[:200])
# the first look is at the main menu (the file's flags); the save's flags come with the load - the
# fewest out by a quest is the game's own state before the scenario enables anything
outs = [int(m.group(2)) for m in (re.search(r"(\d+) in the pool .* (\d+) out by a loaded quest", l) for l in pool) if m]
check(bool(outs) and min(outs) < 40,
      "the radiant quests' waiting places in the pool: %s out by a quest at the fewest (80 before, 2026-11-17)" % (min(outs) if outs else "-"))
out = [l for l in lines[lines.index(pool[0]) + 1:] if " out - a loaded quest" in l and "VampireRadiantQuest" in l] if pool else []
back = [l for l in lines if "back in the pool - no quest's reference" in l]
for l in out[:2] + back[:2]:
    print("        " + l[:220])
check(bool(out), "VampTown71 enabled: its place out of the pool (VampireRadiantQuest)")
name = re.match(r"Dungeons: (.+?) \(", out[0]).group(1) if out else "-"
check(any(name in l for l in back), "disabled again: %s back in the pool" % name)
if site:
    given = [l for l in lines if "given up - a quest uses it now" in l]
    for l in given[:2]:
        print("        " + l[:200])
    check(any(site in l for l in given), "the active site at %s given up once the quest uses it" % site)
print("dungeon-questrefs: %s" % ("PASS" if failures == 0 else "FAIL (%d)" % failures))
sys.exit(1 if failures else 0)
PY
