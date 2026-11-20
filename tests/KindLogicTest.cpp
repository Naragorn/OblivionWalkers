// Every flow of who comes (docs/walkers-spec.md 4.22 and 4.24; Nadi,
// 2026-11-20): the two switches and all their combinations - which group is
// in play, which of the walkers' rules hold for each, who rises, which group
// takes a replaced spawn, who is a siege's or an attack's side, the Daedra's
// own categories and the summoners held back.

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

using walkers::DaedraMode;
using walkers::Group;
using walkers::InvaderKind;
using walkers::InvaderSettings;
using walkers::Side;
using walkers::SpawnTarget;
using walkers::WalkersMode;

InvaderSettings With(WalkersMode w, DaedraMode d) {
	InvaderSettings s;
	s.walkers = w;
	s.daedra = d;
	return s;
}

const WalkersMode kWalkers[] = {WalkersMode::Off, WalkersMode::Zombies, WalkersMode::Apocalypse};
const DaedraMode kDaedra[] = {DaedraMode::Off, DaedraMode::Enemies, DaedraMode::Allies};

void TestSwitches() {
	std::printf("[Walkers] Mode and [Daedra] Mode\n");
	Check(walkers::WalkersModeFromIni(0) == WalkersMode::Off && walkers::WalkersModeFromIni(1) == WalkersMode::Zombies &&
	          walkers::WalkersModeFromIni(2) == WalkersMode::Apocalypse,
	      "Walkers: 0 off, 1 only the game's zombies, 2 the apocalypse");
	Check(walkers::WalkersModeFromIni(-1) == WalkersMode::Zombies && walkers::WalkersModeFromIni(3) == WalkersMode::Zombies,
	      "Walkers, anything else: the default, 1");
	Check(walkers::DaedraModeFromIni(0) == DaedraMode::Off && walkers::DaedraModeFromIni(1) == DaedraMode::Enemies &&
	          walkers::DaedraModeFromIni(2) == DaedraMode::Allies,
	      "Daedra: 0 off, 1 enemies of the walkers, 2 their allies");
	Check(walkers::DaedraModeFromIni(-1) == DaedraMode::Off && walkers::DaedraModeFromIni(3) == DaedraMode::Off,
	      "Daedra, anything else: the default, 0");
	InvaderSettings s;
	Check(s.walkers == WalkersMode::Zombies && s.daedra == DaedraMode::Off && s.daedraPercent == 50 && !s.summons &&
	          !s.afterMainQuest,
	      "defaults: Walkers 1 (4.24), Daedra 0, SharePercent 50, Summons 0 (idea 11), AfterMainQuest 0");
}

void TestDerivedKind() {
	std::printf("The derived kind, all nine combinations\n");
	const InvaderKind expect[3][3] = {
	    {InvaderKind::None, InvaderKind::Daedra, InvaderKind::Daedra},
	    {InvaderKind::Walkers, InvaderKind::Both, InvaderKind::Allied},
	    {InvaderKind::Walkers, InvaderKind::Both, InvaderKind::Allied},
	};
	const char* names[3][3] = {
	    {"Walkers 0, Daedra 0: None (the mod off)", "Walkers 0, Daedra 1: the Daedra alone",
	     "Walkers 0, Daedra 2: the Daedra alone"},
	    {"Walkers 1, Daedra 0: the walkers", "Walkers 1, Daedra 1: both at war", "Walkers 1, Daedra 2: allies"},
	    {"Walkers 2, Daedra 0: the walkers", "Walkers 2, Daedra 1: both at war", "Walkers 2, Daedra 2: allies"},
	};
	for (UInt32 w = 0; w < 3; ++w) {
		for (UInt32 d = 0; d < 3; ++d) {
			Check(walkers::KindOf(With(kWalkers[w], kDaedra[d])) == expect[w][d], names[w][d]);
		}
	}
	Check(walkers::ModOff(With(WalkersMode::Off, DaedraMode::Off)) &&
	          !walkers::ModOff(With(WalkersMode::Zombies, DaedraMode::Off)) &&
	          !walkers::ModOff(With(WalkersMode::Off, DaedraMode::Allies)),
	      "the mod is off only with both switches at 0");
	walkers::InvaderSettings necro = With(WalkersMode::Off, DaedraMode::Off);
	necro.necromancers = true;
	Check(!walkers::ModOff(necro), "the necromancers alone (4.23 B): the mod is on - all three switches decide");
	Check(!walkers::SidesExist(With(WalkersMode::Off, DaedraMode::Off)) &&
	          !walkers::SidesExist(With(WalkersMode::Zombies, DaedraMode::Off)) &&
	          walkers::SidesExist(With(WalkersMode::Apocalypse, DaedraMode::Off)) &&
	          walkers::SidesExist(With(WalkersMode::Zombies, DaedraMode::Enemies)) &&
	          walkers::SidesExist(With(WalkersMode::Off, DaedraMode::Allies)),
	      "a side exists with the apocalypse or a Daedra mode - the zombies are no side");
}

