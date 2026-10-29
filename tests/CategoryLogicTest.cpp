// Every flow of the replacement's categories (docs/walkers-spec.md 4.6a;
// Nadi, 2026-10-29: "1 für Skeletons und alle skeletons egal wie die im CS
// heissen werden ersetzt"): a creature's category by its model's folder, a
// person's by the factions, and what a spawn's entry becomes from the
// category, the INI's lines and the master switch.

#include "ReplaceLogic.h"

#include <cctype>
#include <cstdio>
#include <cstring>
#include <string>

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf("  %s  %s\n", condition ? "ok  " : "FAIL", what);
	if (!condition) {
		++g_failures;
	}
}

using walkers::CreatureFacts;
using walkers::kNoCategory;
using walkers::kReplaceCategories;
using walkers::kReplaceCategoryCount;

UInt8 IndexOf(const char* key) {
	for (UInt32 k = 0; k < kReplaceCategoryCount; ++k) {
		if (std::strcmp(kReplaceCategories[k].key, key) == 0) {
			return static_cast<UInt8>(k);
		}
	}
	return kNoCategory;
}

void TestFolder() {
	std::printf("The model's folder\n");
	char folder[64];
	Check(walkers::ModelFolder("Creatures\\Skeleton\\Skeleton.NIF", folder, sizeof(folder)) &&
	          std::strcmp(folder, "skeleton") == 0,
	      "Creatures\\Skeleton\\Skeleton.NIF: skeleton, the last folder in small letters");
	Check(walkers::ModelFolder("MOO/Zombie/skeleton.nif", folder, sizeof(folder)) && std::strcmp(folder, "zombie") == 0,
	      "MOO/Zombie/skeleton.nif: zombie - forward slashes count too");
	Check(walkers::ModelFolder("Creatures\\Bear\\", folder, sizeof(folder)) && std::strcmp(folder, "bear") == 0,
	      "a path that ends in its folder: that folder");
	Check(!walkers::ModelFolder("skeleton.nif", folder, sizeof(folder)), "a model without a folder: none");
	Check(!walkers::ModelFolder("", folder, sizeof(folder)) && !walkers::ModelFolder(nullptr, folder, sizeof(folder)),
	      "no model: none");
	Check(!walkers::ModelFolder("Creatures\\AFolderNameLongerThanTheBuffer\\x.nif", folder, 8),
	      "a folder longer than the buffer: none, never cut");
}

void TestFolderMatch() {
	std::printf("A folder against a category's folders\n");
	Check(walkers::FolderMatches("ghost|wraith", "wraith"), "wraith among ghost|wraith");
	Check(walkers::FolderMatches("ghost|wraith", "ghost"), "ghost, the first of them");
	Check(!walkers::FolderMatches("rat", "pirate") && !walkers::FolderMatches("rat", "rats"),
	      "a name only whole: rat is no pirate, no rats");
	Check(walkers::FolderMatches("willothewisp|wisp*", "wispvattelys"), "wisp* takes MOO's wispvattelys");
	Check(!walkers::FolderMatches("wisp*", "wis") && !walkers::FolderMatches("wisp*", "willothewisp"),
	      "a prefix must be there whole, at the start");
	Check(!walkers::FolderMatches(nullptr, "skeleton") && !walkers::FolderMatches("skeleton", ""),
	      "no folders or an empty folder: no match");
}

