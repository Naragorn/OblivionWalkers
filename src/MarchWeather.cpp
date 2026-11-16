#include "MarchWeather.h"

#include <cstdio>

#include "Siege.h"
#include "Engine.h"

namespace walkers {

using namespace engine;

namespace {

constexpr UInt32 kPlayerRefId = 0x14;
constexpr UInt32 kRefParentCellOffset = 0x40;
constexpr UInt32 kCellFlagsOffset = 0x24;
// The sky and its current weather (Disguise.cpp: xOBSE Sky_GetSingleton
// 0x00542EA0, the weather at +0x10).
constexpr UInt32 kSkyGetSingleton = 0x00542EA0;
constexpr UInt32 kSkyWeatherOffset = 0x10;
constexpr float kStepSeconds = 1.0f;

const MarchWeatherSettings* g_settings = nullptr;
ConsoleFn g_console = nullptr;
DeadFn g_dead = nullptr;
LogFn g_log = nullptr;
MarchWeatherState g_state;
UInt32 g_before = 0;  // the weather before the red sky
float g_clock = 0.0f;
UInt32 g_logsLeft = 40;
UInt32 g_seen = 0;
UInt32 g_afterLooks = 0;  // looks after letting go in which the sky is logged  // the sky's weather last logged while forced (the proof: the console's answer is none)

bool Outdoors() {
	const UInt32 player = LookupForm(kPlayerRefId);
	const UInt32 cell = LooksLikeObject(player) ? Read(player + kRefParentCellOffset) : 0;
	return LooksLikeObject(cell) && (Read8(cell + kCellFlagsOffset) & 1) == 0;
}

UInt32 WeatherNow() {
	using SkyFn = UInt32(__cdecl*)();
	const UInt32 sky = reinterpret_cast<SkyFn>(kSkyGetSingleton)();
	return LooksLikeObject(sky) ? FormIdOf(Read(sky + kSkyWeatherOffset)) : 0;
}

bool Say(const char* line) {
	const UInt32 player = LookupForm(kPlayerRefId);
	return LooksLikeObject(player) && g_console(player, line);
}

}  // namespace

void ConfigureMarchWeather(const MarchWeatherSettings* settings, ConsoleFn console, DeadFn dead, LogFn log) {
	g_settings = settings;
	g_console = console;
	g_dead = dead;
	g_log = log;
	log("March weather: [Invaders] MarchWeather=%d MarchWeatherDistance=%.0f (the sky %08X)", settings->enabled ? 1 : 0,
	    settings->units, kMarchWeatherForm);
}

void StepMarchWeatherInGame(float dt) {
	if (g_settings == nullptr || g_console == nullptr) {
		return;
	}
	g_clock += dt;
	if (g_clock < kStepSeconds) {
		return;
	}
	const float elapsed = g_clock;
	g_clock = 0.0f;
	const bool outdoors = Outdoors();
	const float nearest = g_state.forced || g_settings->enabled ? NearestDaedraSiegeToPlayer() : -1.0f;
	const MarchWeather step = StepMarchWeather(g_state, *g_settings, outdoors, nearest, elapsed);
	char line[48];
	if (g_afterLooks > 0 && step == MarchWeather::None) {
		--g_afterLooks;
		const UInt32 now = WeatherNow();
		if (now != g_seen && g_logsLeft > 0) {
			--g_logsLeft;
			g_log("March weather: after letting go the sky's weather now %08X", now);
		}
		g_seen = now;
	}
	if (g_state.forced && step == MarchWeather::None) {
		const UInt32 now = WeatherNow();
		if (now != g_seen && g_logsLeft > 0) {
			--g_logsLeft;
			g_log("March weather: the sky's weather now %08X (the red sky %s)", now,
			      now == kMarchWeatherForm ? "in" : "not yet");
		}
		g_seen = now;
	}
	if (step == MarchWeather::Force) {
		g_seen = 0;
		g_before = WeatherNow();
		std::snprintf(line, sizeof(line), "SetWeather %08X 1", kMarchWeatherForm);
		const bool said = Say(line);
		if (g_logsLeft > 0) {
			--g_logsLeft;
			g_log("March weather: a Daedra siege %.0f away - \"%s\" answered %d (the weather before %08X)", nearest, line,
			      said ? 1 : 0, g_before);
		}
	} else if (step == MarchWeather::Release) {
		const bool released = Say("ReleaseWeatherOverride");
		bool back = false;
		if (g_before != 0 && g_before != kMarchWeatherForm) {
			std::snprintf(line, sizeof(line), "SetWeather %08X", g_before);
			back = Say(line);
		}
		if (g_logsLeft > 0) {
			--g_logsLeft;
			g_log("March weather: let go (%s) - ReleaseWeatherOverride answered %d, the weather before %08X set again "
			      "%d",
			      outdoors ? "no Daedra siege near" : "the player inside or switched off", released ? 1 : 0, g_before,
			      back ? 1 : 0);
		}
		g_before = 0;
		g_seen = kMarchWeatherForm;
		g_afterLooks = 60;
	}
}

void ResetMarchWeather() {
	g_state = MarchWeatherState{};
	g_before = 0;
	g_clock = 0.0f;
	g_afterLooks = 0;
}

}  // namespace walkers
