// Every flow of the held dungeons' people and occupants in the game
// (docs/walkers-spec.md 4.20.7, step 3b): which site the one kit of people
// stands at, who of it is shown, where each stands, which actor of a held
// place is taken out and put back, the list of those taken out and its
// co-save image, the test switch's site.

#include "DungeonKitLogic.h"

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
using walkers::DungeonOccupants;
using walkers::DungeonSitesState;
using walkers::DungeonState;

constexpr DungeonCandidate kAll[] = {
    {"A", DungeonKind::Cave, {0x11, 0x12, 0, 0}, 2, 0, 0, true, 0.0f, 0.0f, 0x100, 2, {{100, 0, 0}, {200, 50, -10}}},
    {"B", DungeonKind::Fort, {0x21, 0, 0, 0}, 1, 0, 0, true, 30000.0f, 0.0f, 0x200, 0, {}},
};

DungeonSitesState Two(DungeonState a, DungeonState b) {
	DungeonSitesState st;
	st.count = 2;
	st.sites[0].candidate = 0;
	st.sites[0].state = a;
	st.sites[0].people = 5;
	st.sites[1].candidate = 1;
	st.sites[1].state = b;
	st.sites[1].people = 3;
	return st;
}

void TestKitSite() {
	std::printf("The site the kit stands at\n");
	DungeonSitesState st = Two(DungeonState::Refuge, DungeonState::Besieged);
	const float none[] = {-1.0f, -1.0f};
	Check(walkers::KitSite(st, none, true, -1) == -1, "the player near none: nowhere");
	const float first[] = {3000.0f, -1.0f};
	Check(walkers::KitSite(st, first, true, -1) == 0, "near the first: there");
	Check(walkers::KitSite(st, first, false, 0) == -1, "[Dungeons] Enabled=0: nowhere");
	const float both[] = {3000.0f, 2500.0f};
	Check(walkers::KitSite(st, both, true, -1) == 1, "near both: the nearer");
	Check(walkers::KitSite(st, both, true, 0) == 0, "near both, the kit at the first already, the other 500 nearer: it stays");
	const float inside[] = {3000.0f, 0.0f};
	Check(walkers::KitSite(st, inside, true, 0) == 1, "the player inside the other: it moves there");
	DungeonSitesState clear = Two(DungeonState::ToClear, DungeonState::WalkerDungeon);
	Check(walkers::KitSite(clear, both, true, -1) == -1, "to clear and a walker dungeon: nobody lives inside");
	Check(walkers::DungeonHeld(DungeonState::Refuge) && walkers::DungeonHeld(DungeonState::Besieged) &&
	          !walkers::DungeonHeld(DungeonState::ToClear) && !walkers::DungeonHeld(DungeonState::WalkerDungeon) &&
	          !walkers::DungeonHeld(DungeonState::Free),
	      "held: a refuge and a besieged place");
	const float gone[] = {-1.0f, 2500.0f};
	Check(walkers::KitSite(st, gone, true, 0) == 1, "the kit's site no longer near: to the other");
	DungeonSitesState w = Two(DungeonState::WalkerDungeon, DungeonState::Refuge);
	w.sites[0].walkersLeft = 4;
	Check(walkers::WalkerSite(w, both, true, -1) == 0, "the walkers' pool: to the walker dungeon near, not the refuge");
	w.sites[1].state = DungeonState::Besieged;
	w.sites[1].walkersLeft = 3;
	Check(walkers::WalkerSite(w, both, true, -1) == 1, "two with walkers near: the nearer");
	Check(walkers::WalkerSite(w, both, true, 0) == 0, "the pool at the first already, the other 500 nearer: it stays");
	w.sites[0].walkersLeft = 0;
	Check(walkers::WalkerSite(w, both, true, 0) == 1, "its walkers all ended: to the other");
	Check(walkers::WalkerSite(w, both, false, 1) == -1, "[Dungeons] Enabled=0: nowhere");
}

