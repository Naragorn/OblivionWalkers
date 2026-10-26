#pragma once

// The horde in the game (docs/walkers-spec.md 4.11): the walkers' facts read,
// HordeLogic.h's orders carried out - the group markers moved with the
// console's MoveTo, the follow packages aimed at their leaders, the packages
// given with AddScriptPackage. And the gate guards (4.11b): GuardLogic.h's
// orders carried out on the ESP's pool of guards.

#include "GuardLogic.h"
#include "HordeLogic.h"
#include "Rise.h"

namespace walkers {

// A walker reference the rise found on the process lists.
struct HordeCandidate {
	UInt32 ref = 0;      // the reference
	bool alive = false;  // not dead (Rise.cpp IsDead)
	bool busy = false;   // in a swap of the rise's: left alone
};

using ConsoleFn = bool (*)(UInt32 ref, const char* line);
// Whether an actor reference is dead (Rise.cpp IsDead).
using DeadFn = bool (*)(UInt32 actor);
// The walker references as they stand now, at most `most`; asked for once a
// second only (the process lists are long).
using CandidatesFn = UInt32 (*)(HordeCandidate* out, UInt32 most);

// The settings and the ESP's load index, once a session (the packages and
// markers are the ESP's, tools/esp-write.py). An index above 0xFF: no horde.
void ConfigureHorde(const HordeSettings& settings, UInt32 espIndex, LogFn log);

// The gate guards' settings, once a session, after ConfigureHorde (the
// pool is the ESP's). Without the ESP's guards: no guards.
void ConfigureGuards(const GuardSettings& settings, LogFn log);

// Whether a reference is one of the ESP's gate guards (the death path
// leaves them to their pool).
bool IsGateGuard(UInt32 refId);

// Forgets every group (a load, a new game): they form again from the
// walkers as they stand; the guards are read off the world anew.
void ResetHorde();

// Every frame; the groups and the gates are looked at once a second, the
// leads given a frame after their marker moved, a guard sent out made ready
// (raised, healed) the frame after.
void StepHordeInGame(float dt, CandidatesFn candidates, ConsoleFn console, DeadFn dead, LogFn log);

// Whether a living walker of `candidates` stands within `radius` of `ref`,
// in the same interior or the same worldspace (the safeguard's check).
bool WalkerWithin(UInt32 ref, float radius, const HordeCandidate* candidates, UInt32 count);

// The distance from `ref` to the nearest living walker of `candidates` in
// the same interior or worldspace; -1 when there is none.
float NearestWalkerDistance(UInt32 ref, const HordeCandidate* candidates, UInt32 count);

}  // namespace walkers