void TestNearNecromancers() {
	std::printf("The rise near the necromancers (4.23 B step 6)\n");
	walkers::InvaderSettings necro = With(WalkersMode::Off, DaedraMode::Off);
	necro.necromancers = true;
	Check(walkers::MayRise(necro, false, true) && !walkers::MayRise(necro, false, false),
	      "the necromancers alone: near them the dead rise, far they do not");
	Check(!walkers::MayRise(necro, true, true), "a Daedra never");
	Check(!walkers::MayRise(With(WalkersMode::Off, DaedraMode::Off), false, true), "without the necromancers: nobody");
	Check(walkers::MayRise(With(WalkersMode::Apocalypse, DaedraMode::Off), false, false), "the apocalypse: everywhere");
	Check(walkers::NearNecromancers(true, -1.0f, 8000.0f, false), "in one of their places: near");
	Check(walkers::NearNecromancers(false, 8000.0f, 8000.0f, false) && !walkers::NearNecromancers(false, 8001.0f, 8000.0f, false),
	      "within RiseWithin of a door: near; past it: not");
	Check(walkers::NearNecromancers(false, -1.0f, 8000.0f, true), "killed by one of their walkers: near, wherever");
	Check(!walkers::NearNecromancers(false, -1.0f, 8000.0f, false), "no place known, not theirs: not");
}

void TestInPlay() {
	std::printf("Which group is in play\n");
	Check(walkers::InPlay(InvaderKind::Walkers, Group::Walker) && !walkers::InPlay(InvaderKind::Walkers, Group::Daedra),
	      "walkers only: the walkers");
	Check(!walkers::InPlay(InvaderKind::Daedra, Group::Walker) && walkers::InPlay(InvaderKind::Daedra, Group::Daedra),
	      "Daedra alone: only the Daedra");
	Check(walkers::InPlay(InvaderKind::Both, Group::Walker) && walkers::InPlay(InvaderKind::Both, Group::Daedra),
	      "both at war: both");
	Check(walkers::InPlay(InvaderKind::Allied, Group::Walker) && walkers::InPlay(InvaderKind::Allied, Group::Daedra),
	      "allies: both");
	Check(!walkers::InPlay(InvaderKind::None, Group::Walker) && !walkers::InPlay(InvaderKind::None, Group::Daedra),
	      "none: nobody");
	Check(!walkers::BothInPlay(InvaderKind::Walkers) && !walkers::BothInPlay(InvaderKind::Daedra) &&
	          !walkers::BothInPlay(InvaderKind::None) && walkers::BothInPlay(InvaderKind::Both) &&
	          walkers::BothInPlay(InvaderKind::Allied),
	      "both in play: at war or allied only");
}

