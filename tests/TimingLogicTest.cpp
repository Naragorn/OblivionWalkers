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
	walkers::EndTimingFrame(t);
	Check(t.count == 3 && std::strcmp(t.steps[2].name, "horde") == 0, "the steps by index, their names kept");
	Check(t.steps[0].total == 300 && t.steps[0].most == 200, "a step's sum and its most a frame");
	Check(t.frames == 2 && t.frameTotal == 600 && t.frameMost == 400, "the frames: two, 600 in all, 400 the most");
	Check(walkers::TimingMean(t, 0, 1000000) == 150.0, "the mean of walkers: 150 us a frame at 1 MHz");
	Check(walkers::TimingMean(t, 9, 1000000) == 0.0, "a step never noted: 0");
	Check(walkers::TimingMicros(5, 0) == 0.0 && walkers::TimingMicros(10000000, 10000000) == 1000000.0,
	      "no frequency: 0; a second of ticks: a million us");
	walkers::NoteTiming(t, walkers::kTimingStepsMost, "past", 5);
	Check(t.count == 3, "an index past the most: ignored");
	walkers::ResetTimingPeriod(t);
	Check(t.frames == 0 && t.steps[0].total == 0 && t.steps[0].most == 0 && t.frameMost == 0 &&
	          std::strcmp(t.steps[0].name, "walkers") == 0,
	      "a new period: the counts 0, the names kept");
	walkers::TimingState none;
	Check(walkers::TimingMean(none, 0, 1000000) == 0.0, "no frames yet: 0");
}

}  // namespace

int main() {
	TestTiming();
	std::printf(g_failures == 0 ? "All passed\n" : "%d FAILED\n", g_failures);
	return g_failures == 0 ? 0 : 1;
}
