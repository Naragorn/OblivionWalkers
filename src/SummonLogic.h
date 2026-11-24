#pragma once

// The player's summons, process-free (docs/walkers-spec.md 4.23 B):
// measured 2026-11-23 (harness necro-summon) - with Daedra Mode 2 the
// player's summoned Dremora counted as a Daedra, so the walkers and the
// necromancer were told to let go of it (20 + 20 StopCombat lines) while it
// struck them; with Mode 0 they fought back. A Daedra the player summoned is
// therefore no ally. The same holds for a companion's or another NPC's
// summon (2026-11-24): only a Daedra's own summons (a Dremora mage's) stay
// Daedra allies. Every Actor carries its MagicTarget at +0x68 (xOBSE
// GameObjects.h, Character and Creature alike).
//
// The player's active effects (xOBSE GameObjects.h MagicTarget: EffectNode
// {ActiveEffect* data; next}; GameMagicEffects.h): an ActiveEffect's
// EffectItem at +0x0C, its effect code at +0x00 ('Z' first: a summon,
// EffectSetting::IsSummonEffect); a SummonCreatureEffect's actor at +0x3C.

#include "Types.h"

namespace walkers {

constexpr UInt32 kActiveEffectItemOffset = 0x0C;
constexpr UInt32 kEffectItemCodeOffset = 0x00;
constexpr UInt32 kSummonActorOffset = 0x3C;
constexpr UInt32 kSummonWalkMost = 64;  // nodes walked at most - a broken list ends

// The actors of the summon effects in the list at `head`, at most `room`.
template <typename ReadFn>
UInt32 SummonsInList(UInt32 head, const ReadFn& read, UInt32* out, UInt32 room) {
	UInt32 n = 0;
	UInt32 node = head;
	for (UInt32 step = 0; node != 0 && step < kSummonWalkMost && n < room; ++step) {
		const UInt32 effect = read(node);
		node = read(node + 4);
		const UInt32 item = effect != 0 ? read(effect + kActiveEffectItemOffset) : 0;
		if (item == 0 || (read(item + kEffectItemCodeOffset) & 0xFF) != 'Z') {
			continue;
		}
		const UInt32 actor = read(effect + kSummonActorOffset);
		if (actor != 0) {
			out[n++] = actor;
		}
	}
	return n;
}

// The summons of the `casters` who are no Daedra (the player, companions,
// other NPCs), at most `room`; a caster 0 or repeated is skipped. `listOf`
// gives a caster's effect list head (0: none).
template <typename IsDaedraFn, typename ListFn, typename ReadFn>
UInt32 NonDaedraSummons(const UInt32* casters, UInt32 count, const IsDaedraFn& isDaedra, const ListFn& listOf,
                        const ReadFn& read, UInt32* out, UInt32 room) {
	UInt32 n = 0;
	for (UInt32 i = 0; i < count && n < room; ++i) {
		const UInt32 caster = casters[i];
		bool seen = caster == 0;
		for (UInt32 k = 0; k < i && !seen; ++k) {
			seen = casters[k] == caster;
		}
		if (seen || isDaedra(caster)) {
			continue;
		}
		n += SummonsInList(listOf(caster), read, out + n, room - n);
	}
	return n;
}

// A Daedra is the walkers' ally (Daedra Mode 2) unless one who is no Daedra
// summoned it.
inline bool DaedraAlly(bool daedra, UInt32 actor, const UInt32* summons, UInt32 count) {
	if (!daedra) {
		return false;
	}
	for (UInt32 i = 0; i < count; ++i) {
		if (summons[i] == actor) {
			return false;
		}
	}
	return true;
}

}  // namespace walkers
