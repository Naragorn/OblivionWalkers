#!/usr/bin/env bash
# Runs harness/whocomes.txt (docs/walkers-spec.md 4.24, 4.24a) with the
# switches set for the run, the INI put back whatever happens (md5
# checked); checks OblivionWalkers.log and the roster in OBVR.log. Needs
# tools/linux-walkers.sh on; announce "busy" first.
#
#   tools/whocomes-harness.sh zombies     [Walkers] Mode=1: only the zombie list gives walkers, the bandit stays down
#   tools/whocomes-harness.sh apocalypse  [Walkers] Mode=2: both lists give walkers, the bandit waits to rise
#   tools/whocomes-harness.sh off         Walkers 0, Daedra 0: the mod off - nothing replaced, nobody rises
#   tools/whocomes-harness.sh onboard     the onboarding answered Choose, Apocalypse, Daedra enemies, No necromancers, Start:
#                                         Walkers 2, Daedra 1, written to the INI (the INI copied into the run first)
#   tools/whocomes-harness.sh onboardoff  the onboarding answered "Turn the mod off", Start: all 0
set -euo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
obvr="${OBVR_DIR:-$root/../OBVR}"
game="${WALKERS_LINUX_GAME:-$HOME/.local/share/Steam/steamapps/common/Oblivion}"
ini="$game/Data/OBSE/Plugins/OblivionWalkers.ini"
scenario="${1:-zombies}"
backup="$(mktemp)"
after="$(mktemp)"
cp "$ini" "$backup"
sum="$(md5sum "$ini" | cut -c1-32)"
restore() {
	cp "$backup" "$ini"
	rm -f "$backup" "$after"
	[[ "$(md5sum "$ini" | cut -c1-32)" == "$sum" ]] && echo "whocomes-harness: INI restored (md5 $sum)" ||
		echo "whocomes-harness: INI NOT restored" >&2
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
walkers, daedra, show, answers = {
    "zombies": ("1", "0", "0", ""),
    "apocalypse": ("2", "0", "0", ""),
    "off": ("0", "0", "0", ""),
    # the start before the choice: the default switches, the boxes on
    "onboard": ("1", "0", "1", "0,1,1,0,0"),
    "onboardoff": ("1", "0", "1", "2,0"),
}[scenario]
for section, key, value in (("Walkers", "Mode", walkers), ("Daedra", "Mode", daedra),
                            ("Onboarding", "Show", show), ("Onboarding", "TestAnswers", answers),
                            ("Rise", "Enabled", "1"), ("Rise", "ChancePercent", "100")):
    text = put(text, section, key, value)
open(path, "w", encoding="latin-1", newline="").write(text)
INI
(cd "$obvr" && python3 tools/hand-script-run.py "$root/harness/whocomes.txt" --headless ${WALKERS_RUNNER_ARGS---no-steamvr} \
	--artifact-root "$root/artifacts/hand-script") > "$root/artifacts/whocomes-$scenario-runner.txt" 2>&1 || true
tail -1 "$root/artifacts/whocomes-$scenario-runner.txt"
cp "$ini" "$after"
run="$(ls -td "$root"/artifacts/hand-script/whocomes/*/ | head -1)"
cp "$game/OblivionWalkers.log" "$run/OblivionWalkers.log"
cp "$after" "$run/OblivionWalkers.ini.after"
echo "run: $run"
python3 - "$run" "$scenario" <<'PY'
import re, sys
run, scenario = sys.argv[1], sys.argv[2]
log = open(run + "/OblivionWalkers.log", encoding="latin-1").read().splitlines()
obvr = open(run + "/OBVR.log", encoding="latin-1").read().splitlines()
ini = open(run + "/OblivionWalkers.ini.after", encoding="latin-1").read()
failures = 0
def check(ok, what):
    global failures
    print(("  ok    " if ok else "  FAIL  ") + what)
    failures += 0 if ok else 1
def show(prefix, most=6):
    hits = [l for l in log if prefix in l]
    for l in hits[:most]:
        print("  log   " + l[:220])
    return hits
def section_key(section, key):
    m = re.search(r"^\[" + section + r"\]\s*$(.*?)(?=^\[|\Z)", ini, re.M | re.S)
    k = re.search(r"^" + key + r"=(.*)$", m.group(1), re.M) if m else None
    return k.group(1).strip() if k else None
roster = {}
mark = None
for l in obvr:
    m = re.search(r"HandScript: mark (\w+)", l)
    if m:
        mark = m.group(1)
    if "HandScript: actors" in l and mark:
        roster[mark] = l
counted = roster.get("counted", "")
print("  roster " + counted[:240])
# the bases on the roster (MOO rewrites LL0Goblin100 to its own goblins,
# so a goblin is any base that is neither a walker, a game zombie nor the bandit)
bases = re.findall(r"\b([0-9A-F]{8}) x\d+", counted)
# the game's zombies: Oblivion.esm's four and MOO's (it rewrites LL0ZombieVariant;
# seen 2026-11-20 in the mod-off run: 2310B240, 2310B242, 2310B246)
zombies = ("00025E6E", "00025E5E", "00025E55", "00025E4F", "2310B240", "2310B242", "2310B246")
walker = lambda b: re.match(r"2B000[89A][0-9A-F]{2}$", b) is not None
has_zombie = any(b in zombies for b in bases)
has_walker = any(walker(b) for b in bases)
has_goblin = any(not walker(b) and b not in zombies and b != "00069AD1" for b in bases)
print("  bases  %s" % bases)
mods = show("Mod: ")
spawns = show("Spawns: Walkers Mode=") + show("Spawns: Walkers Mode=0 Daedra Mode=0")
summary = show("leveled creature lists (Enabled=")
bandit = show("00069AD1") or show("Rise: ")
verdicts = [l for l in log if "stays down" in l or "waits to rise" in l]
for l in verdicts[:4]:
    print("  rise  " + l[:200])
if scenario == "zombies":
    check(any("Walkers Mode=1" in l for l in mods), "the start says Walkers Mode=1")
    check(has_walker and not has_zombie, "the zombie list gave walkers, no game zombie left")
    check(has_goblin, "the goblins stayed goblins")
    check(any("Walkers Mode 1" in l for l in verdicts) and not any("waits to rise" in l for l in verdicts),
          "the bandit stays down (Walkers Mode 1)")
elif scenario == "apocalypse":
    check(any("Walkers Mode=2" in l for l in mods), "the start says Walkers Mode=2")
    check(has_walker and not has_zombie and not has_goblin, "both lists gave walkers")
    check(any("waits to rise" in l for l in verdicts), "the bandit waits to rise")
elif scenario == "off":
    check(any("the mod is off" in l for l in mods), "the start says the mod is off")
    check(has_zombie and has_goblin and not has_walker, "the game's zombies and goblins, no walker")
    check(not any("waits to rise" in l for l in verdicts), "nobody waits to rise")
else:
    pages = show("Onboarding: page", 8)
    answered = show("Onboarding: answered", 8)
    chosen = show("Onboarding: chosen", 2)
    want_w, want_d = ("2", "1") if scenario == "onboard" else ("0", "0")
    check(len(pages) >= (4 if scenario == "onboard" else 2), "the pages shown (%d)" % len(pages))
    check(len(answered) >= (4 if scenario == "onboard" else 2), "the answers taken (%d)" % len(answered))
    check(any(("Walkers %s Daedra %s" % (want_w, want_d)) in l for l in chosen), "chosen Walkers %s Daedra %s" % (want_w, want_d))
    got = (section_key("Walkers", "Mode"), section_key("Daedra", "Mode"), section_key("Onboarding", "Show"))
    check(got == (want_w, want_d, "0"), "the INI after the run: Walkers Mode=%s Daedra Mode=%s Show=%s" % got)
    if scenario == "onboard":
        check(has_walker and not has_goblin, "the choice applied at once: the goblins became walkers")
    else:
        check(not has_walker, "the choice applied at once: no walker placed")
print("whocomes %s: %s" % (scenario, "PASS" if failures == 0 else "FAIL (%d)" % failures))
sys.exit(1 if failures else 0)
PY
