// Every flow of finding the creature sound lists a walker's death sound is
// drawn from (the engine's TESCreature::GetSound 0x0051CEC0), and of when
// the sound held back at its unseen death is played at its get-up.

#include "SoundLogic.h"

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

// A few creature bases in a pretend memory: address -> word.
struct Memory {
	std::map<UInt32, UInt32> words;
	UInt32 operator()(UInt32 address) const {
		const auto found = words.find(address);
		return found != words.end() ? found->second : 0u;
	}
	void Creature(UInt32 base, bool ownSounds, UInt32 at0x100) {
		words[base + walkers::kCreatureFlagsOffset] = ownSounds ? walkers::kCreatureHasSounds : 0x40u;
		words[base + walkers::kCreatureSoundsOffset] = at0x100;
	}
};

void TestLists() {
	std::printf("The sound lists a creature plays from\n");
	Memory m;
	m.Creature(0x1000, true, 0x9000);
	Check(walkers::SoundListsOf(0x1000, m) == 0x9000, "sounds of its own: its own lists");
	m.Creature(0x2000, false, 0x1000);
	Check(walkers::SoundListsOf(0x2000, m) == 0x9000,
	      "a walker (flags 0240, no sounds of its own): its sound base's lists - CreatureZombie's");
	m.Creature(0x3000, false, 0x2000);
	Check(walkers::SoundListsOf(0x3000, m) == 0x9000, "a sound base that has one itself: followed on");
	m.Creature(0x4000, true, 0);
	Check(walkers::SoundListsOf(0x4000, m) == 0, "sounds of its own but no lists: none, as the engine gives none");
	m.Creature(0x5000, false, 0);
	Check(walkers::SoundListsOf(0x5000, m) == 0, "no sounds and no sound base: none");
	m.Creature(0x6000, false, 0x7000);
	m.Creature(0x7000, false, 0x6000);
	Check(walkers::SoundListsOf(0x6000, m) == 0, "sound bases in a ring: none, after a few steps (the engine would hang)");
	Check(walkers::SoundListsOf(0, m) == 0, "no base: none");
}

void TestDeathSlot() {
	std::printf("The death list's slot\n");
	Check(walkers::DeathListSlot(0x9000) == 0x9000 + 8 * 4, "the lists' ninth head - Death is type 8 of ten");
	Check(walkers::DeathListSlot(0) == 0, "no lists: no slot");
}

void TestGetUpSound() {
	std::printf("The sound at the get-up\n");
	Check(walkers::GetUpSoundDue(true, 0x0001A2B3), "held back at the unseen death, played as it gets up");
	Check(!walkers::GetUpSoundDue(false, 0x0001A2B3), "[Rise] SoundAtGetUp=0: it gets up silent");
	Check(!walkers::GetUpSoundDue(true, 0), "no sound was drawn (the lists' chances gave none): none to play");
}

}  // namespace

int main() {
	TestLists();
	TestDeathSlot();
	TestGetUpSound();
	if (g_failures != 0) {
		std::printf("%d failed\n", g_failures);
		return 1;
	}
	std::printf("all passed\n");
	return 0;
}
