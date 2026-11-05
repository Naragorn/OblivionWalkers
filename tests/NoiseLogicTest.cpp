// Every flow of the noise (docs/walkers-spec.md 4.20.2; Nadi, 2026-11-05:
// "go 5", toggled in the INI and LINK, on by default): who hears it, the
// way there, the search and its hops, the way back, a noise again near the
// same spot or elsewhere, a group at its gate drawn off and back, the
// package lost on the way, off, and OBVR's message.

#include "HordeLogic.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf("  %s  %s\n", condition ? "ok  " : "FAIL", what);
	if (!condition) {
		++g_failures;
	}
}

using walkers::HordeGate;
using walkers::HordeGroup;
using walkers::HordeOrder;
using walkers::HordeSettings;
using walkers::HordeState;
using walkers::HordeWalker;

constexpr UInt32 kTravel = 0x11000806;
constexpr UInt32 kFollow = 0x11000816;
constexpr UInt32 kWorld = 0x3C;

HordeWalker Walker(UInt32 id, float x, float y, UInt32 rank, UInt32 package = 0) {
	HordeWalker w;
	w.id = id;
	w.x = x;
	w.y = y;
	w.rank = rank;
	w.package = package;
	w.fit = true;
	w.world = kWorld;
	return w;
}

float Distance(float ax, float ay, float bx, float by) {
	return std::sqrt((ax - bx) * (ax - bx) + (ay - by) * (ay - by));
}

struct Run {
	HordeState state;
	HordeSettings settings;
	UInt32 seed = 12345;
	HordeOrder orders[64];
	UInt32 count = 0;

	Run() {
		settings.enabled = true;
		settings.travelBase = kTravel;
		settings.followBase = kFollow;
		settings.towardCities = false;
	}
	UInt32 Step(const HordeWalker* walkers, UInt32 n, float dt = 1.0f) {
		count = walkers::StepHorde(state, walkers, n, settings, dt, seed, orders, 64);
		return count;
	}
	UInt32 Hear(const HordeWalker* walkers, UInt32 n, float x, float y, UInt32 world = kWorld) {
		return walkers::HearNoise(state, walkers, n, settings, x, y, world);
	}
	const HordeOrder* Marker() const {
		for (UInt32 i = 0; i < count; ++i) {
			if (orders[i].kind == HordeOrder::MoveMarker) {
				return &orders[i];
			}
		}
		return nullptr;
	}
	UInt32 Of(HordeOrder::Kind kind, UInt32 walker = 0) const {
		UInt32 found = 0;
		for (UInt32 i = 0; i < count; ++i) {
			found += orders[i].kind == kind && (walker == 0 || orders[i].walker == walker) ? 1u : 0u;
		}
		return found;
	}
	// The group walker `leader` leads.
	HordeGroup* GroupOf(UInt32 leader) {
		for (HordeGroup& g : state.groups) {
			if (g.used && g.leader == leader) {
				return &g;
			}
		}
		return nullptr;
	}
};

// A group of two: leader 2 at 0 0, follower 1 at 300 0; its marker placed,
// its packages running.
void Formed(Run& run, HordeWalker* w) {
	w[0] = Walker(1, 300, 0, 3);
	w[1] = Walker(2, 0, 0, 9);
	run.Step(w, 2);
	const HordeGroup* g = run.GroupOf(2);
	const UInt32 group = g != nullptr ? static_cast<UInt32>(g - run.state.groups) : 0;
	w[0].package = kFollow + group;
	w[1].package = kTravel + group;
	run.Step(w, 2);
}

