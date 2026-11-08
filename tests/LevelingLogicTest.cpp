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

bool Covered(const char* line) {
	for (UInt32 v = 0; v < walkers::kHeldValueCount; ++v) {
		char want[48];
		std::snprintf(want, sizeof want, "SetAV %s 0", walkers::kHeldValues[v].name);
		if (std::strcmp(line, want) == 0) {
			return true;
		}
	}
	return false;
}

void TestRestore() {
	std::printf("Up again: the held values back\n");
	// Nadi, 2026-11-08 (Windows run of 2026-11-07): walkers killed by the
	// player got up again with Aggression 0 and stayed passive.
	bool all = true;
	for (const char* line : walkers::kLevelingHoldLines) {
		all = all && Covered(line);
	}
	for (const char* line : walkers::kObXpHoldLines) {
		all = all && Covered(line);
	}
	Check(all, "every value either hold sets to 0 has a way back");
	Check(walkers::StillHeld(0.0f, 100), "base 0, the record 100: still held, set back");
	Check(!walkers::StillHeld(100.0f, 100), "base as the record: nothing to do");
	Check(!walkers::StillHeld(50.0f, 100), "base lowered by someone else, not to 0: not ours, left");
	Check(!walkers::StillHeld(0.0f, 0), "the record itself 0: nothing held");
	char line[32];
	Check(walkers::RestoreLine(walkers::kHeldValues[0], 100, line, sizeof line) &&
	          std::strcmp(line, "SetAV Aggression 100") == 0,
	      "the line: SetAV Aggression 100");
	char tiny[8];
	Check(!walkers::RestoreLine(walkers::kHeldValues[0], 100, tiny, sizeof tiny), "no room: no line");
	Check(walkers::kHeldValues[0].actorValue == 33 && walkers::kHeldValues[1].actorValue == 34 &&
	          walkers::kHeldValues[0].recordOffset == 0x6C && walkers::kHeldValues[1].recordOffset == 0x6D,
	      "Aggression 33 and Confidence 34, the record's AI bytes +0x6C and +0x6D");
}

}  // namespace

int main() {
	TestWho();
	TestLines();
	TestObXp();
	TestRestore();
	if (g_failures != 0) {
		std::printf("%d failed\n", g_failures);
		return 1;
	}
	std::printf("all passed\n");
	return 0;
}
