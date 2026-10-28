---
type: architecture
title: Spawns, hordes, gate guards and news
description: How the leveled spawns become walkers, how idle walkers form hordes that lean toward the cities, how gate guards come out and the safeguard raises the walkers' victims, and how the Black Horse Courier reports it.
tags: [spawns, leveled-lists, horde, guards, safeguard, news]
verified:
  - by: openwiki/0.7.2
    at: 2026-10-28T07:25:28.767Z
sources:
  - id: openwiki-source-e931c5634d3d7f857f322d7b
    resource: repo://OblivionWalkers.ini
  - id: openwiki-source-bbbd55d098143acb62c82630
    resource: repo://src/CourierRefs.h
  - id: openwiki-source-950aca3e0dc65627c3685371
    resource: repo://src/GuardLogic.h
  - id: openwiki-source-38ba10a035def97372c38efd
    resource: repo://src/HordeLogic.h
  - id: openwiki-source-bd31352b98054df29d87b5ab
    resource: repo://src/NewsLogic.h
  - id: openwiki-source-38d660530927a6b7379b33dc
    resource: repo://src/Replace.cpp
  - id: openwiki-source-75f7c325d7e7e254c608dbfc
    resource: repo://src/ReplaceLogic.h
  - id: openwiki-source-357b60e66710e88e89e848ac
    resource: repo://src/Rise.cpp
  - id: openwiki-source-72c0892feadb174a5626c84f
    resource: repo://src/SafeguardLogic.h
  - id: openwiki-source-5b58748400c85217d70bfc2b
    resource: repo://tools/esp-write.py
generated: { by: "claude-code", at: "2026-10-28T07:25:28.767Z" }
---

# Spawns, hordes, gate guards and news

Spec: `docs/walkers-spec.md` 4.6 and 4.13 (spawns), 4.11 (horde), 4.11b (gates, guards, safeguard), 4.9 (Courier). Settings: `[Replace]`, `[ReplaceCreatures]`, `[Horde]`, `[Guards]`, `[Safeguard]`, `[News]`.

## Spawn replacement - `Replace.cpp`, `ReplaceLogic.h`

- Once a session, at the start, the leveled creature lists in memory are walked; each creature and person in them has its own line in the INI's `[ReplaceCreatures]` (1 replaced, 0 kept), and the plugin writes the lines for new ones it meets, by the same rules, at the end of the section.
- A replaced entry points at the ESP's variant list `WalkersZombieVariant` (0x805): ten count slots, each the any-walker list `WalkersAnyWalker` (0x94A) at a count between `MinCount` and `MaxCount`. 0x94A carries the flag "calculate for each item in count", so every walker of a spawn draws its own look.
- `ExtraPercent` adds walker entries beside the originals in the wilderness and lair lists.
- Nothing is written into the save: the lists are changed in memory every session.

## Hordes - `Horde.cpp`, `HordeLogic.h`

- Once a second the walkers out of combat are grouped (`JoinRadius`, `GroupMost`); each group's leader travels to a marker the plugin moves ahead of it (`LeadUnits`, `TurnDegrees`), the others follow the leader. The ESP holds 16 groups' travel and follow packages and their markers in a holding cell.
- With `TowardCities` the markers lean toward the nearest city gate (`CityPull`, `CityRange`, the gates in `CityGates.h`) and hold `GateHoldUnits` before it.

## Gate guards and safeguard - `GuardLogic.h`, `SafeguardLogic.h`

- Walkers at a gate bring guards out of it: one for every `ZombiesPerGuard`, each `HealthFactor` times a walker's health, from a pool of persistent, initially disabled guard references in the ESP (21 per city) - no reference is made at runtime.
- The safeguard keeps everyone the walkers killed who does not rise, and raises them again once no walker is within `Radius` for `AfterSeconds`; the list is in the co-save.

## Black Horse Courier - `News.cpp`, `NewsLogic.h`

The ESP ships 18 editions (BOOK 0x8FC..0x90D, texts from `tools/courier-editions.py`); the plugin puts the edition that fits - the outbreak, a horde heading for a city, a city's gate attacked, several cities - on Oblivion.esm's own broadsheets in the inns and the Courier's office (`CourierRefs.h`). `RecentDays` and `HeadingHoldDays` decide how long a piece of news stays.

## Not built

The city siege (4.12) is designed, not built.
