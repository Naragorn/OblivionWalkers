#pragma once

// Nights are worse (docs/walkers-spec.md 4.20.5; designed with Nadi
// 2026-11-05, "go 3" the same day - in the INI and LINK, on by default): from
// StartHour to EndHour the walkers are SpeedMultiplier times as quick, the
// spawns give SpawnPercent as many, the hordes' marker lies HordeLeadPercent
// as far ahead; each rises over RampHours after the night begins and falls
// over RampHours before it ends. The grab's chance and the siege stay as
// they are. Rise.cpp carries it out: the walker records' pace (4.4), the
// variant list's counts (4.6), the horde's lead (Horde.cpp) written anew
// whenever the step below changes, and a rumor said while it is on.

#include <cmath>

#include "Types.h"

namespace walkers {

struct NightSettings {
	bool enabled = true;           // [Night] Enabled
	float speedMultiplier = 1.3f;  // [Night] SpeedMultiplier: the pace at full night, times the day's
	UInt32 spawnPercent = 150;     // [Night] SpawnPercent: a spawn's walkers at full night
	UInt32 hordeLeadPercent = 150; // [Night] HordeLeadPercent: the horde's marker this much farther ahead
	float startHour = 20.0f;       // [Night] StartHour
	float endHour = 6.0f;          // [Night] EndHour
	float rampHours = 1.0f;        // [Night] RampHours: the rise after the start, the fall before the end
};

// The ramp in steps: the records and lists are written anew only when the
// step changes - twelve a ramp hour, every five game minutes.
inline constexpr UInt32 kNightSteps = 12;
// A spawn's walkers at night at most - the day's most is ReplaceLogic.h's
// kWalkersPerSpawnMost (10); 1.5 times that, rounded up to 20.
inline constexpr UInt32 kNightWalkersPerSpawnMost = 20;

inline float NightHourOfDay(float hour) {
	float h = std::fmod(hour, 24.0f);
	return h < 0.0f ? h + 24.0f : h;
}

// 0 by day, 1 at full night, between on the ramps. -1 (the clock unknown,
// the main menu) is day.
inline float NightShare(const NightSettings& s, float hour) {
	if (!s.enabled || hour < 0.0f) {
		return 0.0f;
	}
	const float start = NightHourOfDay(s.startHour);
	const float length = NightHourOfDay(NightHourOfDay(s.endHour) - start);
	if (length <= 0.0f) {
		return 0.0f;
	}
	const float t = NightHourOfDay(NightHourOfDay(hour) - start);
	if (t >= length) {
		return 0.0f;
	}
	float ramp = s.rampHours < 0.0f ? 0.0f : s.rampHours;
	ramp = ramp > length / 2.0f ? length / 2.0f : ramp;
	if (ramp <= 0.0f) {
		return 1.0f;
	}
	float share = t / ramp;
	const float fall = (length - t) / ramp;
	share = fall < share ? fall : share;
	return share > 1.0f ? 1.0f : share;
}

struct NightNow {
	UInt32 step = 0;           // 0 day .. kNightSteps full night
	float speedFactor = 1.0f;  // times the day's pace
	UInt32 spawnPercent = 100;
	float leadFactor = 1.0f;   // times the horde's LeadUnits
};

// What the night does at `hour`, in whole steps.
inline NightNow NightAt(const NightSettings& s, float hour) {
	NightNow n;
	n.step = static_cast<UInt32>(NightShare(s, hour) * static_cast<float>(kNightSteps) + 0.5f);
	const float share = static_cast<float>(n.step) / static_cast<float>(kNightSteps);
	const float speed = s.speedMultiplier > 0.0f ? s.speedMultiplier : 1.0f;
	n.speedFactor = 1.0f + (speed - 1.0f) * share;
	const float spawn = static_cast<float>(s.spawnPercent);
	n.spawnPercent = static_cast<UInt32>(100.0f + (spawn - 100.0f) * share + 0.5f);
	n.leadFactor = 1.0f + (static_cast<float>(s.hordeLeadPercent) / 100.0f - 1.0f) * share;
	return n;
}

// A count slot of the variant list at `percent`: rounded half up, at least
// 1, at most kNightWalkersPerSpawnMost; 0 stays 0.
inline UInt32 NightSpawnCount(UInt32 count, UInt32 percent) {
	if (count == 0) {
		return 0;
	}
	const UInt32 n = (count * percent + 50) / 100;
	return n < 1 ? 1 : (n > kNightWalkersPerSpawnMost ? kNightWalkersPerSpawnMost : n);
}

}  // namespace walkers
