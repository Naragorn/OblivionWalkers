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
// (0x144-0x146: absorb, reflect, life detected). So [Daedra] BanishShader
// picks one, by default effectEnchantConjuration (0018B576), the school's
// own.

#include "Bytes.h"
#include "Types.h"

namespace walkers {

inline constexpr UInt32 kLootBag = 0x0000000E;
inline constexpr UInt32 kBanishShaderDefault = 0x0018B576;  // effectEnchantConjuration
inline constexpr UInt32 kBanishMost = 160;

// [Daedra] in OblivionWalkers.ini.
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

// The bag found by its ID (Banish.cpp PlaceBag): scanned from the newest ID
// seen, at most kBagScanMost, until kBagMissesMost IDs in a row are empty.
inline constexpr UInt32 kBagScanMost = 4096;
inline constexpr UInt32 kBagMissesMost = 64;
inline bool IsBagRef(UInt32 baseFormId) { return baseFormId == kLootBag; }
// Where the next scan starts: past the bag found, else at the newest of the
// last start and the corpse's ID.
inline UInt32 NextBagScan(UInt32 from, UInt32 corpse, UInt32 bag) {
	if (bag != 0) {
		return bag + 1;
	}
	return from > corpse ? from : corpse;
}

// Whether a pile goes now: the player this far or farther (-1: not known,
// it stays).
inline bool PileGoes(const BanishSettings& s, float distance) { return distance >= 0.0f && distance >= s.pileUnits; }

// The piles in the world (Banish.cpp): each bag's ID and whether it was
// looked into a second after (an empty one is taken away at once). They go
// into the co-save ('PILE'; Nadi, 2026-11-16: "die beutesäcke ... lücke") -
// read back after a load, they go once the player is far, as before.
inline constexpr UInt32 kPilesMost = 64;

struct PileList {
	UInt32 ids[kPilesMost] = {};
	bool checked[kPilesMost] = {};
	UInt32 count = 0;
};

// A bag made in the game (FF), not noted yet; false when full.
inline bool NotePile(PileList& p, UInt32 id, bool checked) {
	if ((id >> 24) != 0xFF || p.count >= kPilesMost) {
		return false;
	}
	for (UInt32 i = 0; i < p.count; ++i) {
		if (p.ids[i] == id) {
			return false;
		}
	}
	p.checked[p.count] = checked;
	p.ids[p.count++] = id;
	return true;
}

// Pile `at` taken off the list, the rest kept in order.
inline void DropPile(PileList& p, UInt32 at) {
	if (at >= p.count) {
		return;
	}
	for (UInt32 i = at + 1; i < p.count; ++i) {
		p.ids[i - 1] = p.ids[i];
		p.checked[i - 1] = p.checked[i];
	}
	--p.count;
}

inline constexpr UInt32 kPileRecordType = 0x50494C45;  // 'PILE'
inline constexpr UInt32 kPileRecordVersion = 1;
inline constexpr UInt32 kPileImageMost = 4 * kPilesMost;

// The bags' IDs, a word each, as many as fit.
inline UInt32 PileImage(const PileList& p, UInt8* out, UInt32 capacity) {
	UInt32 n = 0;
	for (UInt32 i = 0; i < p.count && 4 * (n + 1) <= capacity; ++i) {
		Put32(out + 4 * n++, p.ids[i]);
	}
	return 4 * n;
}

// After a load: each bag by its loaded ID (`resolve`, the co-save's
// ResolveRefID; false: gone from the game), checked. How many were read.
inline UInt32 ReadPileImage(PileList& p, const UInt8* in, UInt32 length, bool (*resolve)(UInt32, UInt32&)) {
	p = PileList{};
	UInt32 read = 0;
	for (UInt32 at = 0; at + 4 <= length; at += 4) {
		UInt32 id = 0;
		if (resolve(Get32(in + at), id) && NotePile(p, id, true)) {
			++read;
		}
	}
	return read;
}

}  // namespace walkers
