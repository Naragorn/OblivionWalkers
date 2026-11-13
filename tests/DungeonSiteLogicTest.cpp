// Every flow of the held dungeons' situation (docs/walkers-spec.md 4.20.7,
// Nadi's vision of 2026-11-11, step 2 of 2026-11-12): how many are active,
// which places and variants are rolled, a refuge attacked, a siege fought
// out away (the toll) or near (the walkers ended, the people dying), a
// place overrun into a walker dungeon, one cleared and taken, one left
// when the player has not come for a while.

#include "DungeonSiteLogic.h"

#include <cmath>
#include <cstdio>

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf("  %s  %s\n", condition ? "ok  " : "FAIL", what);
	if (!condition) {
		++g_failures;
	}
}

using walkers::DungeonCandidate;
using walkers::DungeonEvent;
using walkers::DungeonKind;
using walkers::DungeonSite;
using walkers::DungeonSiteSettings;
using walkers::DungeonSitesState;
using walkers::DungeonState;

// A fixed sequence of rolls.
struct Rolls {
	const UInt32* values;
	UInt32 count;
	UInt32 at = 0;
	UInt32 operator()() { return values[at++ % count]; }
};

constexpr DungeonCandidate kAll[] = {
    {"A", DungeonKind::Cave, {1, 0, 0, 0}, 1, 0, 0, true, 0.0f, 0.0f},
    {"B", DungeonKind::Fort, {2, 0, 0, 0}, 1, 0, 0, true, 5000.0f, 0.0f},     // too near A
    {"C", DungeonKind::Mine, {3, 0, 0, 0}, 1, 0, 0, true, 30000.0f, 0.0f},
    {"D", DungeonKind::Ayleid, {4, 0, 0, 0}, 1, 0, 0, true, 60000.0f, 0.0f},
    {"E", DungeonKind::Cave, {5, 0, 0, 0}, 1, 0, 0, true, 90000.0f, 0.0f},
    {"F", DungeonKind::Cave, {6, 0, 0, 0}, 1, 0, 0, true, 120000.0f, 0.0f},
};
constexpr UInt32 kCount = 6;

void TestVariant() {
	std::printf("The variant rolled\n");
	DungeonSiteSettings s;
	Check(walkers::RollVariant(s, 0) == DungeonState::Refuge, "roll 0 of four equal weights: refuge");
	Check(walkers::RollVariant(s, 25) == DungeonState::Besieged, "roll 25: besieged");
	Check(walkers::RollVariant(s, 50) == DungeonState::ToClear, "roll 50: to clear");
	Check(walkers::RollVariant(s, 99) == DungeonState::WalkerDungeon, "roll 99: walker dungeon");
	s.weightRefuge = 0;
	s.weightBesieged = 0;
	s.weightToClear = 0;
	Check(walkers::RollVariant(s, 0) == DungeonState::WalkerDungeon, "only one weight: always that one");
	s.weightWalkerDungeon = 0;
	Check(walkers::RollVariant(s, 7) == DungeonState::Refuge, "every weight 0: a refuge");
}

void TestFill() {
	std::printf("The active places\n");
	DungeonSiteSettings s;
	s.activeMin = 3;
	s.activeMax = 3;
	DungeonSitesState st;
	bool inPool[kCount] = {true, true, true, true, true, true};
	const UInt32 r[] = {0, 0, 1, 0, 2, 0, 3, 0, 4, 0};
	Rolls rolls{r, 10};
	DungeonEvent ev[16];
	UInt32 n = walkers::FillDungeons(st, s, kAll, kCount, inPool, 1.0f, rolls, ev, 16);
	Check(st.count == 3 && n == 3, "three active, each an event");
	Check(ev[0].kind == walkers::DungeonEventKind::Rolled, "the event: rolled");
	bool spaced = true;
	for (UInt32 i = 0; i < st.count; ++i) {
		for (UInt32 j = i + 1; j < st.count; ++j) {
			spaced = spaced && !(st.sites[i].candidate <= 1 && st.sites[j].candidate <= 1);
		}
	}
	Check(spaced, "A and B (5000 apart) never both");
	n = walkers::FillDungeons(st, s, kAll, kCount, inPool, 1.0f, rolls, ev, 16);
	Check(n == 0 && st.count == 3, "full: nothing more");
	DungeonSitesState few;
	bool onlyA[kCount] = {true, true, false, false, false, false};
	n = walkers::FillDungeons(few, s, kAll, kCount, onlyA, 1.0f, rolls, ev, 16);
	Check(few.count == 1, "the pool too small or too close: as many as fit");
	DungeonSitesState none;
	bool empty[kCount] = {};
	Check(walkers::FillDungeons(none, s, kAll, kCount, empty, 1.0f, rolls, ev, 16) == 0 && none.count == 0,
	      "an empty pool: none");
	DungeonSiteSettings off = s;
	off.enabled = false;
	DungeonSitesState offSt;
	Check(walkers::FillDungeons(offSt, off, kAll, kCount, inPool, 1.0f, rolls, ev, 16) == 0, "Enabled=0: none");
	DungeonSiteSettings range = s;
	range.activeMin = 3;
	range.activeMax = 5;
	Check(walkers::ActiveTarget(range, 0) == 3 && walkers::ActiveTarget(range, 2) == 5 &&
	          walkers::ActiveTarget(range, 4) == 4,
	      "3..5: each count by the roll");
	range.activeMax = 2;
	Check(walkers::ActiveTarget(range, 9) == 3, "the most below the least: the least");
	range.activeMin = 50;
	range.activeMax = 50;
	Check(walkers::ActiveTarget(range, 0) == walkers::kDungeonActiveMost, "held to the most there can be");
}

