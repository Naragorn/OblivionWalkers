#pragma once

// The walkers' news, process-free (docs/walkers-spec.md 4.9; Nadi,
// 2026-10-26): the Black Horse Courier's broadsheets in the inns, taverns and
// the Courier's office carry the latest from the outbreak - one of 18
// pre-written editions in the ESP (tools/esp-write.py): the outbreak in
// general, a horde seen heading for a city, a city whose gates the walkers
// attacked, or several cities at once.
//
// "Attacked" is what the plugin knows (Nadi's choice, 2026-10-26): walkers
// stood at a city's gate and its guards came out (GuardLogic.h), within the
// last RecentDays game days. Where a horde heads is known only for the
// hordes near the player (HordeLogic.h). Once there are real sieges (4.12,
// not built), they belong in the news too - noted in the spec.

#include "Bytes.h"
#include "Types.h"

#include <cstring>

namespace walkers {

inline constexpr UInt32 kNewsCities = 8;  // CityGates.h City::Count

struct NewsSettings {
	bool courier = true;      // [News] Courier
	float recentDays = 3.0f;  // [News] RecentDays: an attack this many game days old is still news
};

enum class EditionKind : UInt8 { General, Approaching, Attack, Several };

struct Edition {
	EditionKind kind = EditionKind::General;
	int city = -1;  // Approaching, Attack: which; -1 otherwise
};

// The edition's number - the order tools/esp-write.py writes the books in:
// general 0, approaching 1-8, attack 9-16, several 17.
inline constexpr UInt32 kEditionCount = 2 + 2 * kNewsCities;

inline UInt32 EditionIndex(const Edition& e) {
	switch (e.kind) {
		case EditionKind::Approaching:
			return 1 + static_cast<UInt32>(e.city);
		case EditionKind::Attack:
			return 1 + kNewsCities + static_cast<UInt32>(e.city);
		case EditionKind::Several:
			return 1 + 2 * kNewsCities;
		case EditionKind::General:
		default:
			return 0;
	}
}

struct NewsState {
	float lastAttackDays[kNewsCities] = {-1.0f, -1.0f, -1.0f, -1.0f, -1.0f, -1.0f, -1.0f, -1.0f};  // -1: never
};

// Walkers at `city`'s gate and its guards out, at game day `days`.
inline void NoteCityAttack(NewsState& news, UInt32 city, float days) {
	if (city < kNewsCities && days > news.lastAttackDays[city]) {
		news.lastAttackDays[city] = days;
	}
}

// The edition for game day `now`: several cities attacked lately, else the
// one attacked, else the city a horde near the player heads for
// (`headingCity`, -1 none), else the outbreak in general.
inline Edition ChooseEdition(const NewsState& news, float now, float recentDays, int headingCity) {
	UInt32 recent = 0;
	int attacked = -1;
	float latest = -1.0f;
	for (UInt32 c = 0; c < kNewsCities; ++c) {
		const float last = news.lastAttackDays[c];
		if (last >= 0.0f && now - last <= recentDays) {
			++recent;
			if (last > latest) {
				latest = last;
				attacked = static_cast<int>(c);
			}
		}
	}
	if (recent >= 2) {
		return Edition{EditionKind::Several, -1};
	}
	if (recent == 1) {
		return Edition{EditionKind::Attack, attacked};
	}
	if (headingCity >= 0 && headingCity < static_cast<int>(kNewsCities)) {
		return Edition{EditionKind::Approaching, headingCity};
	}
	return Edition{EditionKind::General, -1};
}

// The co-save record: type "NEWS", version 1 - the last attack's game day
// per city.
inline constexpr UInt32 kNewsRecordType = 0x4E455753;  // 'NEWS'
inline constexpr UInt32 kNewsRecordVersion = 1;
inline constexpr UInt32 kNewsImageBytes = 4 * kNewsCities;

inline UInt32 NewsImage(const NewsState& news, UInt8* out, UInt32 capacity) {
	if (capacity < kNewsImageBytes) {
		return 0;
	}
	for (UInt32 c = 0; c < kNewsCities; ++c) {
		UInt32 bits = 0;
		std::memcpy(&bits, &news.lastAttackDays[c], sizeof(bits));
		Put32(out + 4 * c, bits);
	}
	return kNewsImageBytes;
}

// `news` emptied first; false when cut short (nothing read then).
inline bool ReadNewsImage(NewsState& news, const UInt8* in, UInt32 length) {
	news = NewsState{};
	if (length < kNewsImageBytes) {
		return false;
	}
	for (UInt32 c = 0; c < kNewsCities; ++c) {
		const UInt32 bits = Get32(in + 4 * c);
		std::memcpy(&news.lastAttackDays[c], &bits, sizeof(bits));
	}
	return true;
}

}  // namespace walkers
