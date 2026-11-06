#include "Camps.h"

#include "CampCandidates.h"
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
constexpr float kRadToDeg = 57.2957795f;
constexpr UInt32 kEventsMost = 64;
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
bool g_applied = false;  // the rosters and markers set since the load
UInt32 g_markersActive = 0;  // CampActive when the markers were last set
// The kits: the camp each is at (-1 none), the slots it has enabled, and
// whether its props' 3D was rebuilt since the camp's cells came in.
int g_kitCamp[kCampKits] = {-1, -1, -1, -1};
UInt16 g_kitPeople[kCampKits] = {};
bool g_kitRebuilt[kCampKits] = {};
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

// Kit `kit`'s props and people put away: disabled where they stand.
void FreeKit(UInt32 kit) {
	for (UInt32 p = 0; p < kCampProps; ++p) {
		Say(Esp(KitPropLocalId(kit, p)), "Disable");
	}
	for (UInt32 slot = 0; slot < kCampSlots; ++slot) {
		Say(Esp(KitPersonLocalId(kit, slot)), "Disable");
	}
	g_kitPeople[kit] = 0;
	g_kitRebuilt[kit] = false;
}

// A prop onto its spot: SetPos x, y, z, SetAngle x and z, enabled, its 3D
// rebuilt - the collision follows only with Update3D (the probe of
// 2026-11-06, docs/walkers-spec.md 4.20.6).
void PutProp(UInt32 ref, const CampSpot& at) {
	SayF(ref, "SetPos x %.1f", at.x);
	SayF(ref, "SetPos y %.1f", at.y);
	SayF(ref, "SetPos z %.1f", at.z);
	SayF(ref, "SetAngle x %.2f", at.rx * kRadToDeg);
	SayF(ref, "SetAngle z %.2f", at.rz * kRadToDeg);
	Say(ref, "Enable");
	Say(ref, "Update3D");
}

// Kit `kit` put at camp `i`: its props onto the camp's spots, its people
// (the dead raised) onto theirs by the camp's map marker, then enabled as the
// roster says.
void SetUpKit(UInt32 kit, UInt32 i) {
	const CampCandidate& place = Place(i);
	for (UInt32 p = 0; p < kCampProps; ++p) {
		PutProp(Esp(KitPropLocalId(kit, p)), place.props[p]);
	}
	const UInt32 marker = EspFormId(g_espBase, CampMarkerLocalId(i));
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
	g_kitRebuilt[kit] = false;
	g_log("Camps: kit %u to camp %02u %s (%.0f,%.0f,%.0f; %u raised)", kit, i, Name(i), place.x, place.y, place.z,
	      raised);
	ApplyPeople(i, true);
}

// Every camp's map marker: those in play at their places and enabled, the
// others disabled.
void PlaceMarkers() {
	UInt32 shown = 0;
	for (UInt32 i = 0; i < kCampMost; ++i) {
		const UInt32 marker = Esp(CampMarkerLocalId(i));
		if (!LooksLikeObject(marker)) {
			continue;
		}
		if (!g_settings->enabled || !CampInPlay(g_state, *g_settings, i)) {
			Say(marker, "Disable");
			continue;
		}
		const CampCandidate& place = Place(i);
		SayF(marker, "SetPos x %.1f", place.x);
		SayF(marker, "SetPos y %.1f", place.y);
		SayF(marker, "SetPos z %.1f", place.z);
		Say(marker, "Enable");
		++shown;
	}
	g_markersActive = CampActive(*g_settings);
	g_log("Camps: %u map markers at their camps, the other %u disabled", shown, kCampMost - shown);
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
			CampWalkerSpot(place.facing, attack, k, dx, dy);
			const float dz = place.spotZ[(attack ? 0 : kCampWalkersAtOnce) + k] - place.z + kSpotLift;
			char line[96];
			std::snprintf(line, sizeof(line), "MoveTo %08X %.0f %.0f %.0f", EspFormId(g_espBase, CampMarkerLocalId(i)),
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
	std::snprintf(text, sizeof(text), "The refugees %s are safe - the walkers are beaten%s.", Name(i), what);
	Line(text);
	std::snprintf(line, sizeof(line), "PlaySound %08X", kQuestSound);
	const bool sound = Say(player, line);
	g_log("Camps: camp %02u %s defended - %s (the sound answered %d)", i, Name(i), text, sound ? 1 : 0);
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

// The kits to the camps nearest the player; a kit at a camp whose cells came
// in (the player near) has its props' 3D rebuilt once more.
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
		const int c = g_kitCamp[k];
		if (c >= 0 && !g_kitRebuilt[k] && Near(distance, static_cast<UInt32>(c))) {
			g_kitRebuilt[k] = true;
			for (UInt32 p = 0; p < kCampProps; ++p) {
				Say(Esp(KitPropLocalId(k, p)), "Update3D");
			}
			if (Logged()) {
				g_log("Camps: kit %u's props rebuilt at camp %02d %s (the player %.0f off)", k, c,
				      Name(static_cast<UInt32>(c)), distance[c]);
			}
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
			g_log("Camps: [Survivors] Enabled=0 - every camp's people, props, markers and walkers taken in");
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
	StepKits(distance);
	CampEvent events[kEventsMost];
	UInt32 n = 0;
	for (UInt32 k = 0; k < g_endedCount; ++k) {
		CountEnded(g_ended[k], events, n);
	}
	g_endedCount = 0;
	g_testClock += kStepSeconds;
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
		std::snprintf(line, sizeof(line), "MoveTo %08X 0 -1000 200", EspFormId(g_espBase, CampMarkerLocalId(i)));
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
		if (Near(distance, i) && c.status != CampStatus::Overrun) {
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
	for (UInt32 k = 0; k < kCampKits; ++k) {
		g_kitCamp[k] = -1;
		FreeKit(k);
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
		g_kitRebuilt[k] = false;
	}
	g_endedCount = 0;
	g_wasEnabled = true;
	g_testDone = false;
	g_testClock = 0.0f;
	g_applied = false;
	g_markersActive = 0;
	g_logLines = 0;
}

}  // namespace walkers
