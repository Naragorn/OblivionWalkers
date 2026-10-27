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

void TestObXp() {
	std::printf("Oblivion XP Update\n");
	// Read in Oblivion XP.esp 4.5 (OBXPfnOnDeath, set by ObXPMainScript at the
	// game load): bonusXP = (the dead's base Health + Agility + Endurance +
	// Intelligence + Personality + Speed + Strength + Willpower) / the
	// player's, times multXPKillBonus (1 by default); the kill's XP is baseXP
	// times bonusXP - 0 with those eight at 0.
	Check(walkers::kObXpLineCount == 8, "eight actor values");
	bool all = true;
	const char* want[] = {"SetAV Health 0",       "SetAV Agility 0", "SetAV Endurance 0", "SetAV Intelligence 0",
	                      "SetAV Personality 0", "SetAV Speed 0",   "SetAV Strength 0",  "SetAV Willpower 0"};
	for (UInt32 i = 0; i < walkers::kObXpLineCount; ++i) {
		all = all && std::strcmp(walkers::kObXpHoldLines[i], want[i]) == 0;
	}
	Check(all, "the dead walker's eight to 0");
	Check(std::strcmp(walkers::kObXpEsp, "Oblivion XP.esp") == 0 && walkers::kObXpQuestLocalId == 0x000ED3 &&
	          std::strcmp(walkers::kObXpXpVariable, "currentXP") == 0,
	      "found by its file name; its quest ObXPMain (000ED3) keeps currentXP");
	Check(walkers::HoldsLevelingXp(true, true, true) && !walkers::HoldsLevelingXp(false, true, true),
	      "the same switch, NoLevelingXP, for both mods");
}

}  // namespace

int main() {
	TestWho();
	TestLines();
	TestObXp();
	if (g_failures != 0) {
		std::printf("%d failed\n", g_failures);
		return 1;
	}
	std::printf("all passed\n");
	return 0;
}
