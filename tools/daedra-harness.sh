#!/usr/bin/env bash
# Runs a Daedra scenario (docs/walkers-spec.md 4.22) on OBVR's harness with
# [Invaders] set for the run, then checks OblivionWalkers.log. The INI is put
# back whatever happens (md5 checked). Needs tools/linux-walkers.sh on.
# Announce "busy" to the other sessions first.
#
#   tools/daedra-harness.sh spawn [Kind]     harness/daedra-spawn.txt, Kind 1 by default
#   tools/daedra-harness.sh fight [Mode]     harness/daedra-fight.txt: a walker and a Churl
#   tools/daedra-harness.sh band [Mode]      harness/daedra-band.txt: OnlyFromOblivionGates=1, a band placed and led
#   tools/daedra-harness.sh save [Mode]      harness/daedra-save.txt: the band and its leader across a save and a load
#   tools/daedra-harness.sh fuel [Mode]      harness/daedra-fuel.txt: a bandit the Daedra kill rises (Mode 2)
#   tools/daedra-harness.sh gate [Mode]      harness/daedra-gate.txt: "Close the gate" begun and done (4.22a 6)
#   tools/daedra-harness.sh siege [Mode]     harness/daedra-siege.txt: a Daedra siege at Anvil, its dead banished (4.22a 5a, 5b), the gate guards out against them, the red sky (idea 9)
#   tools/daedra-harness.sh piles [Mode]     harness/daedra-piles.txt: the siege's piles across a save and a load ('PILE')
#   tools/daedra-harness.sh daedric [Mode]   harness/daedra-daedric.txt: the Daedric disguise fools a Dremora, too close it fights (idea 7)
#   tools/daedra-harness.sh legion [Mode]    harness/daedra-legion.txt: a band near Anvil - the Legion comes out (idea 10)
#   tools/daedra-harness.sh siegesave [Mode] harness/daedra-siegesave.txt: the siege saved and loaded with its Daedra standing ('PLAC', the red sky again)
#   tools/daedra-harness.sh boss 0           harness/daedra-boss.txt: a goblin boss list placed stays goblins (KeepBosses)
#   tools/daedra-harness.sh daedricblow [Mode] harness/daedra-daedricblow.txt: the disguise across a save, then ended by a kick (OnHit)
#   tools/daedra-harness.sh daedricspell [Mode] harness/daedra-daedricspell.txt: the same, ended by a fire bolt from 60 units (OnHit for a spell?)
set -euo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
obvr="${OBVR_DIR:-$root/../OBVR}"
game="${WALKERS_LINUX_GAME:-$HOME/.local/share/Steam/steamapps/common/Oblivion}"
log="$game/OblivionWalkers.log"
ini="$game/Data/OBSE/Plugins/OblivionWalkers.ini"
scenario="${1:-spawn}"
kind="${2:-1}"
backup="$(mktemp)"
cp "$ini" "$backup"
sum="$(md5sum "$ini" | cut -c1-32)"
restore() {
	cp "$backup" "$ini"
	rm -f "$backup"
	[[ "$(md5sum "$ini" | cut -c1-32)" == "$sum" ]] && echo "daedra-harness: INI restored (md5 $sum)" ||
		echo "daedra-harness: INI NOT restored" >&2
}
trap restore EXIT
python3 - "$ini" "$kind" "$scenario" <<'INI'
import re, sys
path, kind, scenario = sys.argv[1], sys.argv[2], sys.argv[3]
text = open(path, encoding="latin-1", newline="").read()
nl = "\r\n" if "\r\n" in text else "\n"

def set_key(text, section, key, value):
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

text = set_key(text, "Invaders", "Mode", kind)
if scenario in ("band", "save", "legion"):
    text = set_key(text, "Invaders", "OnlyFromOblivionGates", "1")
    text = set_key(text, "Invaders", "TestBandSeconds", "20")
if scenario == "gate":
    text = set_key(text, "Invaders", "TestGateOpen", "2")
    text = set_key(text, "Invaders", "TestGateOpenSeconds", "60")
if scenario == "legion":
    text = set_key(text, "Guards", "LegionDistance", "16000")
