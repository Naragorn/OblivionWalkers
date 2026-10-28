# OpenWiki brief

Oblivion Walkers is a zombie overhaul for The Elder Scrolls IV: Oblivion (2006, 1.2.416): an xOBSE plugin DLL plus an ESP written by script from Oblivion.esm. The dead rise as walkers, leveled spawns become walkers, hordes drift toward the cities and gate guards come out, the Black Horse Courier reports it, and walkers can grab and bite the player.

## Scope and priorities

- The architecture of `src/`: the frame tick in `Rise.cpp` and the modules it drives (`Replace`, `Horde`, `News`, `Pose`, `Grab`, `QuestVars`), and the pattern that keeps decisions testable - pure `*Logic.h` units over plain values, tested in `tests/` without the game.
- The ESP and the animations, written by `tools/esp-write.py` and `tools/grab-anims.py` from the game's own data, never committed.
- Configuration: `OblivionWalkers.ini` and the LINK menu (`src/LinkLogic.h`), with the rule in `AGENTS.md` that every setting LINK can show is in LINK.
- Building, deploying and testing: `tools/build-linux.sh`, the unit tests, the harness scenarios in `harness/` run on OBVR's harness.

## How to treat the existing docs

- `docs/walkers-spec.md` is the design spec and decision record per feature (numbered sections, open bugs in 7a). Link to it for the why instead of restating it; where it and the source disagree, the source and tests are authoritative.

## Out of scope

Harness run output (`artifacts/`), build directories, the generated ESP.
