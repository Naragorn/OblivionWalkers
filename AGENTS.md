# Repository instructions

## Findings for OBNG

- OBNG (Oblivion Next Generation) is the planned community modernisation of Oblivion.exe 1.2.416: reverse engineering the engine and rebuilding it in modern form. Its knowledge lives outside this repo, in `../OBNG/knowledge/` (`/run/media/nadi/Games/Modding/OBNG/knowledge/`).
- Whenever work here turns up a finding that would matter to OBNG - how the engine does something, where it does it, what it costs, or how a modern engine would do it instead - write it there as you go, not only at the end.
- One file per topic, as `../OBNG/README.md` describes: what was read in the EXE (addresses), what was measured in the game, and what a modern engine does instead, each with its source. Add to an existing topic file before starting a new one, and mark what is read, measured, derived or unverified.

## Settings belong in LINK

- Every setting that LINK can show must be in LINK's menu as well as in the INI (Nadi, 2026-10-28: "alle settings die in link sein können sollen es auch"). A new or changed setting goes into `src/LinkLogic.h`: `kLinkSettings` (toggle or slider, range, step, a short title and description), `LinkValue` and `SetLinkValue`, a header if it opens a new group; raise `kLinkVersion`, extend `tests/LinkLogicTest.cpp`, rebuild the ESP (`tools/esp-write.py` reads the variables off the table) and deploy it.
- What LINK cannot show stays in the INI only, and is named as such in the spec section of the feature: text values (names), lists (`[ReplaceCreatures]`), a key or control choice, and test switches (`TestGrabSeconds`, `TestKillSeconds` and the like).
- A setting that needs more than a new value to take effect (records rewritten, lists rebuilt) gets its `LinkEffect`; one that only takes effect at the next start says so in its description.
