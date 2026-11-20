// Every flow of the necromancers' realms as numbers (docs/walkers-spec.md
// 4.23 B, build step 1; Nadi, 2026-11-20): the settings and the teams, the
// start spread over the world, the pace shared by the living, the bands and
// the new places due, a boss's fall, the last fall and the comeback, the
// co-save image.

#include "NecroLogic.h"

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
using walkers::DungeonKind;
using walkers::NecroEvent;
using walkers::NecroEventKind;
using walkers::NecroSettings;
using walkers::NecroState;

// Six places on a line, 10000 apart (x 0, 10000 ... 50000), and one far
// off at 200000.
const DungeonCandidate kPlaces[] = {
    {"A", DungeonKind::Cave, {1, 0, 0, 0}, 1, 0x101, 0x201, false, 0.0f, 0.0f},
    {"B", DungeonKind::Cave, {2, 0, 0, 0}, 1, 0x102, 0x202, false, 10000.0f, 0.0f},
    {"C", DungeonKind::Fort, {3, 0, 0, 0}, 1, 0x103, 0x203, false, 20000.0f, 0.0f},
    {"D", DungeonKind::Mine, {4, 0, 0, 0}, 1, 0x104, 0x204, false, 30000.0f, 0.0f},
    {"E", DungeonKind::Ayleid, {5, 0, 0, 0}, 1, 0x105, 0x205, false, 40000.0f, 0.0f},
    {"F", DungeonKind::Cave, {6, 0, 0, 0}, 1, 0x106, 0x206, false, 50000.0f, 0.0f},
    {"G", DungeonKind::Fort, {7, 0, 0, 0}, 1, 0x107, 0x207, false, 200000.0f, 0.0f},
};
constexpr UInt32 kCount = sizeof(kPlaces) / sizeof(kPlaces[0]);

// A roll that answers the given numbers in turn, then 0.
struct Rolls {
	const UInt32* values;
	UInt32 count;
	UInt32 at = 0;
	UInt32 operator()() { return at < count ? values[at++] : 0u; }
};

void AllFree(bool* free) {
	for (UInt32 i = 0; i < kCount; ++i) {
		free[i] = true;
	}
}

UInt32 CountKind(const NecroEvent* ev, UInt32 n, NecroEventKind kind) {
	UInt32 k = 0;
	for (UInt32 i = 0; i < n; ++i) {
		k += ev[i].kind == kind ? 1u : 0u;
	}
	return k;
}

void TestTeams() {
	std::printf("Teams\n");
	UInt8 t[walkers::kNecroBossesMost] = {};
	walkers::ParseNecroTeams("1,2,3,4,5", t);
	Check(t[0] == 1 && t[1] == 2 && t[4] == 5, "the default: every boss a team of his own");
	walkers::ParseNecroTeams("1,1,1,1,1", t);
	Check(t[0] == 1 && t[2] == 1 && t[4] == 1, "1,1,1,1,1: all allied");
	walkers::ParseNecroTeams(" 1, 1 ,2,3,3", t);
	Check(t[0] == 1 && t[1] == 1 && t[2] == 2 && t[3] == 3 && t[4] == 3, "1,1,2,3,3: some allied, spaces trimmed");
	walkers::ParseNecroTeams("2,2", t);
	Check(t[0] == 2 && t[1] == 2 && t[2] == 3 && t[3] == 4 && t[4] == 5, "fewer entries: the rest a team of their own each");
	walkers::ParseNecroTeams("", t);
	Check(t[0] == 1 && t[4] == 5, "empty: the default");
	walkers::ParseNecroTeams("x,0,300", t);
	Check(t[0] == 1 && t[1] == 2 && t[2] == 3, "junk, 0 or above 255: that boss's own team");
	Check(walkers::NecroAllied(1, 1) && !walkers::NecroAllied(1, 2), "the same number allies, another enemies");
}

