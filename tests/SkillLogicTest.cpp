// Every flow of keeping blows on and from walkers from training the player's
// skills (docs/walkers-spec.md 4.3; Nadi, 2026-10-23 and 2026-10-27 "go A"):
// which skill use is held back, by the hit of the same frame.

#include "SkillLogic.h"

#include <cstdio>
#include <initializer_list>

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf("  %s  %s\n", condition ? "ok  " : "FAIL", what);
	if (!condition) {
		++g_failures;
	}
}

using walkers::SkillHit;

void TestWho() {
	std::printf("Which skill use is held back\n");
	SkillHit playerHitsWalker{true, true, false, false, true};
	SkillHit walkerHitsPlayer{true, false, true, true, false};
	SkillHit playerHitsBandit{true, false, false, false, true};
	SkillHit stale{false, true, false, false, true};
	Check(walkers::HoldsSkillUse(true, walkers::kSkillBlade, playerHitsWalker), "the player's blade on a walker");
	Check(walkers::HoldsSkillUse(true, walkers::kSkillHeavyArmor, walkerHitsPlayer) &&
	          walkers::HoldsSkillUse(true, walkers::kSkillBlock, walkerHitsPlayer),
	      "a walker's blow on the player: the armor and block it trains");
	Check(!walkers::HoldsSkillUse(true, walkers::kSkillBlade, playerHitsBandit), "a bandit: trained as ever");
	Check(!walkers::HoldsSkillUse(true, walkers::kSkillBlade, stale), "no hit this frame: trained as ever");
	Check(!walkers::HoldsSkillUse(false, walkers::kSkillBlade, playerHitsWalker), "the setting off: trained as ever");
	Check(!walkers::HoldsSkillUse(true, walkers::kSkillAthletics, playerHitsWalker) &&
	          !walkers::HoldsSkillUse(true, walkers::kSkillAcrobatics, walkerHitsPlayer),
	      "running and jumping in the same frame: trained as ever (not a skill of the blow)");
	bool combat = true;
	for (UInt32 skill : {walkers::kSkillBlade, walkers::kSkillBlock, walkers::kSkillBlunt, walkers::kSkillHandToHand,
	                     walkers::kSkillHeavyArmor, walkers::kSkillLightArmor, walkers::kSkillMarksman}) {
		combat = combat && walkers::IsBlowSkill(skill);
	}
	Check(combat && !walkers::IsBlowSkill(walkers::kSkillDestruction),
	      "the skills of a blow: Blade, Block, Blunt, Hand to Hand, Heavy and Light Armor, Marksman - not "
	      "Destruction (trained at the cast, before any hit)");
	SkillHit walkerHitsWalker{true, true, true, false, false};
	Check(!walkers::HoldsSkillUse(true, walkers::kSkillBlade, walkerHitsWalker), "a hit without the player: not his");
}

void TestSites() {
	std::printf("Where the skill use comes from\n");
	using walkers::SkillSite;
	Check(walkers::SiteOf(0x005FFCA4) == SkillSite::ArmorOfHit,
	      "returning to 0x005FFCA4: the armor of a hit in AttackHandling (the attacker in EDI)");
	Check(walkers::SiteOf(0x005FD846) == SkillSite::WeaponOfAttack,
	      "returning to 0x005FD846: the weapon skill in the attack update 0x005FCAB0");
	Check(walkers::SiteOf(0x005F5C0B) == SkillSite::BlockOfHit,
	      "returning to 0x005F5C0B: the block in 0x005F5B70 (the attacker in EBP)");
	Check(walkers::SiteOf(0x00401000) == SkillSite::Other, "anywhere else: another use");
	Check(walkers::HoldsSiteUse(true, SkillSite::BlockOfHit, true) && !walkers::HoldsSiteUse(true, SkillSite::BlockOfHit, false),
	      "a block of a walker's blow held back, of anyone else's trained");
	Check(walkers::HoldsSiteUse(true, SkillSite::ArmorOfHit, true) &&
	          !walkers::HoldsSiteUse(true, SkillSite::ArmorOfHit, false),
	      "the armor of a walker's hit held back, of anyone else's trained");
	Check(!walkers::HoldsSiteUse(false, SkillSite::ArmorOfHit, true), "the setting off: trained");
	Check(!walkers::HoldsSiteUse(true, SkillSite::Other, true) && !walkers::HoldsSiteUse(true, SkillSite::WeaponOfAttack, true),
	      "other sites: not decided by the register (the frame's hit decides there)");
}

}  // namespace

int main() {
	TestWho();
	TestSites();
	if (g_failures != 0) {
		std::printf("%d failed\n", g_failures);
		return 1;
	}
	std::printf("all passed\n");
	return 0;
}
