#include "Dungeons.h"

#include <windows.h>

#include <vector>

#include "DungeonCandidates.h"
#include "Replace.h"

namespace walkers {
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

}  // namespace walkers