void TestPace() {
	std::printf("The pace shared by the living (decided 3, 9)\n");
	NecroSettings s;  // 3..5, middle 4, 1 band a day
	NecroState st;
	st.count = 4;
	for (UInt32 b = 0; b < 4; ++b) {
		st.bosses[b].alive = true;
	}
	Check(std::fabs(walkers::NecroBandsPerDay(s, st) - 1.0f) < 1e-4f, "4 alive at the middle: 1 band a day each");
	st.bosses[2].alive = false;
	st.bosses[3].alive = false;
	Check(std::fabs(walkers::NecroBandsPerDay(s, st) - 2.0f) < 1e-4f, "two of four killed: the two left double their pace");
	Check(std::fabs(walkers::NecroNewPlaceDays(s, st) - 1.5f) < 1e-4f, "and take a new place every 1.5 days, not 3");
	s.paceByLiving = false;
	Check(std::fabs(walkers::NecroBandsPerDay(s, st) - 1.0f) < 1e-4f, "PaceByLiving=0: a death changes nothing (4 rose)");
	s.paceByLiving = true;
	st.bosses[1].alive = false;
	Check(std::fabs(walkers::NecroBandsPerDay(s, st) - 4.0f) < 1e-4f, "one boss alone: 4 bands a day (the fewer, the more aggressive)");
	st.bosses[0].alive = false;
	Check(walkers::NecroBandsPerDay(s, st) == 0.0f && walkers::NecroNewPlaceDays(s, st) == 0.0f, "none alive: no pace");
	NecroState three;
	three.count = 3;
	for (UInt32 b = 0; b < 3; ++b) {
		three.bosses[b].alive = true;
	}
	Check(std::fabs(walkers::NecroBandsPerDay(s, three) - 4.0f / 3.0f) < 1e-4f, "three rose: 4/3 bands a day each");
	s.newPlaceEveryDays = 0.0f;
	Check(walkers::NecroNewPlaceDays(s, three) == 0.0f, "NewPlaceEveryDays=0: the realms never grow");
	s.bandsPerDay = 0.0f;
	Check(walkers::NecroBandsPerDay(s, three) == 0.0f, "BandsPerDay=0: no bands");
}

void TestHomes() {
	std::printf("The start spread over the world (read 3)\n");
	bool free[kCount];
	AllFree(free);
	UInt16 homes[walkers::kNecroBossesMost] = {};
	const UInt32 first[] = {0};  // A first, then the farthest
	Rolls r{first, 1};
	UInt32 n = walkers::PickNecroHomes(kPlaces, kCount, free, 2, r, homes);
	Check(n == 2 && homes[0] == 0 && homes[1] == 6, "the first at random (A), the next the farthest free (G)");
	const UInt32 pickSecondFarthest[] = {0, 1};
	Rolls r2{pickSecondFarthest, 2};
	n = walkers::PickNecroHomes(kPlaces, kCount, free, 2, r2, homes);
	Check(n == 2 && homes[1] == 5, "a roll among the few farthest: the second farthest (F) when rolled");
	const UInt32 third[] = {0, 0, 0};
	Rolls r3{third, 3};
	n = walkers::PickNecroHomes(kPlaces, kCount, free, 3, r3, homes);
	Check(n == 3 && homes[2] == 5, "the third: the farthest from its nearest taken home (F: 50000 from A)");
	free[6] = false;
	Rolls r4{first, 1};
	n = walkers::PickNecroHomes(kPlaces, kCount, free, 2, r4, homes);
	Check(n == 2 && homes[1] == 5, "a place not free (G a held place's) is never a home");
	bool none[kCount] = {};
	none[2] = true;
	Rolls r5{first, 1};
	n = walkers::PickNecroHomes(kPlaces, kCount, none, 3, r5, homes);
	Check(n == 1 && homes[0] == 2, "fewer free places than bosses: as many as there are");
	bool empty[kCount] = {};
	Rolls r6{first, 1};
	Check(walkers::PickNecroHomes(kPlaces, kCount, empty, 3, r6, homes) == 0, "no free place: no home");
}

