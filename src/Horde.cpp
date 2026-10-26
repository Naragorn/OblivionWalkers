#include "Horde.h"

#include "Engine.h"

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
constexpr UInt32 kCandidatesMost = 128;
constexpr float kStepSeconds = 1.0f;

HordeSettings g_settings;
HordeState g_state;
UInt32 g_espIndex = 0x100;
UInt32 g_seed = 0x9E3779B9u;
float g_clock = 0.0f;
bool g_ready = false;
// The leads given a frame after their marker moved: a MoveTo and an order
// aimed at the moved reference in one frame do not take (the CS wiki's
// MoveTo notes).
HordeOrder g_pending[kHordeGroups];
UInt32 g_pendingCount = 0;
UInt32 g_logsLeft = 200;

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

bool GivePackage(UInt32 walkerId, UInt32 package, ConsoleFn console) {
	const UInt32 walker = LookupForm(walkerId);
	char line[48];
	std::snprintf(line, sizeof(line), "AddScriptPackage %08X", package);
	return LooksLikeObject(walker) && console(walker, line);
}

}  // namespace

void ConfigureHorde(const HordeSettings& settings, UInt32 espIndex, LogFn log) {
	g_settings = settings;
	g_espIndex = espIndex;
	g_ready = false;
	if (!settings.enabled) {
		return;
	}
	if (espIndex > 0xFF) {
		log("Horde: no ESP - the walkers keep their own AI");
		return;
	}
	g_settings.travelBase = EspFormId(espIndex, kTravelLocalId);
	g_settings.followBase = EspFormId(espIndex, kFollowLocalId);
	UInt32 missing = 0;
	for (UInt32 g = 0; g < kHordeGroups; ++g) {
		missing += PackageForm(g_settings.travelBase + g) == 0 ? 1u : 0u;
		missing += PackageForm(g_settings.followBase + g) == 0 ? 1u : 0u;
		missing += LooksLikeObject(LookupForm(EspFormId(espIndex, kMarkerLocalId + g))) ? 0u : 1u;
	}
	if (missing != 0) {
		log("Horde: %u of the ESP's horde records are missing (an older OblivionWalkers.esp?) - the walkers keep "
		    "their own AI",
		    missing);
		return;
	}
	g_ready = true;
	log("Horde: on - %u groups, a walker joins a leader within %.0f units, the marker %.0f ahead, %u followers at "
	    "most, a new heading after %.0f s without progress",
	    kHordeGroups, g_settings.joinRadius, g_settings.leadUnits, g_settings.groupMost, g_settings.staleSeconds);
}

void ResetHorde() {
	g_state = HordeState{};
	g_pendingCount = 0;
	g_clock = 0.0f;
}

void StepHordeInGame(float dt, CandidatesFn collect, ConsoleFn console, LogFn log) {
	if (!g_ready) {
		return;
	}
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

	static HordeCandidate candidates[kCandidatesMost];
	const UInt32 count = collect(candidates, kCandidatesMost);
	HordeWalker walkers[kCandidatesMost];
	UInt32 n = 0;
	for (UInt32 i = 0; i < count && n < kCandidatesMost; ++i) {
		const UInt32 ref = candidates[i].ref;
		if (!LooksLikeObject(ref)) {
			continue;
		}
		HordeWalker& w = walkers[n++];
		w.id = FormIdOf(ref);
		w.x = FloatAt(ref + kRefPositionOffset);
		w.y = FloatAt(ref + kRefPositionOffset + 4);
		w.rank = RankOf(w.id);
		const UInt32 process = ProcessOf(ref);
		w.package = process != 0 ? CurrentPackageId(process) : 0;
		w.fit = candidates[i].alive && !candidates[i].busy && process != 0 && HighProcess(process) &&
		        Read(ref + kRefNiNodeOffset) != 0 && Outdoors(ref) && !InCombat(ref);
	}
	HordeOrder orders[64];
	const UInt32 ordered = StepHorde(g_state, walkers, n, g_settings, elapsed, g_seed, orders, 64);
	bool moved[kHordeGroups] = {};
	for (UInt32 i = 0; i < ordered; ++i) {
		const HordeOrder& o = orders[i];
		if (o.kind == HordeOrder::MoveMarker) {
			const UInt32 marker = LookupForm(EspFormId(g_espIndex, kMarkerLocalId + o.group));
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
		} else if (o.kind == HordeOrder::Lead) {
			if (moved[o.group] && g_pendingCount < kHordeGroups) {
				g_pending[g_pendingCount++] = o;
				continue;
			}
			const bool given = GivePackage(o.walker, o.package, console);
			if (g_logsLeft > 0) {
				--g_logsLeft;
				log("Horde: group %02u led by %08X (AddScriptPackage %08X, answered %d)", o.group, o.walker, o.package,
				    given ? 1 : 0);
			}
		} else {
			const bool aimed = AimFollow(o.group, g_state.groups[o.group].leader);
			const bool given = aimed && GivePackage(o.walker, o.package, console);
			if (g_logsLeft > 0) {
				--g_logsLeft;
				log("Horde: %08X follows group %02u's leader %08X (aimed %d, AddScriptPackage %08X answered %d)",
				    o.walker, o.group, g_state.groups[o.group].leader, aimed ? 1 : 0, o.package, given ? 1 : 0);
			}
		}
	}
}

}  // namespace walkers
