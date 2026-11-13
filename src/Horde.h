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

// The most walker references looked at once: 21 guards at a gate take 101
// walkers near it (ZombiesPerGuard 5), and more may stand elsewhere.
inline constexpr UInt32 kHordeCandidatesMost = 256;

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

// The settings and the ESP's base (RiseLogic.h EspFormId), once a session
// (the packages and markers are the ESP's, tools/esp-write.py). kNoEsp: no
// horde.
void ConfigureHorde(const HordeSettings& settings, UInt32 espBase, LogFn log);

// 4.20.5 (NightLogic.h): the horde's marker `factor` times LeadUnits ahead -
// farther at night; 1 by day. Kept across ConfigureHorde.
void SetHordeNightLead(float factor);

// 4.20.2: a noise at `ref` (a hit actor, a caster) - outdoors only; heard
// at the next look at the groups. `source` names it in the log.
void HordeNoiseAtRef(UInt32 ref, const char* source);
// A noise at x, y, z in the player's worldspace (OBVR's 'WNOI', a throw).
void HordeNoiseNearPlayer(float x, float y, float z, const char* source);

// The gate guards' settings, once a session, after ConfigureHorde (the
// pool is the ESP's). Without the ESP's guards: no guards.
void ConfigureGuards(const GuardSettings& settings, LogFn log);

// Whether a reference is one of the ESP's gate guards (the death path
// leaves them to their pool).
bool IsGateGuard(UInt32 refId);

// The package an actor runs now, 0 none; `high` its process runs packages.
UInt32 ActorPackageNow(UInt32 actorId, bool& high);
bool ActorInCombat(UInt32 actorId);
UInt8 PackageType(UInt32 packageId);

// The city the biggest group of walkers near the player heads for, -1 none
// (the news, 4.9).
int HordeHeadingCity();

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

// The land's height at x, y in the loaded cells (the engine's
// TES::GetTerrainHeight); false where no loaded cell is.
bool LandHeightAt(float x, float y, float& z);

// The distance from `ref` to the nearest living walker of `candidates` in
// the same interior or worldspace; -1 when there is none.
float NearestWalkerDistance(UInt32 ref, const HordeCandidate* candidates, UInt32 count);

}  // namespace walkers