void TestStart() {
	std::printf("The start\n");
	NecroSettings s;
	s.on = true;
	walkers::ParseNecroTeams("1,1,2", s.teams);
	NecroState st;
	bool free[kCount];
	AllFree(free);
	const UInt32 rolls[] = {1, 0, 0, 0};  // count: 3 + 1 % 3 = 4; then homes
	Rolls r{rolls, 4};
	NecroEvent ev[16];
	UInt32 n = walkers::StepNecromancers(st, s, kPlaces, kCount, free, 2.0f, r, ev, 16);
	Check(st.count == 4 && CountKind(ev, n, NecroEventKind::Rose) == 4, "BossesMin..Max rolled (4): four rise");
	Check(st.bosses[0].team == 1 && st.bosses[1].team == 1 && st.bosses[2].team == 2 && st.bosses[3].team == 4,
	      "their teams from Teams, in the order they rose (the fourth his own)");
	Check(st.bosses[0].alive && st.bosses[0].places == 1 && st.bosses[0].bandFrom == 2.0f,
	      "alive, his home his one place, the clocks from now");
	n = walkers::StepNecromancers(st, s, kPlaces, kCount, free, 2.0f, r, ev, 16);
	Check(n == 0, "risen once: no second start");
	NecroSettings off;
	NecroState idle;
	Check(walkers::StepNecromancers(idle, off, kPlaces, kCount, free, 2.0f, r, ev, 16) == 0 && idle.count == 0,
	      "Mode 0: nobody rises");
	NecroState none;
	bool empty[kCount] = {};
	Rolls r2{rolls, 4};
	n = walkers::StepNecromancers(none, s, kPlaces, kCount, empty, 2.0f, r2, ev, 16);
	Check(n == 0 && none.count == 0, "no free place: nobody rises yet (tried again later)");
	s.bossesMin = 9;
	s.bossesMax = 9;
	NecroState capped;
	Rolls r3{rolls, 4};
	walkers::StepNecromancers(capped, s, kPlaces, kCount, free, 2.0f, r3, ev, 16);
	Check(capped.count == walkers::kNecroBossesMost, "more than five asked: five");
}

NecroState Rose(UInt32 count, float now) {
	NecroState st;
	st.count = count;
	for (UInt32 b = 0; b < count; ++b) {
		st.bosses[b].home = static_cast<UInt16>(b);
		st.bosses[b].team = static_cast<UInt8>(b + 1);
		st.bosses[b].alive = true;
		st.bosses[b].places = 1;
		st.bosses[b].bandFrom = now;
		st.bosses[b].placeFrom = now;
	}
	return st;
}

void TestBandsDue() {
	std::printf("Bands due\n");
	NecroSettings s;
	s.on = true;
	s.newPlaceEveryDays = 0.0f;
	NecroState st = Rose(4, 10.0f);
	bool free[kCount];
	AllFree(free);
	Rolls r{nullptr, 0};
	NecroEvent ev[32];
	UInt32 n = walkers::StepNecromancers(st, s, kPlaces, kCount, free, 10.5f, r, ev, 32);
	Check(CountKind(ev, n, NecroEventKind::BandDue) == 0, "half a day at 1 a day: none yet");
	n = walkers::StepNecromancers(st, s, kPlaces, kCount, free, 11.0f, r, ev, 32);
	Check(CountKind(ev, n, NecroEventKind::BandDue) == 4, "a whole day: one band from each of the four");
	st.bosses[1].alive = false;
	st.bosses[2].alive = false;
	st.bosses[3].alive = false;
	n = walkers::StepNecromancers(st, s, kPlaces, kCount, free, 12.0f, r, ev, 32);
	Check(CountKind(ev, n, NecroEventKind::BandDue) == 4 && ev[0].boss == 0, "one boss alone, a day: four bands, his");
	n = walkers::StepNecromancers(st, s, kPlaces, kCount, free, 30.0f, r, ev, 32);
	Check(CountKind(ev, n, NecroEventKind::BandDue) == walkers::kNecroBandsAtOnceMost,
	      "18 days away: at most kNecroBandsAtOnceMost at once, the rest dropped");
	n = walkers::StepNecromancers(st, s, kPlaces, kCount, free, 29.0f, r, ev, 32);
	Check(CountKind(ev, n, NecroEventKind::BandDue) == 0 && st.bosses[0].bandFrom == 29.0f,
	      "the clock set back: nothing due, counted from now");
}

