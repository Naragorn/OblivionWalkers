#pragma once

// War bands out of the open Oblivion gates - step 3 of docs/walkers-spec.md
// 4.22 (`[Invaders] OnlyFromOblivionGates=1`), process-free; built 2026-11-15 (Nadi: "go
// 1-7"), not wired into the game yet - the wiring reads the gates' flags
// (GateLogic.h) and places a band's actors, which needs a harness run.
//
// OnlyFromOblivionGates 0: the invaders come everywhere, through the spawn replacement, as
// now. OnlyFromOblivionGates 1: the world keeps the game's spawns; every open gate sends a
// band every BandEveryHours game hours toward the nearest city. A band far
// from the player is a number only (the performance rule, 4.11c): its gate,
// its city, its size and where it has got to. Within NearUnits of the player
// the caller puts it into the world (Near) - from then on it is the hordes'
// business (HordeLogic.h). A far band that reaches its city is the siege's
// pressure (Arrived). Closing a gate ends its far bands (Ended) and its
// clock - every gate closed is one source less (4.22). Before any gate is
// open nothing comes (a), or, with EverywhereUntilGatesOpen=1, the spawn
// replacement as with OnlyFromOblivionGates 0 (c).

#include <cmath>
#include <cstring>

#include "Bytes.h"
#include "Types.h"

