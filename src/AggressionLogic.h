#pragma once

// 4.21 Slow aggression (docs/walkers-spec.md; Nadi, 2026-11-08: walkers turn
// on the player and anyone else only once that one has been near them for a
// while; default on, near meaning some 14 m, no line of sight needed; the
// while drawn at random for each one, 2 to 5 s by default - Nadi, 2026-11-08:
// "statt 5s machen wir ein random aus x,y default 2,5 s").
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
	// [Zombies] SlowAggressionSecondsMin / SlowAggressionSecondsMax: how long
	// one must stay near, drawn between the two for each walker and each one
	// near it.
	float secondsMin = 2.0f;
	float secondsMax = 5.0f;
	// [Zombies] SlowAggressionUnits: how near, in game units (1000 is some
	// 14 m: 1 unit 1.43 cm, 70 units a yard).
	float units = 1000.0f;
};

// The walker records' aggression: 0 with the rule on, their own without.
inline UInt8 RecordAggression(bool enabled, UInt8 original) { return enabled ? 0 : original; }

// Near: within `units` in all three axes together.
inline bool InReach(float dx, float dy, float dz, float units) {
	return dx * dx + dy * dy + dz * dz <= units * units;
}

// The time a new pair must stay near: `roll` (0..1, held inside) of the way
// from the fewest to the most seconds; the two taken in order, whichever is
// set higher.
inline float NeededSeconds(const SlowAggressionSettings& settings, float roll) {
	const float low = settings.secondsMin < settings.secondsMax ? settings.secondsMin : settings.secondsMax;
	const float high = settings.secondsMin < settings.secondsMax ? settings.secondsMax : settings.secondsMin;
	const float r = roll < 0.0f ? 0.0f : (roll > 1.0f ? 1.0f : roll);
	return low + (high - low) * r;
}

inline constexpr UInt32 kNearPairsMost = 512;
// Measured 2026-11-08 (harness/slow-aggression.txt): a walker with
// Aggression 0 that StartCombat sent swung once and walked off - the engine
// drops its fight. So while the one stays near, the fight is sent again
// every second.
inline constexpr float kNearKeepSeconds = 1.0f;

enum class NearVerdict : UInt8 {
	None,   // nothing to do
	Start,  // the first StartCombat: the one stayed near long enough
	Keep,   // the fight sent again to hold it
};

struct NearPair {
	UInt32 walker = 0;
	UInt32 target = 0;
	float seconds = 0.0f;
	float needed = 0.0f;  // drawn once, when the pair is new
	float sinceSent = 0.0f;
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

// `target` is near `walker` this tick, `dt` seconds after the last one:
// Start once it has stayed near for its drawn time in one go (`roll` draws it
// when the pair is new, NeededSeconds), then Keep every kNearKeepSeconds
// while it stays near.
inline NearVerdict NoteNear(NearTracker& tracker, const SlowAggressionSettings& settings, UInt32 walker, UInt32 target,
                            float dt, float roll) {
	if (!settings.enabled) {
		return NearVerdict::None;
	}
	NearPair* pair = nullptr;
	for (UInt32 i = 0; i < tracker.count && pair == nullptr; ++i) {
		if (tracker.pairs[i].walker == walker && tracker.pairs[i].target == target) {
			pair = &tracker.pairs[i];
		}
	}
	if (pair == nullptr) {
		if (tracker.count >= kNearPairsMost) {
			return NearVerdict::None;
		}
		pair = &tracker.pairs[tracker.count++];
		*pair = NearPair{};
		pair->walker = walker;
		pair->target = target;
		pair->needed = NeededSeconds(settings, roll);
	}
	pair->seen = true;
	pair->seconds += dt;
	if (pair->fired) {
		pair->sinceSent += dt;
		if (pair->sinceSent + 1e-4f < kNearKeepSeconds) {
			return NearVerdict::None;
		}
		pair->sinceSent = 0.0f;
		return NearVerdict::Keep;
	}
	if (pair->seconds + 1e-4f < pair->needed) {
		return NearVerdict::None;
	}
	pair->fired = true;
	pair->sinceSent = 0.0f;
	return NearVerdict::Start;
}

// The pair of `walker` and `target`, or none.
inline const NearPair* FindNearPair(const NearTracker& tracker, UInt32 walker, UInt32 target) {
	for (UInt32 i = 0; i < tracker.count; ++i) {
		if (tracker.pairs[i].walker == walker && tracker.pairs[i].target == target) {
			return &tracker.pairs[i];
		}
	}
	return nullptr;
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
