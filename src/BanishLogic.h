#pragma once

// A dead Daedra goes back to Oblivion, process-free (docs/walkers-spec.md
// 4.22 idea 8, 4.22a step 5b; Nadi, 2026-11-16: "go 5a 5b 5c 6"): some
// seconds after it fell an effect shader plays on it, then its body is taken
// out of the world and its things are left in a small pile (the game's own
// LootBag, Oblivion.esm 0000000E: the sack the engine drops things in, no
// items of its own) - fewer bodies, cheaper frames.
//
// Only the Daedra the plugin placed (the bands, the sieges, the camps' and
// the held dungeons' attacks): those are references made in the game (FF),
// which nothing else ever removes. The game's own Daedra - an Oblivion
// gate's, a shrine's, a spawn the replacement turned into a Daedra - stay as
// the game keeps them: a disabled reference of the game's might stay
// disabled through its cell's reset (CorpseLogic.h).
//
// The shader: the game has no summon-end shader to take - the summon effects
// of Oblivion.esm (ZDRE, ZSCA, ZXIV, ...) name none (MGEF DATA's effect
// shader 0, read 2026-11-16), and the EXE names only three shaders by ID
// (0x144-0x146: absorb, reflect, life detected). So [Invaders] BanishShader
// picks one, by default effectEnchantConjuration (0018B576), the school's
// own.

#include "Types.h"

namespace walkers {

inline constexpr UInt32 kLootBag = 0x0000000E;
inline constexpr UInt32 kBanishShaderDefault = 0x0018B576;  // effectEnchantConjuration
inline constexpr UInt32 kBanishMost = 160;

// [Invaders] in OblivionWalkers.ini.
struct BanishSettings {
	float seconds = 10.0f;        // BanishSeconds: dead this long, the shader plays; 0 never banished
	float fadeSeconds = 2.0f;     // the shader this long before the body goes
	UInt32 shader = kBanishShaderDefault;  // BanishShader
	float pileUnits = 8000.0f;    // a pile goes once the player is this far (CorpseLootUnits)
};

enum class BanishStage : UInt8 { Watching, Fading };

struct BanishEntry {
	UInt32 ref = 0;
	float down = 0.0f;  // seconds dead
	BanishStage stage = BanishStage::Watching;
};

struct BanishList {
	BanishEntry items[kBanishMost];
	UInt32 count = 0;
};

// A Daedra the plugin placed, watched from now on; false when full, not
// one made in the game (FF), or watched already.
inline bool WatchForBanish(BanishList& b, UInt32 refId) {
	if ((refId >> 24) != 0xFF || b.count >= kBanishMost) {
		return false;
	}
	for (UInt32 i = 0; i < b.count; ++i) {
		if (b.items[i].ref == refId) {
			return false;
		}
	}
	b.items[b.count++] = BanishEntry{refId, 0.0f, BanishStage::Watching};
	return true;
}

inline bool WatchedForBanish(const BanishList& b, UInt32 refId) {
	for (UInt32 i = 0; i < b.count; ++i) {
		if (b.items[i].ref == refId) {
			return true;
		}
	}
	return false;
}

// What one look does to one watched Daedra.
enum class BanishAction : UInt8 { None, Shader, Banish, Forget };

// One watched Daedra over `dt` seconds: alive (`dead` false) its time
// starts again; gone from the game (`exists` false) it is forgotten; dead
// for BanishSeconds the shader, BanishFade later the banishing.
inline BanishAction StepBanishOne(BanishEntry& e, const BanishSettings& s, bool exists, bool dead, float dt) {
	if (!exists) {
		return BanishAction::Forget;
	}
	if (!dead) {
		e.down = 0.0f;
		e.stage = BanishStage::Watching;
		return BanishAction::None;
	}
	if (s.seconds <= 0.0f) {
		return BanishAction::None;
	}
	e.down += dt;
	if (e.stage == BanishStage::Watching && e.down >= s.seconds) {
		e.stage = BanishStage::Fading;
		return BanishAction::Shader;
	}
	if (e.stage == BanishStage::Fading && e.down >= s.seconds + s.fadeSeconds) {
		return BanishAction::Banish;
	}
	return BanishAction::None;
}

// The entries a look marked Banish or Forget taken off the list.
inline void DropBanished(BanishList& b, const bool* drop) {
	UInt32 kept = 0;
	for (UInt32 i = 0; i < b.count; ++i) {
		if (!drop[i]) {
			b.items[kept++] = b.items[i];
		}
	}
	b.count = kept;
}

// A form ID a script's variable got from a reference function, read off the
// variable's double: the game keeps a reference's ID in its low four bytes
// (OBSE's ScriptEventList::Var), a float variable a number - whichever reads
// as a placed reference (FF) is taken; 0 neither.
inline UInt32 RefFromVariable(UInt64 bits, double value) {
	const UInt32 low = static_cast<UInt32>(bits & 0xFFFFFFFFu);
	if ((low >> 24) == 0xFF) {
		return low;
	}
	if (value >= 4278190080.0 && value < 4294967296.0) {  // FF000000..FFFFFFFF as a number
		return static_cast<UInt32>(value);
	}
	return 0;
}

// Whether a pile goes now: the player this far or farther (-1: not known,
// it stays).
inline bool PileGoes(const BanishSettings& s, float distance) { return distance >= 0.0f && distance >= s.pileUnits; }

}  // namespace walkers