void TestCreatureCategory() {
	std::printf("A creature's category\n");
	const UInt8 skeletons = IndexOf("Skeletons");
	const UInt8 zombies = IndexOf("Zombies");
	const UInt8 wolves = IndexOf("Wolves");
	const UInt8 hounds = IndexOf("SkinnedHounds");
	const UInt8 wisps = IndexOf("Wisps");
	const UInt8 xivilai = IndexOf("Xivilai");
	Check(walkers::CreatureCategory("Creatures\\Skeleton\\Skeleton.NIF", walkers::kCreatureTypeUndead) == skeletons,
	      "Skeleton, Skeleton Champion, Bregor the Cremator (one model): Skeletons");
	Check(walkers::CreatureCategory("MOO\\Skeleton\\Skeleton.NIF", walkers::kCreatureTypeUndead) == skeletons,
	      "MOO's skeletons on their own model: Skeletons too");
	Check(walkers::CreatureCategory("MOO\\Zombie\\Skeleton.NIF", walkers::kCreatureTypeUndead) == zombies,
	      "MOO's zombies: Zombies");
	Check(walkers::CreatureCategory("Creatures\\Dog\\Skeleton.NIF", walkers::kCreatureTypeCreature) == wolves,
	      "a wolf or a dog: Wolves");
	Check(walkers::CreatureCategory("Creatures\\Dog\\Skeleton.NIF", walkers::kCreatureTypeUndead) == hounds,
	      "a skinned hound (the dog's model, undead): SkinnedHounds, not Wolves");
	Check(walkers::CreatureCategory("MOO\\WispVattelys\\Skeleton.NIF", walkers::kCreatureTypeCreature) == wisps,
	      "MOO's wisps: Wisps");
	Check(walkers::CreatureCategory("MOO\\XivilaiLord\\Skeleton.NIF", walkers::kCreatureTypeDaedra) == xivilai,
	      "MOO's xivilai lords: Xivilai");
	Check(walkers::CreatureCategory("MOO\\Harpy\\Skeleton.NIF", walkers::kCreatureTypeCreature) == kNoCategory,
	      "a folder no category names (MOO's harpies): none - its own INI line decides");
	Check(walkers::CreatureCategory(nullptr, walkers::kCreatureTypeUndead) == kNoCategory, "no model: none");

	// Every folder Oblivion.esm's leveled lists spawn from (the records read
	// with tools/esm-spawns.py, 2026-10-29): each has its category.
	const char* vanilla[] = {"baliwog", "bear", "boar", "clannfear", "daedroth", "deer", "dog", "elytra",
	                         "flameatronach", "fleshatronach", "frostatronach", "ghost", "gnarl", "goblin",
	                         "grummite", "horse", "hunger", "imp", "landdreugh", "lich", "minotaur",
	                         "mountainlion", "mudcrab", "murkdweller", "ogre", "rat", "scamp", "shambles", "sheep",
	                         "skeleton", "slaughterfish", "spiderdaedra", "spriggan", "stormatronach", "troll",
	                         "willothewisp", "wraith", "xivilai", "zombie"};
	bool all = true;
	for (const char* folder : vanilla) {
		const std::string path = std::string("Creatures\\") + folder + "\\Skeleton.NIF";
		if (walkers::CreatureCategory(path.c_str(), walkers::kCreatureTypeCreature) == kNoCategory) {
			std::printf("    no category: %s\n", folder);
			all = false;
		}
	}
	Check(all, "every one of the 39 folders of Oblivion.esm's spawns has a category");
}

void TestPersonCategory() {
	std::printf("A person's category\n");
	const UInt32 bandit[] = {0x000177E3};
	Check(walkers::PersonCategory(bandit, 1) == IndexOf("Bandits"), "BanditFaction: Bandits");
	const UInt32 blackBow[] = {0x0008595C};
	Check(walkers::PersonCategory(blackBow, 1) == IndexOf("Bandits"), "BlackBowFaction: Bandits");
	const UInt32 vampireNecromancer[] = {0x0000C0F2, 0x00033F53};
	Check(walkers::PersonCategory(vampireNecromancer, 2) == IndexOf("Vampires"),
	      "a vampire necromancer (MOO has 703 vampires): Vampires, the first in the table");
	const UInt32 necromancerConjurer[] = {0x0004B90B, 0x0000C0F2, 0x0004B90C};
	Check(walkers::PersonCategory(necromancerConjurer, 3) == IndexOf("Necromancers"),
	      "MOO's mages in both cults: Necromancers, before Conjurers");
	const UInt32 creature[] = {0x00000013};
	Check(walkers::PersonCategory(creature, 1) == kNoCategory, "CreatureFaction alone: none");
	const UInt32 modFaction[] = {0x010177E3};
	Check(walkers::PersonCategory(modFaction, 1) == kNoCategory,
	      "a mod's faction with BanditFaction's local ID: none - only Oblivion.esm's (index 00)");
	Check(walkers::PersonCategory(nullptr, 0) == kNoCategory, "no faction: none");
}

void TestTable() {
	std::printf("The categories\n");
	Check(kReplaceCategoryCount == 45, "45 categories: 34 of creatures, 11 of people");
	bool unique = true;
	bool lettersOnly = true;
	bool shortKeys = true;
	bool shape = true;
	for (UInt32 k = 0; k < kReplaceCategoryCount; ++k) {
		const auto& c = kReplaceCategories[k];
		for (UInt32 j = k + 1; j < kReplaceCategoryCount; ++j) {
			unique = unique && std::strcmp(c.key, kReplaceCategories[j].key) != 0;
		}
		for (const char* p = c.key; *p != '\0'; ++p) {
			lettersOnly = lettersOnly && std::isalpha(static_cast<unsigned char>(*p)) != 0;
		}
		shortKeys = shortKeys && std::strlen(c.key) <= 16;
		const bool person = walkers::IsPersonCategory(static_cast<UInt8>(k));
		shape = shape && (person ? c.folders == nullptr && c.factions[0] != 0 : c.folders != nullptr);
	}
	Check(unique && lettersOnly && shortKeys, "each key its own, letters only, 16 at most (ini_cat_<key> in LINK)");
	Check(shape, "a creature category names folders, a person category factions");
	bool creaturesFirst = true;
	for (UInt32 k = 1; k < kReplaceCategoryCount; ++k) {
		creaturesFirst = creaturesFirst && !(walkers::IsPersonCategory(static_cast<UInt8>(k - 1)) &&
		                                     !walkers::IsPersonCategory(static_cast<UInt8>(k)));
	}
	Check(creaturesFirst && walkers::kFirstPersonCategory == 34, "the creatures first, the people from 34 on");
	walkers::ReplaceSettings settings;
	bool peopleOff = true;
	bool creaturesOn = true;
	for (UInt32 k = 0; k < kReplaceCategoryCount; ++k) {
		const bool off = k == IndexOf("TameAnimals") || k == IndexOf("Slaughterfish");
		if (walkers::IsPersonCategory(static_cast<UInt8>(k))) {
			peopleOff = peopleOff && !settings.categories[k];
		} else {
			creaturesOn = creaturesOn && settings.categories[k] == !off;
		}
	}
	Check(peopleOff, "every person category off by default (Nadi, 2026-10-24: people start at 0)");
	Check(creaturesOn, "every creature category on but the tame animals and the slaughterfish, as the lines were");
}

