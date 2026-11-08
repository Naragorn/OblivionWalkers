#pragma once

// 4.21 Slow aggression (docs/walkers-spec.md; Nadi, 2026-11-08: walkers turn
// on the player and anyone else only once that one has been near them for a
// while; default on, 5 s, near meaning some 14 m, no line of sight needed).
//
// How: the walker records' aggression goes to 0, so no walker starts a fight
// on its own (the engine's own rule: a creature attacks when its aggression
// is above its disposition to the one it sees). The plugin times every
// living one near each walker and hands the walker "StartCombat <that one>"
// once it has stayed near long enough. A walker that is hit still fights
// back - that is the engine's, not this rule's.

#include <cmath>

#include "Types.h"

namespace walkers {

struct SlowAggressionSettings {
	// [Zombies] SlowAggression: on by default.
	bool enabled = true;
	// [Zombies] SlowAggressionSeconds: how long one must stay near.
	float seconds = 5.0f;
	// [Zombies] SlowAggressionUnits: how near, in game units (1000 is some
	// 14 m: 1 unit 1.43 cm, 70 units a yard).
	float units = 1000.0f;
};

// The walker records' aggression: 0 with the rule on, their own without.
inline UInt8 RecordAggression(bool enabled, UInt8 original) { return enabled ? 0 : original; }

// A live walker whose own aggression is above 0 while the rule is on - an
// older save's SetAV, the way back of NoLevelingXP: set to 0 too.
inline bool AboveSlow(bool enabled, float baseNow) { return enabled && baseNow > 0.5f; }

// Near: within `units` in all three axes together.
inline bool InReach(float dx, float dy, float dz, float units) {
	return dx * dx + dy * dy + dz * dz <= units * units;
}

inline constexpr UInt32 kNearPairsMost = 512;

struct NearPair {
	UInt32 walker = 0;
	UInt32 target = 0;
	float seconds = 0.0f;
	bool fired = false;
	bool seen = false;
};

struct NearTracker {
	NearPair pairs[kNearPairsMost];
	UInt32 count = 0;
};

// A tick: every pair unseen until NoteNear sees it again.
inline void BeginNearTick(NearTracker& tracker) {
	for (UInt32 i = 0; i < tracker.count; ++i) {
		tracker.pairs[i].seen = false;
	}
}

// `target` is near `walker` this tick, `dt` seconds after the last one: true
// once, when it has stayed near for the setting's seconds in one go.
inline bool NoteNear(NearTracker& tracker, const SlowAggressionSettings& settings, UInt32 walker, UInt32 target,
                     float dt) {
	if (!settings.enabled) {
		return false;
	}
	NearPair* pair = nullptr;
	for (UInt32 i = 0; i < tracker.count && pair == nullptr; ++i) {
		if (tracker.pairs[i].walker == walker && tracker.pairs[i].target == target) {
			pair = &tracker.pairs[i];
		}
	}
	if (pair == nullptr) {
		if (tracker.count >= kNearPairsMost) {
			return false;
		}
		pair = &tracker.pairs[tracker.count++];
		*pair = NearPair{};
		pair->walker = walker;
		pair->target = target;
	}
	pair->seen = true;
	pair->seconds += dt;
	if (pair->fired || pair->seconds < settings.seconds) {
		return false;
	}
	pair->fired = true;
	return true;
}

// The tick's end: a pair not seen again is gone - its time starts anew.
inline void EndNearTick(NearTracker& tracker) {
	UInt32 kept = 0;
	for (UInt32 i = 0; i < tracker.count; ++i) {
		if (tracker.pairs[i].seen) {
			tracker.pairs[kept++] = tracker.pairs[i];
		}
	}
	tracker.count = kept;
}

}  // namespace walkers