if scenario in ("daedricblow", "daedricspell"):
    # the Dremora is struck from 55-60 units: still fooled that near
    text = set_key(text, "Disguise", "DaedricNearDistance", "30")
if scenario == "boss":
    # the boss list's entries into the log after each pass and when they change
    text = set_key(text, "Replace", "TestLogList", "000340BA")
    # Mode 2 rolls a band's kind; the Legion answers only Daedra, so every band is Daedra
    text = set_key(text, "Invaders", "DaedraPercent", "100")
if scenario in ("siege", "siegesave"):
    text = set_key(text, "Invaders", "MarchWeather", "1")
if scenario in ("siege", "piles", "siegesave"):
    for key, value in (("TestGate", "1"), ("TestDaedra", "1"), ("DaedraMin", "6"), ("DaedraMax", "6"),
                       ("DaedraAtGate", "4"), ("ChancePerDayPercent", "0")):
        text = set_key(text, "Siege", key, value)
    text = set_key(text, "Invaders", "BanishSeconds", "5")
# the dungeons' runner off, as in the other wrappers: nobody of the plugin's own beside the player
text = set_key(text, "Dungeons", "Enabled", "0")
open(path, "w", encoding="latin-1", newline="").write(text)
INI
mkdir -p "$root/artifacts"
(cd "$obvr" && python3 tools/hand-script-run.py "$root/harness/daedra-$scenario.txt" --headless ${WALKERS_RUNNER_ARGS---no-steamvr} \
	--artifact-root "$root/artifacts/hand-script") | tee "$root/artifacts/daedra-$scenario-runner.txt"
run="$(ls -td "$root"/artifacts/hand-script/daedra-"$scenario"/*/ | head -1)"
cp "$log" "$run/OblivionWalkers.log"
python3 - "$run/OblivionWalkers.log" "$run/OBVR.log" "$scenario" "$kind" <<'PY'
import re, sys
lines = open(sys.argv[1], encoding="latin-1").read().splitlines()
obvr = open(sys.argv[2], encoding="latin-1").read().splitlines()
scenario, kind = sys.argv[3], int(sys.argv[4])
failures = 0

def check(ok, what):
    global failures
    print(("  ok    " if ok else "  FAIL  ") + what)
    failures += 0 if ok else 1

def show(prefix, most=8):
    found = [l for l in lines if prefix in l]
    for l in found[:most]:
        print("        " + l[:240])
    return found

spawns = show("Spawns: Invaders Mode=")
m = re.search(r"- (\d+) entries point at the Daedra \(([0-9A-F]{8})\), (\d+) entries", spawns[-1]) if spawns else None
if kind != 0 and scenario not in ("band", "save", "siege", "gate", "piles", "daedric", "legion", "siegesave", "daedricblow", "daedricspell", "boss"):
    check(m is not None and int(m.group(1)) > 0, "the replacement names the Daedra (%s entries)" % (m.group(1) if m else "-"))
    check(m is not None and m.group(2).endswith("000A94"), "the NOSUMMON list A94 (Summons=0)")
walkers = [l for l in lines if l.startswith("Spawns: ")]
roster = [l for l in obvr if "HandScript: actors" in l]
print("Roster:")
for l in roster[:3]:
    print("        " + l[:400])
if scenario == "spawn" and kind == 1:
    full = [l for l in roster if not re.search(r"actors - 0 loaded", l)]
    check(bool(full), "the placed lists brought someone (%d rosters with actors)" % len(full))
    check(not re.search(r"\b[0-9A-F]{2}000(80[1-4]|9[0-4][0-9A-F])\b", " ".join(full)), "no walker in the roster")
    rises = [l for l in lines if "waits to rise" in l]
    check(not rises, "nobody waits to rise with Mode=1 (%d)" % len(rises))
    show("stays down (Invaders Mode")
if scenario == "band":
    gates = show("Bands: ")
    check(any(re.search(r"\d+ of 103 Oblivion gates open", l) for l in lines), "the 103 gates read")
    found = [re.search(r"Bands: (\d+) new members found", l) for l in lines]
    found = [int(m.group(1)) for m in found if m]
    check(bool(found) and found[-1] >= 1, "the band's members found at the marker (%s)" % found)
    show("Horde: ", 10)
    for l in [l for l in obvr if "HandScript: actors" in l][:6]:
        print("        " + l[:300])
