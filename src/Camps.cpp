#include "Camps.h"

#include "CampCandidates.h"
#include "CityGates.h"
#include "Engine.h"
#include "Horde.h"
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
constexpr float kRadToDeg = 57.2957795f;  // the people's SetAngle
constexpr UInt32 kEventsMost = 64;
constexpr UInt32 kRefBaseOffset = 0x1C;
// The player's level: his TESNPC's TESActorBaseData (at +0x24, as the
// creatures' flags at +0x28 say) level at its +0x0E (xOBSE GameForms.h) -
// derived, logged at each reward so a run shows it.
constexpr UInt32 kNpcLevelOffset = 0x24 + 0x0E;
// TESLevItem: TESLeveledList at +0x24, its entries {data, next} at +0x28,
// ListData {level, pad, form, count} (xOBSE GameForms.h/GameObjects.h), as
// the creature lists Replace.cpp reads.
constexpr UInt8 kFormTypeLeveledItem = 0x2B;
constexpr UInt32 kLevListOffset = 0x28;
// The things reward's lists (Oblivion.esm, read 2026-11-08): LL1LootWeapon100,
// LL1LootArmor100, LL1LootPotions2Average - one drawn, then an entry of it.
constexpr UInt32 kThingLists[] = {0x00032D6E, 0x00032D79, 0x00058F36};
// A camp asks while the player is this near it.
constexpr float kEscortAskUnits = 3000.0f;
constexpr UInt32 kLogLines = 64;

const CampSettings* g_settings = nullptr;
CampConsoleFn g_console = nullptr;
CampRollFn g_roll = nullptr;
CampDeadFn g_dead = nullptr;
CampUnlistFn g_unlist = nullptr;
LogFn g_log = nullptr;
UInt32 g_espBase = 0;
CampsState g_state;
float g_clock = 0.0f;
bool g_wasEnabled = true;
bool g_testDone = false;
float g_testClock = 0.0f;  // seconds of steps since the load, for TestCamp
constexpr float kTestCampAfterSeconds = 30.0f;  // the harness's script started by then
// TestCamp's drops: 15 s after the camp's attack the player dropped from 600
// units above onto its first tent, 6 s on onto the ground 620 units toward
// the road - whether the props stand solid (the props are the ESP's own,
// under the candidate's marker; moved props were not, 2026-11-06).
constexpr float kTestDropAfterSeconds = 15.0f;
constexpr float kTestDropSeconds = 6.0f;
constexpr float kTestDropSample = 0.05f;
struct TestDrop {
	int stage = -1;  // -1 waiting, 0 the tent, 1 the ground, 2 done
	float clock = 0.0f;
	float sampleClock = 0.0f;
	float z[128] = {};
	UInt32 n = 0;
	float stops[2] = {-1e9f, -1e9f};
};
TestDrop g_drop;
bool g_applied = false;  // the rosters and markers set since the load
float g_followClock = 0.0f;
UInt16 g_resetMask = 0;  // escorted slots disabled for a reset, enabled the next step
float g_sinceReset[kCampSlots];  // seconds since each escorted slot's last reset (ForgetResets)
UInt16 g_heldLogged = 0;  // escorted slots whose hold by a package left alone is logged

void ForgetResets() {
	g_resetMask = 0;
	g_heldLogged = 0;
	for (float& since : g_sinceReset) {
		since = kEscortNeverReset;
	}
}
bool g_escortShown = false;  // the escorted kit still out, by the city
int g_testEscortStage = 0;
UInt32 g_probeSeconds = 0;
UInt32 g_markersActive = 0;  // CampActive when the markers were last set
// The kits: the camp each is at (-1 none), the slots it has enabled.
int g_kitCamp[kCampKits] = {-1, -1, -1, -1};
UInt16 g_kitPeople[kCampKits] = {};
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

const CampCandidate& Place(UInt32 i) {
	const UInt16 p = g_state.camps[i].place;
	return kCampCandidates[p < kCampCandidateCount ? p : 0];
}

const char* Name(UInt32 i) { return kCampNames[Place(i).name]; }

// Camp `i`'s person in `slot`: its kit's, 0 when no kit is at the camp.
UInt32 Person(UInt32 i, UInt32 slot) {
	const int kit = KitOf(g_kitCamp, i);
	return kit >= 0 ? Esp(KitPersonLocalId(static_cast<UInt32>(kit), slot)) : 0;
}

bool Say(UInt32 ref, const char* line) {
	return LooksLikeObject(ref) && g_console != nullptr && g_console(ref, line);
}

bool SayF(UInt32 ref, const char* format, float value) {
	char line[64];
	std::snprintf(line, sizeof(line), format, value);
	return Say(ref, line);
}

void Line(const char* text) {
	char line[200];
	std::snprintf(line, sizeof(line), "Message \"%s\"", text);
	Say(LookupForm(kPlayerRefId), line);
}

bool Logged() {
	if (g_logLines >= kLogLines) {
		return false;
	}
	++g_logLines;
	return true;
}

