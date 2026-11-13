#include "Dungeons.h"

#include <windows.h>

#include <vector>

#include "DungeonCandidates.h"
#include "Replace.h"

namespace walkers {
namespace {

bool g_inPool[kDungeonCandidateCount] = {};
bool g_selected = false;

}  // namespace

void SelectDungeonPool(const char* iniPath, LogFn log) {
	if (g_selected) {
		return;
	}
	g_selected = true;
	CollectQuestNames(log);
	const std::vector<UInt32> questCells = QuestCellIds();
	char text[kDungeonSkipMost * kDungeonNameMost] = {};
	if (iniPath != nullptr && iniPath[0] != '\0') {
		GetPrivateProfileStringA("Dungeons", "Skip", "", text, sizeof text, iniPath);
	}
	DungeonSkips skips;
	ParseDungeonSkips(text, skips);
	const DungeonPoolCount n =
	    MarkDungeonPool(kDungeonCandidates, kDungeonCandidateCount, questCells.data(),
	                    static_cast<UInt32>(questCells.size()), skips, g_inPool);
	for (UInt32 i = 0; i < kDungeonCandidateCount; ++i) {
		const DungeonVerdict v =
		    DungeonOut(kDungeonCandidates[i], questCells.data(), static_cast<UInt32>(questCells.size()), skips);
		if (v != DungeonVerdict::In) {
			log("Dungeons: %s (%s, cell %08X) out - %s", kDungeonCandidates[i].name,
			    DungeonKindName(kDungeonCandidates[i].kind), kDungeonCandidates[i].cells[0],
			    v == DungeonVerdict::Quest ? "a loaded quest names something in it" : "[Dungeons] Skip names it");
		}
	}
	log("Dungeons: %u candidates, %u in the pool, %u out by a loaded quest, %u by [Dungeons] Skip (%u entries)",
	    kDungeonCandidateCount, n.in, n.quest, n.skipped, skips.count);
}

bool DungeonInPool(UInt32 index) { return index < kDungeonCandidateCount && g_inPool[index]; }

}  // namespace walkers
