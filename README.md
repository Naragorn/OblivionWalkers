# Oblivion Walkers

A zombie apocalypse for Oblivion (the 2006 one), built to sit beside
[OBVR](https://github.com/Naragorn/OBVR) but not depending on it: an xOBSE
plugin plus a small ESP. The dead get up, spawns become walkers, cities get
besieged, and everything has a switch in an INI.

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
  records: the Walker race for the risen. The ESP is deployed beside the DLL
  and must be active in the game's plugin list.
- `artifacts/` - what the tools and the runs wrote (not for a repository).

Own folder for now; it becomes its own repository later.

## Before a public release

- **Register the opcode base.** The plugin sets opcode base `0x2B00`
  (`src/Rise.cpp`, `kOpcodeBase`): xOBSE runs a plugin's co-save callbacks
  only for one with an opcode base of its own (docs/walkers-spec.md,
  section 2). `0x2B00` was free in xOBSE's list
  (github.com/llde/xOBSE/wiki/Registered-Opcodes-for-OBSE-Plugins) on
  2026-10-24 but is not registered there yet. Before the mod goes public,
  ask for it to be registered there (or take the base it is given and change
  `kOpcodeBase`), so no other plugin picks the same one.
