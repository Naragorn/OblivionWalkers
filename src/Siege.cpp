#include "Siege.h"

#include "Engine.h"
#include "LinkLogic.h"
#include "News.h"
#include "QuestVars.h"
#include "Rise.h"
#include "RiseLogic.h"

#include <cstdio>

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
// The ESP's siege walkers (tools/esp-write.py LOCAL_SIEGE_POOL_FIRST).
constexpr UInt32 kSiegePoolFirstLocalId = 0x953;
constexpr UInt32 kSiegePool = kSiegeAtGateMost;
constexpr UInt32 kSiegeLogLines = 24;
// The ESP's GLOB per city (tools/esp-write.py LOCAL_SIEGE_GLOBAL_FIRST), 1
// while it is besieged: the Rumors topic's siege lines are said on it.
constexpr UInt32 kSiegeGlobalLocalId = 0x9C7;

const SiegeSettings* g_settings = nullptr;
SiegeConsoleFn g_console = nullptr;
SiegeDeadFn g_dead = nullptr;
UInt32 g_espBase = 0;
// The pool: out before the shown gate, or lying there ended (a corpse until
// the siege is hidden), and which gate is shown (-1 none).
bool g_out[kSiegePool] = {};
bool g_corpse[kSiegePool] = {};
int g_shownGate = -1;
UInt32 g_shownLines = 0;
// The siege's walkers ended since the last step (NoteSiegeWalkerEnded).
UInt32 g_ended[32] = {};
UInt32 g_endedCount = 0;
int g_hordeGate = -1;  // a horde holding at this gate row since the last step
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


