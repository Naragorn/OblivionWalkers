#pragma once

// No skill trained by blows on and from walkers, process-free
// (docs/walkers-spec.md 4.3, `[Zombies] NoSkillGain=1`; Nadi, 2026-10-23:
// on by default, 2026-10-27: "go A").
//
// Read in Oblivion.exe 1.2.416 with REA (2026-10-27): the player's skill use
// is PlayerCharacter's virtual at vtable+0x39C (slot 0xE7, the vtable at
// 0x00A73A0C; its function 0x00668C30, thiscall (skill, use index, times)),
// which adds the use's experience (0x00668B30) and raises the skill when it
// is full (0x00668070, where xOBSE's OnSkillUp sits). 19 call sites read the
// slot, among them AttackHandling 0x005FEBF0 and the player's attack
// 0x005FCAB0. None of them hands the hit's target over, so the plugin keeps
// the frame's last hit from xOBSE's OnHit event and holds a blow skill's use
// back when that hit was between the player and a walker.

#include "Types.h"

namespace walkers {

// The skills' actor value codes (TESSkill::skill, the codes 0x0C..0x20 the
// engine's skill functions take; 0x00668070 tests 0x0E, 0x10 and 0x11 for
// the weapon skills' perks).
inline constexpr UInt32 kSkillBlade = 0x0E;
inline constexpr UInt32 kSkillBlock = 0x0F;
inline constexpr UInt32 kSkillBlunt = 0x10;
inline constexpr UInt32 kSkillHandToHand = 0x11;
inline constexpr UInt32 kSkillHeavyArmor = 0x12;
inline constexpr UInt32 kSkillDestruction = 0x16;
inline constexpr UInt32 kSkillAthletics = 0x0D;
inline constexpr UInt32 kSkillAcrobatics = 0x1A;
inline constexpr UInt32 kSkillLightArmor = 0x1B;
inline constexpr UInt32 kSkillMarksman = 0x1C;

inline constexpr UInt32 kPlayerSkillUseSlot = 0x00A73A0C + 0x39C;
inline constexpr UInt32 kPlayerSkillUse = 0x00668C30;

// The skills a blow trains: the weapon and hand, the block, the armor hit.
inline bool IsBlowSkill(UInt32 skill) {
	return skill == kSkillBlade || skill == kSkillBlock || skill == kSkillBlunt || skill == kSkillHandToHand ||
	       skill == kSkillHeavyArmor || skill == kSkillLightArmor || skill == kSkillMarksman;
}

// The frame's last hit as OnHit gave it.
struct SkillHit {
	bool thisFrame;
	bool targetWalker;
	bool attackerWalker;
	bool targetPlayer;
	bool attackerPlayer;
};

inline bool HoldsSkillUse(bool setting, UInt32 skill, const SkillHit& hit) {
	if (!setting || !hit.thisFrame || !IsBlowSkill(skill)) {
		return false;
	}
	return (hit.attackerPlayer && hit.targetWalker) || (hit.targetPlayer && hit.attackerWalker);
}

// The call site a skill use returns to (read with REA 2026-10-27):
// - 0x005FFCA4, after `call edx` at 0x005FFCA2 in AttackHandling: the hit
//   actor's armor (Light 0x1B or Heavy 0x12), ESI the hit actor, EDI the
//   attacker - so the attacker is read off EDI at the hook's entry;
// - 0x005FD846, after `call eax` at 0x005FD844 in the attack update
//   0x005FCAB0: the attacker's weapon skill (via +0x21C), not reached by
//   OBVR's hand-tracked strikes, which call AttackHandling directly;
// - 0x005F5C0B, after `call edx` at 0x005F5C09 in 0x005F5B70 (thiscall:
//   ECX, kept in EBP, the attacker; [esp+0x18], kept in ESI, the blocker -
//   its "%.20s blocks %.0f%% of %.20s's blow!" names them so): the
//   blocker's Block (0x0F), the attacker in EBP.
enum class SkillSite : UInt8 { Other, ArmorOfHit, WeaponOfAttack, BlockOfHit };

inline constexpr UInt32 kArmorOfHitReturn = 0x005FFCA4;
inline constexpr UInt32 kWeaponOfAttackReturn = 0x005FD846;
inline constexpr UInt32 kBlockOfHitReturn = 0x005F5C0B;

inline SkillSite SiteOf(UInt32 returnAddress) {
	if (returnAddress == kArmorOfHitReturn) {
		return SkillSite::ArmorOfHit;
	}
	if (returnAddress == kBlockOfHitReturn) {
		return SkillSite::BlockOfHit;
	}
	if (returnAddress == kWeaponOfAttackReturn) {
		return SkillSite::WeaponOfAttack;
	}
	return SkillSite::Other;
}

// At the armor and the block site the attacker is known: a walker's blow
// trains nothing.
inline bool HoldsSiteUse(bool setting, SkillSite site, bool attackerWalker) {
	return setting && (site == SkillSite::ArmorOfHit || site == SkillSite::BlockOfHit) && attackerWalker;
}

}  // namespace walkers
