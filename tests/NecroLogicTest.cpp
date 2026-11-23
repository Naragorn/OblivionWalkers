// Every flow of the necromancers' realms as numbers (docs/walkers-spec.md
// 4.23 B, build step 1; Nadi, 2026-11-20): the settings and the teams, the
// start spread over the world, the pace shared by the living, the bands and
// the new places due, a boss's fall, the last fall and the comeback, the
// co-save image.

#include "NecroLogic.h"

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

UInt32 CountBand(const walkers::NecroBandEvent* ev, UInt32 n, walkers::NecroBandEventKind kind) {
	UInt32 k = 0;
	for (UInt32 i = 0; i < n; ++i) {
		k += ev[i].kind == kind ? 1u : 0u;
	}
	return k;
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
	// [Necromancers] TestHome: the first home given, the others by the rule
	AllFree(free);
	Rolls r7{first, 1};
	n = walkers::PickNecroHomes(kPlaces, kCount, free, 2, r7, homes, 2);
	Check(n == 2 && homes[0] == 2 && homes[1] != 2 && r7.at == 1, "the first home given (C), no roll for it; the next by the rule");
	free[2] = false;
	Rolls r8{first, 1};
	n = walkers::PickNecroHomes(kPlaces, kCount, free, 1, r8, homes, 2);
	Check(n == 1 && homes[0] == 0, "the given one not free: the first at random as ever");
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
	walkers::NecroBands bands;
	bands.count = 2;
	bands.bands[1].boss = 2;
	bands.bands[1].team = 7;
	bands.bands[1].city = 3;
	bands.bands[1].size = 9;
	bands.bands[1].x = -1234.5f;
	bands.bands[1].y = 6789.25f;
	st.siegeDay = 6;
	st.siegePoints[2][1] = 35.5f;
	st.siegeGate[2] = 5;
	UInt8 image[walkers::kNecroImageBytes];
	Check(walkers::NecroImage(st, bands, image, sizeof image) == walkers::kNecroImageBytes, "written whole");
	NecroState back;
	walkers::NecroBands backBands;
	Check(walkers::ReadNecroImage(back, backBands, image, sizeof image, 91), "read back");
	const walkers::NecroBand& b1 = backBands.bands[1];
	Check(backBands.count == 2 && b1.boss == 2 && b1.team == 7 && b1.city == 3 && b1.size == 9 && b1.x == -1234.5f &&
	          b1.y == 6789.25f,
	      "the far bands with it");
	Check(back.count == 3 && !back.bosses[1].alive && back.bosses[2].home == 77 && back.bosses[2].team == 9 &&
	          back.bosses[2].places == 4 && std::fabs(back.bosses[2].bandCarry - 0.75f) < 1e-6f &&
	          std::fabs(back.bosses[0].bandFrom - 2.5f) < 1e-6f && std::fabs(back.comeBackAt - 12.0f) < 1e-6f,
	      "every field as it was");
	Check(back.siegeDay == 6 && std::fabs(back.siegePoints[2][1] - 35.5f) < 1e-6f && back.siegeGate[2] == 5,
	      "their siege points with it (version 3)");
	NecroState v2;
	walkers::NecroBands v2Bands;
	Check(walkers::ReadNecroImage(v2, v2Bands, image, walkers::kNecroImageBytesV2, 91) && v2.count == 3 &&
	          v2Bands.count == 2 && v2.siegeDay == -1 && v2.siegePoints[2][1] == 0.0f,
	      "a version 2 image (no siege points): the realms read, the points none");
	NecroState shorter;
	walkers::NecroBands shorterBands;
	shorterBands.count = 3;
	Check(!walkers::ReadNecroImage(shorter, shorterBands, image, 8, 91) && shorter.count == 0 && shorterBands.count == 0,
	      "too short: nothing, rolled anew");
	NecroState other;
	Check(walkers::ReadNecroImage(other, backBands, image, sizeof image, 50) && !other.bosses[2].alive,
	      "a home past this build's candidates: that boss counted fallen");
	st.over = true;
	walkers::NecroImage(st, bands, image, sizeof image);
	walkers::ReadNecroImage(back, backBands, image, sizeof image, 91);
	Check(back.over, "over stays over");
	Check(walkers::NecroImage(st, bands, image, 4) == 0, "no room: nothing written");
}

