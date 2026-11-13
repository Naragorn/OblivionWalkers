#pragma once

// The refugee dungeons' pool, process-free (docs/walkers-spec.md 4.20.7).
// The candidates (DungeonCandidates.h, tools/dungeon-candidates.py --header)
// are the dungeons of Oblivion.esm with a door in Tamriel that no quest of
// Oblivion.esm, UOP, the DLCs or Knights uses. At the start the game's own
// load order is read too (Nadi, 2026-11-13: "passt A + B"):
//   A  a candidate one of whose cells holds something a loaded quest names
//      (its script, a stage's result script, a target - Replace.cpp
//      CollectQuestNames), whose cell a quest names itself, or whose door
//      outside or map marker a quest names, is out;
//   B  one the INI's [Dungeons] Skip names - by its name or by one of its
//      cells' form IDs in hex - is out, for what A cannot see (a quest that
//      sends people there by their AI package, a dialogue line's script).
// Skip is a list, so it stays in the INI only (AGENTS.md: lists are not
// LINK's).

#include <cctype>
#include <cstdlib>
#include <cstring>

#include "Types.h"

namespace walkers {

enum class DungeonKind : UInt8 { Cave, Ayleid, Fort, Mine };

inline const char* DungeonKindName(DungeonKind kind) {
	switch (kind) {
		case DungeonKind::Cave:
			return "cave";
		case DungeonKind::Ayleid:
			return "Ayleid ruin";
		case DungeonKind::Fort:
			return "fort";
		case DungeonKind::Mine:
			return "mine";
	}
	return "?";
}

inline constexpr UInt32 kDungeonCellsMost = 4;

struct DungeonCandidate {
	const char* name;  // the map marker's
	DungeonKind kind;
	UInt32 cells[kDungeonCellsMost];  // Oblivion.esm form IDs, the first the one the door outside leads into
	UInt8 cellCount;
	UInt32 door;    // the door outside, in Tamriel
	UInt32 marker;  // its map marker
	bool chosen;    // one of the chosen (roomy, few cells, spread - tools/dungeon-candidates.py)
	float x;        // the door outside, in Tamriel
	float y;
};

// What the loaded quests name (Replace.cpp CollectQuestNames): the cells
// named or holding a named reference, and the named references themselves -
// a quest that shows a dungeon's map marker or targets it (Better Cities,
// Quest of the Week: measured 2026-11-13) names no cell of it.
struct DungeonQuestNames {
	const UInt32* cells;
	UInt32 cellCount;
	const UInt32* refs;
	UInt32 refCount;
};

inline constexpr UInt32 kDungeonSkipMost = 32;
inline constexpr UInt32 kDungeonNameMost = 48;

struct DungeonSkips {
	char names[kDungeonSkipMost][kDungeonNameMost] = {};
	UInt32 count = 0;
};

// "Fort Aurus, lost boy cavern, 00002221": comma-separated, spaces about an
// entry trimmed, empty entries dropped, at most kDungeonSkipMost.
inline void ParseDungeonSkips(const char* text, DungeonSkips& out) {
	out.count = 0;
	const char* p = text != nullptr ? text : "";
	while (*p != '\0' && out.count < kDungeonSkipMost) {
		const char* end = std::strchr(p, ',');
		const char* stop = end != nullptr ? end : p + std::strlen(p);
		const char* a = p;
		const char* b = stop;
		while (a < b && std::isspace(static_cast<unsigned char>(*a))) {
			++a;
		}
		while (b > a && std::isspace(static_cast<unsigned char>(b[-1]))) {
			--b;
		}
		if (b > a) {
			size_t n = static_cast<size_t>(b - a);
			if (n > kDungeonNameMost - 1) {
				n = kDungeonNameMost - 1;
			}
			std::memcpy(out.names[out.count], a, n);
			out.names[out.count][n] = '\0';
			++out.count;
		}
		p = end != nullptr ? end + 1 : stop;
	}
}

inline bool SameName(const char* a, const char* b) {
	for (; *a != '\0' && *b != '\0'; ++a, ++b) {
		if (std::tolower(static_cast<unsigned char>(*a)) != std::tolower(static_cast<unsigned char>(*b))) {
			return false;
		}
	}
	return *a == *b;
}

inline bool SkipsName(const DungeonSkips& s, const char* name) {
	for (UInt32 i = 0; i < s.count; ++i) {
		if (SameName(s.names[i], name)) {
			return true;
		}
	}
	return false;
}

// An entry of hex digits only names a cell's form ID.
inline bool SkipsCell(const DungeonSkips& s, UInt32 cell) {
	for (UInt32 i = 0; i < s.count; ++i) {
		const char* t = s.names[i];
		bool hex = *t != '\0';
		for (const char* c = t; *c != '\0'; ++c) {
			hex = hex && std::isxdigit(static_cast<unsigned char>(*c)) != 0;
		}
		if (hex && static_cast<UInt32>(std::strtoul(t, nullptr, 16)) == cell) {
			return true;
		}
	}
	return false;
}

enum class DungeonVerdict : UInt8 { In, Quest, Skipped, NotChosen };

// The quest is the reason given when both hold: it is the one the player
// did not choose.
inline bool NamedIn(const UInt32* list, UInt32 count, UInt32 form) {
	for (UInt32 i = 0; form != 0 && i < count; ++i) {
		if (list[i] == form) {
			return true;
		}
	}
	return false;
}

inline DungeonVerdict DungeonOut(const DungeonCandidate& d, const DungeonQuestNames& quests,
                                 const DungeonSkips& skips) {
	for (UInt32 c = 0; c < d.cellCount && c < kDungeonCellsMost; ++c) {
		if (NamedIn(quests.cells, quests.cellCount, d.cells[c])) {
			return DungeonVerdict::Quest;
		}
	}
	if (NamedIn(quests.refs, quests.refCount, d.door) || NamedIn(quests.refs, quests.refCount, d.marker)) {
		return DungeonVerdict::Quest;
	}
	if (SkipsName(skips, d.name)) {
		return DungeonVerdict::Skipped;
	}
	for (UInt32 c = 0; c < d.cellCount && c < kDungeonCellsMost; ++c) {
		if (d.cells[c] != 0 && SkipsCell(skips, d.cells[c])) {
			return DungeonVerdict::Skipped;
		}
	}
	return DungeonVerdict::In;
}

struct DungeonPoolCount {
	UInt32 in = 0;
	UInt32 quest = 0;
	UInt32 skipped = 0;
	UInt32 notChosen = 0;
};

// [Dungeons] OnlyChosen (Nadi, 2026-11-13: the player chooses, all of them
// the default): 1 keeps the chosen ones only.
inline DungeonVerdict DungeonInPoolVerdict(const DungeonCandidate& d, const DungeonQuestNames& quests,
                                           const DungeonSkips& skips, bool onlyChosen) {
	const DungeonVerdict v = DungeonOut(d, quests, skips);
	return v == DungeonVerdict::In && onlyChosen && !d.chosen ? DungeonVerdict::NotChosen : v;
}

inline DungeonPoolCount MarkDungeonPool(const DungeonCandidate* all, UInt32 count, const DungeonQuestNames& quests,
                                        const DungeonSkips& skips, bool onlyChosen, bool* in) {
	DungeonPoolCount n;
	for (UInt32 i = 0; i < count; ++i) {
		const DungeonVerdict v = DungeonInPoolVerdict(all[i], quests, skips, onlyChosen);
		n.notChosen += v == DungeonVerdict::NotChosen ? 1 : 0;
		in[i] = v == DungeonVerdict::In;
		n.in += v == DungeonVerdict::In ? 1 : 0;
		n.quest += v == DungeonVerdict::Quest ? 1 : 0;
		n.skipped += v == DungeonVerdict::Skipped ? 1 : 0;
	}
	return n;
}

}  // namespace walkers
