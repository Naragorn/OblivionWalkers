#!/usr/bin/env bash
# Runs a held dungeon's walkers on OBVR's harness (harness/dungeon-walkers.txt
# or dungeon-fight.txt), the live INI set for the run and put back whatever
# happens. Mode: siege (a siege counted off by the console's kills), taken
# (a walker dungeon cleared so), fight (a siege fought by the game - a look),
# clear (a place to clear: the people wait outside, killall, they move in).
# Exit 0: every check held. Announce "busy" first.
set -euo pipefail
mode="${1:-siege}"
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
obvr="${OBVR_DIR:-$root/../OBVR}"
game="${WALKERS_LINUX_GAME:-$HOME/.local/share/Steam/steamapps/common/Oblivion}"
ini="$game/Data/OBSE/Plugins/OblivionWalkers.ini"
case "$mode" in
	siege) state=2; walkers=8; behead=0; scenario=dungeon-walkers; site="Fort Ash" ;;
	taken) state=4; walkers=8; behead=0; scenario=dungeon-walkers; site="Fort Ash" ;;
	fight) state=2; walkers=10; behead=1; scenario=dungeon-fight; site="Fort Ash" ;;
	clear) state=3; walkers=8; behead=0; scenario=dungeon-clear; site="Echo Mine" ;;
	talk) state=3; walkers=8; behead=0; scenario=dungeon-talk; site="Echo Mine" ;;
	runner) state=2; walkers=8; behead=0; scenario=dungeon-runner; site="Fort Ash" ;;
	paper) state=2; walkers=8; behead=0; scenario=dungeon-paper; site="Fort Ash" ;;
	notice) state=1; walkers=8; behead=0; scenario=dungeon-notice; site="Fort Ash" ;;
	*) echo "mode: siege | taken | fight | clear | talk | runner | paper | notice" >&2; exit 2 ;;
esac
# fight only: the people and the walkers of the look, e.g. "fight 5 12" (8 and 10 without).
people="${2:-8}"
if [[ "$mode" == "fight" && -n "${3:-}" ]]; then walkers="$3"; fi
backup="$(mktemp)"
cp "$ini" "$backup"
trap 'cp "$backup" "$ini"; rm -f "$backup"' EXIT
python3 - "$ini" "$state" "$walkers" "$behead" "$site" "$mode" "$people" <<'INI'
import re, sys
path, state, walkers, behead, site, mode, people = sys.argv[1:8]
text = open(path, encoding="latin-1", newline="").read()

def put(text, section, key, value):
    head = re.search(r"^\[" + section + r"\]\s*$", text, re.M)
    if head is None:
        return text + "\r\n[" + section + "]\r\n" + key + "=" + value + "\r\n"
    end = re.search(r"^\[", text[head.end():], re.M)
    stop = head.end() + (end.start() if end else len(text) - head.end())
    body = text[head.end():stop]
    if re.search(r"^" + key + r"=", body, re.M):
        body = re.sub(r"^" + key + r"=.*$", key + "=" + value, body, count=1, flags=re.M)
    else:
        body = "\r\n" + key + "=" + value + body
    return text[:head.end()] + body + text[stop:]

for key, value in (("TestSite", site), ("TestSiteState", state), ("Enabled", "1"),
                   ("AttackWalkersMin", walkers), ("AttackWalkersMax", walkers), ("WalkersAtOnce", "6"),
                   ("PeopleMin", people), ("PeopleMax", people), ("Journal", "1"), ("Rumors", "1"),
                   # the test site alone: it is the news, and its quest the one told
                   ("ActiveMin", "1"), ("ActiveMax", "1"),
                   # a runner for help only in his own run
                   ("MessengerHours", "1" if mode == "runner" else "0"),
                   # runner, paper: the siege away stays one (the scenario's "set gamehour to 12" jumps the
                   # clock - measured 2026-11-13: the toll of those hours ended the siege before anyone heard of it)
                   ("RefugeeKillsPerHour", "0" if mode in ("runner", "paper") else "2"),
                   ("LossPercentPerHour", "0" if mode in ("runner", "paper") else "25"),
                   # siege: a day on, the refuge is attacked again (the quest opened anew)
                   ("AttackChancePercent", "100" if mode == "siege" else "0")):
    text = put(text, "Dungeons", key, value)
