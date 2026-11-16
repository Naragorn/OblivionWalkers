#pragma once

// Invaders from Oblivion, step 1 - the kind's rules, process-free
// (docs/walkers-spec.md 4.22; Nadi, 2026-11-13: "0 zombies, 1 Dremora, 2
// both", decided the same day; built 2026-11-15, not wired in yet).
//
// `[Invaders] Mode` (named Kind until 2026-11-16, Nadi: a better key) says
// which groups are in play: 0 the walkers as they
// are, 1 the Daedra in their place, 2 both at war with each other, 3 both
// as allies (Nadi, 2026-11-15: walkers and Daedra friends, the fallen rise
// as walkers). Each group keeps or drops the
// walkers' rules as 4.22's table has them; every "is a walker" test of the
// plugin will ask RulesOf for its group instead of taking the walkers' rules
// for granted. Nothing here touches the game: Rise.cpp, Grab.cpp and the
// spawn replacement still take the walkers' rules (step 1's wiring, not
// built - it waits for a harness run).

#include "CategoryLogic.h"
#include "DaedraLists.h"
#include "Types.h"

namespace walkers {

enum class InvaderKind : UInt8 { Walkers = 0, Daedra = 1, Both = 2, Allied = 3 };
enum class Group : UInt8 { Walker, Daedra };

// The spawn replacement's Daedra (step 1): Oblivion.esm's LL1Daedra100
// (00034097, read 2026-11-15: the beasts and the seven Dremora ranks by the
// player's level, every level at or below his) - placed 176 times in the
// game's own Oblivion and shrines, so never rewritten itself. The ESP
// carries two lists in the free IDs after the held dungeons' beds
// (0xA93..0xAA3, before the grab's shock sounds): one that names
// LL1Daedra100 (a mod's change to it is taken), and its copy with the
// summoners' NOSUMMON twins (DaedraLists.h) for Summons=0. A replaced
// entry names one of the two (tools/esp-write.py).
inline constexpr UInt32 kGameDaedraList = 0x00034097;
inline constexpr UInt32 kInvaderDaedraLocalId = 0xA93;
inline constexpr UInt32 kInvaderDaedraNoSummonLocalId = 0xA94;

// [Invaders] in OblivionWalkers.ini.
struct InvaderSettings {
	InvaderKind kind = InvaderKind::Walkers;  // Kind: 0 by default (decided 2026-11-13)
	UInt32 daedraPercent = 50;                 // DaedraPercent: Kind 2's share of the replaced spawns
	bool summons = false;                      // Summons: the summoners keep their summons (idea 11, off)
};

// The INI's number; anything but 1, 2 and 3 is the walkers - as without
// the switch.
inline InvaderKind KindFromIni(int value) {
	switch (value) {
		case 1: return InvaderKind::Daedra;
		case 2: return InvaderKind::Both;
		case 3: return InvaderKind::Allied;
		default: return InvaderKind::Walkers;
	}
}

// Both groups in play (Kind 2 or 3).
inline bool BothInPlay(InvaderKind kind) { return kind == InvaderKind::Both || kind == InvaderKind::Allied; }

// Whether the walkers and the Daedra fight each other: only Kind 2 (step 4,
// idea 2); Kind 3 makes them one side.
inline bool GroupsHostile(InvaderKind kind) { return kind == InvaderKind::Both; }

// Kind 3's side: the walker records join Oblivion.esm's DaedraFaction
// (00053FED - it reacts +100 to itself and to DremoraFaction, DremoraFaction
// +100 to it; read 2026-11-15). Kind 2 needs nothing: the game's own
// factions make walkers and Daedra foes already (daedra-fight.txt,
// 2026-11-15, one of the two dead within 45 s).
inline constexpr UInt32 kDaedraFaction = 0x00053FED;
inline bool WalkersJoinDaedra(InvaderKind kind) { return kind == InvaderKind::Allied; }

inline bool InPlay(InvaderKind kind, Group group) {
	return group == Group::Walker ? kind != InvaderKind::Daedra : kind != InvaderKind::Walkers;
}

// What a group takes of the walkers' rules (4.22's table, row by row).
struct GroupRules {
	bool rise;            // the fallen rise as it (4.1)
	bool headRule;        // only a lost head ends one (4.17, 4.17b)
	bool grab;            // grabs and bites (4.19)
	bool slowAggression;  // 4.21
	bool disguise;        // walker blood fools it (4.20.1)
	bool walkOnly;        // walks at its own pace, never runs (4.4)
	bool marches;         // hordes or war bands to the cities, gate guards, sieges, news, camps, dungeons
	bool nightWorse;      // 4.20.5; the Daedra's is the weather (idea 9, off by default)
	bool scaledHealth;    // HealthPercent; the Daedra keep the game's leveling
};

inline constexpr GroupRules RulesOf(Group group) {
	return group == Group::Walker ? GroupRules{true, true, true, true, true, true, true, true, true}
	                              : GroupRules{false, false, false, false, false, false, true, false, false};
}

// Whether one fallen gets up as a walker: only while walkers are in play,
// never a Daedra; with Kind 2 and 3 those the Daedra kill rise too (idea 2;
// Nadi 2026-11-15 for Kind 3).
inline bool MayRise(InvaderKind kind, bool fallenIsDaedra) {
	return !fallenIsDaedra && InPlay(kind, Group::Walker);
}

// The group that takes a replaced spawn; `roll` 0..99 (Kind 2 and 3 only).
inline Group ReplacementGroup(const InvaderSettings& s, UInt32 roll) {
	switch (s.kind) {
		case InvaderKind::Walkers: return Group::Walker;
		case InvaderKind::Daedra: return Group::Daedra;
		case InvaderKind::Both:
		case InvaderKind::Allied: break;
	}
	return roll < s.daedraPercent ? Group::Daedra : Group::Walker;
}

// The Daedra among the replacement categories (CategoryLogic.h), by their
// keys: Atronachs, Clannfears, Daedroths, Scamps, SpiderDaedra, Xivilai,
// Dremora - the ones 4.22 names; the Shivering Isles' Hungers are Sheogorath's,
// not Dagon's, and stay a walkers' category. KindLogicTest checks the keys.
inline constexpr UInt8 kDaedraCategories[] = {6, 7, 8, 9, 10, 11, 39};
inline constexpr UInt32 kDaedraCategoryCount = sizeof(kDaedraCategories) / sizeof(kDaedraCategories[0]);

inline constexpr UInt8 DaedraCategoryIndex(UInt32 i) { return i < kDaedraCategoryCount ? kDaedraCategories[i] : kNoCategory; }

inline bool IsDaedraCategory(UInt8 category) {
	for (UInt8 c : kDaedraCategories) {
		if (c == category) {
			return true;
		}
	}
	return false;
}

// Whether a category switched `on` is replaced: with the Daedra in play
// their own categories stay the game's Daedra - they are the invaders.
inline bool CategoryReplaced(InvaderKind kind, UInt8 category, bool on) {
	return on && !(InPlay(kind, Group::Daedra) && IsDaedraCategory(category));
}

// Idea 11: the summoners (the Dremora's ranks, the spider daedra, the
// xivilai) multiply the actors with their summons - by default their
// NOSUMMON twins (DaedraLists.h).
inline bool HoldSummonersBack(const InvaderSettings& s) { return !s.summons; }

// The ESP list a replaced entry names when it goes to the Daedra.
inline UInt32 InvaderDaedraLocalId(const InvaderSettings& s) {
	return HoldSummonersBack(s) ? kInvaderDaedraNoSummonLocalId : kInvaderDaedraLocalId;
}

// A stable roll 0..99 for one list entry, from its address (ListData): the
// same entry takes the same group at every pass of a session, so a LINK
// change of DaedraPercent moves only the entries it must.
inline UInt32 EntryRoll(UInt32 entryAddress) {
	UInt32 h = entryAddress * 2654435761u;
	h ^= h >> 16;
	return h % 100;
}

// The form a Daedra spawn takes: an Oblivion.esm summoner or rank list its
// NOSUMMON twin while they are held back, any other form itself.
inline UInt32 InvaderForm(const InvaderSettings& s, UInt32 formId) {
	return HoldSummonersBack(s) ? NoSummonTwin(formId) : formId;
}

}  // namespace walkers
