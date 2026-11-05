// Every flow of the nights (docs/walkers-spec.md 4.20.5; Nadi, 2026-11-05:
// "go 3", toggled in the INI and LINK, on by default): when it is night,
// the rise and fall over the ramp, a night across midnight or not, the
// steps the records are rewritten at, the pace, the spawns' counts, the
// hordes' lead, off and the clock unknown.

#include "NightLogic.h"

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

bool Near(float a, float b) { return std::fabs(a - b) < 0.001f; }

using walkers::NightAt;
using walkers::NightNow;
using walkers::NightSettings;
using walkers::NightShare;

void TestShare() {
	std::printf("Night or day, 20:00 to 6:00, an hour's ramp\n");
	NightSettings s;
	Check(Near(NightShare(s, 12.0f), 0.0f), "noon: day");
	Check(Near(NightShare(s, 19.99f), 0.0f), "just before 20:00: day");
	Check(Near(NightShare(s, 20.5f), 0.5f), "20:30: half way up");
	Check(Near(NightShare(s, 21.0f), 1.0f), "21:00: full night");
	Check(Near(NightShare(s, 0.0f), 1.0f), "midnight: full night - the night runs across it");
	Check(Near(NightShare(s, 3.0f), 1.0f), "3:00: full night");
	Check(Near(NightShare(s, 5.5f), 0.5f), "5:30: half way down");
	Check(Near(NightShare(s, 6.0f), 0.0f), "6:00: day again");
	Check(Near(NightShare(s, 24.0f), 1.0f), "24.0 read as midnight");
}

void TestShapes() {
	std::printf("Other nights\n");
	NightSettings s;
	s.startHour = 1.0f;
	s.endHour = 4.0f;
	Check(Near(NightShare(s, 2.5f), 1.0f) && Near(NightShare(s, 0.5f), 0.0f) && Near(NightShare(s, 4.5f), 0.0f),
	      "1:00 to 4:00, not across midnight: night only between");
	s.rampHours = 0.0f;
	Check(Near(NightShare(s, 1.01f), 1.0f) && Near(NightShare(s, 3.99f), 1.0f), "no ramp: full at once");
	s.startHour = 2.0f;
	s.endHour = 3.0f;
	s.rampHours = 2.0f;
	Check(Near(NightShare(s, 2.5f), 1.0f) && Near(NightShare(s, 2.25f), 0.5f),
	      "a ramp longer than half the night: held to half, the middle full");
	s.startHour = 5.0f;
	s.endHour = 5.0f;
	Check(Near(NightShare(s, 5.0f), 0.0f) && Near(NightShare(s, 17.0f), 0.0f), "start and end alike: no night");
	s.startHour = 30.0f;
	s.endHour = -2.0f;
	s.rampHours = 0.0f;
	Check(Near(NightShare(s, 7.0f), 1.0f) && Near(NightShare(s, 23.0f), 0.0f),
	      "hours out of 0..24 taken around the clock (30 is 6:00, -2 is 22:00)");
}

void TestOff() {
	std::printf("Off, or no clock\n");
	NightSettings s;
	s.enabled = false;
	Check(Near(NightShare(s, 0.0f), 0.0f), "Enabled 0: never night");
	NightSettings on;
	Check(Near(NightShare(on, -1.0f), 0.0f), "the clock unknown (-1, the main menu): day");
	const NightNow n = NightAt(s, 0.0f);
	Check(n.step == 0 && Near(n.speedFactor, 1.0f) && n.spawnPercent == 100 && Near(n.leadFactor, 1.0f),
	      "off: the pace, the counts and the lead as by day");
}

void TestEffects() {
	std::printf("What the night does\n");
	NightSettings s;
	const NightNow day = NightAt(s, 12.0f);
	Check(day.step == 0 && Near(day.speedFactor, 1.0f) && day.spawnPercent == 100 && Near(day.leadFactor, 1.0f),
	      "by day: x1, 100 %, x1");
	const NightNow night = NightAt(s, 1.0f);
	Check(night.step == walkers::kNightSteps && Near(night.speedFactor, 1.3f) && night.spawnPercent == 150 &&
	          Near(night.leadFactor, 1.5f),
	      "at night: SpeedMultiplier 1.3, SpawnPercent 150, HordeLeadPercent 150");
	const NightNow half = NightAt(s, 20.5f);
	Check(half.step == walkers::kNightSteps / 2 && Near(half.speedFactor, 1.15f) && half.spawnPercent == 125 &&
	          Near(half.leadFactor, 1.25f),
	      "20:30: half of each");
	Check(NightAt(s, 20.51f).step == NightAt(s, 20.53f).step,
	      "within a step the same - the records not rewritten each second");
	NightSettings slower = s;
	slower.speedMultiplier = 0.5f;
	slower.spawnPercent = 50;
	Check(Near(NightAt(slower, 1.0f).speedFactor, 0.5f) && NightAt(slower, 1.0f).spawnPercent == 50,
	      "under 1 and under 100 % taken as set: calmer nights");
}

void TestCounts() {
	std::printf("A spawn's count at night\n");
	Check(walkers::NightSpawnCount(1, 150) == 2, "1 at 150 %: 2 (rounded half up)");
	Check(walkers::NightSpawnCount(3, 150) == 5, "3 at 150 %: 5");
	Check(walkers::NightSpawnCount(10, 150) == 15, "10 at 150 %: 15 - past the day's most of 10");
	Check(walkers::NightSpawnCount(18, 300) == walkers::kNightWalkersPerSpawnMost, "held to the night's most");
	Check(walkers::NightSpawnCount(1, 10) == 1, "never under 1");
	Check(walkers::NightSpawnCount(4, 100) == 4, "100 %: as by day");
	Check(walkers::NightSpawnCount(0, 150) == 0, "0 stays 0");
}

}  // namespace

int main() {
	TestShare();
	TestShapes();
	TestOff();
	TestEffects();
	TestCounts();
	std::printf(g_failures == 0 ? "PASS\n" : "FAIL (%d)\n", g_failures);
	return g_failures == 0 ? 0 : 1;
}
