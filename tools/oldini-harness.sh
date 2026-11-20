#!/usr/bin/env bash
# Runs harness/oldini.txt with an INI of only the old names (IniNames.h):
# [Zombies] HealthPercent=37, [Replace] MinCount=2 MaxCount=3, no [Walkers]
# and no WalkersPerSpawnMin/Max. The INI is put back whatever happens (md5
# checked). Needs tools/linux-walkers.sh on; announce "busy" first.
set -euo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
obvr="${OBVR_DIR:-$root/../OBVR}"
game="${WALKERS_LINUX_GAME:-$HOME/.local/share/Steam/steamapps/common/Oblivion}"
ini="$game/Data/OBSE/Plugins/OblivionWalkers.ini"
backup="$(mktemp)"
cp "$ini" "$backup"
sum="$(md5sum "$ini" | cut -c1-32)"
restore() {
	cp "$backup" "$ini"
	rm -f "$backup"
	[[ "$(md5sum "$ini" | cut -c1-32)" == "$sum" ]] && echo "oldini-harness: INI restored (md5 $sum)" ||
		echo "oldini-harness: INI NOT restored" >&2
}
trap restore EXIT
python3 - "$ini" <<'INI'
import re, sys
path = sys.argv[1]
text = open(path, encoding="latin-1", newline="").read()
nl = "\r\n" if "\r\n" in text else "\n"
for new in ("[Walkers]",):
    if re.search(r"^" + re.escape(new) + r"\s*$", text, re.M):
        sys.exit("oldini-harness: the INI has %s - not an old one" % new)
if re.search(r"^WalkersPerSpawnM(in|ax)=", text, re.M):
    sys.exit("oldini-harness: the INI has the new WalkersPerSpawn keys")
text = re.sub(r"^HealthPercent=.*$", "HealthPercent=37", text, count=1, flags=re.M)
text = re.sub(r"^MinCount=.*$", "MinCount=2", text, count=1, flags=re.M)
text = re.sub(r"^MaxCount=.*$", "MaxCount=3", text, count=1, flags=re.M)
open(path, "w", encoding="latin-1", newline="").write(text)
INI
source "$root/tools/walkers-ini-mode.sh"
walkers_onboarding_off "$ini"  # 4.24a: no box over the run (an [Onboarding] section is no [Walkers])
grep -n "^\[Zombies\]\|^HealthPercent=\|^MinCount=\|^MaxCount=" "$ini"
(cd "$obvr" && python3 tools/hand-script-run.py "$root/harness/oldini.txt" --headless ${WALKERS_RUNNER_ARGS---no-steamvr} \
	--artifact-root "$root/artifacts/hand-script") > "$root/artifacts/oldini-runner.txt" 2>&1 || true
run="$(ls -td "$root"/artifacts/hand-script/oldini/*/ | head -1)"
cp "$game/OblivionWalkers.log" "$run/OblivionWalkers.log"
python3 - "$run/OblivionWalkers.log" <<'PY'
import re, sys
lines = open(sys.argv[1], encoding="latin-1").read().splitlines()
failures = 0
def check(ok, what):
    global failures
    print(("  ok    " if ok else "  FAIL  ") + what)
    failures += 0 if ok else 1
health = [l for l in lines if re.match(r"Walkers: \w+ health \d+ \(\d+% of \d+\)", l)]
counts = [l for l in lines if "WalkersZombieVariant" in l and "counts" in l]
for l in health[:2] + counts[:1]:
    print("        " + l[:200])
check(bool(health) and all("(37% of" in l for l in health), "[Zombies] HealthPercent=37 read (%d walkers)" % len(health))
m = re.search(r"counts ([\d ]+) \(", counts[0]) if counts else None
got = set(m.group(1).split()) if m else set()
check(got == {"2", "3"}, "[Replace] MinCount=2 MaxCount=3 read: the slots' counts %s" % sorted(got))
print("oldini-harness: %s" % ("PASS" if failures == 0 else "FAIL (%d)" % failures))
sys.exit(1 if failures else 0)
PY
