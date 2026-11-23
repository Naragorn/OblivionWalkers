#!/usr/bin/env bash
# Runs a necromancers' scenario (docs/walkers-spec.md 4.23 B) with the
# switches set for the run, the INI put back whatever happens (md5 checked);
# checks OblivionWalkers.log and the roster in OBVR.log. Needs
# tools/linux-walkers.sh on; announce "busy" first.
#
#   tools/necro-harness.sh hotspot   harness/necro-hotspot.txt: the necromancers alone, one boss at
#                                    Toadstool Hollow - he rises, his walkers and he are placed when the
#                                    player is inside, his walkers killed: he stays, killall: he falls,
#                                    the realm stops, a refuge won
#   tools/necro-harness.sh band      harness/necro-band.txt: one boss at Toadstool Hollow, the player before
#                                    its door, the clock fast - a band of his comes into the world near him
#   tools/necro-harness.sh teams     harness/necro-teams.txt: two bands of enemy teams near the player - set
#                                    on each other by the plugin; two enemy bands far off meet as numbers
#   tools/necro-harness.sh rise      harness/necro-rise.txt: a bandit killed before a necromancer's door
#                                    waits to rise, one killed far off stays down
#   tools/necro-harness.sh allies    harness/necro-allies.txt: two bands of one team, one set on its ally -
#                                    told to let go, all six alive
#   tools/necro-harness.sh daedra    harness/necro-daedra.txt: Daedra Mode 2, a Dremora and the necromancer
#                                    set on each other - both told to let go, both alive
#   tools/necro-harness.sh march     harness/necro-march.txt: the days pass - the realm grows, its bands
#                                    reach a city; saved and loaded, the realm comes back
#   tools/necro-harness.sh summon    harness/necro-summon.txt: a measurement - the player's own summoned
#                                    Dremora with Daedra Mode 2: who lets go of whom
#   tools/necro-harness.sh siege     harness/necro-siege.txt: two allied necromancers besiege Bruma, their
#                                    walkers before the gate carry their team; saved and loaded
set -euo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
obvr="${OBVR_DIR:-$root/../OBVR}"
game="${WALKERS_LINUX_GAME:-$HOME/.local/share/Steam/steamapps/common/Oblivion}"
ini="$game/Data/OBSE/Plugins/OblivionWalkers.ini"
scenario="${1:-hotspot}"
case "$scenario" in
	hotspot | band | teams | rise | allies | daedra | march | siege | summon) ;;
	*) echo "scenario: hotspot | band | teams | rise | allies | daedra | march | siege | summon" >&2; exit 2 ;;
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
import os, re, sys
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
    ("Necromancers", "Teams", "1,2,3,4,5"), ("Necromancers", "TestFight", ""),
    ("Necromancers", "BossesMin", "1"), ("Necromancers", "BossesMax", "1"), ("Necromancers", "SiegeJoinPercent", "50"),
    ("Dungeons", "Enabled", "1"), ("Dungeons", "TestSite", ""), ("Dungeons", "ActiveMin", "1"),
    ("Dungeons", "ActiveMax", "1"), ("Dungeons", "WalkersAtOnce", "6"), ("Dungeons", "WalkersOutsidePercent", "0"),
    ("Dungeons", "MessengerHours", "0"), ("Dungeons", "Journal", "1"), ("Dungeons", "Rumors", "1"),
    ("Debug", "PoolProbe", "1"),
]
if scenario == "band":
    keys += [("Necromancers", "BossesMin", "1"), ("Necromancers", "BossesMax", "1"),
             ("Necromancers", "BandsPerDay", "4"), ("Necromancers", "BandSizeMin", "3"),
             ("Necromancers", "BandSizeMax", "3")]
if scenario == "teams":
    keys += [("Necromancers", "TestHome", ""), ("Necromancers", "TestBandsSeconds", "3"),
             ("Necromancers", "TestFight", "far"), ("Necromancers", "BossesMin", "3"), ("Necromancers", "BossesMax", "5")]
elif scenario == "allies":
    keys += [("Necromancers", "TestHome", ""), ("Necromancers", "TestBandsSeconds", "3"),
             ("Necromancers", "TestFight", "ally"), ("Necromancers", "Teams", "1,1,1,1,1"),
             ("Necromancers", "BossesMin", "2"), ("Necromancers", "BossesMax", "2")]
else:
    keys += [("Necromancers", "TestBandsSeconds", "0")]
