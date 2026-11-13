#pragma once

// Walkers' own cost per frame, process-free (Nadi, 2026-11-13: "baust du
// walkers extrem performant" - measured, never guessed). [Debug] StepTiming=1
// (INI only, a test switch): each step of Rise.cpp's Tick is timed with the
// QueryPerformanceCounter; every report period the mean and the most a frame
// of each, and of all of them together, go to the log.

#include "Types.h"

namespace walkers {

inline constexpr UInt32 kTimingStepsMost = 32;

struct TimingStep {
	const char* name = nullptr;
	unsigned long long total = 0;  // counter ticks this period
	unsigned long long most = 0;   // the most one frame
};

struct TimingState {
	TimingStep steps[kTimingStepsMost];
	UInt32 count = 0;
	UInt32 frames = 0;              // this period
	unsigned long long frameTotal = 0;
	unsigned long long frameMost = 0;
	unsigned long long frameNow = 0;  // the frame being timed
};

// Step `index` named `name` took `ticks` this frame.
inline void NoteTiming(TimingState& t, UInt32 index, const char* name, unsigned long long ticks) {
	if (index >= kTimingStepsMost) {
		return;
	}
	if (index >= t.count) {
		t.count = index + 1;
	}
	TimingStep& s = t.steps[index];
	s.name = name;
	s.total += ticks;
	if (ticks > s.most) {
		s.most = ticks;
	}
	t.frameNow += ticks;
}

// The frame done: its sum counted.
inline void EndTimingFrame(TimingState& t) {
	++t.frames;
	t.frameTotal += t.frameNow;
	if (t.frameNow > t.frameMost) {
		t.frameMost = t.frameNow;
	}
	t.frameNow = 0;
}

// Microseconds from counter ticks at `frequency` a second.
inline double TimingMicros(unsigned long long ticks, unsigned long long frequency) {
	return frequency == 0 ? 0.0 : static_cast<double>(ticks) * 1000000.0 / static_cast<double>(frequency);
}

// The mean a frame of step `index` this period, in microseconds.
inline double TimingMean(const TimingState& t, UInt32 index, unsigned long long frequency) {
	return index >= t.count || t.frames == 0 ? 0.0 : TimingMicros(t.steps[index].total, frequency) / t.frames;
}

// A new period: the counts back to 0, the names kept.
inline void ResetTimingPeriod(TimingState& t) {
	for (UInt32 i = 0; i < t.count; ++i) {
		t.steps[i].total = 0;
		t.steps[i].most = 0;
	}
	t.frames = 0;
	t.frameTotal = 0;
	t.frameMost = 0;
	t.frameNow = 0;
}

}  // namespace walkers
