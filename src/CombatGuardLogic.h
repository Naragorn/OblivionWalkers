#pragma once

// The combat guard, process-free (docs/walkers-spec.md 4.23 B, the pool-12
// crash): measured 2026-11-22 - in both crash runs the probe caught, the
// plugin sent StartCombat both ways and StopCombat on both actors in the
// same frame; pool 12 broke 2 and 226 frames later, and 4 of 15 runs with
// that pair crashed (0 of 5 without). So a StopCombat on an actor waits
// until `hold` frames have passed since a StartCombat on it (the caller sends
// it again later - the rules repeat twice a second).

#include "Types.h"

namespace walkers {

enum class CombatVerb : UInt8 { Other, Start, Stop };

inline CombatVerb CombatVerbOf(const char* line) {
	if (line == nullptr) {
		return CombatVerb::Other;
	}
	auto is = [line](const char* word) {
		UInt32 k = 0;
		for (; word[k] != '\0'; ++k) {
			const char c = line[k];
			const char lower = c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c;
			if (lower != word[k]) {
				return false;
			}
		}
		return line[k] == '\0' || line[k] == ' ';
	};
	return is("startcombat") ? CombatVerb::Start : (is("stopcombat") ? CombatVerb::Stop : CombatVerb::Other);
}

inline constexpr UInt32 kCombatGuardMarks = 32;
inline constexpr UInt32 kCombatGuardHoldFrames = 10;

struct CombatGuard {
	UInt32 ref[kCombatGuardMarks] = {};
	UInt32 frame[kCombatGuardMarks] = {};
	UInt32 next = 0;
};

// Whether a `verb` line on `ref` may go now, at `frame`; a StartCombat is
// noted.
inline bool CombatLineAllowed(CombatGuard& g, UInt32 ref, CombatVerb verb, UInt32 frame, UInt32 hold) {
	if (verb == CombatVerb::Start) {
		for (UInt32 k = 0; k < kCombatGuardMarks; ++k) {
			if (g.ref[k] == ref) {
				g.frame[k] = frame;
				return true;
			}
		}
		g.ref[g.next] = ref;
		g.frame[g.next] = frame;
		g.next = (g.next + 1) % kCombatGuardMarks;
		return true;
	}
	if (verb != CombatVerb::Stop) {
		return true;
	}
	for (UInt32 k = 0; k < kCombatGuardMarks; ++k) {
		if (g.ref[k] == ref && frame >= g.frame[k] && frame - g.frame[k] < hold) {
			return false;
		}
	}
	return true;
}

// [Debug] CombatGuardFrames measured (2026-11-24): how far apart a StartCombat
// and a later StopCombat on one actor come. kCombatGapNone: no StartCombat
// kept for it, or the frame counter went back (a load).
inline constexpr UInt32 kCombatGapNone = 0xFFFFFFFFu;
inline constexpr UInt32 kCombatGapNearFrames = 60;

inline UInt32 FramesSinceStart(const CombatGuard& g, UInt32 ref, UInt32 frame) {
	for (UInt32 k = 0; k < kCombatGuardMarks; ++k) {
		if (g.ref[k] == ref && ref != 0) {
			return frame >= g.frame[k] ? frame - g.frame[k] : kCombatGapNone;
		}
	}
	return kCombatGapNone;
}

// The StopCombat lines counted by their gap: held back in the same frame or
// later, sent within kCombatGapNearFrames of a StartCombat (the least gap
// kept), sent later or with none before.
struct CombatGaps {
	UInt32 heldSameFrame = 0;
	UInt32 heldLater = 0;
	UInt32 sentNear = 0;
	UInt32 sentFar = 0;
	UInt32 leastSent = kCombatGapNone;
};

inline void CountStopGap(CombatGaps& c, UInt32 gap, bool sent) {
	if (!sent) {
		++(gap == 0 ? c.heldSameFrame : c.heldLater);
		return;
	}
	if (gap < kCombatGapNearFrames) {
		++c.sentNear;
	} else {
		++c.sentFar;
	}
	if (gap < c.leastSent) {
		c.leastSent = gap;
	}
}

}  // namespace walkers