CreatureFacts Facts(bool scripted = false, UInt32 flags = 0x40) {
	CreatureFacts facts;
	facts.type = walkers::kCreatureTypeUndead;
	facts.confidence = 100;
	facts.flags = flags;
	facts.scripted = scripted;
	return facts;
}

void TestDecision() {
	std::printf("What a spawn's entry becomes\n");
	const UInt8 skeletons = IndexOf("Skeletons");
	walkers::ReplaceSettings on;
	walkers::ReplaceSettings off;
	off.categories[skeletons] = false;
	walkers::ReplaceSettings disabled;
	disabled.enabled = false;

	Check(walkers::ReplaceOn(on, Facts(), skeletons, -1, -1), "a skeleton, Skeletons on: a walker");
	Check(!walkers::ReplaceOn(off, Facts(), skeletons, 1, -1),
	      "Skeletons off: kept - a [ReplaceCreatures] line of 1 does not count for a creature with a category");
	Check(!walkers::ReplaceOn(disabled, Facts(), skeletons, -1, 1),
	      "[Replace] Enabled=0: kept, whatever the category or an exception says");
	Check(walkers::ReplaceOn(off, Facts(), skeletons, -1, 1), "an exception of 1: a walker though Skeletons is off");
	Check(!walkers::ReplaceOn(on, Facts(), skeletons, -1, 0), "an exception of 0: kept though Skeletons is on");
	Check(!walkers::ReplaceOn(on, Facts(true), skeletons, -1, -1),
	      "a skeleton with a script (a quest may count on it): kept though Skeletons is on");
	Check(walkers::ReplaceOn(on, Facts(true), skeletons, -1, 1), "a scripted one with an exception of 1: a walker");
	Check(!walkers::ReplaceOn(on, Facts(false, walkers::kCreatureSwims), IndexOf("Rats"), -1, -1),
	      "one that only swims: kept though its category is on - it would stand at a lake's bottom");

	CreatureFacts ours = Facts();
	ours.ours = true;
	Check(!walkers::ReplaceOn(on, ours, kNoCategory, 1, 1), "a walker of the ESP: never replaced by itself");

	Check(walkers::ReplaceOn(on, Facts(), kNoCategory, 1, -1), "no category, its line 1: a walker");
	Check(!walkers::ReplaceOn(on, Facts(), kNoCategory, 0, -1), "no category, its line 0: kept");
	Check(walkers::ReplaceOn(on, Facts(), kNoCategory, -1, -1),
	      "no category, no line yet: as its kind says - a fighting creature becomes a walker");
	CreatureFacts person = Facts();
	person.person = true;
	Check(!walkers::ReplaceOn(on, person, kNoCategory, -1, -1), "a person without category or line: kept");
	Check(walkers::ReplaceOn(on, Facts(true), kNoCategory, 1, -1),
	      "no category, a scripted creature's line set to 1: a walker, as before");

	walkers::ReplaceSettings bandits;
	bandits.categories[IndexOf("Bandits")] = true;
	Check(walkers::ReplaceOn(bandits, person, IndexOf("Bandits"), -1, -1), "a bandit, Bandits on: a walker");
	Check(!walkers::ReplaceOn(on, person, IndexOf("Bandits"), -1, -1), "a bandit, Bandits off (the default): kept");

	Check(walkers::WritesLine(kNoCategory) && !walkers::WritesLine(skeletons),
	      "a new [ReplaceCreatures] line only for one without a category - the category is its switch");
}

}  // namespace

int main() {
	TestFolder();
	TestFolderMatch();
	TestCreatureCategory();
	TestPersonCategory();
	TestTable();
	TestDecision();
	if (g_failures != 0) {
		std::printf("%d FAILED\n", g_failures);
		return 1;
	}
	std::printf("all passed\n");
	return 0;
}