text = put(text, "Zombies", "DieOnlyBeheaded", behead)
text = put(text, "Debug", "StepTiming", "1")
open(path, "w", encoding="latin-1", newline="").write(text)
INI
(cd "$obvr" && python3 tools/hand-script-run.py "$root/harness/$scenario.txt" --headless \
	--artifact-root "$root/artifacts/hand-script") > "$root/artifacts/dungeon-walkers-runner.txt" 2>&1 || true
tail -1 "$root/artifacts/dungeon-walkers-runner.txt"
run="$(ls -td "$root"/artifacts/hand-script/$scenario/*/ | head -1)"
cp "$game/OblivionWalkers.log" "$run/OblivionWalkers.log"
echo "run: $run ($mode)"
python3 - "$run/OblivionWalkers.log" "$mode" <<'PY'
import re, sys
lines = open(sys.argv[1], encoding="latin-1").read().splitlines()
mode = sys.argv[2]
failures = 0

def check(ok, what):
    global failures
    print(("  ok    " if ok else "  FAIL  ") + what)
    failures += 0 if ok else 1

d = [l for l in lines if l.startswith("Dungeons: ")]
place = "Echo Mine" if mode in ("clear", "talk") else "Fort Ash"
if mode in ("runner", "paper", "notice"):
    again = len(d)
told = [m for l in d for m in [re.search(
    r"journal - " + place + r": stage (\d+) \((\w[\w ]*); told (\d+) before\)(, taken off completed first)? - SetStage "
    r"(\w+) answered (\d); the quest (\w+) at stage (\d+), flags (\w\w) \((\w\w) before\)", l)] if m]

def journal(stage, why, before, reopened, completed, what, nth=0):
    hits = [m for m in told if m.group(1) == str(stage) and m.group(2) == why and m.group(3) == str(before)]
    ok = len(hits) > nth
    if ok:
        m = hits[nth]
        # (SetStage's answer is no success flag - xOBSE's RunScriptLine2 reads 0; the quest's stage is the proof)
        # and neither is the engine's stage: with repeated stages it stays the highest ever set - the flags
        # are the proof (1 running, 2 completed) and the pictures of the game's box)
        ok = (bool(m.group(4)) == reopened and int(m.group(9), 16) & 1 == 1
              and bool(int(m.group(9), 16) & 2) == completed and m.group(7) != "00000000")
    check(ok, what + (f" ({hits[nth].group(0)[10:]})" if len(hits) > nth else f" (told: {[m.group(1, 2, 3) for m in told]})"))

shown = [m for l in d for m in [re.search(r"map marker \w+ shown: flags (\w+), (\w+) before", l)] if m]
if mode != "notice":
    check(len(shown) == 1 and int(shown[0].group(1), 16) & 1 == 1 and shown[0].group(1) != "FFFF",
          "the place's map marker shown once, visible after" + (f" (flags {shown[0].group(1)}, {shown[0].group(2)} before)" if shown else ""))
