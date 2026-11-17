# Repository instructions

## Performance

- Build everything extremely performant, and make what already exists faster too - whatever can be optimized is optimized, new code and old (Nadi, 2026-11-13). The game runs in VR: a frame has about 11 ms for everything, and a single long frame is a stutter in the headset.
- Cost first: before writing a feature, decide what must run every frame, what once a second and what only on an event. Per frame only what has to be: no allocation, no file I/O, no console line, no lookup that can be kept, no loop over n x m.
- Measure, never guess: every optimization carries a number from before and one from after, taken under the same load; the mean and the worst frame both count. An instrument's own cost is measured too.
- Here: `[Debug] StepTiming=1` times every step of the tick and every console line by its verb (`src/TimingLogic.h`); `tools/walkers-timing.sh` runs it with 60 walkers placed about the player. Results go into `docs/walkers-spec.md`, section Performance.
- Write each measured result where the project keeps its findings (the spec or the knowledge files), with the commit, so the next change can be compared against it.

## Running the harness (OBVR's)

- The harness is OBVR's runner, `../OBVR/tools/hand-script-run.py`; the rules for it live in `../OBVR/AGENTS.md` ("Running the harness on Linux") and `../OBVR/docs/hand-script-harness.md` ("Without SteamVR", "Several runs at once", "A run that measures runs alone"). Read them before a run. The short of it, since 2026-11-17:
- `--no-steamvr`: SteamVR does not have to run; OBVR loads a stand-in runtime. Its headset is the Pimax Dream Air by default (4036x3376 an eye, 72 Hz, IPD 63 mm); `--headset beyond` is the Bigscreen Beyond (2568x2568, 75 Hz) of the runs before that date.
- `--parallel` (with `--headless --no-steamvr`): the run takes a slot of its own - a mirror of the game folder and its own Proton prefix - beside the player's game and beside other runs, with no busy/free. `--plugin FILE` puts a file under test into the mirror's plugin folder (a DLL, an INI); everything else under `Data` is the player's, linked, and still needs a deploy. A deploy and a run without `--parallel` are announced with busy/free as before.
- A run that measures runs alone (Nadi, 2026-11-17): pass `--exclusive` for every load, timing or performance run - it waits until no other harness run and no other game is running and holds the others off until it ends. A scenario with `ini Performance Enabled=1` is exclusive by itself. An exclusive run fails when a game is started beside it.
- A run without SteamVR tests nothing SteamVR does itself (compositing, overlays shown, frame statistics, reprojection), and its performance numbers are not comparable with a run with SteamVR.
- Here: the wrappers under `tools/*.sh` call the runner with `--headless` alone (2026-11-17); the plugin's `.esp` and scripts live under `Data` and are linked into a slot, so a slot runs the Walkers that is deployed. `tools/walkers-timing.sh` and every other timing run have to pass `--exclusive`; they do not yet.

## Findings for OBNG

- OBNG (Oblivion Next Generation) is the planned community modernisation of Oblivion.exe 1.2.416: reverse engineering the engine and rebuilding it in modern form. Its knowledge lives outside this repo, in `../OBNG/knowledge/` (`/run/media/nadi/Games/Modding/OBNG/knowledge/`).
- Whenever work here turns up a finding that would matter to OBNG - how the engine does something, where it does it, what it costs, or how a modern engine would do it instead - write it there as you go, not only at the end.
- One file per topic, as `../OBNG/README.md` describes: what was read in the EXE (addresses), what was measured in the game, and what a modern engine does instead, each with its source. Add to an existing topic file before starting a new one, and mark what is read, measured, derived or unverified.

## Reading Oblivion.exe

- Every function of the EXE is already decompiled: `../OBNG/re/export/rea-all/functions/0x<address>.c` (`/run/media/nadi/Games/Modding/OBNG/re/export/rea-all/` on Linux, `D:\Modding\OBNG\re\export\rea-all\` on Windows; on the shared disk only, not in Git - `../OBNG/re/README.md` says how it was made). Grep it before starting a Ghidra or REA session, and start one only for what the files do not answer (assembly, cross-references, data).

## Settings belong in LINK

- Every setting that LINK can show must be in LINK's menu as well as in the INI (Nadi, 2026-10-28: "alle settings die in link sein können sollen es auch"). A new or changed setting goes into `src/LinkLogic.h`: `kLinkSettings` (toggle or slider, range, step, a short title and description), `LinkValue` and `SetLinkValue`, a header if it opens a new group; raise `kLinkVersion`, extend `tests/LinkLogicTest.cpp`, rebuild the ESP (`tools/esp-write.py` reads the variables off the table) and deploy it.
- What LINK cannot show stays in the INI only, and is named as such in the spec section of the feature: text values (names), lists (`[ReplaceCreatures]`), a key or control choice, and test, proof and fallback switches (`TestGrabSeconds`, `TestKillSeconds`, `AsCreature`, `CloneBase`, `ZombieRace`, `LookAfterSeconds`, `HoldDeathSound`).
- A setting that needs more than a new value to take effect (records rewritten, lists rebuilt) gets its `LinkEffect`; one that only takes effect at the next start says so in its description.

<!-- OPENWIKI:START -->

## OpenWiki

This repository has a generated `openwiki/` evidence index. It is optional just-in-time context, not required startup reading.

- Do not enumerate, preload, or search wikis at task start. Use retrieval when the user asks for it, when unfamiliar architecture or dependency behavior materially affects the task, or when source inspection leaves an important uncertainty. Stop once the question is grounded.
- When those conditions apply and OpenWiki retrieval tools are available, use `openwiki_search` for just-in-time context and `openwiki_read` for the relevant complete sections. If search returns `workspace_required`, ask which listed workspace to use and retry with its ID.
- Use `openwiki_list_workspaces` or `openwiki_list_wikis` when workspace membership itself needs to be discovered.
- If the retrieval tools are unavailable, read `openwiki/quickstart.md` and follow its links to the relevant pages.
- Treat source code and tests as authoritative. A brief's unknowns and review items are verification gaps, not automatic requirements.
- Prefer the narrowest quiet validation that proves the changed behavior. Preserve complete failure output.

The scheduled OpenWiki GitHub Actions workflow refreshes the repository wiki. Do not hand-edit generated OpenWiki pages unless explicitly asked; prefer updating source code/docs and letting OpenWiki regenerate.

<!-- OPENWIKI:END -->
