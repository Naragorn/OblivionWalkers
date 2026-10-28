---
type: workflow
title: Build and deploy
description: Building the tests and the 32-bit DLL on Linux, writing the ESP (plain or ESL-flagged) and the grab's animations from the game's own data, deploying into both game installs, and packing the two release downloads.
tags: [build, deploy, esp, esl, animations, release, linux]
sources:
  - id: openwiki-source-ea70eb6c045047448e446296
    resource: repo://.gitignore
  - id: openwiki-source-d44494ef3e497fea81240ef8
    resource: repo://CMakeLists.txt
  - id: openwiki-source-23775c3de52f3ab95a13cb8b
    resource: repo://README.md
  - id: openwiki-source-3ceeb7b5df15f6df319782cc
    resource: repo://tools/build-linux.sh
  - id: openwiki-source-5b58748400c85217d70bfc2b
    resource: repo://tools/esp-write.py
  - id: openwiki-source-3e7b12a2e775e6053538ff99
    resource: repo://tools/grab-anims.py
  - id: openwiki-source-432a637202332624153a61c2
    resource: repo://tools/package-release.sh
generated: { by: "claude-code", at: "2026-10-28T09:43:45.288Z" }
verified:
  - by: openwiki/0.7.2
    at: 2026-10-28T09:48:19.681Z
---

# Build and deploy

## The DLL and the tests

```
tools/build-linux.sh            # unit tests (clang++ -m32), then the DLL into build-linux/
tools/build-linux.sh --deploy   # and copy everything into the games
```

- The DLL is built with clang-cl and lld-link against xwin's Windows SDK (`cmake/toolchain-linux-clang-cl.cmake`), Release, static runtime; the top-level `CMakeLists.txt` refuses a 64-bit configuration. Releases are meant to be built with MSVC on Windows.
- A failing unit test stops the script before the DLL; `--deploy` copies nothing while Oblivion runs.

## The ESP

```
python3 tools/esp-write.py <Data>/Oblivion.esm "<Data>/Maskar's Oblivion Overhaul.esp" [--esl] [--out <file>]
```

writes `OblivionWalkers.esp` from the ESM's own records (and MOO's for its looks): every record is a copy of a vanilla one with its own IDs, names and the fields the feature needs; the script reads its output back and stops on any difference. `--esl` sets the ESL flag (0x200) in the header for OblivionESL - the same records and IDs, one byte different; the script refuses a local ID past 0xFFF. The file holds Bethesda's data and is not committed (`.gitignore`). The LINK variables are read off `src/LinkLogic.h`, so a new LINK setting needs the ESP written again.

## The grab's animations

```
~/obvr-venv/bin/python tools/grab-anims.py "<Data>/Oblivion - Meshes.bsa"
```

reads the zombie animations out of the BSA (its own small BSA reader), writes the three KFs into `build-linux/meshes/{Creatures/Zombie,MOO/zombie}/IdleAnims/` with pyffi and checks them by reading them back. Also not committed.

## Deploy

`--deploy` copies the DLL into `Data/OBSE/Plugins` of every game in `WALKERS_GAME_DIRS` (by default the Linux Steam copy and the Windows one on the shared drive), the plain ESP into `Data` and the KFs into `Data/Meshes/...`.

## Release

```
tools/package-release.sh <version> [<Data folder>]
```

writes `dist/OblivionWalkers-ESL-<version>.zip` (the ESL-flagged ESP; its README names OblivionESL and Cell Lookup Robustness as requirements) and `dist/OblivionWalkers-ESP-<version>.zip` (the plain ESP), each laid out as a Data folder with the DLL, the INI and the animations, and a README saying what a switch between the two costs.
