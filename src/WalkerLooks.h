#pragma once

// The walkers' looks, process-free (docs/walkers-spec.md 4.1, "Twenty looks"
// and "MOO's looks"; Nadi, 2026-10-27: "brauchen mer diversität an
// zombies"). Every Walker creature of the ESP (tools/esp-write.py) wears one
// zombie body part set:
//   - twenty from Oblivion.esm's own zombies - 0x801..0x804, the four copies
//     of CreatureZombie1..4, and 0x90E..0x91D;
//   - forty-four from Maskar's Oblivion Overhaul's zombie skeleton
//     (MOO\zombie\skeleton.nif): its default set 0x91E..0x925, and its race
//     zombies - Argonian 0x926..0x92F, Orc 0x930..0x939, High Elf
//     0x93A..0x941, Dark Elf 0x942..0x949.
// The MOO looks name mesh paths only, no MOO form, so the ESP needs no MOO
// master; their meshes come with MOO's archives, so they are used only when
// MOO is loaded (Nadi, 2026-10-27: one ESP). A fallen Argonian, Orc, High
// Elf or Dark Elf then rises as a zombie of his race, everyone else as one
// of the vanilla or MOO default looks; the spawns draw from all of them.

#include "Types.h"

namespace walkers {

enum class LookFamily : UInt8 { Vanilla, MooDefault, Argonian, Orc, HighElf, DarkElf };

struct WalkerLook {
	UInt32 localId;
	LookFamily family;
};

inline constexpr UInt32 kVanillaLooks = 20;
inline constexpr UInt32 kWalkerCount = 64;

namespace looks {
inline constexpr LookFamily V = LookFamily::Vanilla;
inline constexpr LookFamily D = LookFamily::MooDefault;
inline constexpr LookFamily A = LookFamily::Argonian;
inline constexpr LookFamily O = LookFamily::Orc;
inline constexpr LookFamily H = LookFamily::HighElf;
inline constexpr LookFamily E = LookFamily::DarkElf;
}  // namespace looks

// In the ESP's order (tools/esp-write.py): the vanilla ones first, as the
// variant list holds them (VariantKeeps).
inline constexpr WalkerLook kWalkerLooks[kWalkerCount] = {
	{0x801, looks::V}, {0x802, looks::V}, {0x803, looks::V}, {0x804, looks::V}, {0x90E, looks::V}, {0x90F, looks::V},
	{0x910, looks::V}, {0x911, looks::V}, {0x912, looks::V}, {0x913, looks::V}, {0x914, looks::V}, {0x915, looks::V},
	{0x916, looks::V}, {0x917, looks::V}, {0x918, looks::V}, {0x919, looks::V}, {0x91A, looks::V}, {0x91B, looks::V},
	{0x91C, looks::V}, {0x91D, looks::V}, {0x91E, looks::D}, {0x91F, looks::D}, {0x920, looks::D}, {0x921, looks::D},
	{0x922, looks::D}, {0x923, looks::D}, {0x924, looks::D}, {0x925, looks::D}, {0x926, looks::A}, {0x927, looks::A},
	{0x928, looks::A}, {0x929, looks::A}, {0x92A, looks::A}, {0x92B, looks::A}, {0x92C, looks::A}, {0x92D, looks::A},
	{0x92E, looks::A}, {0x92F, looks::A}, {0x930, looks::O}, {0x931, looks::O}, {0x932, looks::O}, {0x933, looks::O},
	{0x934, looks::O}, {0x935, looks::O}, {0x936, looks::O}, {0x937, looks::O}, {0x938, looks::O}, {0x939, looks::O},
	{0x93A, looks::H}, {0x93B, looks::H}, {0x93C, looks::H}, {0x93D, looks::H}, {0x93E, looks::H}, {0x93F, looks::H},
	{0x940, looks::H}, {0x941, looks::H}, {0x942, looks::E}, {0x943, looks::E}, {0x944, looks::E}, {0x945, looks::E},
	{0x946, looks::E}, {0x947, looks::E}, {0x948, looks::E}, {0x949, looks::E},
};

// The race families of Oblivion.esm's races (its RACE records, read
// 2026-10-27): Argonian 00023FE9, Orc 000191C0, High Elf 00019204, Dark Elf
// 000191C1. Every other race: no MOO look of its own.
inline LookFamily RaceFamily(UInt32 raceId) {
	switch (raceId) {
		case 0x00023FE9:
			return LookFamily::Argonian;
		case 0x000191C0:
			return LookFamily::Orc;
		case 0x00019204:
			return LookFamily::HighElf;
		case 0x000191C1:
			return LookFamily::DarkElf;
		default:
			return LookFamily::Vanilla;
	}
}

// A look the game can show: the vanilla ones always, MOO's with MOO loaded.
inline bool LookUsable(LookFamily family, bool mooLoaded) { return family == LookFamily::Vanilla || mooLoaded; }

// The walkers one fallen of `raceId` may rise as, from the `count` found
// (`bases`, their `families`), into `out`; their number. With MOO, a race of
// its own: that race's looks; everyone else: the vanilla and MOO's default
// looks. Without MOO, or none of the race's looks found: the vanilla ones.
inline UInt32 RiseCandidates(const UInt32* bases, const LookFamily* families, UInt32 count, UInt32 raceId,
                             bool mooLoaded, UInt32* out) {
	const LookFamily race = RaceFamily(raceId);
	UInt32 n = 0;
	if (mooLoaded && race != LookFamily::Vanilla) {
		for (UInt32 i = 0; i < count; ++i) {
			if (families[i] == race) {
				out[n++] = bases[i];
			}
		}
		if (n > 0) {
			return n;
		}
	}
	for (UInt32 i = 0; i < count; ++i) {
		if (families[i] == LookFamily::Vanilla || (mooLoaded && families[i] == LookFamily::MooDefault)) {
			out[n++] = bases[i];
		}
	}
	return n;
}

// The look of the ESP's record at `localId`; false for no walker.
inline bool FamilyOfLocal(UInt32 localId, LookFamily& family) {
	for (const WalkerLook& look : kWalkerLooks) {
		if (look.localId == localId) {
			family = look.family;
			return true;
		}
	}
	return false;
}

// The variant list's entry for a walker of `family`: kept, or - without
// MOO - cut off with every entry after it (the MOO looks follow the vanilla
// ones in the list, so the cut leaves exactly the vanilla entries).
inline bool VariantKeeps(LookFamily family, bool mooLoaded) { return LookUsable(family, mooLoaded); }

}  // namespace walkers
