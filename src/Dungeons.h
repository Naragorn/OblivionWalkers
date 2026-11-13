#pragma once

// The refugee dungeons in the game (docs/walkers-spec.md 4.20.7): so far
// the pool - DungeonLogic.h's decisions over the candidates
// (DungeonCandidates.h), with the loaded quests' cells (Replace.cpp) and the
// INI's [Dungeons] Skip.

#include "DungeonLogic.h"
#include "DungeonSiteLogic.h"
#include "Rise.h"

namespace walkers {

// Once, at the first tick (the main menu - every quest loaded): which
// candidates are in the pool, logged with the reason for each one out.
void SelectDungeonPool(const char* iniPath, bool onlyChosen, LogFn log);

// Every frame: [Dungeons] OnlyChosen as it is now (LINK may change it) - the
// pool marked anew when it changed.
void StepDungeonPool(bool onlyChosen, LogFn log);

// After SelectDungeonPool: whether candidate `index` (DungeonCandidates.h's
// order) is in the pool.
bool DungeonInPool(UInt32 index);

using DungeonRollFn = UInt32 (*)();

// Once a session: the live settings (LINK writes into them) and the dice.
void ConfigureDungeons(const DungeonSiteSettings* settings, DungeonRollFn roll);

// Every frame (stepped once a game second inside): the active places
// filled up from the pool, each stepped with the player near it or not -
// in one of its cells, or within kDungeonSeenUnits of its door in Tamriel -
// and every event logged.
void StepDungeonsInGame(float dt, LogFn log);

// The sites as they stand, for the co-save.
DungeonSitesState& CurrentDungeons();
// At a load and a new game: the sites emptied (the co-save fills them again).
void ResetDungeonsInGame();
// The candidates' count of this build (a co-save's site past it is dropped).
UInt32 kDungeonCandidateCountForSave();

}  // namespace walkers
