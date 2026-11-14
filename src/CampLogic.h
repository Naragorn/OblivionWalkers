#pragma once

// The survivors' camps, process-free (docs/walkers-spec.md 4.20.6; designed
// with Nadi 2026-11-05, "go 4.20.6" the same day - in the INI and LINK, on by
// default). The first of the design's two steps: the camps and their defence;
// the escort to a city comes second.
//
// CampCount camps (the dynamic camps below), each with PeoplePerCamp refugees,
// WomenPercent of them women, and a trader. A camp is attacked by a chance a
// game day (AttackChancePercent), at an hour of that day, whether the player
// is there or not:
// - near it (kCampSeenUnits), the attack is fought out in the game: its
//   walkers come out of the ESP's pool and walk at the camp (Camps.cpp), at
//   most CampsAttackedAtOnce camps at once; each walker ended counts off the
//   attack, each refugee dead counts off the camp;
// - away, it is rolled each game hour as the siege's toll (SiegeLogic.h): the
//   refugees cut RefugeeKillsPerHour walkers down and lose one of theirs by
//   LossPercentPerHour.
// All walkers ended with a refugee alive: the camp holds; when the player was
// near during the attack it is defended - the rewards (fame, gold, each
// settable, and always the "quest completed" moment, a line and a sound).
// All refugees dead: the camp is overrun - its dead rise as walkers (the rise,
// 4.1, for those killed in the game; the ones lost away and the attack's
// walkers left stay at the camp, shown when the player comes near). An overrun
// camp comes back by ReturnChancePercent a game day.
//
// The escort, the design's second step (Nadi, 2026-11-08: "go A B C D", the
// escort failing "passt" - all dead, or left behind farther than 5000 units -
// and its reward "skaliert je nachdem wieviele überleben"): a living camp's
// refugees ask the player (their greeting, a topic in the ESP) to lead them
// to the nearest city; said yes, they follow him to its gate. All dead on the
// way, or the nearest of them farther than EscortBehindUnits from him: failed
// - the dead rise, the living go back to their camp. At the gate: arrived -
// fame and gold scaled by those who made it, a thing for each two; they stay
// EscortStayDays game days, then leave the game, the camp empty until it is
// settled again by ReturnChancePercent. One escort at a time.

#include <cmath>
#include <cstring>

#include "Bytes.h"
#include "Types.h"

