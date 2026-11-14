// Every flow of the shots and spells that end a walker as a lost head does
// (HeadshotLogic.h): which nodes are a head, each projectile judged once,
// a bolt at the head, overkill, burning.

#include "HeadshotLogic.h"

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

void TestHeadBone() {
	std::printf("Head bones\n");
	Check(IsHeadBone("Bip01 Head") && IsHeadBone("Bip01 Neck") && IsHeadBone("Bip01 Neck1"), "head and neck bones");
	Check(IsHeadBone("BIP01 HEAD") && IsHeadBone("bip01 head"), "in any case");
	Check(!IsHeadBone("Bip01 Spine") && !IsHeadBone("Bip01 R Forearm") && !IsHeadBone("Bip01 Pelvis"),
	      "spine, arm, pelvis: no head");
	Check(!IsHeadBone("") && !IsHeadBone(nullptr), "no name: no head");
	Check(!IsHeadBone("Bip01 Hea"), "a name that only begins so: no head");
}

void TestFreshShot() {
	std::printf("Each projectile judged once\n");
	SeenShots s;
	Check(FreshShot(s, 0xFF000123), "an arrow first seen: judged");
	Check(!FreshShot(s, 0xFF000123), "the same arrow at a later hit (stuck in the body): not again");
	Check(FreshShot(s, 0xFF000124), "another arrow: judged");
	Check(!FreshShot(s, 0), "no projectile: nothing");
	SeenShots full;
	for (UInt32 i = 1; i <= kSeenShotsMost; ++i) {
		FreshShot(full, i);
	}
	Check(full.count == kSeenShotsMost && FreshShot(full, 1000) && FreshShot(full, 1),
	      "the ring full: the newest takes the oldest's place (the oldest judged anew is long gone from the world)");
}

void TestBolt() {
	std::printf("A bolt at the head\n");
	Check(BoltAtHead(10.0f, 10.0f, 10.0f, 30.0f), "17 units off within 30: the head");
	Check(BoltAtHead(30.0f, 0.0f, 0.0f, 30.0f), "just at the radius: the head");
	Check(!BoltAtHead(0.0f, 0.0f, -45.0f, 30.0f), "45 units below (the chest): no head");
	Check(!BoltAtHead(0.0f, 0.0f, 0.0f, 0.0f), "radius 0: off");
}

void TestOverkill() {
	std::printf("Overkill\n");
	Check(Overkill(40.0f, 40.0f) && Overkill(100.0f, 40.0f), "the whole health or more at once");
	Check(!Overkill(39.0f, 40.0f), "less than the whole health: no");
	Check(!Overkill(50.0f, 0.0f) && !Overkill(50.0f, -1.0f), "no base health known: no");
	HeadshotSettings s;
	HitNotes magic;
	NoteHit(magic, 7, 1000);
	Check(SpellOverkill(s, magic, 7, 1200, 50.0f, 40.0f), "a magic effect landed 200 ms before the damage: a spell's");
	Check(!SpellOverkill(s, magic, 7, 1600, 50.0f, 40.0f), "600 ms after: a blow's, not a spell's");
	Check(!SpellOverkill(s, magic, 8, 1200, 50.0f, 40.0f), "another actor's magic: no");
	s.overkill = false;
	Check(!SpellOverkill(s, magic, 7, 1200, 50.0f, 40.0f), "Overkill=0: off");
}

void TestFire() {
	std::printf("Burning\n");
	HeadshotSettings s;
	HitNotes fire;
	NoteHit(fire, 9, 10000);
	Check(Burned(s, fire, 9, 15000), "dead 5 s after a fire hit (6 s): burned");
	Check(!Burned(s, fire, 9, 16500), "6.5 s after: no longer burning");
	Check(!Burned(s, fire, 10, 15000), "another walker: not burned");
	NoteHit(fire, 9, 16000);
	Check(Burned(s, fire, 9, 21000), "hit by fire again: the time from the newest hit");
	HitNotes wrap;
	NoteHit(wrap, 3, 0xFFFFF000u);
	Check(HitWithin(wrap, 3, 0x00000500u, 6000), "the millisecond clock wrapped between hit and death: still counted");
	s.fire = false;
	Check(!Burned(s, fire, 9, 17000), "Fire=0: off");
	s.fire = true;
	s.fireSeconds = 0.0f;
	Check(!Burned(s, fire, 9, 16000), "FireSeconds=0: off");
	HitNotes none;
	NoteHit(none, 0, 5);
	Check(none.count == 0, "no reference: not noted");
	Check(kEffectFireDamage == 0x47444946u, "the fire damage code reads FIDG in memory");
}

}  // namespace

int main() {
	TestHeadBone();
	TestFreshShot();
	TestBolt();
	TestOverkill();
	TestFire();
	std::printf(g_failures == 0 ? "All passed\n" : "%d FAILED\n", g_failures);
	return g_failures == 0 ? 0 : 1;
}