namespace walkers {

enum class InvaderOrigin : UInt8 { Everywhere = 0, Gates = 1 };

inline InvaderOrigin OriginFromIni(int value) { return value == 1 ? InvaderOrigin::Gates : InvaderOrigin::Everywhere; }

inline constexpr UInt32 kBandsMost = 32;       // far bands at once
inline constexpr UInt32 kBandGatesMost = 128;  // OblivionGates.h holds 103
inline constexpr UInt32 kBandEventsMost = 64;
// The gates' flags and the far bands are looked at this often (seconds;
// Nadi, 2026-11-16: once a second was too often). A Near band's placing
// keeps its own once-a-second pace.
inline constexpr float kGateLookSeconds = 5.0f;
// The ESP's marker a Near band is placed at (tools/esp-write.py, in the
// horde markers' holding cell; a free ID after the invaders' lists).
inline constexpr UInt32 kBandMarkerLocalId = 0xA95;
// A band is never placed nearer than this to the player: it comes from
// somewhere, it does not appear before his eyes.
inline constexpr float kBandPlaceLeastUnits = 3000.0f;

// Where a Near band's actors are placed, as an offset from the player: the
// band's own spot, pushed out to kBandPlaceLeastUnits along the same line.
inline void BandPlaceOffset(float bx, float by, float px, float py, float& dx, float& dy) {
	dx = bx - px;
	dy = by - py;
	const float d = std::sqrt(dx * dx + dy * dy);
	if (d < 1.0f) {
		dx = 0.0f;
		dy = kBandPlaceLeastUnits;
		return;
	}
	if (d < kBandPlaceLeastUnits) {
		dx *= kBandPlaceLeastUnits / d;
		dy *= kBandPlaceLeastUnits / d;
	}
}

struct BandSettings {
	InvaderOrigin origin = InvaderOrigin::Everywhere;  // OnlyFromOblivionGates (named Origin, then OnlyFromGates, until 2026-11-16)
	bool everywhereUntilGatesOpen = false;              // EverywhereUntilGatesOpen (Nadi, 2026-11-14)
	float everyHours = 12.0f;                           // BandEveryHours: game hours between a gate's bands
	UInt32 sizeMin = 4;                                 // BandSizeMin
	UInt32 sizeMax = 9;                                 // BandSizeMax
	UInt32 bandsMost = kBandsMost;                      // BandsMost: far bands at once
	// A band's pace on the map, units per game hour - a guess to be measured
	// (a walk of ~120 units/s and the default timescale 30 give ~14400).
	float unitsPerHour = 10000.0f;
	// Nearer than this to the player a band comes into the world - inside
	// the loaded grid of 5x5 (two cells each way, 8192), so its actors are
	// placed into loaded cells; past the fog of a VR view in most weather.
	float nearUnits = 6000.0f;
	// [Invaders] TestBandSeconds (INI only, a test switch; harness
	// daedra-band.txt): this many seconds after a load one band is placed
	// before the player whatever the gates, through the same path as a Near
	// band. 0: off.
	float testBandSeconds = 0.0f;
	// [Invaders] TestKillLeaderSeconds (INI only, a test switch; harness
	// daedra-leader.txt): this many seconds after a band's leader was found
	// the plugin kills him by console - the band breaks up (idea 1). 0: off.
	float testKillLeaderSeconds = 0.0f;
	// Test switches (harness/gate-quest.txt, 4.22a step 6): this gate read
	// open for TestGateOpenSeconds from the first look, then shut (-1 none).
	int testGateOpen = -1;
	float testGateOpenSeconds = 0.0f;
	// A band that reaches its city raises that city's daily siege chance
	// (4.12 ChancePerDayPercent) by this many points - the gates feed the
	// sieges (Nadi, 2026-11-16: "go 3"); the points fade by
	// BandSiegeFadePerDay a game day.
	UInt32 siegePercentPerBand = 5;     // BandSiegePercent
	float siegeFadePercentPerDay = 5.0f;  // BandSiegeFadePerDay
};

inline bool TestKillLeaderDue(float seconds, float sinceFound, bool done) {
	return seconds > 0.0f && !done && sinceFound >= seconds;
}

inline constexpr UInt32 kBandCitiesMost = 8;  // CityGates.h City::Count
inline constexpr UInt32 kSiegePressureMost = 90;

// The spawn replacement on: OnlyFromOblivionGates 0, or 1 with no gate open and the
// fallback (c) chosen.
inline bool ReplacementOn(const BandSettings& s, UInt32 openGates) {
	return s.origin == InvaderOrigin::Everywhere || (s.everywhereUntilGatesOpen && openGates == 0);
}

// Bands out of the gates: OnlyFromOblivionGates 1 and at least one gate open.
inline bool BandsOn(const BandSettings& s, UInt32 openGates) {
	return s.origin == InvaderOrigin::Gates && openGates > 0;
}

inline UInt32 BandSize(UInt32 sizeMin, UInt32 sizeMax, UInt32 roll) {
	const UInt32 low = sizeMin < 1 ? 1 : sizeMin;
	if (sizeMax <= low) {
		return low;
	}
	return low + roll % (sizeMax - low + 1);
}

struct BandGate {
	float x, y;
};
struct BandCity {
	float x, y;
	UInt8 city = 0;  // CityGates.h City: the siege's pressure goes to it
};

struct FarBand {
	UInt16 gate;
	UInt8 city;
	UInt8 size;
	float x, y;
};

struct BandState {
	FarBand bands[kBandsMost] = {};
	UInt32 count = 0;
	float hoursToNext[kBandGatesMost] = {};
	bool wasOpen[kBandGatesMost] = {};
	float pressure[kBandCitiesMost] = {};  // points on a city's daily siege chance
};

inline void AddSiegePressure(BandState& st, const BandSettings& s, UInt32 city) {
	if (city < kBandCitiesMost) {
		const float p = st.pressure[city] + static_cast<float>(s.siegePercentPerBand);
		st.pressure[city] = p > static_cast<float>(kSiegePressureMost) ? static_cast<float>(kSiegePressureMost) : p;
	}
}

// The whole points a city's daily siege chance is up by now.
inline UInt32 SiegePressure(const BandState& st, UInt32 city) {
	return city < kBandCitiesMost && st.pressure[city] > 0.0f ? static_cast<UInt32>(st.pressure[city]) : 0;
}

enum class BandEventKind : UInt8 {
	Formed,   // a band left its gate
	Near,     // within NearUnits of the player: the caller places its actors
	Arrived,  // a far band reached its city: the siege's pressure
	Ended,    // its gate was closed: the far band is gone
};

struct BandEvent {
	BandEventKind kind;
	UInt16 gate;
	UInt8 city;
	UInt8 size;
	float x, y;
};

namespace band_detail {

inline float Dist2(float ax, float ay, float bx, float by) {
	const float dx = ax - bx;
	const float dy = ay - by;
	return dx * dx + dy * dy;
}

inline UInt8 NearestCity(const BandCity* cities, UInt32 cityCount, float x, float y) {
	UInt32 best = 0;
	for (UInt32 c = 1; c < cityCount; ++c) {
		if (Dist2(cities[c].x, cities[c].y, x, y) < Dist2(cities[best].x, cities[best].y, x, y)) {
			best = c;
		}
	}
	return static_cast<UInt8>(best);
}

struct Out {
	BandEvent* events;
	UInt32 room;
	UInt32 n = 0;
	void Add(BandEventKind kind, const FarBand& b) {
		if (n < room) {
			events[n++] = BandEvent{kind, b.gate, b.city, b.size, b.x, b.y};
		}
	}
};

inline void Remove(BandState& st, UInt32 i) {
	for (UInt32 k = i + 1; k < st.count; ++k) {
		st.bands[k - 1] = st.bands[k];
	}
	--st.count;
}

inline bool IsNear(const BandSettings& s, const FarBand& b, float px, float py) {
	return Dist2(b.x, b.y, px, py) < s.nearUnits * s.nearUnits;
}

}  // namespace band_detail

// One step of `hours` game hours. `open[g]` whether gate g is open now
// (GateLogic.h GateOpen), `roll` the caller's random number for the sizes.
// Writes at most `room` events; returns how many.
inline UInt32 StepBands(BandState& st, const BandSettings& s, const BandGate* gates, const bool* open,
                        UInt32 gateCount, const BandCity* cities, UInt32 cityCount, float hours, float px, float py,
                        UInt32 roll, BandEvent* events, UInt32 room) {
	using namespace band_detail;
	Out out{events, room};
	const UInt32 nGates = gateCount < kBandGatesMost ? gateCount : kBandGatesMost;
	if (s.origin != InvaderOrigin::Gates || cityCount == 0) {
		return 0;
	}
	// 0. The pressure fades.
	const float fade = s.siegeFadePercentPerDay * (hours > 0.0f ? hours : 0.0f) / 24.0f;
	for (float& p : st.pressure) {
		p = p > fade ? p - fade : 0.0f;
	}
	// 1. A closed gate ends its far bands.
	for (UInt32 i = 0; i < st.count;) {
		const UInt16 g = st.bands[i].gate;
		if (g < nGates && !open[g]) {
			out.Add(BandEventKind::Ended, st.bands[i]);
			Remove(st, i);
		} else {
			++i;
		}
	}
	// 2. The far bands walk on.
	const float step = s.unitsPerHour * (hours > 0.0f ? hours : 0.0f);
	for (UInt32 i = 0; i < st.count;) {
		FarBand& b = st.bands[i];
		const BandCity& c = cities[b.city < cityCount ? b.city : 0];
		const float left2 = Dist2(b.x, b.y, c.x, c.y);
		if (left2 <= step * step) {
			b.x = c.x;
			b.y = c.y;
			out.Add(BandEventKind::Arrived, b);
			AddSiegePressure(st, s, c.city);
			Remove(st, i);
			continue;
		}
		if (step > 0.0f) {
			const float r = std::sqrt(left2);
			b.x += (c.x - b.x) / r * step;
			b.y += (c.y - b.y) / r * step;
		}
		if (IsNear(s, b, px, py)) {
			out.Add(BandEventKind::Near, b);
			Remove(st, i);
			continue;
		}
		++i;
	}
	// 3. The open gates' clocks.
	const UInt32 most = s.bandsMost < kBandsMost ? s.bandsMost : kBandsMost;
	for (UInt32 g = 0; g < nGates; ++g) {
		if (!open[g]) {
			st.wasOpen[g] = false;
			continue;
		}
		bool due = false;
		if (!st.wasOpen[g]) {
			st.wasOpen[g] = true;
			due = true;
			st.hoursToNext[g] = s.everyHours;
		} else {
			st.hoursToNext[g] -= hours;
			if (st.hoursToNext[g] <= 0.0f) {
				due = true;
				st.hoursToNext[g] += s.everyHours;
				if (st.hoursToNext[g] <= 0.0f) {
					st.hoursToNext[g] = s.everyHours;  // a long jump (a wait, a load) forms one, not a burst
				}
			}
		}
		if (!due || st.count >= most) {
			continue;
		}
		FarBand b{static_cast<UInt16>(g), NearestCity(cities, cityCount, gates[g].x, gates[g].y),
		          static_cast<UInt8>(BandSize(s.sizeMin, s.sizeMax, roll + g)), gates[g].x, gates[g].y};
		out.Add(BandEventKind::Formed, b);
		if (IsNear(s, b, px, py)) {
			out.Add(BandEventKind::Near, b);
		} else {
			st.bands[st.count++] = b;
		}
	}
	return out.n;
}

// A band member placed in the world (Bands.cpp): its reference, the band it
// came with (a session's count) and whether it leads it (idea 1).
struct BandMember {
	UInt32 ref;
	UInt8 band;
	bool leader;
};
inline constexpr UInt32 kBandMembersMost = 128;

// The co-save record 'BAND' (Nadi, 2026-11-16: "go 1" - the bands outlive a
// save and a load): the far bands, the gates' clocks, the pressure and the
// members by their saved reference IDs.
inline constexpr UInt32 kBandRecordType = 0x42414E44;  // 'BAND'
inline constexpr UInt32 kBandImageVersion = 1;
inline constexpr UInt32 kBandImageMost = 4 + kBandsMost * 12 + 4 + kBandGatesMost * 5 + kBandCitiesMost * 4 + 4 +
                                         kBandMembersMost * 6;

namespace band_detail {
inline void PutF(UInt8* out, float v) {
	UInt32 u;
	std::memcpy(&u, &v, 4);
	Put32(out, u);
}
inline float GetF(const UInt8* in) {
	const UInt32 u = Get32(in);
	float v;
	std::memcpy(&v, &u, 4);
	return v;
}
}  // namespace band_detail

inline UInt32 BandImage(const BandState& st, const BandMember* members, UInt32 memberCount, UInt8* out,
                        UInt32 capacity) {
	using namespace band_detail;
	const UInt32 members_ = memberCount < kBandMembersMost ? memberCount : kBandMembersMost;
	const UInt32 length = 4 + st.count * 12 + 4 + kBandGatesMost * 5 + kBandCitiesMost * 4 + 4 + members_ * 6;
	if (capacity < length) {
		return 0;
	}
	UInt32 at = 0;
	Put32(out + at, st.count);
	at += 4;
	for (UInt32 i = 0; i < st.count; ++i) {
		const FarBand& b = st.bands[i];
		out[at] = static_cast<UInt8>(b.gate & 0xFF);
		out[at + 1] = static_cast<UInt8>(b.gate >> 8);
		out[at + 2] = b.city;
		out[at + 3] = b.size;
		PutF(out + at + 4, b.x);
		PutF(out + at + 8, b.y);
		at += 12;
	}
	Put32(out + at, kBandGatesMost);
	at += 4;
	for (UInt32 g = 0; g < kBandGatesMost; ++g) {
		PutF(out + at, st.hoursToNext[g]);
		out[at + 4] = st.wasOpen[g] ? 1 : 0;
		at += 5;
	}
	for (UInt32 c = 0; c < kBandCitiesMost; ++c) {
		PutF(out + at, st.pressure[c]);
		at += 4;
	}
	Put32(out + at, members_);
	at += 4;
	for (UInt32 m = 0; m < members_; ++m) {
		Put32(out + at, members[m].ref);
		out[at + 4] = members[m].band;
		out[at + 5] = members[m].leader ? 1 : 0;
		at += 6;
	}
	return at;
}

// False (and `st` fresh, no members) when the record is of another version
// or cut short. A member whose saved ID resolves to nothing is dropped.
template <typename ResolveFn>
inline bool ReadBandImage(BandState& st, BandMember* members, UInt32& memberCount, const UInt8* in, UInt32 length,
                          UInt32 version, ResolveFn resolve) {
	using namespace band_detail;
	st = BandState{};
	memberCount = 0;
	if (version != kBandImageVersion || length < 4) {
		return false;
	}
	UInt32 at = 0;
	const UInt32 count = Get32(in);
	at += 4;
	if (count > kBandsMost || length < at + count * 12 + 4) {
		return false;
	}
	BandState read;
	read.count = count;
	for (UInt32 i = 0; i < count; ++i) {
		FarBand& b = read.bands[i];
		b.gate = static_cast<UInt16>(in[at] | (in[at + 1] << 8));
		b.city = in[at + 2];
		b.size = in[at + 3];
		b.x = GetF(in + at + 4);
		b.y = GetF(in + at + 8);
		at += 12;
	}
	const UInt32 gates = Get32(in + at);
	at += 4;
	if (gates != kBandGatesMost || length < at + gates * 5 + kBandCitiesMost * 4 + 4) {
		return false;
	}
	for (UInt32 g = 0; g < gates; ++g) {
		read.hoursToNext[g] = GetF(in + at);
		read.wasOpen[g] = in[at + 4] != 0;
		at += 5;
	}
	for (UInt32 c = 0; c < kBandCitiesMost; ++c) {
		read.pressure[c] = GetF(in + at);
		at += 4;
	}
	const UInt32 saved = Get32(in + at);
	at += 4;
	if (saved > kBandMembersMost || length < at + saved * 6) {
		return false;
	}
	st = read;
	for (UInt32 m = 0; m < saved; ++m) {
		UInt32 loaded = 0;
		if (resolve(Get32(in + at), loaded)) {
			members[memberCount++] = BandMember{loaded, in[at + 4], in[at + 5] != 0};
		}
		at += 6;
	}
	return true;
}

}  // namespace walkers
