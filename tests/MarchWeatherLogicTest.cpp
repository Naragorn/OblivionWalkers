// Every flow of the red sky (docs/walkers-spec.md 4.22 idea 9; Nadi,
// 2026-11-16: over a Daedra siege, not a band): a besieged gate near the
// player outdoors brings the gates' red sky; the siege over, or the player
// inside, the sky is let go. The distance is the caller's (SiegeLogic.h
// NearestDaedraSiegeUnits).

#include "MarchWeatherLogic.h"

#include <cstdio>

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf("  %s  %s\n", condition ? "ok  " : "FAIL", what);
	if (!condition) {
		++g_failures;
	}
}

using walkers::MarchWeather;
using walkers::MarchWeatherSettings;
using walkers::MarchWeatherState;

void TestOff() {
	std::printf("Off by default\n");
	MarchWeatherSettings s;
	MarchWeatherState st;
	Check(!s.enabled, "MarchWeather=0 by default (it fights the other weather mods)");
	Check(walkers::StepMarchWeather(st, s, true, 1000.0f, 1.0f) == MarchWeather::None && !st.forced,
	      "off: a band near changes nothing");
}

void TestForce() {
	std::printf("A band near\n");
	MarchWeatherSettings s;
	s.enabled = true;
	MarchWeatherState st;
	Check(walkers::StepMarchWeather(st, s, true, -1.0f, 1.0f) == MarchWeather::None, "no band: nothing");
	Check(walkers::StepMarchWeather(st, s, true, s.units + 1.0f, 1.0f) == MarchWeather::None, "a band farther: nothing");
	Check(walkers::StepMarchWeather(st, s, false, 1000.0f, 1.0f) == MarchWeather::None, "the player inside: nothing");
	Check(walkers::StepMarchWeather(st, s, true, s.units, 1.0f) == MarchWeather::Force && st.forced,
	      "a band within MarchWeatherDistance, outdoors: the red sky");
	Check(walkers::StepMarchWeather(st, s, true, 500.0f, 1.0f) == MarchWeather::None, "it holds");
}

void TestRelease() {
	std::printf("Let go\n");
	MarchWeatherSettings s;
	s.enabled = true;
	MarchWeatherState st;
	walkers::StepMarchWeather(st, s, true, 1000.0f, 1.0f);
	Check(walkers::StepMarchWeather(st, s, true, s.units * 1.2f, 10.0f) == MarchWeather::None && st.forced,
	      "the band a little past the distance: held (no flicker at the edge)");
	Check(walkers::StepMarchWeather(st, s, true, -1.0f, s.releaseSeconds - 1.0f) == MarchWeather::None && st.forced,
	      "gone, not long enough: held");
	Check(walkers::StepMarchWeather(st, s, true, -1.0f, 1.0f) == MarchWeather::Release && !st.forced,
	      "gone ReleaseSeconds: let go");
	walkers::StepMarchWeather(st, s, true, 1000.0f, 1.0f);
	walkers::StepMarchWeather(st, s, true, -1.0f, s.releaseSeconds - 1.0f);
	walkers::StepMarchWeather(st, s, true, 1000.0f, 1.0f);
	Check(walkers::StepMarchWeather(st, s, true, -1.0f, s.releaseSeconds - 1.0f) == MarchWeather::None && st.forced,
	      "back in the meantime: the clock from the start");
	Check(walkers::StepMarchWeather(st, s, false, 1000.0f, 1.0f) == MarchWeather::Release && !st.forced,
	      "the player goes inside: let go at once");
	walkers::StepMarchWeather(st, s, true, 1000.0f, 1.0f);
	MarchWeatherSettings off = s;
	off.enabled = false;
	Check(walkers::StepMarchWeather(st, off, true, 1000.0f, 1.0f) == MarchWeather::Release && !st.forced,
	      "switched off (LINK) while forced: let go");
}

}  // namespace

int main() {
	TestOff();
	TestForce();
	TestRelease();
	std::printf(g_failures == 0 ? "PASS\n" : "FAIL (%d)\n", g_failures);
	return g_failures == 0 ? 0 : 1;
}