void TestHeard() {
	std::printf("Who hears it\n");
	Run run;
	HordeWalker w[2];
	Formed(run, w);
	Check(run.Hear(w, 2, 1000, 0) == 1 && run.GroupOf(2)->drawn, "a group with its leader within Radius (1000 of 1200): drawn");
	run.Step(w, 2);
	const HordeOrder* m = run.Marker();
	Check(m != nullptr && std::fabs(m->x - 1000.0f) < 1.0f && std::fabs(m->y) < 1.0f && run.Of(HordeOrder::Lead, 2) == 1,
	      "its marker set on the spot, the leader led there anew");

	Run far;
	Formed(far, w);
	Check(far.Hear(w, 2, 5000, 0) == 0 && !far.GroupOf(2)->drawn, "beyond Radius of every walker: not heard");

	Run follower;
	Formed(follower, w);
	Check(follower.Hear(w, 2, 1400, 0) == 1 && follower.GroupOf(2)->drawn,
	      "the leader 1400 off, its follower 1100: the follower hears it, the group goes");

	Run fighting;
	Formed(fighting, w);
	w[0].fit = false;
	w[1].fit = false;
	Check(fighting.Hear(w, 2, 100, 0) == 0, "only the group's fighters near (unfit): not heard - they are the noise");
	w[0].fit = true;
	w[1].fit = true;

	Run elsewhere;
	Formed(elsewhere, w);
	Check(elsewhere.Hear(w, 2, 100, 0, 0x99) == 0, "in another worldspace: not heard");

	Run off;
	off.settings.noise.enabled = false;
	Formed(off, w);
	Check(off.Hear(w, 2, 100, 0) == 0 && !off.GroupOf(2)->drawn, "[Noise] Enabled=0: never heard");

	Run lone;
	const HordeWalker none[] = {Walker(7, 0, 0, 1)};
	Check(lone.Hear(none, 1, 0, 0) == 0, "no group yet (the walkers handed out the next step): nobody drawn");
}

void TestSearch() {
	std::printf("There, the search, and on\n");
	Run run;
	HordeWalker w[2];
	Formed(run, w);
	run.Hear(w, 2, 1000, 0);
	run.Step(w, 2);
	run.Step(w, 2);
	Check(run.Marker() == nullptr && run.GroupOf(2)->searchLeft < 0.0f, "on the way: the marker stays on the spot");
	for (int i = 0; i < 40; ++i) {
		run.Step(w, 2);
	}
	Check(run.GroupOf(2)->drawn && run.GroupOf(2)->targetX > 500.0f,
	      "40 s on the way without coming nearer: still drawn - StaleSeconds does not turn it away");
	w[1].x = 900;
	w[0].x = 1100;
	run.Step(w, 2);
	Check(run.GroupOf(2)->searchLeft > 29.0f, "the leader within ReachUnits of the spot: the search begins");
	bool hopped = false;
	bool near = true;
	for (int i = 0; i < 25; ++i) {
		run.Step(w, 2);
		const HordeOrder* m = run.Marker();
		if (m != nullptr) {
			hopped = true;
			near = near && Distance(m->x, m->y, 1000, 0) <= run.settings.noise.searchUnits + 1.0f;
		}
	}
	Check(hopped && near && run.GroupOf(2)->drawn, "searching: the marker hops about the spot, within SearchUnits");
	for (int i = 0; i < 6; ++i) {
		run.Step(w, 2);
	}
	const HordeGroup* g = run.GroupOf(2);
	Check(g != nullptr && !g->drawn, "after SearchSeconds: no longer drawn");
	Check(g != nullptr && std::fabs(Distance(g->targetX, g->targetY, 900, 0) - run.settings.leadUnits) < 1.0f,
	      "the marker LeadUnits ahead again - on as before");
}

void TestNeverThere() {
	std::printf("Never there\n");
	Run run;
	HordeWalker w[2];
	Formed(run, w);
	run.Hear(w, 2, 1000, 0);
	for (int i = 0; i < 61; ++i) {
		run.Step(w, 2);
	}
	Check(run.GroupOf(2)->searchLeft >= 0.0f, "not there after TravelSeconds (a cliff, a river): they search from there");
}

void TestAgain() {
	std::printf("A noise again\n");
	Run run;
	HordeWalker w[2];
	Formed(run, w);
	run.Hear(w, 2, 1000, 0);
	run.Step(w, 2);
	w[1].x = 950;
	w[0].x = 1100;
	run.Step(w, 2);
	for (int i = 0; i < 20; ++i) {
		run.Step(w, 2);
	}
	const float before = run.GroupOf(2)->searchLeft;
	run.Hear(w, 2, 1200, 100);
	run.Step(w, 2);
	Check(before < 15.0f && run.GroupOf(2)->searchLeft > 28.0f && std::fabs(run.GroupOf(2)->noiseX - 1000.0f) < 1.0f,
	      "a fight going on near the spot (within SearchUnits): the search lasts anew, the spot kept");
	run.Hear(w, 2, 1900, 0);
	run.Step(w, 2);
	const HordeOrder* m = run.Marker();
	Check(m != nullptr && std::fabs(m->x - 1900.0f) < 1.0f && run.GroupOf(2)->searchLeft < 0.0f,
	      "a noise farther off: drawn on to it");
}