void TestHostility() {
	std::printf("Walkers and Daedra: foes or friends\n");
	Check(walkers::GroupsHostile(InvaderKind::Both), "Daedra 1 with walkers: at war - a three-way fight where a band meets a horde");
	Check(!walkers::GroupsHostile(InvaderKind::Allied), "Daedra 2 with walkers: allies, one side against the living");
	Check(!walkers::GroupsHostile(InvaderKind::Walkers) && !walkers::GroupsHostile(InvaderKind::Daedra) &&
	          !walkers::GroupsHostile(InvaderKind::None),
	      "one group or none: nobody to fight");
	Check(walkers::WalkersJoinDaedra(InvaderKind::Allied) && !walkers::WalkersJoinDaedra(InvaderKind::Both) &&
	          !walkers::WalkersJoinDaedra(InvaderKind::Walkers) && !walkers::WalkersJoinDaedra(InvaderKind::Daedra) &&
	          !walkers::WalkersJoinDaedra(InvaderKind::None),
	      "only the allies: the walkers join the Daedra's faction");
	Check(walkers::kDaedraFaction == 0x00053FED, "DaedraFaction, Oblivion.esm 00053FED");
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
	Check(!walkers::MayRise(With(WalkersMode::Off, DaedraMode::Off), false) &&
	          !walkers::MayRise(With(WalkersMode::Off, DaedraMode::Enemies), false),
	      "Walkers 0: nobody rises");
	Check(!walkers::MayRise(With(WalkersMode::Zombies, DaedraMode::Off), false) &&
	          !walkers::MayRise(With(WalkersMode::Zombies, DaedraMode::Allies), false),
	      "Walkers 1: the game's zombies only - the dead do not rise");
	Check(walkers::MayRise(With(WalkersMode::Apocalypse, DaedraMode::Off), false) &&
	          walkers::MayRise(With(WalkersMode::Apocalypse, DaedraMode::Enemies), false) &&
	          walkers::MayRise(With(WalkersMode::Apocalypse, DaedraMode::Allies), false),
	      "Walkers 2: the fallen rise, those the Daedra kill too (idea 2, the two plagues feed each other)");
	for (UInt32 w = 0; w < 3; ++w) {
		for (UInt32 d = 0; d < 3; ++d) {
			Check(!walkers::MayRise(With(kWalkers[w], kDaedra[d]), true), "a fallen Daedra never rises, whatever the switches");
		}
	}
}

void TestReplacement() {
	std::printf("Which group takes a replaced spawn\n");
	for (UInt32 d = 0; d < 3; ++d) {
		const InvaderSettings zombies = With(WalkersMode::Zombies, kDaedra[d]);
		const InvaderSettings apoc = With(WalkersMode::Apocalypse, kDaedra[d]);
		Check(walkers::ReplacementTarget(zombies, false, 0) == SpawnTarget::Walker &&
		          walkers::ReplacementTarget(apoc, false, 99) == SpawnTarget::Walker,
		      "walkers on, the Daedra not everywhere: a walker, whatever the roll and the Daedra switch");
		Check(walkers::ReplacementTarget(With(WalkersMode::Off, kDaedra[d]), false, 0) == SpawnTarget::None,
		      "walkers off, the Daedra not everywhere: nothing is replaced");
	}
	InvaderSettings s = With(WalkersMode::Apocalypse, DaedraMode::Off);
	Check(walkers::ReplacementTarget(s, true, 0) == SpawnTarget::Walker,
	      "everywhere but the Daedra off: no Daedra, ever");
	s = With(WalkersMode::Apocalypse, DaedraMode::Enemies);
	Check(walkers::ReplacementTarget(s, true, 49) == SpawnTarget::Daedra &&
	          walkers::ReplacementTarget(s, true, 50) == SpawnTarget::Walker,
	      "everywhere, SharePercent 50: a roll 0..49 a Daedra, 50..99 a walker");
	s.daedra = DaedraMode::Allies;
	Check(walkers::ReplacementTarget(s, true, 49) == SpawnTarget::Daedra && walkers::ReplacementTarget(s, true, 50) == SpawnTarget::Walker,
	      "allies: the same share");
	s.walkers = WalkersMode::Zombies;
	Check(walkers::ReplacementTarget(s, true, 10) == SpawnTarget::Daedra && walkers::ReplacementTarget(s, true, 90) == SpawnTarget::Walker,
	      "Walkers 1 and everywhere: the share too");
	s.walkers = WalkersMode::Off;
	Check(walkers::ReplacementTarget(s, true, 0) == SpawnTarget::Daedra && walkers::ReplacementTarget(s, true, 99) == SpawnTarget::Daedra,
	      "no walkers, the Daedra everywhere: every replaced entry a Daedra");
	s = With(WalkersMode::Apocalypse, DaedraMode::Enemies);
	s.daedraPercent = 0;
	Check(walkers::ReplacementTarget(s, true, 0) == SpawnTarget::Walker, "SharePercent 0: never a Daedra");
	s.daedraPercent = 100;
	Check(walkers::ReplacementTarget(s, true, 99) == SpawnTarget::Daedra, "SharePercent 100: always a Daedra");
	s.daedraPercent = 250;
	Check(walkers::ReplacementTarget(s, true, 99) == SpawnTarget::Daedra, "above 100: as 100");
}

