#pragma once

// The survivors' camps, process-free (docs/walkers-spec.md 4.20.6; designed
// with Nadi 2026-11-05, "go 4.20.6" the same day - in the INI and LINK, on by
// default). The first of the design's two steps: the camps and their defence;
// the escort to a city comes second.
//
// Twelve camps by the roads (CampPlaces.h), each with PeoplePerCamp refugees,
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

#include <cmath>
#include <cstring>

#include "Bytes.h"
#include "CampPlaces.h"
#include "Types.h"

namespace walkers {

// A camp's people in the ESP (tools/esp-write.py): six men, six women and a
// trader, slots 0..5, 6..11 and 12; the roster says which are in the camp.
inline constexpr UInt32 kCampMen = 6;
inline constexpr UInt32 kCampWomen = 6;
inline constexpr UInt32 kCampTraderSlot = 12;
inline constexpr UInt32 kCampSlots = 13;
// The ESP's records (tools/esp-write.py reads these lines): the refugees'
// fourteen bases (six men, six women, a man and a woman who trade), the
// merchant chest the traders sell from, the attack's two markers and travel
// packages, the pool of attacking walkers, then each camp's block - its
// thirteen people, its map marker and its eleven props.
inline constexpr UInt32 kCampBaseFirstLocalId = 0x9D9;
inline constexpr UInt32 kCampBaseCount = 14;
inline constexpr UInt32 kCampChestLocalId = 0x9E7;
inline constexpr UInt32 kCampMarkerFirstLocalId = 0x9E8;
inline constexpr UInt32 kCampTravelFirstLocalId = 0x9EA;
inline constexpr UInt32 kCampPoolFirstLocalId = 0x9EC;
inline constexpr UInt32 kCampFirstLocalId = 0xA04;
inline constexpr UInt32 kCampProps = 11;
inline constexpr UInt32 kCampBlock = 13 + 1 + kCampProps;  // people, the map marker, the props
inline constexpr UInt32 CampPersonLocalId(UInt32 camp, UInt32 slot) { return kCampFirstLocalId + camp * kCampBlock + slot; }
inline constexpr UInt32 CampMarkerLocalId(UInt32 camp) { return kCampFirstLocalId + camp * kCampBlock + 13; }
inline constexpr UInt32 CampPropLocalId(UInt32 camp, UInt32 prop) { return kCampFirstLocalId + camp * kCampBlock + 14 + prop; }

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

// The base of camp `camp`'s person in `slot`: the men and the women each
// turned by the camp's number, so the camps do not all look alike; the
// trader a man in the even camps, a woman in the odd ones.
inline constexpr UInt32 CampBaseIndex(UInt32 camp, UInt32 slot) {
	return slot < 6 ? (slot + camp) % 6 : (slot < 12 ? 6 + (slot - 6 + camp) % 6 : 12 + camp % 2);
}

inline constexpr UInt32 kCampPeopleLeast = 2;
inline constexpr UInt32 kCampPeopleMost = 6;
// The pool of attacking walkers: twelve at one camp at once, two camps.
inline constexpr UInt32 kCampShownMost = 2;
inline constexpr UInt32 kCampWalkersAtOnce = 12;
inline constexpr UInt32 kCampPool = kCampShownMost * kCampWalkersAtOnce;
// The player is at a camp within this of its place, outdoors in Tamriel: its
// people are loaded and fight (the exterior grid's two cells around).
inline constexpr float kCampSeenUnits = 6000.0f;
// Hours of the away toll rolled at one look at most (a week waited).
inline constexpr UInt32 kCampTollHoursMost = 24 * 7;

// [Survivors] in OblivionWalkers.ini.
struct CampSettings {
	bool enabled = true;                // Enabled
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
	// Test switch (harness/camp-defend.txt): camp N attacked at the first
	// look (-1: never).
	int testCamp = -1;
	// Test switch (harness/camp-probe.txt, CampProbeLogic.h): camp N's tent
	// and map marker moved by script and the player dropped onto both spots
	// (-1: never).
	int testProbe = -1;
};

enum class CampStatus : UInt8 { Living, Attacked, Overrun };

struct Camp {
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

struct CampsState {
	Camp camps[kCampCount];
	int lastDay = -1;
};

struct CampEvent {
	enum Kind : UInt8 { Begin, PersonLost, HeldAway, Defended, Overrun, Return };
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
	for (UInt32 i = 0; s.enabled && i < kCampCount; ++i) {
		Camp& c = st.camps[i];
		if (c.status == CampStatus::Living && c.startAt < 0.0f) {
			if (roll() % 100 < s.attackChancePercent) {
				const float at = static_cast<float>(day) + static_cast<float>(roll() % 24) / 24.0f;
				c.startAt = at < now ? now : at;
			}
		} else if (c.status == CampStatus::Overrun) {
			if (roll() % 100 < s.returnChancePercent) {
				c = Camp{};
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
	if (slot >= kCampSlots || c.status == CampStatus::Overrun) {
		return 0;
	}
	const UInt16 bit = static_cast<UInt16>(1u << slot);
	if ((c.roster & bit) == 0 || (c.lost & bit) != 0) {
		return 0;
	}
	c.lost = static_cast<UInt16>(c.lost | bit);
	if (CampAlive(c) > 0) {
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
		bool taken[kCampCount] = {};
		for (UInt32 k = 0; k < n; ++k) {
			taken[shown[k]] = true;
		}
		for (;;) {
			const UInt32 most = pass == 0 ? attackedMost : kCampShownMost;
			if (n >= most) {
				break;
			}
			int best = -1;
			for (UInt32 i = 0; i < kCampCount; ++i) {
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

inline void CampWalkerSpot(const CampPlace& place, bool attack, UInt32 slot, float& dx, float& dy) {
	const float pi = 3.14159265f;
	const float away = (place.facing + 180.0f) * pi / 180.0f;
	// An attack's: away from the road, then a thirtieth to either side in turn
	// (0, +1, -1, +2, -2 ...); an overrun camp's: all around.
	const float side = slot % 2 == 1 ? 1.0f : -1.0f;
	const float at = attack ? away + side * static_cast<float>((slot + 1) / 2) * (2.0f * pi / 30.0f)
	                        : static_cast<float>(slot) * (2.0f * pi / static_cast<float>(kCampWalkersAtOnce));
	const float r = attack ? kCampAttackUnits : kCampNestUnits;
	dx = std::sin(at) * r;
	dy = std::cos(at) * r;
}

// The co-save record: type "CAMP", version 1 - the last day rolled, then per
// camp: status, set, seen, a pad byte, roster and lost (16 bits each), the
// walkers left, the coming attack's day, the toll's day and its carry
// (floats as their bits).
inline constexpr UInt32 kCampRecordType = 0x43414D50;  // 'CAMP'
inline constexpr UInt32 kCampRecordVersion = 1;
inline constexpr UInt32 kCampBytes = 24;
inline constexpr UInt32 kCampImageBytes = 4 + kCampCount * kCampBytes;

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
	for (UInt32 i = 0; i < kCampCount; ++i) {
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
	}
	return kCampImageBytes;
}

// `st` emptied first; false when too short (nothing read then).
inline bool ReadCampImage(CampsState& st, const UInt8* in, UInt32 length) {
	st = CampsState{};
	if (length < kCampImageBytes) {
		return false;
	}
	st.lastDay = static_cast<int>(Get32(in));
	const UInt32 slots = (1u << kCampSlots) - 1u;
	for (UInt32 i = 0; i < kCampCount; ++i) {
		Camp& c = st.camps[i];
		const UInt8* at = in + 4 + i * kCampBytes;
		c.status = at[0] <= static_cast<UInt8>(CampStatus::Overrun) ? static_cast<CampStatus>(at[0]) : CampStatus::Living;
		c.set = at[1] != 0;
		c.seen = at[2] != 0;
		const UInt32 masks = Get32(at + 4);
		c.roster = static_cast<UInt16>(masks & slots);
		c.lost = static_cast<UInt16>((masks >> 16) & c.roster);
		c.walkersLeft = Get32(at + 8);
		c.startAt = camp_detail::Float(Get32(at + 12));
		c.tollFrom = camp_detail::Float(Get32(at + 16));
		c.carry = camp_detail::Float(Get32(at + 20));
	}
	return true;
}

}  // namespace walkers