if scenario == "save":
    written = [re.search(r"Co-save: (\d+) far bands and (\d+) band members written", l) for l in lines]
    written = [int(m.group(2)) for m in written if m]
    read = [re.search(r"Co-save: (\d+) far bands and (\d+) band members read", l) for l in lines]
    read = [int(m.group(2)) for m in read if m]
    show("Bands: ")
    check(bool(written) and written[-1] >= 2, "the band and its leader written (%s)" % written)
    check(bool(read) and read[-1] == written[-1], "the same members read after the load (%s)" % read)
    check(any("the leader found at the marker" in l for l in lines), "a leader found and marked")
if scenario == "fuel":
    show("waits to rise")
    show("stays down (Invaders")
    check(any("waits to rise" in l for l in lines), "a fallen bandit waits to rise")
if scenario == "fight":
    for l in [l for l in obvr if "HandScript: actors" in l or "HandScript: mark" in l][:12]:
        print("        " + l[:300])
    show("Invaders:")
    last = [l for l in roster if "loaded" in l][-1] if roster else ""
    dead = re.search(r"(\d+) dead", last)
    print("        last roster: " + last[:200])
    if kind == 2:
        check(dead is not None and int(dead.group(1)) >= 1, "Kind 2: one of the two is dead")
    if kind == 3:
        check(dead is not None and int(dead.group(1)) == 0, "Kind 3: neither is dead")
if scenario == "gate":
    show("Gate quest: ", 30)
    check(any("Gate quest: test - gate 2 read open" in l for l in lines), "gate 2 read open by the switch")
    begun = [l for l in lines if "Gate quest: begun - gate 2" in l]
    check(bool(begun), "the quest begun at gate 2 once heard")
    check(any(re.search(r"Gate quest: the target \w+ -> 0001E8A3|Gate quest: the target \w+ -> [0-9A-F]{8}", l) for l in lines),
          "its target rewritten to the gate's door")
    check(bool(begun) and re.search(r"flags (\w\w)", begun[0]) is not None and int(re.search(r"flags (\w\w)", begun[0]).group(1), 16) & 1 == 1,
          "the quest running after SetStage 10")
    done = [l for l in lines if "Gate quest: done" in l]
    check(bool(done), "done once the gate read shut")
    check(bool(done) and int(re.search(r"flags (\w\w)", done[0]).group(1), 16) & 2 == 2, "the quest completed (stage 20)")
if scenario == "siege":
    show("Siege: ", 40)
    show("Placer: ", 30)
    show("Banish: ", 30)
    check(any("its Daedra come out" in l for l in lines), "the player at the gate: a Daedra siege shown")
    came = [int(m.group(1)) for m in (re.search(r"Siege: (\d+) Daedra came to the siege", l) for l in lines) if m]
    check(sum(came) >= 4, "the placer brought the Daedra (%s)" % came)
    check(sum(1 for l in lines if "a Daedra fell before Anvil Castle Gate" in l) >= 4, "the fallen counted off")
    check(any("back to Oblivion" in l for l in lines), "a dead Daedra banished")
    bags = [l for l in lines if re.search(r"bag FF[0-9A-F]{6} holds [1-9]", l)]
    check(bool(bags), "its things into a sack (%d)" % len(bags))
    check(any("handed to DeleteReference" in l for l in lines), "bodies deleted")
    check(any("Anvil Castle Gate is over" in l or "the siege at Anvil Castle Gate is over" in l for l in lines),
          "the siege over once all fell")