void TestKitWanted() {
	std::printf("Who of the kit is shown\n");
	Check(walkers::KitWanted(0, true, 0) == 0, "nobody alive: none, no trader either");
	Check(walkers::KitWanted(1, false, 0) == 0x0001, "one: the first man");
	Check(walkers::KitWanted(2, false, 0) == 0x0041, "two: a man and a woman");
	Check(walkers::KitWanted(4, true, 0) == (0x0003 | 0x00C0 | 0x1000), "four and the trader");
	Check(walkers::KitWanted(12, false, 0) == 0x0FFF, "twelve: all");
	Check(walkers::KitWanted(40, false, 0) == 0x0FFF, "more than the kit has: all");
	Check(walkers::KitWanted(2, false, 0x0001) == 0x0042,
	      "the first man dead, two left: the woman and the next man");
	Check(walkers::KitWanted(3, true, 0x1000) == (0x0003 | 0x0040), "the trader dead: no trader");
	Check(walkers::KitTrader(DungeonState::Refuge) && !walkers::KitTrader(DungeonState::Besieged),
	      "the trader trades in a quiet refuge only");
}

void TestSpot() {
	std::printf("Where each stands\n");
	const walkers::DungeonSpot a = walkers::KitSpot(kAll[0], 0);
	const walkers::DungeonSpot b = walkers::KitSpot(kAll[0], 1);
	const walkers::DungeonSpot c = walkers::KitSpot(kAll[0], 2);
	Check(a.dx == 100 && a.dy == 0 && b.dx == 200 && b.dy == 50 && b.dz == -10, "the first two on the two spots");
	Check(c.dz == 0 && (c.dx != 100 || c.dy != 0) && c.dx >= 100 - 60 && c.dx <= 100 + 60,
	      "the third shares the first spot, a step aside");
	const walkers::DungeonSpot z = walkers::KitSpot(kAll[1], 5);
	Check(z.dx == 0 && z.dy != 0, "no spots at all: by the door inside, a step aside each");
	const walkers::DungeonSpot far = walkers::KitSpot(kAll[0], 0, true);
	Check(far.dx == 200 && far.dy == 50, "besieged: the people from the far end - the walkers come in by the door");
	const walkers::DungeonSpot in = walkers::WalkerSpot(kAll[0], 0, true);
	const walkers::DungeonSpot in2 = walkers::WalkerSpot(kAll[0], 3, true);
	Check(in.dx == 100 && in.dy == 0, "a walker coming in: the spot nearest the door");
	Check(in2.dx != 100 || in2.dy != 0, "the fourth coming in: a step aside of a near spot");
	const walkers::DungeonSpot spread = walkers::WalkerSpot(kAll[0], 1, false);
	Check(spread.dx == 200 && spread.dy == 50, "a walker dungeon's stand spread over the spots");
}

void TestOccupant() {
	std::printf("Which actor of a held place is taken out\n");
	walkers::OccupantFacts f;
	f.actor = true;
	Check(walkers::TakesOut(f), "an actor alive in a held place: out");
	walkers::OccupantFacts g = f;
	g.actor = false;
	Check(!walkers::TakesOut(g), "no actor: stays");
	g = f;
	g.player = true;
	Check(!walkers::TakesOut(g), "the player: stays");
	g = f;
	g.ours = true;
	Check(!walkers::TakesOut(g), "one of the kit: stays");
	g = f;
	g.follower = true;
	Check(!walkers::TakesOut(g), "one who follows the player (a companion, a summon): stays");
	g = f;
	g.dead = true;
	Check(!walkers::TakesOut(g), "a dead one: lies as it lay");
	g = f;
	g.disabled = true;
	Check(!walkers::TakesOut(g), "disabled already (a quest's, ours): not ours to count");
	g = f;
	g.risen = true;
	Check(!walkers::TakesOut(g), "a walker risen from one of its people: stays");
}

