#!/usr/bin/env python3
# Checks OblivionWalkers.log after harness/link-save-load.txt. The game loads
# the harness's own save first (once or twice, OBVRHarness - it may hold no
# Walkers values: version 0, EMPTY is right there), then the script's two
# "load WalkersLinkTest". Only those last two are judged: each must read a
# version, its values set, and change at most five values after it.
# Exit 0: PASS.
import sys

RELOADS = 2

lines = open(sys.argv[1], encoding="latin-1").read().splitlines()
looks = [i for i, l in enumerate(lines) if l.startswith("LINK: first look")]
failures = 0
for n, i in enumerate(looks):
    end = looks[n + 1] if n + 1 < len(looks) else len(lines)
    changes = [l for l in lines[i:end] if l.startswith("LINK: ini_") and " now " in l]
    decided = [l for l in lines[i:end] if l.startswith("LINK: loaded") or l.startswith("LINK: not loaded")]
    judged = n >= len(looks) - RELOADS
    bad = judged and ("values EMPTY" in lines[i] or " version 0," in lines[i] or len(changes) > 5)
    failures += 1 if bad else 0
    tag = ("FAIL " if bad else "ok   ") if judged else "seen "
    print("%sload %d: %s" % (tag, n + 1, lines[i]))
    for l in decided[:1]:
        print("             " + l)
    print("             %d value changes after it%s" % (len(changes), (": " + "; ".join(changes[:4])) if changes else ""))
enough = len(looks) >= RELOADS + 1
print("FAIL" if failures or not enough else "PASS",
      "- %d loads seen, the last %d judged" % (len(looks), RELOADS))
sys.exit(1 if failures or not enough else 0)
