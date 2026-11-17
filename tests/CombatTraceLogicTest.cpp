// Every flow of the combat trace's change detector (CombatTraceLogic.h).

#include "CombatTraceLogic.h"

#include <cstdio>

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf("  %s  %s\n", condition ? "ok  " : "FAIL", what);
	if (!condition) {
		++g_failures;
	}
}

void TestChanges() {
	std::printf("Whom an actor fights, when it changes\n");
	walkers::CombatTrace t;
	Check(!walkers::CombatTargetChanged(t, 0xFF000001, 0), "first seen, fighting no one: nothing to say");
	Check(walkers::CombatTargetChanged(t, 0xFF000001, 0xFF000002), "then fighting someone: a change");
	Check(!walkers::CombatTargetChanged(t, 0xFF000001, 0xFF000002), "the same target again: none");
	Check(walkers::CombatTargetChanged(t, 0xFF000001, 0x00000014), "another target (the player): a change");
	Check(walkers::CombatTargetChanged(t, 0xFF000001, 0), "the fight over: a change");
	Check(walkers::CombatTargetChanged(t, 0xFF000003, 0xFF000001), "a new actor first seen fighting: a change");
	Check(t.count == 2, "two actors noted");
}

void TestFull() {
	std::printf("The table full\n");
	walkers::CombatTrace t;
	for (UInt32 i = 0; i < walkers::kCombatTraceMost; ++i) {
		walkers::CombatTargetChanged(t, 0xFF000100 + i, 0);
	}
	Check(!walkers::CombatTargetChanged(t, 0xFF000999, 0xFF000001), "one more actor: not noted, nothing said");
	Check(t.count == walkers::kCombatTraceMost, "the table stays at its most");
	Check(walkers::CombatTargetChanged(t, 0xFF000100, 0xFF000001), "one already in it: still followed");
}

}  // namespace

int main() {
	TestChanges();
	TestFull();
	if (g_failures != 0) {
		std::printf("%d failed\n", g_failures);
		return 1;
	}
	std::printf("all passed\n");
	return 0;
}
