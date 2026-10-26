// Every flow of the gate guards (docs/walkers-spec.md 4.11b): as many out as
// the walkers at the gate ask for, one for every ZombiesPerGuard, from the
// city's pool - waiting ones first, then the dead raised again - and taken
// in once no walker has been near for ClearSeconds.

#include "GuardLogic.h"

#include <cmath>
#include <cstdio>

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf("  %s  %s\n", condition ? "ok  " : "FAIL", what);
	if (!condition) {
		++g_failures;
	}
}

using walkers::GuardOrder;
using walkers::kGuardPool;

constexpr UInt32 kAnvil = 0;
constexpr UInt32 kBruma = 2;

struct Run {
	walkers::GuardState state;
	walkers::GuardSettings settings;
	// Gate 0 and 1 are Anvil's, gate 2 Bruma's.
	UInt32 gateCity[3] = {kAnvil, kAnvil, kBruma};
	UInt32 zombies[3] = {0, 0, 0};
	bool alive[walkers::kGuardCities][kGuardPool] = {};
	GuardOrder orders[32];
	UInt32 count = 0;

	Run() {
		for (auto& city : alive) {
			for (bool& a : city) {
				a = true;
			}
		}
	}
	UInt32 Step(float dt = 1.0f) {
		count = walkers::StepGuards(state, settings, gateCity, 3, zombies, alive, dt, orders, 32);
		// What the engine does with the orders: out and alive, killed, or in.
		for (UInt32 i = 0; i < count; ++i) {
			if (orders[i].kind == GuardOrder::Deploy) {
				alive[orders[i].city][orders[i].slot] = true;
			} else if (orders[i].kind == GuardOrder::Kill) {
				alive[orders[i].city][orders[i].slot] = false;
			}
		}
		return count;
	}
	UInt32 Of(GuardOrder::Kind kind, int gate = -2) const {
		UInt32 n = 0;
		for (UInt32 i = 0; i < count; ++i) {
			n += orders[i].kind == kind && (gate == -2 || orders[i].gate == gate) ? 1u : 0u;
		}
		return n;
	}
	UInt32 Out(int gate) const {
		UInt32 n = 0;
		for (UInt32 c = 0; c < walkers::kGuardCities; ++c) {
			for (UInt32 s = 0; s < kGuardPool; ++s) {
				n += state.slots[c][s].out && state.slots[c][s].gate == gate ? 1u : 0u;
			}
		}
		return n;
	}
};

void TestCount() {
	std::printf("How many come out\n");
	Check(walkers::GuardsWanted(0, 5) == 0 && walkers::GuardsWanted(1, 5) == 1 && walkers::GuardsWanted(5, 5) == 1 &&
	          walkers::GuardsWanted(6, 5) == 2 && walkers::GuardsWanted(11, 5) == 3,
	      "one for every ZombiesPerGuard walkers, the rest rounded up: 1->1, 5->1, 6->2, 11->3");
	Check(walkers::GuardsWanted(40, 5) == kGuardPool, "never more than the city's pool (six)");
	Check(walkers::GuardsWanted(3, 0) == 3, "ZombiesPerGuard 0 read as 1: one guard for each walker");
}

void TestOut() {
	std::printf("Out to a gate\n");
	Run run;
	run.Step();
	Check(run.count == 0, "no walker at any gate: nobody comes out");
	run.zombies[0] = 7;
	run.Step();
	bool rightCity = true;
	for (UInt32 i = 0; i < run.count; ++i) {
		rightCity = rightCity && run.orders[i].city == kAnvil && run.orders[i].gate == 0 && !run.orders[i].raise;
	}
	Check(run.Of(GuardOrder::Deploy, 0) == 2 && rightCity, "seven walkers at Anvil's gate: two of Anvil's guards out there");
	run.Step();
	Check(run.count == 0, "both still standing: nothing more");
	run.zombies[1] = 3;
	run.Step();
	Check(run.Of(GuardOrder::Deploy, 1) == 1 && run.Out(0) == 2, "Anvil's other gate: one more from the same pool");
	run.zombies[2] = 1;
	run.Step();
	Check(run.Of(GuardOrder::Deploy, 2) == 1 && run.orders[0].city == kBruma, "Bruma's gate: Bruma's pool");
}

