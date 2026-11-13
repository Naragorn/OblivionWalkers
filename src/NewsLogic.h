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
	// [News] HeadingHoldDays: a horde seen heading for a city stays the news
	// this long after it was last seen - the Windows run of 2026-10-26 had
	// the sheets flip between the outbreak and the horde 7 times in minutes,
	// whenever the hordes near the player were out of reach for a moment.
	float headingHoldDays = 0.5f;
};

enum class EditionKind : UInt8 { General, Approaching, Attack, Several, Besieged, Lifted, CampSaved, CampLost, Refuges };

struct Edition {
	EditionKind kind = EditionKind::General;
	int city = -1;  // Approaching, Attack, Besieged, Lifted: which; -1 otherwise
};

// The edition's number - the order tools/esp-write.py writes the books in:
// general 0, approaching 1-8, attack 9-16, several 17, besieged 18-25 and
// lifted 26-33 (4.12), a refugee camp saved 34 and lost 35 (4.20.6).
inline constexpr UInt32 kEditionCount = 5 + 4 * kNewsCities;

inline UInt32 EditionIndex(const Edition& e) {
	switch (e.kind) {
		case EditionKind::Approaching:
			return 1 + static_cast<UInt32>(e.city);
		case EditionKind::Attack:
			return 1 + kNewsCities + static_cast<UInt32>(e.city);
		case EditionKind::Several:
			return 1 + 2 * kNewsCities;
		case EditionKind::Besieged:
			return 2 + 2 * kNewsCities + static_cast<UInt32>(e.city);
		case EditionKind::Lifted:
			return 2 + 3 * kNewsCities + static_cast<UInt32>(e.city);
		case EditionKind::CampSaved:
			return 2 + 4 * kNewsCities;
		case EditionKind::CampLost:
			return 3 + 4 * kNewsCities;
		case EditionKind::Refuges:
			return 4 + 4 * kNewsCities;
		case EditionKind::General:
		default:
			return 0;
	}
}

// The ESP's book of edition `index`: the first 18 at 0x8FC.., the siege's
// after the siege pool at 0x9B7.., the camps' two after the camps' kits at
// 0xA38 (CampLogic.h kCampExtraFirstLocalId; tools/esp-write.py
// LOCAL_EDITION_IDS).
inline UInt32 EditionLocalId(UInt32 index) {
	// 36, the held dungeons': DungeonQuestLogic.h kDungeonEditionLocalId.
	return index < 18 ? 0x8FC + index : (index < 34 ? 0x9B7 + (index - 18) : (index < 36 ? 0xA38 + (index - 34) : 0xA75));
}

struct NewsState {
	float lastAttackDays[kNewsCities] = {-1.0f, -1.0f, -1.0f, -1.0f, -1.0f, -1.0f, -1.0f, -1.0f};  // -1: never
	int headingCity = -1;      // the last city a horde near the player was seen heading for, -1 none
	float headingDays = -1.0f;  // the game day it was seen
	// 4.12: the game day each city's siege began, -1 none on; the day the
	// last one was lifted, -1 never.
	float besiegedDays[kNewsCities] = {-1.0f, -1.0f, -1.0f, -1.0f, -1.0f, -1.0f, -1.0f, -1.0f};
	float liftedDays[kNewsCities] = {-1.0f, -1.0f, -1.0f, -1.0f, -1.0f, -1.0f, -1.0f, -1.0f};
	// 4.20.6: the game day of the camps' latest news (-1 none) and whether
	// it was a camp saved (defended, or its refugees brought to a city) or
	// lost (overrun).
	float campDays = -1.0f;
	bool campSaved = false;
	bool refuges = false;  // a held dungeon is news now (Dungeons.cpp says it each second; not kept in the save)
};

// A camp saved or lost at game day `days`: the camps' latest news.
inline void NoteCampNews(NewsState& news, bool saved, float days) {
	if (days >= news.campDays) {
		news.campDays = days;
		news.campSaved = saved;
	}
}

// A siege of `city` begun (`begun`) or lifted at game day `days`.
inline void NoteSiegeNews(NewsState& news, UInt32 city, bool begun, float days) {
	if (city >= kNewsCities) {
		return;
	}
	if (begun) {
		news.besiegedDays[city] = days;
	} else {
		news.besiegedDays[city] = -1.0f;
		news.liftedDays[city] = days;
	}
}

// Walkers at `city`'s gate and its guards out, at game day `days`.
inline void NoteCityAttack(NewsState& news, UInt32 city, float days) {
	if (city < kNewsCities && days > news.lastAttackDays[city]) {
		news.lastAttackDays[city] = days;
	}
}

// A horde near the player seen heading for `city` at game day `days`; -1
// (none seen now) keeps the last sighting, a city out of range is ignored.
inline void NoteHeading(NewsState& news, int city, float days) {
	if (city >= 0 && city < static_cast<int>(kNewsCities)) {
		news.headingCity = city;
		news.headingDays = days;
	}
}