void TestOccupants() {
	std::printf("The list of those taken out\n");
	static DungeonOccupants o;
	o = DungeonOccupants{};
	Check(walkers::AddOccupant(o, 0x11, 0xFF000001) && walkers::AddOccupant(o, 0x11, 0x00012345) &&
	          walkers::AddOccupant(o, 0x12, 0xFF000002) && o.count == 3,
	      "three noted, two of one cell");
	Check(!walkers::AddOccupant(o, 0x11, 0xFF000001) && o.count == 3, "the same again: not twice");
	Check(walkers::OccupantsOf(o, 0x11) == 2 && walkers::OccupantsOf(o, 0x99) == 0, "counted by cell");
	UInt32 out[8];
	Check(walkers::TakeOccupantsOf(o, 0x11, out, 8) == 2 && out[0] == 0xFF000001 && out[1] == 0x00012345 && o.count == 1 &&
	          o.list[0].ref == 0xFF000002,
	      "a cell's taken off the list, the others kept");
	Check(walkers::TakeOccupantsOf(o, 0x11, out, 8) == 0, "none left of that cell");
	Check(walkers::TakeOccupantsOf(o, 0x12, out, 0) == 0 && o.count == 1, "no room to give them: left on the list");
	o = DungeonOccupants{};
	bool all = true;
	for (UInt32 i = 0; i < walkers::kDungeonOccupantsMost; ++i) {
		all = all && walkers::AddOccupant(o, 0x11, 0xFF100000 + i);
	}
	Check(all && !walkers::AddOccupant(o, 0x11, 0xFF200000), "full: no more noted");
	Check(!walkers::AddOccupant(o, 0, 5) && !walkers::AddOccupant(o, 5, 0), "no cell or no reference: not noted");
}

bool Resolve(UInt32 saved, UInt32& loaded) {
	if (saved == 0x05000777) {
		return false;  // its mod gone
	}
	loaded = (saved >> 24) == 0x05 ? (saved & 0x00FFFFFF) | 0x07000000 : saved;
	return true;
}

void TestImage() {
	std::printf("The occupants' co-save image\n");
	static DungeonOccupants o;
	o = DungeonOccupants{};
	walkers::AddOccupant(o, 0x11, 0xFF000001);
	walkers::AddOccupant(o, 0x11, 0x05000123);
	walkers::AddOccupant(o, 0x12, 0x05000777);
	static UInt8 image[walkers::kDungeonOccupantImageMost];
	Check(walkers::OccupantImage(o, image, 4) == 0, "too small a buffer: nothing");
	const UInt32 n = walkers::OccupantImage(o, image, sizeof image);
	Check(n == 4 + 3 * 8, "the count and eight bytes each");
	static DungeonOccupants back;
	Check(walkers::ReadOccupantImage(back, image, n, &Resolve) && back.count == 2, "read back, the one of a mod gone dropped");
	Check(back.list[0].cell == 0x11 && back.list[0].ref == 0xFF000001 && back.list[1].ref == 0x07000123,
	      "a dynamic one as it was, a mod's by its load index now");
	Check(!walkers::ReadOccupantImage(back, image, n - 1, &Resolve) && back.count == 0, "cut short: none");
	Check(!walkers::ReadOccupantImage(back, image, 2, &Resolve) && back.count == 0, "no count: none");
}

