// Every flow of keeping Maskar's Oblivion Overhaul from raising dead walkers
// (docs/walkers-spec.md 7a, "Walkers get up again and again with MOO"): who
// is stripped, MOO's forms at its load index, and the two passes in time.

#include "MooLogic.h"

#include <cstdio>

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf("  %s  %s\n", condition ? "ok  " : "FAIL", what);
	if (!condition) {
		++g_failures;
	}
}

using walkers::MooStrip;

void TestWho() {
	std::printf("Who loses MOO's token and brain\n");
	Check(walkers::StripsMoo(true, true, true), "a dead walker, MOO loaded, the setting on");
	Check(!walkers::StripsMoo(false, true, true), "the setting off: MOO raises them as it likes");
	Check(!walkers::StripsMoo(true, false, true), "no MOO: nothing to take");
	Check(!walkers::StripsMoo(true, true, false), "a vanilla zombie or anyone else: MOO's own business");
}

void TestForms() {
	std::printf("MOO's forms at its load index\n");
	Check(walkers::MooFormId(0x10, walkers::kMooTokenZombieLocalId) == 0x10042C18,
	      "the token MOOTokenZombie: index 10, local 042C18");
	Check(walkers::MooFormId(0x10, walkers::kMooZombieBrainLocalId) == 0x100F8B6D,
	      "the brain MOOZombieBrain: index 10, local 0F8B6D");
	Check(walkers::MooFormId(0x100, walkers::kMooTokenZombieLocalId) == 0, "MOO not loaded (0x100): no form");
	Check(walkers::MooFormId(0, 0x123456) == 0x00123456, "index 0 is a load index like any other");
}

void TestPasses() {
	std::printf("The two passes\n");
	MooStrip strip;
	UInt32 due[8] = {};
	Check(walkers::NoteMooStrip(strip, 0xFF000A01), "a dead walker noted");
	UInt8 pass[8] = {};
	Check(walkers::StepMooStrip(strip, 0.016f, due, 8, pass) == 1 && due[0] == 0xFF000A01 && pass[0] == 1,
	      "pass one at the next tick - MOO's own death handler has given the token by then");
	Check(walkers::StepMooStrip(strip, 1.0f, due, 8) == 0, "nothing between the passes (1 s on)");
	Check(walkers::StepMooStrip(strip, 1.0f, due, 8, pass) == 1 && due[0] == 0xFF000A01 && pass[0] == 2,
	      "pass two 2 s on - for a handler that ran after ours, still before MOO's first look at 10.8 s");
	Check(strip.count == 0 && walkers::StepMooStrip(strip, 5.0f, due, 8) == 0, "then it is dropped");

	MooStrip two;
	walkers::NoteMooStrip(two, 1);
	walkers::StepMooStrip(two, 0.5f, due, 8);
	walkers::NoteMooStrip(two, 2);
	Check(walkers::StepMooStrip(two, 1.6f, due, 8) == 2, "two walkers: the first's second pass, the second's first");
	Check(walkers::StepMooStrip(two, 2.0f, due, 8) == 1 && due[0] == 2 && two.count == 0,
	      "then the second's second pass, and both are dropped");

	MooStrip same;
	walkers::NoteMooStrip(same, 7);
	Check(walkers::NoteMooStrip(same, 7) && same.count == 1, "the same walker twice: noted once");

	MooStrip full;
	bool all = true;
	for (UInt32 i = 0; i < walkers::kMooStripMost; ++i) {
		all = all && walkers::NoteMooStrip(full, 100 + i);
	}
	Check(all && !walkers::NoteMooStrip(full, 999), "full: the next is refused, the caller logs it");
	Check(walkers::StepMooStrip(full, 0.0f, due, 8) == 8 && full.count == walkers::kMooStripMost,
	      "more due than the caller takes: those left wait for the next tick");
	Check(walkers::StepMooStrip(full, 0.0f, due, 8) == 8, "and are handed then");
}

}  // namespace

int main() {
	TestWho();
	TestForms();
	TestPasses();
	if (g_failures != 0) {
		std::printf("%d failed\n", g_failures);
		return 1;
	}
	std::printf("all passed\n");
	return 0;
}
