#pragma once

// Noise draws them (docs/walkers-spec.md 4.20.2; designed with Nadi
// 2026-11-05 - "go 5" the same day, in the INI and LINK, on by default): a
// blow that lands, a spell cast and a thing OBVR throws are heard by every
// horde (4.11) with a walker within Radius of it. The group's marker is set
// on the spot; the leader walks there, its followers after it; there they
// search it - the marker hopping about the spot - for SearchSeconds, then
// they go on as before (back to their gate when they held one). A walker
// that sees the player fights as ever: the engine's combat makes it unfit
// and it drops out of its group (HordeLogic.h).
//
// The bookkeeping is HordeLogic.h's (HearNoise, StepHorde); this file holds
// the settings and OBVR's message.

#include <cmath>
#include <cstring>

#include "Types.h"

namespace walkers {

struct NoiseSettings {
	bool enabled = true;         // [Noise] Enabled
	float radius = 1200.0f;      // [Noise] Radius: heard by a horde with a walker this near (Nadi: "die sind dumpf")
	float searchSeconds = 30.0f; // [Noise] SearchSeconds: how long they search the spot
	float travelSeconds = 60.0f; // [Noise] TravelSeconds: not there by then - they search where they are
	float searchUnits = 400.0f;  // the marker hops within this of the spot; a noise this near the last is the same one
	float hopSeconds = 10.0f;    // a hop this often while they search
};

// OBVR's 'WNOI' (a thing thrown, where it landed or broke): x, y, z as
// floats, then a kind word (1: thrown). The worldspace is the player's -
// the throw is the player's own.
inline constexpr UInt32 kNoiseMessage = 0x494F4E57;  // 'WNOI'
inline constexpr UInt32 kNoiseMessageBytes = 16;
inline constexpr UInt32 kNoiseThrown = 1;

inline bool NoiseMessageOf(const void* data, UInt32 length, float& x, float& y, float& z) {
	if (data == nullptr || length != kNoiseMessageBytes) {
		return false;
	}
	float v[3];
	UInt32 kind = 0;
	std::memcpy(v, data, sizeof(v));
	std::memcpy(&kind, static_cast<const UInt8*>(data) + 12, sizeof(kind));
	if (kind != kNoiseThrown || !std::isfinite(v[0]) || !std::isfinite(v[1]) || !std::isfinite(v[2])) {
		return false;
	}
	x = v[0];
	y = v[1];
	z = v[2];
	return true;
}

}  // namespace walkers
