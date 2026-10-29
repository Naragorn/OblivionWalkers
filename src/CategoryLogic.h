#pragma once

// The spawn replacement's categories, process-free (docs/walkers-spec.md
// 4.6a; Nadi, 2026-10-29: "1 für Skeletons und alle skeletons egal wie die
// im CS heissen werden ersetzt"). A creature's category is the folder its
// model lies in - every skeleton of the game, Bregor the Cremator included,
// is Creatures\Skeleton\Skeleton.NIF, MOO's are MOO\Skeleton\ (the records
// read with tools/esm-spawns.py, 2026-10-29); MOO itself tells its zombies by
// that path (`\Zombie\`). A person's is a faction of Oblivion.esm - a
// vampire's class may be any of twenty, but each is in VampireFaction, and
// MOO's people are in the same factions (703 vampires, 229 bandits).

#include <cstring>

#include "Types.h"

namespace walkers {

// TESCreature::type (xOBSE GameForms.h, at +0x104 in 1.2).
inline constexpr UInt8 kCreatureTypeCreature = 0;
inline constexpr UInt8 kCreatureTypeDaedra = 1;
inline constexpr UInt8 kCreatureTypeUndead = 2;
inline constexpr UInt8 kAnyCreatureType = 0xFF;

inline constexpr UInt8 kNoCategory = 0xFF;

struct ReplaceCategory {
	const char* key;          // [ReplaceCategories] <key>=, LINK's ini_cat_<key in small letters>
	const char* title;        // LINK's menu
	const char* description;  // LINK's menu
	// A creature category: the model folders, "|" between them, a "*" at the
	// end for every folder that starts so. nullptr for a person category.
	const char* folders;
	UInt8 type;  // only creatures of this type (kAnyCreatureType: any)
	// A person category: Oblivion.esm's factions (0 for none).
	UInt32 factions[3];
	bool defaultOn;
};

// In the order LINK shows them and they are matched: the first that fits
// decides - the skinned hounds (the dog's model, undead) before the wolves,
// the vampires before the cults (MOO's vampire necromancers), the
// necromancers before the conjurers (MOO's cult mages are in both). The
// creatures start on and the people off, as their [ReplaceCreatures] lines
// did (Nadi, 2026-10-24): the tame animals (deer, sheep, wild horses) and
// the slaughterfish (they only swim) off too.
inline constexpr ReplaceCategory kReplaceCategories[] = {
	{"Skeletons", "Skeletons", "Every skeleton, the game's and the mods', whatever its name.", "skeleton",
	 kAnyCreatureType, {}, true},
	{"Zombies", "Zombies", "Every zombie, the game's and the mods'.", "zombie", kAnyCreatureType, {}, true},
	{"Ghosts", "Ghosts and wraiths", "Ghosts, wraiths, spectres and the sanctified dead.", "ghost|wraith",
	 kAnyCreatureType, {}, true},
	{"Liches", "Liches", "Liches and nether liches.", "lich", kAnyCreatureType, {}, true},
	{"Shambles", "Shambles", "The Shivering Isles' shambles.", "shambles", kAnyCreatureType, {}, true},
	{"SkinnedHounds", "Skinned hounds", "The Shivering Isles' skinned hounds (the dog's model, undead).", "dog",
	 kCreatureTypeUndead, {}, true},
	{"Atronachs", "Atronachs", "Flame, frost, storm and flesh atronachs.",
	 "flameatronach|frostatronach|stormatronach|fleshatronach", kAnyCreatureType, {}, true},
	{"Clannfears", "Clannfears", "Clannfears and clannfear runts.", "clannfear", kAnyCreatureType, {}, true},
	{"Daedroths", "Daedroths", "Daedroths.", "daedroth", kAnyCreatureType, {}, true},
	{"Scamps", "Scamps", "Scamps.", "scamp", kAnyCreatureType, {}, true},
	{"SpiderDaedra", "Spider daedra", "Spider daedra.", "spiderdaedra", kAnyCreatureType, {}, true},
	{"Xivilai", "Xivilai", "Xivilai.", "xivilai*", kAnyCreatureType, {}, true},
	{"Hungers", "Hungers", "The Shivering Isles' hungers.", "hunger", kAnyCreatureType, {}, true},
	{"Goblins", "Goblins", "Every goblin, the tribes' shamans and warlords too.", "goblin", kAnyCreatureType, {},
	 true},
	{"Trolls", "Trolls", "Trolls.", "troll", kAnyCreatureType, {}, true},
	{"Ogres", "Ogres", "Ogres.", "ogre", kAnyCreatureType, {}, true},
	{"Minotaurs", "Minotaurs", "Minotaurs and minotaur lords.", "minotaur", kAnyCreatureType, {}, true},
	{"LandDreughs", "Land dreughs", "Land dreughs.", "landdreugh", kAnyCreatureType, {}, true},
	{"Imps", "Imps", "Imps.", "imp", kAnyCreatureType, {}, true},
	{"Spriggans", "Spriggans", "Spriggans.", "spriggan", kAnyCreatureType, {}, true},
	{"Wisps", "Will-o-the-wisps", "Will-o-the-wisps.", "willothewisp|wisp*", kAnyCreatureType, {}, true},
	{"Wolves", "Wolves and dogs", "Wolves, timber wolves and dogs.", "dog", kAnyCreatureType, {}, true},
	{"Bears", "Bears", "Bears and their cubs.", "bear", kAnyCreatureType, {}, true},
	{"MountainLions", "Mountain lions", "Mountain lions.", "mountainlion", kAnyCreatureType, {}, true},
	{"Boars", "Boars", "Boars.", "boar", kAnyCreatureType, {}, true},
	{"Rats", "Rats", "Rats.", "rat", kAnyCreatureType, {}, true},
	{"MudCrabs", "Mud crabs", "Mud crabs.", "mudcrab", kAnyCreatureType, {}, true},
	{"TameAnimals", "Deer, sheep and wild horses", "The animals that never fight.", "deer|sheep|horse",
	 kAnyCreatureType, {}, false},
	{"Slaughterfish", "Slaughterfish", "Those that only swim stay - they would stand at a lake's bottom.",
	 "slaughterfish", kAnyCreatureType, {}, false},
	{"Elytra", "Elytra", "The Shivering Isles' elytra.", "elytra", kAnyCreatureType, {}, true},
	{"Gnarls", "Gnarls", "The Shivering Isles' gnarls.", "gnarl", kAnyCreatureType, {}, true},
	{"Grummites", "Grummites", "The Shivering Isles' grummites.", "grummite", kAnyCreatureType, {}, true},
	{"Baliwogs", "Baliwogs", "The Shivering Isles' baliwogs.", "baliwog", kAnyCreatureType, {}, true},
	{"Scalons", "Scalons", "The Shivering Isles' scalons.", "murkdweller", kAnyCreatureType, {}, true},
	// People: Oblivion.esm's factions (FACT form IDs read 2026-10-29).
	{"Vampires", "Vampires", "Every vampire of the leveled lists (VampireFaction).", nullptr, kAnyCreatureType,
	 {0x033F53}, false},
	{"Necromancers", "Necromancers", "Necromancers and their adepts.", nullptr, kAnyCreatureType,
	 {0x00C0F2, 0x04B90C}, false},
	{"Conjurers", "Conjurers", "Conjurers and their adepts.", nullptr, kAnyCreatureType, {0x04B90B}, false},
	{"Bandits", "Bandits", "Bandits, Black Bow bandits and the Veyond tribe.", nullptr, kAnyCreatureType,
	 {0x0177E3, 0x08595C, 0x00555F}, false},
	{"Marauders", "Marauders", "Marauders and their mercenaries.", nullptr, kAnyCreatureType, {0x035FD1}, false},
	{"Dremora", "Dremora", "The dremora of the Oblivion planes.", nullptr, kAnyCreatureType, {0x03E9A6}, false},
	{"MythicDawn", "Mythic Dawn", "The Mythic Dawn's acolytes and guards.", nullptr, kAnyCreatureType,
	 {0x029F82, 0x047221}, false},
	{"Adventurers", "Adventurers", "The adventurers in the dungeons.", nullptr, kAnyCreatureType, {0x01F1A6}, false},
	{"Zealots", "Zealots", "The Shivering Isles' zealots.", nullptr, kAnyCreatureType, {0x04117B}, false},
	{"Heretics", "Heretics", "The Shivering Isles' heretics.", nullptr, kAnyCreatureType, {0x04117C}, false},
	{"KnightsOfOrder", "Knights of Order", "The Shivering Isles' knights and priests of Order.", nullptr,
	 kAnyCreatureType, {0x0134D4}, false},
};
inline constexpr UInt32 kReplaceCategoryCount = sizeof(kReplaceCategories) / sizeof(kReplaceCategories[0]);
inline constexpr UInt32 kFirstPersonCategory = 34;

inline bool IsPersonCategory(UInt8 category) {
	return category < kReplaceCategoryCount && kReplaceCategories[category].folders == nullptr;
}

// The last folder of a model's path, in small letters ("Creatures\Skeleton\
// Skeleton.NIF": skeleton); false without one, or when it does not fit.
inline bool ModelFolder(const char* path, char* out, UInt32 capacity) {
	if (path == nullptr || capacity == 0) {
		return false;
	}
	const char* end = nullptr;
	const char* begin = path;
	for (const char* p = path; *p != '\0'; ++p) {
		if (*p == '\\' || *p == '/') {
			end = p;
		}
	}
	if (end == nullptr) {
		return false;
	}
	// The folder ends at the last separator - before the file's name, or at
	// the path's end when it ends in its folder.
	for (const char* p = path; p < end; ++p) {
		if (*p == '\\' || *p == '/') {
			begin = p + 1;
		}
	}
	const UInt32 length = static_cast<UInt32>(end - begin);
	if (length == 0 || length + 1 > capacity) {
		return false;
	}
	for (UInt32 i = 0; i < length; ++i) {
		const char c = begin[i];
		out[i] = c >= 'A' && c <= 'Z' ? static_cast<char>(c + 32) : c;
	}
	out[length] = '\0';
	return true;
}

// `folder` among a category's `folders` ("ghost|wraith", "wisp*").
inline bool FolderMatches(const char* folders, const char* folder) {
	if (folders == nullptr || folder == nullptr || folder[0] == '\0') {
		return false;
	}
	const UInt32 length = static_cast<UInt32>(std::strlen(folder));
	for (const char* name = folders; *name != '\0';) {
		const char* bar = std::strchr(name, '|');
		const UInt32 size = static_cast<UInt32>(bar != nullptr ? bar - name : std::strlen(name));
		const bool prefix = size > 0 && name[size - 1] == '*';
		const UInt32 stem = prefix ? size - 1 : size;
		if (prefix ? length >= stem && std::strncmp(folder, name, stem) == 0
		           : length == stem && std::strncmp(folder, name, stem) == 0) {
			return true;
		}
		if (bar == nullptr) {
			break;
		}
		name = bar + 1;
	}
	return false;
}

// A creature's category by its model's folder and its type; kNoCategory
// for a folder no category names.
inline UInt8 CreatureCategory(const char* modelPath, UInt8 type) {
	char folder[64];
	if (!ModelFolder(modelPath, folder, sizeof(folder))) {
		return kNoCategory;
	}
	for (UInt32 k = 0; k < kFirstPersonCategory; ++k) {
		const ReplaceCategory& c = kReplaceCategories[k];
		if ((c.type == kAnyCreatureType || c.type == type) && FolderMatches(c.folders, folder)) {
			return static_cast<UInt8>(k);
		}
	}
	return kNoCategory;
}

// A person's category by the full form IDs of its factions: the first
// category one of Oblivion.esm's (load index 00) is in; kNoCategory else.
inline UInt8 PersonCategory(const UInt32* factions, UInt32 count) {
	for (UInt32 k = kFirstPersonCategory; k < kReplaceCategoryCount; ++k) {
		for (UInt32 f = 0; factions != nullptr && f < count; ++f) {
			for (const UInt32 id : kReplaceCategories[k].factions) {
				if (id != 0 && factions[f] == id) {
					return static_cast<UInt8>(k);
				}
			}
		}
	}
	return kNoCategory;
}

}  // namespace walkers
