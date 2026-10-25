#pragma once

// Keeping the corpse's ragdoll out of the walker's, process-free
// (docs/walkers-spec.md 4.1, the swap's look; Nadi, 2026-10-25: "ja mach
// beides" - read the filter, then the fix).
//
// The unseen walker is dropped onto the corpse and settles there while the
// corpse still lies, seen. Both ragdolls are bipeds (layer 8) of two
// groups, so the engine lets every pair of their bodies collide: the corpse
// pushed the walker tens of units off and once sat it on top of itself
// (the runs of 13:00 and 13:48). Read in Oblivion.exe 1.2.416 with REA:
//
// - A body's filter word (hkRigidBody +0x30): the layer in the low 6 bits,
//   the part number in bits 8 to 12, 0x4000 no collision, 0x8000 linked,
//   the system group in the high 16 bits. 0x00889C20 gives every body of
//   one object the same group, a fresh one from a counter when it has none.
// - The rule, 0x008A7F70 cdecl(a, b) - every query of the engine goes
//   through it (OBVR docs/hand-weapon-collision-spec.md) - as RuleCollides
//   below. The tables it reads lie as one run of words from 0x00BA7DB0: the
//   layer matrix's 32 rows, then the bone table, a row per part number; a
//   layer above 31 reads on into it.
// - 0x008A83C0 fills them: no bone row has bit 0 and row 0 is empty, so a
//   biped of part 0 meets no biped of its own group.
//
// So the corpse's bodies are moved into the walker's group with part 0:
// walker and corpse are then one object whose parts never meet, and every
// other pair - the corpse with the world, with anyone else - is decided
// exactly as before, the layer being kept. The corpse's own bodies no
// longer meet each other either; it lies settled, and it is disabled at
// the reveal. Whether the live tables still say so is not assumed:
// KeptApart asks the rule itself, on the tables as they are, for every
// pair.

#include "Types.h"

namespace walkers {

constexpr UInt32 kFilterLayerMask = 0x3F;
constexpr UInt32 kFilterPartMask = 0x1F00;
constexpr UInt32 kFilterPartShift = 8;
constexpr UInt32 kFilterNoCollision = 0x4000;
constexpr UInt32 kFilterLinked = 0x8000;
constexpr UInt32 kFilterGroupMask = 0xFFFF0000;
constexpr UInt32 kBipedLayer = 8;
// The one layer whose bodies the 0x4000 switch does not stop (the rule's
// first test reads only the first body's layer).
constexpr UInt32 kSwitchExemptLayer = 29;
// The layer matrix's 32 rows and the bone table's 32, as one run of words.
constexpr UInt32 kFilterTableWords = 64;
constexpr UInt32 kBoneTableWord = 32;

inline UInt32 FilterLayer(UInt32 filter) { return filter & kFilterLayerMask; }
inline UInt32 FilterPart(UInt32 filter) { return (filter & kFilterPartMask) >> kFilterPartShift; }

// Whether the engine lets bodies of these two filter words collide: the rule
// 0x008A7F70, step by step, over `tables` (kFilterTableWords words, as from
// 0x00BA7DB0).
inline bool RuleCollides(UInt32 a, UInt32 b, const UInt32* tables) {
	const UInt32 layerA = FilterLayer(a);
	const UInt32 layerB = FilterLayer(b);
	if (layerA != kSwitchExemptLayer && ((a | b) & kFilterNoCollision) != 0) {
		return false;
	}
	if ((a & kFilterGroupMask) == 0 || (b & kFilterGroupMask) == 0) {
		return true;
	}
	const bool oneGroup = ((a ^ b) & kFilterGroupMask) == 0;
	const bool matrix = (tables[layerA] & (1u << (layerB & 0x1F))) != 0;
	if (layerA == kBipedLayer && layerB == kBipedLayer) {
		if (!oneGroup) {
			return true;
		}
	} else {
		if (!oneGroup) {
			return matrix;
		}
		if ((a & b & kFilterLinked) != 0) {
			const int apart = static_cast<int>(FilterPart(a)) - static_cast<int>(FilterPart(b));
			return matrix && apart != 1 && apart != -1;
		}
		return false;
	}
	return (tables[kBoneTableWord + FilterPart(a)] & (1u << FilterPart(b))) != 0;
}

// The corpse body's filter word moved into the walker's group: part 0, not
// linked, everything else kept.
inline UInt32 ApartFilter(UInt32 corpse, UInt32 walker) {
	return (walker & kFilterGroupMask) | (corpse & ~(kFilterGroupMask | kFilterPartMask | kFilterLinked));
}

// Whether a corpse body of filter word `apart` meets none of the walker's
// bodies, either way round, by the rule on `tables`. No walker body read:
// false, there is nothing to go by.
inline bool KeptApart(const UInt32* walker, UInt32 count, UInt32 apart, const UInt32* tables) {
	if (count == 0) {
		return false;
	}
	for (UInt32 i = 0; i < count; ++i) {
		if (RuleCollides(walker[i], apart, tables) || RuleCollides(apart, walker[i], tables)) {
			return false;
		}
	}
	return true;
}

}  // namespace walkers
