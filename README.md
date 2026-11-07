# Oblivion Walkers

A zombie apocalypse for Oblivion (the 2006 one), built to sit beside
[OBVR](https://github.com/Naragorn/OBVR) but not depending on it: an xOBSE
plugin plus a small ESP. The dead are now rising as walkers, spawns become walkers, cities get
besieged by hordes, and everything has a switch in an INI.

What exists:

- `docs/walkers-spec.md` - the design, the engine facts it rests on, every
  feature with its setting, cost and test, and the open questions.
- `src/` - the xOBSE plugin, OblivionWalkers.dll: `Plugin.cpp` holds the
  exports, the log and the INI; `PluginEntry` what happens at load;
  `RiseLogic.h` the rise's decisions as pure functions; `Rise.cpp` the death
  watch, the frame tick, the co-save and the engine's Resurrect.
  `OblivionWalkers.ini` is the INI as deployed.
- `tests/` - the unit tests, one flow each, built 32-bit natively.
- `tools/build-linux.sh` - tests, then the DLL with clang-cl against xwin's
  winsysroot; `--deploy` copies it into the games (not while Oblivion runs).
- `harness/` - scenarios for OBVR's hand script harness: the engine measured
  before the plugin (a leveled list's count at a world spawn point, a corpse
  raised by the console) and the plugin's own rise.
- `tools/esm-spawns.py` - lists the leveled creature lists and the spawn
  points of Oblivion.esm, so replacement targets are chosen from data.
- `tools/esp-write.py` - writes `OblivionWalkers.esp` from Oblivion.esm's own
  records (and the part sets of Maskar's Oblivion Overhaul's zombies, read
  off its ESP, which it takes as its second argument): the Walker race for
  the risen, the 64 walker looks, the horde's packages
  and markers, the gate guards with their pool, and the Courier's editions.
  The ESP is deployed
  beside the DLL and must be active in the game's plugin list.
- `tools/city-gates.py` - reads where the city gates are and the spot in
  front of each, the rows of `src/CityGates.h`.
- `tools/courier-editions.py` - the 18 editions of the Black Horse Courier
  (the walkers' news), imported by `tools/esp-write.py`.
- `tools/courier-refs.py` - reads the broadsheets in the inns, taverns and
  the Courier's office, the rows of `src/CourierRefs.h`.
- `artifacts/` - what the tools and the runs wrote (not for a repository).

Building on Windows: 32-bit MSVC, `cmake -B build-win -G Ninja
-DCMAKE_BUILD_TYPE=Release .`, `cmake --build build-win`, `ctest --test-dir
build-win` (from a vcvars32 prompt) - the release DLL. The Nexus page's text
is `docs/nexus.md`.

## Before a public release

- **Two downloads**: the ESL one first, needing
  OblivionESL (Nexus 56578) and Cell Lookup Robustness (Nexus 55924), and the
  plain ESP beside it. `tools/package-release.sh <version>` writes both into
  `dist/` from the game's own data (the ESP and the grab's animations are
  built on the machine, never committed), each with a README naming its
  requirements and what a switch between the two costs (the walkers already
  risen). The DLL in them is the MSVC build in `build-win/` when there is one,
  else the one in `build-linux/`.

- **Register the opcode base.** The plugin sets opcode base `0x2B00`
  (`src/Rise.cpp`, `kOpcodeBase`): xOBSE runs a plugin's co-save callbacks
  only for one with an opcode base of its own (docs/walkers-spec.md,
  section 2). `0x2B00` was free in xOBSE's list
  (github.com/llde/xOBSE/wiki/Registered-Opcodes-for-OBSE-Plugins) on
  2026-10-24 and still on 2026-11-03. Requested on 2026-11-03 as
  0x2B00-0x2B0F (https://github.com/llde/xOBSE/issues/263); open until
  it is in the list. Should another range be given, change `kOpcodeBase`.

## Not planned for now

- **Attack tempo** (docs/walkers-spec.md 4.16): slower walker blows. The
  engine fires an attack's keys twice when its time is moved back, and the
  fix OBVR uses patches the one call that fires them (0x00477AB1); a second
  patch there would collide with OBVR's. Maybe later, through OBVR (a factor
  per walker handed over by plugin messaging) - the spec has the ways.

## Known conflicts

- **Maskar's Oblivion Overhaul (MOO): is now compatible.

## License

**GNU General Public License v3** (`LICENSE`). Copyright (C) 2026 Naragorn.

In short: use it, change it, share it, build on it. If you pass it on, pass the
source on with it under the same license. No closed or paid-for versions. The
license text is what counts; this is just the summary.

`src/Obse.h` copies the layout of xOBSE's plugin interface structs so the
plugin can load, like every OBSE plugin does. The ESP and the grab's
animations are written from the game's own data on your machine
(`tools/esp-write.py`, `tools/grab-anims.py`) and are not part of this
repository.
