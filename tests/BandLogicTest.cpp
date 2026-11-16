// Every flow of the war bands out of the open gates (docs/walkers-spec.md
// 4.22 step 3, Origin=1; Nadi 2026-11-15 "go 1-7"): where the invaders come
// from, the band's size, a band per open gate and its clock, far bands as
// numbers walking to the nearest city, a band near the player put into the
// world, a closed gate ending its far bands, the cap.

#include "BandLogic.h"

#include <cstdio>

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf("  %s  %s\n", condition ? "ok  " : "FAIL", what);
	if (!condition) {
		++g_failures;
	}
}

using walkers::BandEvent;
using walkers::BandSettings;
using walkers::BandState;
using walkers::InvaderOrigin;

// Two gates and two cities on a line, far from each other.
const walkers::BandGate kGates[] = {{0.0f, 0.0f}, {100000.0f, 0.0f}};
const walkers::BandCity kCities[] = {{20000.0f, 0.0f, 3}, {130000.0f, 0.0f, 5}};

struct Run {
	BandState state;
	BandEvent events[walkers::kBandEventsMost];
	UInt32 n = 0;
	UInt32 Step(const BandSettings& s, const bool* open, float hours, float px, float py, UInt32 roll = 0) {
		n = walkers::StepBands(state, s, kGates, open, 2, kCities, 2, hours, px, py, roll, events,
		                       walkers::kBandEventsMost);
		return n;
	}
	UInt32 Count(walkers::BandEventKind kind) const {
		UInt32 c = 0;
		for (UInt32 i = 0; i < n; ++i) {
			c += events[i].kind == kind ? 1u : 0u;
		}
		return c;
	}
};

void TestOrigin() {
	std::printf("Where the invaders come from\n");
	BandSettings s;
	Check(s.origin == InvaderOrigin::Everywhere && !s.everywhereUntilGatesOpen,
	      "defaults: Origin 0, EverywhereUntilGatesOpen 0 (decided 2026-11-13/14)");
	Check(walkers::ReplacementOn(s, 0) && walkers::ReplacementOn(s, 5) && !walkers::BandsOn(s, 5),
	      "Origin 0: the spawn replacement, open gates or not; no bands");
	s.origin = InvaderOrigin::Gates;
	Check(!walkers::ReplacementOn(s, 0) && !walkers::ReplacementOn(s, 3),
	      "Origin 1: the world keeps the game's spawns");
	Check(!walkers::BandsOn(s, 0) && walkers::BandsOn(s, 1), "Origin 1: bands only while a gate is open (a)");
	s.everywhereUntilGatesOpen = true;
	Check(walkers::ReplacementOn(s, 0) && !walkers::ReplacementOn(s, 1),
	      "EverywhereUntilGatesOpen 1: as Origin 0 until the first gate opens (c), then only out of gates");
	Check(walkers::OriginFromIni(1) == InvaderOrigin::Gates && walkers::OriginFromIni(0) == InvaderOrigin::Everywhere &&
	          walkers::OriginFromIni(9) == InvaderOrigin::Everywhere,
	      "the INI's number: 1 the gates, anything else everywhere");
}

void TestPace() {
	std::printf("How often the gates are looked at\n");
	Check(walkers::kGateLookSeconds == 5.0f, "every 5 s (Nadi, 2026-11-16)");
}

void TestSize() {
	std::printf("A band's size\n");
	Check(walkers::BandSize(4, 9, 0) == 4 && walkers::BandSize(4, 9, 5) == 9 && walkers::BandSize(4, 9, 6) == 4,
	      "BandSizeMin..Max by the roll");
	Check(walkers::BandSize(9, 4, 0) == 9 && walkers::BandSize(9, 4, 3) == 9, "min above max: min");
	Check(walkers::BandSize(0, 0, 7) == 1, "never less than one");
}

void TestClock() {
	std::printf("A band per open gate, every BandEveryHours\n");
	BandSettings s;
	s.origin = InvaderOrigin::Gates;
	s.unitsPerHour = 100.0f;  // slow: none arrives within the day
	Run r;
	const bool closed[] = {false, false};
	Check(r.Step(s, closed, 24.0f, 500000.0f, 500000.0f) == 0 && r.state.count == 0, "no gate open: nothing");
	const bool first[] = {true, false};
	r.Step(s, first, 1.0f, 500000.0f, 500000.0f);
	Check(r.Count(walkers::BandEventKind::Formed) == 1 && r.state.count == 1,
	      "a gate just open: its first band at once");
	Check(r.state.bands[0].gate == 0 && r.state.bands[0].city == 0 && r.state.bands[0].size == 4,
	      "at gate 0, heading for the nearest city (0), size by the roll");
	r.Step(s, first, 11.0f, 500000.0f, 500000.0f);
	Check(r.Count(walkers::BandEventKind::Formed) == 0, "eleven hours on: not yet");
	r.Step(s, first, 1.0f, 500000.0f, 500000.0f);
	Check(r.Count(walkers::BandEventKind::Formed) == 1 && r.state.count == 2, "twelve: the next band");
	s.origin = InvaderOrigin::Everywhere;
	Run other;
	Check(other.Step(s, first, 24.0f, 500000.0f, 500000.0f) == 0, "Origin 0: no bands, gates or not");
}

