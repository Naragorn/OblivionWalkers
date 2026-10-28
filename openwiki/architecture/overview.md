---
type: architecture
title: Architecture overview
description: The parts of Oblivion Walkers - the ESP, the DLL and the INI - and how the DLL is put together - the xOBSE entry, InstallRise, the frame tick and its modules, the event handlers, the co-save, and the pure Logic headers.
tags: [architecture, xobse, tick, co-save, testing-pattern]
verified:
  - by: openwiki/0.7.2
    at: 2026-10-28T07:25:28.767Z
sources:
  - id: openwiki-source-ea70eb6c045047448e446296
    resource: repo://.gitignore
  - id: openwiki-source-0211f2f5f95441e40340330f
    resource: repo://src/Plugin.cpp
  - id: openwiki-source-379b78b7e8603f820e9adcec
    resource: repo://src/PluginEntry.h
  - id: openwiki-source-357b60e66710e88e89e848ac
    resource: repo://src/Rise.cpp
  - id: openwiki-source-5b58748400c85217d70bfc2b
    resource: repo://tools/esp-write.py
generated: { by: "claude-code", at: "2026-10-28T07:25:28.767Z" }
---

# Architecture overview

Spec: `docs/walkers-spec.md` section 2 (the engine facts each feature rests on) and 3.

## Three parts

| Part | What |
| --- | --- |
| `OblivionWalkers.esp` | records only, no Construction Set: the Walker race, 64 walker creatures, the spawn lists, horde packages and markers, gate guard pool, Courier editions, the LINK quest, the grab's idles - written by `tools/esp-write.py` from Oblivion.esm (and MOO's ESP for its looks), never committed |
| `OblivionWalkers.dll` | the xOBSE plugin: everything at runtime |
| `OblivionWalkers.ini` | every setting, beside the DLL in `Data/OBSE/Plugins` |

## The DLL

- `src/Plugin.cpp` - the exports, the log (`OblivionWalkers.log`, the last session's kept as `.prev`), reading the INI into `walkers::Settings`, then `InstallRise`.
- `src/PluginEntry.cpp/.h` - what happens at query and load, behind a host so every flow is unit-tested: Oblivion 1.2.416 only, xOBSE 21 or later.
- `src/Rise.cpp` - the hub. `InstallRise` takes xOBSE's interfaces (event manager, tasks, serialization, console, messaging), enqueues the frame tick, registers OnDeath and OnHit, the save/load/new-game callbacks with the opcode base 0x2B00, and configures the grab.
- `Tick()` runs every main-loop pass: finds the ESP's walkers and MOO once, then steps leveling checks, LINK, MOO stripping, spawns, the walk, the grab, the horde and gate guards, the safeguard, the news, loot and fame, the swap in flight and the waiting list.
- Modules beside it: `Replace.cpp` (spawn lists), `Horde.cpp` (groups, guards), `News.cpp` (the Courier), `Pose.cpp` (the walker laid into the corpse's pose), `Grab.cpp` (the grab), `QuestVars.cpp` (LINK's variables).
- `src/Engine.h` - the few engine accessors and addresses shared by the modules (LookupForm, the mod list).
- `src/Obse.h` - the xOBSE interface structures the plugin uses.

## Rules the code follows

- Nothing of an actor is touched inside the engine's death or hit path: handlers only note, the next tick acts.
- What must survive a save (who waits to rise, the risen, fame, the safeguard's list, the news) goes into the co-save with reference IDs resolved at load; everything else is redone each session.
- Decisions live in pure headers (`RiseLogic.h`, `ReplaceLogic.h`, `HordeLogic.h`, `GuardLogic.h`, `SafeguardLogic.h`, `NewsLogic.h`, `PoseLogic.h`, `ApartLogic.h`, `LinkLogic.h`, `SkillLogic.h`, `LevelingLogic.h`, `GrabLogic.h`, ...), plain values in, plain values out; each has a test program in `tests/`.
- Console lines go through xOBSE's `RunScriptLine2` with the reference as `this` (`ConsoleOn`).
