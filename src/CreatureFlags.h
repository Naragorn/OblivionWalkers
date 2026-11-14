#pragma once

// TESActorBaseData flags of a creature (xOBSE GameForms.h), at
// TESCreature +0x28 (section 2 of the spec).

#include "Types.h"

namespace walkers {

inline constexpr UInt32 kCreatureFlagsOffset = 0x28;
inline constexpr UInt32 kCreatureSwims = 0x10;
inline constexpr UInt32 kCreatureFlies = 0x20;
inline constexpr UInt32 kCreatureWalks = 0x40;

// A creature that swims and can neither walk nor fly (a slaughterfish).
inline bool SwimsOnly(UInt32 flags) {
	return (flags & kCreatureSwims) != 0 && (flags & (kCreatureWalks | kCreatureFlies)) == 0;
}

}  // namespace walkers
