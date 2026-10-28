#include "Horde.h"

#include "CityGates.h"
#include "Engine.h"
#include "News.h"

#include <cmath>
#include <cstdio>

namespace walkers {

using namespace engine;

namespace {

// Read in Oblivion.exe 1.2.416 and proven in the game by OBVR's leading by
// the hand (OBVR src/game/Lead.cpp, 2026-09-29), which gives a Follow
// package the same way and reads the same state:
// - The actor's process at +0x58; its level through the process's vtable
//   +0x08 (0: high, the one that runs packages on its own); the package it
//   runs through the process's vtable +0x184.
// - Fighting: the actor's vtable +0x334, thiscall(actor, bool).
// - A package's target data at +0x28: a type byte (0: a reference), the
//   reference at +4, the follow distance at +8 (xOBSE GameForms.h
//   TESPackage::TargetData).
// - Outdoors: the reference's cell at +0x40; a cell whose flags at +0x24
//   carry bit 0 is an interior (0x004C9CF0, which TESObjectREFR's worldspace
//   getter 0x004D6670 calls - read with REA 2026-10-26).
constexpr UInt32 kActorProcessOffset = 0x58;
constexpr UInt32 kProcessLevelSlot = 0x08;
constexpr UInt32 kProcessCurrentPackageSlot = 0x184;
constexpr UInt32 kActorIsInCombatSlot = 0x334;
constexpr UInt32 kPackageTargetOffset = 0x28;
constexpr UInt32 kRefParentCellOffset = 0x40;
constexpr UInt32 kCellFlagsOffset = 0x24;
// An exterior cell's worldspace at +0x50 (0x004C9CF0).
constexpr UInt32 kCellWorldspaceOffset = 0x50;
constexpr UInt32 kRefPositionOffset = 0x2C;
constexpr UInt32 kRefNiNodeOffset = 0x3C;
constexpr UInt32 kTextStart = 0x00401000;
constexpr UInt32 kTextEnd = 0x00A27C39;
// The ESP's horde records (tools/esp-write.py): the travel packages, the
// follow packages and the markers, one of each per group.
constexpr UInt32 kTravelLocalId = 0x806;
constexpr UInt32 kFollowLocalId = 0x816;
constexpr UInt32 kMarkerLocalId = 0x827;
constexpr UInt32 kFormTypePackage = 0x3D;
constexpr UInt32 kCandidatesMost = kHordeCandidatesMost;
constexpr float kStepSeconds = 1.0f;

HordeSettings g_settings;
HordeState g_state;
UInt32 g_espBase = kNoEsp;
UInt32 g_seed = 0x9E3779B9u;
float g_clock = 0.0f;
bool g_ready = false;
// The leads given a frame after their marker moved: a MoveTo and an order
// aimed at the moved reference in one frame do not take (the CS wiki's
// MoveTo notes).
HordeOrder g_pending[kHordeGroups];
UInt32 g_pendingCount = 0;
UInt32 g_logsLeft = 1500;
// A package given again is held back this long per walker: a walker whose
// package something else keeps taking would otherwise get ours anew every
// second, each time starting its path over (the run of 2026-10-26 08:33).
constexpr float kAgainSeconds = 5.0f;
constexpr UInt32 kAgainMost = 128;
UInt32 g_againIds[kAgainMost] = {};
float g_againAge[kAgainMost] = {};
UInt32 g_againCount = 0;
UInt32 g_steps = 0;
UInt32 g_summariesLeft = 60;
constexpr UInt32 kSummaryEverySteps = 5;

// The gate guards (4.11b; tools/esp-write.py): one guard record per city,
// 21 persistent references of it per city in the holding cell, initially
// disabled, in CityGates.h's order (GuardLogic.h PoolLocalId).
constexpr UInt32 kGuardNpcLocalId = 0x837;
constexpr UInt32 kRefBaseFormOffset = 0x1C;
// TESActorBase's TESHealthForm (Replace.cpp ScaleWalkerHealth writes the
// walkers' there).
constexpr UInt32 kActorBaseHealthOffset = 0x84;
// TESObjectREFR::kFlags_Disabled in the form flags (xOBSE GameObjects.h,
// IsDisabled).
constexpr UInt32 kRefFlagsOffset = 0x08;
constexpr UInt32 kRefDisabled = 0x800;
// Health now and base, as OBVR reads its enemies' (OBVR
// src/game/EnemyHealth.cpp, in the game since 2026-10-13): GetActorValue_F
// at the actor's vtable +0x288 and GetBaseActorValue 0x005F1910, both
// thiscall (actor, value), Health the value 8.
constexpr UInt32 kGetActorValueFSlot = 0x288;
constexpr UInt32 kGetBaseActorValue = 0x005F1910;
constexpr UInt32 kActorValueHealth = 8;
constexpr UInt32 kGuardArrivalsMost = 16;

GuardSettings g_guardSettings;
GuardState g_guards;
bool g_guardsReady = false;
bool g_guardsRead = false;  // the pool read off the world since the last load
struct GuardArrival {
	UInt32 city = 0;
	UInt32 slot = 0;
	int gate = -1;
	bool raise = false;
	UInt32 health = 0;
};
// The guards sent out this frame, made ready the next: raised when dead,
// healed to their (new) health.
GuardArrival g_arrivals[kGuardArrivalsMost];
UInt32 g_arrivalCount = 0;
UInt32 g_guardLogsLeft = 400;

UInt32 Slot(UInt32 object, UInt32 offset) {
	const UInt32 vtable = LooksLikeObject(object) ? Read(object) : 0;
	const UInt32 fn = LooksLikeObject(vtable) ? Read(vtable + offset) : 0;
	return fn >= kTextStart && fn < kTextEnd ? fn : 0;
}

UInt32 ProcessOf(UInt32 actor) {
	const UInt32 process = Read(actor + kActorProcessOffset);
	return LooksLikeObject(process) ? process : 0;
}

bool HighProcess(UInt32 process) {
	const UInt32 fn = Slot(process, kProcessLevelSlot);
	using LevelFn = UInt32(__fastcall*)(void* process, void* edx);
	return fn != 0 && reinterpret_cast<LevelFn>(fn)(reinterpret_cast<void*>(process), nullptr) == 0;
}

UInt32 CurrentPackageId(UInt32 process) {
	const UInt32 fn = Slot(process, kProcessCurrentPackageSlot);
	using PackageFn = UInt32(__fastcall*)(void* process, void* edx);
	const UInt32 package = fn != 0 ? reinterpret_cast<PackageFn>(fn)(reinterpret_cast<void*>(process), nullptr) : 0;
	return LooksLikeObject(package) ? FormIdOf(package) : 0;
}

bool InCombat(UInt32 actor) {
	const UInt32 fn = Slot(actor, kActorIsInCombatSlot);
	using CombatFn = bool(__fastcall*)(void* actor, void* edx, bool unk);
	return fn != 0 && reinterpret_cast<CombatFn>(fn)(reinterpret_cast<void*>(actor), nullptr, true);
}

bool Outdoors(UInt32 ref) {
	const UInt32 cell = Read(ref + kRefParentCellOffset);
	return LooksLikeObject(cell) && (Read8(cell + kCellFlagsOffset) & 1) == 0;
}

// Outdoors in Tamriel itself, where the city gates are.
bool InTamriel(UInt32 ref) {
	const UInt32 cell = Read(ref + kRefParentCellOffset);
	if (!LooksLikeObject(cell) || (Read8(cell + kCellFlagsOffset) & 1) != 0) {
		return false;
	}
	const UInt32 world = Read(cell + kCellWorldspaceOffset);
	return LooksLikeObject(world) && FormIdOf(world) == kTamrielWorldspace;
}

// A rank of its own per walker that stays the same across frames and loads.
UInt32 RankOf(UInt32 id) {
	UInt32 h = id * 2654435761u;
	h ^= h >> 15;
	return h * 2246822519u;
}

UInt32 PackageForm(UInt32 id) {
	const UInt32 form = LookupForm(id);
	return LooksLikeObject(form) && Read8(form + kFormTypeOffset) == kFormTypePackage ? form : 0;
}

// The group's follow package aimed at its leader, as xOBSE's SetPackageTarget
// writes it.
bool AimFollow(UInt32 group, UInt32 leaderId) {
	const UInt32 package = PackageForm(g_settings.followBase + group);
	const UInt32 target = package != 0 ? Read(package + kPackageTargetOffset) : 0;
	const UInt32 leader = LookupForm(leaderId);
	if (!LooksLikeObject(target) || !LooksLikeObject(leader)) {
		return false;
	}
	*reinterpret_cast<UInt8*>(target) = 0;
	*reinterpret_cast<UInt32*>(target + 4) = leader;
	return true;
}

// Whether a package given again to `walkerId` is held back now; when not,
// the clock starts anew.
bool HoldBack(UInt32 walkerId) {
	UInt32 oldest = 0;
	for (UInt32 i = 0; i < g_againCount; ++i) {
		if (g_againIds[i] == walkerId) {
			if (g_againAge[i] < kAgainSeconds) {
				return true;
			}
			g_againAge[i] = 0.0f;
			return false;
		}
		oldest = g_againAge[i] > g_againAge[oldest] ? i : oldest;
	}
	const UInt32 slot = g_againCount < kAgainMost ? g_againCount++ : oldest;
	g_againIds[slot] = walkerId;
	g_againAge[slot] = 0.0f;
	return false;
}

bool GivePackage(UInt32 walkerId, UInt32 package, ConsoleFn console) {
	const UInt32 walker = LookupForm(walkerId);
	char line[48];
	std::snprintf(line, sizeof(line), "AddScriptPackage %08X", package);
	return LooksLikeObject(walker) && console(walker, line);
}

// Whether a guard line is still logged (a few hundred a session).
bool GuardLogDue() {
	if (g_guardLogsLeft == 0) {
		return false;
	}
	--g_guardLogsLeft;
	return true;
}

UInt32 GuardRecordOf(UInt32 city) {
	const UInt32 form = LookupForm(EspFormId(g_espBase, kGuardNpcLocalId + city));
	return LooksLikeObject(form) && Read8(form + kFormTypeOffset) == kFormTypeNpc ? form : 0;
}

// The pool guard's reference; 0 unless it is one of the city's guard.
UInt32 PoolGuard(UInt32 city, UInt32 slot) {
	const UInt32 ref = LookupForm(EspFormId(g_espBase, PoolLocalId(city, slot)));
	const UInt32 record = GuardRecordOf(city);
	return LooksLikeObject(ref) && record != 0 && Read(ref + kRefBaseFormOffset) == record ? ref : 0;
}

bool HealthOf(UInt32 actor, float& now, float& base) {
	const UInt32 fn = Slot(actor, kGetActorValueFSlot);
	if (fn == 0) {
		return false;
	}
	using GetFloatFn = float(__fastcall*)(void* actor, void* edx, UInt32 av);
	using GetBaseFn = SInt32(__fastcall*)(void* actor, void* edx, UInt32 av);
	now = reinterpret_cast<GetFloatFn>(fn)(reinterpret_cast<void*>(actor), nullptr, kActorValueHealth);
	base = static_cast<float>(
		reinterpret_cast<GetBaseFn>(kGetBaseActorValue)(reinterpret_cast<void*>(actor), nullptr, kActorValueHealth));
	return true;
}

// After a load: who of the pool stands out at a gate (enabled), read off
// the world.
void ReadGuardsOffWorld(LogFn log) {
	bool enabled[kGuardCities][kGuardPool] = {};
	float xs[kGuardCities][kGuardPool] = {};
	float ys[kGuardCities][kGuardPool] = {};
	UInt32 out = 0;
	for (UInt32 c = 0; c < kGuardCities; ++c) {
		for (UInt32 i = 0; i < kGuardPool; ++i) {
			const UInt32 guard = PoolGuard(c, i);
			enabled[c][i] = guard != 0 && (Read(guard + kRefFlagsOffset) & kRefDisabled) == 0 && InTamriel(guard);
			xs[c][i] = guard != 0 ? FloatAt(guard + kRefPositionOffset) : 0.0f;
			ys[c][i] = guard != 0 ? FloatAt(guard + kRefPositionOffset + 4) : 0.0f;
			out += enabled[c][i] ? 1u : 0u;
		}
	}
	RecoverGuardState(g_guards, enabled, xs, ys, kCityGates, kCityGateCount);
	log("Guards: the pool read off the world - %u out at a gate, the rest waiting", out);
}

// The nearest living walker in Tamriel within the gate's radius; 0 if none.
UInt32 NearestWalker(const CityGate& gate, const HordeWalker* walkers, const bool* alive, UInt32 n) {
	UInt32 best = 0;
	float bestDistance = g_guardSettings.radius * g_guardSettings.radius;
	for (UInt32 i = 0; i < n; ++i) {
		const float dx = walkers[i].x - gate.x;
		const float dy = walkers[i].y - gate.y;
		const float d = dx * dx + dy * dy;
		if (alive[i] && walkers[i].tamriel && d <= bestDistance) {
			best = walkers[i].id;
			bestDistance = d;
		}
	}
	return best;
}

// The guards sent out last frame: raised when dead (without the get-up -
// they come out standing), healed to their health.
void ReadyArrivals(ConsoleFn console, DeadFn dead, LogFn log) {
	for (UInt32 a = 0; a < g_arrivalCount; ++a) {
		const GuardArrival& arrival = g_arrivals[a];
		const UInt32 guard = PoolGuard(arrival.city, arrival.slot);
		if (guard == 0) {
			continue;
		}
		const bool wasDead = dead(guard);
		const bool raised = wasDead && console(guard, "Resurrect 0");
		const bool healed = console(guard, "ResetHealth");
		float now = 0.0f;
		float base = 0.0f;
		const bool read = HealthOf(guard, now, base);
		if (GuardLogDue()) {
			log("Guards: %08X ready at %s (%.0f %.0f %.0f) - %s; health %.0f of %.0f (wanted %u%s; ResetHealth answered "
			    "%d)",
			    FormIdOf(guard), kCityGates[arrival.gate].name, FloatAt(guard + kRefPositionOffset),
			    FloatAt(guard + kRefPositionOffset + 4), FloatAt(guard + kRefPositionOffset + 8),
			    wasDead ? (raised ? "raised" : "dead, Resurrect refused") : "standing", read ? now : -1.0f,
			    read ? base : -1.0f, arrival.health, read && base != static_cast<float>(arrival.health) ? " - WARNING: base differs" : "",
			    healed ? 1 : 0);
		}
	}
	g_arrivalCount = 0;
}

// One look at the gates: GuardLogic.h's orders carried out, the guards out
// sent at the walkers.
void StepGateGuards(float elapsed, const HordeWalker* walkers, const bool* alive, UInt32 n, UInt32 walkerHealth,
                    ConsoleFn console, DeadFn dead, LogFn log) {
	if (!g_guardsRead) {
		ReadGuardsOffWorld(log);
		g_guardsRead = true;
	}
	float xs[kCandidatesMost];
	float ys[kCandidatesMost];
	bool there[kCandidatesMost];
	for (UInt32 i = 0; i < n; ++i) {
		xs[i] = walkers[i].x;
		ys[i] = walkers[i].y;
		there[i] = alive[i] && walkers[i].tamriel;
	}
	UInt32 gateCity[kCityGateCount];
	UInt32 zombies[kCityGateCount];
	for (UInt32 g = 0; g < kCityGateCount; ++g) {
		gateCity[g] = static_cast<UInt32>(kCityGates[g].city);
		zombies[g] = WalkersNear(kCityGates[g].x, kCityGates[g].y, g_guardSettings.radius, xs, ys, there, n);
	}
	bool standing[kGuardCities][kGuardPool];
	for (UInt32 c = 0; c < kGuardCities; ++c) {
		for (UInt32 i = 0; i < kGuardPool; ++i) {
			const UInt32 guard = PoolGuard(c, i);
			standing[c][i] = guard == 0 || !dead(guard);
		}
	}
	// The news (4.9): a city whose guards are out at a gate with walkers
	// near it is attacked, now.
	for (UInt32 c = 0; c < kGuardCities; ++c) {
		if (CityAttacked(g_guards, c, zombies, kCityGateCount)) {
			NoteGateAttack(c);
		}
	}
	GuardOrder orders[kGuardArrivalsMost];
	const UInt32 ordered = StepGuards(g_guards, g_guardSettings, gateCity, kCityGateCount, zombies, standing, elapsed,
	                                  orders, kGuardArrivalsMost);
	bool sent[kGuardCities][kGuardPool] = {};
	for (UInt32 o = 0; o < ordered; ++o) {
		const GuardOrder& order = orders[o];
		const UInt32 guard = PoolGuard(order.city, order.slot);
		const CityGate& gate = kCityGates[order.gate];
		if (guard == 0) {
			continue;
		}
		if (order.kind == GuardOrder::Kill) {
			const bool killed = console(guard, "Kill");
			if (GuardLogDue()) {
				log("Guards: TEST - %08X killed after %.0f s out at %s (TestKillSeconds; Kill answered %d)",
				    FormIdOf(guard), g_guardSettings.testKillSeconds, gate.name, killed ? 1 : 0);
			}
			continue;
		}
		if (order.kind == GuardOrder::Recall) {
			const bool gone = console(guard, "Disable");
			if (GuardLogDue()) {
				log("Guards: %08X back in from %s (no walker near for %.0f s; Disable answered %d)", FormIdOf(guard),
				    gate.name, g_guardSettings.clearSeconds, gone ? 1 : 0);
			}
			continue;
		}
		const UInt32 health = GuardHealth(g_guardSettings.healthFactor, walkerHealth);
		const UInt32 record = GuardRecordOf(order.city);
		if (record != 0 && walkerHealth > 0) {
			*reinterpret_cast<UInt32*>(record + kActorBaseHealthOffset) = health;
		}
		float x = 0.0f;
		float y = 0.0f;
		GuardSpot(gate, order.slot, x, y);
		char line[96];
		std::snprintf(line, sizeof(line), "MoveTo %08X %.0f %.0f %.0f", gate.door, x - gate.x, y - gate.y,
		              gate.outZ - gate.z);
		const bool shown = console(guard, "Enable");
		const bool moved = console(guard, line);
		sent[order.city][order.slot] = true;
		if (g_arrivalCount < kGuardArrivalsMost) {
			g_arrivals[g_arrivalCount++] = GuardArrival{order.city, order.slot, order.gate, order.raise, health};
		}
		if (GuardLogDue()) {
			log("Guards: %08X out at %s for %u walkers near (%s; health %u = %.2f x a walker's %u; Enable %d, %s "
			    "answered %d)",
			    FormIdOf(guard), gate.name, zombies[order.gate], order.raise ? "fallen there before, raised" : "waiting",
			    health, g_guardSettings.healthFactor, walkerHealth, shown ? 1 : 0, line, moved ? 1 : 0);
		}
	}
	// The guards out, standing and not fighting, sent at the nearest walker.
	for (UInt32 c = 0; c < kGuardCities; ++c) {
		for (UInt32 i = 0; i < kGuardPool; ++i) {
			const GuardSlot& slot = g_guards.slots[c][i];
			if (!slot.out || sent[c][i] || slot.gate < 0 || zombies[slot.gate] == 0) {
				continue;
			}
			const UInt32 guard = PoolGuard(c, i);
			if (guard == 0 || dead(guard) || Read(guard + kRefNiNodeOffset) == 0 || InCombat(guard)) {
				continue;
			}
			const UInt32 target = NearestWalker(kCityGates[slot.gate], walkers, alive, n);
			if (target == 0) {
				continue;
			}
			char line[40];
			std::snprintf(line, sizeof(line), "StartCombat %08X", target);
			const bool fights = console(guard, line);
			if (GuardLogDue()) {
				log("Guards: %08X at %s sent at walker %08X (StartCombat answered %d)", FormIdOf(guard),
				    kCityGates[slot.gate].name, target, fights ? 1 : 0);
			}
		}
	}
}

}  // namespace

void ConfigureHorde(const HordeSettings& settings, UInt32 espBase, LogFn log) {
	g_settings = settings;
	g_espBase = espBase;
	g_ready = false;
	if (!settings.enabled) {
		return;
	}
	if (espBase == kNoEsp) {
		log("Horde: no ESP - the walkers keep their own AI");
		return;
	}
	g_settings.gates = HordeGates();
	g_settings.gateCount = kCityGateCount;
	g_settings.travelBase = EspFormId(espBase, kTravelLocalId);
	g_settings.followBase = EspFormId(espBase, kFollowLocalId);
	UInt32 missing = 0;
	for (UInt32 g = 0; g < kHordeGroups; ++g) {
		missing += PackageForm(g_settings.travelBase + g) == 0 ? 1u : 0u;
		missing += PackageForm(g_settings.followBase + g) == 0 ? 1u : 0u;
		missing += LooksLikeObject(LookupForm(EspFormId(espBase, kMarkerLocalId + g))) ? 0u : 1u;
	}
	if (missing != 0) {
		log("Horde: %u of the ESP's horde records are missing (an older OblivionWalkers.esp?) - the walkers keep "
		    "their own AI",
		    missing);
		return;
	}
	g_ready = true;
	log("Horde: on - %u groups, a walker joins a leader within %.0f units, the marker %.0f ahead, %u followers at "
	    "most, a new heading after %.0f s without progress; toward the cities %d (%u gates, pull %.2f, within %.0f, "
	    "holding %.0f in front of the door)",
	    kHordeGroups, g_settings.joinRadius, g_settings.leadUnits, g_settings.groupMost, g_settings.staleSeconds,
	    g_settings.towardCities ? 1 : 0, g_settings.gateCount, g_settings.cityPull, g_settings.cityRange,
	    g_settings.gateHoldUnits);
}

void ConfigureGuards(const GuardSettings& settings, LogFn log) {
	g_guardSettings = settings;
	g_guardsReady = false;
	if (!settings.enabled) {
		return;
	}
	if (g_espBase == kNoEsp) {
		log("Guards: no ESP - no guards come out");
		return;
	}
	UInt32 missing = 0;
	for (UInt32 c = 0; c < kGuardCities; ++c) {
		for (UInt32 i = 0; i < kGuardPool; ++i) {
			missing += PoolGuard(c, i) == 0 ? 1u : 0u;
		}
	}
	if (missing != 0) {
		log("Guards: %u of the ESP's %u pool guards are missing (an older OblivionWalkers.esp?) - no guards come out",
		    missing, kGuardCities * kGuardPool);
		return;
	}
	g_guardsReady = true;
	log("Guards: on - %u cities, %u guards each, one for every %u walkers within %.0f units of a gate, %.2f times a "
	    "walker's health, back in after %.0f s without one",
	    kGuardCities, kGuardPool, settings.zombiesPerGuard, settings.radius, settings.healthFactor,
	    settings.clearSeconds);
}

bool IsGateGuard(UInt32 refId) {
	UInt32 city = 0;
	UInt32 slot = 0;
	return IsEspForm(g_espBase, refId) && PoolSlotOf(EspLocalOf(g_espBase, refId), city, slot);
}

int HordeHeadingCity() {
	const int gate = g_ready ? HordeHeadingGate(g_state) : -1;
	return gate >= 0 && static_cast<UInt32>(gate) < kCityGateCount ? static_cast<int>(kCityGates[gate].city) : -1;
}

void ResetHorde() {
	g_state = HordeState{};
	g_pendingCount = 0;
	g_clock = 0.0f;
	g_againCount = 0;
	g_guards = GuardState{};
	g_guardsRead = false;
	g_arrivalCount = 0;
}

bool WalkerWithin(UInt32 ref, float radius, const HordeCandidate* candidates, UInt32 count) {
	const float d = NearestWalkerDistance(ref, candidates, count);
	return d >= 0.0f && d <= radius;
}

float NearestWalkerDistance(UInt32 ref, const HordeCandidate* candidates, UInt32 count) {
	if (!LooksLikeObject(ref)) {
		return -1.0f;
	}
	const UInt32 cell = Read(ref + kRefParentCellOffset);
	const bool outside = Outdoors(ref);
	const UInt32 world = outside && LooksLikeObject(cell) ? Read(cell + kCellWorldspaceOffset) : 0;
	const float x = FloatAt(ref + kRefPositionOffset);
	const float y = FloatAt(ref + kRefPositionOffset + 4);
	float nearest = -1.0f;
	for (UInt32 i = 0; i < count; ++i) {
		const UInt32 walker = candidates[i].ref;
		if (!candidates[i].alive || !LooksLikeObject(walker)) {
			continue;
		}
		const UInt32 walkerCell = Read(walker + kRefParentCellOffset);
		const bool samePlace = outside ? Outdoors(walker) && LooksLikeObject(walkerCell) &&
		                                     Read(walkerCell + kCellWorldspaceOffset) == world
		                               : walkerCell == cell;
		const float dx = FloatAt(walker + kRefPositionOffset) - x;
		const float dy = FloatAt(walker + kRefPositionOffset + 4) - y;
		const float d = std::sqrt(dx * dx + dy * dy);
		if (samePlace && (nearest < 0.0f || d < nearest)) {
			nearest = d;
		}
	}
	return nearest;
}

void StepHordeInGame(float dt, CandidatesFn collect, ConsoleFn console, DeadFn dead, LogFn log) {
	if (!g_ready && !g_guardsReady) {
		return;
	}
	ReadyArrivals(console, dead, log);
	for (UInt32 i = 0; i < g_pendingCount; ++i) {
		const bool given = GivePackage(g_pending[i].walker, g_pending[i].package, console);
		if (g_logsLeft > 0) {
			--g_logsLeft;
			log("Horde: group %02u led by %08X to its marker (AddScriptPackage %08X, answered %d)", g_pending[i].group,
			    g_pending[i].walker, g_pending[i].package, given ? 1 : 0);
		}
	}
	g_pendingCount = 0;
	g_clock += dt;
	if (g_clock < kStepSeconds) {
		return;
	}
	const float elapsed = g_clock;
	g_clock = 0.0f;
	for (UInt32 i = 0; i < g_againCount; ++i) {
		g_againAge[i] += elapsed;
	}

	static HordeCandidate candidates[kCandidatesMost];
	const UInt32 count = collect(candidates, kCandidatesMost);
	HordeWalker walkers[kCandidatesMost];
	bool alive[kCandidatesMost];
	UInt32 walkerHealth = 0;
	UInt32 n = 0;
	for (UInt32 i = 0; i < count && n < kCandidatesMost; ++i) {
		const UInt32 ref = candidates[i].ref;
		if (!LooksLikeObject(ref)) {
			continue;
		}
		const UInt32 base = Read(ref + kRefBaseFormOffset);
		const UInt32 health = LooksLikeObject(base) ? Read(base + kActorBaseHealthOffset) : 0;
		walkerHealth = health > walkerHealth && health < 100000 ? health : walkerHealth;
		alive[n] = candidates[i].alive;
		HordeWalker& w = walkers[n++];
		w.id = FormIdOf(ref);
		w.x = FloatAt(ref + kRefPositionOffset);
		w.y = FloatAt(ref + kRefPositionOffset + 4);
		w.rank = RankOf(w.id);
		w.tamriel = InTamriel(ref);
		const UInt32 process = ProcessOf(ref);
		w.package = process != 0 ? CurrentPackageId(process) : 0;
		// A package made at runtime (an FF form: the engine's own for an alarm
		// or a flight, or a script's) is something else's business: such a
		// walker is left alone until it runs its own or ours again.
		const bool othersPackage = (w.package >> 24) == 0xFF;
		w.fit = candidates[i].alive && !candidates[i].busy && process != 0 && HighProcess(process) &&
		        Read(ref + kRefNiNodeOffset) != 0 && Outdoors(ref) && !InCombat(ref) && !othersPackage;
	}
	if (g_guardsReady) {
		StepGateGuards(elapsed, walkers, alive, n, walkerHealth, console, dead, log);
	}
	if (!g_ready) {
		return;
	}
	HordeOrder orders[64];
	const UInt32 ordered = StepHorde(g_state, walkers, n, g_settings, elapsed, g_seed, orders, 64);
	// Every few seconds, how the groups stand: the leader's way to its
	// marker, whether it runs its package, how near its followers keep.
	if (++g_steps % kSummaryEverySteps == 0 && g_summariesLeft > 0) {
		--g_summariesLeft;
		UInt32 used = 0;
		UInt32 fit = 0;
		for (UInt32 i = 0; i < n; ++i) {
			fit += walkers[i].fit ? 1u : 0u;
		}
		for (UInt32 gi = 0; gi < kHordeGroups; ++gi) {
			const HordeGroup& g = g_state.groups[gi];
			const HordeWalker* leader = g.used ? horde_detail::Find(walkers, n, g.leader) : nullptr;
			if (leader == nullptr) {
				continue;
			}
			++used;
			float sum = 0.0f;
			float most = 0.0f;
			UInt32 running = 0;
			for (UInt32 i = 0; i < g.count; ++i) {
				const HordeWalker* f = horde_detail::Find(walkers, n, g.followers[i]);
				if (f == nullptr) {
					continue;
				}
				const float d = horde_detail::Distance(f->x, f->y, leader->x, leader->y);
				sum += d;
				most = d > most ? d : most;
				running += f->package == g_settings.followBase + gi ? 1u : 0u;
			}
			if (used <= 6) {
				log("Horde: group %02u - leader %08X at %.0f %.0f, %.0f to its marker, runs %08X (its own: %d); %u "
				    "following (%u on their package), %.0f units from it on average, %.0f at most; gate %s%s",
				    gi, g.leader, leader->x, leader->y,
				    horde_detail::Distance(leader->x, leader->y, g.targetX, g.targetY), leader->package,
				    leader->package == g_settings.travelBase + gi ? 1 : 0, g.count, running,
				    g.count > 0 ? sum / static_cast<float>(g.count) : 0.0f, most,
				    g.gate >= 0 ? kCityGates[g.gate].name : "none", g.holding ? ", holding there" : "");
			}
		}
		log("Horde: %u walkers here, %u fit, %u groups", n, fit, used);
	}
	bool moved[kHordeGroups] = {};
	for (UInt32 i = 0; i < ordered; ++i) {
		const HordeOrder& o = orders[i];
		if (o.kind == HordeOrder::MoveMarker) {
			const UInt32 marker = LookupForm(EspFormId(g_espBase, kMarkerLocalId + o.group));
			const UInt32 leader = LookupForm(o.walker);
			if (!LooksLikeObject(marker) || !LooksLikeObject(leader)) {
				continue;
			}
			char line[80];
			std::snprintf(line, sizeof(line), "MoveTo %08X %.0f %.0f 0", o.walker,
			              o.x - FloatAt(leader + kRefPositionOffset), o.y - FloatAt(leader + kRefPositionOffset + 4));
			const bool done = console(marker, line);
			moved[o.group] = true;
			if (g_logsLeft > 0) {
				--g_logsLeft;
				log("Horde: group %02u's marker to %.0f %.0f, %.0f units ahead of %08X (heading %.0f degrees; %s "
				    "answered %d)",
				    o.group, o.x, o.y, g_settings.leadUnits, o.walker,
				    std::atan2(o.y - FloatAt(leader + kRefPositionOffset + 4), o.x - FloatAt(leader + kRefPositionOffset)) *
				        57.29578f,
				    line, done ? 1 : 0);
			}
		} else if (o.kind == HordeOrder::Aim) {
			const bool aimed = AimFollow(o.group, o.walker);
			if (g_logsLeft > 0) {
				--g_logsLeft;
				log("Horde: group %02u's follow package aimed at its leader %08X (%d)", o.group, o.walker, aimed ? 1 : 0);
			}
		} else if (o.again && HoldBack(o.walker)) {
			continue;
		} else if (o.kind == HordeOrder::Lead) {
			if (moved[o.group] && g_pendingCount < kHordeGroups) {
				g_pending[g_pendingCount++] = o;
				continue;
			}
			const HordeWalker* had = horde_detail::Find(walkers, n, o.walker);
			const bool given = GivePackage(o.walker, o.package, console);
			if (g_logsLeft > 0) {
				--g_logsLeft;
				log("Horde: group %02u led by %08X (it ran %08X; AddScriptPackage %08X, answered %d)", o.group,
				    o.walker, had != nullptr ? had->package : 0u, o.package, given ? 1 : 0);
			}
		} else {
			// The package is aimed at the leader already (an Aim, this step or
			// before - HordeLogic.h SettleFollows).
			const bool given = GivePackage(o.walker, o.package, console);
			if (g_logsLeft > 0) {
				--g_logsLeft;
				log("Horde: %08X follows group %02u's leader %08X (AddScriptPackage %08X answered %d)", o.walker, o.group,
				    g_state.groups[o.group].leader, o.package, given ? 1 : 0);
			}
		}
	}
}

}  // namespace walkers