// The player's distance from each camp in play, outdoors in Tamriel; -1
// elsewhere and for the camps out of play.
void Distances(float* distance) {
	for (UInt32 i = 0; i < kCampMost; ++i) {
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
	for (UInt32 i = 0; i < kCampMost; ++i) {
		if (!CampInPlay(g_state, *g_settings, i)) {
			continue;
		}
		const float dx = px - Place(i).x;
		const float dy = py - Place(i).y;
		distance[i] = std::sqrt(dx * dx + dy * dy);
	}
	// An empty camp's place, and one whose people are in a city, take no kit
	// there (EscortDistances decides the escorted one's).
	for (UInt32 i = 0; i < kCampMost; ++i) {
		const CampStatus st = g_state.camps[i].status;
		if (st == CampStatus::Left || st == CampStatus::Gone) {
			distance[i] = -1.0f;
		}
	}
}

// The worldspace a reference stands in, 0 indoors or unknown.
UInt32 WorldOf(UInt32 ref) {
	const UInt32 cell = LooksLikeObject(ref) ? Read(ref + kRefParentCellOffset) : 0;
	if (!LooksLikeObject(cell) || (Read8(cell + kCellFlagsOffset) & 1) != 0) {
		return 0;
	}
	const UInt32 world = Read(cell + kCellWorldspaceOffset);
	return LooksLikeObject(world) ? world : 0;
}

bool OutdoorsAt(UInt32 ref, float& x, float& y);

// The escorted camp's kit (4.20.6): kept on the way wherever the player goes
// (a door into a house must not free it); by the city as EscortKitWanted
// says - in its streets or near its gate.
void EscortDistances(float* distance) {
	const int e = g_state.escort.camp;
	if (e < 0 || e >= static_cast<int>(kCampMost)) {
		return;
	}
	const Camp& c = g_state.camps[e];
	if (c.status == CampStatus::Escort) {
		distance[e] = 0.0f;
		return;
	}
	if (c.status != CampStatus::Gone) {
		return;
	}
	const int g = g_state.escort.gate;
	const UInt32 player = LookupForm(kPlayerRefId);
	float toGate = -1.0f;
	bool inCity = false;
	float px = 0.0f;
	float py = 0.0f;
	if (g >= 0 && g < static_cast<int>(kCityGateCount)) {
		if (OutdoorsAt(player, px, py)) {
			const float gx = px - kCityGates[g].outX;
			const float gy = py - kCityGates[g].outY;
			toGate = std::sqrt(gx * gx + gy * gy);
		}
		const UInt32 cityWorld = WorldOf(LookupForm(kCityGates[g].back));
		inCity = cityWorld != 0 && WorldOf(player) == cityWorld;
	}
	const bool wanted = EscortKitWanted(g_settings->escortIntoCity, g_escortShown, inCity, toGate);
	if (!g_settings->escortIntoCity) {
		g_escortShown = wanted;
	}
	distance[e] = wanted ? 0.0f : -1.0f;
}

// A reference outdoors in Tamriel: its x and y.
bool OutdoorsAt(UInt32 ref, float& x, float& y) {
	if (!LooksLikeObject(ref)) {
		return false;
	}
	const UInt32 cell = Read(ref + kRefParentCellOffset);
	if (!LooksLikeObject(cell) || (Read8(cell + kCellFlagsOffset) & 1) != 0) {
		return false;
	}
	const UInt32 world = Read(cell + kCellWorldspaceOffset);
	if (!LooksLikeObject(world) || FormIdOf(world) != kTamrielWorldspace) {
		return false;
	}
	x = FloatAt(ref + kRefPositionOffset);
	y = FloatAt(ref + kRefPositionOffset + 4);
	return true;
}

bool Near(const float* distance, UInt32 camp) { return distance[camp] >= 0.0f && distance[camp] <= kCampSeenUnits; }

UInt32 PlayerLevel() {
	const UInt32 player = LookupForm(kPlayerRefId);
	const UInt32 base = LooksLikeObject(player) ? Read(player + kRefBaseOffset) : 0;
	if (!LooksLikeObject(base)) {
		return 1;
	}
	const SInt16 level = *reinterpret_cast<const SInt16*>(base + kNpcLevelOffset);
	return level < 1 ? 1u : static_cast<UInt32>(level);
}

// One entry off leveled item list `list` for `level` (PickLeveled), nested
// lists followed down to four deep; its form and count, 0 none.
UInt32 DrawThing(UInt32 list, UInt32 level, UInt32& count) {
	for (UInt32 depth = 0; depth < 4; ++depth) {
		if (!LooksLikeObject(list) || Read8(list + kFormTypeOffset) != kFormTypeLeveledItem) {
			return 0;
		}
		UInt16 levels[64];
		UInt32 forms[64];
		UInt16 counts[64];
		UInt32 n = 0;
		for (UInt32 node = list + kLevListOffset; LooksLikeObject(node) && n < 64; node = Read(node + 4)) {
			const UInt32 data = Read(node);
			if (!LooksLikeObject(data)) {
				break;
			}
			levels[n] = *reinterpret_cast<const UInt16*>(data);
			forms[n] = Read(data + 4);
			counts[n] = *reinterpret_cast<const UInt16*>(data + 8);
			++n;
		}
		const int k = PickLeveled(levels, n, static_cast<int>(level), g_roll());
		if (k < 0 || !LooksLikeObject(forms[k])) {
			return 0;
		}
		if (Read8(forms[k] + kFormTypeOffset) != kFormTypeLeveledItem) {
			count = counts[k] > 0 ? counts[k] : 1;
			return forms[k];
		}
		list = forms[k];
	}
	return 0;
}

// `things` of the loot lists given to the player; how many were.
UInt32 GiveThings(UInt32 things) {
	const UInt32 player = LookupForm(kPlayerRefId);
	const UInt32 level = PlayerLevel();
	UInt32 given = 0;
	for (UInt32 k = 0; k < things; ++k) {
		// A list with nothing low enough (the potions start at level 2,
		// harness camp-escort 2026-11-08): the next list tried.
		constexpr UInt32 lists = sizeof(kThingLists) / sizeof(kThingLists[0]);
		const UInt32 first = g_roll() % lists;
		UInt32 list = 0;
		UInt32 count = 1;
		UInt32 item = 0;
		for (UInt32 tried = 0; tried < lists && item == 0; ++tried) {
			list = LookupForm(kThingLists[(first + tried) % lists]);
			item = DrawThing(list, level, count);
		}
		if (item == 0) {
			g_log("Camps: a thing - none drawn off %08X at level %u", LooksLikeObject(list) ? FormIdOf(list) : 0, level);
			continue;
		}
		char line[48];
		std::snprintf(line, sizeof(line), "AddItem %08X %u", FormIdOf(item), count);
		const bool said = Say(player, line);
		g_log("Camps: a thing - %08X x%u off %08X at the player's level %u (%s answered %d)", FormIdOf(item), count,
		      FormIdOf(list), level, line, said ? 1 : 0);
		++given;
	}
	return given;
}

// The people a camp should have out: its roster's living, none when off or
// overrun.
UInt16 Wanted(const Camp& c) { return CampPeopleOut(c, g_settings->enabled); }

// Camp `i`'s people enabled and disabled as Wanted says, when a kit is at it;
// `all`: every slot told, else only the changes. A dead one is left to the
// rise.
void ApplyPeople(UInt32 i, bool all) {
	const int kit = KitOf(g_kitCamp, i);
	if (kit < 0) {
		return;
	}
	const Camp& c = g_state.camps[i];
	const UInt16 wanted = Wanted(c);
	UInt32 enabled = 0;
	UInt32 disabled = 0;
	for (UInt32 slot = 0; slot < kCampSlots; ++slot) {
		const UInt16 bit = static_cast<UInt16>(1u << slot);
		const bool want = (wanted & bit) != 0;
		const bool shown = (g_kitPeople[kit] & bit) != 0;
		if (!all && want == shown) {
			continue;
		}
		const UInt32 ref = Person(i, slot);
		if (!LooksLikeObject(ref) || (!want && (c.lost & bit) != 0 && g_dead != nullptr && g_dead(ref))) {
			continue;
		}
		Say(ref, want ? "Enable" : "Disable");
		if (want) {
			// Their camp life's packages (sleep, wander) near where they now
			// stand.
			Say(ref, "EvaluatePackage");
		}
		want ? ++enabled : ++disabled;
	}
	g_kitPeople[kit] = wanted;
	if ((enabled > 0 || disabled > 0) && Logged()) {
		g_log("Camps: camp %02u %s - %u enabled, %u disabled (kit %d, roster %04X, lost %04X)", i, Name(i), enabled,
		      disabled, kit, c.roster, c.lost);
	}
}

// A kit person dead (the rise placed its walker and hid it, or it lies)
// raised and dressed again - its things went onto its walker - and taken off
// the rise's list, so it does not rise where it next stands.
bool Raise(UInt32 ref, UInt32 kit, UInt32 slot) {
	if (!LooksLikeObject(ref) || g_dead == nullptr || !g_dead(ref)) {
		return false;
	}
	if (g_unlist != nullptr) {
		g_unlist(FormIdOf(ref));
	}
	Say(ref, "SetDestroyed 0");
	Say(ref, "Resurrect");
	Say(ref, "RemoveAllItems");
	const UInt32* outfit = kCampOutfits[CampBaseIndex(kit, slot)];
	for (UInt32 k = 0; k < 4; ++k) {
		char line[48];
		std::snprintf(line, sizeof(line), "AddItem %08X 1", outfit[k]);
		Say(ref, line);
		std::snprintf(line, sizeof(line), "EquipItem %08X", outfit[k]);
		Say(ref, line);
	}
	return true;
}

// The camp's people set up anew (it came back): the dead raised, all as the
// roster says.
void RaisePeople(UInt32 i) {
	const int kit = KitOf(g_kitCamp, i);
	UInt32 raised = 0;
	if (kit >= 0) {
		for (UInt32 slot = 0; slot < kCampSlots; ++slot) {
			raised += Raise(Person(i, slot), static_cast<UInt32>(kit), slot) ? 1u : 0u;
		}
		g_kitPeople[kit] = 0;
		ApplyPeople(i, true);
	}
	g_log("Camps: camp %02u %s is back - %u raised and dressed, %u in the camp", i, Name(i), raised,
	      CampAlive(g_state.camps[i]));
}

// Kit `kit`'s people put away: disabled where they stand.
void FreeKit(UInt32 kit) {
	for (UInt32 slot = 0; slot < kCampSlots; ++slot) {
		Say(Esp(KitPersonLocalId(kit, slot)), "Disable");
	}
	g_kitPeople[kit] = 0;
}

// Camp `i`'s map marker: its candidate's (the props' enable parent).
UInt32 MarkerId(UInt32 i) { return EspFormId(g_espBase, CandidateMarkerLocalId(g_state.camps[i].place)); }

// Kit `kit` put at camp `i`: its people (the dead raised) onto their spots by
// the camp's map marker, then enabled as the roster says.
// Where person `slot` stands in the city its camp was brought to: the gate's
// spot inside, a step to either side by slot.
void CitySpotLine(UInt32 slot, char* line, UInt32 size) {
	const CityGate& gate = kCityGates[g_state.escort.gate];
	const float side = static_cast<float>(static_cast<int>(slot % 7) - 3) * 60.0f;
	const float ahead = static_cast<float>(slot / 7) * 80.0f;
	std::snprintf(line, size, "MoveTo %08X %.0f %.0f %.0f", gate.back, gate.inDx + side, gate.inDy + ahead, gate.inDz + 20.0f);
}

bool InCity(UInt32 i) {
	const int g = g_state.escort.gate;
	return g_settings->escortIntoCity && g_state.camps[i].status == CampStatus::Gone && static_cast<int>(i) == g_state.escort.camp &&
	       g >= 0 && g < static_cast<int>(kCityGateCount);
}

void SetUpKit(UInt32 kit, UInt32 i) {
	if (InCity(i)) {
		// Its people live in the city now: put back in its streets.
		UInt32 raised = 0;
		for (UInt32 slot = 0; slot < kCampSlots; ++slot) {
			const UInt32 ref = Esp(KitPersonLocalId(kit, slot));
			if (!LooksLikeObject(ref)) {
				continue;
			}
			raised += Raise(ref, kit, slot) ? 1u : 0u;
			Say(ref, "Disable");
			char line[96];
			CitySpotLine(slot, line, sizeof(line));
			Say(ref, line);
		}
		g_kitPeople[kit] = 0;
		g_log("Camps: kit %u to camp %02u %s's people in the city by %s (%u raised)", kit, i, Name(i),
		      kCityGates[g_state.escort.gate].name, raised);
		ApplyPeople(i, true);
		return;
	}
	const CampCandidate& place = Place(i);
	const UInt32 marker = MarkerId(i);
	UInt32 raised = 0;
	for (UInt32 slot = 0; slot < kCampSlots; ++slot) {
		const UInt32 ref = Esp(KitPersonLocalId(kit, slot));
		if (!LooksLikeObject(ref)) {
			continue;
		}
		raised += Raise(ref, kit, slot) ? 1u : 0u;
		Say(ref, "Disable");
		const CampSpot& at = place.people[slot];
		char line[96];
		std::snprintf(line, sizeof(line), "MoveTo %08X %.0f %.0f %.0f", marker, at.x - place.x, at.y - place.y,
		              at.z - place.z);
		Say(ref, line);
		SayF(ref, "SetAngle z %.2f", at.rz * kRadToDeg);
	}
	g_kitPeople[kit] = 0;
	g_log("Camps: kit %u to camp %02u %s (%.0f,%.0f,%.0f; %u raised)", kit, i, Name(i), place.x, place.y, place.z,
	      raised);
	ApplyPeople(i, true);
}

// Every candidate's map marker - its props' enable parent: those of the camps
// in play enabled (the camp and its props show), the others disabled.
void PlaceMarkers() {
	bool used[kCampCandidateCount] = {};
	for (UInt32 i = 0; i < kCampMost; ++i) {
		if (g_settings->enabled && CampInPlay(g_state, *g_settings, i)) {
			used[g_state.camps[i].place] = true;
		}
	}
	UInt32 shown = 0;
	UInt32 missing = 0;
	for (UInt32 c = 0; c < kCampCandidateCount; ++c) {
		const UInt32 marker = Esp(CandidateMarkerLocalId(c));
		if (!LooksLikeObject(marker)) {
			++missing;
			continue;
		}
		Say(marker, used[c] ? "Enable" : "Disable");
		shown += used[c] ? 1u : 0u;
	}
	g_markersActive = CampActive(*g_settings);
	g_log("Camps: %u candidates' markers and props enabled, the other %u disabled (%u not found)", shown,
	      kCampCandidateCount - shown - missing, missing);
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
		      Name(static_cast<UInt32>(g_bankCamp[bank])), hidden);
	}
	g_bankCamp[bank] = -1;
	g_bankLead[bank] = false;
}