if scenario == "hotspot":
    keys += [("Necromancers", "TestFight", "walkers")]
if scenario == "daedra":
    # NECRO_NOFIGHT=1 (tools/necro-crash-ab.sh): the Dremora put down, not set on him - the control arm
    keys += [("Daedra", "Mode", "2"), ("Necromancers", "TestFight", "" if os.environ.get("NECRO_NOFIGHT") == "1" else "daedra"),
             ("Necromancers", "WalkersMin", "2"), ("Necromancers", "WalkersMax", "2")]
if scenario == "summon":
    # NECRO_DAEDRA=0: the same with Daedra Mode 0 - the control arm (the summon not the walkers' ally)
    keys += [("Daedra", "Mode", os.environ.get("NECRO_DAEDRA", "2")), ("Necromancers", "TestFight", "summon"),
             ("Debug", "CombatTrace", "1")]
if scenario == "siege":
    keys += [("Necromancers", "BossesMin", "2"), ("Necromancers", "BossesMax", "2"), ("Necromancers", "Teams", "1,1"),
             ("Necromancers", "BandsPerDay", "4"), ("Necromancers", "BandSizeMin", "3"), ("Necromancers", "BandSizeMax", "3"),
             ("Necromancers", "SiegeJoinPercent", "100"),
             ("Siege", "Enabled", "1"), ("Siege", "ChancePerDayPercent", "0"), ("Siege", "WalkersMin", "40"),
             ("Siege", "WalkersMax", "40"), ("Siege", "WalkersAtGate", "12"),
             ("Daedra", "BandSiegePercent", "50"), ("Daedra", "BandSiegeFadePerDay", "0")]
if scenario == "march":
    keys += [("Necromancers", "BossesMin", "1"), ("Necromancers", "BossesMax", "1"),
             ("Necromancers", "NewPlaceEveryDays", "1"), ("Necromancers", "BandsPerDay", "4"),
             ("Necromancers", "BandSizeMin", "3"), ("Necromancers", "BandSizeMax", "3"),
             ("Siege", "Enabled", "1"), ("Siege", "ChancePerDayPercent", "0"),
             ("Daedra", "BandSiegePercent", "50"), ("Daedra", "BandSiegeFadePerDay", "0")]
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
    seton = show(find(r"Necromancers: [0-9A-F]{8} \(team (\d+)\) set on [0-9A-F]{8} \(team (\d+)"), 3)
    check(any(re.search(r"team (\d+)\) set on [0-9A-F]{8} \(team (\d+)", l).group(1) !=
              re.search(r"team (\d+)\) set on [0-9A-F]{8} \(team (\d+)", l).group(2) for l in seton),
          "the plugin sets a walker on an enemy team's (%d lines)" % len(seton))
    print("  info  allies told to let go: %d" % len(find(r"told to let go of [0-9A-F]{8} \(an ally")))
    check(bool(show(find(r"Necromancers: test - TestFight=far: two bands of 4, teams 1 and 2"))), "the far test's two bands formed")
    check(bool(show(find(r"Necromancers: a band of \d+ walkers \(boss \d+, team \d+\) (met an enemy band|destroyed by an enemy band)"))),
          "far off they meet as numbers")
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
def end(scenario):
    print("necro %s: %s" % (scenario, "PASS" if failures == 0 else "FAIL (%d)" % failures))
    sys.exit(1 if failures else 0)
def walkers_dead(mark):
    return sum(int(n) for b, n in re.findall(r"\b(2B000[89A][0-9A-F]{2}) x\d+ \(dead (\d+)\)", roster.get(mark, "")))
if scenario == "allies":
    check(bool(show(find(r"Necromancers: test - TestBandsSeconds=3: two bands of 3, teams 1 and 1"))), "two bands of team 1 queued")
    came = show(find(r"Bands: a band of 3 walkers \(a necromancer's, team 1\) comes near"))
    check(len(came) == 2, "both placed, team 1 (%d)" % len(came))
    check(not find(r"\(team \d+\) set on [0-9A-F]{8} \(team"), "the rule sets no ally on an ally")
    test = show(find(r"Necromancers: test - TestFight=ally: ([0-9A-F]{8}) \(team 1\) set on its ally ([0-9A-F]{8})"))
    check(bool(test), "the test sets one on its ally")
    lets = find(r"told to let go of [0-9A-F]{8} \(an ally of its team\)")
    show(lets, 6)
    check(bool(lets), "told to let go of its ally (%d lines)" % len(lets))
    looks = find(r"Necromancers: teams - (\d+) of theirs near")
    near = [int(re.search(r"teams - (\d+) of", l).group(1)) for l in looks]
    show(looks[-2:], 2)
    print("  roster fought: " + roster.get("fought", "")[:240])
    check(walkers_dead("fought") == 0 and near and near[-1] == 6, "all six alive at the end (dead %d; near %s)" % (walkers_dead("fought"), near[-6:]))
    end(scenario)
