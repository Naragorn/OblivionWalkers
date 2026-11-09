// Every flow of a city's siege (docs/walkers-spec.md 4.12; Nadi, 2026-11-04:
// "ja mach 1-4"): when one begins - a chance a game day per city, or a horde
// held at a gate -, how many walkers it has and how many stand at the gate
// at once, the guards' toll while the player is away, the end and the pause,
// the gate's doors locked by the siege and only those unlocked, the co-save.

#include "SiegeLogic.h"

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

using walkers::CitySiege;
using walkers::SiegeEvent;
using walkers::SiegeSettings;
using walkers::SiegeState;

// A roll that hands out the numbers given, then 99s.
struct Rolls {
	const UInt32* values;
	UInt32 count;
	UInt32 next = 0;
	UInt32 operator()() { return next < count ? values[next++] : 99u; }
};

void TestGates() {
	std::printf("Each city's gates\n");
	UInt32 first = 0;
	UInt32 count = 0;
	Check(walkers::CityGates(walkers::City::Anvil, first, count) && first == 0 && count == 3,
	      "Anvil: the table's first three");
	Check(walkers::CityGates(walkers::City::ImperialCity, first, count) && count == 6 &&
	          first + count == walkers::kCityGateCount,
	      "the Imperial City: its six, last in the table");
	Check(!walkers::CityGates(walkers::City::Count, first, count), "no city: none");
}

void TestBegin() {
	std::printf("A siege begins\n");
	SiegeSettings s;
	SiegeState st;
	SiegeEvent ev[16];
	const UInt32 none[1] = {0};
	Rolls quiet{none, 0};
	Check(walkers::StepSiegeDays(st, s, 10.2f, quiet, ev, 16) == 0 && st.lastDay == 10,
	      "the first look: the day noted, no roll for the days before");
	Check(walkers::StepSiegeDays(st, s, 10.9f, quiet, ev, 16) == 0, "the same day again: no roll");
	// Day 11: Anvil rolls 5 (< 10 %), its gate roll 1 (the Castle Gate), its
	// walkers roll 0 (40); every other city rolls 99.
	const UInt32 anvil[3] = {5, 1, 0};
	Rolls rolls{anvil, 3};
	UInt32 n = walkers::StepSiegeDays(st, s, 11.1f, rolls, ev, 16);
	const CitySiege& a = st.cities[0];
	Check(n == 1 && ev[0].kind == SiegeEvent::Begin && ev[0].city == 0 && ev[0].gate == 1 && a.active &&
	          a.gate == 1 && a.walkersLeft == 40,
	      "a roll under ChancePercent 10: Anvil besieged at its second gate, WalkersMin 40 walkers");
	const UInt32 again[1] = {0};
	Rolls rollsAgain{again, 1};
	n = walkers::StepSiegeDays(st, s, 12.1f, rollsAgain, ev, 16);
	Check(n == 1 && ev[0].city == 1 && st.cities[1].active, "a besieged city rolls no more; the next one does");
	SiegeState late;
	walkers::StepSiegeDays(late, s, 3.0f, quiet, ev, 16);
	const UInt32 many[8] = {99, 99, 99, 99, 99, 99, 99, 99};
	Rolls skip{many, 8};
	walkers::StepSiegeDays(late, s, 9.5f, skip, ev, 16);
	Check(late.lastDay == 9, "six days away: rolled once for the day it is, not six times");
	SiegeSettings off = s;
	off.enabled = false;
	SiegeState none2;
	Rolls always{anvil, 3};
	walkers::StepSiegeDays(none2, off, 1.0f, always, ev, 16);
	Check(walkers::StepSiegeDays(none2, off, 2.0f, always, ev, 16) == 0 && !none2.cities[0].active,
	      "Enabled 0: no siege begins");
	const UInt32 top[3] = {0, 0, 200};
	Rolls most{top, 3};
	SiegeState big;
	walkers::StepSiegeDays(big, s, 1.0f, quiet, ev, 16);
	walkers::StepSiegeDays(big, s, 2.0f, most, ev, 16);
	Check(big.cities[0].walkersLeft == 40 + 200 % 81, "the walkers drawn in WalkersMin..WalkersMax");
}