void TestMarch() {
	std::printf("Far bands walk as numbers\n");
	BandSettings s;
	s.origin = InvaderOrigin::Gates;
	s.everyHours = 1000.0f;
	Run r;
	const bool open[] = {true, false};
	r.Step(s, open, 0.0f, 500000.0f, 500000.0f);
	const float before = r.state.bands[0].x;
	r.Step(s, open, 1.0f, 500000.0f, 500000.0f);
	Check(r.state.bands[0].x - before > s.unitsPerHour * 0.99f && r.state.bands[0].x - before < s.unitsPerHour * 1.01f,
	      "an hour: UnitsPerHour toward its city");
	r.Step(s, open, 100.0f, 500000.0f, 500000.0f);
	Check(r.Count(walkers::BandEventKind::Arrived) == 1 && r.state.count == 0,
	      "at the city: Arrived (the siege's pressure), off the list");
	Check(r.events[0].city == 0 && r.events[0].size == 4, "with its city and its size");
}

void TestNear() {
	std::printf("A band near the player comes into the world\n");
	BandSettings s;
	s.origin = InvaderOrigin::Gates;
	s.everyHours = 1000.0f;
	Run r;
	const bool open[] = {true, false};
	r.Step(s, open, 0.0f, 5000.0f, 0.0f);
	Check(r.Count(walkers::BandEventKind::Formed) == 1 && r.Count(walkers::BandEventKind::Near) == 1 &&
	          r.state.count == 0,
	      "formed within NearUnits of the player: Near at once (the caller places it), no number left");
	Run far;
	far.Step(s, open, 0.0f, 0.0f, 60000.0f);
	Check(far.Count(walkers::BandEventKind::Near) == 0 && far.state.count == 1, "beyond NearUnits: a number");
	far.Step(s, open, 1.0f, s.unitsPerHour, 4000.0f);  // where it gets to in the hour
	Check(far.Count(walkers::BandEventKind::Near) == 1 && far.events[0].x > 0.0f && far.state.count == 0,
	      "the player comes near: Near where it has got to");
}

void TestPlace() {
	std::printf("Where a Near band is placed\n");
	float dx = 0.0f;
	float dy = 0.0f;
	walkers::BandPlaceOffset(5000.0f, 0.0f, 0.0f, 0.0f, dx, dy);
	Check(dx == 5000.0f && dy == 0.0f, "farther than 3000: at its own spot");
	walkers::BandPlaceOffset(1000.0f, 0.0f, 0.0f, 0.0f, dx, dy);
	Check(dx > 2999.0f && dx < 3001.0f && dy == 0.0f, "nearer: pushed out to 3000 on the same line");
	walkers::BandPlaceOffset(10.0f, 10.0f, 10.0f, 10.0f, dx, dy);
	Check(dx == 0.0f && dy == 3000.0f, "on the player: 3000 to the north");
}

void TestClosed() {
	std::printf("A closed gate ends its bands\n");
	BandSettings s;
	s.origin = InvaderOrigin::Gates;
	s.everyHours = 1.0f;
	Run r;
	const bool both[] = {true, true};
	r.Step(s, both, 0.0f, 500000.0f, 500000.0f);
	Check(r.state.count == 2, "two gates open: two bands");
	const bool onlySecond[] = {false, true};
	r.Step(s, onlySecond, 0.0f, 500000.0f, 500000.0f);
	Check(r.Count(walkers::BandEventKind::Ended) == 1 && r.state.count == 1 && r.state.bands[0].gate == 1,
	      "gate 0 closed: its band ends, the other walks on");
	r.Step(s, onlySecond, 0.5f, 500000.0f, 500000.0f);
	Check(r.Count(walkers::BandEventKind::Formed) == 0, "the closed gate's clock forms nothing");
	const bool reopened[] = {true, true};
	r.Step(s, reopened, 0.0f, 500000.0f, 500000.0f);
	Check(r.Count(walkers::BandEventKind::Formed) == 1, "open again (MQ00.allowGatesToReopen): a band at once");
}

void TestCap() {
	std::printf("The cap\n");
	BandSettings s;
	s.origin = InvaderOrigin::Gates;
	s.everyHours = 0.1f;
	Run r;
	const bool open[] = {true, true};
	for (int i = 0; i < 400; ++i) {
		r.Step(s, open, 0.1f, 9e6f, 9e6f);
	}
	Check(r.state.count <= walkers::kBandsMost && r.state.count >= walkers::kBandsMost - 2,
	      "never more than kBandsMost far bands");
	s.bandsMost = 3;
	Run small;
	for (int i = 0; i < 40; ++i) {
		small.Step(s, open, 0.1f, 9e6f, 9e6f);
	}
	Check(small.state.count == 3, "BandsMost 3: three");
	Run full;
	BandEvent one[1];
	s.bandsMost = 10;
	const UInt32 n = walkers::StepBands(full.state, s, kGates, open, 2, kCities, 2, 0.0f, 9e6f, 9e6f, 0, one, 1);
	Check(n == 1 && full.state.count == 2, "the events past the caller's room are dropped, the bands kept");
}