if scenario == "daedra":
    check(bool(show(find(r"^Mod: .*Daedra Mode=2"))), "Daedra Mode 2 (allies)")
    check(bool(show(find(r"Necromancers: boss 0 \(team 1\) rose at " + place))), "boss 0 rose at Toadstool Hollow")
    check(bool(show(find(r"Dungeons: the necromancer \w+ came to " + place))), "the necromancer came")
    brought = show(find(r"Necromancers: test - TestFight=daedra: the Daedra ([0-9A-F]{8}) .* brought to the necromancer"))
    check(bool(brought), "a Daedra near brought to him")
    check(bool(show(find(r"Necromancers: test - TestFight=daedra: the Daedra [0-9A-F]{8} and the necromancer [0-9A-F]{8} set on each other"))),
          "the two set on each other")
    his = find(r"Necromancers: [0-9A-F]{8} \(the necromancer of team 1\) told to let go of [0-9A-F]{8} \(a Daedra")
    theirs = find(r"Necromancers: the Daedra [0-9A-F]{8} told to let go of the necromancer")
    show(his, 3)
    show(theirs, 3)
    check(bool(his), "the necromancer told to let go of the Daedra (%d)" % len(his))
    check(bool(theirs), "the Daedra told to let go of the necromancer (%d)" % len(theirs))
    check(not find(place + r" - the necromancer fell"), "the necromancer did not fall")
    after = roster.get("after", "")
    print("  roster after: " + after[:300])
    boss = re.findall(r"\b(0004B94[5-8]) x\d+(?: \(dead (\d+)\))?", after)
    check(bool(boss) and all(not d or d == "0" for _, d in boss), "the necromancer in the roster, alive (%s)" % boss)
    # the Dremora: the base at 'fight' that was not there at 'placed' (the list picks it)
    new = set(re.findall(r"\b([0-9A-F]{8}) x\d+", roster.get("fight", ""))) - set(re.findall(r"\b([0-9A-F]{8}) x\d+", roster.get("placed", "")))
    dremora = [(b, d) for b, d in re.findall(r"\b([0-9A-F]{8}) x\d+(?: \(dead (\d+)\))?", after) if b in new]
    check(bool(dremora) and all(not d or d == "0" for _, d in dremora), "the Dremora (%s) in the roster, alive (%s)" % (sorted(new), dremora))
    walkers_after = re.findall(r"\b(2B000[89A][0-9A-F]{2}) x\d+(?: \(dead (\d+)\))?", after)
    check(all(not d or d == "0" for _, d in walkers_after), "his walkers alive - Daedra 2 makes them the Daedra's allies (%s)" % walkers_after)
    hits = [l for l in log if re.match(r"Hit: ", l)]
    print("  hits logged: %d" % len(hits))
    end(scenario)
