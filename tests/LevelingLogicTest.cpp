// Every flow of keeping walker kills from giving Ultimate Leveling's
// experience (docs/walkers-spec.md 4.3b; Nadi, 2026-10-27): who is held
// back, and the console lines that do it.

#include "LevelingLogic.h"

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

void TestWho() {
	std::printf("Whose kill gives no experience\n");
	Check(walkers::HoldsLevelingXp(true, true, true), "a dead walker, Ultimate Leveling loaded, the setting on");
	Check(!walkers::HoldsLevelingXp(false, true, true), "the setting off: Ultimate Leveling counts walkers as any kill");
	Check(!walkers::HoldsLevelingXp(true, false, true), "no Ultimate Leveling: nothing to hold back");
	Check(!walkers::HoldsLevelingXp(true, true, false), "anyone else: Ultimate Leveling's own business");
}

void TestLines() {
	std::printf("How\n");
	// Read in Ultimate Leveling.esp (2026-10-27): the kill's experience is
	// base * level modifier * ULVL2StatsOnDeathGetToughnessModifierFunctionScript,
	// whose last factor holds (3 * Confidence + Aggression) / 400; with both 0
	// the experience is 0 and "if ULVL.XPmod > 0" gives none, no message.
	Check(std::strcmp(walkers::kLevelingHoldLines[0], "SetAV Confidence 0") == 0 &&
	          std::strcmp(walkers::kLevelingHoldLines[1], "SetAV Aggression 0") == 0 && walkers::kLevelingHoldLineCount == 2,
	      "the dead walker's Confidence and Aggression to 0, in that order");
	Check(std::strcmp(walkers::kLevelingEsp, "Ultimate Leveling.esp") == 0, "found by its file name");
}

}  // namespace

int main() {
	TestWho();
	TestLines();
	if (g_failures != 0) {
		std::printf("%d failed\n", g_failures);
		return 1;
	}
	std::printf("all passed\n");
	return 0;
}
