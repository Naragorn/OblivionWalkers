#include "Siege.h"

#include "Engine.h"
#include "News.h"
#include "QuestVars.h"

#include <cmath>

namespace walkers {

using namespace engine;

namespace {

constexpr UInt32 kPlayerRefId = 0x14;
constexpr UInt32 kRefPositionOffset = 0x2C;
constexpr UInt32 kRefParentCellOffset = 0x40;
constexpr UInt32 kCellFlagsOffset = 0x24;
constexpr UInt32 kCellWorldspaceOffset = 0x50;
constexpr float kStepSeconds = 1.0f;
// The player counts as at the besieged gate (no toll, the fight is seen)
// within this of its door, outdoors in Tamriel: some two exterior cells.
constexpr float kSeenUnits = 8000.0f;
// Step 3 of 4.12 (the walkers brought out before the gate) is not built.
constexpr bool kSiegeWalkersShown = false;

const SiegeSettings* g_settings = nullptr;
SiegeConsoleFn g_console = nullptr;
SiegeRollFn g_roll = nullptr;
LogFn g_log = nullptr;
UInt32 g_quest = 0;
SiegeState g_state;
float g_clock = 0.0f;
bool g_wasEnabled = true;
bool g_testDone = false;
float g_testLeft = -1.0f;  // seconds until the test siege ends, -1 none

// The ESP's quest variable `name` read after `line` ran on `door`; -1 when
// the line did not run (the variable is set to -1 first) or no quest.
double AskDoor(UInt32 door, const char* line, const char* name) {
	double* var = g_quest != 0 ? QuestVariable(g_quest, name) : nullptr;
	if (var == nullptr || g_console == nullptr) {
		return -1.0;
	}
	*var = -1.0;
	g_console(door, line);
	return *var;
}

bool Locked(UInt32 door) { return AskDoor(door, "set Walkers.siege_lock to GetLocked", "siege_lock") > 0.0; }

double LockLevel(UInt32 door) {
	return AskDoor(door, "set Walkers.siege_lock_level to GetLockLevel", "siege_lock_level");
}

UInt32 DoorOf(const CityGate& gate, UInt32 which) { return which == 0 ? gate.door : gate.back; }

void LockGate(CitySiege& c) {
	const CityGate& gate = kCityGates[c.gate];
	for (UInt32 which = 0; which < 2; ++which) {
		const UInt32 door = LookupForm(DoorOf(gate, which));
		if (!LooksLikeObject(door)) {
			g_log("Siege: %s's %s door %08X not found - not locked", gate.name, which == 0 ? "outer" : "inner",
			      DoorOf(gate, which));
			continue;
		}
		const bool was = Locked(door);
		if (!LockDoor(c, which, was)) {
			g_log("Siege: %s's %s door %08X locked already (level %.0f) - left as it was", gate.name,
			      which == 0 ? "outer" : "inner", DoorOf(gate, which), LockLevel(door));
			continue;
		}
		g_console(door, "Lock 100");
		g_log("Siege: %s's %s door %08X locked - read back: locked %d, level %.0f", gate.name,
		      which == 0 ? "outer" : "inner", DoorOf(gate, which), Locked(door) ? 1 : 0, LockLevel(door));
	}
}

void UnlockGate(CitySiege& c) {
	const CityGate& gate = kCityGates[c.gate];
	for (UInt32 which = 0; which < 2; ++which) {
		if (!UnlocksDoor(c, which)) {
			continue;
		}
		const UInt32 door = LookupForm(DoorOf(gate, which));
		if (LooksLikeObject(door)) {
			g_console(door, "Unlock");
		}
		g_log("Siege: %s's %s door %08X unlocked - read back: locked %d", gate.name, which == 0 ? "outer" : "inner",
		      DoorOf(gate, which), LooksLikeObject(door) && Locked(door) ? 1 : 0);
	}
	c.lockedByUs = 0;
}

// The player outdoors in Tamriel within kSeenUnits of the gate's door.
bool PlayerAt(const CityGate& gate) {
	const UInt32 player = LookupForm(kPlayerRefId);
	if (!LooksLikeObject(player)) {
		return false;
	}
	const UInt32 cell = Read(player + kRefParentCellOffset);
	if (!LooksLikeObject(cell) || (Read8(cell + kCellFlagsOffset) & 1) != 0) {
		return false;
	}
	const UInt32 world = Read(cell + kCellWorldspaceOffset);
	if (!LooksLikeObject(world) || FormIdOf(world) != kTamrielWorldspace) {
		return false;
	}
	const float dx = FloatAt(player + kRefPositionOffset) - gate.x;
	const float dy = FloatAt(player + kRefPositionOffset + 4) - gate.y;
	return dx * dx + dy * dy <= kSeenUnits * kSeenUnits;
}

void Carry(const SiegeEvent& ev, float now) {
	CitySiege& c = g_state.cities[ev.city];
	const CityGate& gate = kCityGates[ev.gate];
	if (ev.kind == SiegeEvent::Begin) {
		g_log("Siege: %s besieged - %u walkers (game day %.2f)", gate.name, c.walkersLeft, now);
		LockGate(c);
	} else {
		g_log("Siege: the siege at %s is over (game day %.2f), the city's next not before day %.2f", gate.name, now,
		      c.pauseUntil);
		UnlockGate(c);
	}
}

}  // namespace

void ConfigureSiege(const SiegeSettings* settings, SiegeConsoleFn console, SiegeRollFn roll, LogFn log) {
	g_settings = settings;
	g_console = console;
	g_roll = roll;
	g_log = log;
	g_log("Siege: [Siege] Enabled=%d WalkersAtGate=%u ChancePerDayPercent=%u WalkersMin=%u WalkersMax=%u "
	      "PauseDays=%.1f GuardKillsPerHour=%.1f TestGate=%d TestEndSeconds=%.0f",
	      settings->enabled ? 1 : 0, settings->walkersAtGate, settings->chancePerDayPercent, settings->walkersMin,
	      settings->walkersMax, settings->pauseDays, settings->guardKillsPerHour, settings->testGate,
	      settings->testEndSeconds);
}

void ConfigureSiegeQuest(UInt32 questFormId) { g_quest = questFormId; }

void StepSiegeInGame(float dt) {
	if (g_settings == nullptr || g_console == nullptr) {
		return;
	}
	g_clock += dt;
	if (g_clock < kStepSeconds) {
		return;
	}
	g_clock = 0.0f;
	const float now = GameDaysNow();
	if (now < 0.0f) {
		return;
	}
	SiegeEvent events[kSiegeCities + 1];
	UInt32 n = 0;
	// Switched off with a siege on: each ends, its gate unlocked.
	if (!g_settings->enabled) {
		if (g_wasEnabled) {
			for (UInt32 city = 0; city < kSiegeCities; ++city) {
				CitySiege& c = g_state.cities[city];
				if (c.active) {
					c.active = false;
					g_log("Siege: [Siege] Enabled=0 - the siege at %s ends", kCityGates[c.gate].name);
					UnlockGate(c);
				}
			}
		}
		g_wasEnabled = false;
		return;
	}
	g_wasEnabled = true;
	if (!g_testDone && g_settings->testGate >= 0) {
		g_testDone = true;
		const UInt32 row = static_cast<UInt32>(g_settings->testGate);
		if (BeginSiegeAt(g_state, *g_settings, row, now, g_roll())) {
			events[n++] = SiegeEvent{SiegeEvent::Begin, static_cast<UInt32>(kCityGates[row].city), row};
			g_testLeft = g_settings->testEndSeconds > 0.0f ? g_settings->testEndSeconds : -1.0f;
			g_log("Siege: test - TestGate=%d begins a siege, TestEndSeconds=%.0f", g_settings->testGate,
			      g_settings->testEndSeconds);
		}
	} else if (g_testLeft > 0.0f) {
		g_testLeft -= kStepSeconds;
		const UInt32 row = static_cast<UInt32>(g_settings->testGate);
		const UInt32 city = static_cast<UInt32>(kCityGates[row].city);
		if (g_testLeft <= 0.0f && n < kSiegeCities + 1) {
			g_log("Siege: test - TestEndSeconds are over, the siege ends");
			n += EndSiege(g_state.cities[city], city, *g_settings, now, events + n, kSiegeCities + 1 - n);
		}
	}
	n += StepSiegeDays(g_state, *g_settings, now, g_roll, events + n, kSiegeCities + 1 - n);
	for (UInt32 i = 0; i < n; ++i) {
		Carry(events[i], now);
	}
	for (UInt32 city = 0; city < kSiegeCities; ++city) {
		CitySiege& c = g_state.cities[city];
		if (!c.active) {
			continue;
		}
		SiegeEvent end[1];
		// Until step 3 brings the siege's walkers out before the gate, the
		// toll runs at the gate too: no walker there would ever end it.
		const bool seen = kSiegeWalkersShown && PlayerAt(kCityGates[c.gate]);
		if (StepSiegeToll(c, city, *g_settings, now, seen, end, 1) > 0) {
			Carry(end[0], now);
		}
	}
}

SiegeState& CurrentSieges() { return g_state; }

void ReleaseSiegeLocks() {
	if (g_console == nullptr) {
		return;
	}
	for (CitySiege& c : g_state.cities) {
		if (c.lockedByUs != 0) {
			g_log("Siege: before the load - %s's doors given back", kCityGates[c.gate].name);
			UnlockGate(c);
		}
	}
}

void ApplySiegeLocks() {
	if (g_console == nullptr) {
		return;
	}
	for (CitySiege& c : g_state.cities) {
		const CityGate& gate = kCityGates[c.gate];
		for (UInt32 which = 0; c.active && which < 2; ++which) {
			const UInt32 door = LookupForm(DoorOf(gate, which));
			if (!UnlocksDoor(c, which) || !LooksLikeObject(door) || Locked(door)) {
				continue;
			}
			g_console(door, "Lock 100");
			g_log("Siege: loaded - %s's %s door %08X locked again - read back: locked %d, level %.0f", gate.name,
			      which == 0 ? "outer" : "inner", DoorOf(gate, which), Locked(door) ? 1 : 0, LockLevel(door));
		}
	}
}

void ResetSiegeInGame() {
	g_clock = 0.0f;
	g_wasEnabled = true;
	g_testDone = false;
	g_testLeft = -1.0f;
}

}  // namespace walkers