if scenario == "summon":
    check(bool(show(find(r"^Mod: .*Daedra Mode=2"))), "Daedra Mode 2 (allies)")
    check(bool(show(find(r"Dungeons: the necromancer \w+ came to " + place))), "the necromancer came")
    new = set(re.findall(r"\b([0-9A-F]{8}) x\d+", roster.get("summoned", ""))) - set(re.findall(r"\b([0-9A-F]{8}) x\d+", roster.get("placed", "")))
    print("  roster summoned: " + roster.get("summoned", "")[:240])
    print("  roster fought:   " + roster.get("fought", "")[:240])
    check(bool(new), "a new actor after the cast - the summoned Dremora (%s)" % sorted(new))
    moved = show(find(r"TestFight=summon: the player and his summon [0-9A-F]{8}"))
    check(bool(moved), "the player and his summon brought to the necromancer")
    sid = re.search(r"his summon ([0-9A-F]{8})", moved[0]).group(1) if moved else "--------"
    at = [i for i, l in enumerate(log) if "TestFight=summon" in l]
    after_move = log[at[0]:] if at else []
    by_it = [l for l in after_move if re.match(r"Combat: %s " % sid, l)]
    at_it = [l for l in after_move if re.match(r"Combat: (?!%s)\S+ .* now fights %s" % (sid, sid), l)]
    print("  info  the summon's own targets after the move: %d lines" % len(by_it))
    show(by_it, 8)
    print("  info  others who took the summon as target: %d lines" % len(at_it))
    show(at_it, 8)
    hit_it = [l for l in after_move if re.match(r"Hit: ", l) and sid in l]
    print("  info  hits with the summon: %d" % len(hit_it))
    show(hit_it, 8)
    # the fix (SummonLogic.h): the player's summon is no Daedra ally - nobody told to let go of it, it fought back
    let_go = [l for l in after_move if re.search(r"told to let go of %s|the Daedra %s told to let go" % (sid, sid), l)]
    check(not let_go, "nobody told to let go of the player's summon, nor it of the necromancer (%d lines)" % len(let_go))
    check(bool([l for l in hit_it if re.match(r"Hit: %s hit by" % sid, l)]), "the summon struck back at: hit by the walkers or the necromancer")
    w = find(r"Daedra: \d+ walkers fighting a Daedra told StopCombat")
    n1 = find(r"\(the necromancer of team \d+\) told to let go of [0-9A-F]{8} \(a Daedra")
    n2 = find(r"Necromancers: the Daedra [0-9A-F]{8} told to let go of the necromancer")
    hits = [l for l in log if re.match(r"Hit: ", l)]
    print("  info  walkers told to let go of a Daedra: %d lines; the necromancer of a Daedra: %d; a Daedra of the necromancer: %d" % (len(w), len(n1), len(n2)))
    show(w, 2); show(n1, 2); show(n2, 2)
    print("  info  hits logged: %d" % len(hits))
    for l in hits[:12]:
        print("  hit   " + l[:100])
    end(scenario)
if scenario == "siege":
    saved_at = max([i for i, l in enumerate(log) if re.match(r"Co-save: .* written", l)] or [len(log)])
    load_at = min([i for i, l in enumerate(log) if i > saved_at and re.match(r"Co-save: .* read", l)] or [len(log)])
    before, after = log[:load_at], log[load_at:]
    def findin(lines, pattern):
        return [l for l in lines if re.search(pattern, l)]
    check(bool(show(findin(before, r"Necromancers: boss 0 \(team 1\) rose at " + place))), "boss 0 rose at Toadstool Hollow")
    check(bool(show(findin(before, r"Necromancers: boss 1 \(team 1\) rose at "))), "boss 1, his ally (team 1), rose too")
    show(findin(before, r"siege points at Bruma"), 3)
    sieges = show(findin(before, r"Necromancers: team 1 besieges Bruma (East|North) Gate - boss 0, boss 1 \(2 of them\)$"), 2)
    check(bool(sieges), "team 1 besieges Bruma: boss 0, whose bands marched, and boss 1, who joined")
    begun = show(findin(before, r"Siege: Bruma (East|North) Gate besieged - 80 walkers of the necromancers' team 1 \(2 of them\)"), 2)
    check(bool(begun), "the siege: 40 walkers for each of the two")
    out_at = min([i for i, l in enumerate(before) if re.search(r"Siege: the player at Bruma East Gate - its walkers come out", l)] or [len(before)])
    check(out_at < len(before), "the player at the gate: their walkers come out")
    looks = [int(re.search(r"teams - (\d+) of theirs near", l).group(1)) for l in before[out_at:] if re.search(r"Necromancers: teams - \d+ of theirs near", l)]
    print("  info  theirs near at the gate: %s" % looks[:8])
    check(any(n >= 1 for n in looks), "the siege's walkers carry team 1 (theirs near: %s)" % looks[:6])
    read = show(findin(after, r"Co-save: (\d+) sieges on read \(version 2\)"), 1)
    check(bool(read) and int(re.search(r"(\d+) sieges on", read[0]).group(1)) >= 1, "saved and loaded: the siege read back (version 2)")
    looks2 = [int(re.search(r"teams - (\d+) of theirs near", l).group(1)) for l in after if re.search(r"Necromancers: teams - \d+ of theirs near", l)]
    check(any(n >= 1 for n in looks2), "after the load its walkers are team 1's again (%s)" % looks2[:6])
    end(scenario)
