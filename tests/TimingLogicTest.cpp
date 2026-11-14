// Every flow of Walkers' own cost per frame ([Debug] StepTiming, TimingLogic.h):
// a step noted, a frame ended, the mean and the most, a new period.

#include "TimingLogic.h"

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

void TestTiming() {
	std::printf("The timing\n");
	walkers::TimingState t;
	walkers::NoteTiming(t, 0, "walkers", 100);
	walkers::NoteTiming(t, 2, "horde", 300);
	walkers::EndTimingFrame(t);
	walkers::NoteTiming(t, 0, "walkers", 200);
	walkers::NoteTiming(t, 3, "walkers.loop", 150, true);
	walkers::EndTimingFrame(t);
	Check(t.count == 4 && std::strcmp(t.steps[2].name, "horde") == 0, "the steps by index, their names kept");
	Check(t.steps[3].total == 150, "a part timed inside a step: counted for itself");
	Check(t.steps[0].total == 300 && t.steps[0].most == 200, "a step's sum and its most a frame");
	Check(t.frames == 2 && t.frameTotal == 600 && t.frameMost == 400,
	      "the frames: two, 600 in all, 400 the most - the part not counted twice");
	Check(walkers::TimingMean(t, 0, 1000000) == 150.0, "the mean of walkers: 150 us a frame at 1 MHz");
	Check(walkers::TimingMean(t, 9, 1000000) == 0.0, "a step never noted: 0");
	Check(walkers::TimingMicros(5, 0) == 0.0 && walkers::TimingMicros(10000000, 10000000) == 1000000.0,
	      "no frequency: 0; a second of ticks: a million us");
	walkers::NoteTiming(t, walkers::kTimingStepsMost, "past", 5);
	Check(t.count == 4, "an index past the most: ignored");
	walkers::ResetTimingPeriod(t);
	Check(t.frames == 0 && t.steps[0].total == 0 && t.steps[0].most == 0 && t.frameMost == 0 &&
	          std::strcmp(t.steps[0].name, "walkers") == 0,
	      "a new period: the counts 0, the names kept");
	walkers::TimingState none;
	Check(walkers::TimingMean(none, 0, 1000000) == 0.0, "no frames yet: 0");
}

}  // namespace

void TestTickSeconds() {
	std::printf("The seconds a tick counts\n");
	const UInt32 none[10] = {};
	const UInt32 world[10] = {0x3EC, 0x3ED, 0x3EE, 0x3F2};  // the HUD's menus
	const UInt32 talking[10] = {0x3EC, 0x3ED, 0x3EE, 0x3F2, 0x3F1};
	const UInt32 book[10] = {0x402};
	Check(!walkers::MenuStackHolds(none, 10, walkers::kMenuDialog) &&
	          !walkers::MenuStackHolds(world, 10, walkers::kMenuDialog) &&
	          !walkers::MenuStackHolds(book, 10, walkers::kMenuDialog),
	      "no menu, the HUD's, a book: no dialogue");
	Check(walkers::MenuStackHolds(talking, 10, walkers::kMenuDialog), "the dialogue menu among the open ones: a dialogue");
	Check(!walkers::MenuStackHolds(talking, 4, walkers::kMenuDialog) && !walkers::MenuStackHolds(nullptr, 10, 0x3F1),
	      "only the slots given are read; no stack: none");
	Check(walkers::TickSeconds(0.011f, false) == 0.011f, "a frame in the world: its seconds");
	Check(walkers::TickSeconds(0.011f, true) == 0.0f,
	      "a frame with the dialogue open: none - the world stands, nobody is taken away, moved or reset while the "
	      "player talks (the runner disabled as the dialogue's speaker: the game ended, harness 2026-11-14)");
	Check(walkers::TickSeconds(0.0f, false) == 0.0f && walkers::TickSeconds(-0.5f, false) == 0.0f &&
	          walkers::TickSeconds(1.0f, false) == 0.0f && walkers::TickSeconds(5.0f, false) == 0.0f,
	      "no time, a negative one, a second or more (a load): none, as before");
}

int main() {
	TestTiming();
	TestTickSeconds();
	std::printf(g_failures == 0 ? "All passed\n" : "%d FAILED\n", g_failures);
	return g_failures == 0 ? 0 : 1;
}
