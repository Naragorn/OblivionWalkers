#pragma once

// Written by tools/daedra-lists.py off Oblivion.esm - do not edit; run it again.
// The Daedra's summoners and their NOSUMMON twins (docs/walkers-spec.md 4.22,
// idea 11): each original's spells (its leveled spell lists walked through)
// hold a summoning effect, its twin's none; a list's twin holds, entry for
// entry, the twins. The Dremora's melee and archers summon nothing (their
// NOSUMMON twins differ only in what they carry) - left out.

#include "Types.h"

namespace walkers {

struct SummonTwin {
	UInt32 original;  // Oblivion.esm's form ID
	UInt32 twin;
	const char* name;  // the original's editor ID
	bool list;         // a leveled list (LVLC), else an actor
};

inline constexpr SummonTwin kSummonTwins[] = {
	{0x00092DA5, 0x00000FCB, "Dremora0ChurlMage", false},  // ZCLA, ZDAE, ZFIA, ZSCA, ZSPD, ZSTA, ZXIV
	{0x00092D9A, 0x00000FCE, "Dremora1CaitiffMage", false},  // ZCLA, ZDAE, ZFIA, ZSCA, ZSPD, ZSTA, ZXIV
	{0x00092D9B, 0x00000FD1, "Dremora2KynvalMage", false},  // ZCLA, ZDAE, ZFIA, ZSCA, ZSPD, ZSTA, ZXIV
	{0x00092D9C, 0x00000FD5, "Dremora3KynreeveMage", false},  // ZCLA, ZDAE, ZFIA, ZSCA, ZSPD, ZSTA, ZXIV
	{0x00092D9D, 0x00000FD8, "Dremora4KynmarcherMage", false},  // ZCLA, ZDAE, ZFIA, ZSCA, ZSPD, ZSTA, ZXIV
	{0x00092D9E, 0x00000FDB, "Dremora5MarkynazMage", false},  // ZCLA, ZDAE, ZFIA, ZSCA, ZSPD, ZSTA, ZXIV
	{0x00092D9F, 0x00000FDF, "Dremora6ValkynazMage", false},  // ZCLA, ZDAE, ZFIA, ZSCA, ZSPD, ZSTA, ZXIV
	{0x0001E64C, 0x00000FE0, "CreatureSpiderDaedra", false},  // Z003
	{0x00032655, 0x00000FE2, "CreatureXivilai", false},  // ZCLA
	{0x0001E64D, 0x00000FE3, "CreatureXivilaiWeapon", false},  // ZCLA
	{0x0003E9D4, 0x00000FE4, "LL0Dremora4Kynmarcher100", true},  // list
	{0x0003E9D5, 0x00000FE5, "LL0Dremora5Markynaz100", true},  // list
	{0x0003E9D6, 0x00000FE6, "LL0Dremora6Valkynaz100", true},  // list
	{0x00092DA6, 0x00000FE7, "LL0Dremora0Churl100", true},  // list
	{0x0003E9D1, 0x00000FE8, "LL0Dremora1Caitiff100", true},  // list
	{0x0003E9D2, 0x00000FE9, "LL0Dremora2Kynval100", true},  // list
	{0x0003E9D3, 0x00000FEA, "LL0Dremora3Kynreeve100", true},  // list
	{0x0003265D, 0x00000FED, "LL0Xivilai100", true},  // list
};
inline constexpr UInt32 kSummonTwinCount = sizeof(kSummonTwins) / sizeof(kSummonTwins[0]);

// The Dremora rank lists and their twins with the actors their entries name in
// the file - MOO's dynamic lists rewrite the lists in the game, so the plugin
// places one of these actors itself (a band's leader, an outpost's boss).
inline constexpr UInt32 kRankActorsMost = 8;
struct RankListActors {
	UInt32 list;  // Oblivion.esm's form ID
	UInt32 count;
	UInt32 actors[kRankActorsMost];
};

inline constexpr RankListActors kRankListActors[] = {
	{0x00092DA6, 4, {0x00092DA0, 0x00092DA1, 0x00092DA2, 0x00092DA5}},  // LL0Dremora0Churl100
	{0x00000FE7, 4, {0x00000FCC, 0x00000FCC, 0x00000FCC, 0x00000FCB}},  // LL0Dremora0Churl100NOSUMMON
	{0x0003E9D1, 4, {0x0003EE70, 0x0003EE71, 0x00047389, 0x00092D9A}},  // LL0Dremora1Caitiff100
	{0x00000FE8, 4, {0x00000FCF, 0x00000FCF, 0x00000FCF, 0x00000FCE}},  // LL0Dremora1Caitiff100NOSUMMON
	{0x0003E9D2, 4, {0x0003E9C6, 0x0003EE72, 0x0004738A, 0x00092D9B}},  // LL0Dremora2Kynval100
	{0x00000FE9, 4, {0x00000FD3, 0x00000FD3, 0x00000FD3, 0x00000FD1}},  // LL0Dremora2Kynval100NOSUMMON
	{0x0003E9D3, 4, {0x0003EE7F, 0x0003EE80, 0x0004738B, 0x00092D9C}},  // LL0Dremora3Kynreeve100
	{0x00000FEA, 4, {0x00000FD6, 0x00000FD6, 0x00000FD6, 0x00000FD5}},  // LL0Dremora3Kynreeve100NOSUMMON
	{0x0003E9D4, 4, {0x0003E9C8, 0x0003EE75, 0x0004738C, 0x00092D9D}},  // LL0Dremora4Kynmarcher100
	{0x00000FE4, 4, {0x00000FD9, 0x00000FD9, 0x00000FD9, 0x00000FD8}},  // LL0Dremora4Kynmarcher100NOSUMMON
	{0x0003E9D5, 4, {0x0003EE79, 0x0003EE7A, 0x0004738D, 0x00092D9E}},  // LL0Dremora5Markynaz100
	{0x00000FE5, 4, {0x00000FDC, 0x00000FDC, 0x00000FDC, 0x00000FDB}},  // LL0Dremora5Markynaz100NOSUMMON
	{0x0003E9D6, 4, {0x0003E9CA, 0x0003EE7B, 0x0004738E, 0x00092D9F}},  // LL0Dremora6Valkynaz100
	{0x00000FE6, 4, {0x00000FC8, 0x00000FC8, 0x00000FC8, 0x00000FDF}},  // LL0Dremora6Valkynaz100NOSUMMON
};

// One actor of `list`'s file entries, by `roll`; `list` itself when it is not
// a rank list.
inline UInt32 RankListActor(UInt32 list, UInt32 roll) {
	for (const RankListActors& r : kRankListActors) {
		if (r.list == list && r.count > 0) {
			return r.actors[roll % r.count];
		}
	}
	return list;
}

// The twin of `formId` (Oblivion.esm's ID, load index 0), or itself.
inline UInt32 NoSummonTwin(UInt32 formId) {
	for (const SummonTwin& t : kSummonTwins) {
		if (t.original == formId) {
			return t.twin;
		}
	}
	return formId;
}

}  // namespace walkers
