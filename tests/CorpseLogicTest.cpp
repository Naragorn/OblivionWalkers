// Every flow of the ended walkers' bodies taken out of the world
// (CorpseLogic.h).

#include "CorpseLogic.h"

#include <cstdio>

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf("  %s  %s\n", condition ? "ok  " : "FAIL", what);
	if (!condition) {
		++g_failures;
	}
}

using namespace walkers;

CorpseFacts Ended(float distance) {
	CorpseFacts f;
	f.dead = true;
	f.made = true;
	f.distance = distance;
	return f;
}

void TestGoes() {
	std::printf("Which bodies go\n");
	CorpseSettings s;
	Check(CorpseGoes(s, Ended(4000.0f)) && CorpseGoes(s, Ended(9000.0f)), "ended, 4000 units off or more: goes");
	Check(!CorpseGoes(s, Ended(3999.0f)), "nearer: stays (in sight)");
	CorpseFacts loot = Ended(5000.0f);
	loot.carriesLoot = true;
	Check(!CorpseGoes(s, loot), "carrying the fallen's things, 5000 off: stays");
	loot.distance = 8000.0f;
	Check(CorpseGoes(s, loot), "carrying them, 8000 off: goes");
	CorpseFacts waiting = Ended(9000.0f);
	waiting.waiting = true;
	Check(!CorpseGoes(s, waiting), "one that gets up again: stays");
	CorpseFacts placed = Ended(9000.0f);
	placed.made = false;
	Check(!CorpseGoes(s, placed), "a placed spawn or a pool's walker (not made in the game): left to the cell reset or its pool");
	Check(MadeInGame(0xFF0110AB) && !MadeInGame(0x0001A2B3) && !MadeInGame(0x37000953),
	      "made in the game: an FF reference; Oblivion.esm's and the ESP's are not");
	CorpseFacts alive = Ended(9000.0f);
	alive.dead = false;
	Check(!CorpseGoes(s, alive), "alive: stays");
	CorpseFacts gone = Ended(9000.0f);
	gone.disabled = true;
	Check(!CorpseGoes(s, gone), "disabled already: nothing");
	Check(!CorpseGoes(s, Ended(-1.0f)), "distance not known (another worldspace): stays");
	s.enabled = false;
	Check(!CorpseGoes(s, Ended(9000.0f)), "Enabled=0: none goes");
}

void TestLies() {
	std::printf("Which bodies are counted\n");
	Check(CorpseLies(Ended(10.0f)), "ended in the world: counted");
	CorpseFacts waiting = Ended(10.0f);
	waiting.waiting = true;
	CorpseFacts gone = Ended(10.0f);
	gone.disabled = true;
	CorpseFacts alive = Ended(10.0f);
	alive.dead = false;
	Check(!CorpseLies(waiting) && !CorpseLies(gone) && !CorpseLies(alive), "getting up, gone, alive: not counted");
}

}  // namespace

int main() {
	TestGoes();
	TestLies();
	std::printf(g_failures == 0 ? "All passed\n" : "%d FAILED\n", g_failures);
	return g_failures == 0 ? 0 : 1;
}