void TestPlacesDue() {
	std::printf("The realm grows (decided 4; read 3)\n");
	NecroSettings s;
	s.on = true;
	s.bandsPerDay = 0.0f;
	s.bossesMin = 2;
	s.bossesMax = 2;  // middle 2: two alive, a new place every 3 days
	NecroState st = Rose(2, 0.0f);
	st.bosses[0].home = 0;  // A
	st.bosses[1].home = 5;  // F
	bool free[kCount];
	AllFree(free);
	free[0] = free[5] = false;
	Rolls r{nullptr, 0};
	NecroEvent ev[16];
	UInt32 n = walkers::StepNecromancers(st, s, kPlaces, kCount, free, 2.9f, r, ev, 16);
	Check(CountKind(ev, n, NecroEventKind::PlaceDue) == 0, "before 3 days: no new place");
	n = walkers::StepNecromancers(st, s, kPlaces, kCount, free, 3.0f, r, ev, 16);
	Check(CountKind(ev, n, NecroEventKind::PlaceDue) == 2, "3 days: each takes one");
	Check(ev[0].boss == 0 && ev[0].place == 1 && ev[1].boss == 1 && ev[1].place == 4,
	      "the nearest free in his share: A takes B, F takes E");
	free[1] = free[4] = false;
	walkers::NecroPlaceTaken(st, 0);
	walkers::NecroPlaceTaken(st, 1);
	n = walkers::StepNecromancers(st, s, kPlaces, kCount, free, 6.0f, r, ev, 16);
	Check(n == 2 && ev[0].place == 2 && ev[1].place == 3, "then C and D: nearer to their own home than to the other's");
	free[2] = free[3] = false;
	walkers::NecroPlaceTaken(st, 0);
	walkers::NecroPlaceTaken(st, 1);
	n = walkers::StepNecromancers(st, s, kPlaces, kCount, free, 9.0f, r, ev, 16);
	Check(n == 1 && ev[0].boss == 1 && ev[0].place == 6, "A's share full; G is F's (nearer F than A)");
	NecroState capped = Rose(1, 0.0f);
	capped.bosses[0].places = static_cast<UInt8>(s.realmMost);
	bool all[kCount];
	AllFree(all);
	n = walkers::StepNecromancers(capped, s, kPlaces, kCount, all, 30.0f, r, ev, 16);
	Check(CountKind(ev, n, NecroEventKind::PlaceDue) == 0, "RealmMost places held: no more");
	NecroState two = Rose(2, 0.0f);
	two.bosses[0].home = 2;
	two.bosses[1].home = 3;
	bool same[kCount] = {};
	same[1] = true;  // B only
	n = walkers::StepNecromancers(two, s, kPlaces, kCount, same, 3.0f, r, ev, 16);
	Check(n == 1 && ev[0].boss == 0, "one free place for two in one step: the nearer boss's, never both");
	bool dead[kCount];
	AllFree(dead);
	NecroState gone = Rose(2, 0.0f);
	gone.bosses[1].alive = false;
	gone.bosses[0].home = 0;
	gone.bosses[1].home = 6;
	dead[0] = dead[6] = false;
	n = walkers::StepNecromancers(gone, s, kPlaces, kCount, dead, 3.0f, r, ev, 16);
	Check(n >= 1 && ev[0].boss == 0, "a fallen boss takes nothing; his share is no wall for the living");
}

void TestFall() {
	std::printf("A boss falls (decided 7, 8)\n");
	NecroSettings s;
	s.on = true;
	NecroState st = Rose(2, 0.0f);
	NecroEvent ev[8];
	UInt32 n = walkers::NecroBossFell(st, s, 0, 4.0f, ev, 8);
	Check(n == 1 && ev[0].kind == NecroEventKind::Fell && !st.bosses[0].alive && st.comeBackAt < 0.0f,
	      "one of two: he falls, the other goes on");
	n = walkers::NecroBossFell(st, s, 0, 4.0f, ev, 8);
	Check(n == 0, "fallen already: nothing");
	n = walkers::NecroBossFell(st, s, 1, 5.0f, ev, 8);
	Check(n == 2 && ev[1].kind == NecroEventKind::AllFell && std::fabs(st.comeBackAt - 8.0f) < 1e-4f,
	      "the last: all fell, a new set due after ComeBackDays (3)");
	bool free[kCount];
	AllFree(free);
	const UInt32 rolls[] = {0, 0, 0, 0};
	Rolls r{rolls, 4};
	NecroEvent more[16];
	n = walkers::StepNecromancers(st, s, kPlaces, kCount, free, 7.9f, r, more, 16);
	Check(n == 0, "before the comeback: nothing");
	n = walkers::StepNecromancers(st, s, kPlaces, kCount, free, 8.0f, r, more, 16);
	Check(more[0].kind == NecroEventKind::CameBack && CountKind(more, n, NecroEventKind::Rose) == 3 && st.count == 3,
	      "the comeback: a new set rises (3)");
	Check(st.comeBackAt < 0.0f && st.bosses[0].alive, "the comeback done");
	NecroSettings once;
	once.on = true;
	once.comeBack = false;
	NecroState last = Rose(1, 0.0f);
	n = walkers::NecroBossFell(last, once, 0, 1.0f, ev, 8);
	Check(n == 2 && last.over && last.comeBackAt < 0.0f, "ComeBack=0: it is over");
	Rolls r2{rolls, 4};
	Check(walkers::StepNecromancers(last, once, kPlaces, kCount, free, 50.0f, r2, more, 16) == 0, "and stays over");
	Check(walkers::NecroBossFell(last, once, 7, 1.0f, ev, 8) == 0, "a boss index past the count: nothing");
}

