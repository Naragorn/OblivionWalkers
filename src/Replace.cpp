#include "Replace.h"

#include <windows.h>

#include <cstdio>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "Engine.h"

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
void WriteVariantCounts(UInt32 variant, UInt32 anyWalker, const ReplaceSettings& settings, LogFn log) {
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
		const UInt16 count = static_cast<UInt16>(SpreadCount(settings.minCount, settings.maxCount, slot, slots));
		*reinterpret_cast<UInt16*>(data + kListDataCountOffset) = count;
		written += static_cast<UInt32>(
			std::snprintf(counts + written, sizeof(counts) - written, "%s%u", slot == 0 ? "" : " ", count));
		++slot;
	}
	const UInt8 flags = Read8(anyWalker + kLevCreatureFlagsOffset);
	log("Spawns: WalkersZombieVariant %08X - %u count slots, counts %s%s; the any-walker list's flags %02X (%s)",
	    FormIdOf(variant), slots, counts, other == 0 ? "" : " - some slots not the any-walker list (an older ESP)",
	    flags, (flags & kListCalcEach) != 0 ? "drawn anew for each walker" : "one draw copied - an older ESP");
}

CreatureFacts FactsOf(UInt32 creature, UInt32 espBase) {
	CreatureFacts facts;
	facts.type = Read8(creature + kCreatureTypeOffset);
	facts.confidence = Read8(creature + kCreatureConfidenceOffset);
	facts.flags = Read(creature + kActorBaseFlagsOffset);
	facts.scripted = Read(creature + kScriptOffset) != 0;
	facts.ours = IsEspForm(espBase, FormIdOf(creature));
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

}  // namespace

void RewriteVariantCounts(UInt32 espBase, const ReplaceSettings& settings, LogFn log) {
	if (espBase == kNoEsp) {
		return;
	}
	const UInt32 variant = LookupForm(EspFormId(espBase, kVariantLocalId));
	const UInt32 anyWalker = LookupForm(EspFormId(espBase, kAnyWalkerLocalId));
	if (LooksLikeObject(variant) && Read8(variant + kFormTypeOffset) == kFormTypeLeveledCreature &&
	    LooksLikeObject(anyWalker) && Read8(anyWalker + kFormTypeOffset) == kFormTypeLeveledCreature) {
		WriteVariantCounts(variant, anyWalker, settings, log);
	}
}

UInt32 ReplaceSpawns(UInt32 espBase, const ReplaceSettings& settings, const char* iniPath, bool mooLoaded,
                     LogFn log) {
	if (espBase == kNoEsp) {
		log("Spawns: no ESP, the lists as they are");
		return 0;
	}
	const UInt32 variant = LookupForm(EspFormId(espBase, kVariantLocalId));
	if (!LooksLikeObject(variant) || Read8(variant + kFormTypeOffset) != kFormTypeLeveledCreature) {
		log("Spawns: %08X is no leveled creature list - the lists as they are", EspFormId(espBase, kVariantLocalId));
		return 0;
	}
	static bool s_countsWritten = false;
	if (!s_countsWritten) {
		s_countsWritten = true;
		const UInt32 anyWalker = LookupForm(EspFormId(espBase, kAnyWalkerLocalId));
		if (LooksLikeObject(anyWalker) && Read8(anyWalker + kFormTypeOffset) == kFormTypeLeveledCreature) {
			TrimAnyWalker(anyWalker, mooLoaded, settings.headlessWalkers, log);
			WriteVariantCounts(variant, anyWalker, settings, log);
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
	for (UInt32 b = 0; LooksLikeObject(buckets) && b < bucketCount; ++b) {
		for (UInt32 entry = Read(buckets + b * 4); LooksLikeObject(entry); entry = Read(entry)) {
			const UInt32 form = Read(entry + kMapEntryFormOffset);
			++forms;
			if (!LooksLikeObject(form) || form == variant || Read8(form + kFormTypeOffset) != kFormTypeLeveledCreature) {
				continue;
			}
			++lists;
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
				if (target == variant) {
					// Ours from an earlier pass - or an extra entry (4.13), which
					// named nothing of its own and stays.
					const auto original = g_original.find(data);
					if (original == g_original.end()) {
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
				const bool on = ReplaceOn(settings, known.facts, known.category, known.line, known.exception);
				const UInt32 wanted = on ? variant : own;
				if (wanted != target) {
					*reinterpret_cast<UInt32*>(data + kListDataFormOffset) = wanted;
					++(on ? replaced : restored);
				}
				if (on) {
					++walkerEntries;
					touched = true;
				} else {
					++kept;
				}
			}
			listsTouched += touched ? 1 : 0;
			// Extra walkers only where something of the list's own stays: a
			// list all walkers has nothing to add to, one of lists only feeds
			// its nested lists, which get theirs.
			if (settings.extraPercent > 0 && kept > 0) {
				const UInt32 more = AddExtraEntries(form, variant, settings.extraPercent);
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
	if (settings.extraPercent > 0) {
		log("Spawns: ExtraPercent=%u - %u walker entries added beside the lists' own, in %u lists (%u lists had theirs "
		    "this session)",
		    settings.extraPercent, extraAdded, extraLists, static_cast<UInt32>(g_extraDone.size()));
	}
	if (newLines > 0 && iniPath != nullptr && iniPath[0] != '\0') {
		log("Spawns: %u new creatures written to [%s] (%s)", newLines, kSection,
		    AppendToIni(iniPath, added.data(), addedUsed + newLines + 64));
	}
	return replaced;
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