if scenario == "siege":
    guards = show("Guards: ", 20)
    out = [int(m.group(1)) for m in (re.search(r"out at Anvil Castle Gate for \d+ walkers' worth near, (\d+) Daedra", l) for l in lines) if m]
    check(bool(out) and max(out) >= 1, "the gate guards came out against the Daedra (%s)" % out)
    check(any(re.search(r"Guards: \w+ at Anvil \w+ Gate sent at FF", l) for l in lines) or
          any(l.startswith("Death: gate guard") for l in lines), "a guard sent at a Daedra or fighting (one fell)")
    others = [l for l in lines if re.search(r"out at Anvil (Main|Dock) Gate .* [1-9]\d* Daedra among them", l)]
    check(not others, "the city's other gates stay shut for a siege at one (%d)" % len(others))
    show("March weather: ", 10)
    check(any("March weather: a Daedra siege" in l for l in lines), "at the besieged gate: the red sky asked for")
    check(any("March weather: the sky's weather now 000836D5" in l for l in lines),
          "the sky's weather the red sky (read off the sky; the console's answer is no proof)")
    check(any("March weather: let go" in l for l in lines), "let go once the siege is over")
    check(any(re.search(r"March weather: after letting go the sky's weather now (?!000836D5)[0-9A-F]{8}", l) for l in lines),
          "the sky turns from the red sky afterwards")
if scenario == "piles":
    show("Banish: ", 20)
    show("Co-save: ", 20)
    written = [int(m.group(1)) for m in (re.search(r"Co-save: (\d+) piles of banished Daedra written", l) for l in lines) if m]
    read = [int(m.group(1)) for m in (re.search(r"Co-save: (\d+) piles of banished Daedra read", l) for l in lines) if m]
    check(bool(written) and written[-1] >= 1, "the piles written (%s)" % written)
    check(bool(read) and read[-1] == written[-1], "the same piles read after the load (%s)" % read)
if scenario == "daedric":
    show("Daedric disguise: ", 30)
    check(any("full set 1" in l for l in lines), "the five pieces read as a full set")
    check(any("the player joins DaedraFaction and DremoraFaction" in l and "ranks now 0 0" in l for l in lines),
          "fooled: the player in their factions (GetFactionRank 0 0; the console's answer is no proof)")
    check(any("Too close" in l and "ranks now -1 -1" in l for l in lines), "seen through: out of them again (-1 -1)")
    fooled = [l for l in lines if re.search(r"Daedric disguise: 0 Daedra near in combat \(fooled 1", l)]
    fooledFights = [l for l in lines if re.search(r"Daedric disguise: [1-9]\d* Daedra near in combat \(fooled 1", l)]
    close = [l for l in lines if re.search(r"Daedra near in combat \(fooled 1, known 0; the nearest ([2-9]\d\d|1\d\d\d)\)", l)]
    check(bool(close), "fooled with a Daedra within 2000 units (%d lines)" % len(close))
    check(bool(fooled) and not fooledFights, "fooled, the Dremora 400 units off leaves him be (%d lines with a fight)" % len(fooledFights))
    check(any("Too close" in l and "leaves" in l for l in lines), "moved within reach: they see through it")
    fights = [l for l in lines if re.search(r"Daedric disguise: [1-9]\d* Daedra near in combat \(fooled 0, known 1", l)]
    check(bool(fights), "known, the Dremora fights")
if scenario == "legion":
    show("Bands: ", 10)
    show("Guards: ", 20)
    out = [int(m.group(1)) for m in (re.search(r"out at Anvil \w+ Gate for \d+ walkers' worth near, (\d+) Daedra", l) for l in lines) if m]
    check(bool(out) and max(out) >= 1, "the Legion came out against the band (%s)" % out)
    check(any(re.search(r"Guards: \w+ at Anvil \w+ Gate sent at FF", l) for l in lines), "a guard sent at a band's Daedra")