void TestBossActors() {
	std::printf("The bosses and their chest\n");
	Check(walkers::NecroBossActor(true, 0) == 0x0004B946 && walkers::NecroBossActor(true, 7) == 0x0004B948,
	      "at a home: one of LL1NecromancerBoss100's four by the roll");
	Check(walkers::NecroBossActor(false, 4) == 0x0001E5E5 && walkers::NecroBossActor(false, 19) == 0x00069ADD,
	      "at a place he took: one of LL1Necromancer100's ten");
	Check(walkers::NecroBossChest(DungeonKind::Ayleid) == 0x0003753F && walkers::NecroBossChest(DungeonKind::Fort) == 0x00037542 &&
	          walkers::NecroBossChest(DungeonKind::Cave) == 0x00037542,
	      "the chest: the Ayleid one in a ruin, the fort's elsewhere");
}

void TestBands() {
	std::printf("The bands far away (step 4)\n");
	NecroSettings s;  // 10000 units an hour, near within 6000
	const walkers::NecroCity cities[] = {{100000.0f, 0.0f}, {-100000.0f, 0.0f}};
	walkers::NecroBands st;
	walkers::NecroBandEvent ev[8];
	UInt32 n = walkers::NecroBandFormed(st, s, 1, 2, 6, 40000.0f, 0.0f, cities, 2, -200000.0f, 0.0f, ev, 8);
	Check(n == 1 && ev[0].kind == walkers::NecroBandEventKind::Formed && st.count == 1 && st.bands[0].city == 0 &&
	          st.bands[0].team == 2 && st.bands[0].size == 6,
	      "formed at his home, toward the nearest city, far from the player: a number");
	n = walkers::StepNecroBands(st, s, cities, 2, 3.0f, -200000.0f, 0.0f, ev, 8);
	Check(n == 0 && st.bands[0].x == 70000.0f, "three hours: 30000 units nearer its city");
	n = walkers::StepNecroBands(st, s, cities, 2, 1.0f, 85000.0f, 0.0f, ev, 8);
	Check(n == 1 && ev[0].kind == walkers::NecroBandEventKind::Near && st.count == 0 && ev[0].band.x == 80000.0f,
	      "the player 5000 from it: Near - the caller places its walkers");
	walkers::NecroBandFormed(st, s, 0, 1, 4, 95000.0f, 0.0f, cities, 2, -200000.0f, 0.0f, ev, 8);
	n = walkers::StepNecroBands(st, s, cities, 2, 1.0f, -200000.0f, 0.0f, ev, 8);
	Check(n == 1 && ev[0].kind == walkers::NecroBandEventKind::Arrived && st.count == 0 && ev[0].band.city == 0,
	      "at its city: Arrived - the city's siege pressure");
	n = walkers::NecroBandFormed(st, s, 0, 1, 4, 1000.0f, 0.0f, cities, 2, 0.0f, 0.0f, ev, 8);
	Check(n == 2 && ev[1].kind == walkers::NecroBandEventKind::Near && st.count == 0, "formed near the player: Near at once");
	for (UInt32 k = 0; k < walkers::kNecroBandsMost; ++k) {
		walkers::NecroBandFormed(st, s, 0, 1, 4, 0.0f, 50000.0f, cities, 2, -200000.0f, 0.0f, ev, 8);
	}
	n = walkers::NecroBandFormed(st, s, 0, 1, 4, 0.0f, 50000.0f, cities, 2, -200000.0f, 0.0f, ev, 8);
	Check(n == 2 && ev[1].kind == walkers::NecroBandEventKind::Dropped && st.count == walkers::kNecroBandsMost,
	      "the most on their way: one more dropped");
	walkers::NecroBands none;
	Check(walkers::NecroBandFormed(none, s, 0, 1, 4, 0.0f, 0.0f, cities, 0, 0.0f, 0.0f, ev, 8) == 0 &&
	          walkers::StepNecroBands(none, s, cities, 0, 1.0f, 0.0f, 0.0f, ev, 8) == 0,
	      "no city known: nothing");
	walkers::NecroBands still;
	walkers::NecroBandFormed(still, s, 0, 1, 4, 50000.0f, 0.0f, cities, 2, -200000.0f, 0.0f, ev, 8);
	Check(walkers::StepNecroBands(still, s, cities, 2, -2.0f, -200000.0f, 0.0f, ev, 8) == 0 && still.bands[0].x == 50000.0f,
	      "the clock set back: no step");
}

