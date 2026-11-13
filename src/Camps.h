#pragma once

// The survivors' camps in the game (docs/walkers-spec.md 4.20.6): CampLogic.h's
// decisions carried out - a second's look at the clock and the player, each
// camp's people enabled as its roster says, the attack's walkers out of the
// ESP's pool before the camp the player is at, the rewards, the lines, the
// state kept in the co-save through Rise.cpp.

#include "CampLogic.h"
#include "Rise.h"

namespace walkers {

using CampConsoleFn = bool (*)(UInt32 ref, const char* line);
using CampRollFn = UInt32 (*)();
using CampDeadFn = bool (*)(UInt32 actor);
// Takes a reference off the rise's waiting list (a kit person raised before
// it rose); false when it was not on it.
using CampUnlistFn = bool (*)(UInt32 refId);

// Once a session, at the plugin's load: the live settings (LINK writes into
// them), the console on a reference, a draw, the dead, the rise's list, the
// log.
void ConfigureCamps(const CampSettings* settings, CampConsoleFn console, CampRollFn roll, CampDeadFn dead,
                    CampUnlistFn unlist, LogFn log);

// The ESP's base (RiseLogic.h EspFormId), once it is known.
void ConfigureCampsEsp(UInt32 espBase);

// A walker ended for good (4.17, Rise.cpp - from inside the death path, so
// only noted; counted at the next step): one of a shown camp's pool counts
// off its attack or its walkers. False for any other.
bool NoteCampWalkerEnded(UInt32 refId);

// The refugees' reward by the camps' settings (RewardFame and FamePoints,
// RewardGold and Gold, RewardThings: a thing for each of `alive`), given to
// the player - for a held dungeon defended or taken (4.20.7); `what` gets
// " (+N fame, N gold, ...)" or stays empty.
void GiveRefugeeReward(UInt32 alive, char* what, UInt32 size);
// Vanilla's quest-updated sound.
bool PlayQuestSound();

// One of the camps' pool of walkers, out at a camp now (no horde takes it).
bool IsCampWalker(UInt32 refId);

// One of the camps' people (the safeguard leaves them alone: they rise).
bool IsCampPerson(UInt32 refId);

// Every frame; the clock is looked at once a second.
void StepCampsInGame(float dt);

// The camps kept across saves (Rise.cpp's co-save callbacks).
CampsState& CurrentCamps();

// A new game or a load: the pool and what was shown start again.
void ResetCampsInGame();

// After the co-save is read (or none was): the rosters taken, the pool and
// the kits back in (the next step puts the kits and markers).
void ApplyCampsLoaded();

}  // namespace walkers