void TestSides() {
	std::printf("The side of a siege, an attack, a held dungeon's attackers (4.24)\n");
	Check(walkers::SideOf(With(WalkersMode::Off, DaedraMode::Off), 0) == Side::Nobody &&
	          walkers::SideOf(With(WalkersMode::Zombies, DaedraMode::Off), 99) == Side::Nobody,
	      "no apocalypse and no Daedra (the mod off, or only the zombies): nobody");
	Check(walkers::SideOf(With(WalkersMode::Apocalypse, DaedraMode::Off), 0) == Side::Walkers &&
	          walkers::SideOf(With(WalkersMode::Apocalypse, DaedraMode::Off), 99) == Side::Walkers,
	      "the apocalypse alone: the walkers, whatever the roll");
	for (UInt32 w = 0; w < 2; ++w) {
		Check(walkers::SideOf(With(kWalkers[w], DaedraMode::Enemies), 0) == Side::Daedra &&
		          walkers::SideOf(With(kWalkers[w], DaedraMode::Allies), 99) == Side::Daedra,
		      "the Daedra with no apocalypse (Walkers 0 or 1): the Daedra, the zombies are no side");
	}
	InvaderSettings s = With(WalkersMode::Apocalypse, DaedraMode::Enemies);
	Check(walkers::SideOf(s, 49) == Side::Daedra && walkers::SideOf(s, 50) == Side::Walkers,
	      "the apocalypse and the Daedra: by SharePercent 50");
	s.daedra = DaedraMode::Allies;
	Check(walkers::SideOf(s, 49) == Side::Daedra && walkers::SideOf(s, 50) == Side::Walkers, "allies: the same");
	s.daedraPercent = 0;
	Check(walkers::SideOf(s, 0) == Side::Walkers, "SharePercent 0: the walkers");
	s.daedraPercent = 100;
	Check(walkers::SideOf(s, 99) == Side::Daedra, "SharePercent 100: the Daedra");
}

