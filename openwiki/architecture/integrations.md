---
type: architecture
title: "Other mods: LINK, leveling, skills, MOO"
description: How the plugin works with Maskar's LINK menu, keeps Ultimate Leveling and Oblivion XP from rewarding walker kills, keeps blows on walkers from training skills (with OBVR's strikes), and uses or tames Maskar's Oblivion Overhaul.
tags: [link, ultimate-leveling, oblivion-xp, skills, obvr, moo]
verified:
  - by: openwiki/0.7.2
    at: 2026-10-28T07:25:28.767Z
sources:
  - id: openwiki-source-795f3c31d185ff69b9d390f5
    resource: repo://src/LevelingLogic.h
  - id: openwiki-source-138be82cdc66d8461b6a02f4
    resource: repo://src/LinkLogic.h
  - id: openwiki-source-02da47e7fe4410af980eb7ee
    resource: repo://src/MooLogic.h
  - id: openwiki-source-49af0878b37559eafc52e284
    resource: repo://src/QuestVarLogic.h
  - id: openwiki-source-b60e987d7c8731794fe7a6a6
    resource: repo://src/QuestVars.cpp
  - id: openwiki-source-357b60e66710e88e89e848ac
    resource: repo://src/Rise.cpp
  - id: openwiki-source-ec1b99af8acb5e84e740f94b
    resource: repo://src/SkillLogic.h
  - id: openwiki-source-5b58748400c85217d70bfc2b
    resource: repo://tools/esp-write.py
generated: { by: "claude-code", at: "2026-10-28T07:25:28.767Z" }
---

# Other mods: LINK, leveling, skills, MOO

Each integration is a pure `*Logic.h` header plus a few lines in `src/Rise.cpp`. Specs: `docs/walkers-spec.md` 4.18 (LINK), 4.3 (skills), 4.3b (leveling), 4.1 and 7a (MOO).

## LINK - settings in the game's menu

- `src/LinkLogic.h` holds the menu table `kLinkSettings` (toggles and sliders with ranges, steps, titles, descriptions), the headers, `LinkValue`/`SetLinkValue` between the table and `RiseSettings`/`ReplaceSettings`, and `kLinkVersion`.
- The ESP carries the quest `Walkers` (0x94B) with the script `WalkersLinkScript` (0x94C) declaring one float variable per setting plus `ini_version`; `tools/esp-write.py` reads the variable names off the table.
- At every start the plugin writes `Data\OblivionWalkers.cfg` (LINK's menu) and `Data\OblivionWalkers.ini` (the defaults button) from the plugin INI (`LinkCfgText`, `LinkIniText`).
- At a load `StartOf` decides: without LINK, or with a save of another `kLinkVersion`, the INI's values go into the variables; otherwise the save's values win. With LINK the plugin reads the variables back once a second and applies a change (`LinkEffect`).
- Quest variables are found by name: `src/QuestVars.cpp` walks the quest script's variable list and the quest's event list; `src/QuestVarLogic.h` matches names case-insensitively.
- Rule (`AGENTS.md`): every setting LINK can show is in LINK.

## Ultimate Leveling and Oblivion XP - `[Zombies] NoLevelingXP`

`src/LevelingLogic.h`: in the plugin's own OnDeath handler, which runs before theirs, a dead walker's Confidence and Aggression (Ultimate Leveling's toughness factor) and eight attributes (Oblivion XP's kill bonus) are set to 0, so the kill gives no experience. The check lines in the log read Ultimate Leveling's `ULVL.XP` and Oblivion XP's `currentXP` before and after.

## Skills - `[Zombies] NoSkillGain`

`src/SkillLogic.h` and the hooks in `Rise.cpp`:

- The player's skill use (PlayerCharacter vtable slot +0x39C) is wrapped; a blow skill's use is held back when the frame's last hit (xOBSE's OnHit) was between the player and a walker, or when the call site (the armor of a hit, a block) names a walker as the attacker.
- The player's landed-hit count (vtable slot +0x210) skips a walker target, so the attack update trains no weapon skill for it.
- OBVR's hand-tracked strikes call AttackHandling directly; the plugin sends OBVR the walker bases as skill exceptions (message `SKEX` to "OBVR"), an empty list when NoSkillGain is off, again whenever LINK changes it.

## Maskar's Oblivion Overhaul

- Looks: the ESP has 44 walker creatures with MOO's zombie skeleton and parts (`src/WalkerLooks.h`); they are used only with MOO's ESP loaded, the risen drawn by race.
- `[Zombies] BlockMooResurrect` (`src/MooLogic.h`): MOO gives a dead zombie a token whose script raises it again; the plugin takes the token and MOO's zombie brain off a dead walker, at the next tick and again 2 s later.
