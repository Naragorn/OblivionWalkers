// Every flow of a dead Daedra going back to Oblivion (docs/walkers-spec.md
// 4.22 idea 8, 4.22a step 5b): which are watched, the shader and the
// banishing by the time dead, a Daedra up again, one gone, the pile.

#include "BanishLogic.h"

#include <cstdio>
#include <cstring>

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf("  %s  %s\n", condition ? "ok  " : "FAIL", what);
	if (!condition) {
		++g_failures;
	}
}

using walkers::BanishAction;
using walkers::BanishEntry;
using walkers::BanishList;
using walkers::BanishSettings;

void TestWatch() {
	std::printf("Which are watched\n");
	BanishList b;
	Check(walkers::WatchForBanish(b, 0xFF000020) && walkers::WatchedForBanish(b, 0xFF000020), "a placed Daedra");
	Check(!walkers::WatchForBanish(b, 0xFF000020), "twice: once");
	Check(!walkers::WatchForBanish(b, 0x0001A2B3), "the game's own (an Oblivion gate's): never");
	Check(!walkers::WatchedForBanish(b, 0xFF000021), "another: not watched");
	BanishList full;
	for (UInt32 i = 0; i < walkers::kBanishMost; ++i) {
		walkers::WatchForBanish(full, 0xFF002000 + i);
	}
	Check(!walkers::WatchForBanish(full, 0xFF00FFFF), "full: refused");
}

void TestStep() {
	std::printf("The time dead\n");
	BanishSettings s;
	Check(s.seconds == 10.0f && s.fadeSeconds == 2.0f && s.shader == 0x0018B576 && s.pileUnits == 8000.0f,
	      "defaults: 10 s, 2 s of the shader, effectEnchantConjuration, piles go at 8000");
	BanishEntry e{0xFF000020, 0.0f, walkers::BanishStage::Watching};
	Check(walkers::StepBanishOne(e, s, true, false, 1.0f) == BanishAction::None && e.down == 0.0f, "alive: nothing");
	Check(walkers::StepBanishOne(e, s, true, true, 9.0f) == BanishAction::None, "dead 9 s: nothing yet");
	Check(walkers::StepBanishOne(e, s, true, true, 1.0f) == BanishAction::Shader, "dead 10 s: the shader");
	Check(walkers::StepBanishOne(e, s, true, true, 1.0f) == BanishAction::None, "11 s: it plays");
	Check(walkers::StepBanishOne(e, s, true, true, 1.0f) == BanishAction::Banish, "12 s: banished");
	BanishEntry up{0xFF000021, 0.0f, walkers::BanishStage::Watching};
	walkers::StepBanishOne(up, s, true, true, 10.0f);
	Check(walkers::StepBanishOne(up, s, true, false, 1.0f) == BanishAction::None && up.down == 0.0f &&
	          up.stage == walkers::BanishStage::Watching,
	      "up again during the shader (a resurrect): its time starts again");
	Check(walkers::StepBanishOne(up, s, false, true, 1.0f) == BanishAction::Forget, "gone from the game: forgotten");
	BanishSettings off;
	off.seconds = 0.0f;
	BanishEntry kept{0xFF000022, 0.0f, walkers::BanishStage::Watching};
	Check(walkers::StepBanishOne(kept, off, true, true, 100.0f) == BanishAction::None,
	      "BanishSeconds=0: the body stays");
}

void TestDrop() {
	std::printf("Off the list\n");
	BanishList b;
	walkers::WatchForBanish(b, 0xFF000001);
	walkers::WatchForBanish(b, 0xFF000002);
	walkers::WatchForBanish(b, 0xFF000003);
	const bool drop[3] = {true, false, true};
	walkers::DropBanished(b, drop);
	Check(b.count == 1 && b.items[0].ref == 0xFF000002, "the banished and the forgotten off, the rest kept");
}

void TestBagScan() {
	std::printf("The bag found by its ID\n");
	Check(walkers::IsBagRef(0x0000000E) && !walkers::IsBagRef(0x00037680), "the game's LootBag, nothing else");
	Check(walkers::NextBagScan(0xFF000000, 0xFF000F94, 0xFF000FB2) == 0xFF000FB3, "found: the next scan past it");
	Check(walkers::NextBagScan(0xFF000F00, 0xFF000F94, 0) == 0xFF000F94, "none: from the corpse, the newer");
	Check(walkers::NextBagScan(0xFF001000, 0xFF000F94, 0) == 0xFF001000, "none: the last start, the newer");
}

void TestPile() {
	std::printf("A pile goes\n");
	BanishSettings s;
	Check(!walkers::PileGoes(s, 7999.0f) && walkers::PileGoes(s, 8000.0f), "at 8000 units");
	Check(!walkers::PileGoes(s, -1.0f), "not known: it stays");
}

}  // namespace

int main() {
	TestWatch();
	TestStep();
	TestDrop();
	TestBagScan();
	TestPile();
	std::printf(g_failures == 0 ? "PASS\n" : "FAIL (%d)\n", g_failures);
	return g_failures == 0 ? 0 : 1;
}
