// Every flow of the safeguard (docs/walkers-spec.md 4.11b): who the walkers
// killed is noted - not the walkers, not the player, not the gate guards,
// not those who rise as walkers - and raised once dead AfterSeconds with no
// living walker within Radius; the list survives a save.

#include "SafeguardLogic.h"

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

using walkers::SafeguardList;
using walkers::SafeguardSettings;
using walkers::Victim;
using walkers::VictimNow;

void TestTakes() {
	std::printf("Who is noted\n");
	SafeguardSettings s;
	Victim v;
	v.killedByWalker = true;
	Check(walkers::SafeguardTakes(s, v), "killed by a walker: noted");
	Victim other = v;
	other.killedByWalker = false;
	Check(!walkers::SafeguardTakes(s, other), "killed by anyone else (the player, a guard, a fall): not");
	other = v;
	other.walker = true;
	Check(!walkers::SafeguardTakes(s, other), "a walker itself: not");
	other = v;
	other.player = true;
	Check(!walkers::SafeguardTakes(s, other), "the player: not");
	other = v;
	other.gateGuard = true;
	Check(!walkers::SafeguardTakes(s, other), "a gate guard: not (the pool raises its own)");
	other = v;
	other.rises = true;
	Check(!walkers::SafeguardTakes(s, other), "one who rises as a walker: not");
	s.enabled = false;
	Check(!walkers::SafeguardTakes(s, v), "[Safeguard] Enabled=0: nobody");
}

void TestNoKiller() {
	std::printf("Dead without a killer\n");
	SafeguardSettings s;
	Victim v;
	v.killerUnknown = true;
	Check(!walkers::SafeguardTakes(s, v) && walkers::SafeguardAsks(s, v),
	      "no killer handed over (a bleed, a fall in a fight): asked about - a walker near then counts as the killer");
	Victim known = v;
	known.killerUnknown = false;
	Check(!walkers::SafeguardAsks(s, known), "a killer handed over that is no walker: not asked about");
	Victim other = v;
	other.walker = true;
	Check(!walkers::SafeguardAsks(s, other), "a walker: not");
	other = v;
	other.player = true;
	Check(!walkers::SafeguardAsks(s, other), "the player: not");
	other = v;
	other.gateGuard = true;
	Check(!walkers::SafeguardAsks(s, other), "a gate guard: not");
	other = v;
	other.rises = true;
	Check(!walkers::SafeguardAsks(s, other), "one who rises: not");
	s.enabled = false;
	Check(!walkers::SafeguardAsks(s, v), "off: not");
}

void TestNote() {
	std::printf("The list\n");
	SafeguardList list;
	Check(walkers::NoteVictim(list, 0x100) && list.count == 1, "a first victim");
	Check(walkers::NoteVictim(list, 0x100) && list.count == 1, "the same again: once");
	for (UInt32 i = 1; i < walkers::kSafeguardMost; ++i) {
		walkers::NoteVictim(list, 0x200 + i);
	}
	Check(list.count == walkers::kSafeguardMost && !walkers::NoteVictim(list, 0x9999),
	      "full: one more is refused (the caller logs it)");
}

struct World {
	VictimNow now[4] = {VictimNow::Dead, VictimNow::Dead, VictimNow::Dead, VictimNow::Dead};
	bool safe[4] = {true, true, true, true};
	UInt32 Index(UInt32 id) const { return id - 0x10; }
};

// Who left the list without being raised, and why (the caller logs it).
struct Left {
	UInt32 id[8] = {};
	VictimNow now[8] = {};
	float seconds[8] = {};
	UInt32 count = 0;
};

UInt32 Step(SafeguardList& list, const SafeguardSettings& s, World& world, float dt, UInt32* due, UInt32 most = 8,
            Left* left = nullptr, float nowDays = -1.0f) {
	return walkers::StepSafeguard(
		list, s, dt, [&](UInt32 id) { return world.now[world.Index(id)]; },
		[&](UInt32 id) { return world.safe[world.Index(id)]; },
		[&](UInt32 id, VictimNow now, float seconds) {
			if (left != nullptr && left->count < 8) {
				left->id[left->count] = id;
				left->now[left->count] = now;
				left->seconds[left->count] = seconds;
				++left->count;
			}
		},
		due, most, nowDays);
}

