// Every flow of the held dungeons' situation (docs/walkers-spec.md 4.20.7,
// Nadi's vision of 2026-11-11, step 2 of 2026-11-12): how many are active,
// which places and variants are rolled, a refuge attacked, a siege fought
// out away (the toll) or near (the walkers ended, the people dying), a
// place overrun into a walker dungeon, one cleared and taken, one left
// when the player has not come for a while.

#include "DungeonSiteLogic.h"

#include <cmath>
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

void TestClockSetBack() {
	std::printf("A siege away: the clock set back\n");
	// Measured 2026-11-13 (harness dungeon-runner): the console's "set
	// gamehour" put the clock behind the toll's time; the hours counted came
	// out negative, as an unsigned count some four billion - with kills and
	// losses the siege ended at once, with RefugeeKillsPerHour=0 and
	// LossPercentPerHour=0 the game stood still.
	DungeonSiteSettings s;
	s.refugeeKillsPerHour = 0.0f;
	s.lossPercentPerHour = 0;
	DungeonSite d = Refuge(6);
	d.state = DungeonState::Besieged;
	d.walkersLeft = 4;
	d.tollFrom = 2.5f;
	DungeonEvent ev[16];
	const UInt32 keep[] = {0};
	Rolls k{keep, 1};
	Check(walkers::StepDungeonSite(d, 0, s, 2.25f, false, k, ev, 16) == 0 && d.state == DungeonState::Besieged &&
	          d.walkersLeft == 4 && d.people == 6 && d.tollFrom == 2.25f,
	      "the clock six hours behind the toll's time: nothing counted, the toll counted from now");
	s.refugeeKillsPerHour = 2.0f;
	s.lossPercentPerHour = 100;
	d.tollFrom = 2.5f;
	Check(walkers::StepDungeonSite(d, 0, s, 2.25f, false, k, ev, 16) == 0 && d.walkersLeft == 4 && d.people == 6,
	      "with kills and losses set too: no toll for hours that never were");
	Check(walkers::StepDungeonSite(d, 0, s, 2.25f + 1.5f / 24.0f, false, k, ev, 16) >= 1 && d.people == 5 &&
	          d.walkersLeft == 2,
	      "an hour on from there: that hour's toll");
	DungeonSite never = Refuge(6);
	never.state = DungeonState::Besieged;
	never.walkersLeft = 4;
	never.tollFrom = 1.0f;
	s.refugeeKillsPerHour = 0.0f;
	s.lossPercentPerHour = 0;
	Check(walkers::StepDungeonSite(never, 0, s, 400.0f, false, k, ev, 16) == 0 && never.walkersLeft == 4 &&
	          never.people == 6,
	      "no kills and no losses set, a year on: the siege stands (a week's hours counted at most, then on)");
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
	DungeonSiteSettings s;
	const UInt32 r[] = {1};
	Rolls rolls{r, 1};
	Check(walkers::DungeonWalkerEnded(d, 0, s, rolls, ev, 8) == 0 && d.walkersLeft == 1, "one walker ended: one left");
	const UInt32 n = walkers::DungeonWalkerEnded(d, 0, s, rolls, ev, 8);
	Check(n == 1 && ev[0].kind == walkers::DungeonEventKind::Defended && d.state == DungeonState::Refuge,
	      "the last ended with the player there: defended");
	DungeonSite unseen = Refuge(2);
	unseen.state = DungeonState::Besieged;
	unseen.walkersLeft = 1;
	walkers::DungeonWalkerEnded(unseen, 0, s, rolls, ev, 8);
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
	Check(walkers::DungeonWalkerEnded(quiet, 0, s, rolls, ev, 8) == 0, "a walker ended at a quiet refuge: nothing");
	DungeonSite last = Refuge(0);
	last.state = DungeonState::Besieged;
	last.walkersLeft = 1;
	Check(walkers::DungeonWalkerEnded(last, 0, s, rolls, ev, 8) == 0 && last.state == DungeonState::Besieged,
	      "the last walker ended with nobody alive: not held (the overrun is the people's side)");
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
	DungeonSite three = Refuge(0);
	three.state = DungeonState::ToClear;
	three.waiting = 6;
	Check(!walkers::NoteCellClear(three, 0, 3) && three.cleared == 0x1, "the first of three cells found empty: noted");
	Check(!walkers::NoteCellClear(three, 2, 3) && three.cleared == 0x5, "the third: two of three");
	Check(!walkers::NoteCellClear(three, 2, 3), "the same again: still two");
	Check(walkers::NoteCellClear(three, 1, 3), "the last: every cell empty - cleared");
	walkers::DungeonCleared(three, 0, s, rolls, ev, 8);
	Check(three.state == DungeonState::Refuge && three.cleared == 0 && three.people == 6, "taken: the cells' marks gone");
	DungeonSite notClear = Refuge(3);
	Check(!walkers::NoteCellClear(notClear, 0, 1) && notClear.cleared == 0, "a refuge's cell found empty: nothing");
	DungeonSite bad = Refuge(0);
	bad.state = DungeonState::ToClear;
	Check(!walkers::NoteCellClear(bad, 5, 2) && !walkers::NoteCellClear(bad, 0, 0), "a cell past its count, no cells: nothing");
	DungeonSite wait = Refuge(0);
	wait.state = DungeonState::ToClear;
	wait.waiting = 2;
	walkers::DungeonWaitingDied(wait);
	walkers::DungeonWaitingDied(wait);
	walkers::DungeonWaitingDied(wait);
	Check(wait.waiting == 0 && wait.state == DungeonState::ToClear, "those waiting outside die: counted off, never below none");
	DungeonSite inside = Refuge(3);
	walkers::DungeonWaitingDied(inside);
	Check(inside.people == 3, "a refuge has nobody waiting: nothing");
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
		// the target, the place among those open, the variant, people (roll 1 of 3..6 -> 4), walkers (roll 2 -> 10)
		const UInt32 r[] = {0, 0, variantRoll[k], 1, 2};
		Rolls rolls{r, 5};
		DungeonEvent ev[4];
		walkers::FillDungeons(st, s, kAll, kCount, onlyC, 3.0f, rolls, ev, 4);
		const DungeonSite& d = st.sites[0];
		bool ok = st.count == 1 && d.candidate == 2 && d.state == kinds[k] && d.dayFrom == 3.0f && d.lastSeen == 3.0f;
		switch (kinds[k]) {
			case DungeonState::Refuge:
				ok = ok && d.people == 4 && d.waiting == 0 && d.walkersLeft == 0 && d.tollFrom < 0.0f;
				break;
			case DungeonState::Besieged:
				ok = ok && d.people == 4 && d.walkersLeft == 10 && d.before == 5 && d.tollFrom == 3.0f;
				break;
			case DungeonState::ToClear:
				ok = ok && d.people == 0 && d.waiting == 4 && d.walkersLeft == 0;
				break;
			default:
				ok = ok && d.people == 0 && d.waiting == 0 && d.walkersLeft == 10 && d.before == 0;
				break;
		}
		static const char* what[4] = {"refuge: people inside, no walkers", "besieged: people and walkers, half of them before the door, the toll from now",
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
	DungeonSiteSettings s;
	const UInt32 one[] = {1};
	Rolls people{one, 1};
	Check(walkers::DungeonWalkerEnded(w, 0, s, people, ev, 8) == 0 && w.walkersLeft == 1 &&
	          w.state == DungeonState::WalkerDungeon,
	      "one ended in a walker dungeon: one fewer, still theirs");
	const UInt32 taken = walkers::DungeonWalkerEnded(w, 0, s, people, ev, 8);
	Check(taken == 1 && ev[0].kind == walkers::DungeonEventKind::Taken && w.state == DungeonState::Refuge &&
	          w.people == 4 && w.walkersLeft == 0,
	      "the last one ended: the place is cleared and taken - new people (roll 1 of 3..6: 4), a refuge");
	Check(walkers::DungeonWalkersOut(w, 6) == 0, "a refuge has no walkers out");
	DungeonSite many = Refuge(3);
	many.state = DungeonState::Besieged;
	many.walkersLeft = 9;
	Check(walkers::DungeonWalkersOut(many, 6) == 6, "nine storm it, six at once: six out");
	many.walkersLeft = 2;
	Check(walkers::DungeonWalkersOut(many, 6) == 2, "two left: two out");
	many.walkersLeft = 40;
	Check(walkers::DungeonWalkersOut(many, 99) == walkers::kDungeonPool, "never more than the ESP's pool");
	many.state = DungeonState::WalkerDungeon;
	Check(walkers::DungeonWalkersOut(many, 6) == 6, "a walker dungeon's stand there too");
	many.state = DungeonState::ToClear;
	Check(walkers::DungeonWalkersOut(many, 6) == 0, "a place to clear has the game's own inside");
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

// A siege's walkers before the door (Nadi, 2026-11-14: "go Walker vor der
// Tür bei einer Dungeon-Belagerung").
void TestBeforeTheDoor() {
	std::printf("A siege's walkers before the door\n");
	using walkers::PoolPlace;
	Check(walkers::WalkersBefore(8, 50) == 4 && walkers::WalkersBefore(9, 50) == 4 && walkers::WalkersBefore(1, 50) == 0,
	      "WalkersOutsidePercent 50: half of them, rounded down");
	Check(walkers::WalkersBefore(8, 0) == 0 && walkers::WalkersBefore(8, 100) == 8 && walkers::WalkersBefore(8, 250) == 8,
	      "0: none; 100 and more: all");
	Check(walkers::WalkersBefore(1000, 100) == 255, "never more than the count holds");
	DungeonSiteSettings s;
	s.attackChancePercent = 100;
	s.walkersOutsidePercent = 25;
	DungeonSite d = Refuge(6);
	DungeonEvent ev[8];
	const UInt32 hit[] = {0, 4};  // attacked; walkers 8 + 4
	Rolls h{hit, 2};
	walkers::StepDungeonSite(d, 0, s, 1.0f, false, h, ev, 8);
	Check(d.state == DungeonState::Besieged && d.walkersLeft == 12 && d.before == 3,
	      "a refuge attacked: a quarter of its twelve before the door");

	DungeonSite siege = Refuge(4);
	siege.state = DungeonState::Besieged;
	siege.walkersLeft = 10;
	siege.before = 4;
	siege.tollFrom = 0.0f;
	Check(walkers::PoolPlaceFor(siege, 800.0f, true) == PoolPlace::Outside,
	      "the player before the door, walkers there: the pool outside");
	Check(walkers::PoolPlaceFor(siege, 0.0f, true) == PoolPlace::Inside, "the player inside: the pool inside");
	Check(walkers::PoolPlaceFor(siege, 800.0f, false) == PoolPlace::Inside,
	      "a place without spots before its door: inside, all of them");
	Check(walkers::DungeonWalkersAt(siege, 6, PoolPlace::Outside, true) == 4, "outside: the four before the door");
	Check(walkers::DungeonWalkersAt(siege, 3, PoolPlace::Outside, true) == 3, "never more than WalkersAtOnce");
	Check(walkers::DungeonWalkersAt(siege, 6, PoolPlace::Inside, true) == 6 &&
	          walkers::DungeonWalkersAt(siege, 12, PoolPlace::Inside, true) == 6,
	      "inside: the six who are in");
	Check(walkers::DungeonWalkersAt(siege, 12, PoolPlace::Inside, false) == 10 &&
	          walkers::DungeonWalkersAt(siege, 12, PoolPlace::Outside, false) == 0,
	      "no spots before the door: all ten inside, none outside");

	const UInt32 none[] = {0};
	Rolls r{none, 1};
	Check(walkers::DungeonWalkerEnded(siege, 0, s, r, ev, 8, true) == 0 && siege.walkersLeft == 9 && siege.before == 3,
	      "one ended before the door: one off the siege and off those outside");
	Check(walkers::DungeonWalkerEnded(siege, 0, s, r, ev, 8, false) == 0 && siege.walkersLeft == 8 && siege.before == 3,
	      "one ended inside: those outside stay");
	for (UInt32 k = 0; k < 3; ++k) {
		walkers::DungeonWalkerEnded(siege, 0, s, r, ev, 8, true);
	}
	Check(siege.walkersLeft == 5 && siege.before == 0 && siege.state == DungeonState::Besieged &&
	          walkers::PoolPlaceFor(siege, 800.0f, true) == PoolPlace::Inside,
	      "the last one outside ended: the siege goes on inside - the pool inside though he stands outside");

	DungeonSite skipped = Refuge(4);
	skipped.state = DungeonState::Besieged;
	skipped.walkersLeft = 3;
	skipped.before = 3;
	Check(walkers::DungeonWalkersAt(skipped, 6, PoolPlace::Inside, true) == 0 &&
	          !walkers::DungeonBreakIn(skipped, false) && skipped.before == 3,
	      "all of them before the door, the player outside: nobody inside, they stay where they are");
	Check(walkers::DungeonBreakIn(skipped, true) && skipped.before == 0 && skipped.walkersLeft == 3 &&
	          walkers::DungeonWalkersAt(skipped, 6, PoolPlace::Inside, true) == 3 &&
	          walkers::DungeonWalkersAt(skipped, 6, PoolPlace::Outside, true) == 0,
	      "the player inside and nobody left inside: those before the door break in after him");
	Check(!walkers::DungeonBreakIn(skipped, true), "once");
	{
		DungeonSite both = skipped;
		both.walkersLeft = 5;
		both.before = 3;
		DungeonSite quietOne = Refuge(3);
		quietOne.before = 0;
		Check(!walkers::DungeonBreakIn(both, true) && both.before == 3 && !walkers::DungeonBreakIn(quietOne, true),
		      "someone still inside, or no siege: nobody breaks in");
	}
	Check(walkers::DungeonWalkerEnded(skipped, 0, s, r, ev, 8, false) == 0 && skipped.walkersLeft == 2 &&
	          walkers::DungeonWalkersAt(skipped, 6, PoolPlace::Inside, true) == 2,
	      "one of them ended inside: two left");
	{
		DungeonSite over = Refuge(4);
		over.state = DungeonState::Besieged;
		over.walkersLeft = 3;
		over.before = 3;
		Check(walkers::DungeonWalkerEnded(over, 0, s, r, ev, 8, false) == 0 && over.walkersLeft == 2 && over.before == 2,
		      "never more before the door than walkers left");
	}
	walkers::DungeonWalkerEnded(skipped, 0, s, r, ev, 8, false);
	const UInt32 n = walkers::DungeonWalkerEnded(skipped, 0, s, r, ev, 8, false);
	Check(n == 1 && skipped.state == DungeonState::Refuge && skipped.before == 0 && skipped.walkersLeft == 0,
	      "the last ended: held, nobody before the door");

	DungeonSite away = Refuge(6);
	away.state = DungeonState::Besieged;
	away.walkersLeft = 6;
	away.before = 5;
	away.tollFrom = 0.0f;
	DungeonSiteSettings toll;
	toll.refugeeKillsPerHour = 2.0f;
	toll.lossPercentPerHour = 0;
	const UInt32 keep[] = {99};
	Rolls k{keep, 1};
	walkers::StepDungeonSite(away, 0, toll, 1.0f / 24.0f + 0.001f, false, k, ev, 8);
	Check(away.walkersLeft == 4 && away.before == 4,
	      "the toll away: the people's kills take those inside first, then those before the door");

	DungeonSite lost = Refuge(1);
	lost.state = DungeonState::Besieged;
	lost.walkersLeft = 8;
	lost.before = 4;
	Check(walkers::DungeonPersonDied(lost, 0, ev, 8) == 1 && lost.state == DungeonState::WalkerDungeon && lost.before == 0 &&
	          walkers::PoolPlaceFor(lost, 800.0f, true) == PoolPlace::Inside &&
	          walkers::DungeonWalkersAt(lost, 6, PoolPlace::Inside, true) == 6 &&
	          walkers::DungeonWalkersAt(lost, 6, PoolPlace::Outside, true) == 0,
	      "overrun: a walker dungeon - all of them in it, nobody before the door");
	DungeonSite quiet = Refuge(5);
	Check(walkers::PoolPlaceFor(quiet, 800.0f, true) == PoolPlace::Inside &&
	          walkers::DungeonWalkersAt(quiet, 6, PoolPlace::Outside, true) == 0 &&
	          walkers::DungeonWalkersAt(quiet, 6, PoolPlace::Inside, true) == 0,
	      "a quiet refuge: no walkers anywhere");
	const PoolPlace in = PoolPlace::Inside;
	const PoolPlace door = PoolPlace::Outside;
	Check(walkers::PoolAhead(door, door, 0, 4, 0, 8) == 4,
	      "the player before the door, those outside all out: those inside are put in ahead of him");
	Check(walkers::PoolAhead(door, door, 0, 6, 2, 3) == 3 && walkers::PoolAhead(door, door, 0, 6, 2, 0) == 0,
	      "into slots not in use only, as many as there are");
	Check(walkers::PoolAhead(door, door, 2, 4, 0, 8) == 0, "those outside first");
	Check(walkers::PoolAhead(door, door, 0, 4, 4, 8) == 0 && walkers::PoolAhead(door, door, 0, 0, 0, 8) == 0,
	      "everyone inside there already, or nobody inside at all: none");
	Check(walkers::PoolAhead(in, in, 0, 4, 0, 8) == 0 && walkers::PoolAhead(in, door, 0, 4, 0, 8) == 0,
	      "the place itself being filled: nothing ahead");
	Check(walkers::PoolSlotOrder(false, false, in, door, door) == 0 && walkers::PoolSlotOrder(false, false, door, in, in) == 0,
	      "the next walker takes a slot of the pool not in use first");
	Check(walkers::PoolSlotOrder(false, true, door, in, in) == 1 && walkers::PoolSlotOrder(false, true, in, door, door) == 1,
	      "then one lying ended where the player is not, raised again");
	Check(walkers::PoolSlotOrder(true, false, door, in, in) == 2 && walkers::PoolSlotOrder(true, false, in, door, door) == 2,
	      "then one alive at the other place (out of his sight; it counts on) - a WalkersAtOnce the pool cannot "
	      "hold twice");
	Check(walkers::PoolSlotOrder(false, true, door, door, door) == 3 && walkers::PoolSlotOrder(false, true, in, in, in) == 3,
	      "and only last one lying where he is (more walkers than the pool holds)");
	Check(walkers::PoolSlotOrder(true, false, in, in, in) < 0 && walkers::PoolSlotOrder(true, false, door, door, door) < 0,
	      "one alive at the place being filled: never");
	Check(walkers::PoolSlotOrder(false, true, door, in, door) == 3 && walkers::PoolSlotOrder(true, false, door, in, door) < 0,
	      "the place filled inside while he stands before the door (the last one outside ended): those lying before "
	      "him last, nobody alive taken from before his eyes");
}

void TestImage() {
	std::printf("The co-save image\n");
	DungeonSitesState st;
	st.target = 4;
	st.count = 2;
	st.sites[0] = Refuge(6);
	st.sites[0].candidate = 17;
	st.sites[0].dayFrom = 12.5f;
	st.sites[0].lastSeen = 11.25f;
	st.sites[1].candidate = 3;
	st.sites[1].state = DungeonState::Besieged;
	st.sites[1].people = 2;
	st.sites[1].waiting = 0;
	st.sites[1].lost = 3;
	st.sites[1].walkersLeft = 300;
	st.sites[1].before = 150;
	st.sites[1].journal = 20;
	st.sites[1].tollFrom = 13.0f;
	st.sites[1].carry = 0.5f;
	st.sites[1].seen = true;
	st.sites[1].cleared = 0x5;
	UInt8 image[walkers::kDungeonImageBytes];
	Check(walkers::DungeonImage(st, image, 10) == 0, "too small a buffer: nothing written");
	const UInt32 n = walkers::DungeonImage(st, image, sizeof image);
	Check(n == walkers::kDungeonImageBytes, "the whole image written");
	DungeonSitesState back;
	Check(walkers::ReadDungeonImage(back, image, n, 100), "read back");
	const DungeonSite& a = back.sites[0];
	const DungeonSite& b = back.sites[1];
	Check(back.count == 2 && back.target == 4, "the count and the target");
	Check(a.candidate == 17 && a.state == DungeonState::Refuge && a.people == 6 && a.dayFrom == 12.5f &&
	          a.lastSeen == 11.25f,
	      "a refuge as it was");
	Check(b.candidate == 3 && b.state == DungeonState::Besieged && b.people == 2 && b.lost == 3 &&
	          b.walkersLeft == 300 && b.tollFrom == 13.0f && b.carry == 0.5f && b.seen && b.cleared == 0x5,
	      "a siege as it was");
	Check(b.before == 150 && b.journal == 20 && a.before == 0, "those before its door and its journal's stage with it");
	{
		UInt8 old[walkers::kDungeonImageBytes];
		std::memcpy(old, image, sizeof old);
		old[8 + walkers::kDungeonSiteBytes + 11] = 0;  // a save of before: the byte unused
		DungeonSitesState before;
		Check(walkers::ReadDungeonImage(before, old, n, 100) && before.sites[1].before == 0 &&
		          before.sites[1].walkersLeft == 300,
		      "a save of before: nobody before the door - its siege stays inside");
		old[8 + walkers::kDungeonSiteBytes + 8] = 3;  // 3 walkers left
		old[8 + walkers::kDungeonSiteBytes + 9] = 0;
		old[8 + walkers::kDungeonSiteBytes + 11] = 9;
		Check(walkers::ReadDungeonImage(before, old, n, 100) && before.sites[1].before == 3,
		      "more before the door than walkers left (a damaged save): held to those left");
	}
	Check(!walkers::ReadDungeonImage(back, image, n - 1, 100) && back.count == 0, "cut short: empty, rolled anew");
	Check(walkers::ReadDungeonImage(back, image, n, 10) && back.count == 1 && back.sites[0].candidate == 3,
	      "a candidate past the ones there now (another build): that site dropped");
	image[4] = 99;
	Check(walkers::ReadDungeonImage(back, image, n, 100) && back.count <= walkers::kDungeonActiveMost,
	      "a count past the most: held to the most");
}

}  // namespace

int main() {
	TestImage();
	TestBeforeTheDoor();
	TestRolledKinds();
	TestWalkerDungeonAndCap();
	TestVariant();
	TestFill();
	TestAttack();
	TestSiegeAway();
	TestClockSetBack();
	TestNear();
	TestCleared();
	TestLeft();
	std::printf(g_failures == 0 ? "All passed\n" : "%d FAILED\n", g_failures);
	return g_failures == 0 ? 0 : 1;
}
