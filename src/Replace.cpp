#include "Replace.h"

#include <windows.h>

#include <cstdio>
#include <unordered_map>
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
constexpr UInt32 kListDataFormOffset = 0x04;
constexpr UInt32 kListDataCountOffset = 0x08;
// TESCreature in 1.2 (xOBSE GameForms.h): TESActorBaseData flags +0x28,
// TESAIForm's AISettings at +0x6C (aggression, confidence, ...),
// TESHealthForm's health +0x84, TESFullName's string +0xA4,
// TESScriptableForm's script +0xC8, the type +0x104. The name and the
// script are TESActorBase's, at the same offsets in a TESNPC.
constexpr UInt32 kActorBaseFlagsOffset = 0x28;
constexpr UInt32 kCreatureConfidenceOffset = 0x6D;
constexpr UInt32 kActorBaseHealthOffset = 0x84;
constexpr UInt32 kFullNameOffset = 0xA4;
constexpr UInt32 kScriptOffset = 0xC8;
constexpr UInt32 kCreatureTypeOffset = 0x104;
constexpr UInt32 kVariantLocalId = 0x805;  // the ESP's WalkersZombieVariant
constexpr const char* kSection = "ReplaceCreatures";
constexpr UInt32 kSectionBytes = 1u << 19;

// A creature's decision, made once per session: on or off.
std::unordered_map<UInt32, bool> g_decided;

// The variant list's counts per walker slot (ReplaceLogic.h SpreadCount).
void WriteVariantCounts(UInt32 variant, const ReplaceSettings& settings, LogFn log) {
	UInt32 seen[16] = {};
	UInt32 times[16] = {};
	UInt32 kinds = 0;
	UInt32 entries = 0;
	char counts[128] = {};
	UInt32 written = 0;
	for (UInt32 node = variant + kLevCreatureListOffset; LooksLikeObject(node) && entries < 64;
	     node = Read(node + 4)) {
		const UInt32 data = Read(node);
		if (!LooksLikeObject(data)) {
			break;
		}
		const UInt32 form = Read(data + kListDataFormOffset);
		UInt32 slot = 0;
		UInt32 k = 0;
		for (; k < kinds && seen[k] != form; ++k) {
		}
		if (k == kinds && kinds < 16) {
			seen[kinds++] = form;
		}
		if (k < 16) {
			slot = times[k]++;
		}
		const UInt16 count = static_cast<UInt16>(SpreadCount(settings.minCount, settings.maxCount, slot, kVariantSlots));
		*reinterpret_cast<UInt16*>(data + kListDataCountOffset) = count;
		if (written + 4 < sizeof(counts)) {
			written += static_cast<UInt32>(std::snprintf(counts + written, sizeof(counts) - written, "%s%u",
			                                              entries == 0 ? "" : " ", count));
		}
		++entries;
	}
	log("Spawns: WalkersZombieVariant %08X - %u entries over %u walkers, counts %s", FormIdOf(variant), entries,
	    kinds, counts);
}

CreatureFacts FactsOf(UInt32 creature, UInt32 espIndex) {
	CreatureFacts facts;
	facts.type = Read8(creature + kCreatureTypeOffset);
	facts.confidence = Read8(creature + kCreatureConfidenceOffset);
	facts.flags = Read(creature + kActorBaseFlagsOffset);
	facts.scripted = Read(creature + kScriptOffset) != 0;
	facts.ours = (FormIdOf(creature) >> 24) == espIndex;
	facts.person = Read8(creature + kFormTypeOffset) == kFormTypeNpc;
	return facts;
}

// On or off for one creature: the INI's line, or a new one by its kind.
bool Decide(UInt32 creature, UInt32 espIndex, const char* section, std::vector<char>& added, UInt32& addedUsed,
            UInt32& newLines, LogFn log) {
	const auto known = g_decided.find(creature);
	if (known != g_decided.end()) {
		return known->second;
	}
	const UInt32 formId = FormIdOf(creature);
	const char* mod = ModNameAt(formId >> 24);
	if (mod == nullptr) {
		// A created record (FF) or one of no loaded mod: no line to name it.
		g_decided[creature] = false;
		return false;
	}
	const CreatureFacts facts = FactsOf(creature, espIndex);
	const CreatureKind kind = ClassifyCreature(facts);
	if (kind == CreatureKind::Walker) {
		g_decided[creature] = false;
		return false;
	}
	const UInt32 name = Read(creature + kFullNameOffset);
	char key[160];
	FormatReplaceKey(key, sizeof(key), LooksLikeObject(name) ? reinterpret_cast<const char*>(name) : nullptr, mod,
	                 formId, kind);
	int on = FindReplaceSwitch(section, mod, formId);
	const bool fresh = on < 0;
	if (fresh) {
		on = DefaultOn(kind) ? 1 : 0;
		if (AppendSectionLine(added.data(), static_cast<UInt32>(added.size()), &addedUsed, key, on)) {
			++newLines;
		}
	}
	log("Spawns: %s - type %u, confidence %u, flags %08X - %s%s", key, facts.type, facts.confidence, facts.flags,
	    on != 0 ? "walkers" : "kept", fresh ? " (new in the INI)" : "");
	g_decided[creature] = on != 0;
	return on != 0;
}

}  // namespace