DungeonSite Refuge(UInt8 people) {
	DungeonSite d;
	d.candidate = 2;
	d.state = DungeonState::Refuge;
	d.people = people;
	d.dayFrom = 0.0f;
	d.lastSeen = 0.0f;
	return d;
}

void TestAttack() {
	std::printf("A refuge attacked\n");
	DungeonSiteSettings s;
	s.attackChancePercent = 15;
	DungeonSite d = Refuge(6);
	DungeonEvent ev[8];
	const UInt32 miss[] = {50};
	Rolls m{miss, 1};
	Check(walkers::StepDungeonSite(d, 0, s, 0.5f, false, m, ev, 8) == 0 && d.state == DungeonState::Refuge,
	      "half a day: no roll yet");
	walkers::StepDungeonSite(d, 0, s, 1.0f, false, m, ev, 8);
	Check(d.state == DungeonState::Refuge && d.dayFrom == 1.0f, "a day, roll 50 of 15 %: quiet, the next day counted");
	const UInt32 hit[] = {3, 4};
	Rolls h{hit, 2};
	const UInt32 n = walkers::StepDungeonSite(d, 0, s, 2.0f, false, h, ev, 8);
	Check(d.state == DungeonState::Besieged && n == 1 && ev[0].kind == walkers::DungeonEventKind::Attacked,
	      "roll 3: besieged");
	Check(d.walkersLeft >= s.walkersMin && d.walkersLeft <= s.walkersMax, "its walkers between the least and most");
	DungeonSite q = Refuge(6);
	walkers::StepDungeonSite(q, 0, s, 9.0f, false, h, ev, 8);
	Check(q.state == DungeonState::Besieged, "days missed (asleep, away): rolled for, at most one attack");
}

void TestSiegeAway() {
	std::printf("A siege away: the toll\n");
	DungeonSiteSettings s;
	s.refugeeKillsPerHour = 2.0f;
	s.lossPercentPerHour = 25;
	DungeonSite d = Refuge(6);
	d.state = DungeonState::Besieged;
	d.walkersLeft = 4;
	d.tollFrom = 0.0f;
	DungeonEvent ev[16];
	const UInt32 keep[] = {99};
	Rolls k{keep, 1};
	walkers::StepDungeonSite(d, 0, s, 1.0f / 24.0f * 0.5f, false, k, ev, 16);
	Check(d.walkersLeft == 4, "half an hour: nothing yet");
	const UInt32 n = walkers::StepDungeonSite(d, 0, s, 2.0f / 24.0f + 0.001f, false, k, ev, 16);
	Check(d.state == DungeonState::Refuge && d.walkersLeft == 0 && n >= 1 &&
	          ev[n - 1].kind == walkers::DungeonEventKind::Held,
	      "two hours, two killed an hour: held, a refuge again");
	DungeonSite lose = Refuge(1);
	lose.state = DungeonState::Besieged;
	lose.walkersLeft = 50;
	lose.tollFrom = 0.0f;
	const UInt32 always[] = {0};
	Rolls a{always, 1};
	const UInt32 m = walkers::StepDungeonSite(lose, 0, s, 1.0f / 24.0f + 0.001f, false, a, ev, 16);
	Check(lose.state == DungeonState::WalkerDungeon && lose.people == 0, "the last one lost: overrun");
	Check(m >= 2 && ev[0].kind == walkers::DungeonEventKind::PersonLost &&
	          ev[m - 1].kind == walkers::DungeonEventKind::Overrun,
	      "the events: one lost, then overrun");
	Check(lose.walkersLeft == 51, "the one lost rises: one walker more there");
	DungeonSite near = Refuge(3);
	near.state = DungeonState::Besieged;
	near.walkersLeft = 5;
	near.tollFrom = 0.0f;
	walkers::StepDungeonSite(near, 0, s, 0.5f, true, a, ev, 16);
	Check(near.walkersLeft == 5 && near.people == 3 && near.seen, "near: no toll - the game fights it out");
	walkers::StepDungeonSite(near, 0, s, 0.52f, false, k, ev, 16);
	Check(near.walkersLeft == 5, "left again: the toll counts from the moment he left");
}

