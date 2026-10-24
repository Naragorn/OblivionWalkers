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
	UInt32 maxCount = 3;
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
	bool ours = false;  // a record of OblivionWalkers.esp
};

// Nadi, 2026-10-24: for now only creatures - no people - and no harmless
// animals. A creature with a script stays too: a quest or the arena may
// count on it; one that only swims would stand at the bottom of a lake.
enum class CreatureKind : UInt8 { Replace, HarmlessAnimal, SwimsOnly, Scripted, Walker };

inline CreatureKind ClassifyCreature(const CreatureFacts& facts) {
	if (facts.ours) {
		return CreatureKind::Walker;
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
		default:
			return "";
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

// The lines of `section` into `out`, as many as fit whole, the closing end
// written; returns the bytes used before it.
inline UInt32 CopySectionLines(const char* section, char* out, UInt32 capacity) {
	UInt32 used = 0;
	out[0] = '\0';
	for (const char* line = section; *line != '\0'; line += std::strlen(line) + 1) {
		const UInt32 length = static_cast<UInt32>(std::strlen(line)) + 1;
		if (used + length + 1 > capacity) {
			break;
		}
		std::memcpy(out + used, line, length);
		used += length;
		out[used] = '\0';
	}
	return used;
}

// The ESP's WalkersZombieVariant holds every walker kVariantSlots times
// (tools/esp-write.py); the plugin writes each slot's count from MinCount
// and MaxCount, so the engine's own pick among the entries gives the spread.
inline constexpr UInt32 kVariantSlots = 3;
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

}  // namespace walkers
