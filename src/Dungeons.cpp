#include "Dungeons.h"

#include <windows.h>

#include <vector>

#include <cstdio>

#include "CampLogic.h"
#include "DungeonCandidates.h"
#include "DungeonKitLogic.h"
#include "Engine.h"
#include "News.h"
#include "Replace.h"
#include "RiseLogic.h"

namespace walkers {
using namespace engine;
namespace {

bool g_inPool[kDungeonCandidateCount] = {};
bool g_selected = false;
bool g_onlyChosen = false;
std::vector<UInt32> g_questCells;
std::vector<UInt32> g_questRefs;
DungeonSkips g_skips;

DungeonQuestNames Quests() {
	return {g_questCells.data(), static_cast<UInt32>(g_questCells.size()), g_questRefs.data(),
	        static_cast<UInt32>(g_questRefs.size())};
}

const char* ReasonOf(DungeonVerdict v) {
	switch (v) {
		case DungeonVerdict::Quest:
			return "a loaded quest names something in it, its door or its map marker";
		case DungeonVerdict::Skipped:
			return "[Dungeons] Skip names it";
		case DungeonVerdict::NotChosen:
			return "not one of the chosen ([Dungeons] OnlyChosen=1)";
		default:
			return "in";
	}
}

// The pool marked; with `each` every one out logged with its reason.
void Mark(bool each, LogFn log) {
	const DungeonPoolCount n =
	    MarkDungeonPool(kDungeonCandidates, kDungeonCandidateCount, Quests(), g_skips, g_onlyChosen, g_inPool);
	for (UInt32 i = 0; each && i < kDungeonCandidateCount; ++i) {
		const DungeonVerdict v = DungeonInPoolVerdict(kDungeonCandidates[i], Quests(), g_skips, g_onlyChosen);
		if (v != DungeonVerdict::In && v != DungeonVerdict::NotChosen) {
			log("Dungeons: %s (%s, cell %08X) out - %s", kDungeonCandidates[i].name,
			    DungeonKindName(kDungeonCandidates[i].kind), kDungeonCandidates[i].cells[0], ReasonOf(v));
		}
	}
	log("Dungeons: %u candidates, %u in the pool (OnlyChosen=%d), %u out by a loaded quest, %u by [Dungeons] Skip "
	    "(%u entries), %u not chosen",
	    kDungeonCandidateCount, n.in, g_onlyChosen ? 1 : 0, n.quest, n.skipped, g_skips.count, n.notChosen);
}

}  // namespace

void SelectDungeonPool(const char* iniPath, bool onlyChosen, LogFn log) {
	if (g_selected) {
		return;
	}
	g_selected = true;
	CollectQuestNames(log);
	g_questCells = QuestCellIds();
	g_questRefs = QuestRefIds();
	char text[kDungeonSkipMost * kDungeonNameMost] = {};
	if (iniPath != nullptr && iniPath[0] != '\0') {
		GetPrivateProfileStringA("Dungeons", "Skip", "", text, sizeof text, iniPath);
	}
	ParseDungeonSkips(text, g_skips);
	g_onlyChosen = onlyChosen;
	Mark(true, log);
}

void StepDungeonPool(bool onlyChosen, LogFn log) {
	if (!g_selected || onlyChosen == g_onlyChosen) {
		return;
	}
	g_onlyChosen = onlyChosen;
	Mark(false, log);
}

bool DungeonInPool(UInt32 index) { return index < kDungeonCandidateCount && g_inPool[index]; }

namespace {

constexpr UInt32 kPlayerRefId = 0x14;
constexpr UInt32 kFormFlagsOffset = 0x08;        // TESForm::flags
constexpr UInt32 kFormFlagDisabled = 0x00000800;  // xOBSE TESObjectREFR::kFlags_Disabled
constexpr UInt32 kRefPositionOffset = 0x2C;
constexpr UInt32 kRefParentCellOffset = 0x40;
constexpr UInt32 kRefExtraFirstOffset = 0x48;  // TESObjectREFR::baseExtraList (+0x44), its m_data
constexpr UInt8 kExtraFollower = 0x23;         // xOBSE GameBSExtraData.h; ExtraFollower's list at +0x0C
constexpr UInt32 kCellFlagsOffset = 0x24;
constexpr UInt32 kCellObjectListOffset = 0x48;  // TESObjectCELL::objectList {refr, next} (xOBSE GameForms.h)
constexpr UInt32 kCellWorldspaceOffset = 0x50;
constexpr UInt32 kVtblCharacter = 0x00A6FC9C;
constexpr UInt32 kVtblCreature = 0x00A710F4;
constexpr UInt32 kTamriel = 0x0000003C;
constexpr float kDungeonSeenUnits = 6000.0f;  // the camps' kCampSeenUnits
constexpr float kStepSeconds = 1.0f;
constexpr UInt32 kEventsMost = 32;
constexpr UInt32 kFollowersMost = 32;
constexpr short kSpotLift = 16;  // the people put this far above their path grid point

const DungeonSiteSettings* g_siteSettings = nullptr;
DungeonRollFn g_roll = nullptr;
DungeonConsoleFn g_console = nullptr;
DungeonDeadFn g_dead = nullptr;
DungeonUnlistFn g_unlist = nullptr;
DungeonRisenFn g_risen = nullptr;
UInt32 g_espBase = kNoEsp;
char g_testSite[kDungeonNameMost] = {};
DungeonState g_testState = DungeonState::Refuge;
DungeonSitesState g_sites;
float g_stepClock = 0.0f;

// The kit: the candidate it stands at (-1 none), the slots enabled there,
// those who died there, those still to be moved to their spots, and - after
// a load, when the save may hold them anywhere - every slot still to be told.
int g_kitCandidate = -1;
UInt16 g_kitShown = 0;
UInt16 g_kitDead = 0;
UInt16 g_kitToPlace = 0;
UInt16 g_kitToTell = 0;
bool g_kitAtOnce = true;  // after a load: all in one frame, not one a frame
int g_kitReportIn = -1;   // seconds until the kit's standing is logged, -1 none
bool g_kitBesieged = false;  // put there during a siege: on the far spots

// The pool of walkers: the candidate it is out at (-1 none), per slot out
// or lying ended, those ended since the last step, how many still to come
// in (one a frame), and the seconds no one may be placed (after a Disable:
// it takes hold after the frame).
int g_poolCandidate = -1;
bool g_poolOut[kDungeonPool] = {};
bool g_poolCorpse[kDungeonPool] = {};
UInt32 g_poolEnded[32] = {};
UInt32 g_poolEndedCount = 0;
UInt32 g_poolPending = 0;
UInt32 g_poolPlaced = 0;
bool g_poolSpread = false;  // a walker dungeon's first filling: over all the spots
int g_poolWait = 0;
bool g_poolUnknown = true;  // after a load: whatever the save left enabled goes back in
UInt32 g_poolLogsLeft = 120;

// The occupants taken out, and the player's cell as it was last frame.
DungeonOccupants g_occupants;
UInt32 g_lastCell = 0;
bool g_lastCellHeld = false;
bool g_occupantsFullLogged = false;

struct Roll {
	UInt32 operator()() { return g_roll != nullptr ? g_roll() : 0; }
};

const char* StateName(DungeonState s) {
	switch (s) {
		case DungeonState::Refuge:
			return "refuge";
		case DungeonState::Besieged:
			return "besieged";
		case DungeonState::ToClear:
			return "to clear";
		case DungeonState::WalkerDungeon:
			return "walker dungeon";
		default:
			return "free";
	}
}

const char* EventName(DungeonEventKind k) {
	switch (k) {
		case DungeonEventKind::Rolled:
			return "rolled";
		case DungeonEventKind::Attacked:
			return "attacked";
		case DungeonEventKind::PersonLost:
			return "one of its people lost";
		case DungeonEventKind::Held:
			return "held";
		case DungeonEventKind::Defended:
			return "defended";
		case DungeonEventKind::Overrun:
			return "overrun";
		case DungeonEventKind::Taken:
			return "taken";
		default:
			return "given up";
	}
}

// Each event of one site, `d` as it stands after them (or, given up, as it
// was before).
void LogEvents(const DungeonEvent* ev, UInt32 n, const DungeonSite& d, LogFn log) {
	const bool known = d.candidate < kDungeonCandidateCount;
	for (UInt32 k = 0; k < n && k < kEventsMost; ++k) {
		log("Dungeons: site %u %s (%s) %s - now %s, %u people, %u waiting, %u walkers", ev[k].site,
		    known ? kDungeonCandidates[d.candidate].name : "?",
		    known ? DungeonKindName(kDungeonCandidates[d.candidate].kind) : "-", EventName(ev[k].kind),
		    StateName(ev[k].kind == DungeonEventKind::Left ? DungeonState::Free : d.state), d.people, d.waiting,
		    d.walkersLeft);
	}
}

// The player and candidate `c`: 0 in one of its cells, his distance from its
// door outside when in Tamriel within kDungeonSeenUnits of it, else -1.
float Closeness(const DungeonCandidate& c, UInt32 player) {
	const UInt32 cell = LooksLikeObject(player) ? Read(player + kRefParentCellOffset) : 0;
	if (!LooksLikeObject(cell)) {
		return -1.0f;
	}
	const UInt32 id = FormIdOf(cell);
	for (UInt32 k = 0; k < c.cellCount && k < kDungeonCellsMost; ++k) {
		if (c.cells[k] == id) {
			return 0.0f;
		}
	}
	if ((Read8(cell + kCellFlagsOffset) & 1) != 0) {
		return -1.0f;
	}
	const UInt32 world = Read(cell + kCellWorldspaceOffset);
	if (!LooksLikeObject(world) || FormIdOf(world) != kTamriel) {
		return -1.0f;
	}
	const float dx = FloatAt(player + kRefPositionOffset) - c.x;
	const float dy = FloatAt(player + kRefPositionOffset + 4) - c.y;
	const float d2 = dx * dx + dy * dy;
	// Never 0 outdoors: 0 means inside.
	return d2 <= kDungeonSeenUnits * kDungeonSeenUnits ? (d2 > 1.0f ? std::sqrt(d2) : 1.0f) : -1.0f;
}

bool Say(UInt32 ref, const char* line) { return LooksLikeObject(ref) && g_console != nullptr && g_console(ref, line); }

UInt32 KitRef(UInt32 slot) {
	return g_espBase != kNoEsp ? LookupForm(EspFormId(g_espBase, kDungeonKitFirstLocalId + slot)) : 0;
}

bool Disabled(UInt32 ref) { return (Read(ref + kFormFlagsOffset) & kFormFlagDisabled) != 0; }

// A kit person dead raised and dressed again, and taken off the rise's list
// (Camps.cpp Raise: its things went onto its walker).
bool Raise(UInt32 ref, UInt32 slot) {
	if (!LooksLikeObject(ref) || g_dead == nullptr || !g_dead(ref)) {
		return false;
	}
	if (g_unlist != nullptr) {
		g_unlist(FormIdOf(ref));
	}
	Say(ref, "SetDestroyed 0");
	Say(ref, "Resurrect");
	Say(ref, "RemoveAllItems");
	const UInt32* outfit = kCampOutfits[CampBaseIndex(0, slot)];
	for (UInt32 k = 0; k < 4; ++k) {
		char line[48];
		std::snprintf(line, sizeof(line), "AddItem %08X 1", outfit[k]);
		Say(ref, line);
		std::snprintf(line, sizeof(line), "EquipItem %08X", outfit[k]);
		Say(ref, line);
	}
	return true;
}

// The slots the kit should show now.
UInt16 KitWantedNow() {
	const int site = g_kitCandidate >= 0 ? SiteOfCandidate(g_sites, static_cast<UInt32>(g_kitCandidate)) : -1;
	if (site < 0) {
		return 0;
	}
	const DungeonSite& d = g_sites.sites[site];
	return DungeonHeld(d.state) ? KitWanted(d.people, KitTrader(d.state), g_kitDead) : 0;
}

// One slot brought to what it should be: moved to its spot (a dead one
// raised first) and enabled, or disabled.
void TellSlot(UInt32 slot) {
	const UInt16 bit = static_cast<UInt16>(1u << slot);
	const bool want = (KitWantedNow() & bit) != 0;
	const UInt32 ref = KitRef(slot);
	const bool place = (g_kitToPlace & bit) != 0;
	g_kitToPlace = static_cast<UInt16>(g_kitToPlace & ~bit);
	g_kitToTell = static_cast<UInt16>(g_kitToTell & ~bit);
	if (!LooksLikeObject(ref)) {
		return;
	}
	if (!want) {
		// A dead one lies where it fell (the rise's).
		if (!Disabled(ref) && (g_kitDead & bit) == 0) {
			Say(ref, "Disable");
		}
		g_kitShown = static_cast<UInt16>(g_kitShown & ~bit);
		return;
	}
	// One enabled already (the save's, or shown at the place before) is only
	// moved: a console Disable takes hold after the frame (measured 2026-11-13,
	// dungeon-refuge: the flag still clear right after it, so no Enable
	// followed and the kit stayed away after a load).
	const bool wasDisabled = Disabled(ref);
	if (place && g_kitCandidate >= 0) {
		const DungeonCandidate& c = kDungeonCandidates[g_kitCandidate];
		Raise(ref, slot);
		const DungeonSpot at = KitSpot(c, slot, g_kitBesieged);
		char line[64];
		std::snprintf(line, sizeof(line), "MoveTo %08X %d %d %d", c.insideDoor, at.dx, at.dy, at.dz + kSpotLift);
		Say(ref, line);
	}
	if (wasDisabled) {
		Say(ref, "Enable");
	}
	if (wasDisabled || place) {
		// Their camp life's packages (sleep, wander) near where they now stand.
		Say(ref, "EvaluatePackage");
	}
	g_kitShown = static_cast<UInt16>(g_kitShown | bit);
}

// Every frame: one slot of those to be told (all of them right after a load).
void StepKitSlots() {
	UInt16 todo = static_cast<UInt16>(g_kitToTell | g_kitToPlace);
	while (todo != 0) {
		UInt32 slot = 0;
		while ((todo & (1u << slot)) == 0) {
			++slot;
		}
		TellSlot(slot);
		todo = static_cast<UInt16>(g_kitToTell | g_kitToPlace);
		if (!g_kitAtOnce) {
			return;
		}
	}
	g_kitAtOnce = false;
}

// Once a second: the kit to the held site the player is near, its dead
// counted off, its slots as the site's people say.
void StepKit(const float* closeness, LogFn log) {
	const int current = g_kitCandidate >= 0 ? SiteOfCandidate(g_sites, static_cast<UInt32>(g_kitCandidate)) : -1;
	const bool enabled = g_siteSettings != nullptr && g_siteSettings->enabled && g_espBase != kNoEsp;
	const int want = KitSite(g_sites, closeness, enabled, current);
	const int wantCandidate = want >= 0 ? static_cast<int>(g_sites.sites[want].candidate) : -1;
	if (wantCandidate != g_kitCandidate) {
		if (g_kitCandidate >= 0) {
			log("Dungeons: the kit off %s (%u of its people were out)", kDungeonCandidates[g_kitCandidate].name,
			    CampBits(g_kitShown));
		}
		// Those out are put away (told one a frame); the new place's are moved first.
		g_kitToTell = static_cast<UInt16>(g_kitToTell | g_kitShown);
		g_kitCandidate = wantCandidate;
		g_kitDead = 0;
		g_kitToPlace = 0;
		if (wantCandidate >= 0) {
			const DungeonSite& d = g_sites.sites[want];
			// Those of the kit lying dead in this place (a save loaded, the
			// kit back after a while) stay dead - the living stand in for
			// the site's people; only with too few living are they raised.
			UInt16 deadHere = 0;
			for (UInt32 slot = 0; slot < kDungeonKitSlots; ++slot) {
				const UInt32 ref = KitRef(slot);
				const UInt32 at = LooksLikeObject(ref) ? Read(ref + kRefParentCellOffset) : 0;
				if (LooksLikeObject(at) && g_dead != nullptr && g_dead(ref) &&
				    CandidateOfCell(kDungeonCandidates, kDungeonCandidateCount, FormIdOf(at)) == wantCandidate) {
					deadHere = static_cast<UInt16>(deadHere | (1u << slot));
				}
			}
			if (CampBits(KitWanted(kDungeonKitPeopleMost, false, deadHere)) >= d.people) {
				g_kitDead = deadHere;
			}
			g_kitToPlace = KitWanted(d.people, KitTrader(d.state), g_kitDead);
			g_kitBesieged = d.state == DungeonState::Besieged;
			g_kitReportIn = 3;
			log("Dungeons: the kit to site %d %s (%s; %s, %u people%s) - by its door inside %08X, %u spots", want,
			    kDungeonCandidates[wantCandidate].name, DungeonKindName(kDungeonCandidates[wantCandidate].kind),
			    StateName(d.state), d.people, KitTrader(d.state) && d.people > 0 ? " and the trader" : "",
			    kDungeonCandidates[wantCandidate].insideDoor, kDungeonCandidates[wantCandidate].spotCount);
		}
	}
	if (g_kitCandidate < 0 || g_kitToPlace != 0) {
		return;
	}
	const int site = SiteOfCandidate(g_sites, static_cast<UInt32>(g_kitCandidate));
	// The dead counted off: the last overruns the place.
	for (UInt32 slot = 0; site >= 0 && slot < kDungeonKitSlots; ++slot) {
		const UInt16 bit = static_cast<UInt16>(1u << slot);
		const UInt32 ref = KitRef(slot);
		if ((g_kitShown & bit) == 0 || !LooksLikeObject(ref) || g_dead == nullptr || !g_dead(ref)) {
			continue;
		}
		g_kitDead = static_cast<UInt16>(g_kitDead | bit);
		g_kitShown = static_cast<UInt16>(g_kitShown & ~bit);
		DungeonSite& d = g_sites.sites[site];
		DungeonEvent ev[4];
		const UInt32 n = slot < kDungeonKitPeopleMost ? DungeonPersonDied(d, static_cast<UInt32>(site), ev, 4) : 0;
		log("Dungeons: %s - %s %08X died; %u people left, now %s", kDungeonCandidates[g_kitCandidate].name,
		    slot < kDungeonKitPeopleMost ? "one of its people" : "its trader", FormIdOf(ref), d.people,
		    StateName(d.state));
		LogEvents(ev, n, d, log);
	}
	// What changed since (a siege's toll, the state): told one a frame.
	g_kitToTell = static_cast<UInt16>(g_kitToTell | (KitWantedNow() ^ g_kitShown));
	// Three seconds after it was put there: how the kit stands, for the log.
	if (g_kitReportIn > 0 && --g_kitReportIn == 0) {
		g_kitReportIn = -1;
		UInt32 shown = 0;
		UInt32 enabled = 0;
		UInt32 with3D = 0;
		UInt32 inFirstCell = 0;
		// How far each stands from its spot, and how far beneath it (one
		// fallen through the floor shows here).
		const UInt32 door = LookupForm(kDungeonCandidates[g_kitCandidate].insideDoor);
		float farthest = 0.0f;
		float lowest = 0.0f;
		for (UInt32 slot = 0; slot < kDungeonKitSlots; ++slot) {
			const UInt32 ref = KitRef(slot);
			if ((g_kitShown & (1u << slot)) == 0 || !LooksLikeObject(ref)) {
				continue;
			}
			if (LooksLikeObject(door)) {
				const DungeonSpot at = KitSpot(kDungeonCandidates[g_kitCandidate], slot, g_kitBesieged);
				const float dx = FloatAt(ref + kRefPositionOffset) - FloatAt(door + kRefPositionOffset) - at.dx;
				const float dy = FloatAt(ref + kRefPositionOffset + 4) - FloatAt(door + kRefPositionOffset + 4) - at.dy;
				const float dz = FloatAt(ref + kRefPositionOffset + 8) - FloatAt(door + kRefPositionOffset + 8) - at.dz;
				const float flat = std::sqrt(dx * dx + dy * dy);
				farthest = flat > farthest ? flat : farthest;
				lowest = dz < lowest ? dz : lowest;
			}
			++shown;
			enabled += Disabled(ref) ? 0u : 1u;
			with3D += LooksLikeObject(Read(ref + 0x3C)) ? 1u : 0u;  // TESObjectREFR::niNode
			const UInt32 at = Read(ref + kRefParentCellOffset);
			inFirstCell += LooksLikeObject(at) && FormIdOf(at) == kDungeonCandidates[g_kitCandidate].cells[0] ? 1u : 0u;
		}
		log("Dungeons: the kit at %s stands - %u shown, %u enabled, %u in its first cell %08X, %u with their 3D; "
		    "the farthest %.0f units from its spot, the lowest %.0f beneath it",
		    kDungeonCandidates[g_kitCandidate].name, shown, enabled, inFirstCell,
		    kDungeonCandidates[g_kitCandidate].cells[0], with3D, farthest, -lowest);
	}
}

UInt32 PoolRef(UInt32 slot) {
	return g_espBase != kNoEsp ? LookupForm(EspFormId(g_espBase, kDungeonPoolFirstLocalId + slot)) : 0;
}

// The pool back in: those out and those lying disabled; nobody placed for
// two seconds after (a Disable takes hold after the frame).
void HidePool(LogFn log) {
	UInt32 hidden = 0;
	for (UInt32 slot = 0; slot < kDungeonPool; ++slot) {
		const UInt32 ref = PoolRef(slot);
		if (LooksLikeObject(ref) && (g_poolOut[slot] || g_poolCorpse[slot] || g_poolUnknown) && !Disabled(ref)) {
			hidden += Say(ref, "Disable") ? 1u : 0u;
		}
		g_poolOut[slot] = false;
		g_poolCorpse[slot] = false;
	}
	if (g_poolCandidate >= 0 || hidden > 0) {
		log("Dungeons: the walkers %s%s back in the pool (%u)", g_poolCandidate >= 0 ? "at " : "",
		    g_poolCandidate >= 0 ? kDungeonCandidates[g_poolCandidate].name : "the save left out", hidden);
	}
	g_poolCandidate = -1;
	g_poolPending = 0;
	g_poolUnknown = false;
	g_poolWait = 2;
}

// One walker of the pool into the place: a slot never used first, else one
// lying ended raised; by the door (a siege's, and every later one) or, a
// walker dungeon's first filling, spread over its spots.
void PlacePoolWalker(LogFn log) {
	if (g_poolCandidate < 0 || g_poolPending == 0) {
		g_poolPending = 0;
		return;
	}
	--g_poolPending;
	for (UInt32 pass = 0; pass < 2; ++pass) {
		for (UInt32 slot = 0; slot < kDungeonPool; ++slot) {
			if (g_poolOut[slot] || (pass == 0 && g_poolCorpse[slot]) || (pass == 1 && !g_poolCorpse[slot])) {
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
			const DungeonCandidate& c = kDungeonCandidates[g_poolCandidate];
			const DungeonSpot at = WalkerSpot(c, g_poolPlaced++, !g_poolSpread);
			char line[64];
			std::snprintf(line, sizeof(line), "MoveTo %08X %d %d %d", c.insideDoor, at.dx, at.dy, at.dz + kSpotLift);
			const bool moved = Say(ref, line);
			if (Disabled(ref)) {
				Say(ref, "Enable");
			}
			g_poolOut[slot] = true;
			g_poolCorpse[slot] = false;
			if (g_poolLogsLeft > 0) {
				--g_poolLogsLeft;
				log("Dungeons: walker %08X into %s (slot %u%s, %s; %s answered %d)", FormIdOf(ref), c.name, slot,
				    raised ? ", raised" : "", g_poolSpread ? "among its spots" : "by the door", line, moved ? 1 : 0);
			}
			return;
		}
	}
	g_poolPending = 0;  // no slot free
}

// Once a second: the ended counted off, the pool to the siege or walker
// dungeon the player is near, as many in it as the site wants.
void StepPool(const float* closeness, LogFn log) {
	Roll roll;
	const int siteBefore = g_poolCandidate >= 0 ? SiteOfCandidate(g_sites, static_cast<UInt32>(g_poolCandidate)) : -1;
	for (UInt32 k = 0; k < g_poolEndedCount; ++k) {
		const UInt32 local = g_poolEnded[k] & kEspLocalMask;
		const UInt32 slot = local - kDungeonPoolFirstLocalId;
		if (slot >= kDungeonPool || !g_poolOut[slot]) {
			continue;
		}
		g_poolOut[slot] = false;
		g_poolCorpse[slot] = true;
		if (siteBefore < 0) {
			continue;
		}
		DungeonSite& d = g_sites.sites[siteBefore];
		DungeonEvent ev[4];
		const UInt32 n = DungeonWalkerEnded(d, static_cast<UInt32>(siteBefore), *g_siteSettings, roll, ev, 4);
		log("Dungeons: %s - walker %08X ended; %u walkers left, now %s", kDungeonCandidates[g_poolCandidate].name,
		    g_poolEnded[k], d.walkersLeft, StateName(d.state));
		LogEvents(ev, n, d, log);
	}
	g_poolEndedCount = 0;
	if (g_poolUnknown) {
		HidePool(log);
	}
	if (g_poolWait > 0) {
		--g_poolWait;
	}
	const bool enabled = g_siteSettings->enabled && g_espBase != kNoEsp;
	const int current = g_poolCandidate >= 0 ? SiteOfCandidate(g_sites, static_cast<UInt32>(g_poolCandidate)) : -1;
	const int want = WalkerSite(g_sites, closeness, enabled, current);
	const int wantCandidate = want >= 0 ? static_cast<int>(g_sites.sites[want].candidate) : -1;
	if (wantCandidate != g_poolCandidate) {
		HidePool(log);
		g_poolCandidate = wantCandidate;
		g_poolPlaced = 0;
		g_poolSpread = want >= 0 && g_sites.sites[want].state == DungeonState::WalkerDungeon;
		if (want >= 0) {
			const DungeonSite& d = g_sites.sites[want];
			log("Dungeons: the pool to site %d %s (%s; %u walkers, %u at once)", want,
			    kDungeonCandidates[wantCandidate].name, StateName(d.state), d.walkersLeft,
			    DungeonWalkersOut(d, g_siteSettings->walkersAtOnce));
		}
	}
	if (want < 0) {
		return;
	}
	UInt32 out = 0;
	for (UInt32 slot = 0; slot < kDungeonPool; ++slot) {
		out += g_poolOut[slot] ? 1u : 0u;
	}
	const UInt32 wanted = DungeonWalkersOut(g_sites.sites[want], g_siteSettings->walkersAtOnce);
	g_poolPending = wanted > out ? wanted - out : 0;
	if (g_poolPending == 0) {
		g_poolSpread = false;  // filled: whoever comes later comes in by the door
	}
}

// The player's followers (ExtraFollower on him: companions, summons - the
// ones the engine takes through a door with him).
UInt32 Followers(UInt32 player, UInt32* out, UInt32 most) {
	UInt32 n = 0;
	UInt32 visited = 0;
	for (UInt32 extra = Read(player + kRefExtraFirstOffset); LooksLikeObject(extra) && visited < 64;
	     extra = Read(extra + 8), ++visited) {
		if (Read8(extra + 4) != kExtraFollower) {
			continue;
		}
		UInt32 walked = 0;
		for (UInt32 node = Read(extra + 0x0C); LooksLikeObject(node) && n < most && walked < 64;
		     node = Read(node + 4), ++walked) {
			if (LooksLikeObject(Read(node))) {
				out[n++] = Read(node);
			}
		}
	}
	return n;
}

// A held place's cell the player is in: its occupants taken out, each noted.
void TakeOut(UInt32 cell, UInt32 cellId, UInt32 player, LogFn log) {
	UInt32 followers[kFollowersMost];
	const UInt32 followerCount = Followers(player, followers, kFollowersMost);
	UInt32 out = 0;
	UInt32 spared = 0;
	UInt32 seen = 0;
	for (UInt32 node = cell + kCellObjectListOffset; LooksLikeObject(node) && seen < 16384; node = Read(node + 4), ++seen) {
		const UInt32 ref = Read(node);
		if (!LooksLikeObject(ref)) {
			continue;
		}
		const UInt32 vtable = Read(ref);
		if (vtable != kVtblCharacter && vtable != kVtblCreature) {
			continue;
		}
		OccupantFacts f;
		f.actor = true;
		f.player = ref == player;
		f.ours = IsDungeonPerson(FormIdOf(ref)) || IsDungeonWalker(FormIdOf(ref));
		f.disabled = Disabled(ref);
		f.dead = g_dead != nullptr && g_dead(ref);
		f.risen = g_risen != nullptr && g_risen(FormIdOf(ref));
		for (UInt32 k = 0; k < followerCount; ++k) {
			f.follower = f.follower || followers[k] == ref;
		}
		if (!TakesOut(f)) {
			spared += f.follower ? 1u : 0u;
			continue;
		}
		if (g_occupants.count >= kDungeonOccupantsMost) {
			if (!g_occupantsFullLogged) {
				g_occupantsFullLogged = true;
				log("Dungeons: %u occupants are out already - no more can be noted; the rest stay", g_occupants.count);
			}
			break;
		}
		AddOccupant(g_occupants, cellId, FormIdOf(ref));
		out += Say(ref, "Disable") ? 1u : 0u;
	}
	const int c = CandidateOfCell(kDungeonCandidates, kDungeonCandidateCount, cellId);
	log("Dungeons: %s (cell %08X, %u references) is held - %u occupants taken out, %u followers of the player left; "
	    "%u out in all",
	    c >= 0 ? kDungeonCandidates[c].name : "?", cellId, seen, out, spared, g_occupants.count);
}

// A cell of a place held no more: those taken out of it put back - all at
// once when the player comes in (the door's loading hides it), else four a
// frame (an Enable is some 240 us: 43 at once were 10 ms, 2026-11-13).
UInt32 g_putBackCell = 0;
UInt32 g_putBackDone = 0;
UInt32 g_putBackOf = 0;

void PutBackSome(UInt32 most, LogFn log) {
	static UInt32 refs[kDungeonOccupantsMost];
	const UInt32 n = TakeOccupantsOf(g_occupants, g_putBackCell, refs, most);
	for (UInt32 i = 0; i < n; ++i) {
		const UInt32 ref = LookupForm(refs[i]);
		if (LooksLikeObject(ref) && Disabled(ref)) {
			g_putBackDone += Say(ref, "Enable") ? 1u : 0u;
		}
	}
	if (OccupantsOf(g_occupants, g_putBackCell) > 0) {
		return;
	}
	const int c = CandidateOfCell(kDungeonCandidates, kDungeonCandidateCount, g_putBackCell);
	log("Dungeons: %s (cell %08X) is held no more - %u of %u occupants put back (the others gone from the game)",
	    c >= 0 ? kDungeonCandidates[c].name : "?", g_putBackCell, g_putBackDone, g_putBackOf);
	g_putBackCell = 0;
}

void PutBack(UInt32 cellId, bool atOnce, LogFn log) {
	g_putBackCell = cellId;
	g_putBackDone = 0;
	g_putBackOf = OccupantsOf(g_occupants, cellId);
	if (atOnce) {
		PutBackSome(kDungeonOccupantsMost, log);
	}
}

// The player's cell looked at: a held place's occupants out, a place's held
// no more back in.
void SyncCell(UInt32 cell, UInt32 player, bool atOnce, LogFn log) {
	const UInt32 cellId = FormIdOf(cell);
	const bool enabled = g_siteSettings != nullptr && g_siteSettings->enabled;
	g_lastCellHeld = CellHeld(g_sites, kDungeonCandidates, kDungeonCandidateCount, cellId, enabled);
	if (g_lastCellHeld) {
		g_putBackCell = 0;
		TakeOut(cell, cellId, player, log);
	} else if (OccupantsOf(g_occupants, cellId) > 0) {
		PutBack(cellId, atOnce, log);
	}
}

}  // namespace

void ConfigureDungeons(const DungeonSiteSettings* settings, DungeonRollFn roll, DungeonConsoleFn console,
                       DungeonDeadFn dead, DungeonUnlistFn unlist, DungeonRisenFn risen, const char* testSite,
                       UInt32 testState) {
	g_siteSettings = settings;
	g_roll = roll;
	g_console = console;
	g_dead = dead;
	g_unlist = unlist;
	g_risen = risen;
	std::snprintf(g_testSite, sizeof(g_testSite), "%s", testSite != nullptr ? testSite : "");
	g_testState = testState >= 1 && testState <= 4 ? static_cast<DungeonState>(testState) : DungeonState::Refuge;
}

void ConfigureDungeonsEsp(UInt32 espBase) { g_espBase = espBase; }

bool IsDungeonPerson(UInt32 refId) {
	if (g_espBase == kNoEsp) {
		return false;
	}
	const UInt32 local = refId & kEspLocalMask;
	return EspFormId(g_espBase, local) == refId && local >= kDungeonKitFirstLocalId &&
	       local < kDungeonKitFirstLocalId + kDungeonKitSlots;
}

bool IsDungeonWalker(UInt32 refId) {
	if (g_espBase == kNoEsp) {
		return false;
	}
	const UInt32 local = refId & kEspLocalMask;
	return EspFormId(g_espBase, local) == refId && local >= kDungeonPoolFirstLocalId &&
	       local < kDungeonPoolFirstLocalId + kDungeonPool;
}

bool NoteDungeonWalkerEnded(UInt32 refId) {
	if (!IsDungeonWalker(refId) || !g_poolOut[(refId & kEspLocalMask) - kDungeonPoolFirstLocalId]) {
		return false;
	}
	if (g_poolEndedCount < 32) {
		g_poolEnded[g_poolEndedCount++] = refId;
	}
	return true;
}

DungeonSitesState& CurrentDungeons() { return g_sites; }
DungeonOccupants& CurrentDungeonOccupants() { return g_occupants; }

UInt32 kDungeonCandidateCountForSave() { return kDungeonCandidateCount; }

void ResetDungeonsInGame() {
	g_sites = DungeonSitesState{};
	g_occupants.count = 0;
	g_occupantsFullLogged = false;
	// The first step at once; the kit wherever the save holds it: every slot
	// told, in one frame.
	g_stepClock = kStepSeconds;
	g_kitCandidate = -1;
	g_kitShown = 0;
	g_kitDead = 0;
	g_kitToPlace = 0;
	g_kitToTell = static_cast<UInt16>((1u << kDungeonKitSlots) - 1u);
	g_kitAtOnce = true;
	g_kitReportIn = -1;
	g_poolCandidate = -1;
	for (UInt32 slot = 0; slot < kDungeonPool; ++slot) {
		g_poolOut[slot] = false;
		g_poolCorpse[slot] = false;
	}
	g_poolEndedCount = 0;
	g_poolPending = 0;
	g_poolWait = 0;
	g_poolUnknown = true;
	g_lastCell = 0;
	g_lastCellHeld = false;
	g_putBackCell = 0;
}

void StepDungeonsInGame(float dt, LogFn log) {
	if (!g_selected || g_siteSettings == nullptr) {
		return;
	}
	// Every frame: the player's cell against the last frame's - a new one is
	// read through once; and one kit slot told.
	const UInt32 player = LookupForm(kPlayerRefId);
	const UInt32 cell = LooksLikeObject(player) ? Read(player + kRefParentCellOffset) : 0;
	if (cell != g_lastCell) {
		g_lastCell = cell;
		g_lastCellHeld = false;
		g_putBackCell = 0;  // what was left of another cell's waits for the next visit
		if (LooksLikeObject(cell) && GameDaysNow() > 0.0f) {
			SyncCell(cell, player, true, log);
		}
	}
	if (g_putBackCell != 0) {
		PutBackSome(4, log);
	}
	if (g_poolPending != 0 && g_poolWait <= 0) {
		PlacePoolWalker(log);
	}
	if ((g_kitToTell | g_kitToPlace) != 0 && !g_kitAtOnce) {
		StepKitSlots();
	}
	g_stepClock += dt;
	if (g_stepClock < kStepSeconds) {
		return;
	}
	g_stepClock = 0.0f;
	const float now = GameDaysNow();
	if (!(now > 0.0f) || !LooksLikeObject(player)) {
		return;
	}
	if (g_siteSettings->enabled) {
		// [Dungeons] TestSite: that candidate a refuge, whatever the save held.
		if (g_testSite[0] != '\0') {
			int named = -1;
			for (UInt32 c = 0; c < kDungeonCandidateCount && named < 0; ++c) {
				named = g_inPool[c] && SameName(kDungeonCandidates[c].name, g_testSite) ? static_cast<int>(c) : -1;
			}
			if (named >= 0 && SiteOfCandidate(g_sites, static_cast<UInt32>(named)) < 0) {
				g_sites = DungeonSitesState{};
				const bool forced = ForceSite(g_sites, kDungeonCandidates, kDungeonCandidateCount, g_inPool, g_testSite,
				                              g_testState, *g_siteSettings, now);
				log("Dungeons: test - TestSite=%s made the first site: %s, %u people, %u waiting, %u walkers (%d)",
				    g_testSite, StateName(g_sites.sites[0].state), g_sites.sites[0].people, g_sites.sites[0].waiting,
				    g_sites.sites[0].walkersLeft, forced ? 1 : 0);
			}
		}
		Roll roll;
		DungeonEvent ev[kEventsMost];
		for (UInt32 i = 0; i < g_sites.count; ++i) {
			DungeonSite& d = g_sites.sites[i];
			const bool close =
			    d.candidate < kDungeonCandidateCount && Closeness(kDungeonCandidates[d.candidate], player) >= 0.0f;
			const DungeonSite before = d;
			const UInt32 n = StepDungeonSite(d, i, *g_siteSettings, now, close, roll, ev, kEventsMost);
			// A site given up is logged with its name from before.
			LogEvents(ev, n, d.state == DungeonState::Free ? before : d, log);
		}
		DropFreeSites(g_sites);
		const UInt32 n = FillDungeons(g_sites, *g_siteSettings, kDungeonCandidates, kDungeonCandidateCount, g_inPool,
		                              now, roll, ev, kEventsMost);
		for (UInt32 k = 0; k < n && k < kEventsMost; ++k) {
			LogEvents(ev + k, 1, g_sites.sites[ev[k].site], log);
		}
	}
	float closeness[kDungeonActiveMost];
	for (UInt32 i = 0; i < g_sites.count; ++i) {
		const UInt16 c = g_sites.sites[i].candidate;
		closeness[i] = c < kDungeonCandidateCount ? Closeness(kDungeonCandidates[c], player) : -1.0f;
	}
	// The pool first: a siege's last walker ended makes the place a refuge
	// again before the kit is looked at.
	StepPool(closeness, log);
	StepKit(closeness, log);
	if (g_kitAtOnce) {
		StepKitSlots();
	}
	// The player's cell held now and not before, or the other way (the place
	// overrun, given up, [Dungeons] Enabled changed): looked at again.
	if (LooksLikeObject(cell)) {
		const bool held = CellHeld(g_sites, kDungeonCandidates, kDungeonCandidateCount, FormIdOf(cell),
		                           g_siteSettings->enabled);
		if (held != g_lastCellHeld) {
			SyncCell(cell, player, false, log);
		}
	}
}

}  // namespace walkers