if scenario == "march":
    # the load: the first co-save line read after the last written
    saved_at = max([i for i, l in enumerate(log) if re.match(r"Co-save: .* written", l)] or [len(log)])
    load_at = min([i for i, l in enumerate(log) if i > saved_at and re.match(r"Co-save: .* read", l)] or [len(log)])
    before, after = log[:load_at], log[load_at:]
    def findin(lines, pattern):
        return [l for l in lines if re.search(pattern, l)]
    check(bool(show(findin(before, r"Necromancers: boss 0 \(team 1\) rose at " + place))), "boss 0 rose at Toadstool Hollow")
    took = show(findin(before, r"Necromancers: boss 0 took .* place\(s\) now"), 4)
    check(bool(took), "the realm grows: he took another dungeon (%d)" % len(took))
    formed = findin(before, r"Necromancers: a band of 3 walkers \(boss 0, team 1\) left its home")
    check(bool(formed), "his bands formed (%d)" % len(formed))
    reached = show(findin(before, r"Necromancers: a band of 3 walkers \(boss 0, team 1\) reached its city"), 3)
    check(bool(reached), "a band reached its city - the siege pressure up (%d)" % len(reached))
    show(findin(log, r"besieged"), 3)
    written = show(findin(before, r"Co-save: (\d+) necromancers' bosses and (\d+) of their far bands written"), 2)
    read = show(findin(after, r"Co-save: (\d+) necromancers' bosses read, (\d+) of them alive, (\d+) far bands \(version 2\)$"), 2)
    w = re.search(r"(\d+) necromancers' bosses and (\d+) of their far bands", written[-1]).groups() if written else None
    r = re.search(r"(\d+) necromancers' bosses read, (\d+) of them alive, (\d+) far bands", read[-1]).groups() if read else None
    check(w is not None and r is not None and w[0] == r[0] == "1" and r[1] == "1" and w[1] == r[2],
          "the load reads back what the save wrote (written %s, read %s)" % (w, r))
    dw = findin(before, r"Co-save: (\d+) held dungeons written")
    dr = findin(after, r"Co-save: (\d+) held dungeons read")
    show(dw[-1:] + dr[-1:], 2)
    check(bool(dw) and bool(dr) and re.search(r"(\d+) held", dw[-1]).group(1) == re.search(r"(\d+) held", dr[-1]).group(1),
          "the held dungeons (his places among them) read back")
    check(not findin(after, r"Necromancers: boss \d+ \(team \d+\) rose at"), "no new boss rises after the load - the realm came back")
    check(bool(show(findin(after, r"Necromancers: boss 0 took|Necromancers: a band of"), 2)) or True, "after the load the realm goes on (info)")
    end(scenario)
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
check(bool(show(find(r"Necromancers: test - TestHome=" + place + r": the first boss there \(free\)"))), "TestHome free in the settled pool")
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
check(bool(show(find(r"Necromancers: test - TestFight=walkers: [1-9]\d* walkers at his place killed"))), "his walkers killed, he left alone")
check(bool(show(find(place + r" - walker [0-9A-F]{8} ended; 0 walkers left"))), "his place: 0 walkers left")
fell_at = min([i for i, l in enumerate(log) if re.search(place + r" - the necromancer fell", l)] or [len(log)])
taken = [l for l in log[:fell_at] if re.search(r"Dungeons: the necromancer at " + place + r" taken out", l)]
show(taken)
check(not taken, "alone, he is not taken out while the player is inside")
alone = re.findall(r"\b(0004B94[5-8]) x\d+(?: \(dead (\d+)\))?", roster.get("alone", ""))
check(bool(alone) and all(not d or d == "0" for _, d in alone), "the roster at 'alone' holds him alive (%s)" % alone)
check(bool(show(find(place + r" - the necromancer fell"))), "killall: the necromancer fell")
check(bool(show(find(r"Necromancers: boss 0 \(team 1\) fell - his realm stops"))), "his realm stops")
check(bool(show(find(r"Necromancers: the last of them fell - a new set rises later"))), "the last of them: a new set later (ComeBack=1)")
check(bool(show(find(r"Dungeons: site \d+ " + place + r" \(cave\) taken - now refuge"))), "the place cleared and taken: a refuge")
check(bool(show(find(r"Necromancers: " + place + r" is no longer boss 0's - cleared, a refuge won"))), "the realm told: a refuge won")
print("necro %s: %s" % (scenario, "PASS" if failures == 0 else "FAIL (%d)" % failures))
sys.exit(1 if failures else 0)
PY
