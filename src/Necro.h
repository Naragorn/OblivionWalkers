#pragma once

// The necromancers' realms in the game (docs/walkers-spec.md 4.23 B, build
// step 3): NecroLogic.h's realms as numbers, their places as held dungeons'
// sites of their own (Dungeons.cpp AddNecroPlace) - the bosses and the
// walkers placed where the player is, as the held places' are.

#include "KindLogic.h"
#include "NecroLogic.h"
#include "Rise.h"

namespace walkers {

using NecroRollFn = UInt32 (*)();
using NecroConsoleFn = bool (*)(UInt32 ref, const char* line);
using NecroDeadFn = bool (*)(UInt32 actor);
using NecroIsWalkerFn = bool (*)(UInt32 ref);

// `settings` and `invaders` are read at every step (LINK changes them);
// `testHome` ([Necromancers] TestHome, INI only, a test switch): one boss
// only, at that dungeon (DungeonCandidates.h's name), instead of the roll.
void ConfigureNecro(const NecroSettings* settings, const InvaderSettings* invaders, NecroRollFn roll,
                    NecroConsoleFn console, NecroDeadFn dead, NecroIsWalkerFn isWalker, const char* testHome,
                    float testBandsSeconds, LogFn log);

// Step 5, the teams: twice a second the walkers of a team near let go of an
// ally's or a free walker and are set on an enemy team's (NecroLogic.h);
// with [Daedra] Mode=2 a necromancer lets go of a Daedra. Every frame.
void StepNecroTeams(float dt, LogFn log);
// Whether `refId` belongs to a necromancer: a walker of a team, or a
// necromancer placed - a free walker fighting it lets go (decided 11).
bool NecroTeamed(UInt32 refId);

// Every frame (stepped once a game second inside): with [Necromancers]
// Mode=1 and the dungeon pool settled after the load, the realms risen and
// grown; their bands formed at their homes, walked on far away, put into the
// world near the player (Bands.cpp QueueWalkerBand) or at their city the
// city's siege pressure.
void StepNecroInGame(float dt, LogFn log);

// The realms as they stand, for the co-save; emptied at a load and a new game.
NecroState& CurrentNecro();
NecroBands& CurrentNecroBands();
void ResetNecroInGame();

}  // namespace walkers
