#pragma once

// A city's siege, process-free (docs/walkers-spec.md 4.12; Nadi, 2026-10-26
// and 2026-10-27: a horde before one gate, outside the walls only, whether
// the player is there or not, the gate shut until it is over; 2026-11-04:
// "ja mach 1-4", the guards' toll while the player is away 2 walkers a game
// hour).
//
// A siege begins by a chance a game day per city (ChancePerDayPercent) or
// where a horde the player saw holds at a gate (BeginSiegeAt); it has
// WalkersMin..WalkersMax walkers, of them WalkersAtGate before the gate at
// once. While the player is away the guards cut GuardKillsPerHour down; at
// the gate the fight is seen and every walker ended there counts. None left:
// the end, then PauseDays before that city's next. The besieged gate's two
// doors are locked by the siege - a door locked before is left as it was,
// and only the doors the siege locked are unlocked at the end.

#include <cmath>
#include <cstring>

#include "Bytes.h"
#include "CityGates.h"
#include "Types.h"

namespace walkers {

inline constexpr UInt32 kSiegeCities = static_cast<UInt32>(City::Count);
inline constexpr UInt32 kSiegeAtGateLeast = 12;
inline constexpr UInt32 kSiegeAtGateMost = 100;  // the ESP's pool of siege walkers

// [Siege] in OblivionWalkers.ini.
struct SiegeSettings {
	bool enabled = true;
	UInt32 walkersAtGate = 24;  // before the gate at once, held to 12..100
	UInt32 chancePerDayPercent = 10;
	UInt32 walkersMin = 40;
	UInt32 walkersMax = 120;
	float pauseDays = 3.0f;
	float guardKillsPerHour = 2.0f;  // while the player is away
	// Test switches (harness/siege-lock.txt): a siege begun at this
	// kCityGates row at the first look (-1: never), and ended this many
	// seconds later (0: not by the switch).
	int testGate = -1;
	float testEndSeconds = 0.0f;
	// Test switch (harness/siege-door-read.txt): this door's GetLocked and
	// GetLockLevel logged every 5 s, with the player's cell (0: never).
	UInt32 testReadDoor = 0;
};

struct CitySiege {
	bool active = false;
	UInt8 gate = 0;          // kCityGates row
	UInt8 lockedByUs = 0;    // bit 0 the Tamriel-side door, bit 1 the city-side one
	UInt32 walkersLeft = 0;
	float tollFrom = -1.0f;  // the game day the guards' toll is counted from
	float pauseUntil = -1.0f;
};

struct SiegeState {
	CitySiege cities[kSiegeCities];
	int lastDay = -1;  // the last whole game day rolled for, -1 none yet
};

struct SiegeEvent {
	enum Kind : UInt8 { Begin, End };
	Kind kind;
	UInt32 city;
	UInt32 gate;
};

// `city`'s rows in kCityGates (the table keeps a city's gates together).
inline bool CityGates(City city, UInt32& first, UInt32& count) {
	count = 0;
	for (UInt32 i = 0; i < kCityGateCount; ++i) {
		if (kCityGates[i].city == city) {
			if (count == 0) {
				first = i;
			}
			++count;
		}
	}
	return count > 0;
}

namespace siege_detail {

inline UInt32 Walkers(const SiegeSettings& s, UInt32 roll) {
	const UInt32 least = s.walkersMin < s.walkersMax ? s.walkersMin : s.walkersMax;
	const UInt32 most = s.walkersMin < s.walkersMax ? s.walkersMax : s.walkersMin;
	const UInt32 walkers = least + roll % (most - least + 1);
	return walkers > 0 ? walkers : 1;
}

inline void Begin(CitySiege& c, UInt32 gate, UInt32 walkers, float now) {
	c.active = true;
	c.gate = static_cast<UInt8>(gate);
	c.lockedByUs = 0;
	c.walkersLeft = walkers;
	c.tollFrom = now;
}

inline UInt32 End(CitySiege& c, UInt32 city, const SiegeSettings& s, float now, SiegeEvent* out, UInt32 most) {
	c.active = false;
	c.walkersLeft = 0;
	c.pauseUntil = now + s.pauseDays;
	if (most == 0) {
		return 0;
	}
	out[0] = SiegeEvent{SiegeEvent::End, city, c.gate};
	return 1;
}

inline bool Paused(const CitySiege& c, float now) { return c.pauseUntil >= 0.0f && now < c.pauseUntil; }

}  // namespace siege_detail

// Once a frame or a second with game day `now`: a new whole day rolls each
// city not besieged nor paused once - however many days went by - by
// `roll()` (0..any): the chance, then the gate, then the walkers. The first
// look only notes the day. Returns the events written to `out`.
template <typename RollFn>
UInt32 StepSiegeDays(SiegeState& st, const SiegeSettings& s, float now, RollFn roll, SiegeEvent* out, UInt32 most) {
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
	for (UInt32 city = 0; s.enabled && city < kSiegeCities; ++city) {
		CitySiege& c = st.cities[city];
		if (c.active || siege_detail::Paused(c, now)) {
			continue;
		}
		if (roll() % 100 >= s.chancePerDayPercent) {
			continue;
		}
		UInt32 first = 0;
		UInt32 count = 0;
		CityGates(static_cast<City>(city), first, count);
		const UInt32 gate = first + roll() % (count > 0 ? count : 1);
		siege_detail::Begin(c, gate, siege_detail::Walkers(s, roll()), now);
		if (n < most) {
			out[n++] = SiegeEvent{SiegeEvent::Begin, city, gate};
		}
	}
	return n;
}

// A horde held at gate row `gate` (4.11b): that city besieged there, unless
// it is already or pauses. `walkerRoll`: the caller's draw for the walkers.
inline bool BeginSiegeAt(SiegeState& st, const SiegeSettings& s, UInt32 gate, float now, UInt32 walkerRoll) {
	if (!s.enabled || gate >= kCityGateCount) {
		return false;
	}
	CitySiege& c = st.cities[static_cast<UInt32>(kCityGates[gate].city)];
	if (c.active || siege_detail::Paused(c, now)) {
		return false;
	}
	siege_detail::Begin(c, gate, siege_detail::Walkers(s, walkerRoll), now);
	return true;
}

// The siege ended now, whatever is left (the test switch, Enabled=0).
inline UInt32 EndSiege(CitySiege& c, UInt32 city, const SiegeSettings& s, float now, SiegeEvent* out, UInt32 most) {
	return c.active ? siege_detail::End(c, city, s, now, out, most) : 0;
}

// The walkers before the gate at once: WalkersAtGate held to 12..100, never
// more than are left.
inline UInt32 WalkersAtGateNow(const CitySiege& c, const SiegeSettings& s) {
	if (!c.active) {
		return 0;
	}
	UInt32 at = s.walkersAtGate < kSiegeAtGateLeast ? kSiegeAtGateLeast : s.walkersAtGate;
	at = at > kSiegeAtGateMost ? kSiegeAtGateMost : at;
	return at < c.walkersLeft ? at : c.walkersLeft;
}

// The guards' toll at game day `now`: away, GuardKillsPerHour walkers a game
// hour since the toll was last counted; at the gate (`playerNear`) none -
// the walkers ended there count instead, and the toll starts again when the
// player leaves. Returns the end's event when none are left.
inline UInt32 StepSiegeToll(CitySiege& c, UInt32 city, const SiegeSettings& s, float now, bool playerNear,
                            SiegeEvent* out, UInt32 most) {
	if (!c.active || now < 0.0f) {
		return 0;
	}
	if (playerNear || c.tollFrom < 0.0f || s.guardKillsPerHour <= 0.0f) {
		c.tollFrom = now;
		return 0;
	}
	const float perDay = 24.0f * s.guardKillsPerHour;
	const float kills = std::floor((now - c.tollFrom) * perDay);
	if (kills < 1.0f) {
		return 0;
	}
	if (kills >= static_cast<float>(c.walkersLeft)) {
		return siege_detail::End(c, city, s, now, out, most);
	}
	c.walkersLeft -= static_cast<UInt32>(kills);
	c.tollFrom += kills / perDay;
	return 0;
}

// A walker of the siege ended at the gate (4.17's end, not a fall).
inline UInt32 SiegeWalkerEnded(CitySiege& c, UInt32 city, const SiegeSettings& s, float now, SiegeEvent* out,
                               UInt32 most) {
	if (!c.active) {
		return 0;
	}
	if (c.walkersLeft <= 1) {
		return siege_detail::End(c, city, s, now, out, most);
	}
	--c.walkersLeft;
	return 0;
}

// The siege's walkers before the gate: rows of kSiegeRowLength across the
// way out, the first kSiegeStandOff units out from the door (past where the
// gate guards come out, GuardLogic.h GuardSpot), each next row farther out.
inline constexpr float kSiegeStandOff = 900.0f;
inline constexpr float kSiegeSpacing = 90.0f;
inline constexpr float kSiegeRowSpacing = 110.0f;
inline constexpr UInt32 kSiegeRowLength = 8;

inline void SiegeSpot(const CityGate& gate, UInt32 slot, float& x, float& y) {
	float dx = gate.outX - gate.x;
	float dy = gate.outY - gate.y;
	const float length = std::sqrt(dx * dx + dy * dy);
	if (length < 1.0f) {
		dx = 0.0f;
		dy = 1.0f;
	} else {
		dx /= length;
		dy /= length;
	}
	const float across = (static_cast<float>(slot % kSiegeRowLength) - 0.5f * (kSiegeRowLength - 1)) * kSiegeSpacing;
	const float out = kSiegeStandOff + static_cast<float>(slot / kSiegeRowLength) * kSiegeRowSpacing;
	// Across: the way out turned a quarter to the right.
	x = gate.x + dx * out + dy * across;
	y = gate.y + dy * out - dx * across;
}

// Door 0 (Tamriel side) or 1 (city side) of the besieged gate: true when the
// siege locks it - it was not locked before - and noted so.
inline bool LockDoor(CitySiege& c, UInt32 door, bool wasLocked) {
	if (wasLocked || door > 1) {
		return false;
	}
	c.lockedByUs = static_cast<UInt8>(c.lockedByUs | (1u << door));
	return true;
}

// Whether the siege's end unlocks that door: only one it locked.
inline bool UnlocksDoor(const CitySiege& c, UInt32 door) { return door <= 1 && (c.lockedByUs & (1u << door)) != 0; }

// The co-save record: type "SIEG", version 1 - the last day rolled, then per
// city: active, gate, the doors locked, a pad byte, the walkers left, the
// toll's day and the pause's end (floats as their bits).
inline constexpr UInt32 kSiegeRecordType = 0x53494547;  // 'SIEG'
inline constexpr UInt32 kSiegeRecordVersion = 1;
inline constexpr UInt32 kSiegeCityBytes = 16;
inline constexpr UInt32 kSiegeImageBytes = 4 + kSiegeCities * kSiegeCityBytes;

namespace siege_detail {

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

}  // namespace siege_detail

inline UInt32 SiegeImage(const SiegeState& st, UInt8* out, UInt32 capacity) {
	if (capacity < kSiegeImageBytes) {
		return 0;
	}
	Put32(out, static_cast<UInt32>(st.lastDay));
	for (UInt32 i = 0; i < kSiegeCities; ++i) {
		const CitySiege& c = st.cities[i];
		UInt8* at = out + 4 + i * kSiegeCityBytes;
		at[0] = c.active ? 1 : 0;
		at[1] = c.gate;
		at[2] = c.lockedByUs;
		at[3] = 0;
		Put32(at + 4, c.walkersLeft);
		Put32(at + 8, siege_detail::Bits(c.tollFrom));
		Put32(at + 12, siege_detail::Bits(c.pauseUntil));
	}
	return kSiegeImageBytes;
}

// `st` emptied first; false when too short (nothing read then).
inline bool ReadSiegeImage(SiegeState& st, const UInt8* in, UInt32 length) {
	st = SiegeState{};
	if (length < kSiegeImageBytes) {
		return false;
	}
	st.lastDay = static_cast<int>(Get32(in));
	for (UInt32 i = 0; i < kSiegeCities; ++i) {
		CitySiege& c = st.cities[i];
		const UInt8* at = in + 4 + i * kSiegeCityBytes;
		c.active = at[0] != 0 && at[1] < kCityGateCount;
		c.gate = at[1] < kCityGateCount ? at[1] : 0;
		c.lockedByUs = static_cast<UInt8>(at[2] & 3);
		c.walkersLeft = Get32(at + 4);
		c.tollFrom = siege_detail::Float(Get32(at + 8));
		c.pauseUntil = siege_detail::Float(Get32(at + 12));
	}
	return true;
}

}  // namespace walkers
