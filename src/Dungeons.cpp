#include "Dungeons.h"

#include <windows.h>

#include <vector>

#include "DungeonCandidates.h"
#include "Engine.h"
#include "News.h"
#include "Replace.h"

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
constexpr UInt32 kRefPositionOffset = 0x2C;
constexpr UInt32 kRefParentCellOffset = 0x40;
constexpr UInt32 kCellFlagsOffset = 0x24;
constexpr UInt32 kCellWorldspaceOffset = 0x50;
constexpr UInt32 kTamriel = 0x0000003C;
constexpr float kDungeonSeenUnits = 6000.0f;  // the camps' kCampSeenUnits
constexpr float kStepSeconds = 1.0f;
constexpr UInt32 kEventsMost = 32;

const DungeonSiteSettings* g_siteSettings = nullptr;
DungeonRollFn g_roll = nullptr;
DungeonSitesState g_sites;
float g_stepClock = 0.0f;

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

// Whether the player is near candidate `c`: in one of its cells, or in
// Tamriel within kDungeonSeenUnits of its door.
bool PlayerNear(const DungeonCandidate& c) {
	const UInt32 player = LookupForm(kPlayerRefId);
	if (!LooksLikeObject(player)) {
		return false;
	}
	const UInt32 cell = Read(player + kRefParentCellOffset);
	if (!LooksLikeObject(cell)) {
		return false;
	}
	const UInt32 id = FormIdOf(cell);
	for (UInt32 k = 0; k < c.cellCount && k < kDungeonCellsMost; ++k) {
		if (c.cells[k] == id) {
			return true;
		}
	}
	if ((Read8(cell + kCellFlagsOffset) & 1) != 0) {
		return false;
	}
	const UInt32 world = Read(cell + kCellWorldspaceOffset);
	if (!LooksLikeObject(world) || FormIdOf(world) != kTamriel) {
		return false;
	}
	const float dx = FloatAt(player + kRefPositionOffset) - c.x;
	const float dy = FloatAt(player + kRefPositionOffset + 4) - c.y;
	return dx * dx + dy * dy <= kDungeonSeenUnits * kDungeonSeenUnits;
}

}  // namespace

void ConfigureDungeons(const DungeonSiteSettings* settings, DungeonRollFn roll) {
	g_siteSettings = settings;
	g_roll = roll;
}

DungeonSitesState& CurrentDungeons() { return g_sites; }

UInt32 kDungeonCandidateCountForSave() { return kDungeonCandidateCount; }

void ResetDungeonsInGame() {
	g_sites = DungeonSitesState{};
	g_stepClock = 0.0f;
}

void StepDungeonsInGame(float dt, LogFn log) {
	if (!g_selected || g_siteSettings == nullptr || !g_siteSettings->enabled) {
		return;
	}
	g_stepClock += dt;
	if (g_stepClock < kStepSeconds) {
		return;
	}
	g_stepClock = 0.0f;
	const float now = GameDaysNow();
	if (!(now > 0.0f)) {
		return;
	}
	Roll roll;
	DungeonEvent ev[kEventsMost];
	for (UInt32 i = 0; i < g_sites.count; ++i) {
		DungeonSite& d = g_sites.sites[i];
		const bool close = d.candidate < kDungeonCandidateCount && PlayerNear(kDungeonCandidates[d.candidate]);
		const DungeonSite before = d;
		const UInt32 n = StepDungeonSite(d, i, *g_siteSettings, now, close, roll, ev, kEventsMost);
		// A site given up is logged with its name from before.
		LogEvents(ev, n, d.state == DungeonState::Free ? before : d, log);
	}
	DropFreeSites(g_sites);
	const UInt32 n = FillDungeons(g_sites, *g_siteSettings, kDungeonCandidates, kDungeonCandidateCount, g_inPool, now,
	                              roll, ev, kEventsMost);
	for (UInt32 k = 0; k < n && k < kEventsMost; ++k) {
		LogEvents(ev + k, 1, g_sites.sites[ev[k].site], log);
	}
}

}  // namespace walkers
