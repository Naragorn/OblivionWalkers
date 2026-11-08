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
using walkers::SlowAggressionLevel;
using walkers::SlowAggressionSettings;

// Own seconds, `low` to `high` (the level Custom).
SlowAggressionSettings Own(float low, float high) {
	SlowAggressionSettings s;
	s.level = SlowAggressionLevel::Custom;
	s.secondsMin = low;
	s.secondsMax = high;
	return s;
}

// One tick with the pair (walker 1, target 2) near or not; true when the
// walker is to start the fight.
using walkers::NearVerdict;

// `roll` (0..1) picks the pair's time between the setting's fewest and most
// seconds when the pair is new; 1 by default, the most.
NearVerdict Verdict(NearTracker& tracker, const SlowAggressionSettings& settings, bool near, float dt,
                    float roll = 1.0f) {
	walkers::BeginNearTick(tracker);
	const NearVerdict v = near ? walkers::NoteNear(tracker, settings, 1, 2, dt, roll) : NearVerdict::None;
	walkers::EndNearTick(tracker);
	return v;
}

// True when the walker is to start the fight (the first StartCombat).
bool Tick(NearTracker& tracker, const SlowAggressionSettings& settings, bool near, float dt, float roll = 1.0f) {
	return Verdict(tracker, settings, near, dt, roll) == NearVerdict::Start;
}

void TestDefaults() {
	std::printf("The defaults\n");
	const SlowAggressionSettings s;
	Check(s.enabled && s.level == SlowAggressionLevel::Medium && s.units == 1000.0f,
	      "on, medium, 1000 units (some 14 m, Nadi: \"ja ok 14m\")");
	Check(walkers::NeededSeconds(s, 0.0f) == 5.0f && walkers::NeededSeconds(s, 1.0f) == 10.0f,
	      "medium: 5 to 10 s");
	Check(walkers::RecordAggression(true, 100) == 0, "on: the walker records' aggression 0 - no fight of their own");
	Check(walkers::RecordAggression(false, 100) == 100, "off: the record's own aggression back");
}

void TestReach() {
	std::printf("Who is near\n");
	Check(walkers::InReach(600.0f, 800.0f, 0.0f, 1000.0f), "1000 units off, the bound: near");
	Check(!walkers::InReach(600.0f, 800.0f, 10.0f, 1000.0f), "a little past it: not near");
	Check(!walkers::InReach(0.0f, 0.0f, 1200.0f, 1000.0f), "straight above, 1200 units: not near - the height counts");
}

