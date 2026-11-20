#!/usr/bin/env bash
# Runs harness/dungeon-settle.txt (docs/walkers-spec.md 7a, 2026-11-20): a
# refuge in Fort Horunn saved, a quest's reference there enabled (the refuge
# given up), the save loaded - the refuge must not be given up after the load
# before the pool has settled. The INI is put back whatever happens. Needs
# tools/linux-walkers.sh on; announce "busy" first.
#
#   tools/dungeon-settle.sh          the check as built (the fix: PASS)
#   tools/dungeon-settle.sh --red    the same run, expecting the old fault
#                                    (a build from before 9bb9acf deployed)
set -euo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
obvr="${OBVR_DIR:-$root/../OBVR}"
game="${WALKERS_LINUX_GAME:-$HOME/.local/share/Steam/steamapps/common/Oblivion}"
ini="$game/Data/OBSE/Plugins/OblivionWalkers.ini"
red=0
[[ "${1:-}" == "--red" ]] && red=1
backup="$(mktemp)"
cp "$ini" "$backup"
sum="$(md5sum "$ini" | cut -c1-32)"
restore() {
	cp "$backup" "$ini"
	rm -f "$backup"
	[[ "$(md5sum "$ini" | cut -c1-32)" == "$sum" ]] && echo "dungeon-settle: INI restored (md5 $sum)" ||
		echo "dungeon-settle: INI NOT restored" >&2
}
trap restore EXIT
source "$root/tools/walkers-ini-mode.sh"
walkers_apocalypse "$ini"  # 4.24: the held dungeons need a side
python3 - "$ini" <<'INI'
import re, sys
path = sys.argv[1]
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
for key, value in (("Enabled", "1"), ("TestSite", "Fort Horunn"), ("TestSiteState", "1"), ("MessengerHours", "0"),
                   ("Journal", "0"), ("ActiveMin", "1"), ("ActiveMax", "1")):
    text = put(text, "Dungeons", key, value)
open(path, "w", encoding="latin-1", newline="").write(text)
INI
(cd "$obvr" && python3 tools/hand-script-run.py "$root/harness/dungeon-settle.txt" --headless ${WALKERS_RUNNER_ARGS---no-steamvr} \
	--artifact-root "$root/artifacts/hand-script") > "$root/artifacts/dungeon-settle-runner.txt" 2>&1 || true
tail -1 "$root/artifacts/dungeon-settle-runner.txt"
run="$(ls -td "$root"/artifacts/hand-script/dungeon-settle/*/ | head -1)"
cp "$game/OblivionWalkers.log" "$run/OblivionWalkers.log"
echo "run: $run"
python3 - "$run/OblivionWalkers.log" "$red" <<'PY'
import sys
lines = open(sys.argv[1], encoding="latin-1").read().splitlines()
red = sys.argv[2] == "1"
failures = 0
def check(ok, what):
    global failures
    print(("  ok    " if ok else "  FAIL  ") + what)
    failures += 0 if ok else 1
# a load: LINK's first look after it (the harness's own from the main menu, then the scenario's)
loads = [i for i, l in enumerate(lines) if l.startswith("LINK: first look") and "after the load" in l]
print("  loads at log lines %s" % [i + 1 for i in loads])
made = [i for i, l in enumerate(lines) if "TestSite=Fort Horunn made the first site" in l]
gone = [i for i, l in enumerate(lines) if l.startswith("Dungeons: Fort Horunn given up - a quest uses it now")]
settled = [i for i, l in enumerate(lines) if l.startswith("Dungeons: the pool settled")]
for i in made[:3] + gone[:4] + settled[:3]:
    print("  log   %d: %s" % (i + 1, lines[i][:180]))
check(len(loads) >= 2, "two loads at least: the run's start and the scenario's")
reload = loads[-1] if len(loads) >= 2 else len(lines)  # the scenario's: the last
check(any(i < reload for i in made), "the refuge stood before the scenario's load")
check(any(i < reload for i in gone), "the quest's reference enabled: the refuge given up before the load")
after = [i for i in gone if i > reload]
if red:
    check(bool(after), "RED: the refuge given up again right after the load (the pool from before it)")
else:
    check(not after, "no refuge given up after the load")
    later = [i for i in settled if i > reload]
    check(bool(later), "the pool settled after the scenario's load")
    rolled = [i for i, l in enumerate(lines) if i > reload and " rolled - now " in l]
    check(not rolled or (later and rolled[0] > later[0]), "no place rolled before it settled")
print("dungeon-settle%s: %s" % (" (red)" if red else "", "PASS" if failures == 0 else "FAIL (%d)" % failures))
sys.exit(1 if failures else 0)
PY
