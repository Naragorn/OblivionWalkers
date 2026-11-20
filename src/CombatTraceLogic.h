#pragma once

// [Debug] CombatTrace (INI only, a test switch): whom the actors near the
// player fight, logged when it changes - the harness's look at the combat
// targets (docs/walkers-spec.md 8c: Daedra Mode 2 (allies)'s walkers and Daedra side by
// side, do they see each other?). Process-free: the table of the targets
// last seen.

#include "Types.h"

namespace walkers {

inline constexpr UInt32 kCombatTraceMost = 64;

struct CombatPair {
	UInt32 actor = 0;
	UInt32 target = 0;
};

struct CombatTrace {
	CombatPair seen[kCombatTraceMost];
	UInt32 count = 0;
};

// Notes `actor`'s target now (0: none) and says whether it changed since
// the last look. An actor first seen without a target is no change; the
// table full, a new actor is not noted (and not reported).
inline bool CombatTargetChanged(CombatTrace& t, UInt32 actor, UInt32 target) {
	for (UInt32 i = 0; i < t.count; ++i) {
		if (t.seen[i].actor == actor) {
			const bool changed = t.seen[i].target != target;
			t.seen[i].target = target;
			return changed;
		}
	}
	if (t.count >= kCombatTraceMost) {
		return false;
	}
	t.seen[t.count++] = CombatPair{actor, target};
	return target != 0;
}

}  // namespace walkers
