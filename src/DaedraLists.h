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
