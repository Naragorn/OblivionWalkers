#pragma once

// The marching weather, process-free (docs/walkers-spec.md 4.22 idea 9; Nadi,
// 2026-11-16: "go 9"): a Daedra band on the march carries the gates' red sky
// a little way with it - the player, outdoors, sees an attack coming over the
// hill. The sky is Oblivion.esm's OblivionStormTamriel (000836D5, the gates'
// own; CS wiki SetWeather), set with the override flag while a band is
// within MarchWeatherDistance, let go (ReleaseWeatherOverride, the weather
// before set again) ReleaseSeconds after the last one went or at once when
// the player goes inside. Off by default: forcing the weather fights the
// other weather mods and the game's own scripts that set it.

#include "Types.h"

namespace walkers {

inline constexpr UInt32 kMarchWeatherForm = 0x000836D5;  // OblivionStormTamriel

// [Invaders] in OblivionWalkers.ini.
struct MarchWeatherSettings {
	bool enabled = false;          // MarchWeather
	float units = 12000.0f;        // MarchWeatherDistance: a Daedra band this near (some 170 m)
	float releaseSeconds = 20.0f;  // gone this long: the sky let go
};

struct MarchWeatherState {
	bool forced = false;
	float goneFor = 0.0f;
};

enum class MarchWeather : UInt8 { None, Force, Release };

// One look: `outdoors` the player in the open air, `nearest` the nearest
// living Daedra band member's distance (-1 none). Held while a band stays
// within a quarter past the distance, so the edge does not flicker.
inline MarchWeather StepMarchWeather(MarchWeatherState& st, const MarchWeatherSettings& s, bool outdoors,
                                     float nearest, float dt) {
	if (!s.enabled || !outdoors) {
		const bool was = st.forced;
		st = MarchWeatherState{};
		return was ? MarchWeather::Release : MarchWeather::None;
	}
	if (!st.forced) {
		if (nearest >= 0.0f && nearest <= s.units) {
			st.forced = true;
			st.goneFor = 0.0f;
			return MarchWeather::Force;
		}
		return MarchWeather::None;
	}
	if (nearest >= 0.0f && nearest <= s.units * 1.25f) {
		st.goneFor = 0.0f;
		return MarchWeather::None;
	}
	st.goneFor += dt > 0.0f ? dt : 0.0f;
	if (st.goneFor < s.releaseSeconds) {
		return MarchWeather::None;
	}
	st = MarchWeatherState{};
	return MarchWeather::Release;
}

}  // namespace walkers