void TestHordeBegin() {
	std::printf("A horde held at a gate\n");
	SiegeSettings s;
	SiegeState st;
	Check(walkers::BeginSiegeAt(st, s, 3, 5.0f, 0) && st.cities[1].active && st.cities[1].gate == 3 &&
	          st.cities[1].walkersLeft == 40,
	      "Bravil's gate (table row 3, its only one): Bravil besieged there");
	Check(!walkers::BeginSiegeAt(st, s, 3, 5.0f, 0), "besieged already: not again");
	Check(!walkers::BeginSiegeAt(st, s, walkers::kCityGateCount, 5.0f, 0), "no such gate: none");
}

void TestAtGate() {
	std::printf("Walkers at the gate at once\n");
	SiegeSettings s;
	CitySiege c;
	c.active = true;
	c.walkersLeft = 100;
	Check(walkers::WalkersAtGateNow(c, s) == 24, "WalkersAtGate 24 of 100 left");
	c.walkersLeft = 10;
	Check(walkers::WalkersAtGateNow(c, s) == 10, "fewer left than that: those");
	s.walkersAtGate = 5;
	c.walkersLeft = 100;
	Check(walkers::WalkersAtGateNow(c, s) == 12, "held to 12 at least");
	s.walkersAtGate = 500;
	Check(walkers::WalkersAtGateNow(c, s) == 100, "and to 100 at most (the ESP's pool)");
	c.active = false;
	Check(walkers::WalkersAtGateNow(c, s) == 0, "no siege: none");
}

void TestToll() {
	std::printf("The guards' toll while the player is away, and the end\n");
	SiegeSettings s;
	SiegeState st;
	walkers::BeginSiegeAt(st, s, 0, 2.0f, 0);
	SiegeEvent ev[4];
	CitySiege& a = st.cities[0];
	Check(walkers::StepSiegeToll(a, 0, s, 2.5f, false, ev, 4) == 0 && a.walkersLeft == 40 - 24,
	      "half a day away at GuardKillsPerHour 2: 24 walkers down");
	Check(walkers::StepSiegeToll(a, 0, s, 3.0f, true, ev, 4) == 0 && a.walkersLeft == 16,
	      "the player at the gate: no toll counted - the fight is seen");
	Check(walkers::StepSiegeToll(a, 0, s, 3.1f, false, ev, 4) == 0 && a.walkersLeft == 16 - 4,
	      "away again: counted from the time the player left, not from before");
	const UInt32 n = walkers::StepSiegeToll(a, 0, s, 4.0f, false, ev, 4);
	Check(n == 1 && ev[0].kind == SiegeEvent::End && ev[0].city == 0 && !a.active && a.walkersLeft == 0 &&
	          a.pauseUntil == 4.0f + 3.0f,
	      "none left: the end, then PauseDays 3");
	const UInt32 rolls[3] = {0, 0, 0};
	Rolls r{rolls, 3};
	walkers::StepSiegeDays(st, s, 5.0f, r, ev, 4);
	Check(!a.active, "paused: no new siege at Anvil");
	SiegeSettings noToll = s;
	noToll.guardKillsPerHour = 0.0f;
	SiegeState st2;
	walkers::BeginSiegeAt(st2, noToll, 0, 1.0f, 0);
	walkers::StepSiegeToll(st2.cities[0], 0, noToll, 9.0f, false, ev, 4);
	Check(st2.cities[0].walkersLeft == 40, "GuardKillsPerHour 0: it lasts until the player ends it");
}

