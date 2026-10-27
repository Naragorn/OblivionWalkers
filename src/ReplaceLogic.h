#pragma once

// The spawn replacement's decisions, process-free (docs/walkers-spec.md
// 4.6, 4.5): which creature of a leveled list becomes a walker, the INI line
// that names it, how many walkers one spawn gives, and the walkers' health.
// Replace.cpp reads the engine and calls in here; the tests run every flow.

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "RiseLogic.h"
#include "Types.h"

namespace walkers {

// [Replace] in OblivionWalkers.ini.
struct ReplaceSettings {
	bool enabled = true;
	// Walkers per replaced spawn, spread over the ESP's list (SpreadCount).
	UInt32 minCount = 1;
	UInt32 maxCount = 10;
	// [Replace] ExtraPercent (4.13): walker entries added beside a list's own,
	// so this share of its picks becomes walkers (ExtraEntries). 0: none.
	UInt32 extraPercent = 0;
};

// TESCreature::type (xOBSE GameForms.h, at +0x104 in 1.2).
inline constexpr UInt8 kCreatureTypeCreature = 0;
inline constexpr UInt8 kCreatureTypeDaedra = 1;
inline constexpr UInt8 kCreatureTypeUndead = 2;
// TESActorBaseData flags of a creature (xOBSE GameForms.h).
inline constexpr UInt32 kCreatureSwims = 0x10;
inline constexpr UInt32 kCreatureFlies = 0x20;
inline constexpr UInt32 kCreatureWalks = 0x40;
// An animal this timid never starts a fight: Oblivion.esm's deer read
// confidence 0, the wild horses 10, the sheep 5 and the ram 25; the wolves
// 50 and up, the dogs and boars above 25 too (tools/esm-spawns.py's records,
// 2026-10-24).
inline constexpr UInt8 kHarmlessConfidenceMost = 25;

// A creature as Replace.cpp read it off its base record.
struct CreatureFacts {
	UInt8 type = kCreatureTypeCreature;
	UInt8 confidence = 0;
	UInt32 flags = 0;
	bool scripted = false;
	bool ours = false;    // a record of OblivionWalkers.esp
	bool person = false;  // an NPC_ record, no creature
};

// Nadi, 2026-10-24: every creature and person a leveled list can spawn has
// its line; harmless animals and all people start at 0. A creature with a
// script stays too: a quest or the arena may count on it; one that only
// swims would stand at the bottom of a lake.
enum class CreatureKind : UInt8 { Replace, HarmlessAnimal, SwimsOnly, Scripted, Person, Walker };

inline CreatureKind ClassifyCreature(const CreatureFacts& facts) {
	if (facts.ours) {
		return CreatureKind::Walker;
	}
	if (facts.person) {
		return CreatureKind::Person;
	}
	if (facts.scripted) {
		return CreatureKind::Scripted;
	}
	if ((facts.flags & kCreatureSwims) != 0 && (facts.flags & (kCreatureWalks | kCreatureFlies)) == 0) {
		return CreatureKind::SwimsOnly;
	}
	if (facts.type == kCreatureTypeCreature && facts.confidence <= kHarmlessConfidenceMost) {
		return CreatureKind::HarmlessAnimal;
	}
	return CreatureKind::Replace;
}

inline bool DefaultOn(CreatureKind kind) { return kind == CreatureKind::Replace; }

// The note behind a creature's INI key, so the file says why a line is 0.
inline const char* KindNote(CreatureKind kind) {
	switch (kind) {
		case CreatureKind::HarmlessAnimal:
			return " harmless animal";
		case CreatureKind::SwimsOnly:
			return " swims only";
		case CreatureKind::Scripted:
			return " has a script";
		case CreatureKind::Person:
			return " person";
		default:
			return "";
	}
}

// The facts a creature's log line names. A person's record has no creature
// type: what Replace.cpp reads at that offset of a TESNPC is no field of it.
inline void FormatFacts(char* out, UInt32 capacity, const CreatureFacts& facts) {
	if (facts.person) {
		std::snprintf(out, capacity, "confidence %u, flags %08X", facts.confidence, facts.flags);
	} else {
		std::snprintf(out, capacity, "type %u, confidence %u, flags %08X", facts.type, facts.confidence, facts.flags);
	}
}

// "<name> [<mod> <local ID>]<note>": the name to read, the mod that made
// the record and its ID under that mod (the load index changes with the
// load order, the local ID does not). The INI's own signs in a name become
// spaces; a nameless creature is "Creature".
inline void FormatReplaceKey(char* out, UInt32 capacity, const char* name, const char* mod, UInt32 formId,
                             CreatureKind kind) {
	char clean[96];
	UInt32 length = 0;
	if (name != nullptr) {
		for (; name[length] != '\0' && length + 1 < sizeof(clean); ++length) {
			const char c = name[length];
			clean[length] = (c == '=' || c == ';' || c == '[' || c == ']') ? ' ' : c;
		}
	}
	clean[length] = '\0';
	std::snprintf(out, capacity, "%s [%s %06X]%s", length == 0 ? "Creature" : clean, mod, formId & 0x00FFFFFFu,
	              KindNote(kind));
}

// The mod and the local ID out of a key FormatReplaceKey wrote; false for
// anything else (the mod name too long for `mod` included - never cut).
inline bool ParseReplaceKey(const char* key, char* mod, UInt32 modCapacity, UInt32* localId) {
	const char* open = std::strrchr(key, '[');
	if (open == nullptr) {
		return false;
	}
	const char* close = std::strchr(open, ']');
	const char* space = nullptr;
	for (const char* p = open + 1; close != nullptr && p < close; ++p) {
		if (*p == ' ') {
			space = p;
		}
	}
	if (close == nullptr || space == nullptr || space == open + 1) {
		return false;
	}
	const UInt32 digits = static_cast<UInt32>(close - space - 1);
	if (digits == 0 || digits > 6) {
		return false;
	}
	UInt32 value = 0;
	for (const char* p = space + 1; p < close; ++p) {
		const char c = *p;
		UInt32 digit = 0;
		if (c >= '0' && c <= '9') {
			digit = static_cast<UInt32>(c - '0');
		} else if (c >= 'A' && c <= 'F') {
			digit = static_cast<UInt32>(c - 'A' + 10);
		} else if (c >= 'a' && c <= 'f') {
			digit = static_cast<UInt32>(c - 'a' + 10);
		} else {
			return false;
		}
		value = value * 16 + digit;
	}
	const UInt32 modLength = static_cast<UInt32>(space - open - 1);
	if (modLength + 1 > modCapacity) {
		return false;
	}
	std::memcpy(mod, open + 1, modLength);
	mod[modLength] = '\0';
	*localId = value;
	return true;
}

// The switch of one creature in a section as GetPrivateProfileSection hands
// it over (key=value lines, each ended, the last one twice): 1 on, 0 off
// (a value that is no number reads 0), -1 when no line names it yet.
inline int FindReplaceSwitch(const char* section, const char* mod, UInt32 localId) {
	for (const char* line = section; *line != '\0'; line += std::strlen(line) + 1) {
		const char* equals = std::strchr(line, '=');
		if (equals == nullptr) {
			continue;
		}
		char key[256];
		const UInt32 keyLength = static_cast<UInt32>(equals - line);
		if (keyLength + 1 > sizeof(key)) {
			continue;
		}
		std::memcpy(key, line, keyLength);
		key[keyLength] = '\0';
		char lineMod[160];
		UInt32 lineId = 0;
		if (!ParseReplaceKey(key, lineMod, sizeof(lineMod), &lineId) || lineId != (localId & 0x00FFFFFFu) ||
		    !SameModName(lineMod, mod)) {
			continue;
		}
		return std::atoi(equals + 1) != 0 ? 1 : 0;
	}
	return -1;
}

// "key=value" behind the lines in `buffer` (`used` bytes of them), the
// closing end kept; false with nothing changed when it does not fit.
inline bool AppendSectionLine(char* buffer, UInt32 capacity, UInt32* used, const char* key, int value) {
	char line[288];
	const int length = std::snprintf(line, sizeof(line), "%s=%d", key, value);
	if (length < 0 || static_cast<UInt32>(length) + 1 >= sizeof(line) ||
	    *used + static_cast<UInt32>(length) + 2 > capacity) {
		return false;
	}
	std::memcpy(buffer + *used, line, static_cast<UInt32>(length) + 1);
	*used += static_cast<UInt32>(length) + 1;
	buffer[*used] = '\0';
	return true;
}

// One line of an INI file's text, [begin, end) without its line feed; the
// blanks and a carriage return around it do not count.
inline void TrimLine(const char* text, UInt32* begin, UInt32* end) {
	while (*begin < *end && (text[*begin] == ' ' || text[*begin] == '\t')) {
		++*begin;
	}
	while (*end > *begin && (text[*end - 1] == ' ' || text[*end - 1] == '\t' || text[*end - 1] == '\r')) {
		--*end;
	}
}

// "[section]", the case aside - as the profile API matches a section.
inline bool IsSectionHeader(const char* text, UInt32 begin, UInt32 end, const char* section) {
	const UInt32 length = static_cast<UInt32>(std::strlen(section));
	if (end - begin != length + 2 || text[begin] != '[' || text[end - 1] != ']') {
		return false;
	}
	for (UInt32 i = 0; i < length; ++i) {
		const char a = text[begin + 1 + i];
		const char b = section[i];
		if ((a >= 'A' && a <= 'Z' ? a + 32 : a) != (b >= 'A' && b <= 'Z' ? b + 32 : b)) {
			return false;
		}
	}
	return true;
}

// The INI file's text with `lines` (each ended, the last one twice, as
// AppendSectionLine writes them) put into [section] behind its last
// key=value line - or behind the comments right under its header when it has
// none - so the blank lines, the comments and the other sections stay as
// they were (WritePrivateProfileSection rewrote the whole file under Wine,
// blank lines dropped, a comment with "=" joined, 2026-10-24). No such
// section: it is added at the file's end. The file's own line end, a
// carriage return and line feed when it has none. Returns the bytes written
// to `out`; 0 when they do not fit, nothing half written that counts.
inline UInt32 InsertSectionLines(const char* text, UInt32 length, const char* section, const char* lines, char* out,
                                 UInt32 capacity) {
	bool feedsOnly = false;
	for (UInt32 i = 0; i < length; ++i) {
		if (text[i] == '\n') {
			feedsOnly = i == 0 || text[i - 1] != '\r';
			break;
		}
	}
	const char* newline = feedsOnly ? "\n" : "\r\n";

	bool inSection = false;
	bool keySeen = false;
	bool blankSeen = false;
	UInt32 insertAt = length;
	for (UInt32 begin = 0; begin < length;) {
		UInt32 end = begin;
		while (end < length && text[end] != '\n') {
			++end;
		}
		const UInt32 next = end < length ? end + 1 : length;
		UInt32 from = begin;
		UInt32 to = end;
		TrimLine(text, &from, &to);
		if (!inSection) {
			if (IsSectionHeader(text, from, to, section)) {
				inSection = true;
				insertAt = next;
			}
		} else if (from < to && text[from] == '[') {
			break;
		} else if (from == to) {
			blankSeen = true;
		} else if (text[from] == ';' || text[from] == '#') {
			if (!keySeen && !blankSeen) {
				insertAt = next;
			}
		} else {
			keySeen = true;
			insertAt = next;
		}
		begin = next;
	}

	UInt32 used = 0;
	bool fits = true;
	const auto put = [&](const char* bytes, UInt32 count) {
		if (!fits || used + count > capacity) {
			fits = false;
			return;
		}
		std::memcpy(out + used, bytes, count);
		used += count;
	};
	put(text, insertAt);
	if (insertAt == length && length > 0 && text[length - 1] != '\n' && lines[0] != '\0') {
		put(newline, static_cast<UInt32>(std::strlen(newline)));
	}
	if (!inSection && lines[0] != '\0') {
		put("[", 1);
		put(section, static_cast<UInt32>(std::strlen(section)));
		put("]", 1);
		put(newline, static_cast<UInt32>(std::strlen(newline)));
	}
	for (const char* line = lines; *line != '\0'; line += std::strlen(line) + 1) {
		put(line, static_cast<UInt32>(std::strlen(line)));
		put(newline, static_cast<UInt32>(std::strlen(newline)));
	}
	put(text + insertAt, length - insertAt);
	return fits ? used : 0;
}

// The ESP's WalkersZombieVariant (tools/esp-write.py) holds kVariantSlots
// entries, each the any-walker list at a count the plugin writes from
// MinCount and MaxCount, so the engine's own pick among them gives the
// spread. Ten slots: MinCount=1, MaxCount=10 gives every count from 1 to 10
// once. The any-walker list carries LVLF bit 2 ("calculate for each item in
// count"): TESLeveledList's CalcItemForContainer (0x0046CDE0, read with REA
// 2026-10-27) draws it anew for each of the count, so ten walkers at once
// are ten looks - without it one draw was copied count times (Nadi,
// 2026-10-27; spawn-mixed.txt 14:41 against 15:37).
inline constexpr UInt32 kVariantSlots = 10;
inline constexpr UInt32 kVariantEntriesMost = kVariantSlots;
inline constexpr UInt8 kListCalcEach = 0x02;
inline constexpr UInt32 kAnyWalkerLocalId = 0x94A;
inline constexpr UInt32 kAnyWalkerEntriesMost = 2 * kWalkerCount;

// A walker's entries in the any-walker list: two, a headless one one - half
// as often among the spawns (Nadi, 2026-10-27: "ja" to the headless ones
// less often; 25 of the 64 looks have no head).
inline UInt32 AnyWalkerTimes(bool headless) { return headless ? 1u : 2u; }
inline constexpr UInt32 kWalkersPerSpawnMost = 10;

inline UInt32 SpreadCount(UInt32 minCount, UInt32 maxCount, UInt32 slot, UInt32 slots) {
	UInt32 low = minCount < 1 ? 1 : minCount;
	low = low > kWalkersPerSpawnMost ? kWalkersPerSpawnMost : low;
	UInt32 high = maxCount < low ? low : maxCount;
	high = high > kWalkersPerSpawnMost ? kWalkersPerSpawnMost : high;
	if (slots <= 1) {
		return low;
	}
	if (slot >= slots - 1) {
		return high;
	}
	return low + ((high - low) * slot * 2 + (slots - 1)) / ((slots - 1) * 2);
}

// [Replace] ExtraPercent (4.13): how many entries pointing at the walkers
// go beside the `groupSize` entries one level of a list holds, so that
// `percent` of the picks at that level are walkers. The engine picks alike
// among the entries of a window of levels, the highest at or under the
// player's down to iLevCreaLevelDifferenceMax below it (xOBSE GameForms.cpp
// CalcElement's mirror; one in numMatches each), so a share kept per level
// holds for any window. That is
// groupSize * p / (100 - p) entries; the fraction left is one more entry for
// that share of the lists, by the list's own draw (ExtraDraw, 0..999), so
// the whole of the game's lists comes out at the percent asked. Up to 90
// percent: a list keeps a tenth of its own picks. Never more than
// kExtraEntriesMost per level.
inline constexpr UInt32 kExtraPercentMost = 90;
inline constexpr UInt32 kExtraEntriesMost = 64;

inline UInt32 ExtraEntries(UInt32 groupSize, UInt32 percent, UInt32 draw) {
	const UInt32 p = percent > kExtraPercentMost ? kExtraPercentMost : percent;
	if (p == 0 || groupSize == 0) {
		return 0;
	}
	const UInt32 milli = groupSize * p * 1000 / (100 - p);
	const UInt32 entries = milli / 1000 + (draw < milli % 1000 ? 1 : 0);
	return entries > kExtraEntriesMost ? kExtraEntriesMost : entries;
}

// A list's draw for one of its levels: the same every session (a dungeon
// keeps its share), another from list to list.
inline UInt32 ExtraDraw(UInt32 listFormId, UInt32 level) {
	UInt32 h = listFormId * 0x9E3779B1u ^ (level + 0x7F4A7C15u);
	h ^= h >> 15;
	h *= 0x2C1B3C6Du;
	h ^= h >> 12;
	return h % 1000;
}

// [Zombies] RenameVanilla (4.2): `name` with each "Zombie" standing as a
// word of its own (Zombie, Headless Zombie, Dread Zombie) given `newName`;
// a word run into others (the test records' ZombieMixnMatch01) and small
// letters stay. False with `out` unspecified when nothing changes or the new
// name does not fit.
inline bool RenamedZombie(const char* name, const char* newName, char* out, UInt32 capacity) {
	if (name == nullptr || newName == nullptr || newName[0] == '\0' || capacity == 0) {
		return false;
	}
	static const char kWord[] = "Zombie";
	const UInt32 wordLength = sizeof(kWord) - 1;
	const UInt32 newLength = static_cast<UInt32>(std::strlen(newName));
	const auto letter = [](char c) { return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'); };
	UInt32 used = 0;
	bool changed = false;
	for (const char* p = name; *p != '\0';) {
		const bool word = std::strncmp(p, kWord, wordLength) == 0 && (p == name || !letter(p[-1])) &&
		                  !letter(p[wordLength]);
		const char* piece = word ? newName : p;
		const UInt32 length = word ? newLength : 1;
		if (used + length + 1 > capacity) {
			return false;
		}
		std::memcpy(out + used, piece, length);
		used += length;
		p += word ? wordLength : 1;
		changed = changed || word;
	}
	out[used] = '\0';
	return changed;
}

// [Zombies] HealthPercent: 1 to 100 percent of the record's health, never
// below one point; a record without health keeps none.
inline UInt32 ScaledHealth(UInt32 health, UInt32 percent) {
	if (health == 0) {
		return 0;
	}
	const UInt32 p = percent < 1 ? 1 : (percent > 100 ? 100 : percent);
	const UInt32 scaled = health * p / 100;
	return scaled < 1 ? 1 : scaled;
}

// [Zombies] SpeedMultiplier and SpeedRandomPercent (4.4): the Speed of the
// walker record `slot` of `slots`, from the record's own. The records spread
// evenly from RandomPercent under to RandomPercent over the multiplied pace,
// so the variant list's walkers and the risen (a record drawn at random)
// shamble at as many paces as there are records. A creature walks at
// fMoveCreatureWalkMin + (Max - Min) * Speed / 100 units a second (5 and
// 300 by default; the CS wiki's Movement Game Settings) - Speed 12, the
// zombies', 40. On the records, not
// per reference: SetActorValue would write the base record anyway (the CS
// wiki), and ModActorValue on a reference stays in the save for good. 1 to
// 255, what the record's byte holds; a record of 0 stays so.
// The pace slot of walker record `i` of `count`: spread over the records
// (i times 37, a prime, modulo the count) - they come in families, the
// vanilla twenty, then MOO's by race (WalkerLooks.h), and each family should
// shamble at slow and fast paces alike. A count 37 divides: the record's own
// place.
inline UInt32 SpeedSlot(UInt32 i, UInt32 count) {
	if (count == 0) {
		return 0;
	}
	return count % 37 == 0 ? i % count : (i * 37) % count;
}

inline UInt8 WalkerSpeed(UInt8 base, float multiplier, UInt32 randomPercent, UInt32 slot, UInt32 slots) {
	if (base == 0) {
		return 0;
	}
	const float m = multiplier > 0.0f ? multiplier : 1.0f;
	const float spread = static_cast<float>(randomPercent > 100 ? 100 : randomPercent) / 100.0f;
	float where = 0.0f;
	if (slots > 1) {
		const UInt32 s = slot >= slots ? slots - 1 : slot;
		where = static_cast<float>(s * 2) / static_cast<float>(slots - 1) - 1.0f;
	}
	const float speed = static_cast<float>(base) * m * (1.0f + spread * where) + 0.5f;
	return speed < 1.0f ? 1 : (speed >= 255.0f ? 255 : static_cast<UInt8>(speed));
}

}  // namespace walkers
