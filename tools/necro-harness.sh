#!/usr/bin/env bash
# Runs a necromancers' scenario (docs/walkers-spec.md 4.23 B) with the
# switches set for the run, the INI put back whatever happens (md5 checked);
# checks OblivionWalkers.log and the roster in OBVR.log. Needs
# tools/linux-walkers.sh on; announce "busy" first.
#
#   tools/necro-harness.sh hotspot   harness/necro-hotspot.txt: the necromancers alone, one boss at
#                                    Toadstool Hollow - he rises, his walkers and he are placed when the
#                                    player is inside, killall: he falls, the realm stops, a refuge won
set -euo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
obvr="${OBVR_DIR:-$root/../OBVR}"
game="${WALKERS_LINUX_GAME:-$HOME/.local/share/Steam/steamapps/common/Oblivion}"
ini="$game/Data/OBSE/Plugins/OblivionWalkers.ini"
scenario="${1:-hotspot}"
case "$scenario" in
	hotspot) ;;
	*) echo "scenario: hotspot" >&2; exit 2 ;;
esac
backup="$(mktemp)"
cp "$ini" "$backup"
sum="$(md5sum "$ini" | cut -c1-32)"
restore() {
	cp "$backup" "$ini"
	rm -f "$backup"
	[[ "$(md5sum "$ini" | cut -c1-32)" == "$sum" ]] && echo "necro-harness: INI restored (md5 $sum)" ||
		echo "necro-harness: INI NOT restored" >&2
}
trap restore EXIT
python3 - "$ini" "$scenario" <<'INI'
import re, sys
path, scenario = sys.argv[1], sys.argv[2]
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
keys = [
    ("Walkers", "Mode", "0"), ("Daedra", "Mode", "0"), ("Onboarding", "Show", "0"),
    ("Walkers", "DieOnlyBeheaded", "0"),
    ("Necromancers", "Mode", "1"), ("Necromancers", "TestHome", "Toadstool Hollow"),
    ("Necromancers", "WalkersMin", "4"), ("Necromancers", "WalkersMax", "4"),
    ("Necromancers", "BandsPerDay", "0"), ("Necromancers", "NewPlaceEveryDays", "0"),
    ("Necromancers", "ComeBack", "1"), ("Necromancers", "ComeBackDays", "3"),
    ("Dungeons", "Enabled", "1"), ("Dungeons", "TestSite", ""), ("Dungeons", "ActiveMin", "1"),
    ("Dungeons", "ActiveMax", "1"), ("Dungeons", "WalkersAtOnce", "6"), ("Dungeons", "WalkersOutsidePercent", "0"),
    ("Dungeons", "MessengerHours", "0"), ("Dungeons", "Journal", "1"), ("Dungeons", "Rumors", "1"),
]
for section, key, value in keys:
    text = put(text, section, key, value)
open(path, "w", encoding="latin-1", newline="").write(text)
INI
(cd "$obvr" && python3 tools/hand-script-run.py "$root/harness/necro-$scenario.txt" --headless ${WALKERS_RUNNER_ARGS---no-steamvr} \
	--artifact-root "$root/artifacts/hand-script") > "$root/artifacts/necro-$scenario-runner.txt" 2>&1 || true
tail -1 "$root/artifacts/necro-$scenario-runner.txt"
run="$(ls -td "$root"/artifacts/hand-script/necro-"$scenario"/*/ | head -1)"
cp "$game/OblivionWalkers.log" "$run/OblivionWalkers.log"
echo "run: $run"
python3 - "$run" "$scenario" <<'PY'
import re, sys
run, scenario = sys.argv[1], sys.argv[2]
log = open(run + "/OblivionWalkers.log", encoding="latin-1").read().splitlines()
obvr = open(run + "/OBVR.log", encoding="latin-1").read().splitlines()
failures = 0
def check(ok, what):
    global failures
    print(("  ok    " if ok else "  FAIL  ") + what)
    failures += 0 if ok else 1
def find(pattern):
    return [l for l in log if re.search(pattern, l)]
def show(lines, most=4):
    for l in lines[:most]:
        print("  log   " + l[:200])
    return lines
roster = {}
mark = None
for l in obvr:
    m = re.search(r"HandScript: mark (\w+)", l)
    if m:
        mark = m.group(1)
    if "HandScript: actors" in l and mark:
        roster[mark] = l
print("  roster " + roster.get("placed", "")[:300])
place = "Toadstool Hollow"
check(bool(show(find(r"^Mod: .*Necromancers Mode=1 \(their realms\)"))), "the start: Walkers 0, Daedra 0, Necromancers 1")
check(bool(show(find(r"Necromancers: test - TestHome=" + place + r": one boss there \(free\)"))), "TestHome free in the settled pool")
rose = show(find(r"Necromancers: boss 0 \(team 1\) rose at " + place + r" .* 4 walkers in it$"))
check(bool(rose), "boss 0 rose at his home, 4 walkers in it")
check(bool(show(find(r"the pool to site \d+ " + place))), "the pool's walkers sent to his place with the player inside")
asked = show(find(r"Dungeons: the necromancer asked for at " + place + r" \(0004B94[5-8]\)"))
check(bool(asked), "the necromancer (LL1NecromancerBoss100's) asked of the placer")
came = show(find(r"Dungeons: the necromancer \w+ came to " + place))
check(bool(came), "he came")
check(bool(show(find(r"the necromancer's chest set down at " + place + r" .*chest \w+\)"))), "his chest at his feet")
bases = re.findall(r"\b([0-9A-F]{8}) x\d+", roster.get("placed", ""))
check(any(re.match(r"0004B94[5-8]$", b) for b in bases), "the roster at 'placed' holds him (%s)" % [b for b in bases if b.startswith("0004B9")])
check(bool(show(find(place + r" - the necromancer fell"))), "killall: the necromancer fell")
check(bool(show(find(r"Necromancers: boss 0 \(team 1\) fell - his realm stops"))), "his realm stops")
check(bool(show(find(r"Necromancers: the last of them fell - a new set rises later"))), "the last of them: a new set later (ComeBack=1)")
check(bool(show(find(r"Dungeons: site \d+ " + place + r" \(cave\) taken - now refuge"))), "the place cleared and taken: a refuge")
check(bool(show(find(r"Necromancers: " + place + r" is no longer boss 0's - cleared, a refuge won"))), "the realm told: a refuge won")
print("necro %s: %s" % (scenario, "PASS" if failures == 0 else "FAIL (%d)" % failures))
sys.exit(1 if failures else 0)
PY
