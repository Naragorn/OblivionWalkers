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
	Check(walkers::CombatLineAllowed(g, 0xFF000F8E, CombatVerb::Stop, 200, 0), "hold 0 ([Debug] CombatGuardFrames=0): the guard off");
	Check(!walkers::CombatLineAllowed(g, 0xFF000F8E, CombatVerb::Stop, 200, 1) &&
	          walkers::CombatLineAllowed(g, 0xFF000F8E, CombatVerb::Stop, 201, 1),
	      "hold 1: only the same frame held back");
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

void TestGaps() {
	std::printf("The gap between a StartCombat and a StopCombat, counted\n");
	using walkers::CombatVerb;
	walkers::CombatGuard g;
	Check(walkers::FramesSinceStart(g, 0xFF000F8E, 50) == walkers::kCombatGapNone, "no StartCombat kept: none");
	walkers::CombatLineAllowed(g, 0xFF000F8E, CombatVerb::Start, 100, 10);
	Check(walkers::FramesSinceStart(g, 0xFF000F8E, 100) == 0, "the same frame: 0");
	Check(walkers::FramesSinceStart(g, 0xFF000F8E, 137) == 37, "37 frames on: 37");
	Check(walkers::FramesSinceStart(g, 0xFF000F91, 137) == walkers::kCombatGapNone, "another actor: none");
	Check(walkers::FramesSinceStart(g, 0xFF000F8E, 40) == walkers::kCombatGapNone, "the counter went back (a load): none");
	Check(walkers::FramesSinceStart(g, 0, 137) == walkers::kCombatGapNone, "ref 0 (an empty mark): none");
	walkers::CombatGaps c;
	walkers::CountStopGap(c, 0, false);
	walkers::CountStopGap(c, 4, false);
	walkers::CountStopGap(c, 9, false);
	walkers::CountStopGap(c, 31, true);
	walkers::CountStopGap(c, 59, true);
	walkers::CountStopGap(c, 60, true);
	walkers::CountStopGap(c, walkers::kCombatGapNone, true);
	Check(c.heldSameFrame == 1 && c.heldLater == 2, "held back: 1 in the same frame, 2 later");
	Check(c.sentNear == 2 && c.sentFar == 2, "sent: 2 within 60 frames, 2 at 60 or more or with none");
	Check(c.leastSent == 31, "the least gap sent: 31");
	walkers::CombatGaps none;
	Check(none.leastSent == walkers::kCombatGapNone, "nothing sent: no least gap");
}

}  // namespace

int main() {
	TestGuard();
	TestGaps();
	std::printf(g_failures == 0 ? "All passed\n" : "%d FAILED\n", g_failures);
	return g_failures == 0 ? 0 : 1;
}
