#pragma once

// The boss guard's clean copies, process-free (docs/walkers-spec.md 4.6c;
// Nadi, 2026-11-17: "go 1. A"). The harness run of 2026-11-17 with
// [Replace] TestLogList=000340BA: the pass keeps LL1GoblinBossLvl100's four
// goblins, then MOO's dynamic lists rewrite all its entries in the game to
// MOOLL2RegionGoblin - an ordinary list nested in the boss list, which the
// replacement turns to walkers like any goblin list. So an ordinary list
// named in a boss list is pointed at a clean copy of itself: the copy names
// what the source named before the replacement (each entry's own creature),
// and is built anew whenever the source changes (MOO refills its region
// lists). The source stays replaced everywhere else.

#include "Types.h"

namespace walkers {

// What a boss list's entry names, as Replace.cpp reads it.
struct EntryTarget {
	bool isList = false;      // a leveled creature list (else a creature or a person)
	bool isBossList = false;  // a list kept by its own name - guards its own entries
	bool isOurs = false;      // the walkers' variant list or the Daedra's lists
	bool isCopy = false;      // one of the guard's clean copies
};

enum class EntryAction { Keep, Redirect, Restore };

// With the guard on ([Replace] KeepBosses and the list kept by its name), an
// ordinary nested list is pointed at its clean copy; off, a copy goes back to
// its source. Everything else stays: creatures and people are the pass's.
inline EntryAction BossEntryAction(bool guardOn, const EntryTarget& t) {
	if (!t.isList) {
		return EntryAction::Keep;
	}
	if (!guardOn) {
		return t.isCopy ? EntryAction::Restore : EntryAction::Keep;
	}
	return t.isBossList || t.isOurs || t.isCopy ? EntryAction::Keep : EntryAction::Redirect;
}

// The form a copy's entry names: the one the source's entry named before the
// pass pointed it at the walkers or the Daedra (Replace.cpp's g_original),
// else what it names now.
inline UInt32 CopySourceForm(UInt32 current, bool replaced, UInt32 original) {
	return replaced && original != 0 ? original : current;
}

// Lists nested in a copy are copied too, down to this depth - a guard
// against a cycle and a deep tree alike.
inline constexpr UInt32 kBossCopyDepthMost = 4;
inline bool CopyNestedAt(UInt32 depth) { return depth < kBossCopyDepthMost; }

// A copy is built when there is none yet or its source's entries changed
// since (ReplaceLogic.h ListEntriesHash over the source).
inline bool CopyNeedsBuild(bool exists, UInt32 hashWhenCopied, UInt32 hashNow) {
	return !exists || hashWhenCopied != hashNow;
}

}  // namespace walkers
