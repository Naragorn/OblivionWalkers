// Every flow of the invaders' kind (docs/walkers-spec.md 4.22, step 1; Nadi,
// 2026-11-15: "a b, danach c"): one per row of 4.22's table, for Kind 0, 1
// and 2 - which group is in play, which of the walkers' rules hold for each,
// who rises, which group takes a replaced spawn, the Daedra's own categories
// and the summoners held back.

#include "CategoryLogic.h"
#include "KindLogic.h"

#include <cstdio>
#include <cstring>

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf("  %s  %s\n", condition ? "ok  " : "FAIL", what);
	if (!condition) {
		++g_failures;
	}
}

using walkers::Group;
using walkers::InvaderKind;
using walkers::InvaderSettings;

void TestKindFromIni() {
	std::printf("[Invaders] Kind\n");
	Check(walkers::KindFromIni(0) == InvaderKind::Walkers && walkers::KindFromIni(1) == InvaderKind::Daedra &&
	          walkers::KindFromIni(2) == InvaderKind::Both,
	      "0 the walkers, 1 the Daedra, 2 both");
	Check(walkers::KindFromIni(-1) == InvaderKind::Walkers && walkers::KindFromIni(3) == InvaderKind::Walkers,
	      "anything else: the walkers, as without the switch");
	InvaderSettings s;
	Check(s.kind == InvaderKind::Walkers && s.daedraPercent == 50 && !s.summons,
	      "defaults: Kind 0 (decided 2026-11-13), DaedraPercent 50, Summons 0 (idea 11)");
}

void TestInPlay() {
	std::printf("Which group is in play\n");
	Check(walkers::InPlay(InvaderKind::Walkers, Group::Walker) && !walkers::InPlay(InvaderKind::Walkers, Group::Daedra),
	      "Kind 0: only the walkers");
	Check(!walkers::InPlay(InvaderKind::Daedra, Group::Walker) && walkers::InPlay(InvaderKind::Daedra, Group::Daedra),
	      "Kind 1: only the Daedra");
	Check(walkers::InPlay(InvaderKind::Both, Group::Walker) && walkers::InPlay(InvaderKind::Both, Group::Daedra),
	      "Kind 2: both");
}

void TestRules() {
	std::printf("The walkers' rules, by group (4.22's table)\n");
	const walkers::GroupRules w = walkers::RulesOf(Group::Walker);
	const walkers::GroupRules d = walkers::RulesOf(Group::Daedra);
	Check(w.rise && !d.rise, "the fallen rise (4.1): walkers yes, Daedra no");
	Check(w.headRule && !d.headRule, "only a lost head ends one (4.17): walkers yes, Daedra die as the game's");
	Check(w.grab && !d.grab, "the grab and the bites (4.19): walkers yes, Daedra fight with the game's arms");
	Check(w.slowAggression && !d.slowAggression, "slow aggression (4.21): walkers yes, Daedra no");
	Check(w.disguise && !d.disguise, "walker blood as a disguise (4.20.1): walkers yes, Daedra no");
	Check(w.walkOnly && !d.walkOnly, "walk only at their own pace (4.4): walkers yes, Daedra run and charge");
	Check(w.marches && d.marches,
	      "hordes to the cities, gate guards, sieges, news, camps, dungeons: both (war bands for the Daedra)");
	Check(w.nightWorse && !d.nightWorse, "nights are worse (4.20.5): walkers; for the Daedra the weather (idea 9, off)");
	Check(w.scaledHealth && !d.scaledHealth, "strength: walkers HealthPercent, Daedra the game's own leveling");
}

void TestRise() {
	std::printf("Who rises\n");
	Check(walkers::MayRise(InvaderKind::Walkers, false), "Kind 0: the fallen rise, as now");
	Check(!walkers::MayRise(InvaderKind::Daedra, false), "Kind 1: no walkers, nobody rises");
	Check(walkers::MayRise(InvaderKind::Both, false),
	      "Kind 2: the fallen rise - those the Daedra kill too (idea 2, the two plagues feed each other)");
	Check(!walkers::MayRise(InvaderKind::Walkers, true) && !walkers::MayRise(InvaderKind::Both, true),
	      "a fallen Daedra never rises, whatever the kind");
}

void TestReplacement() {
	std::printf("Which group takes a replaced spawn\n");
	InvaderSettings s;
	Check(walkers::ReplacementGroup(s, 0) == Group::Walker && walkers::ReplacementGroup(s, 99) == Group::Walker,
	      "Kind 0: a walker, whatever the roll");
	s.kind = InvaderKind::Daedra;
	Check(walkers::ReplacementGroup(s, 0) == Group::Daedra && walkers::ReplacementGroup(s, 99) == Group::Daedra,
	      "Kind 1: a Daedra, whatever the roll");
	s.kind = InvaderKind::Both;
	Check(walkers::ReplacementGroup(s, 49) == Group::Daedra && walkers::ReplacementGroup(s, 50) == Group::Walker,
	      "Kind 2, DaedraPercent 50: a roll 0..49 a Daedra, 50..99 a walker");
	s.daedraPercent = 0;
	Check(walkers::ReplacementGroup(s, 0) == Group::Walker, "DaedraPercent 0: never a Daedra");
	s.daedraPercent = 100;
	Check(walkers::ReplacementGroup(s, 99) == Group::Daedra, "DaedraPercent 100: always a Daedra");
	s.daedraPercent = 250;
	Check(walkers::ReplacementGroup(s, 99) == Group::Daedra, "above 100: as 100");
}

