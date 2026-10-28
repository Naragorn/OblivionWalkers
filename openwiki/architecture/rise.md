---
type: architecture
title: The dead rise
description: How a fallen person becomes a walker - the death watch, the waiting list and its clocks, the swap for a Walker creature laid into the corpse's pose, the looks, the walk, chapels and loot.
tags: [rise, swap, pose, waiting-list, co-save, looks]
verified:
  - by: openwiki/0.7.2
    at: 2026-10-28T09:48:19.681Z
sources:
  - id: openwiki-source-242aafd6790611ca390b2a6f
    resource: repo://src/MissingLogic.h
  - id: openwiki-source-34ff7b2b35843613f6b270fa
    resource: repo://src/PoseLogic.h
  - id: openwiki-source-357b60e66710e88e89e848ac
    resource: repo://src/Rise.cpp
  - id: openwiki-source-4600e3a2d965112f438bb37e
    resource: repo://src/RiseLogic.h
  - id: openwiki-source-591fd519d61fca7102934654
    resource: repo://src/WalkerLooks.h
generated: { by: "claude-code", at: "2026-10-28T09:48:19.681Z" }
---

# The dead rise

Spec: `docs/walkers-spec.md` 4.1 (the rise and the swap), 4.2, 4.4, 4.7, 4.15, 4.11b (missing bodies). Settings: `[Rise]` and `[Zombies]`.

## From death to waiting

- xOBSE's OnDeath (`Rise.cpp OnDeath`) only notes the fallen: whether it may rise (`OnlyGeneric`, `OnlyPlayableRaces`, `OnlyHumans`, `ChancePercent`, chapels by `IsChapelCell`) is decided in `RiseLogic.h`; nothing of the actor is touched in the death path.
- The waiting list (`RiseLogic.h`, `WaitingList`, `TickWaiting`) gives each body `AfterSeconds` plus up to `JitterSeconds`. A clock runs only while the body is present in the player's cells (`InPlayersCells`, uGridsToLoad read from the game); a body the game does not hold right now is kept "missing" for up to three game days (`MissingLogic.h`).
- The list goes into the co-save (record `WAIT`), reference IDs resolved at load.

## The swap

With `AsCreature=1` (the default) a due body is swapped for one of the ESP's Walker creatures, stepped by `StepSwap` through its stages: the walker placed, its model awaited, dropped and killed unseen, laid into the corpse's place and pose (`MatchCorpse`, `Pose.cpp` / `PoseLogic.h`: the ragdoll bodies retargeted bone by bone), the two exchanged, then raised with the engine's own Resurrect (vtable +0x20C, animate=1) so the get-up animation plays. `KeepCorpseApart` (`ApartLogic.h`) keeps the corpse's ragdoll out of the walker's way. The corpse's inventory goes to the walker; the corpse is disabled.

## Looks and pace

- `WalkerLooks.h`: 64 walker creatures - the twenty vanilla zombie part sets and 44 of MOO's (used only with MOO loaded; the risen drawn by race).
- `[Zombies] Walk`: every frame a run in a walker's HighProcess movement flags (+0x1FC) becomes a walk (`WalkInsteadOfRun`); `SpeedMultiplier` and `SpeedRandomPercent` set the records' Speed at start.
- `HealthPercent`, `RenameToWalker`/`Name` are written on the records in memory at start.

## Chapels and loot

- `AvoidChapels`: no rise in a chapel; a walker made in one is taken out of the world (`StepChapel`).
- `NoLoot`: a dead walker cannot be searched - the risen, who carry the fallen's gear, stay lootable (`RisenSet`, co-save record).
