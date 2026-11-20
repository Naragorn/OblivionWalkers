#pragma once

// Who comes - the two switches and what follows from them, process-free
// (docs/walkers-spec.md 4.22 and 4.24; Nadi, 2026-11-20).
//
// `[Walkers] Mode` (WalkersMode): 0 off, 1 only the game's zombies, 2 the
// apocalypse. `[Daedra] Mode` (DaedraMode): 0 off, 1 the Daedra out of the
// open gates as enemies of the walkers, 2 as their allies. The old single
// kind stays as a value derived from the two (KindOf): the helpers that
// ask "who is in play" and "do they fight" read it. Each group keeps or
// drops the walkers' rules as 4.22's table has them.

#include "CategoryLogic.h"
#include "DaedraLists.h"
#include "Types.h"

namespace walkers {

// [Walkers] Mode (4.24): 0 none, 1 only the game's zombies become walkers
// (nobody rises, no hordes), 2 the apocalypse (every category as set, the
// dead rise, hordes, sieges, camps, held dungeons).
enum class WalkersMode : UInt8 { Off = 0, Zombies = 1, Apocalypse = 2 };
// [Daedra] Mode (4.22, 4.24): 0 none, 1 out of the open gates, at war with
// the walkers, 2 out of the open gates, the walkers' allies.
enum class DaedraMode : UInt8 { Off = 0, Enemies = 1, Allies = 2 };
// Derived from the two switches (KindOf): which groups are in play and how
// they stand. None: neither.
enum class InvaderKind : UInt8 { Walkers = 0, Daedra = 1, Both = 2, Allied = 3, None = 4 };
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

// [Walkers] Mode and [Daedra] in OblivionWalkers.ini.
struct InvaderSettings {
	WalkersMode walkers = WalkersMode::Zombies;  // [Walkers] Mode: 1 by default (4.24)
	DaedraMode daedra = DaedraMode::Off;         // [Daedra] Mode: 0 by default
	UInt32 daedraPercent = 50;                   // SharePercent: the Daedra's share where both can come
	bool summons = false;                        // Summons: the summoners keep their summons (idea 11, off)
	bool afterMainQuest = false;                 // AfterMainQuest (step 6): off - they stop with the main quest
};

// The INI's numbers; anything but 0, 1 and 2 is the key's default, as
// without the key.
inline WalkersMode WalkersModeFromIni(int value) {
	switch (value) {
		case 0: return WalkersMode::Off;
		case 2: return WalkersMode::Apocalypse;
		default: return WalkersMode::Zombies;
	}
}
inline DaedraMode DaedraModeFromIni(int value) {
	switch (value) {
		case 1: return DaedraMode::Enemies;
		case 2: return DaedraMode::Allies;
		default: return DaedraMode::Off;
	}
}

inline bool WalkersOn(const InvaderSettings& s) { return s.walkers != WalkersMode::Off; }
inline bool Apocalypse(const InvaderSettings& s) { return s.walkers == WalkersMode::Apocalypse; }
inline bool DaedraOn(const InvaderSettings& s) { return s.daedra != DaedraMode::Off; }
// A side for sieges, camp attacks and held dungeons exists: the apocalypse's
// walkers or the Daedra (4.24).
inline bool SidesExist(const InvaderSettings& s) { return Apocalypse(s) || DaedraOn(s); }
// All switches off: the mod does nothing (4.24).
inline bool ModOff(const InvaderSettings& s) { return !WalkersOn(s) && !DaedraOn(s); }

// The derived kind: who is in play and how they stand.
inline InvaderKind KindOf(const InvaderSettings& s) {
	if (!DaedraOn(s)) {
		return WalkersOn(s) ? InvaderKind::Walkers : InvaderKind::None;
	}
	if (!WalkersOn(s)) {
		return InvaderKind::Daedra;
	}
	return s.daedra == DaedraMode::Allies ? InvaderKind::Allied : InvaderKind::Both;
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
	if (kind == InvaderKind::None) {
		return false;
	}
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

// Whether one fallen gets up as a walker: only with the apocalypse (4.24:
// Walkers Mode 1 raises nobody), never a Daedra; the Daedra's kills rise
// too (idea 2; Nadi 2026-11-15 for allies).
inline bool MayRise(const InvaderSettings& s, bool fallenIsDaedra) { return !fallenIsDaedra && Apocalypse(s); }

// The group a replaced spawn goes to, or none (4.22, 4.24). `daedraEverywhere`:
// [Daedra] EverywhereUntilGatesOpen with no gate open (BandLogic.h) - the only
// time the replacement writes Daedra. Then the replaced entries go to the
// Daedra by SharePercent on `roll` (0..99), all of them with no walkers.
enum class SpawnTarget : UInt8 { None, Walker, Daedra };
inline SpawnTarget ReplacementTarget(const InvaderSettings& s, bool daedraEverywhere, UInt32 roll) {
	const bool daedra = daedraEverywhere && DaedraOn(s);
	if (!WalkersOn(s) && !daedra) {
		return SpawnTarget::None;
	}
	if (!daedra) {
		return SpawnTarget::Walker;
	}
	if (!WalkersOn(s)) {
		return SpawnTarget::Daedra;
	}
	return roll < s.daedraPercent ? SpawnTarget::Daedra : SpawnTarget::Walker;
}

// The side of a siege, a camp's attack or a held dungeon's attackers (4.24):
// the walkers only with the apocalypse, the Daedra only with a Daedra mode;
// both by SharePercent on `roll` (0..99); none: nobody - the caller starts
// nothing.
enum class Side : UInt8 { Walkers, Daedra, Nobody };
inline Side SideOf(const InvaderSettings& s, UInt32 roll) {
	const bool walkers = Apocalypse(s);
	const bool daedra = DaedraOn(s);
	if (!walkers && !daedra) {
		return Side::Nobody;
	}
	if (!daedra) {
		return Side::Walkers;
	}
	if (!walkers) {
		return Side::Daedra;
	}
	return roll < s.daedraPercent ? Side::Daedra : Side::Walkers;
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

// [ReplaceCategories] "Zombies" (CategoryLogic.h; KindLogicTest checks the key).
inline constexpr UInt8 kZombiesCategory = 1;
// Walkers Mode 1 replaces the Zombies category alone, whatever
// [ReplaceCategories] says (4.24); any other mode leaves the choice to it.
inline bool CategoryAllowed(const InvaderSettings& s, UInt8 category) {
	return s.walkers != WalkersMode::Zombies || category == kZombiesCategory;
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
// change of SharePercent moves only the entries it must.
inline UInt32 EntryRoll(UInt32 entryAddress) {
	UInt32 h = entryAddress * 2654435761u;
	h ^= h >> 16;
	return h % 100;
}

// Idea 1, a war band's leader (Nadi, 2026-11-16: "go 2"): a Dremora two
// ranks above the band's men. The seven ranks' lists of Oblivion.esm
// (LL0Dremora0Churl100 .. LL0Dremora6Valkynaz100) and the player's level at
// which LL1Dremora100 first gives each (1, 8, 11, 14, 17, 21, 24; read
// 2026-11-15).
inline constexpr UInt32 kDremoraRankLists[7] = {0x00092DA6, 0x0003E9D1, 0x0003E9D2, 0x0003E9D3,
                                                0x0003E9D4, 0x0003E9D5, 0x0003E9D6};
inline constexpr UInt32 kDremoraRankLevels[7] = {1, 8, 11, 14, 17, 21, 24};

// The rank the game's lists give at `level`: the highest whose level is
// reached.
inline UInt32 DremoraRankAt(UInt32 level) {
	UInt32 rank = 0;
	for (UInt32 r = 0; r < 7; ++r) {
		rank = level >= kDremoraRankLevels[r] ? r : rank;
	}
	return rank;
}

// The leader's rank: two above the men's, the seventh at most.
inline UInt32 LeaderRank(UInt32 level) {
	const UInt32 r = DremoraRankAt(level) + 2;
	return r > 6 ? 6 : r;
}

// The form a Daedra spawn takes: an Oblivion.esm summoner or rank list its
// NOSUMMON twin while they are held back, any other form itself.
inline UInt32 InvaderForm(const InvaderSettings& s, UInt32 formId) {
	return HoldSummonersBack(s) ? NoSummonTwin(formId) : formId;
}

// The list a band's leader is placed from - its NOSUMMON twin while the
// summoners are held back (idea 11).
inline UInt32 LeaderList(const InvaderSettings& s, UInt32 level) {
	return InvaderForm(s, kDremoraRankLists[LeaderRank(level)]);
}

// The leader as an actor: one of the rank list's own entries in the file
// (DaedraLists.h kRankListActors), by the caller's roll. MOO's dynamic lists
// rewrite the rank lists in the game - placed through the list a band got two
// clannfear runts or one of MOO's creatures for a leader (daedra-leader.txt,
// 2026-11-17).
inline UInt32 LeaderActor(const InvaderSettings& s, UInt32 level, UInt32 roll) {
	return RankListActor(LeaderList(s, level), roll);
}

// A Daedra outpost's boss (4.22a step 5c, Nadi 2026-11-16 "5: passt"): a
// Dremora three ranks above the men's, the seventh at most.
inline UInt32 OutpostBossRank(UInt32 level) {
	const UInt32 r = DremoraRankAt(level) + 3;
	return r > 6 ? 6 : r;
}
inline UInt32 OutpostBossList(const InvaderSettings& s, UInt32 level) {
	return InvaderForm(s, kDremoraRankLists[OutpostBossRank(level)]);
}
// The boss as an actor, as LeaderActor.
inline UInt32 OutpostBossActor(const InvaderSettings& s, UInt32 level, UInt32 roll) {
	return RankListActor(OutpostBossList(s, level), roll);
}

// [Daedra] AfterMainQuest (4.22a step 6): off (0, the default), the
// Daedra stop once the main quest is done - Oblivion.esm's MQ16 (0002A38F)
// completed (TESQuest::questFlags +0x3C, bit 1). From then on the Daedra
// are off (4.24): no new band, siege, camp attack or held place of theirs;
// the walkers are untouched. What stands is fought out.
inline constexpr UInt32 kMainQuestEnd = 0x0002A38F;
inline constexpr UInt8 kQuestCompleted = 0x02;
inline bool DaedraStopped(bool afterMainQuest, UInt8 mainQuestEndFlags) {
	return !afterMainQuest && (mainQuestEndFlags & kQuestCompleted) != 0;
}
inline InvaderSettings InvadersNow(const InvaderSettings& s, bool stopped) {
	InvaderSettings now = s;
	if (stopped) {
		now.daedra = DaedraMode::Off;
	}
	return now;
}
// Whether the spawn replacement runs at all: with walkers, or for the Daedra
// alone while they come everywhere (`daedraEverywhere`, BandLogic.h). `now`
// is InvadersNow's answer.
inline bool ReplacementRuns(const InvaderSettings& now, bool daedraEverywhere) {
	return WalkersOn(now) || (daedraEverywhere && DaedraOn(now));
}

// The outpost's boss chest: Oblivion.esm's DungOblivionBossSack01
// (00037680, read 2026-11-16: the flesh sack of the game's own Oblivion
// bosses, its loot leveled - gold, gems, a weapon, Welkynd and Varla
// stones), placed by PlaceAtMe, so no record of the ESP's.
inline constexpr UInt32 kOutpostBossChest = 0x00037680;

}  // namespace walkers