void TestReplace() {
	std::printf("A guard falls\n");
	Run run;
	run.zombies[0] = 7;
	run.Step();
	const UInt32 fallen = run.orders[0].slot;
	run.alive[kAnvil][fallen] = false;
	run.Step();
	Check(run.Of(GuardOrder::Deploy, 0) == 1 && !run.orders[0].raise && run.orders[0].slot != fallen,
	      "one dies: a waiting one comes out in its place");
	for (UInt32 s = 0; s < kGuardPool; ++s) {
		run.alive[kAnvil][s] = false;
	}
	run.state = walkers::GuardState{};
	run.Step();
	Check(run.Of(GuardOrder::Deploy, 0) == 2 && run.orders[0].raise && run.orders[1].raise &&
	          run.orders[0].slot != run.orders[1].slot,
	      "none waiting, all dead: two of the dead raised and sent out again (the pool is all there is)");
	run.zombies[0] = 40;
	for (UInt32 s = 0; s < kGuardPool; ++s) {
		run.alive[kAnvil][s] = true;
	}
	run.Step();
	run.Step();
	Check(run.Out(0) == kGuardPool, "forty walkers: the whole pool out, no more");
}

void TestIn() {
	std::printf("Taken in\n");
	Run run;
	run.settings.clearSeconds = 10.0f;
	run.zombies[0] = 7;
	run.Step();
	run.zombies[0] = 0;
	UInt32 recalls = 0;
	for (int t = 0; t < 9; ++t) {
		run.Step();
		recalls += run.Of(GuardOrder::Recall);
	}
	Check(recalls == 0, "9 s without a walker near: still out");
	run.Step();
	Check(run.Of(GuardOrder::Recall) == 2 && run.Out(0) == 0, "ClearSeconds without one: both taken in");
	run.zombies[0] = 7;
	run.Step();
	run.zombies[0] = 0;
	for (int t = 0; t < 5; ++t) {
		run.Step();
	}
	run.zombies[0] = 2;
	run.Step();
	run.zombies[0] = 0;
	recalls = 0;
	for (int t = 0; t < 9; ++t) {
		run.Step();
		recalls += run.Of(GuardOrder::Recall);
	}
	Check(recalls == 0, "a walker back in between: the count starts over");
}

void TestOff() {
	std::printf("Off\n");
	Run run;
	run.settings.enabled = false;
	run.zombies[0] = 7;
	Check(run.Step() == 0, "[Guards] Enabled=0: nobody comes out");
}

void TestKill() {
	std::printf("The test switch TestKillSeconds\n");
	Run run;
	run.zombies[0] = 7;
	run.Step();
	UInt32 kills = 0;
	for (int t = 0; t < 20; ++t) {
		run.Step();
		kills += run.Of(GuardOrder::Kill);
	}
	Check(kills == 0, "0 (the default): nobody is killed");
	Run test;
	test.settings.testKillSeconds = 3.0f;
	test.zombies[0] = 7;
	test.Step();
	test.Step();
	test.Step();
	Check(test.Of(GuardOrder::Kill) == 0, "2 s out: still standing");
	test.Step();
	bool both = test.Of(GuardOrder::Kill) == 2;
	for (UInt32 i = 0; i < test.count; ++i) {
		both = both && (test.orders[i].kind != GuardOrder::Kill || test.orders[i].gate == 0);
	}
	Check(both, "3 s out: both killed, where they stand");
	test.Step();
	Check(test.Of(GuardOrder::Deploy, 0) == 2 && !test.orders[0].raise, "the next step: two waiting ones out in their place");
	UInt32 raised = 0;
	UInt32 deployed = 0;
	for (int t = 0; t < 30; ++t) {
		test.Step();
		for (UInt32 i = 0; i < test.count; ++i) {
			if (test.orders[i].kind == GuardOrder::Deploy) {
				++deployed;
				raised += test.orders[i].raise ? 1u : 0u;
			}
		}
	}
	Check(raised > 0 && deployed > raised, "the waiting used up: the fallen raised and sent out again, over and over");
}

void TestNearGate() {
	std::printf("Walkers near a gate\n");
	const float xs[] = {100.0f, 3900.0f, 4100.0f, -2000.0f};
	const float ys[] = {0.0f, 0.0f, 0.0f, 0.0f};
	const bool alive[] = {true, true, true, false};
	Check(walkers::WalkersNear(0.0f, 0.0f, 4000.0f, xs, ys, alive, 4) == 2,
	      "the living within Radius of the door count; the dead and those farther do not");
}

