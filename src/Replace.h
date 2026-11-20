#pragma once

// The spawn replacement in the game (docs/walkers-spec.md 4.6) and the
// walkers' health (4.5). The decisions are ReplaceLogic.h's; this file only
// reads and writes the engine's records and the INI.

#include <vector>

#include "KindLogic.h"
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
// Every quest's names, read once (the first call; ReplaceSpawns calls it
// too): its script, its stages' result scripts, its targets.
void CollectQuestNames(LogFn log);

// The cells a quest names, or that hold a reference a quest names - after
// CollectQuestNames (the refugee dungeons' A, docs/walkers-spec.md 4.20.7).
std::vector<UInt32> QuestCellIds();

// The quest that named `formId` (a cell or a reference) first, and how
// ("MQ05 (a target)"); nullptr when none did.
const char* QuestNameWhy(UInt32 formId);
// The references a quest names, by form ID - after CollectQuestNames.
std::vector<UInt32> QuestRefIds();

// The cell of a reference a quest names (0 none, or not named).
UInt32 QuestRefCell(UInt32 refId);

// Walkers Mode 1 replaces the Zombies category alone; with the Daedra
// coming everywhere (`daedraEverywhere`, Bands.h: EverywhereUntilGatesOpen
// and no gate open) a replaced entry names the ESP's Daedra list instead -
// all of them with no walkers, SharePercent of them otherwise, by each
// entry's stable roll (KindLogic.h ReplacementTarget). The Daedra's own
// categories stay the game's whenever a Daedra mode is on. With both
// switches off nothing is replaced.
UInt32 ReplaceSpawns(UInt32 espBase, const ReplaceSettings& settings, const InvaderSettings& invaders,
                     bool daedraEverywhere, const char* iniPath, bool mooLoaded, LogFn log);

// The walker records' health at `percent` of their own, from the values
// they had at the first call - the record in memory only.
// The variant list's count slots written anew from MinCount and MaxCount
// (LINK's menu, 4.18); nothing without the ESP's lists.
void RewriteVariantCounts(UInt32 espBase, const ReplaceSettings& settings, UInt32 nightPercent, LogFn log);

void ScaleWalkerHealth(const UInt32* walkerIds, UInt32 count, UInt32 percent, LogFn log);

// Daedra Mode 2 (allies) (KindLogic.h WalkersJoinDaedra): the walker records join
// DaedraFaction (a faction entry of ours appended to each record's list in
// memory), or leave it again - only the entries this call added.
void SetWalkersAllied(const UInt32* walkerIds, UInt32 count, bool allied, LogFn log);

// The walker records' Speed (4.4): each record's own times the multiplier,
// spread over the records (ReplaceLogic.h WalkerSpeed), from the values they
// had at the first call - the records in memory only.
// The walkers' name (4.2): Name on the ESP's walker records (the game's
// "Zombie" with RenameToWalker=0), and with RenameVanilla every other
// creature's "Zombie" too (ReplaceLogic.h RenamedZombie) - in memory only.
void RenameZombies(const UInt32* walkerIds, UInt32 count, UInt32 espBase, const RiseSettings& settings, LogFn log);

// [Replace] TestLogList: the list's entries into the log - `why` says when
// (after a pass, or changed in the game); WatchTestList logs only a change
// since the last look. Nothing with 0.
void LogTestList(UInt32 listId, const char* why, LogFn log);

// The boss guard's clean copies (BossCopyLogic.h): every ordinary list
// nested in a boss list pointed at a copy of itself that names the source's
// own creatures; built anew when the source changes; back to the source with
// the guard off. After each pass and once a second (MOO rewrites boss lists
// in the game).
void GuardBossLists(UInt32 espBase, const ReplaceSettings& settings, LogFn log);
bool IsBossCopy(UInt32 form);
void WatchTestList(UInt32 listId, LogFn log);

void ScaleWalkerSpeed(const UInt32* walkerIds, UInt32 count, float multiplier, UInt32 randomPercent, LogFn log);

}  // namespace walkers
