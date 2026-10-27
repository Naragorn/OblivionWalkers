#pragma once

// The spawn replacement in the game (docs/walkers-spec.md 4.6) and the
// walkers' health (4.5). The decisions are ReplaceLogic.h's; this file only
// reads and writes the engine's records and the INI.

#include "ReplaceLogic.h"
#include "Rise.h"

namespace walkers {

// Every leveled creature list in memory: an entry whose creature
// [ReplaceCreatures] switches on (a new creature gets its line, on or off by
// ClassifyCreature) points at the ESP's WalkersZombieVariant from now on, for
// the session - the engine then spawns walkers at its own spawn points. The
// variant list's counts are written from MinCount/MaxCount first. Run again,
// it only takes the entries still pointing at a creature. Returns the
// entries replaced by this run.
UInt32 ReplaceSpawns(UInt32 espIndex, const ReplaceSettings& settings, const char* iniPath, bool mooLoaded,
                     LogFn log);

// The walker records' health at `percent` of their own, from the values
// they had at the first call - the record in memory only.
// The variant list's count slots written anew from MinCount and MaxCount
// (LINK's menu, 4.18); nothing without the ESP's lists.
void RewriteVariantCounts(UInt32 espIndex, const ReplaceSettings& settings, LogFn log);

void ScaleWalkerHealth(const UInt32* walkerIds, UInt32 count, UInt32 percent, LogFn log);

// The walker records' Speed (4.4): each record's own times the multiplier,
// spread over the records (ReplaceLogic.h WalkerSpeed), from the values they
// had at the first call - the records in memory only.
// The walkers' name (4.2): Name on the ESP's walker records (the game's
// "Zombie" with RenameToWalker=0), and with RenameVanilla every other
// creature's "Zombie" too (ReplaceLogic.h RenamedZombie) - in memory only.
void RenameZombies(const UInt32* walkerIds, UInt32 count, UInt32 espIndex, const RiseSettings& settings, LogFn log);

void ScaleWalkerSpeed(const UInt32* walkerIds, UInt32 count, float multiplier, UInt32 randomPercent, LogFn log);

}  // namespace walkers