rewards = [l for l in d if l.startswith("Dungeons: reward - ")]
link = [l for l in lines if "variables" in l and "LINK" in l]
check(any("195 of 195" in l for l in link), "LINK holds the 195 variables" + (f" ({link[-1][:90]})" if link else ""))
pool = [l for l in d if "the pool to site" in l and "Fort Ash" in l]
again = next((k for k, l in enumerate(d) if "site 0 Fort Ash (fort) attacked" in l), len(d))
into = [l for l in d[:again] if re.search(r"walker \w+ into Fort Ash", l)]
ended = [l for l in d[:again] if "Fort Ash - walker" in l and "ended" in l]
if mode == "clear":
    kit = [l for l in d if "the kit to site 0 Echo Mine" in l]
    check(len(kit) >= 1 and "(mine; to clear, 8 people) - waiting outside" in kit[0], "the kit waits outside Echo Mine, eight of them")
    stands = [m for l in d for m in [re.search(r"stands - (\d+) shown, (\d+) enabled, (\d+) in its first cell \w+, (\d+) with their 3D; the farthest (\d+) units from its spot, the lowest (-?\d+) beneath it", l)] if m]
    check(len(stands) >= 1 and stands[0].group(1, 2, 4) == ("8", "8", "8") and int(stands[0].group(6)) < 150,
          "outside the eight stand enabled with their 3D on the land" + (f" ({stands[0].group(0)})" if stands else ""))
    noted = [l for l in d if "nobody living left in it" in l]
    check(len(noted) == 1 and "cells cleared 01 - the place is cleared" in noted[0], "after the killall the one cell is noted: cleared")
    check(not any("nobody living" in l for l in lines[:lines.index(noted[0])] if "Echo Mine" in l) if noted else False,
          "not before - its occupants lived")
    check(any("site 0 Echo Mine (mine) taken - now refuge, 8 people" in l for l in d), "the place taken: a refuge of the eight")
    check(any("move in by the door (9)" in l for l in d), "those who waited move in by the door, the trader with them")
    check(len(stands) >= 2 and stands[-1].group(1, 2, 3) == ("9", "9", "9"),
          "three seconds on the nine stand in its cell" + (f" ({stands[-1].group(0)})" if len(stands) >= 2 else ""))
    journal(30, "seen", 0, True, False, "met before the door: the quest says to clear it, open")
    journal(60, "taken", 30, False, True, "cleared: the quest says taken, completed")
    check(len(rewards) == 1 and "Echo Mine is cleared - refugees move in" in rewards[0], "the reward once" + (f" ({rewards[0][19:]})" if rewards else ""))
elif mode == "notice":
    hung = [l for l in d if l.startswith("Dungeons: notice - ")]
    cities = {l.split(" - ")[1].split(":")[0] for l in hung if ": hung " in l or " already" in l}
    check(len(cities) == 8, f"each of the eight cities' notices is seen to ({sorted(cities)})")
    boards = [l for l in hung if "on MOO's board" in l]
    check(len(boards) >= 1 and any(l.startswith("Dungeons: notice - anvil") for l in boards),
          f"with MOO loaded they hang on its boards, Anvil's among them ({len(boards)} on boards, {len(hung) - len(boards)} elsewhere)")
    for l in hung:
        print("        " + l[10:230])
    read = [l for l in d if "news - the player reads" in l]
    check(len(read) == 1 and read[0].endswith("A84"), "the notice on the board, activated, is read in the book menu" + (f" ({read[0][10:]})" if read else ""))
    journal(5, "heard", 0, True, False, "reading it tells of the refuge: heard of, open")
    print("  look  the eye pictures a-* and b-*: Anvil's board from both sides, the notice turned five ways")
elif mode == "runner":
    sent = [l for l in d if "runner - sent with news of kind 2" in l]
    check(len(sent) == 1, "a runner is sent with the siege's news, once" + (f" ({sent[0][19:140]})" if sent else ""))
    spoke = [l for l in d if "runner - told to speak to the player" in l]
    check(len(spoke) >= 1 and "his 3D 1" in spoke[0], "a second on he is told to speak to the player, his 3D there" + (f" ({spoke[0][19:150]})" if spoke else ""))
    check(any("news - told to the player (site 0)" in l for l in d), "his greeting's result script tells the news")
    journal(20, "heard", 0, True, False, "the quest says besieged, open")
    check(any("runner - he told his news" in l for l in d), "he has told his news and stays about")
    check(any("runner - taken away" in l for l in d), "the player gone, he is taken away")
    print("  look  the picture at 'spoken': the dialogue with the runner")
elif mode == "paper":
    read = [l for l in d if "news - the player reads" in l]
    check(len(read) == 1 and read[0].endswith("A76"), "the notice opened in the book menu is seen, once" + (f" ({read[0][10:]})" if read else ""))
    journal(20, "heard", 0, True, False, "reading it tells the news: the quest says besieged, open")
    closed = [l for l in d if "is closed (the book menu now holds 00000000)" in l]
    check(len(closed) == 1, "the menus closed, the book menu holds nothing" + (f" ({closed[0][10:]})" if closed else ""))
    news = [l for l in lines if l.startswith("News: ")]
    check(not any("missing" in l and " 0 missing" not in l for l in news), "every edition's book is there" + (f" ({news[0][:110]})" if news else " (no News line)"))
