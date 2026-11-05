#pragma once

// The survivors' camps' places (docs/walkers-spec.md 4.20.6), proposed by
// tools/camp-places.py off Oblivion.esm's roads on 2026-11-05: a road point at
// least 9000 units from every city gate, 2500 from every door and 2000 from
// every map marker, moved 450 units off the road to its flatter side, where
// the land within 400 units rises or falls no more than 90; each the farthest
// from those picked before, so they spread over the map. z is the land's
// height there, facing the way to the road (degrees, 0 north, clockwise). The
// names after the nearest map marker that is no Oblivion gate's. The ESP
// (tools/esp-write.py) lays each camp out about its place; Nadi picks or
// changes the places (the spec's "still to settle").

#include "Types.h"

namespace walkers {

struct CampPlace {
	float x;
	float y;
	float z;
	float facing;
	const char* name;
};

inline constexpr CampPlace kCampPlaces[] = {
	{-198312.0f, -16030.0f, 3624.0f, 115.0f, "near Hrota Cave"},
	{125216.0f, 125340.0f, 15432.0f, 207.0f, "near Walker Camp"},
	{108010.0f, -137793.0f, 1072.0f, 99.0f, "near Darkfathom Cave"},
	{-22371.0f, 25989.0f, 2872.0f, 115.0f, "near Ceyatatar"},
	{102978.0f, 15731.0f, 984.0f, 23.0f, "near Crestbridge Camp"},
	{15222.0f, 131484.0f, 20752.0f, 237.0f, "near Bruma Caverns"},
	{-86311.0f, 94036.0f, 16384.0f, 196.0f, "near Pillaged Mine"},
	{83104.0f, -58886.0f, 2248.0f, 205.0f, "near Bawn"},
	{-129196.0f, -17684.0f, 7152.0f, 290.0f, "near Mortal Camp"},
	{60115.0f, 81551.0f, 1032.0f, 71.0f, "near Fort Urasek"},
	{41023.0f, -7702.0f, 5552.0f, 14.0f, "near Mingo Cave"},
	{57633.0f, 166046.0f, 27584.0f, 239.0f, "near Dragonclaw Rock"},
};
inline constexpr UInt32 kCampCount = sizeof(kCampPlaces) / sizeof(kCampPlaces[0]);

}  // namespace walkers
