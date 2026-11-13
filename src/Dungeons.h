#pragma once

// The refugee dungeons in the game (docs/walkers-spec.md 4.20.7): so far
// the pool - DungeonLogic.h's decisions over the candidates
// (DungeonCandidates.h), with the loaded quests' cells (Replace.cpp) and the
// INI's [Dungeons] Skip.

#include "DungeonKitLogic.h"
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
using DungeonConsoleFn = bool (*)(UInt32 ref, const char* line);
using DungeonDeadFn = bool (*)(UInt32 actor);
// Takes a reference off the rise's waiting list (a kit person raised again).
using DungeonUnlistFn = bool (*)(UInt32 refId);
// A walker the rise made (it stays in a held place: one of its people's).
using DungeonRisenFn = bool (*)(UInt32 refId);

// Once a session: the live settings (LINK writes into them), the dice, the
// console on a reference, the dead, the rise's list and its walkers, and
// [Dungeons] TestSite and TestSiteState (a candidate's name made the first
// site - 1 a refuge, 2 besieged, 3 to clear, 4 a walker dungeon; "" none).
void ConfigureDungeons(const DungeonSiteSettings* settings, DungeonRollFn roll, DungeonConsoleFn console,
                       DungeonDeadFn dead, DungeonUnlistFn unlist, DungeonRisenFn risen, const char* testSite,
                       UInt32 testState);
// The ESP's base (RiseLogic.h EspFormId), once it is known.
void ConfigureDungeonsEsp(UInt32 espBase);

// One of the held dungeons' kit of people (the safeguard leaves them alone:
// they rise; a walker one of them fells is ended).
bool IsDungeonPerson(UInt32 refId);

// One of the held dungeons' pool of walkers (no horde takes it).
bool IsDungeonWalker(UInt32 refId);

// A walker ended for good (4.17, Rise.cpp - from inside the death path, so
// only noted; counted at the next step): one of the pool out in a place
// counts off its siege or its walker dungeon. False for any other.
bool NoteDungeonWalkerEnded(UInt32 refId);

// Every frame (stepped once a game second inside): the active places
// filled up from the pool, each stepped with the player near it or not -
// in one of its cells, or within kDungeonSeenUnits of its door in Tamriel -
// and every event logged.
void StepDungeonsInGame(float dt, LogFn log);

// The sites as they stand, for the co-save.
DungeonSitesState& CurrentDungeons();
// The occupants taken out of the held places, for the co-save.
DungeonOccupants& CurrentDungeonOccupants();
// At a load and a new game: the sites emptied (the co-save fills them again).
void ResetDungeonsInGame();
// The candidates' count of this build (a co-save's site past it is dropped).
UInt32 kDungeonCandidateCountForSave();

}  // namespace walkers
