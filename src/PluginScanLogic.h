#pragma once

// The boss guard's reading of the plugin files, process-free
// (docs/walkers-spec.md 4.6c; Nadi, 2026-11-16: "ja A bauen"). The game
// keeps no editor IDs of leveled lists, creatures or people in memory, so the
// plugin reads them off the loaded files once: the top groups LVLC, CREA and
// NPC_ of each, every other group passed over by its size.
//
// Read off the files of Nadi's Linux game (2026-11-16): leveled lists with
// "Boss" in their editor ID - Oblivion.esm 59 of 682, MOO 325 of 1894,
// Ascension 13 of 271 (all overrides of the game's; its 28 new lists are
// packs, "...Group100", and it adds no boss of its own). MOO names its bosses
// also Champion (22 lists), Leader (8) and Lord (4: the Daedric and Xivilai
// lords); its "Hard" (23) are harder variants of common foes, "High" (28)
// the High Elves and the Highlands - not kept. Read natively off the same
// files the same day (this header's code): Oblivion.esm 64 lists and 22
// creatures kept, MOO 359 and 4, Ascension 13 and 0 - and every NPC_ record
// of all three is compressed (2482, 2605, 126 compressed records): people
// are kept through their boss lists only. Creatures and people only by
// "Boss": a Minotaur Lord or a Goblin Warlord is a common foe of the higher
// levels, a "BanditBossMale1" or "SECreatureAtronachFlesh5Boss" is not.

#include <cstring>

#include "Types.h"

namespace walkers {

struct BossGuard {
	bool keepBosses = true;                                // [Replace] KeepBosses
	char listWords[128] = "Boss,Champion,Leader,Lord,Chief";  // [Replace] BossListWords
	char actorWords[64] = "Boss";                          // [Replace] BossActorWords
};

inline char LowerAscii(char c) { return c >= 'A' && c <= 'Z' ? static_cast<char>(c + 32) : c; }

// Whether `name` holds one of the comma-separated `words`, in any case.
inline bool HasAnyWord(const char* name, const char* words) {
	if (name == nullptr || words == nullptr || name[0] == '\0') {
		return false;
	}
	const UInt32 nameLength = static_cast<UInt32>(std::strlen(name));
	const char* w = words;
	while (*w != '\0') {
		while (*w == ' ' || *w == ',') {
			++w;
		}
		const char* end = w;
		while (*end != '\0' && *end != ',') {
			++end;
		}
		const char* last = end;
		while (last > w && last[-1] == ' ') {
			--last;
		}
		const UInt32 length = static_cast<UInt32>(last - w);
		for (UInt32 at = 0; length > 0 && at + length <= nameLength; ++at) {
			UInt32 k = 0;
			while (k < length && LowerAscii(name[at + k]) == LowerAscii(w[k])) {
				++k;
			}
			if (k == length) {
				return true;
			}
		}
		w = end;
	}
	return false;
}

// A list kept from the replacement: the INI's [ReplaceKeepLists] switch
// decides (1 kept, 0 never; FindReplaceSwitch's -1 none), else its name.
inline bool KeepList(const BossGuard& g, const char* editorId, int iniSwitch) {
	if (iniSwitch >= 0) {
		return iniSwitch != 0;
	}
	return g.keepBosses && HasAnyWord(editorId, g.listWords);
}

// A creature or person kept from the replacement by its own name.
inline bool KeepActor(const BossGuard& g, const char* editorId) {
	return g.keepBosses && HasAnyWord(editorId, g.actorWords);
}

inline constexpr UInt32 kMastersMost = 32;

struct PluginMasters {
	char names[kMastersMost][96] = {};
	UInt32 count = 0;
};

inline UInt32 Le32(const UInt8* p) {
	return static_cast<UInt32>(p[0]) | (static_cast<UInt32>(p[1]) << 8) | (static_cast<UInt32>(p[2]) << 16) |
	       (static_cast<UInt32>(p[3]) << 24);
}
inline UInt16 Le16(const UInt8* p) { return static_cast<UInt16>(p[0] | (p[1] << 8)); }

// The MAST subrecords of a TES4 record's data, in order. False only when
// `data` is null.
inline bool ReadMasters(const UInt8* data, UInt32 size, PluginMasters& out) {
	if (data == nullptr) {
		return false;
	}
	for (UInt32 at = 0; at + 6 <= size;) {
		const UInt16 length = Le16(data + at + 4);
		if (at + 6 + length > size) {
			break;
		}
		if (std::memcmp(data + at, "MAST", 4) == 0 && out.count < kMastersMost) {
			UInt32 n = length < sizeof(out.names[0]) - 1 ? length : static_cast<UInt32>(sizeof(out.names[0]) - 1);
			std::memcpy(out.names[out.count], data + at + 6, n);
			out.names[out.count][n] = '\0';
			++out.count;
		}
		at += 6 + length;
	}
	return true;
}

// A form ID as the file writes it (its top byte the index into its masters,
// or past them its own) as the game knows it. `masterIndex[i]` the game's
// load index of master i (0xFF: not loaded); 0 when the master is missing.
inline UInt32 ResolveFormId(UInt32 fileFormId, const UInt8* masterIndex, UInt32 masterCount, UInt8 ownIndex) {
	const UInt32 top = fileFormId >> 24;
	UInt32 index = ownIndex;
	if (top < masterCount) {
		if (masterIndex[top] == 0xFF) {
			return 0;
		}
		index = masterIndex[top];
	}
	return (index << 24) | (fileFormId & 0x00FFFFFF);
}

inline constexpr UInt32 kRecordHeaderBytes = 20;  // type, size, flags, form ID, version control
inline constexpr UInt32 kRecordCompressed = 0x00040000;

// Each record of a top group's contents: its type, its file form ID and its
// editor ID (empty without one). Compressed records are counted and passed
// over (no zlib in the plugin; Oblivion.esm, MOO and Ascension store their
// lists plain - the counts are logged). Nested groups are passed over.
template <typename Fn>
inline void WalkGroupRecords(const UInt8* data, UInt32 size, UInt32& compressed, Fn&& fn) {
	for (UInt32 at = 0; at + kRecordHeaderBytes <= size;) {
		const UInt32 length = Le32(data + at + 4);
		if (std::memcmp(data + at, "GRUP", 4) == 0) {
			if (length < kRecordHeaderBytes) {
				return;
			}
			at += length;  // a group's size counts its header
			continue;
		}
		if (at + kRecordHeaderBytes + length > size) {
			return;
		}
		const UInt32 flags = Le32(data + at + 8);
		const UInt32 formId = Le32(data + at + 12);
		char type[5] = {static_cast<char>(data[at]), static_cast<char>(data[at + 1]), static_cast<char>(data[at + 2]),
		                static_cast<char>(data[at + 3]), '\0'};
		if ((flags & kRecordCompressed) != 0) {
			++compressed;
		} else {
			const UInt8* body = data + at + kRecordHeaderBytes;
			char edid[128] = "";
			if (length >= 6 && std::memcmp(body, "EDID", 4) == 0) {
				const UInt16 n = Le16(body + 4);
				const UInt32 take = n < sizeof(edid) - 1 ? n : static_cast<UInt32>(sizeof(edid) - 1);
				if (6 + take <= length) {
					std::memcpy(edid, body + 6, take);
					edid[take] = '\0';
				}
			}
			fn(type, formId, edid);
		}
		at += kRecordHeaderBytes + length;
	}
}

}  // namespace walkers