void TestPressure() {
	std::printf("A band at its city: the siege's pressure\n");
	BandSettings s;
	s.origin = InvaderOrigin::Gates;
	s.everyHours = 1000.0f;
	Check(s.siegePercentPerBand == 5 && s.siegeFadePercentPerDay == 5.0f,
	      "defaults: 5 points a band, 5 points fade a day");
	Run r;
	const bool open[] = {true, false};
	r.Step(s, open, 0.0f, 9e6f, 9e6f);
	r.Step(s, open, 10.0f, 9e6f, 9e6f);
	Check(r.Count(walkers::BandEventKind::Arrived) == 1 && walkers::SiegePressure(r.state, 3) == 5 &&
	          walkers::SiegePressure(r.state, 5) == 0,
	      "arrived at the gate of city 3: its daily siege chance 5 points up, the other city's not");
	r.Step(s, open, 24.0f * 0.5f, 9e6f, 9e6f);
	Check(walkers::SiegePressure(r.state, 3) == 2, "half a day: 2.5 points faded, 2 whole left");
	r.Step(s, open, 24.0f, 9e6f, 9e6f);
	Check(walkers::SiegePressure(r.state, 3) == 0, "a day more: none, never below 0");
	for (int i = 0; i < 40; ++i) {
		walkers::AddSiegePressure(r.state, s, 3);
	}
	Check(walkers::SiegePressure(r.state, 3) == walkers::kSiegePressureMost, "held to 90 points");
	Check(walkers::SiegePressure(r.state, 200) == 0, "a city past the table: 0");
}

UInt32 Resolve(UInt32 saved, UInt32& loaded) {
	if (saved == 0xFF000BAD) {
		return 0;
	}
	loaded = saved + 1;
	return 1;
}

void TestImage() {
	std::printf("The bands in the co-save\n");
	BandSettings s;
	s.origin = InvaderOrigin::Gates;
	s.everyHours = 7.0f;
	Run r;
	const bool open[] = {true, true};
	r.Step(s, open, 0.0f, 9e6f, 9e6f);
	walkers::AddSiegePressure(r.state, s, 5);
	walkers::BandMember members[3] = {{0xFF000A01, 0, false}, {0xFF000A02, 0, true}, {0xFF000BAD, 1, false}};
	UInt8 image[walkers::kBandImageMost];
	const UInt32 length = walkers::BandImage(r.state, members, 3, image, sizeof(image));
	Check(length > 0 && length <= walkers::kBandImageMost, "written, within its most");
	BandState back;
	walkers::BandMember read[walkers::kBandMembersMost];
	UInt32 readCount = 0;
	const bool whole = walkers::ReadBandImage(back, read, readCount, image, length, walkers::kBandImageVersion,
	                                          [](UInt32 saved, UInt32& loaded) { return Resolve(saved, loaded) != 0; });
	Check(whole && back.count == 2 && back.bands[1].gate == 1 && back.bands[0].size == r.state.bands[0].size &&
	          back.bands[0].x == r.state.bands[0].x,
	      "the far bands back, gate, size and place");
	Check(back.wasOpen[0] && back.wasOpen[1] && back.hoursToNext[0] == 7.0f,
	      "the gates' clocks back: no band at once after a load");
	Check(walkers::SiegePressure(back, 5) == 5, "the pressure back");
	Check(readCount == 2 && read[0].ref == 0xFF000A02 && read[1].ref == 0xFF000A03 && read[1].leader && read[1].band == 0,
	      "the members through ResolveRefID, the leader marked; one gone from the game dropped");
	BandState cut;
	Check(!walkers::ReadBandImage(cut, read, readCount, image, 10, walkers::kBandImageVersion,
	                              [](UInt32 saved, UInt32& loaded) { return Resolve(saved, loaded) != 0; }) &&
	          cut.count == 0 && readCount == 0,
	      "cut short: nothing, a fresh start");
	Check(!walkers::ReadBandImage(cut, read, readCount, image, length, 99,
	                              [](UInt32 saved, UInt32& loaded) { return Resolve(saved, loaded) != 0; }),
	      "another version: not read");
}

}  // namespace

int main() {
	TestOrigin();
	TestPace();
	TestSize();
	TestClock();
	TestMarch();
	TestNear();
	TestPlace();
	TestClosed();
	TestCap();
	TestPressure();
	TestImage();
	if (g_failures != 0) {
		std::printf("%d failed\n", g_failures);
		return 1;
	}
	std::printf("all passed\n");
	return 0;
}
