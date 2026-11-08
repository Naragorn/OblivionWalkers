// Every flow of the slow aggression (docs/walkers-spec.md 4.21; Nadi,
// 2026-11-08): a walker turns on someone only after that one has been near
// it for a while.

#include "AggressionLogic.h"

#include <cstdio>

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf("  %s  %s\n", condition ? "ok  " : "FAIL", what);
	if (!condition) {
		++g_failures;
	}
}

using walkers::NearTracker;
using walkers::SlowAggressionSettings;

// One tick with the pair (walker 1, target 2) near or not; true when the
// walker is to start the fight.
bool Tick(NearTracker& tracker, const SlowAggressionSettings& settings, bool near, float dt) {
	walkers::BeginNearTick(tracker);
	const bool fire = near && walkers::NoteNear(tracker, settings, 1, 2, dt);
	walkers::EndNearTick(tracker);
	return fire;
}

void TestDefaults() {
	std::printf("The defaults\n");
	const SlowAggressionSettings s;
	Check(s.enabled && s.seconds == 5.0f && s.units == 1000.0f,
	      "on, 5 s, 1000 units (some 14 m, Nadi: \"ja ok 14m\")");
	Check(walkers::RecordAggression(true, 100) == 0, "on: the walker records' aggression 0 - no fight of their own");
	Check(walkers::RecordAggression(false, 100) == 100, "off: the record's own aggression back");
	Check(walkers::AboveSlow(true, 100.0f), "on, a walker with its own aggression 100 (an older save's SetAV): to 0");
	Check(!walkers::AboveSlow(true, 0.0f), "on, already 0: nothing");
	Check(!walkers::AboveSlow(false, 100.0f), "off: never lowered by this rule");
}

void TestReach() {
	std::printf("Who is near\n");
	Check(walkers::InReach(600.0f, 800.0f, 0.0f, 1000.0f), "1000 units off, the bound: near");
	Check(!walkers::InReach(600.0f, 800.0f, 10.0f, 1000.0f), "a little past it: not near");
	Check(!walkers::InReach(0.0f, 0.0f, 1200.0f, 1000.0f), "straight above, 1200 units: not near - the height counts");
}

void TestTime() {
	std::printf("How long\n");
	const SlowAggressionSettings s;
	NearTracker t;
	bool fired = false;
	for (int i = 0; i < 9; ++i) {
		fired = fired || Tick(t, s, true, 0.5f);
	}
	Check(!fired, "4.5 s near: still calm");
	Check(Tick(t, s, true, 0.5f), "5 s near: the fight starts");
	Check(!Tick(t, s, true, 0.5f) && !Tick(t, s, true, 10.0f), "still near: started once, not again and again");

	Check(!Tick(t, s, false, 0.5f), "gone: nothing");
	Check(t.count == 0, "gone: the pair forgotten");
	fired = false;
	for (int i = 0; i < 9; ++i) {
		fired = fired || Tick(t, s, true, 0.5f);
	}
	Check(!fired && Tick(t, s, true, 0.5f), "back again: the full 5 s anew, then the fight");

	NearTracker broken;
	bool out = false;
	for (int i = 0; i < 8; ++i) {
		out = out || Tick(broken, s, true, 0.5f);
	}
	out = out || Tick(broken, s, false, 0.5f);
	for (int i = 0; i < 8; ++i) {
		out = out || Tick(broken, s, true, 0.5f);
	}
	Check(!out, "4 s, a step away, 4 s: no 5 s in one go - calm");
}

void TestOff() {
	std::printf("Switched off or zero\n");
	SlowAggressionSettings off;
	off.enabled = false;
	NearTracker t;
	bool fired = false;
	for (int i = 0; i < 40; ++i) {
		fired = fired || Tick(t, off, true, 0.5f);
	}
	Check(!fired && t.count == 0, "off: the plugin starts no fight and keeps no pairs - the record's own aggression decides");
	SlowAggressionSettings now;
	now.seconds = 0.0f;
	NearTracker n;
	Check(Tick(n, now, true, 0.5f), "0 s: the first tick near starts the fight");
}

void TestMany() {
	std::printf("Many at once\n");
	const SlowAggressionSettings s;
	NearTracker t;
	walkers::BeginNearTick(t);
	for (UInt32 w = 0; w < walkers::kNearPairsMost; ++w) {
		walkers::NoteNear(t, s, 100 + w, 2, 1.0f);
	}
	Check(t.count == walkers::kNearPairsMost, "the table full");
	Check(!walkers::NoteNear(t, s, 9999, 2, 10.0f), "one more: no room, no fight, no crash");
	walkers::EndNearTick(t);
	walkers::BeginNearTick(t);
	walkers::EndNearTick(t);
	Check(t.count == 0, "a tick with none near: the table empty again");
	walkers::BeginNearTick(t);
	Check(!walkers::NoteNear(t, s, 1, 2, 2.0f) && !walkers::NoteNear(t, s, 1, 3, 2.0f),
	      "one walker, two near: each its own clock");
	walkers::EndNearTick(t);
	Check(t.count == 2, "the two kept");
	walkers::BeginNearTick(t);
	const bool first = walkers::NoteNear(t, s, 1, 2, 3.0f);
	const bool second = walkers::NoteNear(t, s, 1, 3, 3.0f);
	walkers::EndNearTick(t);
	Check(first && second, "5 s each: the walker turns on both, one after the other");
}

}  // namespace

int main() {
	TestDefaults();
	TestReach();
	TestTime();
	TestOff();
	TestMany();
	if (g_failures != 0) {
		std::printf("%d failed\n", g_failures);
		return 1;
	}
	std::printf("all passed\n");
	return 0;
}
