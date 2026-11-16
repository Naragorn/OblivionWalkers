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
#   tools/daedra-harness.sh siege [Mode]     harness/daedra-siege.txt: a Daedra siege at Anvil, its dead banished (4.22a 5a, 5b)
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
if scenario in ("band", "save"):
    text = set_key(text, "Invaders", "OnlyFromOblivionGates", "1")
    text = set_key(text, "Invaders", "TestBandSeconds", "20")
if scenario == "siege":
    for key, value in (("TestGate", "1"), ("TestDaedra", "1"), ("DaedraMin", "6"), ("DaedraMax", "6"),
                       ("DaedraAtGate", "4"), ("ChancePerDayPercent", "0")):
        text = set_key(text, "Siege", key, value)
    text = set_key(text, "Invaders", "BanishSeconds", "5")
# the dungeons' runner off, as in the other wrappers: nobody of the plugin's own beside the player
text = set_key(text, "Dungeons", "Enabled", "0")
open(path, "w", encoding="latin-1", newline="").write(text)
INI
mkdir -p "$root/artifacts"
(cd "$obvr" && python3 tools/hand-script-run.py "$root/harness/daedra-$scenario.txt" --headless \
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
if kind != 0 and scenario not in ("band", "save", "siege"):
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
if scenario == "siege":
    show("Siege: ", 40)
    show("Placer: ", 30)
    show("Banish: ", 30)
    check(any("its Daedra come out" in l for l in lines), "the player at the gate: a Daedra siege shown")
    came = [int(m.group(1)) for m in (re.search(r"Siege: (\d+) Daedra came to the siege", l) for l in lines) if m]
    check(sum(came) >= 4, "the placer brought the Daedra (%s)" % came)
    check(sum(1 for l in lines if "a Daedra fell before Anvil Castle Gate" in l) >= 4, "the fallen counted off")
    check(any("back to Oblivion" in l for l in lines), "a dead Daedra banished")
    bags = [l for l in lines if re.search(r"into bag FF[0-9A-F]{6} \(1\)", l)]
    check(bool(bags), "its things into a sack (%d)" % len(bags))
    check(any("handed to DeleteReference" in l for l in lines), "bodies deleted")
    check(any("Anvil Castle Gate is over" in l or "the siege at Anvil Castle Gate is over" in l for l in lines),
          "the siege over once all fell")
print("daedra-harness: %s" % ("PASS" if failures == 0 else "FAIL (%d)" % failures))
sys.exit(1 if failures else 0)
PY
