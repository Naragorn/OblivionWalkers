#include "Camps.h"

#include "CampSpots.h"
#include "CityGates.h"
#include "Engine.h"
#include "News.h"
#include "RiseLogic.h"

#include <cmath>
#include <cstdio>
#include <cstring>

namespace walkers {

using namespace engine;

namespace {

constexpr UInt32 kPlayerRefId = 0x14;
constexpr UInt32 kGoldId = 0x0000000F;
constexpr UInt32 kQuestSound = 0x00185DD3;  // UIQuestUpdate (Oblivion.esm)
constexpr UInt32 kRefPositionOffset = 0x2C;
constexpr UInt32 kRefParentCellOffset = 0x40;
constexpr UInt32 kCellFlagsOffset = 0x24;
constexpr UInt32 kCellWorldspaceOffset = 0x50;
constexpr float kStepSeconds = 1.0f;
constexpr float kSpotLift = 40.0f;  // a walker put this far above its spot's land
constexpr UInt32 kEventsMost = 64;
constexpr UInt32 kLogLines = 48;

const CampSettings* g_settings = nullptr;
CampConsoleFn g_console = nullptr;
CampRollFn g_roll = nullptr;
CampDeadFn g_dead = nullptr;
LogFn g_log = nullptr;
UInt32 g_espBase = 0;
CampsState g_state;
float g_clock = 0.0f;
bool g_wasEnabled = true;
bool g_testDone = false;
bool g_applied = false;     // the rosters enabled since the load
UInt16 g_shownPeople[kCampCount] = {};  // the slots enabled by this session
// The pool: per bank (kCampWalkersAtOnce slots each) the camp it is out at
// (-1 none), whether the camp's package is still to be given, and per slot
// out or lying ended.
int g_bankCamp[kCampShownMost] = {-1, -1};
bool g_bankLead[kCampShownMost] = {};
bool g_out[kCampPool] = {};
bool g_corpse[kCampPool] = {};
UInt32 g_ended[32] = {};
UInt32 g_endedCount = 0;
UInt32 g_logLines = 0;

UInt32 Esp(UInt32 local) { return g_espBase != 0 ? LookupForm(EspFormId(g_espBase, local)) : 0; }

UInt32 Person(UInt32 camp, UInt32 slot) { return Esp(CampPersonLocalId(camp, slot)); }

bool Say(UInt32 ref, const char* line) {
	return LooksLikeObject(ref) && g_console != nullptr && g_console(ref, line);
}

void Line(const char* text) {
	char line[200];
	std::snprintf(line, sizeof(line), "Message \"%s\"", text);
	Say(LookupForm(kPlayerRefId), line);
}

// The player's distance from each camp, outdoors in Tamriel; -1 elsewhere.
void Distances(float* distance) {
	for (UInt32 i = 0; i < kCampCount; ++i) {
		distance[i] = -1.0f;
	}
	const UInt32 player = LookupForm(kPlayerRefId);
	if (!LooksLikeObject(player)) {
		return;
	}
	const UInt32 cell = Read(player + kRefParentCellOffset);
	if (!LooksLikeObject(cell) || (Read8(cell + kCellFlagsOffset) & 1) != 0) {
		return;
	}
	const UInt32 world = Read(cell + kCellWorldspaceOffset);
	if (!LooksLikeObject(world) || FormIdOf(world) != kTamrielWorldspace) {
		return;
	}
	const float px = FloatAt(player + kRefPositionOffset);
	const float py = FloatAt(player + kRefPositionOffset + 4);
	for (UInt32 i = 0; i < kCampCount; ++i) {
		const float dx = px - kCampPlaces[i].x;
		const float dy = py - kCampPlaces[i].y;
		distance[i] = std::sqrt(dx * dx + dy * dy);
	}
}

bool Near(const float* distance, UInt32 camp) { return distance[camp] >= 0.0f && distance[camp] <= kCampSeenUnits; }

// The people a camp should have out: its roster's living, none when off or
// overrun.
UInt16 Wanted(const Camp& c) {
	if (!g_settings->enabled || c.status == CampStatus::Overrun || !c.set) {
		return 0;
	}
	return static_cast<UInt16>(c.roster & ~c.lost);
}

// Camp `i`'s people enabled and disabled as Wanted says; `all`: every slot
// told (a load: the save holds what an earlier session enabled), else only
// the changes. A dead one is left to the rise.
void ApplyPeople(UInt32 i, bool all) {
	const Camp& c = g_state.camps[i];
	const UInt16 wanted = Wanted(c);
	UInt32 enabled = 0;
	UInt32 disabled = 0;
	for (UInt32 slot = 0; slot < kCampSlots; ++slot) {
		const UInt16 bit = static_cast<UInt16>(1u << slot);
		const bool want = (wanted & bit) != 0;
		const bool shown = (g_shownPeople[i] & bit) != 0;
		if (!all && want == shown) {
			continue;
		}
		const UInt32 ref = Person(i, slot);
		if (!LooksLikeObject(ref) || (!want && (c.lost & bit) != 0 && g_dead != nullptr && g_dead(ref))) {
			continue;
		}
		Say(ref, want ? "Enable" : "Disable");
		want ? ++enabled : ++disabled;
	}
	g_shownPeople[i] = wanted;
	if ((enabled > 0 || disabled > 0) && g_logLines < kLogLines) {
		++g_logLines;
		g_log("Camps: camp %02u %s - %u enabled, %u disabled (roster %04X, lost %04X)", i, kCampPlaces[i].name,
		      enabled, disabled, c.roster, c.lost);
	}
}

// The camp's people set up anew: the dead raised and dressed again (the
// rise moved their things onto their walkers), all enabled.
void RaisePeople(UInt32 i) {
	const Camp& c = g_state.camps[i];
	UInt32 raised = 0;
	for (UInt32 slot = 0; slot < kCampSlots; ++slot) {
		if ((c.roster & (1u << slot)) == 0) {
			continue;
		}
		const UInt32 ref = Person(i, slot);
		if (!LooksLikeObject(ref) || g_dead == nullptr || !g_dead(ref)) {
			continue;
		}
		Say(ref, "SetDestroyed 0");
		Say(ref, "Resurrect");
		Say(ref, "RemoveAllItems");
		const UInt32* outfit = kCampOutfits[CampBaseIndex(i, slot)];
		for (UInt32 k = 0; k < 4; ++k) {
			char line[48];
			std::snprintf(line, sizeof(line), "AddItem %08X 1", outfit[k]);
			Say(ref, line);
			std::snprintf(line, sizeof(line), "EquipItem %08X", outfit[k]);
			Say(ref, line);
		}
		++raised;
	}
	g_shownPeople[i] = 0;
	ApplyPeople(i, true);
	g_log("Camps: camp %02u %s is back - %u raised and dressed, %u in the camp", i, kCampPlaces[i].name, raised,
	      CampAlive(c));
}

UInt32 PoolRef(UInt32 slot) { return Esp(kCampPoolFirstLocalId + slot); }

void HideBank(UInt32 bank) {
	UInt32 hidden = 0;
	for (UInt32 k = 0; k < kCampWalkersAtOnce; ++k) {
		const UInt32 slot = bank * kCampWalkersAtOnce + k;
		if (!g_out[slot] && !g_corpse[slot]) {
			continue;
		}
		Say(PoolRef(slot), "Disable");
		g_out[slot] = false;
		g_corpse[slot] = false;
		++hidden;
	}
	if (g_bankCamp[bank] >= 0) {
		g_log("Camps: the walkers at camp %02d %s back in the pool (%u)", g_bankCamp[bank],
		      kCampPlaces[g_bankCamp[bank]].name, hidden);
	}
	g_bankCamp[bank] = -1;
	g_bankLead[bank] = false;
}

// Bank `bank` at camp `i`: as many out as CampWalkersOut wants - an attack's
// on the ring away from the road, led at the camp; an overrun camp's among
// its tents. A slot never used first, else an ended one raised.
void ShowBank(UInt32 bank, UInt32 i) {
	const Camp& c = g_state.camps[i];
	const bool attack = c.status == CampStatus::Attacked;
	if (g_bankCamp[bank] != static_cast<int>(i)) {
		HideBank(bank);
		g_bankCamp[bank] = static_cast<int>(i);
		g_log("Camps: the player at camp %02u %s - %s (%u walkers, %u at once)", i, kCampPlaces[i].name,
		      attack ? "its attackers come out" : "its walkers stand among the tents", c.walkersLeft,
		      CampWalkersOut(c));
		if (attack) {
			// The bank's marker on the camp; the package a step later (the
			// horde's MoveTo notes: a package's target is read when given).
			char line[48];
			std::snprintf(line, sizeof(line), "MoveTo %08X 0 0 0", EspFormId(g_espBase, CampMarkerLocalId(i)));
			const bool moved = Say(Esp(kCampMarkerFirstLocalId + bank), line);
			g_log("Camps: marker %u onto camp %02u (%s answered %d)", bank, i, line, moved ? 1 : 0);
			g_bankLead[bank] = true;
		}
	} else if (g_bankLead[bank]) {
		g_bankLead[bank] = false;
		char line[48];
		std::snprintf(line, sizeof(line), "AddScriptPackage %08X", EspFormId(g_espBase, kCampTravelFirstLocalId + bank));
		// The console answers 0 for AddScriptPackage even where it takes (the
		// horde's lines, horde-walk.txt): the count is of those handed it.
		UInt32 led = 0;
		for (UInt32 k = 0; k < kCampWalkersAtOnce; ++k) {
			const UInt32 slot = bank * kCampWalkersAtOnce + k;
			if (g_out[slot]) {
				Say(PoolRef(slot), line);
				++led;
			}
		}
		g_log("Camps: %u walkers led at camp %02u (%s)", led, i, line);
	}
	UInt32 out = 0;
	for (UInt32 k = 0; k < kCampWalkersAtOnce; ++k) {
		out += g_out[bank * kCampWalkersAtOnce + k] ? 1u : 0u;
	}
	const UInt32 wanted = CampWalkersOut(c);
	for (UInt32 pass = 0; pass < 2 && out < wanted; ++pass) {
		for (UInt32 k = 0; k < kCampWalkersAtOnce && out < wanted; ++k) {
			const UInt32 slot = bank * kCampWalkersAtOnce + k;
			if (g_out[slot] || (pass == 0 && g_corpse[slot]) || (pass == 1 && !g_corpse[slot])) {
				continue;
			}
			const UInt32 ref = PoolRef(slot);
			if (!LooksLikeObject(ref)) {
				continue;
			}
			const bool raised = g_dead != nullptr && g_dead(ref);
			if (raised) {
				Say(ref, "SetDestroyed 0");
				Say(ref, "Resurrect");
			}
			float dx = 0.0f;
			float dy = 0.0f;
			CampWalkerSpot(kCampPlaces[i], attack, k, dx, dy);
			const float dz = kCampSpotZ[i][attack ? 0 : 1][k] - kCampPlaces[i].z + kSpotLift;
			char line[96];
			std::snprintf(line, sizeof(line), "MoveTo %08X %.0f %.0f %.0f", EspFormId(g_espBase, CampMarkerLocalId(i)),
			              dx, dy, dz);
			Say(ref, "Enable");
			const bool moved = Say(ref, line);
			g_out[slot] = true;
			g_corpse[slot] = false;
			++out;
			if (g_logLines < kLogLines) {
				++g_logLines;
				g_log("Camps: walker %08X out at camp %02u (slot %u%s; %s answered %d)", FormIdOf(ref), i, slot,
				      raised ? ", raised" : "", line, moved ? 1 : 0);
			}
		}
	}
}

bool Shown(UInt32 i) {
	for (UInt32 b = 0; b < kCampShownMost; ++b) {
		if (g_bankCamp[b] == static_cast<int>(i)) {
			return true;
		}
	}
	return false;
}

void Reward(UInt32 i) {
	const UInt32 player = LookupForm(kPlayerRefId);
	char line[64];
	char text[160];
	char what[64] = "";
	if (g_settings->rewardFame && g_settings->famePoints > 0) {
		std::snprintf(line, sizeof(line), "ModPCFame %u", g_settings->famePoints);
		Say(player, line);
		std::snprintf(what, sizeof(what), " (+%u fame", g_settings->famePoints);
	}
	if (g_settings->rewardGold && g_settings->gold > 0) {
		std::snprintf(line, sizeof(line), "AddItem %08X %u", kGoldId, g_settings->gold);
		Say(player, line);
		char gold[32];
		std::snprintf(gold, sizeof(gold), "%s%u gold", what[0] != '\0' ? ", " : " (", g_settings->gold);
		std::snprintf(what + std::strlen(what), sizeof(what) - std::strlen(what), "%s", gold);
	}
	if (what[0] != '\0') {
		std::snprintf(what + std::strlen(what), sizeof(what) - std::strlen(what), ")");
	}
	std::snprintf(text, sizeof(text), "The refugees %s are safe - the walkers are beaten%s.", kCampPlaces[i].name, what);
	Line(text);
	std::snprintf(line, sizeof(line), "PlaySound %08X", kQuestSound);
	const bool sound = Say(player, line);
	g_log("Camps: camp %02u %s defended - %s (the sound answered %d)", i, kCampPlaces[i].name, text, sound ? 1 : 0);
}

void Carry(const CampEvent& ev, const float* distance, float now) {
	const UInt32 i = ev.camp;
	Camp& c = g_state.camps[i];
	const bool near = Near(distance, i);
	char text[160];
	switch (ev.kind) {
		case CampEvent::Begin:
			g_log("Camps: camp %02u %s attacked - %u walkers (game day %.3f, the player %s)", i, kCampPlaces[i].name,
			      c.walkersLeft, now, near ? "near" : "away");
			if (near) {
				std::snprintf(text, sizeof(text), "Walkers are coming for the refugees %s!", kCampPlaces[i].name);
				Line(text);
			}
			break;
		case CampEvent::PersonLost: {
			const UInt32 ref = Person(i, ev.slot);
			Say(ref, "Disable");
			g_shownPeople[i] = static_cast<UInt16>(g_shownPeople[i] & ~(1u << ev.slot));
			g_log("Camps: camp %02u %s lost slot %u (%08X) away - %u left, %u walkers", i, kCampPlaces[i].name, ev.slot,
			      FormIdOf(ref), CampAlive(c), c.walkersLeft);
			break;
		}
		case CampEvent::HeldAway:
			g_log("Camps: camp %02u %s held, the player away - %u of its people alive", i, kCampPlaces[i].name,
			      CampAlive(c));
			break;
		case CampEvent::Defended:
			Reward(i);
			break;
		case CampEvent::Overrun:
			g_log("Camps: camp %02u %s overrun (game day %.3f) - %u walkers stay at it", i, kCampPlaces[i].name, now,
			      c.walkersLeft);
			if (near) {
				std::snprintf(text, sizeof(text), "The refugees %s are lost.", kCampPlaces[i].name);
				Line(text);
			}
			ApplyPeople(i, false);
			break;
		case CampEvent::Return:
			RaisePeople(i);
			break;
	}
}

void CountEnded(UInt32 refId, CampEvent* events, UInt32& n) {
	const UInt32 slot = (refId & kEspLocalMask) - kCampPoolFirstLocalId;
	const UInt32 bank = slot / kCampWalkersAtOnce;
	if (!g_out[slot] || g_bankCamp[bank] < 0) {
		return;
	}
	g_out[slot] = false;
	g_corpse[slot] = true;
	const UInt32 i = static_cast<UInt32>(g_bankCamp[bank]);
	n += CampWalkerEnded(g_state.camps[i], i, events + n, kEventsMost - n);
	g_log("Camps: walker %08X ended at camp %02u %s - %u left", refId, i, kCampPlaces[i].name,
	      g_state.camps[i].walkersLeft);
}

}  // namespace

void ConfigureCamps(const CampSettings* settings, CampConsoleFn console, CampRollFn roll, CampDeadFn dead, LogFn log) {
	g_settings = settings;
	g_console = console;
	g_roll = roll;
	g_dead = dead;
	g_log = log;
	g_log("Camps: [Survivors] Enabled=%d PeoplePerCamp=%u WomenPercent=%u Trader=%d RewardFame=%d FamePoints=%u "
	      "RewardGold=%d Gold=%u AttackChancePercent=%u AttackWalkers=%u..%u CampsAttackedAtOnce=%u "
	      "RefugeeKillsPerHour=%.1f LossPercentPerHour=%u ReturnChancePercent=%u TestCamp=%d",
	      settings->enabled ? 1 : 0, settings->peoplePerCamp, settings->womenPercent, settings->trader ? 1 : 0,
	      settings->rewardFame ? 1 : 0, settings->famePoints, settings->rewardGold ? 1 : 0, settings->gold,
	      settings->attackChancePercent, settings->walkersMin, settings->walkersMax, settings->campsAttackedAtOnce,
	      settings->refugeeKillsPerHour, settings->lossPercentPerHour, settings->returnChancePercent,
	      settings->testCamp);
}

void ConfigureCampsEsp(UInt32 espBase) { g_espBase = espBase; }

bool NoteCampWalkerEnded(UInt32 refId) {
	if (g_espBase == 0) {
		return false;
	}
	const UInt32 local = refId & kEspLocalMask;
	if (EspFormId(g_espBase, local) != refId || local < kCampPoolFirstLocalId ||
	    local >= kCampPoolFirstLocalId + kCampPool || !g_out[local - kCampPoolFirstLocalId]) {
		return false;
	}
	if (g_endedCount < 32) {
		g_ended[g_endedCount++] = refId;
	}
	return true;
}

bool IsCampWalker(UInt32 refId) {
	if (g_espBase == 0) {
		return false;
	}
	const UInt32 local = refId & kEspLocalMask;
	return EspFormId(g_espBase, local) == refId && local >= kCampPoolFirstLocalId &&
	       local < kCampPoolFirstLocalId + kCampPool && g_out[local - kCampPoolFirstLocalId];
}

bool IsCampPerson(UInt32 refId) {
	if (g_espBase == 0) {
		return false;
	}
	const UInt32 local = refId & kEspLocalMask;
	if (EspFormId(g_espBase, local) != refId || local < kCampFirstLocalId ||
	    local >= kCampFirstLocalId + kCampCount * kCampBlock) {
		return false;
	}
	return (local - kCampFirstLocalId) % kCampBlock < kCampSlots;
}

void StepCampsInGame(float dt) {
	if (g_settings == nullptr || g_console == nullptr || g_espBase == 0) {
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
	if (!g_applied) {
		ApplyCampsLoaded();
	}
	if (!g_settings->enabled) {
		if (g_wasEnabled) {
			g_log("Camps: [Survivors] Enabled=0 - every camp's people and walkers taken in");
			for (UInt32 b = 0; b < kCampShownMost; ++b) {
				HideBank(b);
			}
			for (UInt32 i = 0; i < kCampCount; ++i) {
				ApplyPeople(i, false);
			}
		}
		g_wasEnabled = false;
		return;
	}
	if (!g_wasEnabled) {
		g_wasEnabled = true;
		for (UInt32 i = 0; i < kCampCount; ++i) {
			ApplyPeople(i, false);
		}
	}
	float distance[kCampCount];
	Distances(distance);
	CampEvent events[kEventsMost];
	UInt32 n = 0;
	for (UInt32 k = 0; k < g_endedCount; ++k) {
		CountEnded(g_ended[k], events, n);
	}
	g_endedCount = 0;
	if (!g_testDone && g_settings->testCamp >= 0 && static_cast<UInt32>(g_settings->testCamp) < kCampCount) {
		g_testDone = true;
		const UInt32 i = static_cast<UInt32>(g_settings->testCamp);
		CampTakeRoster(g_state.camps[i], *g_settings);
		ApplyPeople(i, false);
		n += BeginCampAttack(g_state.camps[i], i, *g_settings, now, Near(distance, i), g_roll(), events + n,
		                     kEventsMost - n);
		g_log("Camps: test - TestCamp=%d attacked at the first look", g_settings->testCamp);
	}
	n += StepCampDays(g_state, *g_settings, now, g_roll, events + n, kEventsMost - n);
	for (UInt32 i = 0; i < kCampCount; ++i) {
		Camp& c = g_state.camps[i];
		if (CampTakeRoster(c, *g_settings)) {
			ApplyPeople(i, false);
		}
		// The toll stands, and the attack counts as defended, while the camp's
		// fight is shown - near but not shown (a second camp attacked with
		// CampsAttackedAtOnce=1), it is rolled as away.
		n += StepCamp(c, i, *g_settings, now, Shown(i), g_roll, events + n, kEventsMost - n);
		// The dead among the people of a camp the player is at (only there
		// does the game fight).
		if (Near(distance, i) && c.status != CampStatus::Overrun) {
			for (UInt32 slot = 0; slot < kCampSlots; ++slot) {
				const UInt16 bit = static_cast<UInt16>(1u << slot);
				if ((c.roster & ~c.lost & bit) == 0) {
					continue;
				}
				const UInt32 ref = Person(i, slot);
				if (LooksLikeObject(ref) && g_dead != nullptr && g_dead(ref)) {
					g_log("Camps: camp %02u %s - slot %u (%08X) dead, %u left", i, kCampPlaces[i].name, slot,
					      FormIdOf(ref), CampAlive(c) - 1);
					g_shownPeople[i] = static_cast<UInt16>(g_shownPeople[i] & ~bit);
					n += CampPersonDied(c, i, slot, events + n, kEventsMost - n);
				}
			}
		}
	}
	for (UInt32 k = 0; k < n; ++k) {
		Carry(events[k], distance, now);
	}
	UInt32 shown[kCampShownMost];
	const UInt32 count = ChooseShownCamps(g_state, *g_settings, distance, shown);
	// A bank keeps its camp while it is shown; the others free a bank.
	bool keep[kCampShownMost] = {};
	bool placed[kCampShownMost] = {};
	for (UInt32 s = 0; s < count; ++s) {
		for (UInt32 b = 0; b < kCampShownMost; ++b) {
			if (g_bankCamp[b] == static_cast<int>(shown[s])) {
				keep[b] = true;
				placed[s] = true;
			}
		}
	}
	for (UInt32 b = 0; b < kCampShownMost; ++b) {
		if (!keep[b] && g_bankCamp[b] >= 0) {
			HideBank(b);
		}
	}
	// A bank's camp taken on now is shown once this step; its walkers are led
	// at the next (harness camp-defend 14:56: led in the step they came out,
	// AddScriptPackage answered 0 for all six).
	bool fresh[kCampShownMost] = {};
	for (UInt32 s = 0; s < count; ++s) {
		for (UInt32 b = 0; !placed[s] && b < kCampShownMost; ++b) {
			if (g_bankCamp[b] < 0 && !keep[b]) {
				keep[b] = true;
				placed[s] = true;
				fresh[b] = true;
				ShowBank(b, shown[s]);
			}
		}
	}
	for (UInt32 b = 0; b < kCampShownMost; ++b) {
		if (g_bankCamp[b] >= 0 && !fresh[b]) {
			ShowBank(b, static_cast<UInt32>(g_bankCamp[b]));
		}
	}
}

CampsState& CurrentCamps() { return g_state; }

void ApplyCampsLoaded() {
	if (g_settings == nullptr || g_console == nullptr || g_espBase == 0) {
		return;
	}
	g_applied = true;
	for (UInt32 slot = 0; slot < kCampPool; ++slot) {
		Say(PoolRef(slot), "Disable");
	}
	UInt32 living = 0;
	UInt32 attacked = 0;
	UInt32 overrun = 0;
	for (UInt32 i = 0; i < kCampCount; ++i) {
		Camp& c = g_state.camps[i];
		CampTakeRoster(c, *g_settings);
		ApplyPeople(i, true);
		living += c.status == CampStatus::Living ? 1u : 0u;
		attacked += c.status == CampStatus::Attacked ? 1u : 0u;
		overrun += c.status == CampStatus::Overrun ? 1u : 0u;
	}
	g_log("Camps: loaded - %u living, %u attacked, %u overrun; the pool back in", living, attacked, overrun);
}

void ResetCampsInGame() {
	g_clock = 0.0f;
	for (UInt32 slot = 0; slot < kCampPool; ++slot) {
		g_out[slot] = false;
		g_corpse[slot] = false;
	}
	for (UInt32 b = 0; b < kCampShownMost; ++b) {
		g_bankCamp[b] = -1;
		g_bankLead[b] = false;
	}
	for (UInt16& people : g_shownPeople) {
		people = 0;
	}
	g_endedCount = 0;
	g_wasEnabled = true;
	g_testDone = false;
	g_applied = false;
	g_logLines = 0;
}

}  // namespace walkers
