---
type: workflow
title: Build and deploy
description: Building the tests and the 32-bit DLL on Linux, writing the ESP and the grab's animations from the game's own data, and deploying all of it into both game installs.
tags: [build, deploy, esp, animations, linux]
verified:
  - by: openwiki/0.7.2
    at: 2026-10-28T07:25:28.767Z
sources:
  - id: openwiki-source-ea70eb6c045047448e446296
    resource: repo://.gitignore
  - id: openwiki-source-d44494ef3e497fea81240ef8
    resource: repo://CMakeLists.txt
  - id: openwiki-source-3ceeb7b5df15f6df319782cc
    resource: repo://tools/build-linux.sh
  - id: openwiki-source-5b58748400c85217d70bfc2b
    resource: repo://tools/esp-write.py
  - id: openwiki-source-3e7b12a2e775e6053538ff99
    resource: repo://tools/grab-anims.py
generated: { by: "claude-code", at: "2026-10-28T07:25:28.767Z" }
---

# Build and deploy

## The DLL and the tests

```
tools/build-linux.sh            # unit tests (clang++ -m32), then the DLL into build-linux/
tools/build-linux.sh --deploy   # and copy everything into the games
```

- The DLL is built with clang-cl and lld-link against xwin's Windows SDK (`cmake/toolchain-linux-clang-cl.cmake`), Release, static runtime; the top-level `CMakeLists.txt` refuses a 64-bit configuration. Releases are meant to be built with MSVC on Windows.
- A failing unit test stops the script before the DLL.

## The ESP

```
python3 tools/esp-write.py <Data>/Oblivion.esm "<Data>/Maskar's Oblivion Overhaul.esp"
```

writes `OblivionWalkers.esp` in the repository root from the ESM's own records (and MOO's for its looks): every record is a copy of a vanilla one with its own IDs, names and the fields the feature needs; the script reads its output back and stops on any difference. The file holds Bethesda's data and is not committed (`.gitignore`). The LINK variables are read off `src/LinkLogic.h`, so a new LINK setting needs the ESP written again.

## The grab's animations

```
~/obvr-venv/bin/python tools/grab-anims.py "<Data>/Oblivion - Meshes.bsa"
```

reads the zombie animations out of the BSA (its own small BSA reader), writes the three KFs into `build-linux/meshes/{Creatures/Zombie,MOO/zombie}/IdleAnims/` with pyffi and checks them by reading them back. Also not committed.

## Deploy

`--deploy` refuses while Oblivion runs. Into every game in `WALKERS_GAME_DIRS` (by default the Linux Steam copy and the Windows one on the shared drive) it copies:

- the DLL into `Data/OBSE/Plugins`, and the INI only when none is there yet;
- the ESP into `Data`, keeping the file time the game already had - the load order goes by it;
- the KFs into `Data/Meshes/...`, each folder matched without regard to case.
