#include "Replace.h"

#include <windows.h>

#include <cstdio>
#include <cstring>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "BossCopyLogic.h"
#include "DungeonQuestLogic.h"
#include "Engine.h"
#include "NightLogic.h"

namespace walkers {
namespace {

using namespace engine;

// The engine's form table: NiTPointerMap<TESForm> at 0x00B0613C itself
// (LookupFormByID 0x0046B250 loads its ecx with that address, the bytes
// read 2026-10-24; xOBSE GameTypes.cpp names it g_formTable): the bucket
// count at +4, the buckets at +8, an entry {next, key, form}.
constexpr UInt32 kFormTable = 0x00B0613C;
constexpr UInt32 kMapBucketCountOffset = 0x04;
constexpr UInt32 kMapBucketsOffset = 0x08;
constexpr UInt32 kMapEntryFormOffset = 0x08;
// TESLevCreature: TESLeveledList at +0x24, its first ListEntry {data, next}
// inline at +4 of that; ListData {level, pad, form, count} (xOBSE
// GameForms.h).
constexpr UInt32 kLevCreatureListOffset = 0x28;
// The list's flags byte (LVLF), at the TESLeveledList's +0xD - as
// CalcElement and CalcItemForContainer read it (0x0046CC70, 0x0046CDE0;
// the list component at +0x24, its entries at +0x28).
constexpr UInt32 kLevCreatureFlagsOffset = 0x24 + 0x0D;
constexpr UInt32 kListDataFormOffset = 0x04;
constexpr UInt32 kListDataCountOffset = 0x08;
// TESCreature in 1.2 (xOBSE GameForms.h): TESActorBaseData flags +0x28,
// TESAIForm's AISettings at +0x6C (aggression, confidence, ...),
// TESHealthForm's health +0x84, TESFullName's string +0xA4,
// TESScriptableForm's script +0xC8, the type +0x104. The name and the
// script are TESActorBase's, at the same offsets in a TESNPC.
constexpr UInt32 kActorBaseFlagsOffset = 0x28;
// ACBS No Head, as the rise reads it (Rise.cpp).
constexpr UInt32 kActorBaseNoHead = 0x8000;
constexpr UInt32 kCreatureConfidenceOffset = 0x6D;
constexpr UInt32 kActorBaseHealthOffset = 0x84;
// TESAttributes at +0x88, its attr[8] at +4 of that: Speed the fifth.
constexpr UInt32 kActorBaseSpeedOffset = 0x90;
constexpr UInt32 kFullNameOffset = 0xA4;
// TESModel at +0xAC, its nifPath (a BSStringT, the text first) at +4 of it
// (xOBSE GameForms.h TESActorBase): a creature's skeleton,
// "Creatures\Skeleton\Skeleton.NIF" (CategoryLogic.h).
constexpr UInt32 kModelPathOffset = 0xB0;
// TESActorBaseData at +0x24, its factionList inline at +0x18 of it: a
// FactionListEntry {data, next}, data {faction, rank} (xOBSE GameForms.h).
constexpr UInt32 kFactionListOffset = 0x3C;
constexpr UInt32 kFactionsMost = 32;
constexpr UInt32 kScriptOffset = 0xC8;
constexpr UInt32 kCreatureTypeOffset = 0x104;
constexpr UInt32 kVariantLocalId = 0x805;  // the ESP's WalkersZombieVariant
constexpr const char* kSection = "ReplaceCreatures";
constexpr const char* kExceptionSection = "ReplaceExceptions";
constexpr UInt32 kSectionBytes = 1u << 19;

// What the session knows of a creature or person, read once: its facts, its
// category and its two INI switches (ReplaceLogic.h ReplaceOn). What its
// entries point at is decided anew from these at every pass, so a category
// switched in LINK takes at once (Nadi, 2026-10-29).
struct Known {
	CreatureFacts facts;
	UInt8 category = kNoCategory;
	int line = -1;
	int exception = -1;
};
std::unordered_map<UInt32, Known> g_known;
// The creature or person each list entry (its ListData) named before it
// pointed at the walkers - so a pass can point it back.
std::unordered_map<UInt32, UInt32> g_original;
// The leveled lists and the bases the quests name (CollectQuestNames).
std::unordered_set<UInt32> g_questLists;
std::unordered_set<UInt32> g_questBases;
// The cells a quest names, or holding a reference a quest names, by form ID
// (the refugee dungeons' A, docs/walkers-spec.md 4.20.7).
std::unordered_set<UInt32> g_questCells;
// The references a quest names, by form ID (a dungeon's map marker, its door).
std::unordered_map<UInt32, UInt32> g_questRefs;  // a named reference -> its cell (0 none)
// The lists that got their extra walker entries this session (4.13).
std::unordered_set<UInt32> g_extraDone;
// The engine's own heap, as xOBSE's TESLeveledList::CreateData and
// CreateEntry take a list's entries from it (GameAPI.cpp FormHeap_Allocate,
// cdecl (size)) - so the engine frees ours like its own when a script
// removes them.
constexpr UInt32 kFormHeapAllocate = 0x00401F00;
constexpr UInt32 kListDataBytes = 12;   // {level, pad, form, count}
constexpr UInt32 kListEntryBytes = 8;   // {data, next}

// Without MOO the any-walker list ends before its first MOO walker
// (WalkerLooks.h VariantKeeps): their meshes are not there. The MOO looks
// follow the vanilla ones, and the list's first entry is a vanilla walker,
// so the cut is one link. Without `keepHeadless` (DieOnlyBeheaded, 4.17)
// every walker without a head is unlinked too - the first entry, inline in
// the list, by taking its successor's place. Returns the entries kept.
UInt32 TrimAnyWalker(UInt32 anyWalker, bool mooLoaded, bool keepHeadless, LogFn log) {
	UInt32 kept = 0;
	UInt32 dropped = 0;
	UInt32 previous = 0;
	UInt32 node = anyWalker + kLevCreatureListOffset;
	while (LooksLikeObject(node) && kept < kAnyWalkerEntriesMost) {
		const UInt32 data = Read(node);
		if (!LooksLikeObject(data)) {
			break;
		}
		LookFamily family = LookFamily::Vanilla;
		const UInt32 form = Read(data + kListDataFormOffset);
		const bool walker = FamilyOfLocal(FormIdOf(form) & 0x00FFFFFF, family);
		if (walker && !VariantKeeps(family, mooLoaded) && previous != 0) {
			*reinterpret_cast<UInt32*>(previous + 4) = 0;
			log("Spawns: WalkersAnyWalker cut after %u entries - MOO is not loaded, its looks are left out (%u "
			    "without a head left out before)",
			    kept, dropped);
			return kept;
		}
		const UInt32 next = Read(node + 4);
		if (walker && !keepHeadless && LooksLikeObject(form) &&
		    (Read(form + kActorBaseFlagsOffset) & kActorBaseNoHead) != 0) {
			++dropped;
			if (previous != 0) {
				*reinterpret_cast<UInt32*>(previous + 4) = next;
				node = next;
			} else if (LooksLikeObject(next)) {
				*reinterpret_cast<UInt32*>(node) = Read(next);
				*reinterpret_cast<UInt32*>(node + 4) = Read(next + 4);
			} else {
				break;
			}
			continue;
		}
		previous = node;
		node = next;
		++kept;
	}
	log("Spawns: WalkersAnyWalker %08X - %u entries, a walker with a head twice, %s%s", FormIdOf(anyWalker), kept,
	    keepHeadless ? "a headless one once" : "none without a head (DieOnlyBeheaded=1)",
	    mooLoaded ? "" : " (no MOO entry met)");
	return kept;
}

// The variant list's counts per slot (ReplaceLogic.h SpreadCount), and the
// any-walker list's flag - drawn anew for each of the count - read back.
// nightPercent (4.20.5, NightLogic.h): each slot's count at that percent of
// the day's; 100 by day.
void WriteVariantCounts(UInt32 variant, UInt32 anyWalker, const ReplaceSettings& settings, UInt32 nightPercent,
                        LogFn log) {
	UInt32 slots = 0;
	for (UInt32 node = variant + kLevCreatureListOffset; LooksLikeObject(node) && slots < kVariantEntriesMost;
	     node = Read(node + 4)) {
		if (!LooksLikeObject(Read(node))) {
			break;
		}
		++slots;
	}
	UInt32 slot = 0;
	UInt32 other = 0;
	char counts[64] = {};
	UInt32 written = 0;
	for (UInt32 node = variant + kLevCreatureListOffset; LooksLikeObject(node) && slot < slots; node = Read(node + 4)) {
		const UInt32 data = Read(node);
		if (Read(data + kListDataFormOffset) != anyWalker) {
			++other;
		}
		const UInt16 count = static_cast<UInt16>(
			NightSpawnCount(SpreadCount(settings.minCount, settings.maxCount, slot, slots), nightPercent));
		*reinterpret_cast<UInt16*>(data + kListDataCountOffset) = count;
		written += static_cast<UInt32>(
			std::snprintf(counts + written, sizeof(counts) - written, "%s%u", slot == 0 ? "" : " ", count));
		++slot;
	}
	const UInt8 flags = Read8(anyWalker + kLevCreatureFlagsOffset);
	log("Spawns: WalkersZombieVariant %08X - %u count slots, counts %s (%u %% of the day's)%s; the any-walker list's flags %02X (%s)",
	    FormIdOf(variant), slots, counts, nightPercent, other == 0 ? "" : " - some slots not the any-walker list (an older ESP)",
	    flags, (flags & kListCalcEach) != 0 ? "drawn anew for each walker" : "one draw copied - an older ESP");
}

// TESActorBase's TESContainer at +0x44, its first Entry {data, next} inline
// at +8 of it, Data {count, form}; TESForm's flags at +8, kFormFlags_QuestItem
// 0x400 (xOBSE GameForms.h, via deepwiki 2026-11-08).
constexpr UInt32 kContainerListOffset = 0x44 + 0x08;
constexpr UInt32 kContainerDataFormOffset = 0x04;
constexpr UInt32 kFormFlagsOffset = 0x08;
constexpr UInt32 kFormFlagQuestItem = 0x400;

bool CarriesQuestItem(UInt32 actorBase) {
	UInt32 visited = 0;
	for (UInt32 node = actorBase + kContainerListOffset; LooksLikeObject(node) && visited < 256;
	     node = Read(node + 4), ++visited) {
		const UInt32 data = Read(node);
		if (!LooksLikeObject(data)) {
			continue;
		}
		const UInt32 item = Read(data + kContainerDataFormOffset);
		if (LooksLikeObject(item) && (Read(item + kFormFlagsOffset) & kFormFlagQuestItem) != 0) {
			return true;
		}
	}
	return false;
}

// The boss guard (4.6c, PluginScanLogic.h): the lists and the creatures and
// people kept by their editor IDs, read once off the loaded plugin files.
// By the game's form ID: what each list's name says (kListName*) and its
// [ReplaceKeepLists] switch (-1 none) - the decision at every pass.
struct ListName {
	UInt8 bits;
	signed char iniSwitch;
};
std::unordered_map<UInt32, ListName> g_listNames;
std::unordered_set<UInt32> g_bossActors;
bool g_bossesScanned = false;
constexpr const char* kKeepListsSection = "ReplaceKeepLists";

bool SameFileName(const char* a, const char* b) {
	for (; *a != '\0' && *b != '\0'; ++a, ++b) {
		if (LowerAscii(*a) != LowerAscii(*b)) {
			return false;
		}
	}
	return *a == *b;
}

UInt8 LoadIndexOf(const char* name) {
	for (UInt32 i = 0; i <= 0xFE; ++i) {
		const char* mod = ModNameAt(i);
		if (mod == nullptr) {
			return 0xFF;
		}
		if (SameFileName(mod, name)) {
			return static_cast<UInt8>(i);
		}
	}
	return 0xFF;
}

void ScanBosses(const ReplaceSettings& settings, const char* iniPath, LogFn log) {
	g_bossesScanned = true;
	char data[260];
	std::snprintf(data, sizeof(data), "%s", iniPath != nullptr ? iniPath : "");
	for (int up = 0; up < 3; ++up) {
		char* slash = std::strrchr(data, '\\');
		if (slash == nullptr) {
			log("Bosses: no Data folder above %s - no boss guard", iniPath != nullptr ? iniPath : "(none)");
			return;
		}
		*slash = '\0';
	}
	std::vector<char> keepSection(1u << 16, '\0');
	GetPrivateProfileSectionA(kKeepListsSection, keepSection.data(), static_cast<DWORD>(keepSection.size()), iniPath);
	// By the game's form ID: kept or not, the last file's name deciding (a
	// later plugin's override renames nothing in practice, but it may).
	std::unordered_map<UInt32, bool> actors;
	std::vector<UInt8> group;
	UInt32 filesRead = 0;
	UInt32 compressedTotal = 0;
	for (UInt32 i = 0; i <= 0xFE; ++i) {
		const char* mod = ModNameAt(i);
		if (mod == nullptr) {
			break;
		}
		char path[400];
		std::snprintf(path, sizeof(path), "%s\\%s", data, mod);
		FILE* file = std::fopen(path, "rb");
		if (file == nullptr) {
			log("Bosses: %s not readable - its lists not looked at", path);
			continue;
		}
		UInt8 header[kRecordHeaderBytes];
		PluginMasters masters;
		bool ok = std::fread(header, 1, sizeof(header), file) == sizeof(header) && std::memcmp(header, "TES4", 4) == 0;
		if (ok) {
			std::vector<UInt8> tes4(Le32(header + 4));
			ok = tes4.empty() || std::fread(tes4.data(), 1, tes4.size(), file) == tes4.size();
			ok = ok && ReadMasters(tes4.data(), static_cast<UInt32>(tes4.size()), masters);
		}
		UInt8 masterIndex[kMastersMost];
		for (UInt32 m = 0; m < masters.count; ++m) {
			masterIndex[m] = LoadIndexOf(masters.names[m]);
		}
		UInt32 compressed = 0;
		UInt32 listsHere = 0;
		UInt32 actorsHere = 0;
		while (ok && std::fread(header, 1, sizeof(header), file) == sizeof(header)) {
			if (std::memcmp(header, "GRUP", 4) != 0) {
				break;
			}
			const UInt32 size = Le32(header + 4);
			if (size < kRecordHeaderBytes) {
				break;
			}
			const bool isList = std::memcmp(header + 8, "LVLC", 4) == 0;
			const bool isActor = std::memcmp(header + 8, "CREA", 4) == 0 || std::memcmp(header + 8, "NPC_", 4) == 0;
			if (!isList && !isActor) {
				if (std::fseek(file, static_cast<long>(size - kRecordHeaderBytes), SEEK_CUR) != 0) {
					break;
				}
				continue;
			}
			group.resize(size - kRecordHeaderBytes);
			if (std::fread(group.data(), 1, group.size(), file) != group.size()) {
				break;
			}
			WalkGroupRecords(group.data(), static_cast<UInt32>(group.size()), compressed,
			                 [&](const char*, UInt32 fileId, const char* edid) {
				                 const UInt32 id = ResolveFormId(fileId, masterIndex, masters.count, static_cast<UInt8>(i));
				                 if (id == 0) {
					                 return;
				                 }
				                 if (isList) {
					                 const char* owner = ModNameAt(id >> 24);
					                 const int line = owner != nullptr ? FindReplaceSwitch(keepSection.data(), owner, id) : -1;
					                 const UInt8 bits = ListNameBits(settings.bosses, edid);
					                 g_listNames[id] = ListName{bits, static_cast<signed char>(line)};
					                 listsHere += KeepListByBits(settings.bosses, bits, line) ? 1 : 0;
				                 } else {
					                 const bool keep = ActorNamedBoss(settings.bosses, edid);
					                 actors[id] = keep;
					                 actorsHere += keep ? 1 : 0;
				                 }
			                 });
		}
		std::fclose(file);
		++filesRead;
		compressedTotal += compressed;
		if (listsHere > 0 || actorsHere > 0 || compressed > 0) {
			log("Bosses: %s - %u lists and %u creatures or people named bosses%s", mod, listsHere, actorsHere,
			    compressed > 0 ? " (some records compressed, not read)" : "");
			if (compressed > 0) {
				log("Bosses: %s - %u compressed records passed over", mod, compressed);
			}
		}
	}
	UInt32 keptLists = 0;
	for (const auto& l : g_listNames) {
		keptLists += KeepListByBits(settings.bosses, l.second.bits, l.second.iniSwitch) ? 1 : 0;
	}
	for (const auto& a : actors) {
		if (a.second) {
			g_bossActors.insert(a.first);
		}
	}
	log("Bosses: KeepBosses=%d (lists by \"%s\", creatures and people by \"%s\") - %u files read, %u lists and %u "
	    "creatures or people kept from the replacement, %u compressed records passed over",
	    settings.bosses.keepBosses ? 1 : 0, settings.bosses.listWords, settings.bosses.actorWords, filesRead,
	    keptLists, static_cast<UInt32>(g_bossActors.size()), compressedTotal);
}

CreatureFacts FactsOf(UInt32 creature, UInt32 espBase) {
	CreatureFacts facts;
	facts.type = Read8(creature + kCreatureTypeOffset);
	facts.confidence = Read8(creature + kCreatureConfidenceOffset);
	facts.flags = Read(creature + kActorBaseFlagsOffset);
	facts.scripted = Read(creature + kScriptOffset) != 0;
	facts.questItem = CarriesQuestItem(creature);
	facts.questNamed = g_questBases.count(creature) != 0;
	facts.ours = IsEspForm(espBase, FormIdOf(creature));
	facts.boss = g_bossActors.count(FormIdOf(creature)) != 0;
	facts.person = Read8(creature + kFormTypeOffset) == kFormTypeNpc;
	return facts;
}

// A creature's category by its model's folder, a person's by its factions.
UInt8 CategoryOf(UInt32 creature, const CreatureFacts& facts) {
	if (facts.person) {
		UInt32 factions[kFactionsMost];
		UInt32 count = 0;
		for (UInt32 node = creature + kFactionListOffset; LooksLikeObject(node) && count < kFactionsMost;
		     node = Read(node + 4)) {
			const UInt32 data = Read(node);
			if (!LooksLikeObject(data)) {
				break;
			}
			factions[count++] = FormIdOf(Read(data));
		}
		return PersonCategory(factions, count);
	}
	const UInt32 path = Read(creature + kModelPathOffset);
	return CreatureCategory(LooksLikeObject(path) ? reinterpret_cast<const char*>(path) : nullptr, facts.type);
}

// What the session knows of one creature or person, read at its first sight:
// a new line in [ReplaceCreatures] for one without a category, by its kind.
const Known& Learn(UInt32 creature, UInt32 espBase, const ReplaceSettings& settings, const char* section,
                   const char* exceptions, std::vector<char>& added, UInt32& addedUsed, UInt32& newLines, LogFn log) {
	const auto found = g_known.find(creature);
	if (found != g_known.end()) {
		return found->second;
	}
	Known& known = g_known[creature];
	const UInt32 formId = FormIdOf(creature);
	const char* mod = ModNameAt(formId >> 24);
	known.facts = FactsOf(creature, espBase);
	if (mod == nullptr) {
		// A created record (FF) or one of no loaded mod: no line names it, and
		// it stays itself.
		known.exception = 0;
		return known;
	}
	const CreatureKind kind = ClassifyCreature(known.facts);
	if (kind == CreatureKind::Walker) {
		return known;
	}
	known.category = CategoryOf(creature, known.facts);
	known.line = FindReplaceSwitch(section, mod, formId);
	known.exception = FindReplaceSwitch(exceptions, mod, formId);
	const UInt32 name = Read(creature + kFullNameOffset);
	char key[160];
	FormatReplaceKey(key, sizeof(key), LooksLikeObject(name) ? reinterpret_cast<const char*>(name) : nullptr, mod,
	                 formId, kind);
	const bool fresh = known.line < 0 && WritesLine(known.category);
	if (fresh) {
		known.line = DefaultOn(kind) ? 1 : 0;
		if (AppendSectionLine(added.data(), static_cast<UInt32>(added.size()), &addedUsed, key, known.line)) {
			++newLines;
		}
	}
	char note[64];
	FormatFacts(note, sizeof(note), known.facts);
	const bool on = ReplaceOn(settings, known.facts, known.category, known.line, known.exception);
	log("Spawns: %s - %s - %s - %s%s%s", key, note,
	    known.category < kReplaceCategoryCount ? kReplaceCategories[known.category].key : "no category",
	    on ? "walkers" : "kept", known.exception >= 0 ? " (an exception)" : "", fresh ? " (new in the INI)" : "");
	return known;
}

// The new lines into the INI file itself, the rest of it byte for byte
// (ReplaceLogic.h InsertSectionLines): written beside it, then moved over
// it, so a failed write leaves the old file. `extra` bounds what the lines
// add. Returns what happened, for the log.
const char* AppendToIni(const char* iniPath, const char* lines, UInt32 extra) {
	std::vector<char> text;
	if (FILE* in = std::fopen(iniPath, "rb")) {
		char chunk[65536];
		for (size_t got; (got = std::fread(chunk, 1, sizeof(chunk), in)) > 0;) {
			text.insert(text.end(), chunk, chunk + got);
		}
		std::fclose(in);
	} else {
		return "the INI could not be read";
	}
	// Each line gains a carriage return, the section header its own line.
	std::vector<char> out(text.size() + extra * 2 + 64, '\0');
	const UInt32 used = InsertSectionLines(text.data(), static_cast<UInt32>(text.size()), kSection, lines, out.data(),
	                                       static_cast<UInt32>(out.size()));
	if (used == 0) {
		return "no room to put them in";
	}
	char tempPath[MAX_PATH];
	if (std::snprintf(tempPath, sizeof(tempPath), "%s.new", iniPath) >= static_cast<int>(sizeof(tempPath))) {
		return "the path is too long";
	}
	FILE* file = std::fopen(tempPath, "wb");
	if (file == nullptr) {
		return "the new INI could not be opened";
	}
	const bool whole = std::fwrite(out.data(), 1, used, file) == used;
	const bool closed = std::fclose(file) == 0;
	if (!whole || !closed) {
		DeleteFileA(tempPath);
		return "the new INI could not be written";
	}
	if (MoveFileExA(tempPath, iniPath, MOVEFILE_REPLACE_EXISTING) == 0) {
		DeleteFileA(tempPath);
		return "the new INI could not replace the old";
	}
	return "written";
}

// ExtraPercent (4.13): walker entries put beside the own of each level of
// `list`, each behind the last entry of its level, so the list stays in the
// ascending order the engine's pick walks it in. Returns the entries added.
UInt32 AddExtraEntries(UInt32 list, UInt32 variant, UInt32 percent) {
	if (!g_extraDone.insert(list).second) {
		return 0;
	}
	struct Group {
		UInt16 level;
		UInt32 size;
		UInt32 last;  // its last node
	};
	Group groups[64];
	UInt32 groupCount = 0;
	UInt32 visited = 0;
	for (UInt32 node = list + kLevCreatureListOffset; LooksLikeObject(node) && visited < 512;
	     node = Read(node + 4), ++visited) {
		const UInt32 data = Read(node);
		if (!LooksLikeObject(data)) {
			break;
		}
		const UInt16 level = *reinterpret_cast<const UInt16*>(data);
		if (groupCount > 0 && groups[groupCount - 1].level == level) {
			++groups[groupCount - 1].size;
			groups[groupCount - 1].last = node;
		} else if (groupCount < 64) {
			groups[groupCount++] = Group{level, 1, node};
		} else {
			break;
		}
	}
	using AllocateFn = void*(__cdecl*)(UInt32 size);
	const auto allocate = reinterpret_cast<AllocateFn>(kFormHeapAllocate);
	UInt32 added = 0;
	for (UInt32 g = 0; g < groupCount; ++g) {
		const UInt32 count = ExtraEntries(groups[g].size, percent, ExtraDraw(FormIdOf(list), groups[g].level));
		for (UInt32 i = 0; i < count; ++i) {
			auto* data = static_cast<UInt8*>(allocate(kListDataBytes));
			auto* entry = static_cast<UInt32*>(allocate(kListEntryBytes));
			if (data == nullptr || entry == nullptr) {
				return added;
			}
			// A new entry of ours points at the walkers from the start: it
			// never named a creature of its own.
			g_original.erase(reinterpret_cast<UInt32>(data));
			*reinterpret_cast<UInt16*>(data) = groups[g].level;
			*reinterpret_cast<UInt16*>(data + 2) = 0;
			*reinterpret_cast<UInt32*>(data + kListDataFormOffset) = variant;
			*reinterpret_cast<UInt32*>(data + kListDataCountOffset) = 1;
			entry[0] = reinterpret_cast<UInt32>(data);
			entry[1] = Read(groups[g].last + 4);
			*reinterpret_cast<UInt32*>(groups[g].last + 4) = reinterpret_cast<UInt32>(entry);
			++added;
		}
	}
	return added;
}

// What the quests name (docs/walkers-spec.md 7a, the quest audit's option 1;
// Nadi, 2026-11-08). Measured 2026-11-08 at the main menu
// (harness/quest-probe.txt): every quest's script, its stages' result
// scripts and its targets are in memory then, each name resolved - placed
// references with their base. Layouts: xOBSE GameForms.h TESQuest (script
// +0x1C, stageList +0x40 {StageEntry*, next}, targetList +0x48
// {TargetEntry*, next}); StageEntry's item list inline at +4 {item, next};
// a stage item's result script inline at +0x0C (measured: the Script vtable
// 00A49DA4 there in every item of seven quests); TargetEntry's target +0x0C;
// xOBSE Script.h: a Script's names at +0x40 {RefVariable*, next}, the name's
// form at +0x08. Dialogue result scripts are read from the file on demand
// (TESTopicInfo unk34) - not seen here.
constexpr UInt8 kFormTypeQuest = 0x3B;
constexpr UInt32 kQuestScriptOffset = 0x1C;
constexpr UInt32 kQuestStagesOffset = 0x40;
constexpr UInt32 kQuestTargetsOffset = 0x48;
constexpr UInt32 kQuestEditorIdOffset = 0x60;  // TESQuest::editorName (a BSStringT: its characters first; xOBSE GameForms.h)
constexpr UInt32 kStageItemsOffset = 0x04;
constexpr UInt32 kStageItemScriptOffset = 0x0C;
constexpr UInt32 kTargetRefOffset = 0x0C;
constexpr UInt32 kScriptNamesOffset = 0x40;
constexpr UInt32 kNameFormOffset = 0x08;
constexpr UInt32 kRefBaseOffset = 0x1C;
constexpr UInt32 kRefParentCellOffset = 0x40;  // TESObjectREFR::parentCell (Siege.cpp, Camps.cpp)

struct QuestNames {
	UInt32 quests = 0;
	UInt32 names = 0;
	UInt32 placed = 0;  // lists kept through a placed reference
	UInt32 direct = 0;  // lists named themselves
	UInt32 own = 0;     // the held dungeons' own quests, not read
};

// Who named a cell or a reference first, and how - the log's answer to "why
// is this dungeon out" (8c, 2026-11-17: 80 of 91 out on Linux).
const char* g_namingQuest = "";
const char* g_namingRoute = "";
std::unordered_map<UInt32, std::string> g_questNameWhy;

void NoteWhy(UInt32 formId) {
	if (g_questNameWhy.find(formId) == g_questNameWhy.end()) {
		g_questNameWhy[formId] = std::string(g_namingQuest) + " (" + g_namingRoute + ")";
	}
}

void NoteQuestName(UInt32 form, QuestNames& seen) {
	if (!LooksLikeObject(form)) {
		return;
	}
	++seen.names;
	const UInt8 type = Read8(form + kFormTypeOffset);
	const bool reference = type >= 0x31 && type <= 0x33;
	const UInt32 base = reference ? Read(form + kRefBaseOffset) : 0;
	if (type == kFormTypeCell) {
		g_questCells.insert(FormIdOf(form));
		NoteWhy(FormIdOf(form));
	} else if (reference) {
		// The reference with its cell: it counts for the dungeon pool only
		// while enabled (DungeonLogic.h DungeonQuestNames, 2026-11-18).
		const UInt32 cell = Read(form + kRefParentCellOffset);
		const bool inCell = LooksLikeObject(cell) && Read8(cell + kFormTypeOffset) == kFormTypeCell;
		g_questRefs.emplace(FormIdOf(form), inCell ? FormIdOf(cell) : 0u);
		NoteWhy(FormIdOf(form));
	}
	const UInt8 baseType = LooksLikeObject(base) ? Read8(base + kFormTypeOffset) : 0;
	switch (QuestNameOf(type, baseType)) {
		case QuestName::List:
			seen.direct += g_questLists.insert(form).second ? 1 : 0;
			break;
		case QuestName::PlacedList:
			seen.placed += g_questLists.insert(base).second ? 1 : 0;
			break;
		case QuestName::Base:
			g_questBases.insert(form);
			break;
		default:
			break;
	}
}

void NoteScriptNames(UInt32 script, QuestNames& seen) {
	UInt32 visited = 0;
	for (UInt32 node = script + kScriptNamesOffset; LooksLikeObject(node) && visited < 512;
	     node = Read(node + 4), ++visited) {
		const UInt32 var = Read(node);
		if (LooksLikeObject(var)) {
			NoteQuestName(Read(var + kNameFormOffset), seen);
		}
	}
}

}  // namespace

// Once, at the first pass (the main menu): every quest's names.
void CollectQuestNames(LogFn log) {
	static bool s_done = false;
	if (s_done) {
		return;
	}
	s_done = true;
	QuestNames seen;
	const UInt32 buckets = Read(kFormTable + kMapBucketsOffset);
	const UInt32 bucketCount = Read(kFormTable + kMapBucketCountOffset);
	for (UInt32 b = 0; LooksLikeObject(buckets) && b < bucketCount; ++b) {
		for (UInt32 entry = Read(buckets + b * 4); LooksLikeObject(entry); entry = Read(entry)) {
			const UInt32 quest = Read(entry + kMapEntryFormOffset);
			if (!LooksLikeObject(quest) || Read8(quest + kFormTypeOffset) != kFormTypeQuest) {
				continue;
			}
			// The held dungeons' own quests target every place's map marker:
			// not quests that need the place (measured 2026-11-13: with them
			// read, all 91 candidates went out of the pool).
			const UInt32 editorId = Read(quest + kQuestEditorIdOffset);
			if (LooksLikeObject(editorId) && IsDungeonQuestEditorId(reinterpret_cast<const char*>(editorId))) {
				++seen.own;
				continue;
			}
			++seen.quests;
			g_namingQuest = LooksLikeObject(editorId) ? reinterpret_cast<const char*>(editorId) : "?";
			g_namingRoute = "its script";
			if (LooksLikeObject(Read(quest + kQuestScriptOffset))) {
				NoteScriptNames(Read(quest + kQuestScriptOffset), seen);
			}
			g_namingRoute = "a stage's result script";
			UInt32 stages = 0;
			for (UInt32 node = quest + kQuestStagesOffset; LooksLikeObject(node) && stages < 256;
			     node = Read(node + 4), ++stages) {
				const UInt32 stage = Read(node);
				if (!LooksLikeObject(stage)) {
					continue;
				}
				UInt32 items = 0;
				for (UInt32 inode = stage + kStageItemsOffset; LooksLikeObject(inode) && items < 64;
				     inode = Read(inode + 4), ++items) {
					const UInt32 item = Read(inode);
					if (LooksLikeObject(item)) {
						NoteScriptNames(item + kStageItemScriptOffset, seen);
					}
				}
			}
			g_namingRoute = "a target";
			UInt32 targets = 0;
			for (UInt32 node = quest + kQuestTargetsOffset; LooksLikeObject(node) && targets < 64;
			     node = Read(node + 4), ++targets) {
				const UInt32 target = Read(node);
				if (LooksLikeObject(target)) {
					NoteQuestName(Read(target + kTargetRefOffset), seen);
				}
			}
		}
	}
	log("Quests: %u quests read, %u names - %u leveled lists a quest needs (%u through a placed reference, %u named "
	    "themselves), %u creatures and people named",
	    seen.quests, seen.names, static_cast<UInt32>(g_questLists.size()), seen.placed, seen.direct,
	    static_cast<UInt32>(g_questBases.size()));
	log("Quests: %u cells named themselves, %u references named (each counts for the dungeon pool while enabled); %u quests of the held dungeons' "
	    "own not read",
	    static_cast<UInt32>(g_questCells.size()), static_cast<UInt32>(g_questRefs.size()), seen.own);
	UInt32 shown = 0;
	for (const UInt32 list : g_questLists) {
		if (shown++ < 64) {
			log("Quests: leveled list %08X is a quest's", FormIdOf(list));
		}
	}
}

const char* QuestNameWhy(UInt32 formId) {
	const auto it = g_questNameWhy.find(formId);
	return it != g_questNameWhy.end() ? it->second.c_str() : nullptr;
}

std::vector<UInt32> QuestCellIds() { return std::vector<UInt32>(g_questCells.begin(), g_questCells.end()); }

std::vector<UInt32> QuestRefIds() {
	std::vector<UInt32> ids;
	ids.reserve(g_questRefs.size());
	for (const auto& r : g_questRefs) {
		ids.push_back(r.first);
	}
	return ids;
}

UInt32 QuestRefCell(UInt32 refId) {
	const auto it = g_questRefs.find(refId);
	return it != g_questRefs.end() ? it->second : 0u;
}

void RewriteVariantCounts(UInt32 espBase, const ReplaceSettings& settings, UInt32 nightPercent, LogFn log) {
	if (espBase == kNoEsp) {
		return;
	}
	const UInt32 variant = LookupForm(EspFormId(espBase, kVariantLocalId));
	const UInt32 anyWalker = LookupForm(EspFormId(espBase, kAnyWalkerLocalId));
	if (LooksLikeObject(variant) && Read8(variant + kFormTypeOffset) == kFormTypeLeveledCreature &&
	    LooksLikeObject(anyWalker) && Read8(anyWalker + kFormTypeOffset) == kFormTypeLeveledCreature) {
		WriteVariantCounts(variant, anyWalker, settings, nightPercent, log);
	}
}

// The boss guard's clean copies (BossCopyLogic.h): an ordinary list nested in
// a boss list - MOO's dynamic lists rewrite boss lists to their region lists
// in the game - is pointed at a copy of itself that names what the source
// named before the replacement. One copy per source, shared by every boss
// list that names it, built anew when the source changes.
namespace {

// TESLevCreature: 0x48 bytes (xOBSE GameForms.h: TESBoundObject, the
// TESLeveledList at +0x24, TESScriptableForm at +0x34, the template at +0x44).
constexpr UInt32 kLevCreatureBytes = 0x48;

struct CleanCopy {
	UInt32 copy = 0;
	UInt32 hash = 0;
};
std::unordered_map<UInt32, CleanCopy> g_copies;     // by source list
std::unordered_set<UInt32> g_copySet;               // the copies themselves
std::unordered_map<UInt32, UInt32> g_redirected;    // a boss list's entry data -> the source it named
std::vector<UInt32> g_bossListForms;                // the boss lists in memory, found once
bool g_bossListsFound = false;
UInt32 g_copyLinesLeft = 40;

bool IsLevList(UInt32 form) { return LooksLikeObject(form) && Read8(form + kFormTypeOffset) == kFormTypeLeveledCreature; }

bool IsBossListForm(const ReplaceSettings& settings, UInt32 form) {
	const auto named = g_listNames.find(FormIdOf(form));
	return named != g_listNames.end() && KeepListByBits(settings.bosses, named->second.bits, named->second.iniSwitch);
}

// The source's entries as a copy would name them (each replaced entry's own
// form), into `out`; the count.
UInt32 SourceEntries(UInt32 source, ListEntryView* out, UInt32* forms, UInt32 most) {
	UInt32 n = 0;
	UInt32 visited = 0;
	for (UInt32 node = source + kLevCreatureListOffset; LooksLikeObject(node) && visited < 512 && n < most;
	     node = Read(node + 4), ++visited) {
		const UInt32 data = Read(node);
		if (!LooksLikeObject(data)) {
			break;
		}
		const UInt32 current = Read(data + kListDataFormOffset);
		const auto original = g_original.find(data);
		const UInt32 form = CopySourceForm(current, original != g_original.end(), original != g_original.end() ? original->second : 0u);
		out[n].level = *reinterpret_cast<const UInt16*>(data);
		out[n].form = form;  // the pointer: the hash tells a change, the copy takes it
		out[n].count = *reinterpret_cast<const UInt16*>(data + kListDataCountOffset);
		forms[n] = form;
		++n;
	}
	return n;
}

template <typename Ours>
UInt32 EnsureCopy(const ReplaceSettings& settings, UInt32 source, UInt32 depth, const Ours& ours, LogFn log);

template <typename Ours>
EntryTarget TargetOf(const ReplaceSettings& settings, UInt32 form, const Ours& ours) {
	EntryTarget t;
	t.isList = IsLevList(form);
	if (t.isList) {
		t.isBossList = IsBossListForm(settings, form);
		t.isOurs = ours(form);
		t.isCopy = g_copySet.count(form) != 0;
	}
	return t;
}

template <typename Ours>
UInt32 EnsureCopy(const ReplaceSettings& settings, UInt32 source, UInt32 depth, const Ours& ours, LogFn log) {
	static ListEntryView entries[64];
	static UInt32 forms[64];
	const UInt32 n = SourceEntries(source, entries, forms, 64);
	const UInt32 hash = ListEntriesHash(entries, n);
	CleanCopy& c = g_copies[source];
	if (!CopyNeedsBuild(c.copy != 0, c.hash, hash)) {
		return c.copy;
	}
	using AllocateFn = void*(__cdecl*)(UInt32 size);
	const auto allocate = reinterpret_cast<AllocateFn>(kFormHeapAllocate);
	const bool fresh = c.copy == 0;
	if (fresh) {
		auto* copy = static_cast<UInt8*>(allocate(kLevCreatureBytes));
		if (copy == nullptr) {
			g_copies.erase(source);
			return 0;
		}
		std::memcpy(copy, reinterpret_cast<const void*>(source), kLevCreatureBytes);
		c.copy = reinterpret_cast<UInt32>(copy);
		g_copySet.insert(c.copy);
	}
	c.hash = hash;
	// Its entries anew (the old ones stay on the engine's heap: a few bytes
	// a change). Copied entries name their own forms; a nested ordinary list
	// its clean copy, down to kBossCopyDepthMost.
	const UInt32 copy = c.copy;
	UInt32 last = 0;
	UInt32 nested = 0;
	auto* inlineEntry = reinterpret_cast<UInt32*>(copy + kLevCreatureListOffset);
	inlineEntry[0] = 0;
	inlineEntry[1] = 0;
	// copies of nested lists first: they may rebuild the static buffers
	UInt32 own[64];
	UInt16 levels[64];
	UInt16 counts[64];
	for (UInt32 i = 0; i < n; ++i) {
		own[i] = forms[i];
		levels[i] = entries[i].level;
		counts[i] = entries[i].count;
	}
	for (UInt32 i = 0; i < n; ++i) {
		UInt32 form = own[i];
		if (CopyNestedAt(depth + 1) && BossEntryAction(true, TargetOf(settings, form, ours)) == EntryAction::Redirect) {
			const UInt32 sub = EnsureCopy(settings, form, depth + 1, ours, log);
			if (sub != 0) {
				form = sub;
				++nested;
			}
		}
		auto* data = static_cast<UInt8*>(allocate(kListDataBytes));
		if (data == nullptr) {
			break;
		}
		*reinterpret_cast<UInt16*>(data) = levels[i];
		*reinterpret_cast<UInt16*>(data + 2) = 0;
		*reinterpret_cast<UInt32*>(data + kListDataFormOffset) = form;
		*reinterpret_cast<UInt32*>(data + kListDataCountOffset) = counts[i];
		if (i == 0) {
			inlineEntry[0] = reinterpret_cast<UInt32>(data);
			last = reinterpret_cast<UInt32>(inlineEntry);
			continue;
		}
		auto* entry = static_cast<UInt32*>(allocate(kListEntryBytes));
		if (entry == nullptr) {
			break;
		}
		entry[0] = reinterpret_cast<UInt32>(data);
		entry[1] = 0;
		reinterpret_cast<UInt32*>(last)[1] = reinterpret_cast<UInt32>(entry);
		last = reinterpret_cast<UInt32>(entry);
	}
	if (log != nullptr && g_copyLinesLeft > 0) {
		--g_copyLinesLeft;
		static ListEntryView shown[64];
		for (UInt32 i = 0; i < n; ++i) {
			shown[i] = ListEntryView{levels[i], LooksLikeObject(own[i]) ? FormIdOf(own[i]) : 0u, counts[i]};
		}
		char line[400];
		ListEntriesLine(shown, n, line, sizeof(line));
		log("Bosses: a clean copy of %08X %s (depth %u) - %u entries, %u of them nested copies: %s", FormIdOf(source),
		    fresh ? "made" : "built anew, its source changed", depth, n, nested, line);
	}
	return copy;
}

}  // namespace

void GuardBossLists(UInt32 espBase, const ReplaceSettings& settings, LogFn log) {
	if (espBase == kNoEsp || !g_bossesScanned) {
		return;
	}
	const auto espList = [espBase](UInt32 local) {
		const UInt32 form = LookupForm(EspFormId(espBase, local));
		return IsLevList(form) ? form : 0u;
	};
	const UInt32 variant = espList(kVariantLocalId);
	const UInt32 daedraWithSummons = espList(kInvaderDaedraLocalId);
	const UInt32 daedraNoSummons = espList(kInvaderDaedraNoSummonLocalId);
	const auto ours = [&](UInt32 form) {
		return form != 0 && (form == variant || form == daedraWithSummons || form == daedraNoSummons);
	};
	if (!g_bossListsFound) {
		g_bossListsFound = true;
		for (const auto& l : g_listNames) {
			const UInt32 form = LookupForm(l.first);
			if (IsLevList(form)) {
				g_bossListForms.push_back(form);
			}
		}
	}
	UInt32 redirected = 0;
	UInt32 restored = 0;
	for (const UInt32 list : g_bossListForms) {
		const bool guardOn = IsBossListForm(settings, list);
		UInt32 visited = 0;
		for (UInt32 node = list + kLevCreatureListOffset; LooksLikeObject(node) && visited < 512; node = Read(node + 4), ++visited) {
			const UInt32 data = Read(node);
			if (!LooksLikeObject(data)) {
				break;
			}
			const UInt32 target = Read(data + kListDataFormOffset);
			const EntryTarget t = TargetOf(settings, target, ours);
			const EntryAction action = BossEntryAction(guardOn, t);
			if (action == EntryAction::Redirect) {
				const UInt32 copy = EnsureCopy(settings, target, 0, ours, log);
				if (copy != 0) {
					*reinterpret_cast<UInt32*>(data + kListDataFormOffset) = copy;
					g_redirected[data] = target;
					++redirected;
				}
			} else if (action == EntryAction::Restore) {
				const auto source = g_redirected.find(data);
				if (source != g_redirected.end()) {
					*reinterpret_cast<UInt32*>(data + kListDataFormOffset) = source->second;
					g_redirected.erase(source);
					++restored;
				}
			} else if (t.isCopy && guardOn) {
				// the copy's source may have changed: built anew in place
				const auto source = g_redirected.find(data);
				if (source != g_redirected.end()) {
					EnsureCopy(settings, source->second, 0, ours, log);
				}
			}
		}
	}
	if (log != nullptr && (redirected > 0 || restored > 0) && g_copyLinesLeft > 0) {
		--g_copyLinesLeft;
		log("Bosses: %u entries of boss lists pointed at clean copies, %u back to their own lists (%u copies in all)",
		    redirected, restored, static_cast<UInt32>(g_copySet.size()));
	}
}

bool IsBossCopy(UInt32 form) { return g_copySet.count(form) != 0; }

UInt32 ReplaceSpawns(UInt32 espBase, const ReplaceSettings& settings, const InvaderSettings& invaders,
                     const char* iniPath, bool mooLoaded, LogFn log) {
	CollectQuestNames(log);
	if (!g_bossesScanned) {
		ScanBosses(settings, iniPath, log);
	}
	if (espBase == kNoEsp) {
		log("Spawns: no ESP, the lists as they are");
		return 0;
	}
	const UInt32 variant = LookupForm(EspFormId(espBase, kVariantLocalId));
	if (!LooksLikeObject(variant) || Read8(variant + kFormTypeOffset) != kFormTypeLeveledCreature) {
		log("Spawns: %08X is no leveled creature list - the lists as they are", EspFormId(espBase, kVariantLocalId));
		return 0;
	}
	// The Daedra's two lists (KindLogic.h): the one this pass names, and
	// both to tell an entry of ours from a list's own.
	const auto espList = [espBase](UInt32 local) {
		const UInt32 form = LookupForm(EspFormId(espBase, local));
		return LooksLikeObject(form) && Read8(form + kFormTypeOffset) == kFormTypeLeveledCreature ? form : 0u;
	};
	const UInt32 daedraWithSummons = espList(kInvaderDaedraLocalId);
	const UInt32 daedraNoSummons = espList(kInvaderDaedraNoSummonLocalId);
	const UInt32 daedraList = espList(InvaderDaedraLocalId(invaders));
	const bool daedraInPlay = InPlay(invaders.kind, Group::Daedra);
	if (daedraInPlay && daedraList == 0) {
		log("Spawns: Invaders Mode=%u, but %08X is no leveled creature list (an older ESP) - no Daedra in the lists",
		    static_cast<UInt32>(invaders.kind), EspFormId(espBase, InvaderDaedraLocalId(invaders)));
	}
	const auto ours = [&](UInt32 form) {
		return form == variant || (form != 0 && (form == daedraWithSummons || form == daedraNoSummons));
	};
	// The list a replaced entry names: by the kind and the entry's roll.
	const auto targetOf = [&](UInt32 data) {
		if (!daedraInPlay || daedraList == 0) {
			return invaders.kind == InvaderKind::Daedra ? 0u : variant;
		}
		return ReplacementGroup(invaders, EntryRoll(data)) == Group::Daedra ? daedraList : variant;
	};
	static bool s_countsWritten = false;
	if (!s_countsWritten) {
		s_countsWritten = true;
		const UInt32 anyWalker = LookupForm(EspFormId(espBase, kAnyWalkerLocalId));
		if (LooksLikeObject(anyWalker) && Read8(anyWalker + kFormTypeOffset) == kFormTypeLeveledCreature) {
			TrimAnyWalker(anyWalker, mooLoaded, settings.headlessWalkers, log);
			WriteVariantCounts(variant, anyWalker, settings, 100, log);
		} else {
			log("Spawns: %08X is no leveled creature list (an older ESP) - the variant list's counts as written",
			    EspFormId(espBase, kAnyWalkerLocalId));
		}
	}
	std::vector<char> section(kSectionBytes, '\0');
	std::vector<char> exceptions(kSectionBytes, '\0');
	if (iniPath != nullptr && iniPath[0] != '\0') {
		const DWORD read = GetPrivateProfileSectionA(kSection, section.data(), kSectionBytes, iniPath);
		if (read >= kSectionBytes - 2) {
			log("Spawns: [%s] is longer than %u bytes - read as far as it went", kSection, kSectionBytes);
		}
		GetPrivateProfileSectionA(kExceptionSection, exceptions.data(), kSectionBytes, iniPath);
	}
	std::vector<char> added(kSectionBytes, '\0');
	UInt32 addedUsed = 0;
	UInt32 newLines = 0;

	const UInt32 buckets = Read(kFormTable + kMapBucketsOffset);
	const UInt32 bucketCount = Read(kFormTable + kMapBucketCountOffset);
	UInt32 forms = 0;
	UInt32 lists = 0;
	UInt32 listsTouched = 0;
	UInt32 replaced = 0;
	UInt32 restored = 0;
	UInt32 walkerEntries = 0;
	UInt32 extraAdded = 0;
	UInt32 extraLists = 0;
	UInt32 questListsSeen = 0;
	UInt32 questEntriesKept = 0;
	UInt32 daedraEntries = 0;
	UInt32 bossListsSeen = 0;
	UInt32 bossEntriesKept = 0;
	UInt32 daedraKept = 0;
	for (UInt32 b = 0; LooksLikeObject(buckets) && b < bucketCount; ++b) {
		for (UInt32 entry = Read(buckets + b * 4); LooksLikeObject(entry); entry = Read(entry)) {
			const UInt32 form = Read(entry + kMapEntryFormOffset);
			++forms;
			if (!LooksLikeObject(form) || ours(form) || Read8(form + kFormTypeOffset) != kFormTypeLeveledCreature) {
				continue;
			}
			++lists;
			const bool questList = g_questLists.count(form) != 0;
			const auto named = g_listNames.find(FormIdOf(form));
			const bool bossList = named != g_listNames.end() &&
			                      KeepListByBits(settings.bosses, named->second.bits, named->second.iniSwitch);
			bossListsSeen += bossList ? 1 : 0;
			questListsSeen += questList ? 1 : 0;
			bool touched = false;
			UInt32 kept = 0;
			UInt32 visited = 0;
			for (UInt32 node = form + kLevCreatureListOffset; LooksLikeObject(node) && visited < 512;
			     node = Read(node + 4), ++visited) {
				const UInt32 data = Read(node);
				if (!LooksLikeObject(data)) {
					break;
				}
				const UInt32 target = Read(data + kListDataFormOffset);
				if (!LooksLikeObject(target)) {
					continue;
				}
				UInt32 own = target;
				if (ours(target)) {
					// Ours from an earlier pass - or an extra entry (4.13), which
					// named nothing of its own: it stays ours, its group as the
					// kind says now.
					const auto original = g_original.find(data);
					if (original == g_original.end()) {
						const UInt32 extra = targetOf(data);
						if (extra != 0 && extra != target) {
							*reinterpret_cast<UInt32*>(data + kListDataFormOffset) = extra;
						}
						continue;
					}
					own = original->second;
				} else {
					// A creature or a person: Oblivion's leveled creature lists
					// hold both (LL1BanditBossLvl100 holds NPC_ records only).
					const UInt8 targetType = Read8(target + kFormTypeOffset);
					if (targetType != kFormTypeCreature && targetType != kFormTypeNpc) {
						continue;
					}
					g_original[data] = target;
				}
				const Known& known = Learn(own, espBase, settings, section.data(), exceptions.data(), added,
				                           addedUsed, newLines, log);
				const bool listed =
				    ReplaceInList(settings, questList, known.facts, known.category, known.line, known.exception, bossList);
				if (bossList && ReplaceOn(settings, known.facts, known.category, known.line, known.exception)) {
					++bossEntriesKept;
				}
				// The Daedra in play keep their own categories (KindLogic.h).
				const bool daedraKeeps = listed && !CategoryReplaced(invaders.kind, known.category, true);
				daedraKept += daedraKeeps ? 1 : 0;
				const UInt32 replacement = listed && !daedraKeeps ? targetOf(data) : 0u;
				const bool on = replacement != 0;
				if (questList && settings.keepQuestLists && ReplaceOn(settings, known.facts, known.category, known.line,
				                                                      known.exception)) {
					++questEntriesKept;
				}
				const UInt32 wanted = on ? replacement : own;
				if (wanted != target) {
					*reinterpret_cast<UInt32*>(data + kListDataFormOffset) = wanted;
					++(on ? replaced : restored);
				}
				if (on) {
					++(replacement == variant ? walkerEntries : daedraEntries);
					touched = true;
				} else {
					++kept;
				}
			}
			listsTouched += touched ? 1 : 0;
			// Extra walkers only where something of the list's own stays: a
			// list all walkers has nothing to add to, one of lists only feeds
			// its nested lists, which get theirs.
			if (settings.extraPercent > 0 && kept > 0 && !(questList && settings.keepQuestLists) && !bossList) {
				const UInt32 extraTarget = targetOf(form);
				const UInt32 more = extraTarget == 0 ? 0 : AddExtraEntries(form, extraTarget, settings.extraPercent);
				extraAdded += more;
				extraLists += more > 0 ? 1 : 0;
			}
		}
	}
	UInt32 on = 0;
	for (const auto& known : g_known) {
		const Known& k = known.second;
		on += ReplaceOn(settings, k.facts, k.category, k.line, k.exception) ? 1 : 0;
	}
	log("Spawns: %u forms, %u leveled creature lists (Enabled=%d) - %u entries in %u lists point at the walkers, %u of "
	    "them new, %u entries back to their own; %u creatures and people known, %u of them walkers",
	    forms, lists, settings.enabled ? 1 : 0, walkerEntries, listsTouched, replaced, restored,
	    static_cast<UInt32>(g_known.size()), on);
	if (invaders.kind != InvaderKind::Walkers) {
		log("Spawns: Invaders Mode=%u DaedraPercent=%u Summons=%d - %u entries point at the Daedra (%08X), %u entries "
		    "of the Daedra's own categories stay the game's",
		    static_cast<UInt32>(invaders.kind), invaders.daedraPercent, invaders.summons ? 1 : 0, daedraEntries,
		    daedraList != 0 ? FormIdOf(daedraList) : 0u, daedraKept);
	}
	log("Spawns: KeepBosses=%d KeepHardLists=%d KeepHighLists=%d - %u lists kept by their names; %u entries in them "
	    "that would be replaced stay their own",
	    settings.bosses.keepBosses ? 1 : 0, settings.bosses.keepHard ? 1 : 0, settings.bosses.keepHigh ? 1 : 0,
	    bossListsSeen, bossEntriesKept);
	log("Spawns: KeepQuestLists=%d - %u of the lists are a quest's; %u entries in them that would be walkers stay "
	    "their own",
	    settings.keepQuestLists ? 1 : 0, questListsSeen, questEntriesKept);
	if (settings.extraPercent > 0) {
		log("Spawns: ExtraPercent=%u - %u walker entries added beside the lists' own, in %u lists (%u lists had theirs "
		    "this session)",
		    settings.extraPercent, extraAdded, extraLists, static_cast<UInt32>(g_extraDone.size()));
	}
	if (newLines > 0 && iniPath != nullptr && iniPath[0] != '\0') {
		log("Spawns: %u new creatures written to [%s] (%s)", newLines, kSection,
		    AppendToIni(iniPath, added.data(), addedUsed + newLines + 64));
	}
	GuardBossLists(espBase, settings, log);
	LogTestList(settings.testLogList, "after the pass", log);
	return replaced;
}

namespace {
UInt32 g_testListHash = 0;

UInt32 ReadTestList(UInt32 listId, ListEntryView* out, UInt32 most, UInt8& flags, UInt8& chanceNone) {
	const UInt32 list = LookupForm(listId);
	if (!LooksLikeObject(list) || Read8(list + kFormTypeOffset) != kFormTypeLeveledCreature) {
		return 0xFFFFFFFFu;
	}
	flags = Read8(list + kLevCreatureFlagsOffset);
	chanceNone = Read8(list + kLevCreatureFlagsOffset - 1);
	UInt32 n = 0;
	UInt32 visited = 0;
	for (UInt32 node = list + kLevCreatureListOffset; LooksLikeObject(node) && visited < 512 && n < most;
	     node = Read(node + 4), ++visited) {
		const UInt32 data = Read(node);
		if (!LooksLikeObject(data)) {
			break;
		}
		const UInt32 target = Read(data + kListDataFormOffset);
		out[n].level = *reinterpret_cast<const UInt16*>(data);
		out[n].form = LooksLikeObject(target) ? FormIdOf(target) : 0u;
		out[n].count = *reinterpret_cast<const UInt16*>(data + kListDataCountOffset);
		++n;
	}
	return n;
}
}  // namespace

namespace {
UInt32 CopiesIn(UInt32 listId) {
	UInt32 copies = 0;
	UInt32 visited = 0;
	const UInt32 listForm = LookupForm(listId);
	for (UInt32 node = listForm + kLevCreatureListOffset; LooksLikeObject(node) && visited < 512; node = Read(node + 4), ++visited) {
		const UInt32 data = Read(node);
		if (!LooksLikeObject(data)) {
			break;
		}
		copies += IsBossCopy(Read(data + kListDataFormOffset)) ? 1u : 0u;
	}
	return copies;
}
}  // namespace

void LogTestList(UInt32 listId, const char* why, LogFn log) {
	if (listId == 0 || log == nullptr) {
		return;
	}
	static ListEntryView entries[64];
	UInt8 flags = 0;
	UInt8 chanceNone = 0;
	const UInt32 n = ReadTestList(listId, entries, 64, flags, chanceNone);
	if (n == 0xFFFFFFFFu) {
		log("Spawns: TestLogList=%08X - no leveled creature list (%s)", listId, why);
		return;
	}
	g_testListHash = ListEntriesHash(entries, n) ^ (CopiesIn(listId) * 2654435761u);
	char line[700];
	ListEntriesLine(entries, n, line, sizeof(line));
	const auto named = g_listNames.find(listId);
	const UInt32 copies = CopiesIn(listId);
	log("Spawns: TestLogList=%08X %s - %u entries (level:form x count): %s; %u of them the boss guard's clean copies; "
	    "flags %02X, chance none %u; named %s",
	    listId, why, n, line, copies, flags, chanceNone,
	    named == g_listNames.end() ? "nothing (not read off the files)" : (named->second.bits != 0 ? "a boss's" : "an ordinary one"));
}

void WatchTestList(UInt32 listId, LogFn log) {
	if (listId == 0) {
		return;
	}
	static ListEntryView entries[64];
	UInt8 flags = 0;
	UInt8 chanceNone = 0;
	const UInt32 n = ReadTestList(listId, entries, 64, flags, chanceNone);
	if (n != 0xFFFFFFFFu && (ListEntriesHash(entries, n) ^ (CopiesIn(listId) * 2654435761u)) != g_testListHash) {
		LogTestList(listId, "changed in the game", log);
	}
}

// TESActorBaseData's faction list (kFactionListOffset): FactionListEntry
// {data, next}, the first inline; data {TESFaction*, SInt8 rank, pad[3]}
// (xOBSE GameForms.h). The entries this session appended, by record.
std::unordered_map<UInt32, UInt32> g_alliedNode;

void SetWalkersAllied(const UInt32* walkerIds, UInt32 count, bool allied, LogFn log) {
	const UInt32 faction = LookupForm(kDaedraFaction);
	if (!LooksLikeObject(faction)) {
		log("Invaders: DaedraFaction %08X not found - the walkers keep their factions", kDaedraFaction);
		return;
	}
	using AllocateFn = void*(__cdecl*)(UInt32 size);
	const auto allocate = reinterpret_cast<AllocateFn>(kFormHeapAllocate);
	UInt32 joined = 0;
	UInt32 left = 0;
	UInt32 already = 0;
	for (UInt32 i = 0; i < count; ++i) {
		const UInt32 record = LookupForm(walkerIds[i]);
		if (!LooksLikeObject(record)) {
			continue;
		}
		UInt32 last = 0;
		UInt32 before = 0;  // the node before ours, to unlink it
		bool member = false;
		UInt32 visited = 0;
		for (UInt32 node = record + kFactionListOffset; LooksLikeObject(node) && visited < kFactionsMost;
		     node = Read(node + 4), ++visited) {
			const UInt32 data = Read(node);
			const auto ours = g_alliedNode.find(record);
			if (ours != g_alliedNode.end() && Read(node + 4) == ours->second) {
				before = node;
			}
			member = member || (LooksLikeObject(data) && Read(data) == faction);
			last = node;
		}
		const auto ours = g_alliedNode.find(record);
		if (allied) {
			if (member) {
				++already;
				continue;
			}
			if (Read(record + kFactionListOffset) == 0) {
				// an empty list: the inline first node takes ours
				auto* data = static_cast<UInt32*>(allocate(8));
				if (data == nullptr) {
					break;
				}
				data[0] = faction;
				data[1] = 0;
				*reinterpret_cast<UInt32*>(record + kFactionListOffset) = reinterpret_cast<UInt32>(data);
				g_alliedNode[record] = record + kFactionListOffset;
				++joined;
				continue;
			}
			auto* data = static_cast<UInt32*>(allocate(8));
			auto* node = static_cast<UInt32*>(allocate(8));
			if (data == nullptr || node == nullptr || last == 0) {
				break;
			}
			data[0] = faction;
			data[1] = 0;  // rank 0
			node[0] = reinterpret_cast<UInt32>(data);
			node[1] = 0;
			*reinterpret_cast<UInt32*>(last + 4) = reinterpret_cast<UInt32>(node);
			g_alliedNode[record] = reinterpret_cast<UInt32>(node);
			++joined;
		} else if (ours != g_alliedNode.end()) {
			if (ours->second == record + kFactionListOffset) {
				*reinterpret_cast<UInt32*>(record + kFactionListOffset) = 0;
			} else if (before != 0) {
				*reinterpret_cast<UInt32*>(before + 4) = Read(ours->second + 4);
			}
			g_alliedNode.erase(ours);
			++left;
		}
	}
	log("Invaders: the walkers %s DaedraFaction (%08X) - %u records joined, %u left, %u members already",
	    allied ? "join" : "keep out of", kDaedraFaction, joined, left, already);
}

void ScaleWalkerHealth(const UInt32* walkerIds, UInt32 count, UInt32 percent, LogFn log) {
	static UInt32 s_ids[kWalkerCount] = {};
	static UInt32 s_health[kWalkerCount] = {};
	for (UInt32 i = 0; i < count && i < kWalkerCount; ++i) {
		const UInt32 walker = LookupForm(walkerIds[i]);
		if (!LooksLikeObject(walker) || Read8(walker + kFormTypeOffset) != kFormTypeCreature) {
			continue;
		}
		UInt32* const health = reinterpret_cast<UInt32*>(walker + kActorBaseHealthOffset);
		if (s_ids[i] != walkerIds[i]) {
			s_ids[i] = walkerIds[i];
			s_health[i] = *health;
		}
		*health = ScaledHealth(s_health[i], percent);
		log("Walkers: %08X health %u (%u%% of %u)", walkerIds[i], *health, percent, s_health[i]);
	}
}

namespace {

// BSStringT_Set (thiscall: the string, the chars, the least buffer; xOBSE
// GameTypes.cpp Replace_MinBufLen) - the engine's own way to give a
// TESFullName new chars, its heap and all.
constexpr UInt32 kStringSet = 0x004028D0;

void SetFullName(UInt32 form, const char* name) {
	using SetFn = void(__fastcall*)(void* string, void* edx, const char* chars, UInt32 least);
	reinterpret_cast<SetFn>(kStringSet)(reinterpret_cast<void*>(form + kFullNameOffset), nullptr, name, 0);
}

const char* FullNameOf(UInt32 form) {
	const UInt32 chars = Read(form + kFullNameOffset);
	return LooksLikeObject(chars) ? reinterpret_cast<const char*>(chars) : nullptr;
}

}  // namespace

void RenameZombies(const UInt32* walkerIds, UInt32 count, UInt32 espBase, const RiseSettings& settings, LogFn log) {
	const char* const name = settings.renameToWalker ? settings.walkerName : "Zombie";
	for (UInt32 i = 0; i < count; ++i) {
		const UInt32 walker = LookupForm(walkerIds[i]);
		if (LooksLikeObject(walker) && Read8(walker + kFormTypeOffset) == kFormTypeCreature) {
			SetFullName(walker, name);
		}
	}
	const UInt32 first = count > 0 ? LookupForm(walkerIds[0]) : 0;
	const char* reads = LooksLikeObject(first) ? FullNameOf(first) : nullptr;
	log("Walkers: the %u walker records named \"%s\" (the first reads \"%s\")", count, name,
	    reads != nullptr ? reads : "(none)");
	if (!settings.renameVanilla || !settings.renameToWalker) {
		return;
	}
	UInt32 renamed = 0;
	const UInt32 buckets = Read(kFormTable + kMapBucketsOffset);
	const UInt32 bucketCount = Read(kFormTable + kMapBucketCountOffset);
	for (UInt32 b = 0; LooksLikeObject(buckets) && b < bucketCount; ++b) {
		for (UInt32 entry = Read(buckets + b * 4); LooksLikeObject(entry); entry = Read(entry)) {
			const UInt32 form = Read(entry + kMapEntryFormOffset);
			if (!LooksLikeObject(form) || Read8(form + kFormTypeOffset) != kFormTypeCreature ||
			    IsEspForm(espBase, FormIdOf(form))) {
				continue;
			}
			const char* before = FullNameOf(form);
			char after[128];
			if (!RenamedZombie(before, settings.walkerName, after, sizeof(after))) {
				continue;
			}
			char was[128];
			std::snprintf(was, sizeof(was), "%s", before);
			SetFullName(form, after);
			if (renamed < 12) {
				const char* now = FullNameOf(form);
				log("Walkers: %08X \"%s\" now reads \"%s\" (RenameVanilla)", FormIdOf(form), was,
				    now != nullptr ? now : "(none)");
			}
			++renamed;
		}
	}
	log("Walkers: %u of the game's and the mods' zombie records renamed (RenameVanilla)", renamed);
}

void ScaleWalkerSpeed(const UInt32* walkerIds, UInt32 count, float multiplier, UInt32 randomPercent, LogFn log) {
	static UInt32 s_ids[kWalkerCount] = {};
	static UInt8 s_speed[kWalkerCount] = {};
	for (UInt32 i = 0; i < count && i < kWalkerCount; ++i) {
		const UInt32 walker = LookupForm(walkerIds[i]);
		if (!LooksLikeObject(walker) || Read8(walker + kFormTypeOffset) != kFormTypeCreature) {
			continue;
		}
		UInt8* const speed = reinterpret_cast<UInt8*>(walker + kActorBaseSpeedOffset);
		if (s_ids[i] != walkerIds[i]) {
			s_ids[i] = walkerIds[i];
			s_speed[i] = *speed;
		}
		const UInt32 slot = SpeedSlot(i, count);
		*speed = WalkerSpeed(s_speed[i], multiplier, randomPercent, slot, count);
		log("Walkers: %08X speed %u (x%.2f, %u%% spread, slot %u of %u, the record's %u)", walkerIds[i], *speed,
		    multiplier, randomPercent, slot + 1, count, s_speed[i]);
	}
}

}  // namespace walkers
