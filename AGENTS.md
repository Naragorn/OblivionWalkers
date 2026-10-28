# Repository instructions

## Findings for OBNG

- OBNG (Oblivion Next Generation) is the planned community modernisation of Oblivion.exe 1.2.416: reverse engineering the engine and rebuilding it in modern form. Its knowledge lives outside this repo, in `../OBNG/knowledge/` (`/run/media/nadi/Games/Modding/OBNG/knowledge/`).
- Whenever work here turns up a finding that would matter to OBNG - how the engine does something, where it does it, what it costs, or how a modern engine would do it instead - write it there as you go, not only at the end.
- One file per topic, as `../OBNG/README.md` describes: what was read in the EXE (addresses), what was measured in the game, and what a modern engine does instead, each with its source. Add to an existing topic file before starting a new one, and mark what is read, measured, derived or unverified.

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
