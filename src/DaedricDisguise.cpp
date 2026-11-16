#include "DaedricDisguise.h"

#include <cstdio>

#include "Engine.h"
#include "Placer.h"

namespace walkers {

using namespace engine;

namespace {

constexpr UInt32 kPlayerRefId = 0x14;
constexpr UInt32 kRefBaseFormOffset = 0x1C;
constexpr UInt32 kCreatureTypeOffset = 0x104;  // Replace.cpp
constexpr UInt32 kNpcRacePointerOffset = 0xE8;  // Rise.cpp
constexpr UInt8 kCreatureTypeDaedra = 1;        // CategoryLogic.h
constexpr UInt32 kDremoraRaceId = 0x00038010;   // RiseLogic.h kDremoraRace
// The worn stack of an item in an actor's inventory, as the game's
// UnequipItem command finds it (OBVR src/game/GameAddresses.h, read
// 2026-09-27; OBVR's PlayerWearsItem uses them since): the container changes
// off the actor's extra list (+0x44), then the first extra-data list of that
// item marked worn - null when it is not worn.
constexpr UInt32 kGetContainerChanges = 0x0041E6F0;
constexpr UInt32 kFindWornExtraData = 0x00485FA0;
constexpr UInt32 kActorExtraListOffset = 0x44;
constexpr float kStepSeconds = 0.5f;
constexpr float kStopCombatUnits = 8192.0f;

const DaedricSettings* g_settings = nullptr;
ConsoleFn g_console = nullptr;
DeadFn g_dead = nullptr;
LogFn g_log = nullptr;
DaedricState g_state;
bool g_struck = false;
bool g_leaveDue = true;  // a load: out of the factions first
float g_clock = 0.0f;
UInt8 g_lastSlots = 0xFF;
UInt32 g_logsLeft = 60;

bool Say(UInt32 ref, const char* line) { return LooksLikeObject(ref) && g_console != nullptr && g_console(ref, line); }

bool IsDaedra(UInt32 actor) {
	const UInt32 base = Read(actor + kRefBaseFormOffset);
	if (!LooksLikeObject(base)) {
		return false;
	}
	const UInt8 type = Read8(base + kFormTypeOffset);
	if (type == kFormTypeCreature) {
		return Read8(base + kCreatureTypeOffset) == kCreatureTypeDaedra;
	}
	if (type == kFormTypeNpc) {
		const UInt32 race = Read(base + kNpcRacePointerOffset);
		return LooksLikeObject(race) && FormIdOf(race) == kDremoraRaceId;
	}
	return false;
}

// The body slots the player's worn Daedric pieces cover.
UInt8 WornDaedricSlots() {
	const UInt32 player = LookupForm(kPlayerRefId);
	if (!LooksLikeObject(player)) {
		return 0;
	}
	using ChangesFn = void*(__fastcall*)(void* extraList, void* edx);
	using FindWornFn = void*(__fastcall*)(void* changes, void* edx, void* item, UInt32 zero);
	void* const changes =
	    reinterpret_cast<ChangesFn>(kGetContainerChanges)(reinterpret_cast<void*>(player + kActorExtraListOffset), nullptr);
	if (!LooksLikeObject(reinterpret_cast<UInt32>(changes))) {
		return 0;
	}
	UInt8 slots = 0;
	for (UInt32 i = 0; i < kDaedricPieceCount; ++i) {
		const UInt32 item = LookupForm(kDaedricPieces[i].formId);
		if (!LooksLikeObject(item)) {
			continue;
		}
		void* const worn =
		    reinterpret_cast<FindWornFn>(kFindWornExtraData)(changes, nullptr, reinterpret_cast<void*>(item), 0);
		if (LooksLikeObject(reinterpret_cast<UInt32>(worn))) {
			slots |= kDaedricPieces[i].slots;
		}
	}
	return slots;
}

// The nearest living Daedra's distance from the player, -1 none.
float NearestDaedra() {
	static UInt32 actors[1024];
	const UInt32 count = ActorsOnHighList(actors, 1024);
	float best = -1.0f;
	for (UInt32 i = 0; i < count; ++i) {
		const UInt32 actor = actors[i];
		if (FormIdOf(actor) == kPlayerRefId || !IsDaedra(actor) || (g_dead != nullptr && g_dead(actor))) {
			continue;
		}
		const float d = DistanceFromPlayer(actor);
		if (d >= 0.0f && d < 1.0e8f && (best < 0.0f || d < best)) {
			best = d;
		}
	}
	return best;
}

// Fooled: the Daedra near who fight stop (they fought the player; a Daedra
// fighting a walker takes it up again on its own).
UInt32 StopDaedraFighting() {
	static UInt32 actors[1024];
	const UInt32 count = ActorsOnHighList(actors, 1024);
	UInt32 stopped = 0;
	for (UInt32 i = 0; i < count; ++i) {
		const UInt32 actor = actors[i];
		if (!IsDaedra(actor) || (g_dead != nullptr && g_dead(actor)) || !ActorInCombat(FormIdOf(actor))) {
			continue;
		}
		const float d = DistanceFromPlayer(actor);
		if (d >= 0.0f && d <= kStopCombatUnits && Say(actor, "StopCombat")) {
			++stopped;
		}
	}
	return stopped;
}

void Factions(bool join, const char* why) {
	const UInt32 player = LookupForm(kPlayerRefId);
	char line[48];
	std::snprintf(line, sizeof(line), "SetFactionRank %08X %d", kDaedraFactionId, join ? 0 : -1);
	const bool a = Say(player, line);
	std::snprintf(line, sizeof(line), "SetFactionRank %08X %d", kDremoraFactionId, join ? 0 : -1);
	const bool b = Say(player, line);
	const UInt32 stopped = join ? StopDaedraFighting() : 0;
	if (g_log != nullptr && g_logsLeft > 0) {
		--g_logsLeft;
		g_log("Daedric disguise: the player %s DaedraFaction and DremoraFaction (%s; answered %d %d)",
		      join ? "joins" : "leaves", why, a ? 1 : 0, b ? 1 : 0);
		if (join) {
			g_log("Daedric disguise: %u Daedra fighting near told StopCombat", stopped);
		}
	}
}

void Line(const char* text) {
	char line[200];
	std::snprintf(line, sizeof(line), "Message \"%s\"", text);
	Say(LookupForm(kPlayerRefId), line);
}

}  // namespace

void ConfigureDaedricDisguise(const DaedricSettings* settings, ConsoleFn console, DeadFn dead, LogFn log) {
	g_settings = settings;
	g_console = console;
	g_dead = dead;
	g_log = log;
	log("Daedric disguise: [Disguise] Daedric=%d DaedricNearDistance=%.0f DaedricForgetSeconds=%.0f (%u pieces known)",
	    settings->enabled ? 1 : 0, settings->nearUnits, settings->forgetSeconds, kDaedricPieceCount);
}

void NoteDaedricHit(UInt32 target, UInt32 attacker) {
	if (g_settings == nullptr || !LooksLikeObject(target) || !LooksLikeObject(attacker)) {
		return;
	}
	if (FormIdOf(attacker) == kPlayerRefId && IsDaedra(target)) {
		g_struck = true;
	}
}

void StepDaedricDisguiseInGame(float dt) {
	if (g_settings == nullptr || g_console == nullptr) {
		return;
	}
	g_clock += dt;
	if (g_clock < kStepSeconds) {
		return;
	}
	const float elapsed = g_clock;
	g_clock = 0.0f;
	if (g_leaveDue) {
		g_leaveDue = false;
		Factions(false, "after a load, before the first look");
	}
	const UInt8 slots = WornDaedricSlots();
	const float nearest = NearestDaedra();
	const bool struck = g_struck;
	g_struck = false;
	if (slots != g_lastSlots && g_log != nullptr && g_logsLeft > 0) {
		--g_logsLeft;
		g_log("Daedric disguise: the player wears Daedric pieces on slots %02X (full set %d); the nearest Daedra %.0f",
		      slots, FullDaedricSet(slots) ? 1 : 0, nearest);
	}
	g_lastSlots = slots;
	const DaedricChange change = StepDaedric(g_state, *g_settings, FullDaedricSet(slots), nearest, struck, elapsed);
	if (change == DaedricChange::None) {
		return;
	}
	if (DaedricJoins(change)) {
		Factions(true, DaedricChangeText(change));
	} else if (DaedricLeaves(change)) {
		Factions(false, DaedricChangeText(change));
	}
	Line(DaedricChangeText(change));
}

bool DaedricDisguised() { return g_state.on; }

void ResetDaedricDisguise() {
	g_state = DaedricState{};
	g_struck = false;
	g_leaveDue = true;
	g_clock = 0.0f;
	g_lastSlots = 0xFF;
}

}  // namespace walkers