elif mode == "talk":
    journal(30, "seen", 0, True, False, "met before the door: the quest says to clear it, open")
    talk = [l for l in d if "talk - the kit's people speak as" in l]
    check(len(talk) >= 1 and "speak as 3" in talk[-1], "the kit's people speak as those waiting outside (3)" + (f" ({talk[-1][10:60]})" if talk else ""))
    news = [l for l in d if l.startswith("Dungeons: news - now")]
    check(len(news) >= 1 and "now 3" in news[0] and "Echo Mine" in news[0], "before he met them the place was news, a place to clear" + (f" ({news[0][10:]})" if news else ""))
    check(len(news) >= 2 and "now 0" in news[-1], "met: no news any more")
    print("  look  the picture at 'talked': the greeting and the two topics")
elif mode == "fight":
    died = [l for l in d if "one of its people" in l and "died" in l]
    print(f"  look  the pool: {len(pool)}, walkers into the place: {len(into)}, walkers ended: {len(ended)}, "
          f"people died: {len(died)}")
    for l in [l for l in d if " defended " in l or " overrun " in l or " held " in l]:
        print("  look  " + l[:200])
    for l in [l for l in lines if l.startswith("Slow aggression: walker") and "roused" in l][:3]:
        print("  look  " + l[:200])
else:
    name = "besieged" if mode == "siege" else "walker dungeon"
    check(len(pool) >= 1 and f"({name}; 8 walkers, 6 at once)" in pool[0], f"the pool goes to Fort Ash, a {name} of 8, six at once")
    first = into[:6]
    where = "by the door" if mode == "siege" else "among its spots"
    check(len(first) == 6 and all(where in l and "answered 1" in l for l in first), f"six come into the place {where}")
    check(len(into) == 8 and all("by the door" in l for l in into[6:]), "the last two come in by the door as the six fall")
    check(len(ended) == 8 and "0 walkers left" in ended[-1], f"all eight counted off ({len(ended)})")
    out = [int(m.group(1)) for l in d for m in [re.search(r"is held - (\d+) occupants taken out", l)] if m]
    check(len(out) >= 1 and out[0] >= 1, f"the game's occupants out on entering ({out})")
    if mode == "siege":
        kit = [l for l in d if "the kit to site" in l and "besieged, 8 people" in l]
        check(len(kit) >= 1, "the kit stands in the besieged place")
        check(any("site 0 Fort Ash (fort) defended - now refuge, 8 people" in l for l in d), "the last walker ended: defended, a refuge again")
        journal(20, "heard", 0, True, False, "heard of outside (the rumor's global): the quest says besieged, open")
        check(not any(m.group(1) == "20" and m.group(2) == "seen" and m.group(3) == "20" for m in told), "inside nothing new")
        journal(50, "defended", 20, False, True, "defended: the quest says so, completed")
        check(len(rewards) == 1 and "The refugees of Fort Ash are safe" in rewards[0], "the reward once" + (f" ({rewards[0][19:]})" if rewards else ""))
        check(any("site 0 Fort Ash (fort) attacked" in l for l in d), "a game day on the refuge is attacked again")
        journal(20, "seen", 50, True, False, "and the completed quest is opened again: besieged")
    else:
        taken = [l for l in d if "site 0 Fort Ash (fort) taken - now refuge" in l]
        check(len(taken) == 1, "the last walker ended: the place taken, a refuge" + (f" ({taken[0][-40:]})" if taken else ""))
        check(any("the kit to site 0 Fort Ash" in l and "refuge" in l for l in d), "and the kit of people moves in")
        journal(40, "heard", 0, True, False, "heard of outside (the rumor's global): the quest says the place is lost, open")
        journal(60, "taken", 40, False, True, "the last walker ended: the quest says taken, completed")
        check(len(rewards) == 1 and "Fort Ash is cleared - refugees move in" in rewards[0], "the reward once" + (f" ({rewards[0][19:]})" if rewards else ""))
for l in d[2:44]:
    print("        " + l[:210])
for l in [l for l in lines if l.startswith("Timing:   dungeons")][-8:]:
    print("        " + l)
sys.exit(1 if failures else 0)
PY