// The gate stays open (Nadi, 2026-11-04: "als player würde ich schon gerne
// die stadt verlassen können um mitzuhelfen bei der belagerung" - go A):
// a siege locks nothing. A gate a build of 2026-11-04 locked (the co-save's
// bit) is unlocked: its outer door, whose lock the inner door shares
// (siege-door-read.txt 20:36), both read back.
void UnlockGate(CitySiege& c) {
	const CityGate& gate = kCityGates[c.gate];
	if (UnlocksDoor(c, 0)) {
		const UInt32 outer = LookupForm(gate.door);
		const UInt32 inner = LookupForm(gate.back);
		if (LooksLikeObject(outer)) {
			g_console(outer, "Unlock");
		}
		g_log("Siege: %s unlocked - read back: outer %08X locked %d, inner %08X locked %d", gate.name, gate.door,
		      LooksLikeObject(outer) && Locked(outer) ? 1 : 0, gate.back,
		      LooksLikeObject(inner) && Locked(inner) ? 1 : 0);
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

// The city's rumor global set: 1 besieged, 0 not.
void SetRumor(UInt32 city, bool besieged) {
	const UInt32 global = g_espBase != 0 ? LookupForm(EspFormId(g_espBase, kSiegeGlobalLocalId + city)) : 0;
	if (!LooksLikeObject(global) || Read8(global + kFormTypeOffset) != kFormTypeGlobal) {
		g_log("Siege: no rumor global %08X for city %u (an older ESP) - nothing said",
		      EspFormId(g_espBase, kSiegeGlobalLocalId + city), city);
		return;
	}
	*reinterpret_cast<float*>(global + kGlobalValueOffset) = besieged ? 1.0f : 0.0f;
	g_log("Siege: rumor global %08X (city %u) now %.0f", FormIdOf(global), city,
	      FloatAt(global + kGlobalValueOffset));
}

UInt32 PoolRef(UInt32 slot) {
	return g_espBase != 0 ? LookupForm(EspFormId(g_espBase, kSiegePoolFirstLocalId + slot)) : 0;
}

// The pool back in: every walker out or lying before the gate disabled.
void HideSiege() {
	UInt32 hidden = 0;
	for (UInt32 i = 0; i < kSiegePool; ++i) {
		if (!g_out[i] && !g_corpse[i]) {
			continue;
		}
		const UInt32 ref = PoolRef(i);
		if (LooksLikeObject(ref)) {
			g_console(ref, "Disable");
		}
		g_out[i] = false;
		g_corpse[i] = false;
		++hidden;
	}
	if (g_shownGate >= 0) {
		g_log("Siege: the walkers before %s back in the pool (%u)", kCityGates[g_shownGate].name, hidden);
	}
	g_shownGate = -1;
}

// Before the besieged gate: as many out as WalkersAtGateNow wants, a slot
// never used first, else a corpse's - raised (SetDestroyed 0 against NoLoot,
// Resurrect), enabled and moved onto its spot.
void ShowSiege(const CitySiege& c) {
	if (g_shownGate != static_cast<int>(c.gate)) {
		HideSiege();
		g_shownGate = static_cast<int>(c.gate);
		g_shownLines = 0;
		g_log("Siege: the player at %s - its walkers come out (%u left, %u at once)", kCityGates[c.gate].name,
		      c.walkersLeft, WalkersAtGateNow(c, *g_settings));
	}
	UInt32 out = 0;
	for (UInt32 i = 0; i < kSiegePool; ++i) {
		out += g_out[i] ? 1u : 0u;
	}
	const UInt32 wanted = WalkersAtGateNow(c, *g_settings);
	const CityGate& gate = kCityGates[c.gate];
	for (UInt32 pass = 0; pass < 2 && out < wanted; ++pass) {
		for (UInt32 i = 0; i < kSiegePool && out < wanted; ++i) {
			if (g_out[i] || (pass == 0 && g_corpse[i]) || (pass == 1 && !g_corpse[i])) {
				continue;
			}
			const UInt32 ref = PoolRef(i);
			if (!LooksLikeObject(ref)) {
				continue;
			}
			const bool raised = g_dead != nullptr && g_dead(ref);
			if (raised) {
				g_console(ref, "SetDestroyed 0");
				g_console(ref, "Resurrect");
				NoteRaised(ref, "the siege pool");
			}
			float x = 0.0f;
			float y = 0.0f;
			SiegeSpot(gate, i, x, y);
			char line[96];
			std::snprintf(line, sizeof(line), "MoveTo %08X %.0f %.0f %.0f", gate.door, x - gate.x, y - gate.y,
			              gate.outZ - gate.z);
			g_console(ref, "Enable");
			const bool moved = g_console(ref, line);
			g_out[i] = true;
			g_corpse[i] = false;
			++out;
			if (g_shownLines < kSiegeLogLines) {
				++g_shownLines;
				g_log("Siege: walker %08X out before %s (slot %u%s; %s answered %d)", FormIdOf(ref), gate.name, i,
				      raised ? ", raised" : "", line, moved ? 1 : 0);
			}
		}
	}
}

void Carry(const SiegeEvent& ev, float now) {
	CitySiege& c = g_state.cities[ev.city];
	const CityGate& gate = kCityGates[ev.gate];
	if (ev.kind == SiegeEvent::Begin) {
		g_log("Siege: %s besieged - %u walkers (game day %.2f), its gate open", gate.name, c.walkersLeft, now);
		NoteSiegeInNews(ev.city, true);
		SetRumor(ev.city, true);
	} else {
		g_log("Siege: the siege at %s is over (game day %.2f), the city's next not before day %.2f", gate.name, now,
		      c.pauseUntil);
		UnlockGate(c);
		NoteSiegeInNews(ev.city, false);
		SetRumor(ev.city, false);
		if (g_shownGate == static_cast<int>(ev.gate)) {
			HideSiege();
		}
	}
}

}  // namespace

void ConfigureSiege(const SiegeSettings* settings, SiegeConsoleFn console, SiegeRollFn roll, SiegeDeadFn dead,
                    LogFn log) {
	g_settings = settings;
	g_console = console;
	g_dead = dead;
	g_roll = roll;
	g_log = log;
	g_log("Siege: [Siege] Enabled=%d WalkersAtGate=%u ChancePerDayPercent=%u WalkersMin=%u WalkersMax=%u "
	      "PauseDays=%.1f GuardKillsPerHour=%.1f TestGate=%d TestEndSeconds=%.0f",
	      settings->enabled ? 1 : 0, settings->walkersAtGate, settings->chancePerDayPercent, settings->walkersMin,
	      settings->walkersMax, settings->pauseDays, settings->guardKillsPerHour, settings->testGate,
	      settings->testEndSeconds);
}

void NoteHordeAtGate(UInt32 gate) {
	if (gate < kCityGateCount) {
		g_hordeGate = static_cast<int>(gate);
	}
}

void ConfigureSiegeEsp(UInt32 espBase) {
	g_espBase = espBase;
	g_quest = EspFormId(espBase, kLinkQuestLocalId);
}

bool NoteSiegeWalkerEnded(UInt32 refId) {
	if (g_espBase == 0 || g_shownGate < 0 || g_settings == nullptr) {
		return false;
	}
	const UInt32 local = refId & kEspLocalMask;
	if (EspFormId(g_espBase, local) != refId || local < kSiegePoolFirstLocalId ||
	    local >= kSiegePoolFirstLocalId + kSiegePool || !g_out[local - kSiegePoolFirstLocalId]) {
		return false;
	}
	if (g_endedCount < 32) {
		g_ended[g_endedCount++] = refId;
	}
	return true;
}

namespace {

void CountEnded(UInt32 refId) {
	const UInt32 slot = (refId & kEspLocalMask) - kSiegePoolFirstLocalId;
	if (g_shownGate < 0 || !g_out[slot]) {
		return;
	}
	g_out[slot] = false;
	g_corpse[slot] = true;
	const UInt32 city = static_cast<UInt32>(kCityGates[g_shownGate].city);
	CitySiege& c = g_state.cities[city];
	SiegeEvent end[1];
	const float now = GameDaysNow();
	const bool over = SiegeWalkerEnded(c, city, *g_settings, now, end, 1) > 0;
	g_log("Siege: walker %08X ended before %s - %u left%s", refId, kCityGates[g_shownGate].name, c.walkersLeft,
	      over ? ", the siege is over" : "");
	if (over) {
		Carry(end[0], now);
	}
}

}  // namespace

void StepSiegeInGame(float dt) {
	if (g_settings == nullptr || g_console == nullptr) {
		return;
	}
	g_clock += dt;
	if (g_clock < kStepSeconds) {
		return;
	}
	g_clock = 0.0f;
	static float s_readClock = 0.0f;
	s_readClock += kStepSeconds;
	if (g_settings->testReadDoor != 0 && s_readClock >= 5.0f) {
		s_readClock = 0.0f;
		const UInt32 door = LookupForm(g_settings->testReadDoor);
		const UInt32 player = LookupForm(kPlayerRefId);
		const UInt32 cell = LooksLikeObject(player) ? Read(player + kRefParentCellOffset) : 0;
		const UInt32 doorCell = LooksLikeObject(door) ? Read(door + kRefParentCellOffset) : 0;
		g_log("Siege: test - door %08X locked %d, level %.0f; its cell %08X (3D %d), the player's %08X",
		      g_settings->testReadDoor, LooksLikeObject(door) && Locked(door) ? 1 : 0,
		      LooksLikeObject(door) ? LockLevel(door) : -2.0, FormIdOf(doorCell),
		      LooksLikeObject(door) && Read(door + 0x3C) != 0 ? 1 : 0, FormIdOf(cell));
	}
	const float now = GameDaysNow();
	if (now < 0.0f) {
		return;
	}
	for (UInt32 i = 0; i < g_endedCount; ++i) {
		CountEnded(g_ended[i]);
	}
	g_endedCount = 0;
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
					NoteSiegeInNews(city, false);
					SetRumor(city, false);
				}
			}
			HideSiege();
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
	if (g_hordeGate >= 0 && n < kSiegeCities + 1) {
		const UInt32 row = static_cast<UInt32>(g_hordeGate);
		if (BeginSiegeAt(g_state, *g_settings, row, now, g_roll())) {
			events[n++] = SiegeEvent{SiegeEvent::Begin, static_cast<UInt32>(kCityGates[row].city), row};
			g_log("Siege: a horde holds at %s - the city is besieged there", kCityGates[row].name);
		}
	}
	g_hordeGate = -1;
	n += StepSiegeDays(g_state, *g_settings, now, g_roll, events + n, kSiegeCities + 1 - n);
	for (UInt32 i = 0; i < n; ++i) {
		Carry(events[i], now);
	}
	bool shown = false;
	for (UInt32 city = 0; city < kSiegeCities; ++city) {
		CitySiege& c = g_state.cities[city];
		if (!c.active) {
			continue;
		}
		SiegeEvent end[1];
		const bool seen = g_espBase != 0 && PlayerAt(kCityGates[c.gate]);
		if (StepSiegeToll(c, city, *g_settings, now, seen, end, 1) > 0) {
			Carry(end[0], now);
			continue;
		}
		if (seen) {
			ShowSiege(c);
			shown = true;
		}
	}
	if (!shown && g_shownGate >= 0) {
		HideSiege();
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
	// The rumors as the co-save's sieges are.
	for (UInt32 city = 0; g_espBase != 0 && city < kSiegeCities; ++city) {
		SetRumor(city, g_state.cities[city].active);
	}
	// A save made with the siege shown holds its walkers enabled: all back in
	// the pool, the siege brings out its own again.
	for (UInt32 i = 0; i < kSiegePool; ++i) {
		const UInt32 ref = PoolRef(i);
		if (LooksLikeObject(ref)) {
			g_console(ref, "Disable");
		}
	}
	// A gate an earlier build locked (the co-save's bit): opened.
	for (CitySiege& c : g_state.cities) {
		if (c.lockedByUs != 0) {
			g_log("Siege: loaded - %s was locked by an earlier build, opened", kCityGates[c.gate].name);
			UnlockGate(c);
		}
	}
}

void ResetSiegeInGame() {
	g_clock = 0.0f;
	for (UInt32 i = 0; i < kSiegePool; ++i) {
		g_out[i] = false;
		g_corpse[i] = false;
	}
	g_shownGate = -1;
	g_endedCount = 0;
	g_wasEnabled = true;
	g_testDone = false;
	g_testLeft = -1.0f;
}

}  // namespace walkers