// Bank `bank` at camp `i`: as many out as CampWalkersOut wants - an attack's
// on the ring away from the road, led at the camp; an overrun camp's among
// its tents. A slot never used first, else an ended one raised.
void ShowBank(UInt32 bank, UInt32 i) {
	const Camp& c = g_state.camps[i];
	const CampCandidate& place = Place(i);
	const bool attack = c.status == CampStatus::Attacked;
	if (g_bankCamp[bank] != static_cast<int>(i)) {
		HideBank(bank);
		g_bankCamp[bank] = static_cast<int>(i);
		g_log("Camps: the player at camp %02u %s - %s (%u walkers, %u at once)", i, Name(i),
		      attack ? "its attackers come out" : "its walkers stand among the tents", c.walkersLeft,
		      CampWalkersOut(c));
		if (attack) {
			// The bank's marker on the camp; the package a step later (the
			// horde's MoveTo notes: a package's target is read when given).
			char line[48];
			std::snprintf(line, sizeof(line), "MoveTo %08X 0 0 0", MarkerId(i));
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
			CampWalkerSpot(place.facing, attack, k, dx, dy);
			const float dz = place.spotZ[(attack ? 0 : kCampWalkersAtOnce) + k] - place.z + kSpotLift;
			char line[96];
			std::snprintf(line, sizeof(line), "MoveTo %08X %.0f %.0f %.0f", MarkerId(i),
			              dx, dy, dz);
			Say(ref, "Enable");
			const bool moved = Say(ref, line);
			g_out[slot] = true;
			g_corpse[slot] = false;
			++out;
			if (Logged()) {
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

// The reward's fame, gold and things given; `what` gets " (+N fame, N gold,
// N things)" or stays empty.
void GiveReward(UInt32 fame, UInt32 gold, UInt32 things, char* what, UInt32 size) {
	const UInt32 player = LookupForm(kPlayerRefId);
	char line[64];
	what[0] = '\0';
	if (fame > 0) {
		std::snprintf(line, sizeof(line), "ModPCFame %u", fame);
		Say(player, line);
		std::snprintf(what, size, " (+%u fame", fame);
	}
	if (gold > 0) {
		std::snprintf(line, sizeof(line), "AddItem %08X %u", kGoldId, gold);
		Say(player, line);
		std::snprintf(what + std::strlen(what), size - std::strlen(what), "%s%u gold", what[0] != '\0' ? ", " : " (",
		              gold);
	}
	const UInt32 given = GiveThings(things);
	if (given > 0) {
		std::snprintf(what + std::strlen(what), size - std::strlen(what), "%s%s", what[0] != '\0' ? ", " : " (",
		              given == 1 ? "a thing of theirs" : "some of their things");
	}
	if (what[0] != '\0') {
		std::snprintf(what + std::strlen(what), size - std::strlen(what), ")");
	}
}

bool QuestSound() {
	char line[32];
	std::snprintf(line, sizeof(line), "PlaySound %08X", kQuestSound);
	return Say(LookupForm(kPlayerRefId), line);
}

void Reward(UInt32 i) {
	char text[200];
	char what[96];
	GiveReward(g_settings->rewardFame ? g_settings->famePoints : 0, g_settings->rewardGold ? g_settings->gold : 0,
	           CampThings(g_settings->rewardThings, CampAlive(g_state.camps[i]), 0, false), what, sizeof(what));
	std::snprintf(text, sizeof(text), "The refugees %s are safe - the walkers are beaten%s.", Name(i), what);
	Line(text);
	const bool sound = QuestSound();
	g_log("Camps: camp %02u %s defended - %s (the sound answered %d)", i, Name(i), text, sound ? 1 : 0);
	NoteCampInNews(true);
}

const char* GateName(int gate) {
	return gate >= 0 && gate < static_cast<int>(kCityGateCount) ? kCityGates[gate].name : "the city";
}

// Every living escorted person told `line`; how many.
UInt32 TellEscorted(UInt32 i, const char* line) {
	const Camp& c = g_state.camps[i];
	UInt32 told = 0;
	for (UInt32 slot = 0; slot < kCampSlots; ++slot) {
		if ((c.roster & ~c.lost & (1u << slot)) == 0) {
			continue;
		}
		const UInt32 ref = Person(i, slot);
		if (LooksLikeObject(ref) && (g_dead == nullptr || !g_dead(ref))) {
			Say(ref, line);
			++told;
		}
	}
	return told;
}

void FollowLine(char* line, UInt32 size) {
	std::snprintf(line, size, "AddScriptPackage %08X", EspFormId(g_espBase, kEscortFollowLocalId));
}

void StopFollowing(UInt32 i) {
	// One reset this second: back before the escort ends.
	for (UInt32 slot = 0; slot < kCampSlots; ++slot) {
		if ((g_resetMask & (1u << slot)) != 0) {
			Say(Person(i, slot), "Enable");
		}
	}
	g_resetMask = 0;
	TellEscorted(i, "RemoveScriptPackage");
	TellEscorted(i, "EvaluatePackage");
}

void Carry(const CampEvent& ev, const float* distance, float now) {
	const UInt32 i = ev.camp;
	Camp& c = g_state.camps[i];
	const bool near = Near(distance, i);
	char text[160];
	switch (ev.kind) {
		case CampEvent::Begin:
			g_log("Camps: camp %02u %s attacked - %u walkers (game day %.3f, the player %s)", i, Name(i), c.walkersLeft,
			      now, near ? "near" : "away");
			if (near) {
				std::snprintf(text, sizeof(text), "Walkers are coming for the refugees %s!", Name(i));
				Line(text);
			}
			break;
		case CampEvent::PersonLost: {
			const UInt32 ref = Person(i, ev.slot);
			Say(ref, "Disable");
			const int kit = KitOf(g_kitCamp, i);
			if (kit >= 0) {
				g_kitPeople[kit] = static_cast<UInt16>(g_kitPeople[kit] & ~(1u << ev.slot));
			}
			g_log("Camps: camp %02u %s lost slot %u away - %u left, %u walkers", i, Name(i), ev.slot, CampAlive(c),
			      c.walkersLeft);
			break;
		}
		case CampEvent::HeldAway:
			g_log("Camps: camp %02u %s held, the player away - %u of its people alive", i, Name(i), CampAlive(c));
			break;
		case CampEvent::Defended:
			Reward(i);
			break;
		case CampEvent::Overrun:
			NoteCampInNews(false);
			g_log("Camps: camp %02u %s overrun (game day %.3f) - %u walkers stay at it", i, Name(i), now,
			      c.walkersLeft);
			if (near) {
				std::snprintf(text, sizeof(text), "The refugees %s are lost.", Name(i));
				Line(text);
			}
			ApplyPeople(i, false);
			break;
		case CampEvent::Return:
			RaisePeople(i);
			break;
		case CampEvent::EscortBegin: {
			char line[48];
			FollowLine(line, sizeof(line));
			g_state.escort.kit = KitOf(g_kitCamp, i);
			const UInt32 told = TellEscorted(i, line);
			// The first look for those held a second on, then every
			// EscortFollowSeconds.
			g_followClock = g_settings->escortFollowSeconds > 1.0f ? g_settings->escortFollowSeconds - 1.0f : 0.0f;
			ForgetResets();
			std::snprintf(text, sizeof(text), "The refugees %s follow you. Lead them to %s - and don't leave them behind.",
			              Name(i), GateName(g_state.escort.gate));
			Line(text);
			g_log("Camps: camp %02u %s - the escort begun, %u set out for %s (%s to %u; game day %.3f)", i, Name(i),
			      g_state.escort.started, GateName(g_state.escort.gate), line, told, now);
			break;
		}
		case CampEvent::EscortArrived: {
			const UInt32 alive = CampAlive(c);
			const UInt32 started = g_state.escort.started;
			char what[96];
			GiveReward(ScaledReward(g_settings->escortFame, alive, started),
			           ScaledReward(g_settings->escortGold, alive, started),
			           CampThings(g_settings->rewardThings, alive, started, true), what, sizeof(what));
			StopFollowing(i);
			g_escortShown = true;
			if (InCity(i)) {
				// Into the city through its gate (EscortIntoCity).
				UInt32 moved = 0;
				for (UInt32 slot = 0; slot < kCampSlots; ++slot) {
					if ((c.roster & ~c.lost & (1u << slot)) == 0) {
						continue;
					}
					const UInt32 ref = Person(i, slot);
					char line[96];
					CitySpotLine(slot, line, sizeof(line));
					moved += Say(ref, line) ? 1u : 0u;
					Say(ref, "EvaluatePackage");
				}
				g_log("Camps: camp %02u %s - %u taken into the city by %s (MoveTo its city-side door %08X)", i, Name(i),
				      moved, GateName(g_state.escort.gate), kCityGates[g_state.escort.gate].back);
			}
			std::snprintf(text, sizeof(text), "You brought %u of %u refugees safe to %s%s%s.", alive, started,
			              GateName(g_state.escort.gate), what, InCity(i) ? " - they go into the city" : "");
			Line(text);
			const bool sound = QuestSound();
			NoteCampInNews(true);
			g_log("Camps: camp %02u %s - the escort arrived: %s (the sound answered %d; game day %.3f)", i, Name(i), text,
			      sound ? 1 : 0, now);
			break;
		}
		case CampEvent::EscortBehind:
			StopFollowing(i);
			std::snprintf(text, sizeof(text), "You left the refugees %s behind - they go back to their camp.", Name(i));
			Line(text);
			g_log("Camps: camp %02u %s - the escort failed: left behind; %u go back to the camp", i, Name(i),
			      CampAlive(c));
			// Back at their camp: the kit set up there anew when the player next
			// comes near.
			{
				const int kit = KitOf(g_kitCamp, i);
				if (kit >= 0) {
					g_kitCamp[kit] = -1;
					FreeKit(static_cast<UInt32>(kit));
				}
			}
			break;
		case CampEvent::EscortDead:
			std::snprintf(text, sizeof(text), "None of the refugees %s made it.", Name(i));
			Line(text);
			NoteCampInNews(false);
			g_log("Camps: camp %02u %s - the escort failed: all dead on the way; the camp lies empty", i, Name(i));
			break;
		case CampEvent::EscortLeft: {
			const int kit = KitOf(g_kitCamp, i);
			if (kit >= 0) {
				g_kitCamp[kit] = -1;
				FreeKit(static_cast<UInt32>(kit));
			}
			g_escortShown = false;
			g_log("Camps: camp %02u %s - its refugees moved on from the city (game day %.3f); the camp lies empty", i,
			      Name(i), now);
			break;
		}
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
	g_log("Camps: walker %08X ended at camp %02u %s - %u left", refId, i, Name(i), g_state.camps[i].walkersLeft);
}

// The places of the camps in play that have none rolled (a new game, a
// larger CampCount), their map markers set.
void RollPlaces() {
	const UInt32 placed = RollCampPlaces(g_state, *g_settings, kCampCandidates, kCampCandidateCount, g_roll);
	UInt32 inPlay = 0;
	for (UInt32 i = 0; i < kCampMost; ++i) {
		inPlay += CampInPlay(g_state, *g_settings, i) ? 1u : 0u;
	}
	if (placed > 0 || inPlay < CampActive(*g_settings)) {
		g_log("Camps: %u places rolled - %u camps in play of CampCount=%u, at least %.0f units apart (%u candidates)",
		      placed, inPlay, CampActive(*g_settings), g_settings->spacingUnits, kCampCandidateCount);
		for (UInt32 i = 0; i < kCampMost && placed > 0; ++i) {
			if (CampInPlay(g_state, *g_settings, i)) {
				g_log("Camps: camp %02u %s at %.0f,%.0f,%.0f (candidate %u%s)", i, Name(i), Place(i).x, Place(i).y,
				      Place(i).z, g_state.camps[i].place, Place(i).byRoad != 0 ? ", by a road" : "");
			}
		}
	}
	if (placed > 0 || g_markersActive != CampActive(*g_settings)) {
		PlaceMarkers();
	}
}

// The escort's globals, its ask and its step: the offer (1 a camp within
// kEscortAskUnits asks, 2 one on the way, 0 none) for the refugees' lines;
// the ask the yes sets - taken by the nearest camp that asks; the escort's
// distances from its living people.
UInt32 SetGlobal(UInt32 local, float value) {
	const UInt32 global = Esp(local);
	if (!LooksLikeObject(global) || Read8(global + kFormTypeOffset) != kFormTypeGlobal) {
		return 0;
	}
	*reinterpret_cast<float*>(global + kGlobalValueOffset) = value;
	return global;
}

float GlobalOf(UInt32 local) {
	const UInt32 global = Esp(local);
	return LooksLikeObject(global) && Read8(global + kFormTypeOffset) == kFormTypeGlobal
	           ? FloatAt(global + kGlobalValueOffset)
	           : 0.0f;
}

int AskingCamp(const float* distance) {
	int best = -1;
	for (UInt32 i = 0; i < CampActive(*g_settings); ++i) {
		if (distance[i] < 0.0f || distance[i] > kEscortAskUnits || KitOf(g_kitCamp, i) < 0 ||
		    !CampOffersEscort(g_state, *g_settings, i)) {
			continue;
		}
		if (best < 0 || distance[i] < distance[best]) {
			best = static_cast<int>(i);
		}
	}
	return best;
}

UInt32 StepEscortInGame(const float* distance, float now, CampEvent* events, UInt32 most) {
	UInt32 n = 0;
	const int asking = AskingCamp(distance);
	if (GlobalOf(kEscortAskGlobalLocalId) >= 1.0f) {
		SetGlobal(kEscortAskGlobalLocalId, 0.0f);
		if (asking >= 0) {
			const CampCandidate& place = Place(static_cast<UInt32>(asking));
			const int gate = NearestGate(kCityGates, kCityGateCount, place.x, place.y);
			n += BeginEscort(g_state, *g_settings, static_cast<UInt32>(asking), gate, now, events + n, most - n);
		} else {
			g_log("Camps: the escort asked for, but no camp near asks (an escort under way, or none within %.0f units)",
			      kEscortAskUnits);
		}
	}
	const int e = g_state.escort.camp;
	SetGlobal(kEscortOfferGlobalLocalId,
	          e >= 0 && g_state.camps[e].status == CampStatus::Escort ? 2.0f : (asking >= 0 ? 1.0f : 0.0f));
	if (e < 0) {
		return n;
	}
	const UInt32 i = static_cast<UInt32>(e);
	float behind = -1.0f;
	float toGate = -1.0f;
	float px = 0.0f;
	float py = 0.0f;
	const bool playerOut = OutdoorsAt(LookupForm(kPlayerRefId), px, py);
	const int g = g_state.escort.gate;
	const Camp& c = g_state.camps[i];
	for (UInt32 slot = 0; playerOut && c.status == CampStatus::Escort && slot < kCampSlots; ++slot) {
		if ((c.roster & ~c.lost & (1u << slot)) == 0) {
			continue;
		}
		const UInt32 ref = Person(i, slot);
		float x = 0.0f;
		float y = 0.0f;
		if ((g_dead != nullptr && g_dead(ref)) || !OutdoorsAt(ref, x, y)) {
			continue;
		}
		const float d = std::sqrt((x - px) * (x - px) + (y - py) * (y - py));
		behind = behind < 0.0f || d < behind ? d : behind;
		if (g >= 0 && g < static_cast<int>(kCityGateCount)) {
			const float gx = x - kCityGates[g].outX;
			const float gy = y - kCityGates[g].outY;
			const float dg = std::sqrt(gx * gx + gy * gy);
			toGate = toGate < 0.0f || dg < toGate ? dg : toGate;
		}
	}
	// TestEscort: what each of them runs, each second for the first 20 (the
	// follow found dropped, 2026-11-09).
	if (c.status == CampStatus::Escort && g_settings->testEscort >= 0 && g_probeSeconds < 20) {
		++g_probeSeconds;
		for (UInt32 slot = 0; slot < kCampSlots; ++slot) {
			if ((c.roster & ~c.lost & (1u << slot)) == 0) {
				continue;
			}
			const UInt32 ref = Person(i, slot);
			float x = 0.0f;
			float y = 0.0f;
			const bool out = OutdoorsAt(ref, x, y);
			bool high = false;
			const UInt32 package = ActorPackageNow(FormIdOf(ref), high);
			g_log("Camps: probe %2u s - slot %2u %.0f units off, package %08X type %u, high %d, combat %d", g_probeSeconds,
			      slot, out && playerOut ? std::sqrt((x - px) * (x - px) + (y - py) * (y - py)) : -1.0f, package,
			      PackageType(package), high ? 1 : 0, ActorInCombat(FormIdOf(ref)) ? 1 : 0);
		}
	}
	if (c.status == CampStatus::Escort) {
		g_followClock += kStepSeconds;
		for (float& since : g_sinceReset) {
			since = since < kEscortNeverReset ? since + kStepSeconds : since;
		}
		const bool resend = g_settings->escortFollowSeconds > 0.0f && g_followClock >= g_settings->escortFollowSeconds;
		if (resend || (g_settings->escortFollowSeconds <= 0.0f && g_followClock >= 5.0f && Logged())) {
			if (!resend) {
				g_followClock = 0.0f;
				g_log("Camps: escort of camp %02u - the nearest %.0f units from the player, %.0f from %s (no resend)", i,
				      behind, toGate, GateName(g));
			}
		}
		// Those reset a second ago: enabled again and given the follow.
		if (g_resetMask != 0) {
			char line[48];
			FollowLine(line, sizeof(line));
			for (UInt32 slot = 0; slot < kCampSlots; ++slot) {
				if ((g_resetMask & (1u << slot)) == 0) {
					continue;
				}
				const UInt32 ref = Person(i, slot);
				Say(ref, "Enable");
				Say(ref, line);
				g_log("Camps: escort of camp %02u - slot %u enabled again and given the follow", i, slot);
			}
			g_resetMask = 0;
		}
		if (resend) {
			g_followClock = 0.0f;
			char line[48];
			FollowLine(line, sizeof(line));
			const UInt32 follow = EspFormId(g_espBase, kEscortFollowLocalId);
			for (UInt32 slot = 0; slot < kCampSlots; ++slot) {
				if ((c.roster & ~c.lost & (1u << slot)) == 0) {
					continue;
				}
				const UInt32 ref = Person(i, slot);
				if (!LooksLikeObject(ref) || (g_dead != nullptr && g_dead(ref))) {
					continue;
				}
				bool high = false;
				const UInt32 package = ActorPackageNow(FormIdOf(ref), high);
				const UInt8 type = PackageType(package);
				const bool fights = ActorInCombat(FormIdOf(ref));
				if (EscortResets(package, follow, fights, type, g_sinceReset[slot])) {
					Say(ref, "Disable");
					g_resetMask = static_cast<UInt16>(g_resetMask | (1u << slot));
					g_sinceReset[slot] = 0.0f;
					g_heldLogged = static_cast<UInt16>(g_heldLogged & ~(1u << slot));
					g_log("Camps: escort of camp %02u - slot %u held by package %08X (type %u): reset", i, slot, package,
					      type);
					continue;
				}
				// Held all the same - a greeting, or reset a moment ago: said
				// once, the follow sent on.
				const bool held = EscortResets(package, follow, fights, kPackageTypeDialogue, kEscortNeverReset);
				if (held && (g_heldLogged & (1u << slot)) == 0) {
					g_log("Camps: escort of camp %02u - slot %u held by package %08X (type %u), left to end (%s)", i, slot,
					      package, type,
					      type == kPackageTypeGreet ? "a greeting" : "reset within the last half minute");
				}
				g_heldLogged = static_cast<UInt16>(held ? g_heldLogged | (1u << slot) : g_heldLogged & ~(1u << slot));
				Say(ref, line);
			}
			if (Logged()) {
				g_log("Camps: escort of camp %02u - the nearest %.0f units from the player, %.0f from %s", i, behind,
				      toGate, GateName(g));
			}
		}
	}
	n += StepEscort(g_state, *g_settings, now, behind, toGate, events + n, most - n);
	return n;
}

// TestEscort: camp N's refugees asked and led - the player taken to the camp
// 30 s after the load (1500 units off its fire), the ask set at 40 s, at 60 s
// the escorted's distance logged (whether they followed), then the player
// and they taken to the gate (or, TestEscortBehind, the player 7000 off).
void StepTestEscort(float now, CampEvent* events, UInt32& n) {
	const int t = g_settings->testEscort;
	if (t < 0 || !CampInPlay(g_state, *g_settings, static_cast<UInt32>(t)) || g_testEscortStage >= 6) {
		return;
	}
	// After the arrival (EscortIntoCity): where they stand, then the player
	// taken into the city by the same gate and where they stand from him.
	if (g_testEscortStage >= 4) {
		const UInt32 i = static_cast<UInt32>(t);
		const int g = g_state.escort.gate;
		if (g < 0 || g >= static_cast<int>(kCityGateCount) || g_settings->testEscortBehind) {
			return;
		}
		const bool look = (g_testEscortStage == 4 && g_testClock >= kTestCampAfterSeconds + 45.0f) ||
		                  (g_testEscortStage == 5 && g_testClock >= kTestCampAfterSeconds + 60.0f);
		if (!look) {
			return;
		}
		const UInt32 player = LookupForm(kPlayerRefId);
		const UInt32 cityWorld = WorldOf(LookupForm(kCityGates[g].back));
		const Camp& c = g_state.camps[i];
		for (UInt32 slot = 0; slot < kCampSlots; ++slot) {
			if ((c.roster & ~c.lost & (1u << slot)) == 0) {
				continue;
			}
			const UInt32 ref = Person(i, slot);
			const UInt32 world = WorldOf(ref);
			float d = -1.0f;
			if (LooksLikeObject(ref) && world != 0 && world == WorldOf(player)) {
				const float dx = FloatAt(ref + kRefPositionOffset) - FloatAt(player + kRefPositionOffset);
				const float dy = FloatAt(ref + kRefPositionOffset + 4) - FloatAt(player + kRefPositionOffset + 4);
				d = std::sqrt(dx * dx + dy * dy);
			}
			g_log("Camps: test - after the arrival, slot %u (%08X) in worldspace %08X (the city's %08X), %.0f units from "
			      "the player (-1 another world), status %u",
			      slot, LooksLikeObject(ref) ? FormIdOf(ref) : 0, world != 0 ? FormIdOf(world) : 0,
			      cityWorld != 0 ? FormIdOf(cityWorld) : 0, d, static_cast<UInt32>(c.status));
		}
		if (g_testEscortStage == 4) {
			char line[96];
			std::snprintf(line, sizeof(line), "MoveTo %08X %.0f %.0f %.0f", kCityGates[g].back, kCityGates[g].inDx,
			              kCityGates[g].inDy - 300.0f, kCityGates[g].inDz + 20.0f);
			g_log("Camps: test - the player taken into the city (%s answered %d)", line, Say(player, line) ? 1 : 0);
		}
		++g_testEscortStage;
		return;
	}
	const UInt32 i = static_cast<UInt32>(t);
	const UInt32 player = LookupForm(kPlayerRefId);
	char line[96];
	if (g_testEscortStage == 0 && g_testClock >= kTestCampAfterSeconds) {
		g_testEscortStage = 1;
		CampTakeRoster(g_state.camps[i], *g_settings);
		std::snprintf(line, sizeof(line), "MoveTo %08X 0 -1500 200", MarkerId(i));
		g_log("Camps: test - TestEscort=%d, the player taken to the camp (%s answered %d)", t, line,
		      Say(player, line) ? 1 : 0);
	} else if (g_testEscortStage == 1 && g_testClock >= kTestCampAfterSeconds + 10.0f) {
		g_testEscortStage = 2;
		const UInt32 global = SetGlobal(kEscortAskGlobalLocalId, 1.0f);
		g_log("Camps: test - the ask set as the topic's yes would (global %08X; camp %02u %s, offer %.0f)",
		      global != 0 ? FormIdOf(global) : 0, i, CampOffersEscort(g_state, *g_settings, i) ? "asks" : "does not ask",
		      GlobalOf(kEscortOfferGlobalLocalId));
	} else if (g_testEscortStage == 2 && g_testClock >= kTestCampAfterSeconds + 30.0f) {
		g_testEscortStage = 3;
		const Camp& c = g_state.camps[i];
		for (UInt32 slot = 0; slot < kCampSlots; ++slot) {
			if ((c.roster & ~c.lost & (1u << slot)) == 0) {
				continue;
			}
			float x = 0.0f;
			float y = 0.0f;
			float px = 0.0f;
			float py = 0.0f;
			const UInt32 ref = Person(i, slot);
			if (OutdoorsAt(ref, x, y) && OutdoorsAt(player, px, py)) {
				bool high = false;
				const UInt32 package = ActorPackageNow(FormIdOf(ref), high);
				g_log("Camps: test - escorted slot %u (%08X) %.0f units from the player (20 s after the ask; set out "
				      "some 1500 off); running package %08X, high process %d, in combat %d",
				      slot, FormIdOf(ref), std::sqrt((x - px) * (x - px) + (y - py) * (y - py)), package, high ? 1 : 0,
				      ActorInCombat(FormIdOf(ref)) ? 1 : 0);
			}
		}
	} else if (g_testEscortStage == 3 && g_testClock >= kTestCampAfterSeconds + 32.0f) {
		g_testEscortStage = 4;
		if (g_settings->testEscortBehind) {
			std::snprintf(line, sizeof(line), "MoveTo %08X 0 -7000 300", MarkerId(i));
			g_log("Camps: test - the player 7000 units off (%s answered %d)", line, Say(player, line) ? 1 : 0);
			return;
		}
		const int g = g_state.escort.gate;
		if (g < 0 || g >= static_cast<int>(kCityGateCount)) {
			g_log("Camps: test - no escort under way to take to a gate");
			return;
		}
		std::snprintf(line, sizeof(line), "MoveTo %08X 0 -300 50", kCityGates[g].door);
		const bool moved = Say(player, line);
		std::snprintf(line, sizeof(line), "MoveTo %08X 0 -400 50", kCityGates[g].door);
		const UInt32 told = TellEscorted(i, line);
		g_log("Camps: test - the player and %u escorted taken to %s (%08X; the player's MoveTo answered %d)", told,
		      GateName(g), kCityGates[g].door, moved ? 1 : 0);
	}
	(void)now;
	(void)events;
	(void)n;
}

// The kits to the camps nearest the player.
void StepKits(const float* distance) {
	bool moved[kCampKits];
	int before[kCampKits];
	for (UInt32 k = 0; k < kCampKits; ++k) {
		before[k] = g_kitCamp[k];
	}
	ChooseCampKits(g_kitCamp, distance, g_settings->enabled ? CampActive(*g_settings) : 0, moved);
	for (UInt32 k = 0; k < kCampKits; ++k) {
		if (before[k] >= 0 && g_kitCamp[k] != before[k]) {
			FreeKit(k);
			g_log("Camps: kit %u off camp %02d %s", k, before[k], Name(static_cast<UInt32>(before[k])));
		}
		if (moved[k]) {
			SetUpKit(k, static_cast<UInt32>(g_kitCamp[k]));
		}
	}
}

}  // namespace

void ConfigureCamps(const CampSettings* settings, CampConsoleFn console, CampRollFn roll, CampDeadFn dead,
                    CampUnlistFn unlist, LogFn log) {
	g_settings = settings;
	g_console = console;
	g_roll = roll;
	g_dead = dead;
	g_unlist = unlist;
	g_log = log;
	g_log("Camps: [Survivors] Enabled=%d CampCount=%u CampSpacingUnits=%.0f PeoplePerCamp=%u WomenPercent=%u "
	      "Trader=%d RewardFame=%d FamePoints=%u RewardGold=%d Gold=%u AttackChancePercent=%u AttackWalkers=%u..%u "
	      "CampsAttackedAtOnce=%u RefugeeKillsPerHour=%.1f LossPercentPerHour=%u ReturnChancePercent=%u TestCamp=%d; "
	      "%u candidates",
	      settings->enabled ? 1 : 0, CampActive(*settings), settings->spacingUnits, settings->peoplePerCamp,
	      settings->womenPercent, settings->trader ? 1 : 0, settings->rewardFame ? 1 : 0, settings->famePoints,
	      settings->rewardGold ? 1 : 0, settings->gold, settings->attackChancePercent, settings->walkersMin,
	      settings->walkersMax, settings->campsAttackedAtOnce, settings->refugeeKillsPerHour,
	      settings->lossPercentPerHour, settings->returnChancePercent, settings->testCamp, kCampCandidateCount);
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

void GiveRefugeeReward(UInt32 alive, char* what, UInt32 size) {
	what[0] = '\0';
	if (g_settings == nullptr) {
		return;
	}
	GiveReward(g_settings->rewardFame ? g_settings->famePoints : 0, g_settings->rewardGold ? g_settings->gold : 0,
	           CampThings(g_settings->rewardThings, alive, 0, false), what, size);
}

bool PlayQuestSound() { return QuestSound(); }

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
	if (EspFormId(g_espBase, local) != refId || local < kCampKitFirstLocalId || local >= kCampLocalIdEnd) {
		return false;
	}
	return (local - kCampKitFirstLocalId) % kCampKitBlock < kCampSlots;
}

namespace {

// The player put 600 units above spot `stage` of TestCamp's camp.
void DropAt(UInt32 i, int stage) {
	const CampCandidate& place = Place(i);
	float dx = 0.0f;
	float dy = 0.0f;
	if (stage == 0) {
		CampTentSpot(place.facing, -1.0f, dx, dy);
	} else {
		const float road = place.facing * 3.14159265f / 180.0f;
		dx = std::sin(road) * kCampTentUnits;
		dy = std::cos(road) * kCampTentUnits;
	}
	const UInt32 player = LookupForm(kPlayerRefId);
	SayF(player, "SetPos x %.1f", place.x + dx);
	SayF(player, "SetPos y %.1f", place.y + dy);
	SayF(player, "SetPos z %.1f", place.z + 600.0f);
	g_drop.n = 0;
	g_drop.sampleClock = 0.0f;
	g_log("Camps: test - the player dropped onto camp %02u's %s (%.0f,%.0f from %.0f)", i,
	      stage == 0 ? "first tent" : "ground toward the road", place.x + dx, place.y + dy, place.z + 600.0f);
}

void StepTestDrop(float dt) {
	if (!g_testDone || g_drop.stage >= 2 || g_settings->testCamp < 0) {
		return;
	}
	const UInt32 i = static_cast<UInt32>(g_settings->testCamp);
	g_drop.clock += dt > 0.0f && dt < 1.0f ? dt : 0.0f;
	if (g_drop.stage < 0) {
		if (g_drop.clock >= kTestDropAfterSeconds) {
			g_drop.stage = 0;
			g_drop.clock = 0.0f;
			DropAt(i, 0);
		}
		return;
	}
	g_drop.sampleClock += dt;
	const UInt32 player = LookupForm(kPlayerRefId);
	if (g_drop.sampleClock >= kTestDropSample && g_drop.n < 128 && LooksLikeObject(player)) {
		g_drop.sampleClock = 0.0f;
		g_drop.z[g_drop.n++] = FloatAt(player + kRefPositionOffset + 8);
	}
	if (g_drop.clock < kTestDropSeconds) {
		return;
	}
	const float stop = CampFallStop(g_drop.z, g_drop.n);
	g_drop.stops[g_drop.stage] = stop;
	g_log("Camps: test - the fall onto the %s stopped at %.1f (%u heights %.1f..%.1f; the camp's fire at z %.1f)",
	      g_drop.stage == 0 ? "first tent" : "ground", stop, g_drop.n, g_drop.n > 0 ? g_drop.z[0] : 0.0f,
	      g_drop.n > 0 ? g_drop.z[g_drop.n - 1] : 0.0f, Place(i).z);
	g_drop.clock = 0.0f;
	if (++g_drop.stage == 1) {
		DropAt(i, 1);
	} else {
		g_log("Camps: test - the tent's stop %.1f units over the ground's (a tent's roof stands some 100 over its "
		      "base: solid there)",
		      g_drop.stops[0] - g_drop.stops[1]);
	}
}

}  // namespace

void StepCampsInGame(float dt) {
	if (g_settings == nullptr || g_console == nullptr || g_espBase == 0) {
		return;
	}
	StepTestDrop(dt);
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
			g_log("Camps: [Survivors] Enabled=0 - every camp's people, props, markers and walkers taken in");
			SetGlobal(kCampRumorGlobalLocalId, 0.0f);
			SetGlobal(kEscortOfferGlobalLocalId, 0.0f);
			for (UInt32 b = 0; b < kCampShownMost; ++b) {
				HideBank(b);
			}
			for (UInt32 k = 0; k < kCampKits; ++k) {
				g_kitCamp[k] = -1;
				FreeKit(k);
			}
			PlaceMarkers();
		}
		g_wasEnabled = false;
		return;
	}
	if (!g_wasEnabled) {
		g_wasEnabled = true;
		g_markersActive = 0;
	}
	RollPlaces();
	float distance[kCampMost];
	Distances(distance);
	EscortDistances(distance);
	StepKits(distance);
	CampEvent events[kEventsMost];
	UInt32 n = 0;
	for (UInt32 k = 0; k < g_endedCount; ++k) {
		CountEnded(g_ended[k], events, n);
	}
	g_endedCount = 0;
	g_testClock += kStepSeconds;
	SetGlobal(kCampRumorGlobalLocalId, 1.0f);
	StepTestEscort(now, events, n);
	n += StepEscortInGame(distance, now, events + n, kEventsMost - n);
	if (!g_testDone && g_testClock >= kTestCampAfterSeconds && g_settings->testCamp >= 0 &&
	    CampInPlay(g_state, *g_settings, static_cast<UInt32>(g_settings->testCamp))) {
		g_testDone = true;
		const UInt32 i = static_cast<UInt32>(g_settings->testCamp);
		CampTakeRoster(g_state.camps[i], *g_settings);
		ApplyPeople(i, false);
		n += BeginCampAttack(g_state.camps[i], i, *g_settings, now, Near(distance, i), g_roll(), events + n,
		                     kEventsMost - n);
		// The player taken to it, 1000 units off its fire (the places are
		// rolled: the harness cannot know where it is).
		char line[64];
		std::snprintf(line, sizeof(line), "MoveTo %08X 0 -1000 200", MarkerId(i));
		const bool moved = Say(LookupForm(kPlayerRefId), line);
		g_log("Camps: test - TestCamp=%d attacked 30 s after the load, the player taken there (%s answered %d)",
		      g_settings->testCamp, line, moved ? 1 : 0);
	}
	n += StepCampDays(g_state, *g_settings, now, g_roll, events + n, kEventsMost - n);
	for (UInt32 i = 0; i < CampActive(*g_settings); ++i) {
		Camp& c = g_state.camps[i];
		if (c.place == kCampNoPlace) {
			continue;
		}
		if (CampTakeRoster(c, *g_settings)) {
			ApplyPeople(i, false);
		}
		// The toll stands, and the attack counts as defended, while the camp's
		// fight is shown - near but not shown (a second camp attacked with
		// CampsAttackedAtOnce=1), it is rolled as away.
		n += StepCamp(c, i, *g_settings, now, Shown(i), g_roll, events + n, kEventsMost - n);
		// The dead among the people of a camp the player is at (only there
		// does the game fight).
		if (Near(distance, i) && c.status != CampStatus::Overrun && c.status != CampStatus::Left) {
			const int kit = KitOf(g_kitCamp, i);
			for (UInt32 slot = 0; kit >= 0 && slot < kCampSlots; ++slot) {
				const UInt16 bit = static_cast<UInt16>(1u << slot);
				if ((c.roster & ~c.lost & bit) == 0) {
					continue;
				}
				const UInt32 ref = Person(i, slot);
				if (LooksLikeObject(ref) && g_dead != nullptr && g_dead(ref)) {
					g_log("Camps: camp %02u %s - slot %u (%08X) dead, %u left", i, Name(i), slot, FormIdOf(ref),
					      CampAlive(c) - 1);
					g_kitPeople[kit] = static_cast<UInt16>(g_kitPeople[kit] & ~bit);
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
	// The kits start put away: the save may hold them anywhere; the next
	// step puts them at the camps near the player.
	// The escort's kit, though, stays where the save holds its people: on
	// the way, or by the city.
	const CampEscort& escort = g_state.escort;
	const bool escorted = escort.camp >= 0 && escort.camp < static_cast<int>(kCampMost) && escort.kit >= 0 &&
	                      (g_state.camps[escort.camp].status == CampStatus::Escort ||
	                       g_state.camps[escort.camp].status == CampStatus::Gone);
	for (UInt32 k = 0; k < kCampKits; ++k) {
		g_kitCamp[k] = -1;
		if (escorted && static_cast<int>(k) == escort.kit) {
			g_kitCamp[k] = escort.camp;
			g_kitPeople[k] = 0;
			continue;
		}
		FreeKit(k);
	}
	g_escortShown = escorted && g_state.camps[escort.camp].status == CampStatus::Gone;
	if (escorted) {
		ApplyPeople(static_cast<UInt32>(escort.camp), true);
		g_log("Camps: loaded - camp %02d's escort %s, kit %d kept where the save holds its people", escort.camp,
		      g_state.camps[escort.camp].status == CampStatus::Escort ? "on the way" : "in the city", escort.kit);
	} else if (escort.camp >= 0 && g_state.camps[escort.camp].status == CampStatus::Escort) {
		// On the way without a kit known: they go back to their camp.
		g_state.camps[escort.camp].status = CampStatus::Living;
		g_state.escort = CampEscort{};
	}
	UInt32 living = 0;
	UInt32 attacked = 0;
	UInt32 overrun = 0;
	UInt32 placed = 0;
	for (UInt32 i = 0; i < kCampMost; ++i) {
		Camp& c = g_state.camps[i];
		if (c.place != kCampNoPlace && c.place >= kCampCandidateCount) {
			c = Camp{};  // a place past the candidates (another version's): rolled anew
		}
		if (c.place == kCampNoPlace) {
			continue;
		}
		++placed;
		CampTakeRoster(c, *g_settings);
		living += c.status == CampStatus::Living ? 1u : 0u;
		attacked += c.status == CampStatus::Attacked ? 1u : 0u;
		overrun += c.status == CampStatus::Overrun ? 1u : 0u;
	}
	g_markersActive = 0;  // set again at the next step
	g_log("Camps: loaded - %u placed (%u living, %u attacked, %u overrun); the pool and the kits back in", placed,
	      living, attacked, overrun);
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
	for (UInt32 k = 0; k < kCampKits; ++k) {
		g_kitCamp[k] = -1;
		g_kitPeople[k] = 0;
	}
	g_endedCount = 0;
	g_wasEnabled = true;
	g_testDone = false;
	g_testClock = 0.0f;
	g_drop = TestDrop{};
	g_applied = false;
	g_markersActive = 0;
	g_logLines = 0;
	g_followClock = 0.0f;
	g_escortShown = false;
	g_testEscortStage = 0;
	g_probeSeconds = 0;
	ForgetResets();
}

}  // namespace walkers