void TestTeamsFight() {
	std::printf("Who fights whom (step 5; decided 5, 11)\n");
	Check(!walkers::NecroFightStops(1, 2), "team 1 against team 2 (enemies): fights on");
	Check(walkers::NecroFightStops(1, 1), "against its own team (or its own necromancer): lets go");
	Check(walkers::NecroFightStops(3, 0) && walkers::NecroFightStops(0, 3), "a team's and a free walker: let go (decided 11)");
	Check(!walkers::NecroFightStops(0, 0), "free walkers among themselves: as ever");
	Check(walkers::NecroSetOn(1, 2, false, 1000.0f, walkers::kNecroSetOnUnits), "an enemy's walker within reach: set on");
	Check(!walkers::NecroSetOn(1, 2, true, 1000.0f, walkers::kNecroSetOnUnits), "already fighting: not set on another");
	Check(!walkers::NecroSetOn(1, 2, false, 2000.0f, walkers::kNecroSetOnUnits), "too far: not");
	Check(!walkers::NecroSetOn(1, 1, false, 100.0f, walkers::kNecroSetOnUnits) &&
	          !walkers::NecroSetOn(0, 2, false, 100.0f, walkers::kNecroSetOnUnits) &&
	          !walkers::NecroSetOn(2, 0, false, 100.0f, walkers::kNecroSetOnUnits),
	      "an ally, a free walker, or none's: never set on");
	Check(!walkers::NecroSetOn(1, 2, false, -1.0f, walkers::kNecroSetOnUnits), "not measurable (another cell): not");
	// decided 12: with Daedra Mode 2 a necromancer and a Daedra leave each other alone - both sides
	Check(walkers::NecroDaedraLetGo(true, true, true), "Daedra Mode 2, a necromancer and a Daedra: let go (either side)");
	Check(!walkers::NecroDaedraLetGo(false, true, true), "Daedra Mode 0 or 1: they fight as ever");
	Check(!walkers::NecroDaedraLetGo(true, false, true), "not the necromancer (his walkers: the walkers' own rule)");
	Check(!walkers::NecroDaedraLetGo(true, true, false), "the necromancer against one not a Daedra: fights on");
}

