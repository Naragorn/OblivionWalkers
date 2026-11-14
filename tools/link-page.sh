#!/usr/bin/env bash
# Runs harness/link-page.txt on OBVR's harness (the Linux game, headless)
# with [Debug] TestLinkPage=24 in the live plugin INI, put back whatever
# happens: LINK's page of the plugin shown and left by its Return button's
# script. Exit 0: the one value set between two positions was cut to the
# lower one (the page wrote its positions back) and no other setting changed.
# Announce "busy" to the other sessions first.
set -euo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
obvr="${OBVR_DIR:-$root/../OBVR}"
game="${WALKERS_LINUX_GAME:-$HOME/.local/share/Steam/steamapps/common/Oblivion}"
ini="$game/Data/OBSE/Plugins/OblivionWalkers.ini"
backup="$(mktemp)"
cp "$ini" "$backup"
trap 'cp "$backup" "$ini"; rm -f "$backup"' EXIT
if grep -q '^\[Debug\]' "$ini"; then
	sed -i '0,/^\[Debug\]\r\?$/s//[Debug]\nTestLinkPage=24/' "$ini"
else
	printf "\n[Debug]\nTestLinkPage=24\n" >> "$ini"
fi
# The held dungeons off: their runner's news would open the journal's box.
if grep -q '^\[Dungeons\]' "$ini"; then
	sed -i '0,/^\[Dungeons\]\r\?$/s//[Dungeons]\nEnabled=0/' "$ini"
else
	printf "\n[Dungeons]\nEnabled=0\n" >> "$ini"
fi
(cd "$obvr" && python3 tools/hand-script-run.py "$root/harness/link-page.txt" --headless \
	--artifact-root "$root/artifacts/hand-script") > "$root/artifacts/link-page-runner.txt" 2>&1 || true
tail -1 "$root/artifacts/link-page-runner.txt"
run="$(ls -td "$root"/artifacts/hand-script/link-page/*/ | head -1)"
cp "$game/OblivionWalkers.log" "$run/OblivionWalkers.log"
echo "run: $run"
python3 - "$run/OblivionWalkers.log" "$run/OBVR.log" <<'PY'
import sys
lines = open(sys.argv[1], encoding="latin-1").read().splitlines()
obvr = open(sys.argv[2], encoding="latin-1").read()
failures = 0

def check(ok, what):
    global failures
    print(("  ok    " if ok else "  FAIL  ") + what)
    failures += 0 if ok else 1

link = [l for l in lines if l.startswith("LINK: ")]
last = max((k for k, l in enumerate(link) if "first look" in l), default=-1)
link = link[last:] if last >= 0 else link
check(last >= 0 and "196 of 196 variables" in link[0], "LINK is loaded, the 196 variables there" + (f" ({link[0][:110]})" if link else ""))
shown = [k for k, l in enumerate(link) if "test - the plugin's page shown" in l and "called: 1" in l]
left = [k for k, l in enumerate(link) if "test - the page left by its Return" in l and "called: 1" in l]
check(len(shown) == 1 and len(left) == 1 and shown[0] < left[0], "the page is shown and left, each by LINK's own script")
check("menu Generic" in obvr, "a generic menu was open (OBVR.log) - LINK's page")
now = [(k, l) for k, l in enumerate(link) if " now " in l and l.startswith("LINK: ini_")]
before = [l for k, l in now if not shown or k < shown[0]]
after = [l for k, l in now if left and k > left[0]]
between = [l for k, l in now if shown and left and shown[0] < k < left[0]]
check(before == ["LINK: ini_grab_bite_seconds now 4.3"], f"before the page: the console's value between two positions, 4.3 ({before})")
check(not between, f"while the page is open nothing changes ({len(between)})")
check("LINK: ini_grab_bite_seconds now 4.0" in after, "left by Return: LINK wrote its position's value back, 4.0 - the page does write")
others = [l for l in after if "ini_grab_bite_seconds" not in l]
check(not others, f"and no other setting changed ({len(others)} others)")
for l in [l for k, l in now][:16] + [l for l in link if "test - " in l]:
    print("        " + l[:150])
sys.exit(1 if failures else 0)
PY
