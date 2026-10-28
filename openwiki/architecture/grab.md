---
type: architecture
title: Grabbed by walkers
description: How a walker's hit grabs the player's arm, how the hold slows and roots the player, the bites, the ways to break free, the animations and the messages to OBVR.
tags: [grab, combat, animation, flat, vr]
verified:
  - by: openwiki/0.7.2
    at: 2026-10-28T09:48:19.681Z
sources:
  - id: openwiki-source-c9e8e6f0f6d76658fa87e40a
    resource: repo://src/Grab.cpp
  - id: openwiki-source-606b175a4ff2f0beff54c094
    resource: repo://src/GrabLogic.h
  - id: openwiki-source-357b60e66710e88e89e848ac
    resource: repo://src/Rise.cpp
  - id: openwiki-source-3e7b12a2e775e6053538ff99
    resource: repo://tools/grab-anims.py
generated: { by: "claude-code", at: "2026-10-28T09:48:19.681Z" }
---

# Grabbed by walkers

Spec: `docs/walkers-spec.md` section 4.19 (the design, the decisions and the harness runs). This page maps the code.

## Split

| File | Role |
| --- | --- |
| `src/GrabLogic.h` | every decision as pure functions over plain values: `TryGrab`, `HoldOf`, `HeldPlayerFlags`, `HolderFlags`, `StepGrab` (let-go and bites), `OnStruck`, `BiteDamage`, `StrugglePressesNeeded`, `StepStruggle`, `GrabText`, `GrabMessage` |
| `src/Grab.cpp` | the engine side: queued hits, flags written each frame, idles played, damage through the console, the struggle key read, the messages to OBVR |
| `src/Grab.h` | `GrabServices` - what `Rise.cpp` lends the grab (console line, dead check, walker check, high process, random roll, nearest walker, walkers near) |
| `tools/grab-anims.py` | writes the three KF animations from the game's own zombie animations |

## Flow of a frame

1. xOBSE's `OnHit` handler in `Rise.cpp` hands every hit to `GrabOnHit`, which only queues it - nothing of an actor is touched inside the engine's hit path.
2. `StepGrabInGame`, called from the rise's frame tick, handles the queued hits: a living walker's hit on the player calls `TryGrab` (the arm on the walker's side from `SideOf`, at most two holders, by `ChancePercent`); the player's hit on a holder counts toward `StruggleHits` via `OnStruck`.
3. The struggle key (flat): the control's state is read from OSInputGlobals; `StepStruggle` counts presses against `StrugglePressesNeeded` (base x crowd x tiredness) and lets the longest holder go when the bar is full.
4. `StepGrab` lets holders go (cut down, gone, torn free beyond `TearFreeDistance`) and runs each holder's bite clock; the last of `BitesToDie` bites ends the hold and every holder lets go.
5. Every frame the holders' HighProcess movement flags lose steps and pace, the player's lose the run (one holder) or every step (two) - nothing of a hold is in the save.

## Animations

`grab-anims.py` reads `Oblivion - Meshes.bsa` and writes `WalkersGrabHold.kf` (the upper body of `Walk01ArmsUp.kf`, a loop at priority 70, the upper arms turned so the hands close before the body - `GRIP_TARGETS`), `WalkersGrabBite.kf` (the forward power attack's spine and head with the hold's arms) and `WalkersGrabLetGo.kf` (`Stagger.kf`). They go beneath both zombie skeletons' `IdleAnims` folders; the ESP's IDLE records 0x94D..0x952 name them with a condition that is never true, and `Grab.cpp` queues them the way xOBSE's `PlayIdle` does.

## Damage and messages

A bite takes `BiteHealthPercent` of the base health through `ModAV2 Health`; a bite that leaves nothing runs `Kill`; god mode leaves a point. With `Messages=1` a line goes to the screen through the console's `Message` (`GrabText`).

## OBVR

Whenever a holder takes hold or lets go, the holders go to OBVR as message `WGRB` (count, then reference and arm); a `WGFR` from OBVR (one reference) lets that holder go. OBVR has not built its side yet.

## Test switch

`[Grab] TestGrabSeconds` moves the nearest walker before the player and grabs as at a hit - used by `harness/grab-anims.txt` to film the animations.