void TestSieges() {
	std::printf("Their sieges: one necromancer or his allies, or none (decided 13)\n");
	using walkers::NecroSiege;
	walkers::NecroSettings s;  // SiegeJoinPercent 50
	NecroState st = Rose(3, 1.0f);
	st.bosses[1].team = 1;  // bosses 0 and 1 allies (team 1), boss 2 team 3
	const bool open[walkers::kNecroCities] = {true, true, true, true, true, true, true, true};
	NecroSiege out[4];
	const UInt32 none[] = {0};
	Rolls zero{none, 1};
	Check(walkers::NecroSiegesDue(st, s, 1.2f, open, zero, out, 4) == 0 && st.siegeDay == 1, "the first look: the day noted, nothing");
	// boss 0's band reached Bruma (city 2) at its East Gate (row 4): 50 points
	walkers::NecroBandArrived(st, 0, 2, 4, 50.0f);
	walkers::NecroBandArrived(st, 0, 2, 4, 70.0f);
	Check(std::fabs(st.siegePoints[2][0] - 100.0f) < 1e-4f && st.siegeGate[2] == 4, "points add up to 100 at most, the gate kept");
	Check(walkers::NecroSiegesDue(st, s, 1.9f, open, zero, out, 4) == 0, "the same day: nothing");
	// a new day: the roll 0 < 100 - besieged; boss 1 (his ally, no band there) joins at the roll 0 < 50
	const UInt32 join[] = {0, 0, 0};
	Rolls j{join, 3};
	UInt32 n = walkers::NecroSiegesDue(st, s, 2.1f, open, j, out, 4);
	Check(n == 1 && out[0].city == 2 && out[0].gate == 4 && out[0].team == 1 && out[0].bossMask == 3 && out[0].bosses == 2,
	      "team 1 besieges Bruma at the gate its band reached: boss 0, and his ally joins - together");
	Check(st.siegePoints[2][0] == 0.0f, "the points spent");
	// alone: the ally does not join (roll 60 >= 50)
	walkers::NecroBandArrived(st, 0, 2, 4, 100.0f);
	const UInt32 alone[] = {0, 0, 60};
	Rolls a{alone, 3};
	n = walkers::NecroSiegesDue(st, s, 3.1f, open, a, out, 4);
	Check(n == 1 && out[0].bossMask == 1 && out[0].bosses == 1, "alone: his ally stays home");
	// both marched: both besiege, whatever the join roll
	walkers::NecroBandArrived(st, 0, 2, 4, 30.0f);
	walkers::NecroBandArrived(st, 1, 2, 5, 30.0f);
	const UInt32 both[] = {0, 0, 99};
	Rolls b{both, 3};
	n = walkers::NecroSiegesDue(st, s, 4.1f, open, b, out, 4);
	Check(n == 1 && out[0].bossMask == 3 && out[0].gate == 5, "both marched there: both besiege, 30 + 30 points, the last gate reached");
	// none: the roll above the points
	walkers::NecroBandArrived(st, 0, 2, 4, 10.0f);
	const UInt32 miss[] = {0, 50};
	Rolls m{miss, 2};
	Check(walkers::NecroSiegesDue(st, s, 5.1f, open, m, out, 4) == 0 && st.siegePoints[2][0] > 0.0f,
	      "not at all: the roll 50 above 10 points - the points stay");
	// a city not open (besieged or pausing): none, the points stay
	bool shut[walkers::kNecroCities] = {true, true, false, true, true, true, true, true};
	Rolls z2{none, 1};
	Check(walkers::NecroSiegesDue(st, s, 6.1f, shut, z2, out, 4) == 0 && st.siegePoints[2][0] > 0.0f, "Bruma besieged already: none");
	// the dead: no points of theirs count, none of them joins
	NecroState d = Rose(2, 1.0f);
	d.bosses[1].team = 1;
	d.siegeDay = 1;
	walkers::NecroBandArrived(d, 1, 2, 4, 100.0f);
	d.bosses[1].alive = false;
	Rolls z3{none, 1};
	Check(walkers::NecroSiegesDue(d, s, 2.5f, open, z3, out, 4) == 0, "a fallen necromancer's points: no siege");
	walkers::NecroBandArrived(d, 0, 2, 4, 100.0f);
	s.siegeJoinPercent = 100;
	const UInt32 all[] = {0, 0, 0};
	Rolls al{all, 3};
	n = walkers::NecroSiegesDue(d, s, 3.5f, open, al, out, 4);
	Check(n == 1 && out[0].bossMask == 1, "a fallen ally never joins");
	// two teams at one city: one siege a day there
	NecroState two = Rose(2, 1.0f);  // teams 1 and 2
	two.siegeDay = 1;
	walkers::NecroBandArrived(two, 0, 2, 4, 100.0f);
	walkers::NecroBandArrived(two, 1, 2, 5, 100.0f);
	const UInt32 zeros[] = {0, 0, 0, 0, 0, 0};
	Rolls zs{zeros, 6};
	Check(walkers::NecroSiegesDue(two, s, 2.5f, open, zs, out, 4) == 1, "two enemy teams before Bruma: one siege, one team");
	// the points fade
	NecroState f = Rose(1, 1.0f);
	walkers::NecroBandArrived(f, 0, 3, 6, 20.0f);
	walkers::NecroSiegeFade(f, 2.0f, 5.0f);
	Check(std::fabs(f.siegePoints[3][0] - 10.0f) < 1e-4f, "BandSiegeFadePerDay 5 over two days: 20 down to 10");
	walkers::NecroSiegeFade(f, 10.0f, 5.0f);
	Check(f.siegePoints[3][0] == 0.0f, "never below none");
	Check(walkers::NecroCityOfGate(4) == 2 && walkers::NecroCityOfGate(0) == 0, "a gate row's city");
}

void TestTestFight() {
	std::printf("The harness's TestFight switch (INI only)\n");
	Check(walkers::ParseNecroTestFight("") == walkers::NecroTestFight::Off &&
	          walkers::ParseNecroTestFight(nullptr) == walkers::NecroTestFight::Off,
	      "empty or none: off");
	Check(walkers::ParseNecroTestFight("ally") == walkers::NecroTestFight::Ally &&
	          walkers::ParseNecroTestFight("Daedra") == walkers::NecroTestFight::Daedra &&
	          walkers::ParseNecroTestFight(" FAR ") == walkers::NecroTestFight::Far &&
	          walkers::ParseNecroTestFight("walkers") == walkers::NecroTestFight::Walkers &&
	          walkers::ParseNecroTestFight("Summon") == walkers::NecroTestFight::Summon,
	      "ally, daedra, far, walkers, summon - in any case, spaces around ignored");
	Check(walkers::ParseNecroTestFight("allies") == walkers::NecroTestFight::Off, "anything else: off");
	Check(std::strcmp(walkers::NecroTestFightName(walkers::NecroTestFight::Far), "far") == 0 &&
	          std::strcmp(walkers::NecroTestFightName(walkers::NecroTestFight::Walkers), "walkers") == 0 &&
	          std::strcmp(walkers::NecroTestFightName(walkers::NecroTestFight::Summon), "summon") == 0 &&
	          std::strcmp(walkers::NecroTestFightName(walkers::NecroTestFight::Off), "off") == 0,
	      "its name for the log");
}

