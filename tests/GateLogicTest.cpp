// Every flow of an Oblivion gate's state (docs/walkers-spec.md 4.22, step 3's
// groundwork): open, not yet open, closed, a parent's state, the table read
// off Oblivion.esm.

#include "GateLogic.h"

#include <cstdio>

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf("  %s  %s\n", condition ? "ok  " : "FAIL", what);
	if (!condition) {
		++g_failures;
	}
}

using walkers::GateOpen;
using walkers::kFormFlagDestroyed;
using walkers::kFormFlagDisabled;

void TestOpen() {
	std::printf("A gate open or not\n");
	Check(GateOpen(0, true, 0), "door and parent enabled, not destroyed: open");
	Check(!GateOpen(0, true, kFormFlagDisabled), "its parent disabled: not yet opened");
	Check(!GateOpen(kFormFlagDisabled, true, 0), "the door itself disabled: not open");
	Check(!GateOpen(kFormFlagDestroyed, true, 0) && walkers::GateClosed(kFormFlagDestroyed),
	      "destroyed (its sigil stone taken): closed, though the door stays enabled");
	Check(!GateOpen(kFormFlagDestroyed, true, kFormFlagDisabled), "destroyed and its parent off: not open");
	Check(GateOpen(0, false, kFormFlagDisabled), "no parent: the door alone decides");
	Check(GateOpen(0x00000008 | 0x00004000, true, 0x00000400), "other flags of either: no matter");
	Check(!walkers::GateClosed(0) && !walkers::GateClosed(kFormFlagDisabled), "not destroyed: not closed");
	Check(kFormFlagDestroyed == (1u << 13), "the flag GetDestroyed reads: bit 13 (0x004F82C0)");
}

void TestTable() {
	std::printf("The gates read off Oblivion.esm (OblivionGates.h)\n");
	UInt32 random = 0;
	UInt32 city = 0;
	UInt32 parents = 0;
	bool inTamriel = true;
	for (const walkers::OblivionGateRef& g : walkers::kOblivionGates) {
		random += g.kind == walkers::GateKind::Random ? 1u : 0u;
		city += g.kind == walkers::GateKind::City ? 1u : 0u;
		parents += g.parent != 0 ? 1u : 0u;
		inTamriel = inTamriel && g.x > -300000.0f && g.x < 300000.0f && g.y > -300000.0f && g.y < 300000.0f &&
		            (g.door >> 24) == 0;
	}
	Check(random == 90, "90 random gates, as 4.22 counted the doors of OblivionGatetoOblivion");
	Check(city == 6, "six on the cities' script: the five of MQ11 and MS13's");
	Check(parents == walkers::kOblivionGateCount, "each with an enable parent");
	Check(inTamriel, "all in Tamriel's bounds, all Oblivion.esm's");
}

}  // namespace

int main() {
	TestOpen();
	TestTable();
	if (g_failures != 0) {
		std::printf("%d failed\n", g_failures);
		return 1;
	}
	std::printf("all passed\n");
	return 0;
}