namespace walkers {

// The dynamic camps (Nadi, 2026-11-06: "go dynamic"): up to kCampMost camps,
// CampCount of them, each at a place rolled once per game from the
// candidates (CampCandidates.h, tools/camp-places.py --candidates), at least
// CampSpacingUnits apart. A camp's people are a kit of the ESP's, kCampKits
// kits, moved to the camps nearest the player (ChooseCampKits). Its map marker
// and props stand at every candidate in the ESP, the marker the props' enable
// parent, enabled for the candidates in play - a prop moved across the map is
// not solid there (the probes of 2026-11-06); the engine's own enable parents
// need no move (Nadi, 2026-11-06: "A").
inline constexpr UInt32 kCampMost = 64;
inline constexpr UInt16 kCampNoPlace = 0xFFFF;

// A spot about a camp: where, and turned about x and z (radians, the
// engine's: 0 north, clockwise).
struct CampSpot {
	float x;
	float y;
	float z;
	float rx;
	float rz;
};

// A camp's people in the ESP (tools/esp-write.py): six men, six women and a
// trader, slots 0..5, 6..11 and 12; the roster says which are in the camp.
inline constexpr UInt32 kCampMen = 6;
inline constexpr UInt32 kCampWomen = 6;
inline constexpr UInt32 kCampTraderSlot = 12;
inline constexpr UInt32 kCampSlots = 13;
inline constexpr UInt32 kCampWalkersAtOnce = 12;

// A place a camp can be rolled to: its fire, the way to the road (degrees),
// the name's index (kCampNames), whether it is by a road, the people's spots
// (tools/esp-write.py camp_person_spot's) and the land's height under the
// walker spots (CampWalkerSpot: the attack's ring, then the overrun camp's).
// Its props are the ESP's (camp_layout), at the candidate itself.
struct CampCandidate {
	float x;
	float y;
	float z;
	float facing;
	UInt16 name;
	UInt8 byRoad;
	CampSpot people[kCampSlots];
	float spotZ[2 * kCampWalkersAtOnce];
};
// The ESP's records (tools/esp-write.py reads these lines): the refugees'
// fourteen bases (six men, six women, a man and a woman who trade), the
// merchant chest the traders sell from, the attack's two markers and travel
// packages, the pool of attacking walkers, then the kits - each thirteen
// people; after the grab's shock sounds (GrabLogic.h) the candidates' map
// markers, one per candidate in CampCandidates.h's order, each its props'
// enable parent, then their props (the ESP's alone: eleven a candidate in the
// plain ESP, eight in the ESL, whose IDs end at 0xFFF - Nadi, 2026-11-06:
// "Mach die esp großzügiger, und die esl light"; the plugin only enables the
// markers).
inline constexpr UInt32 kCampBaseFirstLocalId = 0x9D9;
inline constexpr UInt32 kCampBaseCount = 14;
inline constexpr UInt32 kCampChestLocalId = 0x9E7;
inline constexpr UInt32 kCampMarkerFirstLocalId = 0x9E8;
inline constexpr UInt32 kCampTravelFirstLocalId = 0x9EA;
inline constexpr UInt32 kCampPoolFirstLocalId = 0x9EC;
inline constexpr UInt32 kCampFirstLocalId = 0xA04;
inline constexpr UInt32 kCampKits = 4;
inline constexpr UInt32 kCampKitBlock = kCampSlots;  // the people
inline constexpr UInt32 kCampKitFirstLocalId = kCampFirstLocalId;
inline constexpr UInt32 kCampLocalIdEnd = kCampKitFirstLocalId + kCampKits * kCampKitBlock;
inline constexpr UInt32 KitPersonLocalId(UInt32 kit, UInt32 slot) { return kCampKitFirstLocalId + kit * kCampKitBlock + slot; }
// After the kits, before the grab's shock sounds (0xAA4): the camps' news
// (two editions, NewsLogic.h EditionLocalId), their rumor and its global,
// the escort's globals, topic, lines and package, and the refugees' camp
// life packages (tools/esp-write.py writes them in this order).
inline constexpr UInt32 kCampExtraFirstLocalId = kCampLocalIdEnd;           // 0xA38: the editions
inline constexpr UInt32 kCampRumorGlobalLocalId = kCampExtraFirstLocalId + 2;  // 1 while the camps are on
inline constexpr UInt32 kCampRumorLocalId = kCampExtraFirstLocalId + 3;
inline constexpr UInt32 kEscortOfferGlobalLocalId = kCampExtraFirstLocalId + 4;  // 1 a camp near asks, 2 on the way
inline constexpr UInt32 kEscortAskGlobalLocalId = kCampExtraFirstLocalId + 5;    // set 1 by the topic's line
inline constexpr UInt32 kEscortTopicLocalId = kCampExtraFirstLocalId + 6;
inline constexpr UInt32 kEscortGreetingLocalId = kCampExtraFirstLocalId + 7;
inline constexpr UInt32 kEscortYesLocalId = kCampExtraFirstLocalId + 8;
inline constexpr UInt32 kEscortOnTheWayLocalId = kCampExtraFirstLocalId + 9;
inline constexpr UInt32 kEscortFollowLocalId = kCampExtraFirstLocalId + 10;
inline constexpr UInt32 kRefugeeWanderLocalId = kCampExtraFirstLocalId + 11;
inline constexpr UInt32 kRefugeeSleepLocalId = kCampExtraFirstLocalId + 12;
inline constexpr UInt32 kCampExtraEnd = kCampExtraFirstLocalId + 13;
inline constexpr UInt32 kCandidateMarkerFirstLocalId = 0xAA7;
inline constexpr UInt32 CandidateMarkerLocalId(UInt32 candidate) {
	return kCandidateMarkerFirstLocalId + candidate;
}

// Each base's outfit, Oblivion.esm's: a shirt, trousers, shoes and a rusty
// weapon (the ESP gives them; a refugee who comes back is dressed anew - the
// rise moved his things onto his walker).
inline constexpr UInt32 kCampOutfits[kCampBaseCount][4] = {
	{0x000229AA, 0x000229AB, 0x000229AC, 0x00090616}, {0x00000883, 0x00000857, 0x0001C82B, 0x00090615},
	{0x0001C82E, 0x0001C82C, 0x0001C82F, 0x00090614}, {0x000229AD, 0x000229AE, 0x000229AF, 0x00090613},
	{0x0001C831, 0x0001C830, 0x0001C883, 0x00090616}, {0x00000B86, 0x0001047C, 0x00000BEA, 0x00090615},
	{0x000229AA, 0x000229AB, 0x000229AC, 0x00090614}, {0x00000883, 0x00000857, 0x0001C82B, 0x00090613},
	{0x0001C82E, 0x0001C82C, 0x0001C82F, 0x00090616}, {0x000229AD, 0x000229AE, 0x000229AF, 0x00090615},
	{0x0001C831, 0x0001C830, 0x0001C883, 0x00090614}, {0x00000B86, 0x0001047C, 0x00000BEA, 0x00090613},
	{0x000229AD, 0x000229AE, 0x000229AF, 0x00090616}, {0x00000B86, 0x0001047C, 0x00000BEA, 0x00090616},
};

// The base of kit `camp`'s person in `slot`: the men and the women each
// turned by the kit's number, so the kits do not all look alike; the trader
// a man in the even kits, a woman in the odd ones.
inline constexpr UInt32 CampBaseIndex(UInt32 camp, UInt32 slot) {
	return slot < 6 ? (slot + camp) % 6 : (slot < 12 ? 6 + (slot - 6 + camp) % 6 : 12 + camp % 2);
}

inline constexpr UInt32 kCampPeopleLeast = 2;
inline constexpr UInt32 kCampPeopleMost = 6;
// The pool of attacking walkers: twelve at one camp at once, two camps.
inline constexpr UInt32 kCampShownMost = 2;
inline constexpr UInt32 kCampPool = kCampShownMost * kCampWalkersAtOnce;
// The player is at a camp within this of its place, outdoors in Tamriel: its
// people are loaded and fight (the exterior grid's two cells around).
inline constexpr float kCampSeenUnits = 6000.0f;
// A kit is put at a camp within this of the player (before its cells load:
// the exterior grid's two cells around reach some 10000), and kept while the
// player is within the second.
inline constexpr float kCampKitUnits = 14000.0f;
inline constexpr float kCampKitKeepUnits = 18000.0f;
// Hours of the away toll rolled at one look at most (a week waited).
inline constexpr UInt32 kCampTollHoursMost = 24 * 7;

// [Survivors] in OblivionWalkers.ini.
struct CampSettings {
	bool enabled = true;                // Enabled
	UInt32 campCount = kCampMost;       // CampCount, held to 1..kCampMost
	float spacingUnits = 10000.0f;      // CampSpacingUnits: the places at least this far apart
	UInt32 peoplePerCamp = 6;           // PeoplePerCamp, held to 2..6
	UInt32 womenPercent = 50;           // WomenPercent
	bool trader = true;                 // Trader: one more in each camp, who trades
	bool rewardFame = true;             // RewardFame
	UInt32 famePoints = 1;              // FamePoints
	bool rewardGold = true;             // RewardGold
	UInt32 gold = 150;                  // Gold
	UInt32 attackChancePercent = 15;    // AttackChancePercent: a camp and game day
	UInt32 walkersMin = 6;              // AttackWalkersMin
	UInt32 walkersMax = 12;             // AttackWalkersMax
	UInt32 campsAttackedAtOnce = 1;     // CampsAttackedAtOnce: fought out near the player, 1..2
	float refugeeKillsPerHour = 2.0f;   // RefugeeKillsPerHour: away
	UInt32 lossPercentPerHour = 25;     // LossPercentPerHour: away, one refugee lost an hour by this
	UInt32 returnChancePercent = 10;    // ReturnChancePercent: an overrun camp back, a game day
	bool rewardThings = true;           // RewardThings: a thing off the game's loot lists
	bool escort = true;                 // Escort: the refugees ask to be led to the nearest city
	UInt32 escortFame = 2;              // EscortFame: all brought there; scaled by those who made it
	UInt32 escortGold = 300;            // EscortGold: the same
	float escortBehindUnits = 5000.0f;  // EscortBehindUnits: the nearest farther from the player: failed
	float escortStayDays = 7.0f;        // EscortStayDays: in the city, then they leave the game
	bool escortIntoCity = true;         // EscortIntoCity: brought in through the gate, not left before it
	// Test switch (harness/camp-defend.txt): camp N attacked 30 s after the
	// load and the player taken to it (-1: never).
	int testCamp = -1;
	// Test switch (harness/camp-escort.txt): camp N's refugees asked 40 s
	// after the load (the player taken there 30 s after it, 1500 units off),
	// then the player and they taken to the nearest gate - or, with
	// TestEscortBehind, the player 7000 units off (-1: never).
	int testEscort = -1;
	bool testEscortBehind = false;
	// EscortFollowSeconds (INI only): the follow package sent again this
	// often while on the way; 0 only at the start and after a load.
	float escortFollowSeconds = 5.0f;
};

// Escort: its refugees on the way to a city; Gone: there, for
// EscortStayDays; Left: empty (they left the game, or died on the way),
// settled again as an overrun camp is.
enum class CampStatus : UInt8 { Living, Attacked, Overrun, Escort, Gone, Left };

struct Camp {
	UInt16 place = kCampNoPlace;  // the candidate it was rolled to
	CampStatus status = CampStatus::Living;
	bool set = false;   // the roster chosen
	bool seen = false;  // the player near during this attack
	UInt16 roster = 0;  // the slots in the camp
	UInt16 lost = 0;    // of them dead or lost
	UInt32 walkersLeft = 0;  // the attack's; the overrun camp's walkers
	float startAt = -1.0f;   // a coming attack's game day, -1 none
	float tollFrom = -1.0f;  // the away toll counted from this game day
	float carry = 0.0f;      // the toll's kills short of a whole walker
};

// The one escort: the camp (-1 none), the city gate it heads for
// (CityGates.h kCityGates), how many set out, the game day it arrived (-1
// on the way), the kit its people are (-1 not known) - a save loaded on the
// way finds them where they were, not at their camp.
struct CampEscort {
	int camp = -1;
	int gate = -1;
	UInt32 started = 0;
	float arrivedAt = -1.0f;
	int kit = -1;
};

struct CampsState {
	Camp camps[kCampMost];
	int lastDay = -1;
	CampEscort escort;
};

// The camps in play: CampCount, held to 1..kCampMost.
inline UInt32 CampActive(const CampSettings& s) {
	return s.campCount < 1 ? 1u : (s.campCount > kCampMost ? kCampMost : s.campCount);
}

// Whether camp `i` is in play: one of CampActive, rolled to a place.
inline bool CampInPlay(const CampsState& st, const CampSettings& s, UInt32 i) {
	return i < CampActive(s) && st.camps[i].place != kCampNoPlace;
}

// One roll: a place for each camp in play that has none, from the `count`
// candidates in an order drawn by `roll()` (each candidate once), taking one
// at least CampSpacingUnits from every camp placed - the earlier rolls kept.
// Returns the camps placed now; a camp left without one when no candidate is
// far enough (CampInPlay false: it is not in play).
template <typename RollFn>
UInt32 RollCampPlacesOnce(CampsState& st, const CampSettings& s, const CampCandidate* candidates, UInt32 count,
                      RollFn roll) {
	const UInt32 active = CampActive(s);
	UInt32 wanted = 0;
	for (UInt32 i = 0; i < active; ++i) {
		wanted += st.camps[i].place == kCampNoPlace ? 1u : 0u;
	}
	if (wanted == 0 || count == 0) {
		return 0;
	}
	const float apart = s.spacingUnits > 0.0f ? s.spacingUnits * s.spacingUnits : 0.0f;
	// The order: a draw among those not yet tried, each step (Fisher-Yates
	// over the indices, kept in a small window of the space at a time).
	UInt16 order[4096];
	const UInt32 n = count < 4096 ? count : 4096;
	for (UInt32 k = 0; k < n; ++k) {
		order[k] = static_cast<UInt16>(k);
	}
	UInt32 placed = 0;
	UInt32 next = 0;
	for (UInt32 k = 0; k < n && placed < wanted; ++k) {
		const UInt32 pick = k + roll() % (n - k);
		const UInt16 c = order[pick];
		order[pick] = order[k];
		order[k] = c;
		bool far = true;
		for (UInt32 i = 0; i < kCampMost && far; ++i) {
			const UInt16 p = st.camps[i].place;
			if (p == kCampNoPlace || p >= count) {
				continue;
			}
			const float dx = candidates[p].x - candidates[c].x;
			const float dy = candidates[p].y - candidates[c].y;
			far = p != c && dx * dx + dy * dy >= apart;
		}
		if (!far) {
			continue;
		}
		while (next < active && st.camps[next].place != kCampNoPlace) {
			++next;
		}
		st.camps[next].place = c;
		++placed;
	}
	return placed;
}

// The places rolled up to kCampRollTries times, the roll that placed the
// most kept (the first that placed all ends it): the reachable land free of
// trees and rocks holds some 64 to 71 camps 10000 apart (tools/camp-places.py
// on 2026-11-06, 200 random orders of its 140 candidates), so one order may
// fall short of 64 where another does not.
inline constexpr UInt32 kCampRollTries = 8;

template <typename RollFn>
UInt32 RollCampPlaces(CampsState& st, const CampSettings& s, const CampCandidate* candidates, UInt32 count,
                      RollFn roll) {
	UInt32 wanted = 0;
	for (UInt32 i = 0; i < CampActive(s); ++i) {
		wanted += st.camps[i].place == kCampNoPlace ? 1u : 0u;
	}
	CampsState best = st;
	UInt32 bestPlaced = 0;
	for (UInt32 t = 0; t < kCampRollTries && bestPlaced < wanted; ++t) {
		CampsState trial = st;
		// The draws go on across the tries (a functor is not copied anew).
		const UInt32 placed = RollCampPlacesOnce(trial, s, candidates, count, [&roll]() { return roll(); });
		if (placed > bestPlaced || t == 0) {
			best = trial;
			bestPlaced = placed;
		}
	}
	st = best;
	return bestPlaced;
}

// The kits (kCampKits, each a camp's people and props): `kitCamp[k]` the camp
// kit k is at (-1 none), `distance` the player's from each camp in play (a
// negative one: not in Tamriel, or not in play). A kit is kept at a camp
// within kCampKitKeepUnits and freed farther (or when the camp is out of
// play); the camps within kCampKitUnits without one, the nearest first, take
// the free kits. `moved[k]` says kit k was put at a camp now (to be set up).
inline void ChooseCampKits(int* kitCamp, const float* distance, UInt32 active, bool* moved) {
	for (UInt32 k = 0; k < kCampKits; ++k) {
		moved[k] = false;
		const int c = kitCamp[k];
		if (c >= 0 && (static_cast<UInt32>(c) >= active || distance[c] < 0.0f || distance[c] > kCampKitKeepUnits)) {
			kitCamp[k] = -1;
		}
	}
	for (;;) {
		int free = -1;
		for (UInt32 k = 0; k < kCampKits && free < 0; ++k) {
			free = kitCamp[k] < 0 ? static_cast<int>(k) : -1;
		}
		if (free < 0) {
			return;
		}
		int best = -1;
		for (UInt32 i = 0; i < active; ++i) {
			if (distance[i] < 0.0f || distance[i] > kCampKitUnits) {
				continue;
			}
			bool has = false;
			for (UInt32 k = 0; k < kCampKits; ++k) {
				has = has || kitCamp[k] == static_cast<int>(i);
			}
			if (!has && (best < 0 || distance[i] < distance[best])) {
				best = static_cast<int>(i);
			}
		}
		if (best < 0) {
			return;
		}
		kitCamp[free] = best;
		moved[free] = true;
	}
}

// The kit at camp `i`, -1 none.
inline int KitOf(const int* kitCamp, UInt32 i) {
	for (UInt32 k = 0; k < kCampKits; ++k) {
		if (kitCamp[k] == static_cast<int>(i)) {
			return static_cast<int>(k);
		}
	}
	return -1;
}

struct CampEvent {
	enum Kind : UInt8 {
		Begin,
		PersonLost,
		HeldAway,
		Defended,
		Overrun,
		Return,
		EscortBegin,    // they follow the player
		EscortArrived,  // at the gate: the reward
		EscortBehind,   // failed: left behind, back to their camp
		EscortDead,     // failed: none made it
		EscortLeft,     // their days in the city over: gone from the game
	};
	Kind kind;
	UInt8 camp;
	UInt8 slot;  // PersonLost: the slot
};

inline UInt32 CampBits(UInt32 mask) {
	UInt32 n = 0;
	for (; mask != 0; mask &= mask - 1) {
		++n;
	}
	return n;
}

inline UInt32 CampPeople(const CampSettings& s) {
	return s.peoplePerCamp < kCampPeopleLeast ? kCampPeopleLeast
	                                          : (s.peoplePerCamp > kCampPeopleMost ? kCampPeopleMost : s.peoplePerCamp);
}

// The women among `people` at `percent`, rounded half up.
inline UInt32 CampWomenOf(UInt32 people, UInt32 percent) {
	const UInt32 p = percent > 100 ? 100 : percent;
	return (people * p + 50) / 100;
}

// The slots a camp is set up with: the first men, the first women, the trader.
inline UInt16 CampRoster(const CampSettings& s) {
	const UInt32 people = CampPeople(s);
	const UInt32 women = CampWomenOf(people, s.womenPercent);
	const UInt32 men = people - women;
	UInt32 roster = (1u << men) - 1u;
	roster |= ((1u << women) - 1u) << kCampMen;
	if (s.trader) {
		roster |= 1u << kCampTraderSlot;
	}
	return static_cast<UInt16>(roster);
}

inline UInt32 CampAlive(const Camp& c) { return CampBits(static_cast<UInt32>(c.roster & ~c.lost)); }

// A living camp nobody of which is lost takes the settings' roster (LINK, a
// first look); true when it changed.
inline bool CampTakeRoster(Camp& c, const CampSettings& s) {
	if (c.status != CampStatus::Living || c.lost != 0) {
		return false;
	}
	const UInt16 roster = CampRoster(s);
	const bool changed = !c.set || roster != c.roster;
	c.roster = roster;
	c.set = true;
	return changed;
}

namespace camp_detail {

inline UInt32 Walkers(const CampSettings& s, UInt32 roll) {
	const UInt32 least = s.walkersMin < s.walkersMax ? s.walkersMin : s.walkersMax;
	const UInt32 most = s.walkersMin < s.walkersMax ? s.walkersMax : s.walkersMin;
	const UInt32 walkers = least + roll % (most - least + 1);
	return walkers > 0 ? walkers : 1;
}

inline UInt32 Put(CampEvent* out, UInt32 n, UInt32 most, CampEvent::Kind kind, UInt32 camp, UInt32 slot = 0) {
	if (n < most) {
		out[n] = CampEvent{kind, static_cast<UInt8>(camp), static_cast<UInt8>(slot)};
		return n + 1;
	}
	return n;
}

// The `nth` living slot (0-based) of the camp.
inline UInt32 NthAlive(const Camp& c, UInt32 nth) {
	const UInt32 alive = static_cast<UInt32>(c.roster & ~c.lost);
	for (UInt32 slot = 0; slot < kCampSlots; ++slot) {
		if ((alive & (1u << slot)) != 0) {
			if (nth == 0) {
				return slot;
			}
			--nth;
		}
	}
	return kCampSlots;
}

inline UInt32 Overrun(Camp& c, UInt32 camp, CampEvent* out, UInt32 n, UInt32 most) {
	c.status = CampStatus::Overrun;
	c.startAt = -1.0f;
	c.tollFrom = -1.0f;
	c.carry = 0.0f;
	return Put(out, n, most, CampEvent::Overrun, camp);
}

inline UInt32 Held(Camp& c, UInt32 camp, CampEvent* out, UInt32 n, UInt32 most) {
	const bool defended = c.seen;
	c.status = CampStatus::Living;
	c.walkersLeft = 0;
	c.seen = false;
	c.tollFrom = -1.0f;
	c.carry = 0.0f;
	return Put(out, n, most, defended ? CampEvent::Defended : CampEvent::HeldAway, camp);
}

}  // namespace camp_detail

// Once a second with game day `now`: a new whole day rolls each camp once -
// however many days went by - by `roll()` (0..any): a living camp with no
// attack coming is attacked by AttackChancePercent, at an hour of that day
// (not before now); an overrun one comes back by ReturnChancePercent, its
// people set up anew. The first look only notes the day.
template <typename RollFn>
UInt32 StepCampDays(CampsState& st, const CampSettings& s, float now, RollFn roll, CampEvent* out, UInt32 most) {
	if (now < 0.0f) {
		return 0;
	}
	const int day = static_cast<int>(now);
	if (st.lastDay < 0 || day <= st.lastDay) {
		st.lastDay = st.lastDay < 0 ? day : st.lastDay;
		return 0;
	}
	st.lastDay = day;
	UInt32 n = 0;
	for (UInt32 i = 0; s.enabled && i < CampActive(s); ++i) {
		Camp& c = st.camps[i];
		if (c.place == kCampNoPlace) {
			continue;
		}
		if (c.status == CampStatus::Living && c.startAt < 0.0f) {
			if (roll() % 100 < s.attackChancePercent) {
				const float at = static_cast<float>(day) + static_cast<float>(roll() % 24) / 24.0f;
				c.startAt = at < now ? now : at;
			}
		} else if (c.status == CampStatus::Overrun || c.status == CampStatus::Left) {
			if (roll() % 100 < s.returnChancePercent) {
				const UInt16 place = c.place;
				c = Camp{};
				c.place = place;
				c.roster = CampRoster(s);
				c.set = true;
				n = camp_detail::Put(out, n, most, CampEvent::Return, i);
			}
		}
	}
	return n;
}

// Camp `i`'s attack begun now (the test switch): `roll` draws its walkers.
inline UInt32 BeginCampAttack(Camp& c, UInt32 i, const CampSettings& s, float now, bool near, UInt32 roll,
                              CampEvent* out, UInt32 most) {
	if (!s.enabled || c.status != CampStatus::Living || CampAlive(c) == 0) {
		return 0;
	}
	c.status = CampStatus::Attacked;
	c.walkersLeft = camp_detail::Walkers(s, roll);
	c.startAt = -1.0f;
	c.tollFrom = now;
	c.carry = 0.0f;
	c.seen = near;
	return camp_detail::Put(out, 0, most, CampEvent::Begin, i);
}

// Once a second for camp `i` at game day `now`, the player `near` it or not:
// a coming attack begun when its hour has come; an attack away rolled for
// each whole game hour since the toll was counted - RefugeeKillsPerHour
// walkers cut down (the part short of one carried), one refugee lost by
// LossPercentPerHour - until the walkers or the refugees are gone. Near, no
// toll: the fight is the game's, and the attack counts as defended.
template <typename RollFn>
UInt32 StepCamp(Camp& c, UInt32 i, const CampSettings& s, float now, bool near, RollFn roll, CampEvent* out,
                UInt32 most) {
	if (!s.enabled || now < 0.0f) {
		return 0;
	}
	UInt32 n = 0;
	if (c.status == CampStatus::Living && c.startAt >= 0.0f && now >= c.startAt) {
		n = BeginCampAttack(c, i, s, now, near, roll(), out, most);
		if (n == 0) {
			c.startAt = -1.0f;
		}
		return n;
	}
	if (c.status != CampStatus::Attacked) {
		return 0;
	}
	if (near || c.tollFrom < 0.0f) {
		c.seen = c.seen || near;
		c.tollFrom = now;
		return 0;
	}
	float hours = std::floor((now - c.tollFrom) * 24.0f);
	if (hours < 1.0f) {
		return 0;
	}
	if (hours > static_cast<float>(kCampTollHoursMost)) {
		hours = static_cast<float>(kCampTollHoursMost);
	}
	const UInt32 whole = static_cast<UInt32>(hours);
	for (UInt32 h = 0; h < whole; ++h) {
		c.tollFrom += 1.0f / 24.0f;
		c.carry += s.refugeeKillsPerHour > 0.0f ? s.refugeeKillsPerHour : 0.0f;
		const UInt32 kills = static_cast<UInt32>(c.carry);
		c.carry -= static_cast<float>(kills);
		c.walkersLeft = kills >= c.walkersLeft ? 0 : c.walkersLeft - kills;
		const UInt32 alive = CampAlive(c);
		if (alive > 0 && roll() % 100 < s.lossPercentPerHour) {
			const UInt32 slot = camp_detail::NthAlive(c, roll() % alive);
			c.lost = static_cast<UInt16>(c.lost | (1u << slot));
			n = camp_detail::Put(out, n, most, CampEvent::PersonLost, i, slot);
		}
		if (CampAlive(c) == 0) {
			// The ones lost rise and stay at the camp with the walkers left.
			c.walkersLeft += CampBits(c.lost);
			return camp_detail::Overrun(c, i, out, n, most);
		}
		if (c.walkersLeft == 0) {
			return camp_detail::Held(c, i, out, n, most);
		}
	}
	return n;
}

// A walker of camp `i`'s ended (4.17's end): one off the attack - the last
// with a refugee alive holds the camp, defended when the player was near -
// or one off the overrun camp's walkers.
inline UInt32 CampWalkerEnded(Camp& c, UInt32 i, CampEvent* out, UInt32 most) {
	if (c.walkersLeft == 0) {
		return 0;
	}
	--c.walkersLeft;
	if (c.status == CampStatus::Attacked && c.walkersLeft == 0 && CampAlive(c) > 0) {
		return camp_detail::Held(c, i, out, 0, most);
	}
	return 0;
}

// Camp `i`'s person in `slot` found dead in the game: lost; the last of
// them overruns the camp (the rise raises them - the attack's walkers left
// stay at the camp).
inline UInt32 CampPersonDied(Camp& c, UInt32 i, UInt32 slot, CampEvent* out, UInt32 most) {
	if (slot >= kCampSlots || c.status == CampStatus::Overrun || c.status == CampStatus::Left) {
		return 0;
	}
	const UInt16 bit = static_cast<UInt16>(1u << slot);
	if ((c.roster & bit) == 0 || (c.lost & bit) != 0) {
		return 0;
	}
	c.lost = static_cast<UInt16>(c.lost | bit);
	// On the way or in the city: StepEscort decides what the last one means.
	if (CampAlive(c) > 0 || c.status == CampStatus::Escort || c.status == CampStatus::Gone) {
		return 0;
	}
	if (c.status != CampStatus::Attacked) {
		c.walkersLeft = 0;
	}
	return camp_detail::Overrun(c, i, out, 0, most);
}

// The camps whose walkers are shown, `distance` the player's from each (a
// negative one: not near): attacked camps first, the nearest first, at most
// CampsAttackedAtOnce (1..kCampShownMost) of them; then overrun camps with
// walkers, the nearest first - kCampShownMost in all. Returns the count
// written to `shown`.
inline UInt32 ChooseShownCamps(const CampsState& st, const CampSettings& s, const float* distance, UInt32* shown) {
	UInt32 attackedMost = s.campsAttackedAtOnce < 1 ? 1 : s.campsAttackedAtOnce;
	attackedMost = attackedMost > kCampShownMost ? kCampShownMost : attackedMost;
	UInt32 n = 0;
	if (!s.enabled) {
		return 0;
	}
	for (UInt32 pass = 0; pass < 2; ++pass) {
		bool taken[kCampMost] = {};
		for (UInt32 k = 0; k < n; ++k) {
			taken[shown[k]] = true;
		}
		for (;;) {
			const UInt32 most = pass == 0 ? attackedMost : kCampShownMost;
			if (n >= most) {
				break;
			}
			int best = -1;
			for (UInt32 i = 0; i < CampActive(s); ++i) {
				const Camp& c = st.camps[i];
				const bool wanted = pass == 0 ? c.status == CampStatus::Attacked
				                              : c.status == CampStatus::Overrun && c.walkersLeft > 0;
				if (!wanted || taken[i] || distance[i] < 0.0f || distance[i] > kCampSeenUnits) {
					continue;
				}
				if (best < 0 || distance[i] < distance[best]) {
					best = static_cast<int>(i);
				}
			}
			if (best < 0) {
				break;
			}
			taken[best] = true;
			shown[n++] = static_cast<UInt32>(best);
		}
	}
	return n;
}

// The walkers out at a shown camp at once.
inline UInt32 CampWalkersOut(const Camp& c) {
	return c.walkersLeft < kCampWalkersAtOnce ? c.walkersLeft : kCampWalkersAtOnce;
}

// Where walker `slot` (0..kCampWalkersAtOnce-1) of a shown camp comes out,
// from the camp's place: an attack's on a ring kCampAttackUnits out, a
// thirtieth turn apart, starting on the side away from the road; an overrun
// camp's among the tents.
inline constexpr float kCampAttackUnits = 1600.0f;
inline constexpr float kCampNestUnits = 350.0f;

// Where a camp's tent stands off its fire (tools/esp-write.py camp_layout:
// side -1 the first, +1 the second): behind the fire from the road, 45
// degrees to the side, 620 units out.
inline constexpr float kCampTentUnits = 620.0f;

inline void CampTentSpot(float facing, float side, float& dx, float& dy) {
	const float pi = 3.14159265f;
	const float at = facing * pi / 180.0f + pi + side * pi / 4.0f;
	dx = std::sin(at) * kCampTentUnits;
	dy = std::cos(at) * kCampTentUnits;
}

// The test switch's drop (TestCamp): where a fall first stopped - the first
// height no lower than the one before by more than half a unit, once it has
// come 20 units down from the first; -1e9 when it never stopped (or never
// fell). A stop on a tent's roof counts, not where the player slid to.
inline float CampFallStop(const float* z, UInt32 n) {
	for (UInt32 i = 1; i < n; ++i) {
		if (z[0] - z[i] >= 20.0f && z[i - 1] - z[i] <= 0.5f) {
			return z[i];
		}
	}
	return -1e9f;
}

inline void CampWalkerSpot(float facing, bool attack, UInt32 slot, float& dx, float& dy) {
	const float pi = 3.14159265f;
	const float away = (facing + 180.0f) * pi / 180.0f;
	// An attack's: away from the road, then a thirtieth to either side in turn
	// (0, +1, -1, +2, -2 ...); an overrun camp's: all around.
	const float side = slot % 2 == 1 ? 1.0f : -1.0f;
	const float at = attack ? away + side * static_cast<float>((slot + 1) / 2) * (2.0f * pi / 30.0f)
	                        : static_cast<float>(slot) * (2.0f * pi / static_cast<float>(kCampWalkersAtOnce));
	const float r = attack ? kCampAttackUnits : kCampNestUnits;
	dx = std::sin(at) * r;
	dy = std::cos(at) * r;
}

// The people a camp has out: its roster's living - in the camp, on the way
// and in the city - none when the camps are off or it lies empty.
inline UInt16 CampPeopleOut(const Camp& c, bool enabled) {
	if (!enabled || !c.set || c.status == CampStatus::Overrun || c.status == CampStatus::Left) {
		return 0;
	}
	return static_cast<UInt16>(c.roster & ~c.lost);
}

// The things reward (Nadi's "gold or things", 2026-11-05): one entry of a
// leveled list for the player's `level` - its entries' `levels` in the
// ascending order the game keeps them in. The top level at or below the
// player's and every entry down to kThingsLevelSpread below it (the game's
// iLevItemLevelDifferenceMax, 8 by default - derived from UESP's leveled
// lists page, not read off the engine), `roll` picking among them; -1 when
// none is low enough.
inline constexpr int kThingsLevelSpread = 8;

inline int PickLeveled(const UInt16* levels, UInt32 n, int level, UInt32 roll) {
	int top = -1;
	for (UInt32 k = 0; k < n; ++k) {
		if (static_cast<int>(levels[k]) <= level && static_cast<int>(levels[k]) > top) {
			top = levels[k];
		}
	}
	if (top < 0) {
		return -1;
	}
	UInt32 count = 0;
	for (UInt32 k = 0; k < n; ++k) {
		count += static_cast<int>(levels[k]) <= top && static_cast<int>(levels[k]) >= top - kThingsLevelSpread ? 1u : 0u;
	}
	UInt32 nth = roll % count;
	for (UInt32 k = 0; k < n; ++k) {
		if (static_cast<int>(levels[k]) <= top && static_cast<int>(levels[k]) >= top - kThingsLevelSpread) {
			if (nth == 0) {
				return static_cast<int>(k);
			}
			--nth;
		}
	}
	return -1;
}

// How many things: a defended camp one; an escort one for each two who made
// it, rounded up; none with RewardThings off or nobody alive.
inline UInt32 CampThings(bool rewardThings, UInt32 alive, UInt32 started, bool escort) {
	if (!rewardThings || alive == 0) {
		return 0;
	}
	(void)started;
	return escort ? (alive + 1) / 2 : 1;
}

// The escort's fame or gold: `base` for all who set out, scaled by those
// alive, rounded half up.
inline UInt32 ScaledReward(UInt32 base, UInt32 alive, UInt32 started) {
	if (started == 0 || alive == 0) {
		return 0;
	}
	const UInt32 a = alive > started ? started : alive;
	return (base * a * 2 + started) / (started * 2);
}

// An escort arrives with the nearest of them this near the gate's outside
// spot (some 21 m).
inline constexpr float kEscortArriveUnits = 1500.0f;

// Whether camp `i`'s refugees ask to be led to a city: the camps and the
// escort on, the camp living and not under attack, someone alive, no escort
// under way or in a city.
inline bool CampOffersEscort(const CampsState& st, const CampSettings& s, UInt32 i) {
	if (!s.enabled || !s.escort || i >= kCampMost || st.escort.camp >= 0) {
		return false;
	}
	const Camp& c = st.camps[i];
	return c.place != kCampNoPlace && c.set && c.status == CampStatus::Living && CampAlive(c) > 0;
}

// Camp `i`'s refugees said yes: on the way to city gate `gate`.
inline UInt32 BeginEscort(CampsState& st, const CampSettings& s, UInt32 i, int gate, float now, CampEvent* out,
                          UInt32 most) {
	(void)now;
	if (!CampOffersEscort(st, s, i) || gate < 0) {
		return 0;
	}
	Camp& c = st.camps[i];
	c.status = CampStatus::Escort;
	c.startAt = -1.0f;
	st.escort = CampEscort{};
	st.escort.camp = static_cast<int>(i);
	st.escort.gate = gate;
	st.escort.started = CampAlive(c);
	return camp_detail::Put(out, 0, most, CampEvent::EscortBegin, i);
}

// Once a second at game day `now`: `behind` the player's distance from the
// nearest of the escorted, `toGate` the nearest's from the gate (each -1 when
// not known - indoors, or not loaded). On the way: none alive fails it (the
// camp empty), the nearest farther than EscortBehindUnits fails it (they go
// back, the camp living), one within kEscortArriveUnits of the gate brings
// them there. In the city: EscortStayDays on, or none alive, they leave.
inline UInt32 StepEscort(CampsState& st, const CampSettings& s, float now, float behind, float toGate, CampEvent* out,
                         UInt32 most) {
	if (!s.enabled || st.escort.camp < 0 || st.escort.camp >= static_cast<int>(kCampMost)) {
		return 0;
	}
	const UInt32 i = static_cast<UInt32>(st.escort.camp);
	Camp& c = st.camps[i];
	if (c.status == CampStatus::Gone) {
		if (CampAlive(c) == 0 || now - st.escort.arrivedAt >= s.escortStayDays) {
			c.status = CampStatus::Left;
			st.escort = CampEscort{};
			return camp_detail::Put(out, 0, most, CampEvent::EscortLeft, i);
		}
		return 0;
	}
	if (c.status != CampStatus::Escort) {
		st.escort = CampEscort{};
		return 0;
	}
	if (CampAlive(c) == 0) {
		c.status = CampStatus::Left;
		st.escort = CampEscort{};
		return camp_detail::Put(out, 0, most, CampEvent::EscortDead, i);
	}
	if (toGate >= 0.0f && toGate <= kEscortArriveUnits) {
		c.status = CampStatus::Gone;
		st.escort.arrivedAt = now;
		return camp_detail::Put(out, 0, most, CampEvent::EscortArrived, i);
	}
	if (behind > s.escortBehindUnits) {
		c.status = CampStatus::Living;
		st.escort = CampEscort{};
		return camp_detail::Put(out, 0, most, CampEvent::EscortBehind, i);
	}
	return 0;
}

// Whether the kit of a camp whose people were brought to a city is out
// (Nadi, 2026-11-09: "Eskorte endet in stadt als toggle default an"): into
// the city - while the player walks its streets (`playerInCity`, the gate's
// city-side door's worldspace) or stands within kCampKitKeepUnits of the gate
// outside (`toGate`, -1 not known), freed otherwise and put back in the city
// when he comes again; by the gate (EscortIntoCity=0) - while still `shown`
// and near, never put back once freed.
inline bool EscortKitWanted(bool intoCity, bool shown, bool playerInCity, float toGate) {
	const bool nearGate = toGate >= 0.0f && toGate <= kCampKitKeepUnits;
	return intoCity ? playerInCity || nearGate : shown && nearGate;
}

// The engine's own package types that hold an escorted one (TESPackage's
// type byte at +0x20). Read off the EXE on 2026-11-14: GetCurrentAIPackage's
// handler 0x004F78B0 switches on that byte over 33 types (its jump table at
// 0x004F7A6C) and returns the numbers the CS wiki lists for the command - the
// byte 18 gives 6, DIALOGUE; the byte 20 gives 12, GREET.
inline constexpr UInt8 kPackageTypeDialogue = 18;
inline constexpr UInt8 kPackageTypeGreet = 20;
// A reset of the same one at the soonest this long after the last: a reset
// that frees nobody shall not blink him in a row.
inline constexpr float kEscortResetGapSeconds = 30.0f;
inline constexpr float kEscortNeverReset = 1.0e9f;  // `sinceReset` of one never reset

// Whether an escorted one is reset (disabled, enabled a second later, then
// given the follow): it runs a package the engine made (its ID's top byte
// 0xFF) and is not fighting - harness 2026-11-09: the four who stayed ran
// such a package, shared by pairs (two talking, type 18, an interrupt
// AddScriptPackage does not break; Nadi, 2026-11-09: "a"). Not for the
// engine's greeting: Nadi's Windows log of 2026-11-13 has the last one alive
// of seven, his six dead about him, reset seventeen times in a row, a new
// greeting each time, and left behind all the same - the reset frees nobody
// there; whether it kept the greetings coming is not known. And not within
// kEscortResetGapSeconds of the same one's last reset.
inline bool EscortResets(UInt32 packageId, UInt32 followId, bool inCombat, UInt8 type, float sinceReset) {
	return packageId != 0 && packageId != followId && (packageId >> 24) == 0xFF && !inCombat &&
	       type != kPackageTypeGreet && sinceReset >= kEscortResetGapSeconds;
}

// The co-save record: type "CAMP", version 2 (the dynamic camps) - the last
// day rolled, then per camp of kCampMost: status, set, seen, a pad byte,
// roster and lost (16 bits each), the walkers left, the coming attack's day,
// the toll's day and its carry (floats as their bits), the place (16 bits)
// and two pad bytes. Version 1 (the twelve fixed camps) is not read: its
// camps stood elsewhere, the camps are rolled anew.
// Version 3 adds the escort: its camp and gate (-1 none), those set out,
// the day it arrived and its kit; a version 2 image reads with none.
inline constexpr UInt32 kCampRecordType = 0x43414D50;  // 'CAMP'
inline constexpr UInt32 kCampRecordVersion = 3;
inline constexpr UInt32 kCampBytes = 28;
inline constexpr UInt32 kCampImageV2Bytes = 4 + kCampMost * kCampBytes;
inline constexpr UInt32 kCampEscortBytes = 20;
inline constexpr UInt32 kCampImageBytes = kCampImageV2Bytes + kCampEscortBytes;

namespace camp_detail {

inline UInt32 Bits(float f) {
	UInt32 bits = 0;
	std::memcpy(&bits, &f, sizeof(bits));
	return bits;
}

inline float Float(UInt32 bits) {
	float f = 0.0f;
	std::memcpy(&f, &bits, sizeof(f));
	return f;
}

}  // namespace camp_detail

inline UInt32 CampImage(const CampsState& st, UInt8* out, UInt32 capacity) {
	if (capacity < kCampImageBytes) {
		return 0;
	}
	Put32(out, static_cast<UInt32>(st.lastDay));
	for (UInt32 i = 0; i < kCampMost; ++i) {
		const Camp& c = st.camps[i];
		UInt8* at = out + 4 + i * kCampBytes;
		at[0] = static_cast<UInt8>(c.status);
		at[1] = c.set ? 1 : 0;
		at[2] = c.seen ? 1 : 0;
		at[3] = 0;
		Put32(at + 4, static_cast<UInt32>(c.roster) | (static_cast<UInt32>(c.lost) << 16));
		Put32(at + 8, c.walkersLeft);
		Put32(at + 12, camp_detail::Bits(c.startAt));
		Put32(at + 16, camp_detail::Bits(c.tollFrom));
		Put32(at + 20, camp_detail::Bits(c.carry));
		Put32(at + 24, c.place);
	}
	UInt8* e = out + kCampImageV2Bytes;
	Put32(e, static_cast<UInt32>(st.escort.camp));
	Put32(e + 4, static_cast<UInt32>(st.escort.gate));
	Put32(e + 8, st.escort.started);
	Put32(e + 12, camp_detail::Bits(st.escort.arrivedAt));
	Put32(e + 16, static_cast<UInt32>(st.escort.kit));
	return kCampImageBytes;
}

// `st` emptied first; false when too short (nothing read then).
inline bool ReadCampImage(CampsState& st, const UInt8* in, UInt32 length) {
	st = CampsState{};
	if (length < kCampImageV2Bytes) {
		return false;
	}
	st.lastDay = static_cast<int>(Get32(in));
	const UInt32 slots = (1u << kCampSlots) - 1u;
	for (UInt32 i = 0; i < kCampMost; ++i) {
		Camp& c = st.camps[i];
		const UInt8* at = in + 4 + i * kCampBytes;
		c.status = at[0] <= static_cast<UInt8>(CampStatus::Left) ? static_cast<CampStatus>(at[0]) : CampStatus::Living;
		c.set = at[1] != 0;
		c.seen = at[2] != 0;
		const UInt32 masks = Get32(at + 4);
		c.roster = static_cast<UInt16>(masks & slots);
		c.lost = static_cast<UInt16>((masks >> 16) & c.roster);
		c.walkersLeft = Get32(at + 8);
		c.startAt = camp_detail::Float(Get32(at + 12));
		c.tollFrom = camp_detail::Float(Get32(at + 16));
		c.carry = camp_detail::Float(Get32(at + 20));
		c.place = static_cast<UInt16>(Get32(at + 24) & 0xFFFF);
	}
	if (length >= kCampImageBytes) {
		const UInt8* e = in + kCampImageV2Bytes;
		const int camp = static_cast<int>(Get32(e));
		if (camp >= 0 && camp < static_cast<int>(kCampMost)) {
			st.escort.camp = camp;
			st.escort.gate = static_cast<int>(Get32(e + 4));
			st.escort.started = Get32(e + 8);
			st.escort.arrivedAt = camp_detail::Float(Get32(e + 12));
			const int kit = static_cast<int>(Get32(e + 16));
			st.escort.kit = kit >= 0 && kit < static_cast<int>(kCampKits) ? kit : -1;
		}
	}
	return true;
}

}  // namespace walkers
