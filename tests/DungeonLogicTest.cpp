// Every flow of the refugee dungeons' pool (docs/walkers-spec.md 4.20.7; Nadi,
// 2026-11-13: "passt A + B"): a candidate a loaded quest uses is out (A - the
// quests of the player's own load order, read in the game), one the INI's
// [Dungeons] Skip names is out (B - by name or by a cell's form ID), the
// rest is the pool.

#include "DungeonLogic.h"

#include <cstdio>
#include <cstring>

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf("  %s  %s\n", condition ? "ok  " : "FAIL", what);
	if (!condition) {
		++g_failures;
	}
}

using walkers::DungeonCandidate;
using walkers::DungeonKind;
using walkers::DungeonOut;
using walkers::DungeonSkips;

constexpr DungeonCandidate kFort = {"Fort Aurus", DungeonKind::Fort, {0x00001111, 0x00001112, 0, 0}, 2};
constexpr DungeonCandidate kCave = {"Lost Boy Cavern", DungeonKind::Cave, {0x00002221, 0, 0, 0}, 1};

void TestSkipList() {
	std::printf("The INI's skip list\n");
	DungeonSkips none;
	walkers::ParseDungeonSkips("", none);
	Check(none.count == 0, "empty: nothing skipped");
	DungeonSkips s;
	walkers::ParseDungeonSkips(" Fort Aurus ,lost boy cavern, 00002221 ,,", s);
	Check(s.count == 3, "three entries, the empty ones dropped");
	Check(std::strcmp(s.names[0], "Fort Aurus") == 0, "spaces around a name trimmed");
	Check(walkers::SkipsName(s, "Lost Boy Cavern"), "a name matches without regard to case");
	Check(!walkers::SkipsName(s, "Fort"), "a part of a name does not match");
	Check(walkers::SkipsCell(s, 0x00002221), "a form ID in hex names a cell");
	Check(!walkers::SkipsCell(s, 0x00001111), "a cell not listed is not skipped");
	DungeonSkips full;
	walkers::ParseDungeonSkips("a,b,c,d,e,f,g,h,i,j,k,l,m,n,o,p,q,r,s,t,u,v,w,x,y,z,aa,bb,cc,dd,ee,ff,gg,hh", full);
	Check(full.count == walkers::kDungeonSkipMost, "more than the most: the rest dropped");
	DungeonSkips longName;
	walkers::ParseDungeonSkips("A Name Far Longer Than Any Dungeon Of Cyrodiil Ever Carried On Its Map Marker", longName);
	Check(longName.count == 1 && std::strlen(longName.names[0]) == walkers::kDungeonNameMost - 1,
	      "a name too long: cut, not lost");
}

void TestOut() {
	std::printf("Out of the pool or in it\n");
	DungeonSkips none;
	const UInt32 questCells[] = {0x00009999, 0x00001112};
	Check(DungeonOut(kFort, questCells, 2, none) == walkers::DungeonVerdict::Quest,
	      "a quest names a reference in its second cell: out (A)");
	Check(DungeonOut(kCave, questCells, 2, none) == walkers::DungeonVerdict::In, "no quest's cell: in");
	Check(DungeonOut(kCave, nullptr, 0, none) == walkers::DungeonVerdict::In, "no quest cells at all: in");
	DungeonSkips byName;
	walkers::ParseDungeonSkips("lost boy cavern", byName);
	Check(DungeonOut(kCave, nullptr, 0, byName) == walkers::DungeonVerdict::Skipped, "named in the INI: out (B)");
	DungeonSkips byCell;
	walkers::ParseDungeonSkips("00001112", byCell);
	Check(DungeonOut(kFort, nullptr, 0, byCell) == walkers::DungeonVerdict::Skipped,
	      "one of its cells in the INI: out (B)");
	Check(DungeonOut(kFort, questCells, 2, byCell) == walkers::DungeonVerdict::Quest,
	      "both: the quest is the reason logged");
	const DungeonCandidate noCells = {"Nowhere", DungeonKind::Mine, {0, 0, 0, 0}, 0};
	Check(DungeonOut(noCells, questCells, 2, none) == walkers::DungeonVerdict::In,
	      "a candidate without cells is never a quest's (cell 0 matches nothing)");
	const UInt32 zero[] = {0};
	Check(DungeonOut(noCells, zero, 1, none) == walkers::DungeonVerdict::In, "a quest cell 0 matches nothing");
}

void TestPool() {
	std::printf("The pool\n");
	const DungeonCandidate all[] = {kFort, kCave};
	DungeonSkips skips;
	walkers::ParseDungeonSkips("Lost Boy Cavern", skips);
	const UInt32 questCells[] = {0x00001111};
	bool in[2] = {true, true};
	walkers::DungeonPoolCount count = walkers::MarkDungeonPool(all, 2, questCells, 1, skips, in);
	Check(!in[0] && !in[1], "the fort a quest's, the cave skipped: both out");
	Check(count.quest == 1 && count.skipped == 1 && count.in == 0, "counted by reason");
	walkers::DungeonSkips none;
	count = walkers::MarkDungeonPool(all, 2, nullptr, 0, none, in);
	Check(in[0] && in[1] && count.in == 2, "nothing named: both in");
}

void TestKindNames() {
	std::printf("The kinds' names\n");
	Check(std::strcmp(walkers::DungeonKindName(DungeonKind::Cave), "cave") == 0, "cave");
	Check(std::strcmp(walkers::DungeonKindName(DungeonKind::Ayleid), "Ayleid ruin") == 0, "Ayleid ruin");
	Check(std::strcmp(walkers::DungeonKindName(DungeonKind::Fort), "fort") == 0, "fort");
	Check(std::strcmp(walkers::DungeonKindName(DungeonKind::Mine), "mine") == 0, "mine");
}

}  // namespace

int main() {
	TestSkipList();
	TestOut();
	TestPool();
	TestKindNames();
	std::printf(g_failures == 0 ? "All passed\n" : "%d FAILED\n", g_failures);
	return g_failures == 0 ? 0 : 1;
}