if scenario == "boss":
    listed = show("Spawns: TestLogList", 12)
    copies = show("Bosses: a clean copy of", 12)
    show("Bosses: ", 6)
    last = listed[-1] if listed else ""
    check(bool(last) and not re.search(r":2B[0-9A-F]{6}x", last), "the boss list's last look names no record of ours")
    moo = bool(re.search(r":23[0-9A-F]{6}x", last))
    n = re.search(r"; (\d+) of them the boss guard's clean copies", last)
    check(not moo or (n is not None and int(n.group(1)) >= 1),
          "MOO's rewrite pointed at clean copies (MOO entries %d, copies %s)" % (moo, n.group(1) if n else "-"))
    check(not moo or (bool(copies) and not any(re.search(r":2B[0-9A-F]{6}x", l) for l in copies)),
          "the clean copies name no walker (%d copies logged)" % len(copies))
    kept = [re.search(r"(\d+) files read, (\d+) lists and (\d+) creatures or people kept", l) for l in lines]
    kept = [m for m in kept if m]
    check(bool(kept) and int(kept[-1].group(1)) >= 1 and int(kept[-1].group(2)) >= 1,
          "the plugin files read in the game, boss lists kept (%s)" % ([m.group(0) for m in kept][-1:]))
    marks = [i for i, l in enumerate(obvr) if "HandScript: mark placed" in l]
    rosters = [(i, l) for i, l in enumerate(obvr) if "HandScript: actors - " in l]
    before = [l for i, l in rosters if marks and i < marks[0]]
    after = [l for i, l in rosters if marks and i > marks[0]]
    for l in (before[-1:] + after[-1:]):
        print("        " + l[:400])
    count = lambda l: int(re.search(r"actors - (\d+) loaded", l).group(1)) if l else 0
    grown = count(after[-1] if after else "") - count(before[-1] if before else "")
    check(grown >= 3, "the three placed came (%d more loaded)" % grown)
if scenario == "siegesave":
    show("Co-save: ", 30)
    show("Placer: ", 20)
    show("March weather: ", 12)
    placed = [int(m.group(1)) for m in (re.search(r"Co-save: (\d+) placed Daedra still in the world written", l) for l in lines) if m]
    check(bool(placed) and max(placed) >= 1, "the standing Daedra written ('PLAC': %s)" % placed)
    taken = [int(m.group(1)) for m in (re.search(r"Placer: (\d+) placed ones of the save taken out of the world", l) for l in lines) if m]
    check(bool(taken) and max(taken) >= 1, "after the load the save's placed ones taken out (%s)" % taken)
    loaded = max((i for i, l in enumerate(lines) if "placed ones of the save taken out" in l), default=-1)
    after = lines[loaded + 1:] if loaded >= 0 else []
    came = [int(m.group(1)) for m in (re.search(r"Siege: (\d+) Daedra came to the siege", l) for l in after) if m]
    check(sum(came) >= 1, "the siege places its own again after the load (%s)" % came)
    check(any("March weather: the sky's weather now 000836D5" in l for l in after), "the red sky again after the load")
    check(any("the siege at Anvil Castle Gate is over" in l or "Anvil Castle Gate is over" in l for l in after),
          "the siege over once all fell")
    check(any("March weather: let go" in l for l in after), "the sky let go once the siege is over")
if scenario in ("daedricblow", "daedricspell"):
    show("Daedric disguise: ", 40)
    loads = [i for i, l in enumerate(lines) if "after a load, before the first look" in l and "ranks now -1 -1" in l]
    check(len(loads) >= 2, "taken out of the factions at the start and after the load (%d)" % len(loads))
    last = loads[-1] if loads else len(lines)
    joins = [i for i, l in enumerate(lines) if i > last and "the player joins" in l and "ranks now 0 0" in l]
    check(bool(joins), "after the load the full set fools them again (ranks 0 0)")
    joinsBefore = [i for i, l in enumerate(lines) if i < last and "the player joins" in l and "ranks now 0 0" in l]
    check(bool(joinsBefore), "fooled before the save")
    struck = [i for i, l in enumerate(lines) if "You struck one of them" in l and "leaves" in l and "ranks now -1 -1" in l]
    check(bool(struck) and bool(joins) and struck[-1] > joins[0], "the blow (OnHit): out of the factions (-1 -1)")
    after = struck[-1] if struck else len(lines)
    fights = [l for l in lines[after:] if re.search(r"Daedric disguise: [1-9]\d* Daedra near in combat \(fooled 0, known 1", l)]
    check(bool(fights), "after the blow the Dremora fights")
    show("Hit: ", 6)
    hits = [l for l in lines if re.match(r"Hit: \w+ hit by 00000014", l)]
    check(bool(hits), "OnHit saw the player's blow (%d)" % len(hits))
print("daedra-harness: %s" % ("PASS" if failures == 0 else "FAIL (%d)" % failures))
sys.exit(1 if failures else 0)
PY
