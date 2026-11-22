// Every flow of the combat guard (docs/walkers-spec.md 4.23 B, the pool-12
// crash): a StopCombat on an actor this soon after a StartCombat on it waits.

#include "CombatGuardLogic.h"

#include <cstdio>

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf("  %s  %s\n", condition ? "ok  " : "FAIL", what);
	if (!condition) {
		++g_failures;
	}
}

void TestGuard() {
	std::printf("StartCombat and StopCombat on one actor\n");
	using walkers::CombatVerb;
	walkers::CombatGuard g;
	Check(walkers::CombatLineAllowed(g, 0xFF000F8E, CombatVerb::Stop, 100, 10), "a StopCombat with no StartCombat before: sent");
	Check(walkers::CombatLineAllowed(g, 0xFF000F8E, CombatVerb::Start, 200, 10), "a StartCombat: sent, its frame kept");
	Check(!walkers::CombatLineAllowed(g, 0xFF000F8E, CombatVerb::Stop, 200, 10),
	      "a StopCombat in the same frame (the crash runs' frame 2772): held back");
	Check(!walkers::CombatLineAllowed(g, 0xFF000F8E, CombatVerb::Stop, 209, 10), "9 frames after: still held back");
	Check(walkers::CombatLineAllowed(g, 0xFF000F8E, CombatVerb::Stop, 210, 10), "10 frames after: sent");
	Check(walkers::CombatLineAllowed(g, 0xFF000F91, CombatVerb::Stop, 200, 10), "another actor: not held");
	Check(walkers::CombatLineAllowed(g, 0xFF000F8E, CombatVerb::Other, 200, 10), "any other line: sent");
	Check(walkers::CombatVerbOf("StartCombat FF000F8E") == CombatVerb::Start &&
	          walkers::CombatVerbOf("stopcombat") == CombatVerb::Stop && walkers::CombatVerbOf("StopCombatAlarm") == CombatVerb::Other &&
	          walkers::CombatVerbOf("MoveTo FF000F8E 0 150 0") == CombatVerb::Other && walkers::CombatVerbOf(nullptr) == CombatVerb::Other,
	      "the verb read off the line, in any case");
	walkers::CombatGuard full;
	for (UInt32 i = 0; i < walkers::kCombatGuardMarks + 5; ++i) {
		walkers::CombatLineAllowed(full, 0xFF001000 + i, CombatVerb::Start, 500, 10);
	}
	Check(!walkers::CombatLineAllowed(full, 0xFF001000 + walkers::kCombatGuardMarks + 4, CombatVerb::Stop, 501, 10),
	      "more actors than marks: the newest kept");
	Check(walkers::CombatLineAllowed(g, 0xFF000F8E, CombatVerb::Start, 300, 10) &&
	          walkers::CombatLineAllowed(g, 0xFF000F8E, CombatVerb::Stop, 5, 10),
	      "the frame counter set back (a load): sent");
}

}  // namespace

int main() {
	TestGuard();
	std::printf(g_failures == 0 ? "All passed\n" : "%d FAILED\n", g_failures);
	return g_failures == 0 ? 0 : 1;
}
