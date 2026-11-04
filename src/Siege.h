#pragma once

// A city's siege in the game (docs/walkers-spec.md 4.12): SiegeLogic.h's
// decisions carried out - a second's look at the game's clock, the besieged
// gate's doors locked with the engine's Lock 100 ("needs a key") and read
// back with GetLocked and GetLockLevel, unlocked at the end, the state kept
// in the co-save through Rise.cpp.

#include "Rise.h"
#include "SiegeLogic.h"

namespace walkers {

using SiegeConsoleFn = bool (*)(UInt32 ref, const char* line);
using SiegeRollFn = UInt32 (*)();
using SiegeDeadFn = bool (*)(UInt32 actor);

// Once a session, at the plugin's load: the live settings (LINK writes into
// them), the console on a reference, a draw, the log.
void ConfigureSiege(const SiegeSettings* settings, SiegeConsoleFn console, SiegeRollFn roll, SiegeDeadFn dead,
                    LogFn log);

// The ESP's base (RiseLogic.h EspFormId), once it is known: its quest's
// variables siege_lock and siege_lock_level read a door back, its pool of
// siege walkers (0x953.., esp-write.py LOCAL_SIEGE_POOL_FIRST).
void ConfigureSiegeEsp(UInt32 espBase);

// A horde holding at kCityGates row `gate` (Horde.cpp): that city besieged
// there at the next step, unless it is already or pauses.
void NoteHordeAtGate(UInt32 gate);

// A walker ended for good (4.17, Rise.cpp - also from inside the engine's
// death path, so only noted; counted at the next step): one of the siege's
// out before its gate counts off the siege. False for any other.
bool NoteSiegeWalkerEnded(UInt32 refId);

// Every frame; the clock is looked at once a second.
void StepSiegeInGame(float dt);

// The sieges kept across saves (Rise.cpp's co-save callbacks).
SiegeState& CurrentSieges();

// A new game or a load: the test switches and the clock start again.
void ResetSiegeInGame();

// Before a load or a new game replaces the sieges: the doors this session's
// sieges locked unlocked - a door's lock outlives a load the save has no
// change for (siege-lock.txt 2026-11-04 16:02: still locked after a reload).
void ReleaseSiegeLocks();

// After the co-save is read: the doors its sieges locked locked again
// (those the save left unlocked).
void ApplySiegeLocks();

}  // namespace walkers
