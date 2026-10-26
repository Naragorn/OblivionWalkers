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
  records: the Walker race for the risen, the walkers, the horde's packages
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

## Not planned for now

- **Attack tempo** (docs/walkers-spec.md 4.16): slower walker blows. The
  engine fires an attack's keys twice when its time is moved back, and the
  fix OBVR uses patches the one call that fires them (0x00477AB1); a second
  patch there would collide with OBVR's. Maybe later, through OBVR (a factor
  per walker handed over by plugin messaging) - the spec has the ways.

## Known conflicts

- **Maskar's Oblivion Overhaul (MOO): walkers that never stay dead.** MOO
  gives every creature with a zombie model (`\Zombie\`) a brain in its
  inventory (70 % by default) and raises a dead zombie again while it still
  carries one (MOOAddZombieBrainFunctionScript and MOOTokenZombieScript in
  its ESP). The walkers wear the zombie models, so MOO takes them for
  zombies - and with `[Zombies] NoLoot=1` nobody can take the brain from the
  corpse. Handled since 2026-10-25: with `[Zombies] BlockMooResurrect=1`
  (the default) a dead walker loses MOO's zombie token and brain right after
  its death, so MOO leaves it down and MOO's INI can stay as it is - MOO's
  own zombies keep MOO's behaviour. With `0`, set
  `set MOO.ini_zombie_resurrect to 0` in `Maskar's Oblivion Overhaul.ini`
  instead (MOO's own zombies stay down then too).