void TestStep() {
	std::printf("Raised when safe\n");
	SafeguardSettings s;
	s.afterSeconds = 10.0f;
	SafeguardList list;
	World world;
	walkers::NoteVictim(list, 0x10);
	walkers::NoteVictim(list, 0x11);
	UInt32 due[8];
	UInt32 count = 0;
	for (int t = 0; t < 9; ++t) {
		count += Step(list, s, world, 1.0f, due);
	}
	Check(count == 0 && list.count == 2, "9 s dead: both still down");
	world.safe[1] = false;
	count = Step(list, s, world, 1.0f, due);
	Check(count == 1 && due[0] == 0x10 && list.count == 1, "AfterSeconds dead, no walker near: raised, off the list");
	for (int t = 0; t < 20; ++t) {
		count = Step(list, s, world, 1.0f, due);
	}
	Check(count == 0 && list.count == 1, "a walker near: kept down, however long");
	world.safe[1] = true;
	count = Step(list, s, world, 1.0f, due);
	Check(count == 1 && due[0] == 0x11 && list.count == 0, "the walker gone: raised at once (the time counted on)");
	walkers::NoteVictim(list, 0x12);
	walkers::NoteVictim(list, 0x13);
	for (int t = 0; t < 3; ++t) {
		Step(list, s, world, 1.0f, due);
	}
	world.safe[2] = world.safe[3] = false;
	Step(list, s, world, 1.0f, due);
	world.now[2] = VictimNow::Alive;
	world.now[3] = VictimNow::Gone;
	Left left;
	count = Step(list, s, world, 1.0f, due, 8, &left);
	Check(count == 0 && list.count == 0, "up again by other means, or gone from the game: off the list, nothing done");
	Check(left.count == 2 && left.id[0] == 0x12 && left.now[0] == VictimNow::Alive && left.id[1] == 0x13 &&
	          left.now[1] == VictimNow::Gone && left.seconds[0] == 4.0f && left.seconds[1] == 4.0f,
	      "both handed to the caller with why and how long dead (the Black Guar of 2026-10-26 left unlogged)");
	world.safe[2] = world.safe[3] = true;
	for (UInt32 id = 0x10; id < 0x14; ++id) {
		world.now[world.Index(id)] = VictimNow::Dead;
		walkers::NoteVictim(list, id);
	}
	for (int t = 0; t < 9; ++t) {
		Step(list, s, world, 1.0f, due, 2);
	}
	Left raised;
	count = Step(list, s, world, 1.0f, due, 2, &raised);
	Check(count == 2 && list.count == 2, "more due than asked for: the rest the next step");
	Check(raised.count == 0, "the raised and the kept are not handed over as left");
	s.enabled = false;
	Check(Step(list, s, world, 1.0f, due) == 0 && list.count == 2, "off: nothing raised, the list kept");
}

// The same as the rise's waiting list (rise-after-away.txt 2026-10-27
// 10:22): a victim whose reference the game freed with its cell keeps its
// place, the time dead standing, for kForgetMissingDays game days.
void TestMissing() {
	std::printf("A victim the game does not hold right now\n");
	SafeguardSettings s;
	s.afterSeconds = 10.0f;
	SafeguardList list;
	World world;
	walkers::NoteVictim(list, 0x10);
	UInt32 due[8];
	Step(list, s, world, 4.0f, due);
	world.now[0] = VictimNow::Missing;
	Left left;
	Check(Step(list, s, world, 20.0f, due, 8, &left, 5.0f) == 0 && list.count == 1 && list.entries[0].seconds == 4.0f &&
	          list.entries[0].missingSince == 5.0f && left.count == 0,
	      "missing: kept, the time dead standing, the day noted, not handed over as left");
	world.now[0] = VictimNow::Dead;
	Check(Step(list, s, world, 6.0f, due, 8, nullptr, 6.0f) == 1 && due[0] == 0x10,
	      "held again: the time runs on from 4 s, raised at 10");
	walkers::NoteVictim(list, 0x11);
	world.now[1] = VictimNow::Missing;
	Step(list, s, world, 1.0f, due, 8, nullptr, 7.0f);
	Left forgotten;
	Check(Step(list, s, world, 1.0f, due, 8, &forgotten, 10.0f) == 0 && list.count == 0 && forgotten.count == 1 &&
	          forgotten.id[0] == 0x11 && forgotten.now[0] == VictimNow::Missing,
	      "missing three game days: off the list, handed over as missing");
}

void TestImage() {
	std::printf("In the co-save\n");
	SafeguardList list;
	walkers::NoteVictim(list, 0x10);
	walkers::NoteVictim(list, 0x11);
	list.entries[0].seconds = 4.5f;
	UInt8 image[walkers::kSafeguardImageMost];
	const UInt32 length = walkers::SafeguardImage(list, image, sizeof(image));
	SafeguardList back;
	const bool whole = walkers::ReadSafeguardImage(back, image, length, [](UInt32 id, UInt32& out) {
		out = id + 0x1000;
		return id != 0x11;
	});
	Check(whole && back.count == 1 && back.entries[0].refId == 0x1010 && back.entries[0].seconds == 4.5f,
	      "read back: resolved anew, the seconds kept, an unresolved one dropped");
	list.entries[0].missingSince = 7.25f;
	const UInt32 withMissing = walkers::SafeguardImage(list, image, sizeof(image));
	SafeguardList again;
	Check(withMissing == 4 + 2 * 12 && walkers::ReadSafeguardImage(again, image, withMissing, [](UInt32 id, UInt32& out) {
		out = id;
		return true;
	}) && again.entries[0].missingSince == 7.25f && again.entries[1].missingSince == -1.0f,
	      "version 2: twelve bytes an entry, the missing day kept");
	UInt8 old[4 + 8] = {1, 0, 0, 0, 0x10, 0, 0, 0, 0, 0, 0x90, 0x40};
	SafeguardList fromOld;
	Check(walkers::ReadSafeguardImage(fromOld, old, sizeof(old), [](UInt32 id, UInt32& out) {
		out = id;
		return true;
	}, 1) && fromOld.count == 1 && fromOld.entries[0].seconds == 4.5f && fromOld.entries[0].missingSince == -1.0f,
	      "a version 1 image (eight bytes an entry): read, nobody missing");
	list.entries[0].missingSince = -1.0f;
	walkers::SafeguardImage(list, image, sizeof(image));
	Check(walkers::SafeguardImage(list, image, 4) == 0, "too small a buffer: nothing written");
	SafeguardList cut;
	Check(!walkers::ReadSafeguardImage(cut, image, length - 3, [](UInt32 id, UInt32& out) {
		out = id;
		return true;
	}) && cut.count == 1,
	      "cut short: false, what was whole read");
}

}  // namespace

int main() {
	TestTakes();
	TestNoKiller();
	TestNote();
	TestStep();
	TestMissing();
	TestImage();
	if (g_failures != 0) {
		std::printf("%d failed\n", g_failures);
		return 1;
	}
	std::printf("all passed\n");
	return 0;
}
