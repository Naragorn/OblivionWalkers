---
type: testing
title: Tests and harness
description: The unit tests that run every flow of the pure Logic headers without the game, and the harness scenarios in harness/ that run in the game on OBVR's harness, with the log lines they read.
tags: [testing, unit-tests, harness]
verified:
  - by: openwiki/0.7.2
    at: 2026-10-28T09:48:19.681Z
sources:
  - id: openwiki-source-d44494ef3e497fea81240ef8
    resource: repo://CMakeLists.txt
  - id: openwiki-source-cc47d8a9ac7e32be5f28a377
    resource: repo://harness/esl-check.txt
  - id: openwiki-source-3b2853c3c68ade34f4118ec8
    resource: repo://harness/grab-anims.txt
  - id: openwiki-source-f532486c7ed4dc32defb7f49
    resource: repo://harness/rise-after-away.txt
  - id: openwiki-source-fa68239bf614d837d7e5522c
    resource: repo://tests/CMakeLists.txt
generated: { by: "claude-code", at: "2026-10-28T09:48:19.681Z" }
---

# Tests and harness

## Unit tests

- One test program per Logic header in `tests/` (`RiseLogicTest`, `ReplaceLogicTest`, `PoseLogicTest`, `ApartLogicTest`, `MooLogicTest`, `SoundLogicTest`, `HordeLogicTest`, `GuardLogicTest`, `SafeguardLogicTest`, `NewsLogicTest`, `WalkerLooksTest`, `LevelingLogicTest`, `QuestVarLogicTest`, `LinkLogicTest`, `SkillLogicTest`, `GrabLogicTest`, plus `PluginEntryTest`). Each prints `ok`/`FAIL` per flow and `PASS (0 failed)`.
- On Linux they build natively, 32-bit, with `clang++ -m32` from `tests/CMakeLists.txt`; the build script runs them before every DLL build and stops on a failure (see [Build and deploy](../workflows/build-and-deploy.md)). The top-level `CMakeLists.txt` lists the same tests for the Windows build.
- The rule (global): every flow through a decision gets a test - new logic goes into a pure header first.

## Harness scenarios

- `harness/*.txt` are hand scripts for OBVR's harness: run from the OBVR folder, always `--headless`, with `--artifact-root ../OblivionWalkers/artifacts/hand-script`:

  ```
  python3 tools/hand-script-run.py ../OblivionWalkers/harness/<scenario>.txt --headless \
      --artifact-root ../OblivionWalkers/artifacts/hand-script
  ```

- A scenario's header comment says what it shows, what the log should read, and records each run (date, folder, PASS/FAIL, what was seen and not seen). The runner checks OBVR.log's `expect`/`reject` lines; the plugin's own `OblivionWalkers.log` is copied into the run's folder and read beside the pictures.
- Console lines name the plugin's forms by their runtime ID (`25000801` at the current load order; `FE000801` under OblivionESL in `esl-check.txt`): the console takes no editor ID for the walker creatures (`grab-anims.txt`, run 2026-10-28 10:03), so a change of the load order means new IDs in the scenarios.
- Some runs need a temporary line in the Linux install's plugin INI (for example `[Grab] ChancePercent=100`, `TestGrabSeconds=15`), taken out after the run.
- The game on the machine is shared with other sessions (OBVR and others): announce busy, run, announce free.

Examples: `rise-after-away.txt` (a waiting body while the player is away), `link-settings.txt` (LINK's variables set by the console), `grab-anims.txt` (the grab's animations before the camera, using the test switch).
