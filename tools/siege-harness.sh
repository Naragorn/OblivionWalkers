#!/usr/bin/env bash
# Runs harness/siege-full.txt (docs/walkers-spec.md 4.12) on OBVR's harness
# with the [Siege] lines it needs in the live plugin INI of the Linux game,
# puts that INI back whatever happens, and checks OblivionWalkers.log in
# order - the runner itself reads OBVR.log only. Exit 0: every check held.
#
#   tools/siege-harness.sh            (OBVR's checkout next to this one)
#
# Announce "busy" to the other sessions first: SteamVR takes one scene app.
set -euo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
obvr="${OBVR_DIR:-$root/../OBVR}"
game="${WALKERS_LINUX_GAME:-$HOME/.local/share/Steam/steamapps/common/Oblivion}"
ini="$game/Data/OBSE/Plugins/OblivionWalkers.ini"
log="$game/OblivionWalkers.log"
backup="$(mktemp)"
cp "$ini" "$backup"
trap 'cp "$backup" "$ini"; rm -f "$backup"' EXIT
printf '\r\n[Siege]\r\nTestGate=1\r\nChancePerDayPercent=100\r\nWalkersMin=80\r\nWalkersMax=80\r\nGuardKillsPerHour=2\r\n' >> "$ini"
(cd "$obvr" && python3 tools/hand-script-run.py "$root/harness/siege-full.txt" --headless \
	--artifact-root "$root/artifacts/hand-script") | tee "$root/artifacts/siege-full-runner.txt"
run="$(ls -td "$root"/artifacts/hand-script/siege-full/*/ | head -1)"
cp "$log" "$run/OblivionWalkers.log"
python3 - "$run/OblivionWalkers.log" <<'PY'
import sys
lines = open(sys.argv[1], encoding="latin-1").read().splitlines()
siege = [l for l in lines if l.startswith(("Siege:", "News:", "Co-save:"))]
failures = 0

def check(ok, what):
    global failures
    print(("  ok    " if ok else "  FAIL  ") + what)
    failures += 0 if ok else 1

def count(text):
    return sum(1 for l in siege if text in l)

def order(*texts):
    at = 0
    for t in texts:
        while at < len(siege) and t not in siege[at]:
            at += 1
        if at == len(siege):
            return False
        at += 1
    return True

check(count("TestGate=1 begins a siege") >= 1, "the test switch begins Anvil's siege")
check(count("Anvil Castle Gate besieged - 80 walkers") >= 1 and count("its gate open") >= 8, "the gates stay open (Nadi, 2026-11-04: the player helps)")
check(count("besieged - 80 walkers") >= 8, "eight cities besieged (the test one and the day's roll), 80 walkers each")
check(count("rumor global") >= 8 and count("now 1") >= 8, "the rumors set for the besieged")
check(count("edition 18 ") >= 1 or count("(the siege of") >= 1, "the Courier on a siege edition")
check(order("the player at Anvil Castle Gate - its walkers come out (",
            "out before Anvil Castle Gate", "ended before Anvil Castle Gate"),
      "the walkers out before the gate, then counted off as they end")
check(order("sieges on read", "its walkers come out"),
      "at the load: the sieges read, the walkers out again")
check(count("1 sieges on read") >= 1, "Anvil's siege in the co-save")
check(count("back in the pool") >= 2, "the pool back in on leaving (twice)")
check(count("is over") >= 8, "every siege over after the toll")
check(count(" locked - read back") == 0 and count("locked again") == 0, "nothing locked")
check(count("(the siege lifted at") >= 1, "the Courier on a lifted edition")
check(count("no rumor global") == 0 and count("not found - not locked") == 0, "no global or door missing")
print("PASS" if failures == 0 else f"FAIL ({failures})")
sys.exit(1 if failures else 0)
PY