UInt32 ReplaceSpawns(UInt32 espIndex, const ReplaceSettings& settings, const char* iniPath, LogFn log) {
	if (espIndex > 0xFF) {
		log("Spawns: no ESP, the lists as they are");
		return 0;
	}
	const UInt32 variant = LookupForm(EspFormId(espIndex, kVariantLocalId));
	if (!LooksLikeObject(variant) || Read8(variant + kFormTypeOffset) != kFormTypeLeveledCreature) {
		log("Spawns: %08X is no leveled creature list - the lists as they are", EspFormId(espIndex, kVariantLocalId));
		return 0;
	}
	static bool s_countsWritten = false;
	if (!s_countsWritten) {
		s_countsWritten = true;
		WriteVariantCounts(variant, settings, log);
	}
	std::vector<char> section(kSectionBytes, '\0');
	if (iniPath != nullptr && iniPath[0] != '\0') {
		const DWORD read = GetPrivateProfileSectionA(kSection, section.data(), kSectionBytes, iniPath);
		if (read >= kSectionBytes - 2) {
			log("Spawns: [%s] is longer than %u bytes - read as far as it went", kSection, kSectionBytes);
		}
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
	for (UInt32 b = 0; LooksLikeObject(buckets) && b < bucketCount; ++b) {
		for (UInt32 entry = Read(buckets + b * 4); LooksLikeObject(entry); entry = Read(entry)) {
			const UInt32 form = Read(entry + kMapEntryFormOffset);
			++forms;
			if (!LooksLikeObject(form) || form == variant || Read8(form + kFormTypeOffset) != kFormTypeLeveledCreature) {
				continue;
			}
			++lists;
			bool touched = false;
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
				// A creature or a person: Oblivion's leveled creature lists hold
				// both (LL1BanditBossLvl100 holds NPC_ records only).
				const UInt8 targetType = Read8(target + kFormTypeOffset);
				if (targetType != kFormTypeCreature && targetType != kFormTypeNpc) {
					continue;
				}
				if (Decide(target, espIndex, section.data(), added, addedUsed, newLines, log)) {
					*reinterpret_cast<UInt32*>(data + kListDataFormOffset) = variant;
					++replaced;
					touched = true;
				}
			}
			listsTouched += touched ? 1 : 0;
		}
	}
	UInt32 on = 0;
	for (const auto& decided : g_decided) {
		on += decided.second ? 1 : 0;
	}
	log("Spawns: %u forms, %u leveled creature lists - %u entries in %u lists now walkers; %u creatures decided, %u "
	    "of them walkers",
	    forms, lists, replaced, listsTouched, static_cast<UInt32>(g_decided.size()), on);
	if (newLines > 0 && iniPath != nullptr && iniPath[0] != '\0') {
		std::vector<char> merged(kSectionBytes * 2, '\0');
		UInt32 used = CopySectionLines(section.data(), merged.data(), static_cast<UInt32>(merged.size()));
		for (const char* line = added.data(); *line != '\0'; line += std::strlen(line) + 1) {
			const UInt32 length = static_cast<UInt32>(std::strlen(line)) + 1;
			if (used + length + 1 > merged.size()) {
				break;
			}
			std::memcpy(merged.data() + used, line, length);
			used += length;
			merged[used] = '\0';
		}
		const bool wrote = WritePrivateProfileSectionA(kSection, merged.data(), iniPath) != 0;
		log("Spawns: %u new creatures written to [%s] (%s)", newLines, kSection, wrote ? "written" : "the INI refused");
	}
	return replaced;
}

void ScaleWalkerHealth(const UInt32* walkerIds, UInt32 count, UInt32 percent, LogFn log) {
	static UInt32 s_ids[8] = {};
	static UInt32 s_health[8] = {};
	for (UInt32 i = 0; i < count && i < 8; ++i) {
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

}  // namespace walkers
