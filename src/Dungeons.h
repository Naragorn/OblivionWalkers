#pragma once

// The refugee dungeons in the game (docs/walkers-spec.md 4.20.7): so far
// the pool - DungeonLogic.h's decisions over the candidates
// (DungeonCandidates.h), with the loaded quests' cells (Replace.cpp) and the
// INI's [Dungeons] Skip.

#include "DungeonLogic.h"
#include "Rise.h"

namespace walkers {

// Once, at the first tick (the main menu - every quest loaded): which
// candidates are in the pool, logged with the reason for each one out.
void SelectDungeonPool(const char* iniPath, LogFn log);

// After SelectDungeonPool: whether candidate `index` (DungeonCandidates.h's
// order) is in the pool.
bool DungeonInPool(UInt32 index);

}  // namespace walkers