void TestTime() {
	std::printf("How long\n");
	const SlowAggressionSettings s = Own(5.0f, 5.0f);
	NearTracker t;
	bool fired = false;
	for (int i = 0; i < 9; ++i) {
		fired = fired || Tick(t, s, true, 0.5f);
	}
	Check(!fired, "4.5 s near: still calm");
	Check(Tick(t, s, true, 0.5f), "5 s near: the fight starts");
	// Measured 2026-11-08 (slow-aggression.txt): with Aggression 0 a walker
	// that StartCombat sent swung once and walked off - the engine drops the
	// fight. So the fight is sent again every second while the one stays near.
	Check(Verdict(t, s, true, 0.5f) == NearVerdict::None, "still near, half a second on: nothing yet");
	Check(Verdict(t, s, true, 0.5f) == NearVerdict::Keep, "a second on: the fight sent again (kept up)");
	Check(Verdict(t, s, true, 0.25f) == NearVerdict::None && Verdict(t, s, true, 0.75f) == NearVerdict::Keep,
	      "and again each second, never a second start");

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

// How long at random (Nadi, 2026-11-08: "statt 5s machen wir ein random aus
// x,y default 2,5 s"): each pair draws its time once, when it is new.
bool StartsAfter(const SlowAggressionSettings& s, float roll, float seconds) {
	NearTracker t;
	bool early = false;
	float spent = 0.0f;
	while (spent + 0.25f < seconds - 1e-3f) {
		early = early || Tick(t, s, true, 0.25f, roll);
		spent += 0.25f;
	}
	return !early && Tick(t, s, true, 0.25f, roll);
}

void TestRandom() {
	std::printf("How long, at random\n");
	const SlowAggressionSettings s = Own(2.0f, 5.0f);
	Check(walkers::NeededSeconds(s, 0.0f) == 2.0f, "the lowest roll: the fewest, 2 s");
	Check(walkers::NeededSeconds(s, 1.0f) == 5.0f, "the highest roll: the most, 5 s");
	Check(walkers::NeededSeconds(s, 0.5f) == 3.5f, "half way: 3.5 s");
	Check(walkers::NeededSeconds(s, -1.0f) == 2.0f && walkers::NeededSeconds(s, 2.0f) == 5.0f,
	      "a roll out of 0..1 held inside: 2 and 5 s");
	const SlowAggressionSettings swapped = Own(6.0f, 3.0f);
	Check(walkers::NeededSeconds(swapped, 0.0f) == 3.0f && walkers::NeededSeconds(swapped, 1.0f) == 6.0f,
	      "fewest above most (set so in LINK): the two taken as 3 to 6 s");
	const SlowAggressionSettings fixed = Own(4.0f, 4.0f);
	Check(walkers::NeededSeconds(fixed, 0.0f) == 4.0f && walkers::NeededSeconds(fixed, 0.9f) == 4.0f,
	      "fewest and most the same: always that, no chance");

	Check(StartsAfter(s, 0.0f, 2.0f), "rolled low: calm until 2 s, then the fight");
	Check(StartsAfter(s, 0.5f, 3.5f), "rolled half: calm until 3.5 s, then the fight");
	Check(StartsAfter(s, 1.0f, 5.0f), "rolled high: calm until 5 s, then the fight");

	NearTracker kept;
	bool fired = false;
	for (int i = 0; i < 8; ++i) {
		fired = fired || Tick(kept, s, true, 0.5f, i == 0 ? 1.0f : 0.0f);
	}
	Check(!fired, "the roll taken when the pair is new: later low rolls do not shorten its 5 s");

	NearTracker t;
	walkers::BeginNearTick(t);
	walkers::NoteNear(t, s, 1, 2, 1.5f, 0.0f);
	walkers::NoteNear(t, s, 1, 3, 1.5f, 1.0f);
	walkers::EndNearTick(t);
	walkers::BeginNearTick(t);
	const bool quick = walkers::NoteNear(t, s, 1, 2, 0.5f, 0.5f) == NearVerdict::Start;
	const bool slow = walkers::NoteNear(t, s, 1, 3, 0.5f, 0.5f) == NearVerdict::None;
	walkers::EndNearTick(t);
	const walkers::NearPair* found = walkers::FindNearPair(t, 1, 3);
	Check(found != nullptr && found->needed == 5.0f && walkers::FindNearPair(t, 1, 4) == nullptr,
	      "a pair found with its drawn time (for the log); one never near: none");
	Check(quick && slow, "two near one walker: each its own roll - one turned on at 2 s, the other still calm");

	NearTracker again;
	Check(!Tick(again, s, true, 1.0f, 1.0f), "first time near, rolled high: calm at 1 s");
	Check(!Tick(again, s, false, 0.5f), "gone: the pair forgotten");
	Check(Tick(again, s, true, 2.0f, 0.0f), "back, a new roll (low): the fight at 2 s");
}

// How aggressive (Nadi, 2026-11-08: "bei weniger ist der rand 10,15s bei
// medium 5,10, bei hoch 1,5s default medium").
void TestLevels() {
	std::printf("How aggressive\n");
	SlowAggressionSettings s;
	s.level = SlowAggressionLevel::Low;
	Check(walkers::NeededSeconds(s, 0.0f) == 10.0f && walkers::NeededSeconds(s, 1.0f) == 15.0f, "low: 10 to 15 s");
	s.level = SlowAggressionLevel::Medium;
	Check(walkers::NeededSeconds(s, 0.0f) == 5.0f && walkers::NeededSeconds(s, 1.0f) == 10.0f, "medium: 5 to 10 s");
	s.level = SlowAggressionLevel::High;
	Check(walkers::NeededSeconds(s, 0.0f) == 1.0f && walkers::NeededSeconds(s, 1.0f) == 5.0f, "high: 1 to 5 s");
	s.secondsMin = 20.0f;
	s.secondsMax = 30.0f;
	Check(walkers::NeededSeconds(s, 0.0f) == 1.0f, "a level set: the own seconds not used");
	s.level = SlowAggressionLevel::Custom;
	Check(walkers::NeededSeconds(s, 0.0f) == 20.0f && walkers::NeededSeconds(s, 1.0f) == 30.0f,
	      "custom: the own seconds, 20 to 30 s");
	s.level = static_cast<SlowAggressionLevel>(9);
	Check(walkers::NeededSeconds(s, 0.0f) == 5.0f && walkers::NeededSeconds(s, 1.0f) == 10.0f,
	      "a level unknown (a bad LINK value): medium");

	using walkers::ParseSlowAggressionLevel;
	Check(ParseSlowAggressionLevel("Low") == SlowAggressionLevel::Low &&
	          ParseSlowAggressionLevel("medium") == SlowAggressionLevel::Medium &&
	          ParseSlowAggressionLevel("HIGH") == SlowAggressionLevel::High &&
	          ParseSlowAggressionLevel("Custom") == SlowAggressionLevel::Custom,
	      "the INI's words, any case");
	Check(ParseSlowAggressionLevel("0") == SlowAggressionLevel::Low &&
	          ParseSlowAggressionLevel("2") == SlowAggressionLevel::High &&
	          ParseSlowAggressionLevel("3") == SlowAggressionLevel::Custom,
	      "or their numbers, as LINK holds them");
	Check(ParseSlowAggressionLevel("") == SlowAggressionLevel::Medium &&
	          ParseSlowAggressionLevel("wild") == SlowAggressionLevel::Medium &&
	          ParseSlowAggressionLevel("7") == SlowAggressionLevel::Medium && ParseSlowAggressionLevel(nullptr) ==
	          SlowAggressionLevel::Medium,
	      "empty, unknown or missing: medium");

	NearTracker t;
	SlowAggressionSettings high;
	high.level = SlowAggressionLevel::High;
	Check(Tick(t, high, true, 1.0f, 0.0f), "high, rolled low: the fight after 1 s");
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
	SlowAggressionSettings now = Own(0.0f, 0.0f);
	NearTracker n;
	Check(Tick(n, now, true, 0.5f), "0 s: the first tick near starts the fight");
}

void TestMany() {
	std::printf("Many at once\n");
	const SlowAggressionSettings s = Own(5.0f, 5.0f);
	NearTracker t;
	walkers::BeginNearTick(t);
	for (UInt32 w = 0; w < walkers::kNearPairsMost; ++w) {
		walkers::NoteNear(t, s, 100 + w, 2, 1.0f, 1.0f);
	}
	Check(t.count == walkers::kNearPairsMost, "the table full");
	Check(walkers::NoteNear(t, s, 9999, 2, 10.0f, 1.0f) == NearVerdict::None, "one more: no room, no fight, no crash");
	walkers::EndNearTick(t);
	walkers::BeginNearTick(t);
	walkers::EndNearTick(t);
	Check(t.count == 0, "a tick with none near: the table empty again");
	walkers::BeginNearTick(t);
	Check(walkers::NoteNear(t, s, 1, 2, 2.0f, 1.0f) == NearVerdict::None &&
	          walkers::NoteNear(t, s, 1, 3, 2.0f, 1.0f) == NearVerdict::None,
	      "one walker, two near: each its own clock");
	walkers::EndNearTick(t);
	Check(t.count == 2, "the two kept");
	walkers::BeginNearTick(t);
	const bool first = walkers::NoteNear(t, s, 1, 2, 3.0f, 0.0f) == NearVerdict::Start;
	const bool second = walkers::NoteNear(t, s, 1, 3, 3.0f, 0.0f) == NearVerdict::Start;
	walkers::EndNearTick(t);
	Check(first && second, "5 s each: the walker turns on both, one after the other");
}

}  // namespace

int main() {
	TestDefaults();
	TestReach();
	TestTime();
	TestRandom();
	TestLevels();
	TestOff();
	TestMany();
	if (g_failures != 0) {
		std::printf("%d failed\n", g_failures);
		return 1;
	}
	std::printf("all passed\n");
	return 0;
}