void TestCategories() {
	std::printf("The Daedra's own categories\n");
	UInt32 daedra = 0;
	bool named = true;
	for (UInt32 k = 0; k < walkers::kReplaceCategoryCount; ++k) {
		if (walkers::IsDaedraCategory(static_cast<UInt8>(k))) {
			++daedra;
			const char* key = walkers::kReplaceCategories[k].key;
			named = named && (std::strcmp(key, "Atronachs") == 0 || std::strcmp(key, "Clannfears") == 0 ||
			                  std::strcmp(key, "Daedroths") == 0 || std::strcmp(key, "Scamps") == 0 ||
			                  std::strcmp(key, "SpiderDaedra") == 0 || std::strcmp(key, "Xivilai") == 0 ||
			                  std::strcmp(key, "Dremora") == 0);
		}
	}
	Check(daedra == 7 && named,
	      "seven: Atronachs, Clannfears, Daedroths, Scamps, SpiderDaedra, Xivilai, Dremora (4.22, by their keys)");
	Check(!walkers::IsDaedraCategory(walkers::kNoCategory) && !walkers::IsDaedraCategory(200),
	      "no category, or one past the table: no");
	const UInt8 dremora = walkers::DaedraCategoryIndex(6);
	const UInt8 bandits = 37;
	Check(std::strcmp(walkers::kReplaceCategories[dremora].key, "Dremora") == 0 &&
	          std::strcmp(walkers::kReplaceCategories[bandits].key, "Bandits") == 0,
	      "(the indices the next checks use: Dremora, Bandits)");
	Check(walkers::CategoryReplaced(InvaderKind::Walkers, dremora, true) &&
	          walkers::CategoryReplaced(InvaderKind::Walkers, bandits, true),
	      "Kind 0: a category switched on is replaced, the Daedra's too - as now");
	Check(!walkers::CategoryReplaced(InvaderKind::Daedra, dremora, true) &&
	          walkers::CategoryReplaced(InvaderKind::Daedra, bandits, true),
	      "Kind 1: the Daedra stay the game's Daedra, the bandits are replaced");
	Check(!walkers::CategoryReplaced(InvaderKind::Both, dremora, true), "Kind 2: the Daedra stay too");
	Check(!walkers::CategoryReplaced(InvaderKind::Daedra, bandits, false) &&
	          !walkers::CategoryReplaced(InvaderKind::Walkers, bandits, false),
	      "a category switched off: never");
}

void TestSummoners() {
	std::printf("The summoners held back (idea 11)\n");
	InvaderSettings s;
	Check(walkers::HoldSummonersBack(s), "Summons 0 (the default): the NOSUMMON twins");
	Check(walkers::InvaderForm(s, 0x00092DA6) == 0x00000FE7 && walkers::InvaderForm(s, 0x0003265D) == 0x00000FED,
	      "held back: the Churls' list LL0Dremora0Churl100 its NOSUMMON twin, the xivilai's too");
	Check(walkers::InvaderForm(s, 0x00092DA5) == 0x00000FCB && walkers::InvaderForm(s, 0x0001E64C) == 0x00000FE0,
	      "held back: the Churl mage and the spider daedra their twins");
	Check(walkers::InvaderForm(s, 0x00092DA0) == 0x00092DA0 && walkers::InvaderForm(s, 0x0002D802) == 0x0002D802,
	      "a Churl of the sword (summons nothing) and a scamp: themselves");
	s.summons = true;
	Check(!walkers::HoldSummonersBack(s), "Summons 1: the game's own, with their summons");
	Check(walkers::InvaderForm(s, 0x00092DA6) == 0x00092DA6, "Summons 1: the list itself");
}

void TestTwins() {
	std::printf("The twins read off Oblivion.esm (DaedraLists.h)\n");
	UInt32 lists = 0;
	UInt32 mages = 0;
	bool distinct = true;
	bool loadIndexZero = true;
	for (UInt32 i = 0; i < walkers::kSummonTwinCount; ++i) {
		const walkers::SummonTwin& t = walkers::kSummonTwins[i];
		lists += t.list ? 1u : 0u;
		mages += std::strstr(t.name, "Mage") != nullptr && std::strncmp(t.name, "Dremora", 7) == 0 ? 1u : 0u;
		distinct = distinct && t.original != t.twin;
		loadIndexZero = loadIndexZero && (t.original >> 24) == 0 && (t.twin >> 24) == 0;
		for (UInt32 j = i + 1; j < walkers::kSummonTwinCount; ++j) {
			distinct = distinct && walkers::kSummonTwins[j].original != t.original;
		}
	}
	Check(lists == 8, "eight lists: the seven ranks' and the xivilai's");
	Check(mages == 7, "a mage of each of the seven ranks - the Dremora who summon");
	Check(walkers::kSummonTwinCount == 18, "eighteen in all: eight lists, seven mages, the spider daedra, two xivilai");
	Check(distinct && loadIndexZero, "each original once, never its own twin, all Oblivion.esm's (load index 0)");
	Check(walkers::NoSummonTwin(0x12345678) == 0x12345678, "a form not in the table: itself");
}

}  // namespace

int main() {
	TestKindFromIni();
	TestInPlay();
	TestRules();
	TestRise();
	TestReplacement();
	TestCategories();
	TestSummoners();
	TestTwins();
	if (g_failures != 0) {
		std::printf("%d failed\n", g_failures);
		return 1;
	}
	std::printf("all passed\n");
	return 0;
}
