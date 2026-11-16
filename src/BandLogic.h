#pragma once

// War bands out of the open Oblivion gates - step 3 of docs/walkers-spec.md
// 4.22 (`[Invaders] OnlyFromGates=1`), process-free; built 2026-11-15 (Nadi: "go
// 1-7"), not wired into the game yet - the wiring reads the gates' flags
// (GateLogic.h) and places a band's actors, which needs a harness run.
//
// OnlyFromGates 0: the invaders come everywhere, through the spawn replacement, as
// now. OnlyFromGates 1: the world keeps the game's spawns; every open gate sends a
// band every BandEveryHours game hours toward the nearest city. A band far
// from the player is a number only (the performance rule, 4.11c): its gate,
// its city, its size and where it has got to. Within NearUnits of the player
// the caller puts it into the world (Near) - from then on it is the hordes'
// business (HordeLogic.h). A far band that reaches its city is the siege's
// pressure (Arrived). Closing a gate ends its far bands (Ended) and its
// clock - every gate closed is one source less (4.22). Before any gate is
// open nothing comes (a), or, with EverywhereUntilGatesOpen=1, the spawn
// replacement as with OnlyFromGates 0 (c).

#include <cmath>

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
	InvaderOrigin origin = InvaderOrigin::Everywhere;  // OnlyFromGates (named Origin until 2026-11-16)
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
};

// The spawn replacement on: OnlyFromGates 0, or 1 with no gate open and the
// fallback (c) chosen.
inline bool ReplacementOn(const BandSettings& s, UInt32 openGates) {
	return s.origin == InvaderOrigin::Everywhere || (s.everywhereUntilGatesOpen && openGates == 0);
}

// Bands out of the gates: OnlyFromGates 1 and at least one gate open.
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
};

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

}  // namespace walkers