void TestCells() {
	std::printf("A cell's candidate and site\n");
	Check(walkers::CandidateOfCell(kAll, 2, 0x12) == 0 && walkers::CandidateOfCell(kAll, 2, 0x21) == 1,
	      "a cell of a candidate: that one");
	Check(walkers::CandidateOfCell(kAll, 2, 0x99) == -1 && walkers::CandidateOfCell(kAll, 2, 0) == -1, "another cell: none");
	DungeonSitesState st = Two(DungeonState::Refuge, DungeonState::WalkerDungeon);
	Check(walkers::SiteOfCandidate(st, 1) == 1 && walkers::SiteOfCandidate(st, 7) == -1, "the site of a candidate, or none");
	Check(walkers::CellHeld(st, kAll, 2, 0x11, true) && walkers::CellHeld(st, kAll, 2, 0x21, true),
	      "a refuge's and a walker dungeon's cells: the game's occupants are out");
	DungeonSitesState clearing = Two(DungeonState::ToClear, DungeonState::Besieged);
	Check(!walkers::CellHeld(clearing, kAll, 2, 0x11, true) && walkers::CellHeld(clearing, kAll, 2, 0x21, true),
	      "a place to clear keeps its occupants (the player clears them), a besieged one not");
	Check(walkers::OccupantsOut(DungeonState::Refuge) && walkers::OccupantsOut(DungeonState::Besieged) &&
	          walkers::OccupantsOut(DungeonState::WalkerDungeon) && !walkers::OccupantsOut(DungeonState::ToClear) &&
	          !walkers::OccupantsOut(DungeonState::Free),
	      "occupants out: refuge, besieged, walker dungeon");
	Check(!walkers::CellHeld(st, kAll, 2, 0x11, false), "[Dungeons] Enabled=0: nothing is held");
	Check(!walkers::CellHeld(st, kAll, 2, 0x99, true), "no candidate's cell: not held");
}

void TestForce() {
	std::printf("The test switch's site\n");
	const bool inPool[2] = {true, true};
	DungeonSitesState st;
	const walkers::DungeonSiteSettings fs;
	Check(walkers::ForceSite(st, kAll, 2, inPool, "b", DungeonState::Refuge, fs, 2.5f) && st.count == 1 && st.sites[0].candidate == 1 &&
	          st.sites[0].state == DungeonState::Refuge && st.sites[0].people == 8 && st.sites[0].dayFrom == 2.5f &&
	          st.sites[0].lastSeen == 2.5f && st.sites[0].walkersLeft == 0,
	      "TestSite=b: the first site, a refuge with PeopleMax (the name without regard to case)");
	Check(!walkers::ForceSite(st, kAll, 2, inPool, "A", DungeonState::Refuge, fs, 3.0f) && st.count == 1,
	      "sites there already: nothing");
	DungeonSitesState none;
	Check(!walkers::ForceSite(none, kAll, 2, inPool, "Nowhere", DungeonState::Refuge, fs, 1.0f) && none.count == 0,
	      "no such candidate: nothing");
	const bool out[2] = {false, true};
	Check(!walkers::ForceSite(none, kAll, 2, out, "A", DungeonState::Refuge, fs, 1.0f), "the candidate out of the pool: nothing");
	Check(!walkers::ForceSite(none, kAll, 2, inPool, "", DungeonState::Refuge, fs, 1.0f), "no name: nothing");
	DungeonSitesState siege;
	Check(walkers::ForceSite(siege, kAll, 2, inPool, "A", DungeonState::Besieged, fs, 1.0f) &&
	          siege.sites[0].people == 8 && siege.sites[0].walkersLeft == 16 && siege.sites[0].tollFrom == 1.0f,
	      "TestSiteState besieged: PeopleMax inside, AttackWalkersMax storming");
	DungeonSitesState dead;
	Check(walkers::ForceSite(dead, kAll, 2, inPool, "A", DungeonState::WalkerDungeon, fs, 1.0f) &&
	          dead.sites[0].people == 0 && dead.sites[0].walkersLeft == 16,
	      "a walker dungeon: its walkers, nobody");
	DungeonSitesState clear;
	Check(walkers::ForceSite(clear, kAll, 2, inPool, "A", DungeonState::ToClear, fs, 1.0f) &&
	          clear.sites[0].people == 0 && clear.sites[0].waiting == 8 && clear.sites[0].walkersLeft == 0,
	      "to clear: the people waiting outside");
	DungeonSitesState free;
	Check(!walkers::ForceSite(free, kAll, 2, inPool, "A", DungeonState::Free, fs, 1.0f), "no state: nothing");
}

}  // namespace

int main() {
	TestKitSite();
	TestKitWanted();
	TestSpot();
	TestOccupant();
	TestOccupants();
	TestImage();
	TestCells();
	TestForce();
	std::printf(g_failures == 0 ? "All passed\n" : "%d FAILED\n", g_failures);
	return g_failures == 0 ? 0 : 1;
}
