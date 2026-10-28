---
type: operations
title: Configuration
description: OblivionWalkers.ini section by section, where the settings are read, the LINK menu that mirrors them in the game, and the rule that every setting LINK can show is in LINK.
tags: [configuration, ini, link]
verified:
  - by: openwiki/0.7.2
    at: 2026-10-28T09:48:19.681Z
sources:
  - id: openwiki-source-8037e2358a2c4f9b2c722a11
    resource: repo://AGENTS.md
  - id: openwiki-source-34b8b80a0b74c6c69ffa7337
    resource: repo://docs/walkers-spec.md
  - id: openwiki-source-e931c5634d3d7f857f322d7b
    resource: repo://OblivionWalkers.ini
  - id: openwiki-source-138be82cdc66d8461b6a02f4
    resource: repo://src/LinkLogic.h
  - id: openwiki-source-0211f2f5f95441e40340330f
    resource: repo://src/Plugin.cpp
generated: { by: "claude-code", at: "2026-10-28T09:48:19.681Z" }
---

# Configuration

## The INI

`OblivionWalkers.ini` sits beside the DLL in `Data/OBSE/Plugins` and is read once when the game starts (`src/Plugin.cpp ReadSettings`). Every key is documented in the file itself; the spec section of each feature names its keys.

| Section | Feature | Spec |
| --- | --- | --- |
| `[Rise]` | the dead rise, the swap, its look and sound | 4.1 |
| `[Zombies]` | walk and pace, health, names, chapels, loot, fame, NoLevelingXP, NoSkillGain, BlockMooResurrect | 4.2-4.5, 4.7, 4.8, 4.15, 4.3, 4.3b |
| `[Replace]` | spawn replacement, counts, extra spawns | 4.6, 4.13 |
| `[Horde]` | groups, leads, toward the cities | 4.11, 4.11b |
| `[Guards]` | gate guards | 4.11b |
| `[Safeguard]` | the walkers' victims raised again | 4.11b |
| `[News]` | the Black Horse Courier | 4.9 |
| `[Grab]` | grabbed by walkers, bites, the struggle, messages, the test switch | 4.19 |
| `[ReplaceCreatures]` | one line per creature or person of the leveled lists, 1 replaced, 0 kept; the plugin adds new ones at the end | 4.6 |

## The LINK menu

With Maskar's LINK loaded the settings in `src/LinkLogic.h kLinkSettings` show in LINK's in-game menu - 66 of them, grouped under headers (The dead rise, Walkers, Spawns, Hordes gate guards and news, Training, Grabbed), then the fine ones under six "- advanced" headers. The plugin writes `Data\OblivionWalkers.cfg` and `Data\OblivionWalkers.ini` for LINK at every start; the values live in the save, read once a second. Who wins at a load is in [Other mods](../architecture/integrations.md).

INI-only, by the rule below: `Name` (text), `[ReplaceCreatures]` (a list), `StruggleControl` (a control), the test switches `TestKillSeconds` and `TestGrabSeconds`, and the proof and fallback switches `AsCreature`, `CloneBase`, `ZombieRace`, `LookAfterSeconds`, `HoldDeathSound`.

## Rule

`AGENTS.md`: every setting that LINK can show is in LINK as well as in the INI. Only text values, lists, a key or control choice and test switches stay INI-only, and the spec names them. A new LINK setting raises `kLinkVersion` and rebuilds the ESP.