// The edition for game day `now`: several cities attacked lately, else the
// one attacked, else the city a horde was last seen heading for within
// `holdDays`, else the outbreak in general.
inline Edition ChooseEdition(const NewsState& news, float now, float recentDays, float holdDays) {
	// A siege on is the news before all else, the latest begun; then a
	// siege lifted lately, the latest.
	int besieged = -1;
	int lifted = -1;
	for (UInt32 c = 0; c < kNewsCities; ++c) {
		if (news.besiegedDays[c] >= 0.0f &&
		    (besieged < 0 || news.besiegedDays[c] > news.besiegedDays[static_cast<UInt32>(besieged)])) {
			besieged = static_cast<int>(c);
		}
		if (news.liftedDays[c] >= 0.0f && now - news.liftedDays[c] <= recentDays &&
		    (lifted < 0 || news.liftedDays[c] > news.liftedDays[static_cast<UInt32>(lifted)])) {
			lifted = static_cast<int>(c);
		}
	}
	if (besieged >= 0) {
		return Edition{EditionKind::Besieged, besieged};
	}
	if (lifted >= 0) {
		return Edition{EditionKind::Lifted, lifted};
	}
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
	if (news.campDays >= 0.0f && now - news.campDays <= recentDays) {
		return Edition{news.campSaved ? EditionKind::CampSaved : EditionKind::CampLost, -1};
	}
	if (news.headingCity >= 0 && news.headingCity < static_cast<int>(kNewsCities) && news.headingDays >= 0.0f &&
	    now - news.headingDays <= holdDays) {
		return Edition{EditionKind::Approaching, news.headingCity};
	}
	return Edition{news.refuges ? EditionKind::Refuges : EditionKind::General, -1};
}

// The game's clock in days: GameDaysPassed's whole days plus GameHour. The
// harness run of 2026-10-26 19:59 read GameDaysPassed 1.00 for 100 s of
// play at TimeScale 30 (0.03 days), so it may count whole days only; the
// whole days and the hour are right either way. Below 0: no clock.
inline float GameTimeDays(float daysPassed, float hour) {
	if (daysPassed < 0.0f) {
		return -1.0f;
	}
	const float day = static_cast<float>(static_cast<UInt32>(daysPassed));
	return hour >= 0.0f && hour < 24.0f ? day + hour / 24.0f : day;
}

// The co-save record: type "NEWS", version 4 - the last attack's game day
// per city, the last sighting's city and game day, each city's siege begun
// and lifted days, then the camps' news day and kind (1 saved). Version 1
// had the attacks alone, version 2 no sieges, version 3 no camps, read as
// such.
inline constexpr UInt32 kNewsRecordType = 0x4E455753;  // 'NEWS'
inline constexpr UInt32 kNewsRecordVersion = 4;
inline constexpr UInt32 kNewsImageV1Bytes = 4 * kNewsCities;
inline constexpr UInt32 kNewsImageV2Bytes = kNewsImageV1Bytes + 8;
inline constexpr UInt32 kNewsImageV3Bytes = kNewsImageV2Bytes + 8 * kNewsCities;
inline constexpr UInt32 kNewsImageBytes = kNewsImageV3Bytes + 8;

inline UInt32 NewsImage(const NewsState& news, UInt8* out, UInt32 capacity) {
	if (capacity < kNewsImageBytes) {
		return 0;
	}
	for (UInt32 c = 0; c < kNewsCities; ++c) {
		UInt32 bits = 0;
		std::memcpy(&bits, &news.lastAttackDays[c], sizeof(bits));
		Put32(out + 4 * c, bits);
	}
	UInt32 days = 0;
	std::memcpy(&days, &news.headingDays, sizeof(days));
	Put32(out + kNewsImageV1Bytes, static_cast<UInt32>(news.headingCity));
	Put32(out + kNewsImageV1Bytes + 4, days);
	for (UInt32 c = 0; c < kNewsCities; ++c) {
		UInt32 begun = 0;
		UInt32 lifted = 0;
		std::memcpy(&begun, &news.besiegedDays[c], sizeof(begun));
		std::memcpy(&lifted, &news.liftedDays[c], sizeof(lifted));
		Put32(out + kNewsImageV2Bytes + 4 * c, begun);
		Put32(out + kNewsImageV2Bytes + 4 * kNewsCities + 4 * c, lifted);
	}
	UInt32 camp = 0;
	std::memcpy(&camp, &news.campDays, sizeof(camp));
	Put32(out + kNewsImageV3Bytes, camp);
	Put32(out + kNewsImageV3Bytes + 4, news.campSaved ? 1u : 0u);
	return kNewsImageBytes;
}

// `news` emptied first; false when shorter than the attacks (nothing read
// then). The sighting only from a record long enough for it.
inline bool ReadNewsImage(NewsState& news, const UInt8* in, UInt32 length) {
	news = NewsState{};
	if (length < kNewsImageV1Bytes) {
		return false;
	}
	for (UInt32 c = 0; c < kNewsCities; ++c) {
		const UInt32 bits = Get32(in + 4 * c);
		std::memcpy(&news.lastAttackDays[c], &bits, sizeof(bits));
	}
	if (length >= kNewsImageBytes) {
		const UInt32 camp = Get32(in + kNewsImageV3Bytes);
		std::memcpy(&news.campDays, &camp, sizeof(camp));
		news.campSaved = Get32(in + kNewsImageV3Bytes + 4) != 0;
	}
	if (length >= kNewsImageV3Bytes) {
		for (UInt32 c = 0; c < kNewsCities; ++c) {
			const UInt32 begun = Get32(in + kNewsImageV2Bytes + 4 * c);
			const UInt32 lifted = Get32(in + kNewsImageV2Bytes + 4 * kNewsCities + 4 * c);
			std::memcpy(&news.besiegedDays[c], &begun, sizeof(begun));
			std::memcpy(&news.liftedDays[c], &lifted, sizeof(lifted));
		}
	}
	if (length >= kNewsImageV2Bytes) {
		const UInt32 days = Get32(in + kNewsImageV1Bytes + 4);
		NoteHeading(news, static_cast<int>(Get32(in + kNewsImageV1Bytes)), 0.0f);
		std::memcpy(&news.headingDays, &days, sizeof(days));
		if (news.headingCity < 0) {
			news.headingDays = -1.0f;
		}
	}
	return true;
}

}  // namespace walkers
