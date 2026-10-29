#pragma once

// The spawn replacement in the game (docs/walkers-spec.md 4.6) and the
// walkers' health (4.5). The decisions are ReplaceLogic.h's; this file only
// reads and writes the engine's records and the INI.

#include "ReplaceLogic.h"
#include "Rise.h"

namespace walkers {

// Every leveled creature list in memory: an entry whose creature or person
// ReplaceLogic.h's ReplaceOn switches on - by its category
// ([ReplaceCategories]), an exception ([ReplaceExceptions]) or, without a
// category, its [ReplaceCreatures] line (a new one written by its kind) -
// points at the ESP's WalkersZombieVariant, the engine then spawns walkers at
// its own spawn points; one switched off points at its own again. The
// variant list's counts are written from MinCount/MaxCount at the first run.
// Run again - after a load, at a change in LINK - it points every entry as the
// settings say now. Returns the entries pointed at the walkers by this run.
UInt32 ReplaceSpawns(UInt32 espBase, const ReplaceSettings& settings, const char* iniPath, bool mooLoaded,
                     LogFn log);

// The walker records' health at `percent` of their own, from the values
// they had at the first call - the record in memory only.
// The variant list's count slots written anew from MinCount and MaxCount
// (LINK's menu, 4.18); nothing without the ESP's lists.
void RewriteVariantCounts(UInt32 espBase, const ReplaceSettings& settings, LogFn log);

void ScaleWalkerHealth(const UInt32* walkerIds, UInt32 count, UInt32 percent, LogFn log);

// The walker records' Speed (4.4): each record's own times the multiplier,
// spread over the records (ReplaceLogic.h WalkerSpeed), from the values they
// had at the first call - the records in memory only.
// The walkers' name (4.2): Name on the ESP's walker records (the game's
// "Zombie" with RenameToWalker=0), and with RenameVanilla every other
// creature's "Zombie" too (ReplaceLogic.h RenamedZombie) - in memory only.
void RenameZombies(const UInt32* walkerIds, UInt32 count, UInt32 espBase, const RiseSettings& settings, LogFn log);

void ScaleWalkerSpeed(const UInt32* walkerIds, UInt32 count, float multiplier, UInt32 randomPercent, LogFn log);

}  // namespace walkers
