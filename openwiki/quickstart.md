---
type: quickstart
title: Oblivion Walkers quickstart
description: Entry point to the Oblivion Walkers wiki - what the mod is, how the repository is laid out, the rules for changes, and which page answers which task.
tags: [quickstart, overview, navigation]
verified:
  - by: openwiki/0.7.2
    at: 2026-10-28T07:25:28.767Z
sources:
  - id: openwiki-source-8037e2358a2c4f9b2c722a11
    resource: repo://AGENTS.md
  - id: openwiki-source-34b8b80a0b74c6c69ffa7337
    resource: repo://docs/walkers-spec.md
  - id: openwiki-source-23775c3de52f3ab95a13cb8b
    resource: repo://README.md
  - id: openwiki-source-0211f2f5f95441e40340330f
    resource: repo://src/Plugin.cpp
generated: { by: "claude-code", at: "2026-10-28T07:25:28.767Z" }
---

# Oblivion Walkers quickstart

Oblivion Walkers is a zombie apocalypse for The Elder Scrolls IV: Oblivion (2006, 1.2.416): an xOBSE plugin plus a small ESP, built to sit beside [OBVR](https://github.com/Naragorn/OBVR) but not depending on it. The dead get up as walkers, leveled spawns become walkers, hordes drift toward the cities and gate guards come out, the Black Horse Courier reports it, walkers can grab and bite the player - and every part has a switch in the INI and, where it can, in LINK's menu.

## Repository map

| Path | What is there |
| --- | --- |
| `src/` | the plugin DLL; `Plugin.cpp` the exports, log and INI, `Rise.cpp` the hub and frame tick, `*Logic.h` the pure decisions |
| `tests/` | one unit-test program per Logic header, built 32-bit natively |
| `tools/` | `build-linux.sh` (tests, DLL, deploy), `esp-write.py` (the ESP), `grab-anims.py` (the grab's KFs), the data readers (`esm-spawns.py`, `city-gates.py`, `courier-refs.py`, `courier-editions.py`, `ini-creatures.py`) |
| `harness/` | scenarios for OBVR's hand-script harness, each with its run history |
| `docs/walkers-spec.md` | the design spec: engine facts, every feature with its settings, cost, decisions and tests, open questions and bugs |
| `OblivionWalkers.ini` | the INI as deployed, documented key by key |
| `AGENTS.md` | rules for changes in this repository |

## Rules for changes

- Everything in the repository is in English.
- Decisions go into pure `*Logic.h` headers with a test for every flow first; the engine side only reads and calls.
- Every setting LINK can show is in LINK as well as in the INI (`AGENTS.md`).
- Findings about the engine that matter to OBNG go to `../OBNG/knowledge/` as they come up (`AGENTS.md`).
- The ESP and the KFs are written from Bethesda's data and never committed.

## Where to look

| Task | Page |
| --- | --- |
| How the DLL hangs together | [Architecture overview](architecture/overview.md) |
| The rise of the dead | [The dead rise](architecture/rise.md) |
| Spawns, hordes, guards, news | [Spawns, hordes, gate guards and news](architecture/world.md) |
| Being grabbed and bitten | [Grabbed by walkers](architecture/grab.md) |
| LINK, Ultimate Leveling, Oblivion XP, skills, MOO | [Other mods](architecture/integrations.md) |
| A setting | [Configuration](operations/configuration.md) |
| Building and deploying | [Build and deploy](workflows/build-and-deploy.md) |
| Testing | [Tests and harness](testing/tests-and-harness.md) |