void TestReplacementRuns() {
	std::printf("Whether the spawn replacement runs\n");
	Check(!walkers::ReplacementRuns(With(WalkersMode::Off, DaedraMode::Off), false) &&
	          !walkers::ReplacementRuns(With(WalkersMode::Off, DaedraMode::Off), true),
	      "the mod off: never");
	Check(walkers::ReplacementRuns(With(WalkersMode::Zombies, DaedraMode::Off), false) &&
	          walkers::ReplacementRuns(With(WalkersMode::Apocalypse, DaedraMode::Allies), false),
	      "with walkers: yes");
	Check(!walkers::ReplacementRuns(With(WalkersMode::Off, DaedraMode::Enemies), false),
	      "the Daedra alone out of the gates: the world keeps its spawns");
	Check(walkers::ReplacementRuns(With(WalkersMode::Off, DaedraMode::Enemies), true),
	      "the Daedra alone while they come everywhere: yes");
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
	      "walkers only: a category switched on is replaced, the Daedra's too - as now");
	Check(walkers::CategoryReplaced(InvaderKind::None, dremora, true),
	      "the mod off: no kind keeps a category (the replacement does not run then)");
	Check(!walkers::CategoryReplaced(InvaderKind::Daedra, dremora, true) &&
	          walkers::CategoryReplaced(InvaderKind::Daedra, bandits, true),
	      "the Daedra alone: the Daedra stay the game's Daedra, the bandits are replaced");
	Check(!walkers::CategoryReplaced(InvaderKind::Both, dremora, true) &&
	          !walkers::CategoryReplaced(InvaderKind::Allied, dremora, true),
	      "at war or allied: the Daedra stay too");
	Check(!walkers::CategoryReplaced(InvaderKind::Daedra, bandits, false) &&
	          !walkers::CategoryReplaced(InvaderKind::Walkers, bandits, false),
	      "a category switched off: never");

	std::printf("Walkers Mode 1: the Zombies category alone\n");
	Check(std::strcmp(walkers::kReplaceCategories[walkers::kZombiesCategory].key, "Zombies") == 0,
	      "kZombiesCategory is the Zombies key");
	const InvaderSettings zombies = With(WalkersMode::Zombies, DaedraMode::Off);
	Check(walkers::CategoryAllowed(zombies, walkers::kZombiesCategory) && !walkers::CategoryAllowed(zombies, bandits) &&
	          !walkers::CategoryAllowed(zombies, dremora) && !walkers::CategoryAllowed(zombies, walkers::kNoCategory),
	      "Walkers 1: Zombies yes, every other category (and none) off, whatever [ReplaceCategories] says");
	Check(walkers::CategoryAllowed(With(WalkersMode::Apocalypse, DaedraMode::Off), bandits) &&
	          walkers::CategoryAllowed(With(WalkersMode::Off, DaedraMode::Enemies), bandits),
	      "Walkers 2 (and 0, for the Daedra alone everywhere): every category as set");
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
	Check(walkers::InvaderDaedraLocalId(s) == walkers::kInvaderDaedraNoSummonLocalId,
	      "held back: a replaced entry names the ESP's NOSUMMON copy");
	s.summons = true;
	Check(!walkers::HoldSummonersBack(s), "Summons 1: the game's own, with their summons");
	Check(walkers::InvaderDaedraLocalId(s) == walkers::kInvaderDaedraLocalId,
	      "Summons 1: the ESP's list that names LL1Daedra100");
	Check(walkers::InvaderForm(s, 0x00092DA6) == 0x00092DA6, "Summons 1: the list itself");
}

void TestEntryRoll() {
	std::printf("The stable roll of a list entry\n");
	bool inRange = true;
	UInt32 below50 = 0;
	for (UInt32 a = 0x10000000; a < 0x10000000 + 4000 * 12; a += 12) {
		const UInt32 r = walkers::EntryRoll(a);
		inRange = inRange && r < 100;
		below50 += r < 50 ? 1u : 0u;
	}
	Check(inRange, "always 0..99");
	Check(walkers::EntryRoll(0x1234ABC0) == walkers::EntryRoll(0x1234ABC0), "the same entry: the same roll");
	Check(below50 > 1800 && below50 < 2200,
	      "4000 entries 12 bytes apart (ListData's size): about half below 50");
}