void TestHealth() {
	std::printf("A guard's health\n");
	Check(walkers::GuardHealth(5.0f, 12) == 60, "HealthFactor 5 of a walker's 12: 60");
	Check(walkers::GuardHealth(2.5f, 13) == 33, "2.5 of 13: rounded, 33");
	Check(walkers::GuardHealth(0.0f, 12) == 1 && walkers::GuardHealth(5.0f, 0) == 1, "never below 1");
}

void TestTable() {
	std::printf("The gates\n");
	bool near = true;
	bool known = walkers::kCityGateCount <= walkers::kGuardGatesMost;
	UInt32 perCity[walkers::kGuardCities] = {};
	for (UInt32 g = 0; g < walkers::kCityGateCount; ++g) {
		const walkers::CityGate& gate = walkers::kCityGates[g];
		near = near && std::hypot(gate.outX - gate.x, gate.outY - gate.y) < 400.0f && gate.outZ < gate.z;
		const UInt32 city = static_cast<UInt32>(gate.city);
		known = known && city < walkers::kGuardCities;
		perCity[city < walkers::kGuardCities ? city : 0] += 1;
	}
	bool every = true;
	for (UInt32 c : perCity) {
		every = every && c > 0;
	}
	Check(known && every, "every city has a gate, the table fits the guards' arrays");
	Check(near, "every spot outside lies in front of its door (within 400 units) and below it (on the ground)");
}

void TestSpot() {
	std::printf("Where a guard comes out\n");
	walkers::CityGate gate{walkers::City::Anvil, 1, 0.0f, 0.0f, 200.0f, 0.0f, 100.0f, 0.0f, "test"};
	float x[kGuardPool];
	float y[kGuardPool];
	for (UInt32 s = 0; s < kGuardPool; ++s) {
		walkers::GuardSpot(gate, s, x[s], y[s]);
	}
	bool inLine = true;
	bool apart = true;
	for (UInt32 s = 0; s < kGuardPool; ++s) {
		inLine = inLine && std::fabs(y[s] - 100.0f) < 0.01f && std::fabs(x[s]) <= 200.0f;
		for (UInt32 t = s + 1; t < kGuardPool; ++t) {
			apart = apart && std::fabs(x[s] - x[t]) >= walkers::kGuardSpacing - 0.01f;
		}
	}
	Check(inLine && apart, "the six side by side across the way out, kGuardSpacing apart, around the spot");
}

void TestRecover() {
	std::printf("After a load\n");
	walkers::GuardState state;
	bool enabled[walkers::kGuardCities][kGuardPool] = {};
	float xs[walkers::kGuardCities][kGuardPool] = {};
	float ys[walkers::kGuardCities][kGuardPool] = {};
	const walkers::CityGate gates[] = {
		{walkers::City::Anvil, 1, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, "a"},
		{walkers::City::Anvil, 2, 5000.0f, 0.0f, 0.0f, 5000.0f, 0.0f, 0.0f, "b"},
		{walkers::City::Bruma, 3, 0.0f, 9000.0f, 0.0f, 0.0f, 9000.0f, 0.0f, "c"},
	};
	enabled[kAnvil][2] = true;
	xs[kAnvil][2] = 4800.0f;
	enabled[kBruma][0] = true;
	xs[kBruma][0] = 4800.0f;  // nearer Anvil's second gate, but a Bruma guard
	walkers::RecoverGuardState(state, enabled, xs, ys, gates, 3);
	Check(state.slots[kAnvil][2].out && state.slots[kAnvil][2].gate == 1, "an enabled one is out, at its city's nearest gate");
	Check(state.slots[kBruma][0].out && state.slots[kBruma][0].gate == 2, "only its own city's gates count");
	Check(!state.slots[kAnvil][0].out && !state.slots[kAnvil][5].out, "the disabled ones wait");
}

}  // namespace

int main() {
	TestCount();
	TestOut();
	TestReplace();
	TestIn();
	TestOff();
	TestNearGate();
	TestKill();
	TestHealth();
	TestTable();
	TestSpot();
	TestRecover();
	if (g_failures != 0) {
		std::printf("%d failed\n", g_failures);
		return 1;
	}
	std::printf("all passed\n");
	return 0;
}
