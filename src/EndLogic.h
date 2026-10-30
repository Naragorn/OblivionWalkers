#pragma once

// Only a lost head ends a walker, process-free (docs/walkers-spec.md 4.17;
// Nadi, 2026-10-25: "walker nur sterben wenn geköpft. default an. ansonsten
// resurrect[en] die immer"; 2026-10-30: "jo" to the build).
//
// A walker that falls gets up again, the same reference raised in place
// after RiseAgainSeconds plus a share of RiseAgainJitterSeconds, until its
// head is off. OBVR cuts heads (its [Hands] Dismemberment) and says so:
// 'WGDM' whether it cuts, 'WGHD' for each head it cut off or crushed. Without
// a cutter a power attack's kill ends a walker instead (Nadi, 2026-10-25, b);
// with one, never both rules. A walker without a head (ACBS No Head, made
// with the rule off) ends at its first death: it could never lose one - and
// with the rule on none is made (Nadi, 2026-10-27).

#include "Types.h"

namespace walkers {

// [Zombies] in OblivionWalkers.ini.
struct EndSettings {
	bool dieOnlyBeheaded = true;
	float riseAgainSeconds = 20.0f;
	float riseAgainJitterSeconds = 10.0f;
};

// A walker's death as the death watch reads it.
struct FallFacts {
	bool headless = false;   // its base has no head (ACBS No Head)
	bool headNoted = false;  // OBVR cut or crushed its head before the death
	bool cutterOn = false;   // OBVR said it cuts heads ('WGDM' 1)
	bool powerKill = false;  // the killer's blow was a power attack
};

enum class WalkerFall : UInt8 {
	RisesAgain,        // down for now: on the waiting list
	EndedRuleOff,      // DieOnlyBeheaded=0: killed once
	EndedHeadless,     // no head to lose
	EndedHead,         // its head is off or crushed
	EndedPowerAttack,  // no cutter, and the killing blow a power attack
};

inline WalkerFall WalkerFallVerdict(bool dieOnlyBeheaded, const FallFacts& f) {
	if (!dieOnlyBeheaded) {
		return WalkerFall::EndedRuleOff;
	}
	if (f.headless) {
		return WalkerFall::EndedHeadless;
	}
	if (f.headNoted) {
		return WalkerFall::EndedHead;
	}
	if (!f.cutterOn && f.powerKill) {
		return WalkerFall::EndedPowerAttack;
	}
	return WalkerFall::RisesAgain;
}

inline bool Ends(WalkerFall fall) { return fall != WalkerFall::RisesAgain; }

inline const char* WalkerFallText(WalkerFall fall) {
	switch (fall) {
		case WalkerFall::RisesAgain: return "down - it gets up again";
		case WalkerFall::EndedRuleOff: return "ended (DieOnlyBeheaded=0)";
		case WalkerFall::EndedHeadless: return "ended (it has no head to lose)";
		case WalkerFall::EndedHead: return "ended (its head is off)";
		case WalkerFall::EndedPowerAttack: return "ended (a power attack, and no head cutter)";
	}
	return "?";
}

// Seconds until a fallen walker gets up again; `unit` the caller's random
// number in 0..1.
inline float RiseAgainIn(const EndSettings& s, float unit) {
	const float u = unit < 0.0f ? 0.0f : (unit > 1.0f ? 1.0f : unit);
	return s.riseAgainSeconds + s.riseAgainJitterSeconds * u;
}

// From OBVR ('WGDM' in memory): one word, 1 when it cuts heads. Sent at its
// start and after an INI reload, again until a dispatch is taken.
inline constexpr UInt32 kDismemberMessage = 0x4D444757;
// From OBVR ('WGHD'): the reference, then the kind - 1 cut off, 2 crushed.
inline constexpr UInt32 kHeadMessage = 0x44484757;

enum class HeadKind : UInt8 { Cut = 1, Crushed = 2 };

inline bool DismemberOf(const void* data, UInt32 length, bool& on) {
	if (data == nullptr || length < 4) {
		return false;
	}
	on = *static_cast<const UInt32*>(data) != 0;
	return true;
}

inline bool HeadOf(const void* data, UInt32 length, UInt32& refId, HeadKind& kind) {
	if (data == nullptr || length < 8) {
		return false;
	}
	const UInt32* words = static_cast<const UInt32*>(data);
	if (words[0] == 0 || (words[1] != 1 && words[1] != 2)) {
		return false;
	}
	refId = words[0];
	kind = static_cast<HeadKind>(words[1]);
	return true;
}

// What a head off does to the reference.
enum class HeadAction : UInt8 {
	Ignore,      // no walker
	EndWaiting,  // it lies to rise again: off the list, the end
	Kill,        // alive still: killed (its death then finds the head noted)
	Note,        // dead, not waiting: noted for its death watch
};

inline HeadAction HeadActionFor(bool walker, bool waiting, bool dead) {
	if (!walker) {
		return HeadAction::Ignore;
	}
	if (waiting) {
		return HeadAction::EndWaiting;
	}
	return dead ? HeadAction::Note : HeadAction::Kill;
}

// The heads OBVR reported, for the deaths that come after them: a ring, the
// oldest giving way. Only the order of a few frames needs it, so no co-save.
inline constexpr UInt32 kHeadNotesMost = 32;

struct HeadNotes {
	UInt32 ids[kHeadNotesMost] = {};
	UInt32 count = 0;
	UInt32 next = 0;
};

inline bool HeadNoted(const HeadNotes& n, UInt32 refId) {
	for (UInt32 i = 0; refId != 0 && i < n.count; ++i) {
		if (n.ids[i] == refId) {
			return true;
		}
	}
	return false;
}

inline void NoteHead(HeadNotes& n, UInt32 refId) {
	if (refId == 0 || HeadNoted(n, refId)) {
		return;
	}
	n.ids[n.next] = refId;
	n.next = (n.next + 1) % kHeadNotesMost;
	if (n.count < kHeadNotesMost) {
		++n.count;
	}
}

// Whether the spawns and the rise may make walkers without a head.
inline bool SpawnsHeadless(bool dieOnlyBeheaded) { return !dieOnlyBeheaded; }

}  // namespace walkers
