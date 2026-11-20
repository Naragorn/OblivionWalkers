#!/usr/bin/env bash
# Runs a necromancers' scenario (docs/walkers-spec.md 4.23 B) with the
# switches set for the run, the INI put back whatever happens (md5 checked);
# checks OblivionWalkers.log and the roster in OBVR.log. Needs
# tools/linux-walkers.sh on; announce "busy" first.
#
#   tools/necro-harness.sh hotspot   harness/necro-hotspot.txt: the necromancers alone, one boss at
#                                    Toadstool Hollow - he rises, his walkers and he are placed when the
#                                    player is inside, killall: he falls, the realm stops, a refuge won
#   tools/necro-harness.sh band      harness/necro-band.txt: one boss at Toadstool Hollow, the player before
#                                    its door, the clock fast - a band of his comes into the world near him
#   tools/necro-harness.sh teams     harness/necro-teams.txt: two bands of enemy teams near the player - set
#                                    on each other by the plugin
#   tools/necro-harness.sh rise      harness/necro-rise.txt: a bandit killed before a necromancer's door
#                                    waits to rise, one killed far off stays down
set -euo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
obvr="${OBVR_DIR:-$root/../OBVR}"
game="${WALKERS_LINUX_GAME:-$HOME/.local/share/Steam/steamapps/common/Oblivion}"
ini="$game/Data/OBSE/Plugins/OblivionWalkers.ini"
scenario="${1:-hotspot}"
case "$scenario" in
	hotspot | band | teams | rise) ;;
	*) echo "scenario: hotspot | band | teams | rise" >&2; exit 2 ;;
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
if scenario == "band":
    keys += [("Necromancers", "BossesMin", "1"), ("Necromancers", "BossesMax", "1"),
             ("Necromancers", "BandsPerDay", "4"), ("Necromancers", "BandSizeMin", "3"),
             ("Necromancers", "BandSizeMax", "3")]
if scenario == "teams":
    keys += [("Necromancers", "TestHome", ""), ("Necromancers", "TestBandsSeconds", "3")]
else:
    keys += [("Necromancers", "TestBandsSeconds", "0")]
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
if scenario == "rise":
    check(bool(show(find(r"Necromancers: boss 0 \(team 1\) rose at " + place))), "boss 0 rose at Toadstool Hollow")
    verdicts = [l for l in log if "00069AD1" in l and ("waits to rise" in l or "stays down" in l)]
    verdicts = verdicts or [l for l in log if ("waits to rise" in l or "stays down (no apocalypse" in l)]
    show(verdicts, 6)
    check(len(verdicts) >= 2 and "waits to rise" in verdicts[0], "the bandit before the necromancer's door waits to rise")
    check(len(verdicts) >= 2 and "only near the necromancers" in verdicts[1], "the one far off stays down - only near them")
    print("necro %s: %s" % (scenario, "PASS" if failures == 0 else "FAIL (%d)" % failures))
    sys.exit(1 if failures else 0)
if scenario == "teams":
    check(bool(show(find(r"Necromancers: test - TestBandsSeconds=3: two bands of 3, teams (\d+) and (\d+)"))),
          "the test's two bands queued")
    came = show(find(r"Bands: a band of 3 walkers \(a necromancer's, team (\d+)\) comes near"))
    teams = {re.search(r"team (\d+)", l).group(1) for l in came}
    check(len(teams) == 2, "both placed, two teams (%s)" % sorted(teams))
    looks = show(find(r"Necromancers: teams - \d+ of theirs near; \d+ told StopCombat .*, (\d+) set on .*; (\d+) fight an enemy team's"), 3)
    fights = [(int(re.search(r"(\d+) set on", l).group(1)), int(re.search(r"(\d+) fight an enemy team's", l).group(1))) for l in looks]
    # set on by the plugin, or at it on their own (measured 2026-11-20: the engine had them fight at once)
    check(any(s >= 1 or e >= 2 for s, e in fights), "the two teams fight each other (%s)" % fights[:4])
    dead = sum(int(n) for n in re.findall(r"2B000[89A][0-9A-F]{2} x\d+ \(dead (\d+)\)", roster.get("fought", "")))
    near = [int(re.search(r"teams - (\d+) of theirs near", l).group(1)) for l in looks]
    # the dead lie in the roster or are gone from it (the bodies taken away)
    check(dead >= 1 or (near and max(near) - near[-1] >= 1),
          "walkers dead of it (dead at 'fought' %d; of theirs near %s)" % (dead, near))
    def walkers_at(mark):
        bases = re.findall(r"\b([0-9A-F]{8}) x(\d+)", roster.get(mark, ""))
        return sum(int(n) for b, n in bases if re.match(r"2B000[89A][0-9A-F]{2}$", b))
    print("  roster met:    " + roster.get("met", "")[:200])
    print("  roster fought: " + roster.get("fought", "")[:200])
    print("  walkers at met %d, at fought %d" % (walkers_at("met"), walkers_at("fought")))
    print("necro %s: %s" % (scenario, "PASS" if failures == 0 else "FAIL (%d)" % failures))
    sys.exit(1 if failures else 0)
if scenario == "band":
    rose = show(find(r"Necromancers: boss 0 \(team 1\) rose at " + place))
    check(bool(rose), "boss 0 rose at his home")
    near = show(find(r"Necromancers: a band of 3 walkers \(boss 0, team 1\) comes near you - into the world"))
    check(bool(near), "a band of his formed near the player: into the world")
    placed = show(find(r"Bands: a band of 3 walkers \(a necromancer's, team 1\) comes near"))
    check(bool(placed), "the bands' placing takes it: the walkers' list, his team")
    found = show(find(r"Bands: (\d+) new members found at the marker"))
    check(any(int(re.search(r"(\d+) new members", l).group(1)) >= 1 for l in found), "its walkers found at the marker")
    check(not any("the band's leader" in l for l in log), "no leader (a necromancer's band)")
    bases = re.findall(r"\b([0-9A-F]{8}) x(\d+)", roster.get("counted", ""))
    walkers = sum(int(n) for b, n in bases if re.match(r"2B000[89A][0-9A-F]{2}$", b))
    print("  roster at counted: %s" % roster.get("counted", "")[:240])
    check(walkers >= 3, "the roster at 'counted' holds the walkers (%d)" % walkers)
    print("necro %s: %s" % (scenario, "PASS" if failures == 0 else "FAIL (%d)" % failures))
    sys.exit(1 if failures else 0)
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