void TestBandsMeet() {
	std::printf("Enemy bands meet far away (step 5)\n");
	walkers::NecroBands st;
	st.count = 3;
	st.bands[0] = {0, 1, 0, 8, 0.0f, 0.0f};
	st.bands[1] = {1, 2, 0, 6, 1000.0f, 0.0f};
	st.bands[2] = {2, 1, 0, 5, 1500.0f, 0.0f};  // team 1: band 0's ally
	const UInt32 full[] = {50, 50, 50, 50};  // 50 + 50 % 51 = 100%: all of the other's size
	Rolls all{full, 4};
	walkers::NecroBandEvent ev[16];
	// band 0 (8) and band 1 (6) meet: 0 loses 6 -> 2, 1 loses 8 -> 0; band 2 (team 1) is band 0's ally
	// and band 1 (team 2) at 500 meets band 2 too - but band 1 is down to none after the first fight
	UInt32 n = walkers::NecroBandsMeet(st, 2000.0f, all, ev, 16);
	Check(st.count == 2 && st.bands[0].size == 2 && st.bands[1].size == 5 && st.bands[1].team == 1,
	      "the enemies fight: 8 against 6 at the full roll - 2 left of the first, the second gone; the ally untouched");
	Check(CountBand(ev, n, walkers::NecroBandEventKind::Met) == 2 && CountBand(ev, n, walkers::NecroBandEventKind::Destroyed) == 1,
	      "met, and one destroyed");
	walkers::NecroBands half;
	half.count = 2;
	half.bands[0] = {0, 1, 0, 8, 0.0f, 0.0f};
	half.bands[1] = {1, 2, 0, 6, 0.0f, 0.0f};
	const UInt32 low[] = {0};  // 50%
	Rolls least{low, 1};
	walkers::NecroBandsMeet(half, 2000.0f, least, ev, 16);
	Check(half.count == 2 && half.bands[0].size == 5 && half.bands[1].size == 2, "at the least roll: half of the other's size");
	walkers::NecroBands apart;
	apart.count = 2;
	apart.bands[0] = {0, 1, 0, 8, 0.0f, 0.0f};
	apart.bands[1] = {1, 2, 0, 6, 9000.0f, 0.0f};
	Check(walkers::NecroBandsMeet(apart, 2000.0f, least, ev, 16) == 0 && apart.bands[0].size == 8, "too far apart: nothing");
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

void TestRisenTeam() {
	std::printf("the risen near them join a team (4.23 B, the gap after step 7)\n");
	using walkers::RisenTeam;
	Check(RisenTeam(2, 1, true) == 2, "killed by a walker of team 2 at team 1's place: team 2 (the killer's)");
	Check(RisenTeam(0, 3, true) == 3, "killed by someone else near team 3's place: team 3");
	Check(RisenTeam(0, 3, false) == 0, "far from every place, not killed by theirs: a free walker");
	Check(RisenTeam(4, 0, false) == 4, "killed by team 4's walker far from any place: team 4");
	Check(RisenTeam(0, 0, true) == 0, "near a place with no team known: a free walker");

	walkers::RisenTeams p;
	Check(walkers::TakeRisenTeam(p, 0x100) == 0, "nothing noted: no team");
	walkers::NoteRisenTeam(p, 0x100, 2);
	walkers::NoteRisenTeam(p, 0x200, 0);
	Check(p.count == 1, "a fallen of no team is not noted");
	walkers::NoteRisenTeam(p, 0x100, 3);
	Check(p.count == 1 && walkers::TakeRisenTeam(p, 0x100) == 3 && p.count == 0,
	      "noted again: the newer team; taken once, then gone");
	for (UInt32 k = 0; k < walkers::kRisenTeamsMost + 3; ++k) {
		walkers::NoteRisenTeam(p, 0x1000 + k, 1);
	}
	Check(p.count == walkers::kRisenTeamsMost && walkers::TakeRisenTeam(p, 0x1000) == 0 &&
	          walkers::TakeRisenTeam(p, 0x1000 + walkers::kRisenTeamsMost + 2) == 1,
	      "full: the oldest makes room for the newest");
	walkers::NoteRisenTeam(p, 0, 1);
	Check(walkers::TakeRisenTeam(p, 0) == 0, "no reference: never noted");
}

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
	TestBossActors();
	TestBands();
	TestTeamsFight();
	TestBandsMeet();
	TestTestFight();
	TestSieges();
	TestRisenTeam();
	std::printf(g_failures == 0 ? "All passed\n" : "%d FAILED\n", g_failures);
	return g_failures == 0 ? 0 : 1;
}