void TestEnded() {
	std::printf("A walker of the siege ended\n");
	SiegeSettings s;
	SiegeState st;
	walkers::BeginSiegeAt(st, s, 0, 1.0f, 0);
	st.cities[0].walkersLeft = 2;
	SiegeEvent ev[2];
	Check(walkers::SiegeWalkerEnded(st.cities[0], 0, s, 1.5f, ev, 2) == 0 && st.cities[0].walkersLeft == 1,
	      "one fewer");
	Check(walkers::SiegeWalkerEnded(st.cities[0], 0, s, 1.6f, ev, 2) == 1 && ev[0].kind == SiegeEvent::End &&
	          !st.cities[0].active,
	      "the last: the siege is over");
	Check(walkers::SiegeWalkerEnded(st.cities[0], 0, s, 1.7f, ev, 2) == 0, "no siege: nothing");
}

void TestLocks() {
	std::printf("The gate's doors\n");
	CitySiege c;
	Check(walkers::LockDoor(c, 0, false) && (c.lockedByUs & 1) != 0, "the outer door open: locked by the siege");
	Check(!walkers::LockDoor(c, 1, true) && (c.lockedByUs & 2) == 0, "the inner door locked already: left as it was");
	Check(walkers::UnlocksDoor(c, 0) && !walkers::UnlocksDoor(c, 1),
	      "at the end only the door the siege locked is unlocked");
}

void TestSpot() {
	std::printf("Where the siege's walkers stand\n");
	walkers::CityGate gate{walkers::City::Anvil, 1, 0.0f, 0.0f, 0.0f, 0.0f, 100.0f, 0.0f, 2, 0.0f, 0.0f, 0.0f, "test"};
	float x = 0.0f;
	float y = 0.0f;
	walkers::SiegeSpot(gate, 0, x, y);
	Check(std::fabs(y - walkers::kSiegeStandOff) < 0.5f && std::fabs(x + 3.5f * walkers::kSiegeSpacing) < 0.5f,
	      "the first: kSiegeStandOff out from the door, on the way out, the row's left end");
	walkers::SiegeSpot(gate, 7, x, y);
	Check(std::fabs(x - 3.5f * walkers::kSiegeSpacing) < 0.5f && std::fabs(y - walkers::kSiegeStandOff) < 0.5f,
	      "the eighth: the row's right end");
	walkers::SiegeSpot(gate, 8, x, y);
	Check(std::fabs(y - walkers::kSiegeStandOff - walkers::kSiegeRowSpacing) < 0.5f, "the ninth: the next row, farther out");
	walkers::CityGate flat{walkers::City::Anvil, 1, 50.0f, 50.0f, 0.0f, 50.0f, 50.0f, 0.0f, 2, 0.0f, 0.0f, 0.0f, "same spot"};
	walkers::SiegeSpot(flat, 0, x, y);
	Check(x == x && y == y, "a gate whose way out is its door's spot: still a number, no division by zero");
}

void TestImage() {
	std::printf("The co-save\n");
	SiegeSettings s;
	SiegeState st;
	walkers::BeginSiegeAt(st, s, 1, 7.25f, 0);
	walkers::LockDoor(st.cities[0], 0, false);
	st.cities[3].pauseUntil = 9.5f;
	st.lastDay = 7;
	UInt8 image[walkers::kSiegeImageBytes];
	Check(walkers::SiegeImage(st, image, sizeof(image)) == walkers::kSiegeImageBytes, "written whole");
	SiegeState back;
	Check(walkers::ReadSiegeImage(back, image, sizeof(image)) && back.cities[0].active && back.cities[0].gate == 1 &&
	          back.cities[0].walkersLeft == 40 && back.cities[0].lockedByUs == 1 &&
	          back.cities[0].tollFrom == 7.25f && back.cities[3].pauseUntil == 9.5f && back.lastDay == 7,
	      "read back as written");
	Check(!walkers::ReadSiegeImage(back, image, 10) && !back.cities[0].active, "too short: nothing, emptied");
}

}  // namespace

int main() {
	TestGates();
	TestBegin();
	TestHordeBegin();
	TestAtGate();
	TestToll();
	TestEnded();
	TestLocks();
	TestSpot();
	TestImage();
	std::printf(g_failures == 0 ? "PASS\n" : "FAIL (%d)\n", g_failures);
	return g_failures == 0 ? 0 : 1;
}
