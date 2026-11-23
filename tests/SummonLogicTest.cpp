// Every flow of the player's summons (docs/walkers-spec.md 4.23 B): with
// Daedra Mode 2 a Daedra the player summoned is no ally of the walkers.

#include "SummonLogic.h"

#include <cstdio>
#include <map>

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf("  %s  %s\n", condition ? "ok  " : "FAIL", what);
	if (!condition) {
		++g_failures;
	}
}

// A fake memory: address -> word.
struct Memory {
	std::map<UInt32, UInt32> words;
	UInt32 operator()(UInt32 address) const {
		const auto it = words.find(address);
		return it != words.end() ? it->second : 0u;
	}
};

constexpr UInt32 kZdre = 'Z' | ('D' << 8) | ('R' << 16) | ('E' << 24);
constexpr UInt32 kFidg = 'F' | ('I' << 8) | ('D' << 16) | ('G' << 24);

// One effect at `effect`: its effect item at `item` with `code`, the actor at +0x3C.
void Effect(Memory& m, UInt32 effect, UInt32 item, UInt32 code, UInt32 actor) {
	m.words[effect + walkers::kActiveEffectItemOffset] = item;
	m.words[item + walkers::kEffectItemCodeOffset] = code;
	m.words[effect + walkers::kSummonActorOffset] = actor;
}

void TestSummons() {
	std::printf("The player's summoned actors from his active effects\n");
	Memory m;
	UInt32 out[4] = {};
	Check(walkers::PlayerSummons(0, m, out, 4) == 0, "no list: none");

	// node 0x100 -> 0x110 -> 0x120 -> end: a summon, a fortify, a summon
	m.words[0x100] = 0x1000; m.words[0x104] = 0x110;
	m.words[0x110] = 0x2000; m.words[0x114] = 0x120;
	m.words[0x120] = 0x3000; m.words[0x124] = 0;
	Effect(m, 0x1000, 0x5000, kZdre, 0xA000);
	Effect(m, 0x2000, 0x5100, kFidg, 0xB000);
	Effect(m, 0x3000, 0x5200, kZdre, 0xC000);
	UInt32 n = walkers::PlayerSummons(0x100, m, out, 4);
	Check(n == 2 && out[0] == 0xA000 && out[1] == 0xC000, "the summon effects' actors, the fortify skipped");
	Check(walkers::PlayerSummons(0x100, m, out, 1) == 1 && out[0] == 0xA000, "no more than room for");

	Memory m2 = m;
	m2.words[0x1000 + walkers::kSummonActorOffset] = 0;  // the summon not out yet
	m2.words[0x110] = 0;                                   // an empty node
	m2.words[0x3000 + walkers::kActiveEffectItemOffset] = 0;  // no effect item
	n = walkers::PlayerSummons(0x100, m2, out, 4);
	Check(n == 0, "no actor yet, an empty node, no effect item: none, the walk goes on");

	Memory loop;
	loop.words[0x100] = 0x1000; loop.words[0x104] = 0x100;  // a node pointing at itself
	Effect(loop, 0x1000, 0x5000, kZdre, 0xA000);
	n = walkers::PlayerSummons(0x100, loop, out, 4);
	Check(n == 4, "a broken list loops: the room caps it, no hang");
	Memory loop2;
	loop2.words[0x100] = 0x2000; loop2.words[0x104] = 0x100;
	Effect(loop2, 0x2000, 0x5100, kFidg, 0);
	Check(walkers::PlayerSummons(0x100, loop2, out, 4) == 0, "a broken list with no summon: the step cap ends it");
}

void TestAlly() {
	std::printf("Who counts as the walkers' Daedra ally\n");
	const UInt32 summons[] = {0xA000, 0xC000};
	Check(walkers::DaedraAlly(true, 0xB000, summons, 2), "a Daedra not his: an ally");
	Check(!walkers::DaedraAlly(true, 0xC000, summons, 2), "a Daedra the player summoned: no ally");
	Check(!walkers::DaedraAlly(false, 0xB000, summons, 2), "no Daedra: no ally");
	Check(walkers::DaedraAlly(true, 0xA000, summons, 0), "no summons: a Daedra is an ally");
}

}  // namespace

int main() {
	TestSummons();
	TestAlly();
	std::printf(g_failures == 0 ? "All passed\n" : "%d FAILED\n", g_failures);
	return g_failures == 0 ? 0 : 1;
}