void TestLost() {
	std::printf("The package lost on the way\n");
	Run run;
	HordeWalker w[2];
	Formed(run, w);
	run.Hear(w, 2, 1000, 0);
	run.Step(w, 2);
	const UInt32 group = static_cast<UInt32>(run.GroupOf(2) - run.state.groups);
	w[1].package = 0x22;
	w[0].package = 0x22;
	run.Step(w, 2);
	Check(run.Of(HordeOrder::Lead, 2) == 1 && run.Of(HordeOrder::Follow, 1) == 1 && run.Marker() == nullptr,
	      "the leader's and the follower's packages given again, the marker left on the spot");
	w[1].package = kTravel + group;
	w[0].package = kFollow + group;
}

void TestGate() {
	std::printf("A group at its gate\n");
	Run run;
	static const HordeGate gates[] = {{0.0f, 3000.0f}};
	run.settings.towardCities = true;
	run.settings.gates = gates;
	run.settings.gateCount = 1;
	HordeWalker w[2] = {Walker(1, 300, 2000, 3), Walker(2, 0, 2100, 9)};
	for (auto& x : w) {
		x.tamriel = true;
	}
	run.Step(w, 2);
	HordeGroup* g = run.GroupOf(2);
	const UInt32 group = static_cast<UInt32>(g - run.state.groups);
	w[0].package = kFollow + group;
	w[1].package = kTravel + group;
	run.Step(w, 2);
	Check(g->holding, "held at its gate before");
	run.Hear(w, 2, 800, 2100);
	run.Step(w, 2);
	Check(g->drawn && !g->holding && run.Marker() != nullptr && std::fabs(run.Marker()->x - 800.0f) < 1.0f,
	      "a noise off to the side: drawn off the gate");
	w[1].x = 800;
	for (int i = 0; i < 32; ++i) {
		run.Step(w, 2);
	}
	Check(!g->drawn && g->holding && Distance(g->targetX, g->targetY, 0, 3000) <= run.settings.gateHoldUnits + 1.0f,
	      "the search over: back to holding before the gate");
}

void TestMessage() {
	std::printf("OBVR's 'WNOI'\n");
	float x = 0.0f;
	float y = 0.0f;
	float z = 0.0f;
	UInt8 data[16];
	const float v[3] = {10.0f, -20.0f, 30.0f};
	UInt32 kind = walkers::kNoiseThrown;
	std::memcpy(data, v, 12);
	std::memcpy(data + 12, &kind, 4);
	Check(walkers::NoiseMessageOf(data, 16, x, y, z) && x == 10.0f && y == -20.0f && z == 30.0f,
	      "16 bytes, a throw: the spot taken");
	Check(!walkers::NoiseMessageOf(data, 12, x, y, z) && !walkers::NoiseMessageOf(nullptr, 16, x, y, z),
	      "the wrong length or no data: refused");
	kind = 7;
	std::memcpy(data + 12, &kind, 4);
	Check(!walkers::NoiseMessageOf(data, 16, x, y, z), "a kind not known: refused");
	kind = walkers::kNoiseThrown;
	std::memcpy(data + 12, &kind, 4);
	const float nan = std::numeric_limits<float>::quiet_NaN();
	std::memcpy(data, &nan, 4);
	Check(!walkers::NoiseMessageOf(data, 16, x, y, z), "a spot not a number: refused");
}

}  // namespace

int main() {
	TestHeard();
	TestSearch();
	TestNeverThere();
	TestAgain();
	TestLost();
	TestGate();
	TestMessage();
	std::printf(g_failures == 0 ? "PASS\n" : "FAIL (%d)\n", g_failures);
	return g_failures == 0 ? 0 : 1;
}
