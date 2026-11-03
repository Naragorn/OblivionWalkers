# Oblivion Walkers - the Nexus page

The text for the Nexus Mods page (short and plain; the README and
`docs/walkers-spec.md` say the rest). Version 0.1.0, the first public one -
a beta, and the page says so.

## Summary (the one line under the title)

A zombie apocalypse for Oblivion: the dead get up, spawns turn into slow
walking hordes, and only a lost head keeps them down. An xOBSE plugin plus a
small ESP, everything switchable.

## Description

Cyrodiil has a walker problem. People who fall get back up. Monster spawns
are walkers now, shuffling around in groups and drifting toward the cities.
The guards come out to hold the gates, and the Black Horse Courier writes
about all of it.

No scripts doing the heavy lifting: it's a native xOBSE plugin, working
right inside the engine's own frame.

**What it does**

- **The dead rise.** A bandit, a marauder, any generic person you kill gets
  up again 20 to 30 seconds later as a walker, right where he fell, with the
  get-up animation and his gear on him. Only while you're near, so you see
  it happen.
- **Spawns become walkers.** Skeletons, goblins, wolves, Daedra and most
  other creature spawns turn into 1 to 10 walkers. Every kind has its own
  switch; people (bandits, necromancers, ...) stay people unless you turn
  them on.
- **They walk.** Slow, each a bit different, a lot of them, low health.
- **Hordes.** Walkers find each other, follow a leader, and drift toward the
  cities. The gate guards come out to meet them.
- **Grabbed.** A walker can grab you and bite. Hit it, kill it or mash
  Block to break free - the more of them around you, the harder.
- **Only a lost head ends a walker** - with a mod that can cut heads off
  (OBVR in VR). Without one, a kill is a kill.
- **No farming.** Walkers carry no loot, give no skill gain and no Ultimate
  Leveling XP. They avoid chapels.
- **Nobody important is lost.** Whoever the walkers kill gets up again later
  (Safeguard), so quests don't break.
- **The news.** The Courier's broadsheets in inns and taverns report the
  outbreak, hordes on the move and attacks on the gates.
- **Settings in the game** with LINK, or in `OblivionWalkers.ini`.

**Needs**

- Oblivion 1.2.0.416 (tested with the Steam version), xOBSE 22.13 or later.
- **ESL download:** OblivionESL (Nexus 56578) and Cell Lookup Robustness
  (Nexus 55924). Takes no load-order slot.
- **ESP download:** nothing else.

**Recommended**

- LINK (Nexus 50144) for the settings menu in the game.
- Maskar's Oblivion Overhaul: its zombie looks join the walkers, and MOO's
  own zombie resurrect leaves the walkers alone.

**Install**

Pick one of the two downloads, install it with your mod manager (or copy
the Data folder into Oblivion's) and activate `OblivionWalkers.esp`.
Switching between the two later keeps your game, but walkers that already
rose from the fallen are gone.

**Heads up, it's a beta**

- Lots of walkers in a tight dungeon cost frames. Lower `[Replace]
  MaxCount` (10 by default) if a dungeon gets heavy.
- Some walkers have been seen standing around peacefully. Not tracked down
  yet - the log `OblivionWalkers.log` in the game folder helps if you see it.
- One crash on loading a save inside a dungeon, cause still open.
- Bug reports with the log are very welcome.

**Source**

GPL-3.0, on GitHub: https://github.com/Naragorn/OblivionWalkers

Like it? Buy me a coffee on Ko-fi: https://ko-fi.com/somadb (somadb is
another project of mine; the link goes to me).

## Files

Two downloads, same mod (Nadi, 2026-10-28):

- `OblivionWalkers-ESL-0.1.0.zip` - the ESL-flagged ESP, main file, needs
  OblivionESL and Cell Lookup Robustness.
- `OblivionWalkers-ESP-0.1.0.zip` - the plain ESP.

Both from `tools/package-release.sh 0.1.0`: the DLL (the MSVC build in
`build-win/` first), the INI, the ESP and the grab's animations written from
the game's own data on the machine that packs, and a README as text.

## Changelog

- 0.1.0 (beta): first public version.
