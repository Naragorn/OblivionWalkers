#pragma once

// The horde in the game (docs/walkers-spec.md 4.11): the walkers' facts read,
// HordeLogic.h's orders carried out - the group markers moved with the
// console's MoveTo, the follow packages aimed at their leaders, the packages
// given with AddScriptPackage.

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
// The walker references as they stand now, at most `most`; asked for once a
// second only (the process lists are long).
using CandidatesFn = UInt32 (*)(HordeCandidate* out, UInt32 most);

// The settings and the ESP's load index, once a session (the packages and
// markers are the ESP's, tools/esp-write.py). An index above 0xFF: no horde.
void ConfigureHorde(const HordeSettings& settings, UInt32 espIndex, LogFn log);

// Forgets every group (a load, a new game): they form again from the
// walkers as they stand.
void ResetHorde();

// Every frame; the groups are looked at once a second, the leads given a
// frame after their marker moved.
void StepHordeInGame(float dt, CandidatesFn candidates, ConsoleFn console, LogFn log);

}  // namespace walkers