void TestNear() {
	std::printf("A siege near: the game's fight\n");
	DungeonSite d = Refuge(2);
	d.state = DungeonState::Besieged;
	d.walkersLeft = 2;
	d.seen = true;
	DungeonEvent ev[8];
	Check(walkers::DungeonWalkerEnded(d, 0, ev, 8) == 0 && d.walkersLeft == 1, "one walker ended: one left");
	const UInt32 n = walkers::DungeonWalkerEnded(d, 0, ev, 8);
	Check(n == 1 && ev[0].kind == walkers::DungeonEventKind::Defended && d.state == DungeonState::Refuge,
	      "the last ended with the player there: defended");
	DungeonSite unseen = Refuge(2);
	unseen.state = DungeonState::Besieged;
	unseen.walkersLeft = 1;
	walkers::DungeonWalkerEnded(unseen, 0, ev, 8);
	Check(ev[0].kind == walkers::DungeonEventKind::Held, "the last ended unseen: held, not defended");
	DungeonSite die = Refuge(2);
	die.state = DungeonState::Besieged;
	die.walkersLeft = 3;
	Check(walkers::DungeonPersonDied(die, 0, ev, 8) == 0 && die.people == 1, "one of two died: one left");
	const UInt32 m = walkers::DungeonPersonDied(die, 0, ev, 8);
	Check(m == 1 && ev[0].kind == walkers::DungeonEventKind::Overrun && die.state == DungeonState::WalkerDungeon,
	      "the last died: overrun");
	Check(walkers::DungeonPersonDied(die, 0, ev, 8) == 0 && die.people == 0, "a walker dungeon: no one to lose");
	DungeonSite quiet = Refuge(1);
	Check(walkers::DungeonWalkerEnded(quiet, 0, ev, 8) == 0, "a walker ended at a quiet refuge: nothing");
}

void TestCleared() {
	std::printf("Cleared and taken\n");
	DungeonSiteSettings s;
	DungeonEvent ev[8];
	const UInt32 r[] = {2};
	Rolls rolls{r, 1};
	DungeonSite clear = Refuge(0);
	clear.state = DungeonState::ToClear;
	clear.waiting = 5;
	Check(walkers::DungeonCleared(clear, 0, s, rolls, ev, 8) == 1 && clear.state == DungeonState::Refuge &&
	          ev[0].kind == walkers::DungeonEventKind::Taken && clear.people == 5,
	      "to clear, cleared: those waiting outside move in");
	DungeonSite walker = Refuge(0);
	walker.state = DungeonState::WalkerDungeon;
	walkers::DungeonCleared(walker, 0, s, rolls, ev, 8);
	Check(walker.state == DungeonState::Refuge && walker.people >= s.peopleMin && walker.people <= s.peopleMax,
	      "a walker dungeon cleared: taken again, new people");
	DungeonSite refuge = Refuge(4);
	Check(walkers::DungeonCleared(refuge, 0, s, rolls, ev, 8) == 0 && refuge.people == 4,
	      "a refuge is not cleared: nothing");
}

void TestLeft() {
	std::printf("Left when the player stays away\n");
	DungeonSiteSettings s;
	s.leaveAfterDays = 7.0f;
	DungeonEvent ev[8];
	const UInt32 miss[] = {99};
	Rolls m{miss, 1};
	DungeonSite d = Refuge(4);
	walkers::StepDungeonSite(d, 0, s, 6.9f, false, m, ev, 8);
	Check(d.state == DungeonState::Refuge, "6.9 days unseen: still there");
	const UInt32 n = walkers::StepDungeonSite(d, 0, s, 7.0f, false, m, ev, 8);
	Check(d.state == DungeonState::Free && n >= 1 && ev[n - 1].kind == walkers::DungeonEventKind::Left,
	      "7 days unseen: the place given up, free for another");
	DungeonSite seen = Refuge(4);
	walkers::StepDungeonSite(seen, 0, s, 6.0f, true, m, ev, 8);
	walkers::StepDungeonSite(seen, 0, s, 12.0f, false, m, ev, 8);
	Check(seen.state == DungeonState::Refuge, "seen at day 6: the days count from then");
	DungeonSite siege = Refuge(4);
	siege.state = DungeonState::Besieged;
	siege.walkersLeft = 200;
	siege.tollFrom = 0.0f;
	walkers::StepDungeonSite(siege, 0, s, 7.5f, false, m, ev, 8);
	Check(siege.state != DungeonState::Free, "a siege is fought out first, never left");
	DungeonSiteSettings never = s;
	never.leaveAfterDays = 0.0f;
	DungeonSite stay = Refuge(4);
	walkers::StepDungeonSite(stay, 0, never, 100.0f, false, m, ev, 8);
	Check(stay.state == DungeonState::Refuge, "LeaveAfterDays=0: never left");
	DungeonSitesState st;
	st.count = 2;
	st.sites[0] = d;
	st.sites[1] = seen;
	walkers::DropFreeSites(st);
	Check(st.count == 1 && st.sites[0].state == DungeonState::Refuge, "the free ones dropped from the list");
}

