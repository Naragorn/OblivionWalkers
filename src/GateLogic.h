#pragma once

// Which Oblivion gates are open, process-free (docs/walkers-spec.md 4.22,
// step 3's groundwork; built 2026-11-15, not wired in).
//
// Read in Oblivion.exe 1.2.416 (2026-11-15, the console's command table
// 0x00B0E878 and its eval): GetDestroyed (opcode 0x10CB) answers bit 13 of
// the reference's form flags, (+0x08 >> 13) & 1 - its eval 0x004F82C0;
// SetDestroyed (0x10CC) sets or clears 0x2000 there through 0x0046AA50 and
// marks the form changed (vtable +0x40). The disabled flag is 0x800 of the
// same word (as Camps.cpp reads it). Read off Oblivion.esm: a gate is there
// when its enable parent is enabled (OblivionGateRandomScript enables it),
// closed when the door reads destroyed - the door stays enabled
// (OblivionGates.h, tools/oblivion-gates.py). So: open = the door and its
// parent enabled, the door not destroyed. Not seen in the game: a gate's
// flags read while it opens and after its sigil stone is taken.

#include "OblivionGates.h"
#include "Types.h"

namespace walkers {

inline constexpr UInt32 kFormFlagDisabled = 0x00000800;
inline constexpr UInt32 kFormFlagDestroyed = 0x00002000;

// `doorFlags` and `parentFlags` the references' form flags (+0x08);
// `hasParent` false for a door without one (none in Oblivion.esm's 103).
inline bool GateOpen(UInt32 doorFlags, bool hasParent, UInt32 parentFlags) {
	return (doorFlags & (kFormFlagDisabled | kFormFlagDestroyed)) == 0 &&
	       (!hasParent || (parentFlags & kFormFlagDisabled) == 0);
}

// A gate closed by the player: destroyed, whatever its parent.
inline bool GateClosed(UInt32 doorFlags) { return (doorFlags & kFormFlagDestroyed) != 0; }

}  // namespace walkers
