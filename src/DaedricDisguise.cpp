#include "DaedricDisguise.h"

#include <cstdio>

#include "Engine.h"
#include "LinkLogic.h"
#include "QuestVars.h"
#include "RiseLogic.h"
#include "Placer.h"
#include "SummonLogic.h"

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
int g_lastFighting = -1;
float g_fightLogClock = 0.0f;
UInt32 g_quest = 0;

// The player's rank in `faction` (GetFactionRank, asked into the ESP
// quest's siege_lock as Banish.cpp asks a sack): -1 none, -2 not read.
double FactionRank(UInt32 faction) {
	double* var = g_quest != 0 ? QuestVariable(g_quest, "siege_lock") : nullptr;
	if (var == nullptr) {
		return -2.0;
	}
	*var = -2.0;
	char line[64];
	std::snprintf(line, sizeof(line), "set Walkers.siege_lock to player.GetFactionRank %08X", faction);
	g_console(LookupForm(kPlayerRefId), line);
	return *var;
}

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

// The body slots the player's worn Daedric pieces cover: every armor in his
// inventory changes (ExtraContainerChanges::Data, its first word the entries'
// list {EntryData*, next}; an entry's form at +8 - xOBSE GameExtraData.h,
// OBVR GameAddresses.h kMakeContainerChanges) that is worn, by its model
// (DaedricDisguiseLogic.h DaedricModelSlots; TESObjectARMO: the biped slots
// UInt16 at +0x68, the male model's path BSStringT at +0x70 - xOBSE
// GameForms.h, read 2026-11-18).
constexpr UInt8 kFormTypeArmor = 0x14;
constexpr UInt32 kArmorPartMaskOffset = 0x68;
constexpr UInt32 kArmorMaleModelPathOffset = 0x70;
constexpr UInt32 kEntryFormOffset = 0x08;

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
	UInt32 visited = 0;
	for (UInt32 node = Read(reinterpret_cast<UInt32>(changes)); LooksLikeObject(node) && visited < 1024;
	     node = Read(node + 4), ++visited) {
		const UInt32 entry = Read(node);
		const UInt32 item = LooksLikeObject(entry) ? Read(entry + kEntryFormOffset) : 0;
		if (!LooksLikeObject(item) || Read8(item + kFormTypeOffset) != kFormTypeArmor) {
			continue;
		}
		const UInt32 path = Read(item + kArmorMaleModelPathOffset);
		const UInt8 piece = DaedricModelSlots(LooksLikeObject(path) ? reinterpret_cast<const char*>(path) : nullptr,
		                                      *reinterpret_cast<const UInt16*>(item + kArmorPartMaskOffset));
		if (piece == 0 || (slots & piece) == piece) {
			continue;
		}
		void* const worn =
		    reinterpret_cast<FindWornFn>(kFindWornExtraData)(changes, nullptr, reinterpret_cast<void*>(item), 0);
		if (LooksLikeObject(reinterpret_cast<UInt32>(worn))) {
			slots |= piece;
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

// The living Daedra within kStopCombatUnits in combat (the log's proof that
// the fooled are left alone).
UInt32 DaedraFightingNear() {
	static UInt32 actors[1024];
	const UInt32 count = ActorsOnHighList(actors, 1024);
	UInt32 n = 0;
	for (UInt32 i = 0; i < count; ++i) {
		const UInt32 actor = actors[i];
		if (!IsDaedra(actor) || (g_dead != nullptr && g_dead(actor)) || !ActorInCombat(FormIdOf(actor))) {
			continue;
		}
		const float d = DistanceFromPlayer(actor);
		n += d >= 0.0f && d <= kStopCombatUnits ? 1u : 0u;
	}
	return n;
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
		g_log("Daedric disguise: the player %s DaedraFaction and DremoraFaction (%s; answered %d %d) - his ranks now "
		      "%.0f %.0f",
		      join ? "joins" : "leaves", why, a ? 1 : 0, b ? 1 : 0, FactionRank(kDaedraFactionId),
		      FactionRank(kDremoraFactionId));
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

namespace {

// The player's summoned actors (SummonLogic.h): MagicTarget::GetEffectList,
// slot 2 of the MagicTarget at the player's +0x68 (xOBSE GameObjects.h; a
// thiscall), its list walked with guarded reads.
constexpr UInt32 kActorMagicTargetOffset = 0x68;
constexpr UInt32 kGetEffectListSlot = 0x08;
constexpr UInt32 kCodeStart = 0x00401000;  // Oblivion.exe .text (Rise.cpp, Pose.cpp)
constexpr UInt32 kCodeEnd = 0x00A27C39;

UInt32 PlayerSummonActors(UInt32* out, UInt32 room) {
	const UInt32 player = LookupForm(kPlayerRefId);
	if (!LooksLikeObject(player)) {
		return 0;
	}
	const UInt32 target = player + kActorMagicTargetOffset;
	const UInt32 vtable = Read(target);
	const UInt32 fn = LooksLikeObject(vtable) ? Read(vtable + kGetEffectListSlot) : 0;
	if (fn < kCodeStart || fn >= kCodeEnd) {
		return 0;
	}
	using ListFn = UInt32(__fastcall*)(void* target, void* edx);
	const UInt32 head = reinterpret_cast<ListFn>(fn)(reinterpret_cast<void*>(target), nullptr);
	const auto read = [](UInt32 address) { return LooksLikeObject(address) ? Read(address) : 0u; };
	return PlayerSummons(LooksLikeObject(head) ? head : 0u, read, out, room);
}

}  // namespace

bool IsDaedraKind(UInt32 actor) { return LooksLikeObject(actor) && IsDaedra(actor); }

// A Daedra, and not one the player summoned (SummonLogic.h DaedraAlly).
bool IsDaedraActor(UInt32 actor) {
	if (!LooksLikeObject(actor) || !IsDaedra(actor)) {
		return false;
	}
	UInt32 summons[8];
	const UInt32 count = PlayerSummonActors(summons, 8);
	return DaedraAlly(true, actor, summons, count);
}

void ConfigureDaedricDisguise(const DaedricSettings* settings, ConsoleFn console, DeadFn dead, LogFn log) {
	g_settings = settings;
	g_console = console;
	g_dead = dead;
	g_log = log;
	log("Daedric disguise: [Disguise] Daedric=%d DaedricNearDistance=%.0f DaedricForgetSeconds=%.0f (the pieces by "
	    "their model at run time)",
	    settings->enabled ? 1 : 0, settings->nearUnits, settings->forgetSeconds);
}

void ConfigureDaedricDisguiseEsp(UInt32 espBase) {
	g_quest = espBase == kNoEsp ? 0 : EspFormId(espBase, kLinkQuestLocalId);
}

void NoteDaedricHit(UInt32 target, UInt32 attacker, bool targetAlliedWalker) {
	if (g_settings == nullptr || !LooksLikeObject(target) || !LooksLikeObject(attacker)) {
		return;
	}
	if (DaedricStruck(FormIdOf(attacker) == kPlayerRefId, IsDaedra(target), targetAlliedWalker)) {
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
	if (g_settings->enabled && nearest >= 0.0f) {
		const int fighting = static_cast<int>(DaedraFightingNear());
		g_fightLogClock += elapsed;
		const bool due = g_fightLogClock >= 5.0f && nearest <= 2000.0f;  // a Daedra close: every 5 s
		if ((fighting != g_lastFighting || due) && g_log != nullptr && g_logsLeft > 0) {
			g_fightLogClock = 0.0f;
			--g_logsLeft;
			g_log("Daedric disguise: %d Daedra near in combat (fooled %d, known %d; the nearest %.0f)", fighting,
			      g_state.on ? 1 : 0, g_state.known ? 1 : 0, nearest);
		}
		g_lastFighting = fighting;
	}
	const DaedricChange change = StepDaedric(g_state, *g_settings, FullDaedricSet(slots), nearest, struck, elapsed);
	if (change == DaedricChange::None) {
		return;
	}
	g_lastFighting = -1;  // the next look logs who fights, the new state beside it
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
	g_lastFighting = -1;
}

}  // namespace walkers