void TestRolledKinds() {
	std::printf("What each rolled variant starts with\n");
	const DungeonState kinds[4] = {DungeonState::Refuge, DungeonState::Besieged, DungeonState::ToClear,
	                               DungeonState::WalkerDungeon};
	const UInt32 variantRoll[4] = {0, 25, 50, 75};
	for (UInt32 k = 0; k < 4; ++k) {
		DungeonSiteSettings s;
		s.activeMin = 1;
		s.activeMax = 1;
		DungeonSitesState st;
		bool onlyC[kCount] = {false, false, true, false, false, false};
		// the target, the place among those open, the variant, people (roll 1 -> 5), walkers (roll 2 -> 10)
		const UInt32 r[] = {0, 0, variantRoll[k], 1, 2};
		Rolls rolls{r, 5};
		DungeonEvent ev[4];
		walkers::FillDungeons(st, s, kAll, kCount, onlyC, 3.0f, rolls, ev, 4);
		const DungeonSite& d = st.sites[0];
		bool ok = st.count == 1 && d.candidate == 2 && d.state == kinds[k] && d.dayFrom == 3.0f && d.lastSeen == 3.0f;
		switch (kinds[k]) {
			case DungeonState::Refuge:
				ok = ok && d.people == 5 && d.waiting == 0 && d.walkersLeft == 0 && d.tollFrom < 0.0f;
				break;
			case DungeonState::Besieged:
				ok = ok && d.people == 5 && d.walkersLeft == 10 && d.tollFrom == 3.0f;
				break;
			case DungeonState::ToClear:
				ok = ok && d.people == 0 && d.waiting == 5 && d.walkersLeft == 0;
				break;
			default:
				ok = ok && d.people == 0 && d.waiting == 0 && d.walkersLeft == 10;
				break;
		}
		static const char* what[4] = {"refuge: people inside, no walkers", "besieged: people and walkers, the toll from now",
		                              "to clear: people waiting outside", "walker dungeon: walkers, nobody"};
		Check(ok, what[k]);
	}
}

void TestWalkerDungeonAndCap() {
	std::printf("A walker dungeon's walkers, the toll's cap\n");
	DungeonEvent ev[8];
	DungeonSite w = Refuge(0);
	w.state = DungeonState::WalkerDungeon;
	w.walkersLeft = 2;
	Check(walkers::DungeonWalkerEnded(w, 0, ev, 8) == 0 && w.walkersLeft == 1 && w.state == DungeonState::WalkerDungeon,
	      "one ended in a walker dungeon: one fewer, still theirs (cleared by the player only)");
	DungeonSiteSettings s;
	s.refugeeKillsPerHour = 0.0f;
	s.lossPercentPerHour = 0;
	DungeonSite d = Refuge(3);
	d.state = DungeonState::Besieged;
	d.walkersLeft = 5;
	d.tollFrom = 0.0f;
	const UInt32 r[] = {99};
	Rolls rolls{r, 1};
	walkers::StepDungeonSite(d, 0, s, 30.0f, false, rolls, ev, 8);
	Check(d.state == DungeonState::Besieged && std::fabs(d.tollFrom - 7.0f) < 0.001f,
	      "thirty days away, nobody killed: the toll counts at most seven days at a time");
}

}  // namespace

int main() {
	TestRolledKinds();
	TestWalkerDungeonAndCap();
	TestVariant();
	TestFill();
	TestAttack();
	TestSiegeAway();
	TestNear();
	TestCleared();
	TestLeft();
	std::printf(g_failures == 0 ? "All passed\n" : "%d FAILED\n", g_failures);
	return g_failures == 0 ? 0 : 1;
}
