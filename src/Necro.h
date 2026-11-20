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

// `settings` and `invaders` are read at every step (LINK changes them);
// `testHome` ([Necromancers] TestHome, INI only, a test switch): one boss
// only, at that dungeon (DungeonCandidates.h's name), instead of the roll.
void ConfigureNecro(const NecroSettings* settings, const InvaderSettings* invaders, NecroRollFn roll,
                    const char* testHome, LogFn log);

// Every frame (stepped once a game second inside): with [Necromancers]
// Mode=1 and the dungeon pool settled after the load, the realms risen,
// grown, their bands due logged (the bands come with step 4).
void StepNecroInGame(float dt, LogFn log);

// The realms as they stand, for the co-save; emptied at a load and a new game.
NecroState& CurrentNecro();
void ResetNecroInGame();

}  // namespace walkers