void TestPlaces() {
	std::printf("A realm's places counted\n");
	NecroState st = Rose(1, 0.0f);
	walkers::NecroPlaceTaken(st, 0);
	Check(st.bosses[0].places == 2, "one taken: two");
	walkers::NecroPlaceLost(st, 0);
	walkers::NecroPlaceLost(st, 0);
	walkers::NecroPlaceLost(st, 0);
	Check(st.bosses[0].places == 0, "lost below none: none");
	walkers::NecroPlaceTaken(st, 9);
	Check(st.bosses[0].places == 0, "a boss past the count: nothing");
}

void TestImage() {
	std::printf("The co-save image\n");
	NecroState st = Rose(3, 2.5f);
	st.bosses[1].alive = false;
	st.bosses[2].home = 77;
	st.bosses[2].team = 9;
	st.bosses[2].bandCarry = 0.75f;
	st.bosses[2].places = 4;
	st.comeBackAt = 12.0f;
	UInt8 image[walkers::kNecroImageBytes];
	Check(walkers::NecroImage(st, image, sizeof image) == walkers::kNecroImageBytes, "written whole");
	NecroState back;
	Check(walkers::ReadNecroImage(back, image, sizeof image, 91), "read back");
	Check(back.count == 3 && !back.bosses[1].alive && back.bosses[2].home == 77 && back.bosses[2].team == 9 &&
	          back.bosses[2].places == 4 && std::fabs(back.bosses[2].bandCarry - 0.75f) < 1e-6f &&
	          std::fabs(back.bosses[0].bandFrom - 2.5f) < 1e-6f && std::fabs(back.comeBackAt - 12.0f) < 1e-6f,
	      "every field as it was");
	NecroState shorter;
	Check(!walkers::ReadNecroImage(shorter, image, 8, 91) && shorter.count == 0, "too short: nothing, rolled anew");
	NecroState other;
	Check(walkers::ReadNecroImage(other, image, sizeof image, 50) && !other.bosses[2].alive,
	      "a home past this build's candidates: that boss counted fallen");
	st.over = true;
	walkers::NecroImage(st, image, sizeof image);
	walkers::ReadNecroImage(back, image, sizeof image, 91);
	Check(back.over, "over stays over");
	Check(walkers::NecroImage(st, image, 4) == 0, "no room: nothing written");
}

void TestWalkers() {
	std::printf("A hotspot's walkers\n");
	NecroSettings s;  // 16..30
	Check(walkers::NecroHotspotWalkers(s, 0) == 16 && walkers::NecroHotspotWalkers(s, 14) == 30 &&
	          walkers::NecroHotspotWalkers(s, 15) == 16,
	      "WalkersMin..WalkersMax by the roll");
	Check(walkers::NecroBandSize(s, 5) == 9 && walkers::NecroBandSize(s, 0) == 4, "a band BandSizeMin..BandSizeMax");
}

}  // namespace

int main() {
	TestTeams();
	TestPace();
	TestHomes();
	TestStart();
	TestBandsDue();
	TestPlacesDue();
	TestFall();
	TestPlaces();
	TestImage();
	TestWalkers();
	std::printf(g_failures == 0 ? "All passed\n" : "%d FAILED\n", g_failures);
	return g_failures == 0 ? 0 : 1;
}