void TestLeader() {
	std::printf("A war band's leader, two ranks above its men (idea 1)\n");
	Check(walkers::DremoraRankAt(1) == 0 && walkers::DremoraRankAt(7) == 0 && walkers::DremoraRankAt(8) == 1 &&
	          walkers::DremoraRankAt(23) == 5 && walkers::DremoraRankAt(24) == 6 && walkers::DremoraRankAt(60) == 6,
	      "the men's rank by the player's level: Churl from 1, Caitiff from 8 .. Valkynaz from 24");
	Check(walkers::LeaderRank(1) == 2 && walkers::LeaderRank(14) == 5 && walkers::LeaderRank(21) == 6 &&
	          walkers::LeaderRank(30) == 6,
	      "the leader two above, Valkynaz at most");
	InvaderSettings s;
	Check(walkers::LeaderList(s, 1) == walkers::NoSummonTwin(0x0003E9D2) && walkers::LeaderList(s, 1) != 0x0003E9D2,
	      "summoners held back: the Kynval list's NOSUMMON twin");
	s.summons = true;
	Check(walkers::LeaderList(s, 1) == 0x0003E9D2, "Summons 1: LL0Dremora2Kynval100 itself");

	std::printf("The leader and the outpost's boss placed as actors (MOO rewrites the rank lists in the game)\n");
	InvaderSettings held;
	Check(walkers::LeaderActor(held, 1, 3) == 0x00000FD1 && walkers::LeaderActor(held, 1, 0) == 0x00000FD3,
	      "held back: one of LL0Dremora2Kynval100NOSUMMON's own entries (the mage's and the melee's twins)");
	Check(walkers::LeaderActor(s, 1, 0) == 0x0003E9C6 && walkers::LeaderActor(s, 1, 7) == 0x00092D9B,
	      "Summons 1: one of LL0Dremora2Kynval100's entries, the roll wrapped");
	Check(walkers::OutpostBossActor(held, 1, 3) == 0x00000FD5, "the boss three ranks up: a Kynreeve's twin");
	Check(walkers::OutpostBossActor(held, 40, 1) == 0x00000FC8, "at level 40 the seventh rank, Valkynaz, at most");
	bool all = true;
	for (const walkers::RankListActors& r : walkers::kRankListActors) {
		for (UInt32 i = 0; i < r.count; ++i) {
			all = all && walkers::RankListActor(r.list, i) != r.list;
		}
	}
	Check(all && sizeof(walkers::kRankListActors) / sizeof(walkers::kRankListActors[0]) == 14,
	      "the seven rank lists and their seven twins, every roll an actor, never the list");
	Check(walkers::RankListActor(0x12345678, 2) == 0x12345678, "not a rank list: itself");
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

void TestAfterMainQuest() {
	std::printf("After the main quest (4.22a step 6)\n");
	Check(walkers::DaedraStopped(false, 0x03) && !walkers::DaedraStopped(false, 0x01),
	      "AfterMainQuest=0: stopped once MQ16 reads completed, not while it runs");
	Check(!walkers::DaedraStopped(true, 0x03), "AfterMainQuest=1: never stopped");
	const InvaderSettings s = With(WalkersMode::Apocalypse, DaedraMode::Allies);
	Check(walkers::InvadersNow(s, true).daedra == DaedraMode::Off && walkers::InvadersNow(s, true).walkers == WalkersMode::Apocalypse &&
	          walkers::InvadersNow(s, false).daedra == DaedraMode::Allies,
	      "stopped: only the Daedra go off, the walkers are untouched");
	Check(walkers::KindOf(walkers::InvadersNow(s, true)) == InvaderKind::Walkers,
	      "stopped: the derived kind is the walkers'");
	const InvaderSettings alone = With(WalkersMode::Off, DaedraMode::Enemies);
	Check(walkers::KindOf(walkers::InvadersNow(alone, true)) == InvaderKind::None &&
	          !walkers::ReplacementRuns(walkers::InvadersNow(alone, true), true),
	      "the Daedra alone, stopped: nobody - no replacement even while 'everywhere'");
	Check(walkers::ReplacementRuns(walkers::InvadersNow(s, true), false),
	      "walkers and Daedra, stopped: the replacement keeps its walkers");
}

int main() {
	TestAfterMainQuest();
	TestSwitches();
	TestDerivedKind();
	TestInPlay();
	TestNearNecromancers();
	TestHostility();
	TestRules();
	TestRise();
	TestReplacement();
	TestSides();
	TestReplacementRuns();
	TestCategories();
	TestSummoners();
	TestEntryRoll();
	TestLeader();
	TestTwins();
	if (g_failures != 0) {
		std::printf("%d failed\n", g_failures);
		return 1;
	}
	std::printf("all passed\n");
	return 0;
}
