// Every flow of being grabbed by walkers (docs/walkers-spec.md 4.19; Nadi,
// 2026-10-28: "go a + b", the bites the player takes settable, 3 by default,
// no third walker at the neck, a grab only at a landed hit): which arm, who
// may grab, held and rooted, the bites, letting go, the message to OBVR.

#include "GrabLogic.h"

#include <cmath>
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

using walkers::Arm;
using walkers::GrabEvent;
using walkers::GrabRefusal;
using walkers::GrabSettings;
using walkers::GrabState;
using walkers::HolderView;
using walkers::LetGo;
using walkers::PlayerHold;

constexpr float kPi = 3.14159265f;

void TestSide() {
	std::printf("Which side a walker stands on\n");
	// Heading 0 looks along +y; its right is +x.
	Check(walkers::SideOf(0.0f, 50.0f, 80.0f) == Arm::Right, "facing north, a walker ahead and to the east: right");
	Check(walkers::SideOf(0.0f, -50.0f, 80.0f) == Arm::Left, "facing north, ahead and to the west: left");
	Check(walkers::SideOf(kPi / 2.0f, 0.0f, -70.0f) == Arm::Right,
	      "facing east (heading pi/2), a walker to the south: right");
	Check(walkers::SideOf(kPi / 2.0f, 0.0f, 70.0f) == Arm::Left, "facing east, to the north: left");
	Check(walkers::SideOf(0.0f, 0.0f, 80.0f) == Arm::Right, "straight ahead: the right arm (the weapon arm first)");
}

void TestGrab() {
	std::printf("Who may grab\n");
	GrabSettings s;
	GrabState g;
	Arm arm = Arm::None;
	Check(walkers::TryGrab(g, s, 0xFF000A01, Arm::Right, 10, &arm) == GrabRefusal::None && arm == Arm::Right &&
	          g.count == 1,
	      "a landed hit, rolled under the chance: the walker holds the arm on its side");
	Check(walkers::Holds(g, 0xFF000A01) && !walkers::Holds(g, 0xFF000A02), "the holder is known, another is not");
	Check(walkers::TryGrab(g, s, 0xFF000A01, Arm::Left, 10, &arm) == GrabRefusal::AlreadyHolding && g.count == 1,
	      "the holder hits again: it holds already, nothing new");
	Check(walkers::TryGrab(g, s, 0xFF000A02, Arm::Right, 10, &arm) == GrabRefusal::None && arm == Arm::Left &&
	          g.count == 2,
	      "a second walker on the held right side: it takes the free left arm");
	Check(walkers::TryGrab(g, s, 0xFF000A03, Arm::Left, 0, &arm) == GrabRefusal::BothArmsHeld && g.count == 2,
	      "a third: both arms held, no grab (no walker at the neck, Nadi 2026-10-28)");
	GrabState fresh;
	Check(walkers::TryGrab(fresh, s, 0xFF000A04, Arm::Left, 50, &arm) == GrabRefusal::NotRolled && fresh.count == 0,
	      "rolled 50 against a chance of 50: no grab, the hit is only a hit");
	Check(walkers::TryGrab(fresh, s, 0xFF000A04, Arm::Left, 49, &arm) == GrabRefusal::None,
	      "rolled 49 against 50: grabbed");
	GrabSettings off;
	off.enabled = false;
	GrabState none;
	Check(walkers::TryGrab(none, off, 0xFF000A05, Arm::Left, 0, &arm) == GrabRefusal::Off && none.count == 0,
	      "the setting off: never");
	GrabSettings never;
	never.chancePercent = 0;
	Check(walkers::TryGrab(none, never, 0xFF000A05, Arm::Left, 0, &arm) == GrabRefusal::NotRolled,
	      "a chance of 0: never");
	GrabSettings always;
	always.chancePercent = 100;
	Check(walkers::TryGrab(none, always, 0xFF000A05, Arm::Left, 99, &arm) == GrabRefusal::None,
	      "a chance of 100: always");
	GrabState odd;
	Check(walkers::TryGrab(odd, s, 0, Arm::Left, 0, &arm) == GrabRefusal::NoWalker && odd.count == 0,
	      "no reference: nothing");
	Check(walkers::ArmFor(g, Arm::Right) == Arm::None && walkers::ArmFor(GrabState{}, Arm::None) == Arm::Right,
	      "no free arm: none; no side known: the right");
}

void TestHold() {
	std::printf("Held and rooted\n");
	GrabSettings s;
	s.chancePercent = 100;
	GrabState g;
	Arm arm = Arm::None;
	Check(walkers::HoldOf(g) == PlayerHold::Free, "nobody holds: free");
	walkers::TryGrab(g, s, 0xFF000B01, Arm::Left, 0, &arm);
	Check(walkers::HoldOf(g) == PlayerHold::Rooted, "one holder: rooted already (Nadi, 2026-10-30: walked away)");
	walkers::TryGrab(g, s, 0xFF000B02, Arm::Left, 0, &arm);
	Check(walkers::HoldOf(g) == PlayerHold::Rooted, "two holders: rooted");
	using walkers::kMoveBackward;
	using walkers::kMoveForward;
	using walkers::kMoveLeft;
	using walkers::kMoveRight;
	using walkers::kMoveRun;
	using walkers::kMoveWalk;
	const UInt16 running = kMoveForward | kMoveLeft | kMoveRun;
	Check(walkers::HeldPlayerFlags(running, PlayerHold::Free) == running, "free: the player's flags untouched");
	Check(walkers::HeldPlayerFlags(running | kMoveRight | kMoveBackward | 0x0400, PlayerHold::Rooted) == 0x0400,
	      "rooted: no step in any direction, no run or walk; anything else kept");
	Check(walkers::HolderFlags(kMoveForward | kMoveRun | 0x0400) == 0x0400,
	      "the holder: no step and no pace either, it stands at the arm it holds");
}

HolderView Near(float distance = 80.0f) { return HolderView{true, false, distance}; }

void TestBites() {
	std::printf("The bites\n");
	GrabSettings s;  // a bite every 4 s, the third kills
	s.chancePercent = 100;
	GrabState g;
	Arm arm = Arm::None;
	walkers::TryGrab(g, s, 0xFF000C01, Arm::Right, 0, &arm);
	GrabEvent ev[8];
	HolderView one[1] = {Near()};
	Check(walkers::StepGrab(g, s, 3.9f, one, ev, 8) == 0, "held 3.9 s: no bite yet");
	UInt32 n = walkers::StepGrab(g, s, 0.2f, one, ev, 8);
	Check(n == 1 && ev[0].kind == GrabEvent::Bite && ev[0].refId == 0xFF000C01 && ev[0].biteNumber == 1 &&
	          !ev[0].lethal && g.bites == 1,
	      "at 4 s: the first bite, not the last");
	n = walkers::StepGrab(g, s, 4.0f, one, ev, 8);
	Check(n == 1 && ev[0].biteNumber == 2 && !ev[0].lethal, "4 s later the second");
	n = walkers::StepGrab(g, s, 4.0f, one, ev, 8);
	Check(n == 1 && ev[0].biteNumber == 3 && ev[0].lethal, "the third: the last the player takes (BitesToDie 3)");
	GrabSettings five = s;
	five.bitesToDie = 5;
	GrabState g5;
	walkers::TryGrab(g5, five, 0xFF000C02, Arm::Right, 0, &arm);
	UInt32 lethalAt = 0;
	for (UInt32 i = 1; i <= 6 && lethalAt == 0; ++i) {
		if (walkers::StepGrab(g5, five, 4.0f, one, ev, 8) == 1 && ev[0].lethal) {
			lethalAt = i;
		}
	}
	Check(lethalAt == 5, "BitesToDie 5: the fifth bite is the last");
	GrabState two;
	walkers::TryGrab(two, s, 0xFF000C03, Arm::Right, 0, &arm);
	walkers::StepGrab(two, s, 2.0f, one, ev, 8);
	walkers::TryGrab(two, s, 0xFF000C04, Arm::Left, 0, &arm);
	HolderView both[2] = {Near(), Near()};
	n = walkers::StepGrab(two, s, 2.0f, both, ev, 8);
	Check(n == 1 && ev[0].refId == 0xFF000C03, "two holders: each bites on its own clock, the first at its 4 s");
	n = walkers::StepGrab(two, s, 2.0f, both, ev, 8);
	Check(n == 1 && ev[0].refId == 0xFF000C04 && two.bites == 2,
	      "the second at its own 4 s - two holders bite twice as often");
	GrabState after;
	walkers::TryGrab(after, s, 0xFF000C05, Arm::Right, 0, &arm);
	walkers::TryGrab(after, s, 0xFF000C06, Arm::Left, 0, &arm);
	n = walkers::StepGrab(after, s, 8.0f, both, ev, 8);
	Check(n == 2 && after.bites == 2 && !ev[0].lethal && !ev[1].lethal, "a long frame: each holder bites once");
	n = walkers::StepGrab(after, s, 4.0f, both, ev, 8);
	Check(n == 1 && ev[0].lethal && after.bites == 3, "the lethal bite ends the step: no bite after the last");
	Check(walkers::StepGrab(after, s, 4.0f, both, ev, 8) == 0, "and none after it, ever, until the state is reset");
	GrabState capped;
	walkers::TryGrab(capped, s, 0xFF000C07, Arm::Right, 0, &arm);
	walkers::TryGrab(capped, s, 0xFF000C08, Arm::Left, 0, &arm);
	Check(walkers::StepGrab(capped, s, 4.0f, both, ev, 1) == 1, "no more events than the caller holds");
}

void TestBiteDamage() {
	std::printf("What a bite takes\n");
	GrabSettings s;  // 20 % of the base health
	bool kills = false;
	Check(std::fabs(walkers::BiteDamage(s, 200.0f, 150.0f, false, false, &kills) - 40.0f) < 0.01f && !kills,
	      "base 200, at 150: a bite takes 40");
	Check(std::fabs(walkers::BiteDamage(s, 200.0f, 30.0f, false, false, &kills) - 30.0f) < 0.01f && kills,
	      "at 30: the bite takes the rest - low health dies before the last bite (never below 0)");
	Check(std::fabs(walkers::BiteDamage(s, 200.0f, 150.0f, true, false, &kills) - 150.0f) < 0.01f && kills,
	      "the last bite: all that is left");
	Check(std::fabs(walkers::BiteDamage(s, 200.0f, 150.0f, true, true, &kills) - 149.0f) < 0.01f && !kills,
	      "the last bite in god mode: one point is left, the player lives");
	Check(std::fabs(walkers::BiteDamage(s, 200.0f, 30.0f, false, true, &kills) - 29.0f) < 0.01f && !kills,
	      "a bite at low health in god mode: one point left");
	Check(walkers::BiteDamage(s, 200.0f, 0.5f, false, true, &kills) == 0.0f && !kills,
	      "god mode under one point: nothing taken");
	Check(walkers::BiteDamage(s, 200.0f, -1.0f, false, false, &kills) == 0.0f && !kills,
	      "health not read: nothing taken");
	GrabSettings none = s;
	none.biteHealthPercent = 0;
	Check(walkers::BiteDamage(none, 200.0f, 150.0f, false, false, &kills) == 0.0f && !kills,
	      "BiteHealthPercent 0: a bite takes nothing, only the last kills");
}

void TestLetGo() {
	std::printf("Letting go\n");
	GrabSettings s;  // two hits free, torn free beyond 160
	s.chancePercent = 100;
	GrabState g;
	Arm arm = Arm::None;
	walkers::TryGrab(g, s, 0xFF000D01, Arm::Right, 0, &arm);
	walkers::TryGrab(g, s, 0xFF000D02, Arm::Left, 0, &arm);
	Check(!walkers::OnStruck(g, s, 0xFF000D01) && g.count == 2, "the player's first blow on a holder: still held");
	Check(walkers::OnStruck(g, s, 0xFF000D01) && g.count == 1 && !walkers::Holds(g, 0xFF000D01),
	      "the second blow: it lets go (StruggleHits 2)");
	Check(!walkers::OnStruck(g, s, 0xFF000D09), "a blow on a walker that holds nothing: nothing");
	GrabEvent ev[8];
	HolderView dead[1] = {HolderView{true, true, 80.0f}};
	UInt32 n = walkers::StepGrab(g, s, 0.1f, dead, ev, 8);
	Check(n == 1 && ev[0].kind == GrabEvent::Release && ev[0].why == LetGo::Dead && ev[0].arm == Arm::Left &&
	          g.count == 0,
	      "a holder cut down: it lets go");
	walkers::TryGrab(g, s, 0xFF000D03, Arm::Right, 0, &arm);
	HolderView apart[1] = {HolderView{true, false, 161.0f}};
	n = walkers::StepGrab(g, s, 0.1f, apart, ev, 8);
	Check(n == 0 && g.count == 1, "161 units apart (VR's room, a shove): still held - walking off frees no one");
	HolderView far[1] = {HolderView{true, false, 1001.0f}};
	n = walkers::StepGrab(g, s, 0.1f, far, ev, 8);
	Check(n == 1 && ev[0].why == LetGo::TornFree && g.count == 0,
	      "1001 units apart (a door, a teleport): torn free (TearFreeDistance 1000)");
	walkers::TryGrab(g, s, 0xFF000D04, Arm::Right, 0, &arm);
	HolderView gone[1] = {HolderView{false, false, 0.0f}};
	n = walkers::StepGrab(g, s, 0.1f, gone, ev, 8);
	Check(n == 1 && ev[0].why == LetGo::Gone && g.count == 0, "a holder not found any more: gone");
	walkers::TryGrab(g, s, 0xFF000D05, Arm::Right, 0, &arm);
	walkers::TryGrab(g, s, 0xFF000D06, Arm::Left, 0, &arm);
	HolderView mixed[2] = {HolderView{true, true, 80.0f}, Near()};
	n = walkers::StepGrab(g, s, 0.1f, mixed, ev, 8);
	Check(n == 1 && g.count == 1 && g.holders[0].refId == 0xFF000D06,
	      "the first of two lets go: the other moves up, its view next step at index 0");
	Check(walkers::ReleaseHolder(g, 0xFF000D06) && g.count == 0 && !walkers::ReleaseHolder(g, 0xFF000D06),
	      "let go when asked (OBVR's shake); asked again: nothing");
	GrabSettings one = s;
	one.struggleHits = 1;
	walkers::TryGrab(g, one, 0xFF000D07, Arm::Right, 0, &arm);
	Check(walkers::OnStruck(g, one, 0xFF000D07), "StruggleHits 1: one blow frees");
	GrabSettings zero = s;
	zero.struggleHits = 0;
	walkers::TryGrab(g, zero, 0xFF000D08, Arm::Right, 0, &arm);
	Check(walkers::OnStruck(g, zero, 0xFF000D08), "StruggleHits 0 counts as 1: a blow frees");
}

void TestBitesForgotten() {
	std::printf("The bites forgotten\n");
	GrabSettings s;  // 30 s free and the bites are forgotten
	s.chancePercent = 100;
	GrabState g;
	Arm arm = Arm::None;
	GrabEvent ev[4];
	HolderView one[1] = {Near()};
	walkers::TryGrab(g, s, 0xFF000E01, Arm::Right, 0, &arm);
	walkers::StepGrab(g, s, 4.0f, one, ev, 4);
	walkers::StepGrab(g, s, 4.0f, one, ev, 4);
	walkers::ReleaseHolder(g, 0xFF000E01);
	Check(g.bites == 2, "two bites, then free");
	walkers::StepGrab(g, s, 29.0f, nullptr, ev, 4);
	Check(g.bites == 2, "29 s free: still two");
	walkers::TryGrab(g, s, 0xFF000E02, Arm::Right, 0, &arm);
	walkers::StepGrab(g, s, 0.5f, one, ev, 4);
	walkers::ReleaseHolder(g, 0xFF000E02);
	walkers::StepGrab(g, s, 29.0f, nullptr, ev, 4);
	Check(g.bites == 2, "grabbed again in between: the free time starts anew");
	walkers::StepGrab(g, s, 1.5f, nullptr, ev, 4);
	Check(g.bites == 0, "30 s free in one go: forgotten, the next grab starts from no bite");
	walkers::TryGrab(g, s, 0xFF000E03, Arm::Right, 0, &arm);
	const UInt32 n = walkers::StepGrab(g, s, 4.0f, one, ev, 4);
	Check(n == 1 && ev[0].biteNumber == 1, "a new holder's first bite comes after its full 4 s and counts 1");
	walkers::ResetGrab(g);
	Check(g.count == 0 && g.bites == 0, "a load or a new game: nobody holds, no bite counts");
}

void TestMessage() {
	std::printf("The message to OBVR\n");
	GrabSettings s;
	s.chancePercent = 100;
	GrabState g;
	Arm arm = Arm::None;
	UInt32 words[walkers::kGrabMessageWords];
	Check(walkers::GrabMessage(g, words) == 4 && words[0] == 0, "nobody holds: the count 0 alone");
	walkers::TryGrab(g, s, 0xFF000F01, Arm::Right, 0, &arm);
	walkers::TryGrab(g, s, 0xFF000F02, Arm::Left, 0, &arm);
	const UInt32 bytes = walkers::GrabMessage(g, words);
	Check(bytes == 20 && words[0] == 2 && words[1] == 0xFF000F01 && words[2] == 2 && words[3] == 0xFF000F02 &&
	          words[4] == 1,
	      "two holders: the count, then each reference with its arm (1 left, 2 right)");
	Check(walkers::kGrabStateMessage == 0x42524757 && walkers::kGrabFreeMessage == 0x52464757,
	      "the types read 'WGRB' and 'WGFR' in memory");
	UInt32 bite[walkers::kGrabBiteWords];
	const GrabEvent second{GrabEvent::Bite, 0xFF000F01, Arm::Right, LetGo::Dead, false, 2};
	Check(walkers::GrabBiteMessage(second, s, bite) == 16 && bite[0] == 2 && bite[1] == 2 && bite[2] == 3 &&
	          bite[3] == 0,
	      "a bite to OBVR: the arm, which bite of how many, not the last");
	const GrabEvent last{GrabEvent::Bite, 0xFF000F02, Arm::Left, LetGo::Dead, true, 3};
	Check(walkers::GrabBiteMessage(last, s, bite) == 16 && bite[0] == 1 && bite[1] == 3 && bite[3] == 1,
	      "the last bite: the lethal word 1");
	Check(walkers::kGrabBiteMessage == 0x54424757, "the type reads 'WGBT' in memory");
	Check(walkers::ListenToObvrAt(0), "xOBSE's PostLoad (0): every plugin is loaded, OBVR's listener now");
	Check(!walkers::ListenToObvrAt(9) && !walkers::ListenToObvrAt(1),
	      "any other message of xOBSE (PostPostLoad 9, ExitGame 1): not again");
	const UInt32 request = 0xFF000F02;
	UInt32 ref = 0;
	Check(walkers::FreeRequestOf(&request, 4, ref) && ref == 0xFF000F02, "OBVR's free request: the holder to free");
	Check(!walkers::FreeRequestOf(&request, 3, ref) && !walkers::FreeRequestOf(nullptr, 4, ref),
	      "a request too short or without data: none");
}

void TestTestGrab() {
	std::printf("The test switch\n");
	GrabSettings s;
	float clock = 0.0f;
	Check(!walkers::TestGrabDue(s, clock, 10.0f, false), "TestGrabSeconds 0 (the default): never");
	s.testGrabSeconds = 5.0f;
	Check(!walkers::TestGrabDue(s, clock, 4.0f, false) && walkers::TestGrabDue(s, clock, 1.0f, false) && clock == 0.0f,
	      "TestGrabSeconds 5: due after 5 s free, the clock from 0 again");
	Check(!walkers::TestGrabDue(s, clock, 6.0f, true) && clock == 0.0f, "while a walker holds: never, the clock stands at 0");
	Check(walkers::TestGrabDue(s, clock, 5.0f, false), "free again for 5 s: due again");
}

void TestStruggle() {
	std::printf("Wrenching free by the key (flat)\n");
	GrabSettings s;  // 6 presses needed here, none more than 0.8 s apart
	walkers::Struggle st;
	bool pressed = false;
	Check(!walkers::StepStruggle(st, s, 6, true, 0.1f, false, &pressed) && !pressed && st.bar == 0.0f,
	      "nobody holds: the key does nothing");
	Check(!walkers::StepStruggle(st, s, 6, true, 0.1f, true, &pressed) && !pressed,
	      "held while the key was already down: no press (a press is the key going down)");
	walkers::StepStruggle(st, s, 6, false, 0.05f, true, &pressed);
	bool freed = false;
	UInt32 presses = 0;
	for (UInt32 i = 0; i < 20 && !freed; ++i) {
		freed = walkers::StepStruggle(st, s, 6, true, 0.05f, true, &pressed);
		presses += pressed ? 1u : 0u;
		if (!freed) {
			walkers::StepStruggle(st, s, 6, false, 0.05f, true, &pressed);
		}
	}
	Check(freed && presses == 6 && st.bar == 0.0f, "six quick presses (ten a second): free, the bar empty again");
	walkers::Struggle slow;
	walkers::StepStruggle(slow, s, 6, false, 0.0f, true, &pressed);
	freed = false;
	for (UInt32 i = 0; i < 20 && !freed; ++i) {
		freed = walkers::StepStruggle(slow, s, 6, true, 0.05f, true, &pressed);
		walkers::StepStruggle(slow, s, 6, false, 0.95f, true, &pressed);
	}
	Check(!freed && slow.bar <= 1.0f, "one press a second: each gap empties the bar - mash it");
	walkers::Struggle off;
	GrabSettings none = s;
	none.strugglePresses = 0;
	walkers::StepStruggle(off, none, 6, false, 0.0f, true, &pressed);
	Check(!walkers::StepStruggle(off, none, 6, true, 0.05f, true, &pressed) && !pressed, "StrugglePresses 0: off");
	walkers::Struggle drop;
	walkers::StepStruggle(drop, s, 6, false, 0.0f, true, &pressed);
	walkers::StepStruggle(drop, s, 6, true, 0.0f, true, &pressed);
	walkers::StepStruggle(drop, s, 6, false, 0.0f, false, &pressed);
	Check(drop.bar == 0.0f, "let go meanwhile: the bar starts empty at the next grab");
}

void TestNeeded() {
	std::printf("How many presses (Nadi, 2026-10-28: the more walkers and the less stamina, the harder)\n");
	GrabSettings s;  // 4 at the base, half again per walker more, at most x3; up to x2 tired
	Check(walkers::StrugglePressesNeeded(s, 1, 100.0f, 100.0f) == 4, "one walker, fresh: 4");
	Check(walkers::StrugglePressesNeeded(s, 2, 100.0f, 100.0f) == 6, "two walkers: 6");
	Check(walkers::StrugglePressesNeeded(s, 3, 100.0f, 100.0f) == 8, "three: 8");
	Check(walkers::StrugglePressesNeeded(s, 5, 100.0f, 100.0f) == 12 &&
	          walkers::StrugglePressesNeeded(s, 20, 100.0f, 100.0f) == 12,
	      "five: 12, and no more for a crowd (x3 at most)");
	Check(walkers::StrugglePressesNeeded(s, 1, 50.0f, 100.0f) == 6, "half the stamina: x1.5");
	Check(walkers::StrugglePressesNeeded(s, 1, 0.0f, 100.0f) == 8 &&
	          walkers::StrugglePressesNeeded(s, 1, -20.0f, 100.0f) == 8,
	      "none left (or below): x2");
	Check(walkers::StrugglePressesNeeded(s, 3, 25.0f, 100.0f) == 14, "three walkers, a quarter left: 4 x 2 x 1.75 = 14");
	Check(walkers::StrugglePressesNeeded(s, 0, 100.0f, 100.0f) == 4, "none counted (the holder itself not seen): as one");
	Check(walkers::StrugglePressesNeeded(s, 1, 100.0f, 0.0f) == 4 && walkers::StrugglePressesNeeded(s, 1, 150.0f, 100.0f) == 4,
	      "stamina not read, or above its base: fresh");
	GrabSettings off = s;
	off.strugglePresses = 0;
	Check(walkers::StrugglePressesNeeded(off, 5, 0.0f, 100.0f) == 0, "StrugglePresses 0: off whatever the crowd");
}

void TestShake() {
	std::printf("Shaking a holder off in VR (OBVR's strokes)\n");
	GrabSettings s;
	Check(walkers::ShakeStrokesNeeded(s, 1, 100.0f, 100.0f) == 6, "one walker, fresh: 6 strokes");
	Check(walkers::ShakeStrokesNeeded(s, 3, 25.0f, 100.0f) == 21,
	      "the press count's crowd and tiredness: three walkers, a quarter left: 6 x 2 x 1.75 = 21");
	GrabSettings off = s;
	off.shakeStrokes = 0;
	Check(walkers::ShakeStrokesNeeded(off, 1, 100.0f, 100.0f) == 0, "ShakeStrokes 0: off");
	GrabSettings flatOff = s;
	flatOff.strugglePresses = 0;
	Check(walkers::ShakeStrokesNeeded(flatOff, 1, 100.0f, 100.0f) == 6, "the Block key off: the shake still counts");

	GrabState g;
	Arm arm = Arm::None;
	s.chancePercent = 100;
	Check(walkers::HolderOn(g, Arm::Left) == 0, "nobody holds: no holder on the left");
	walkers::TryGrab(g, s, 0xFF001001, Arm::Right, 0, &arm);
	walkers::TryGrab(g, s, 0xFF001002, Arm::Left, 0, &arm);
	Check(walkers::HolderOn(g, Arm::Right) == 0xFF001001 && walkers::HolderOn(g, Arm::Left) == 0xFF001002 &&
	          walkers::HolderOn(g, Arm::None) == 0,
	      "the holder on each arm; no arm: none");

	walkers::Struggle st;
	Check(!walkers::StepShake(st, s, 6, 3, 0.1f, false) && st.bar == 0, "the arm not held: strokes count nothing");
	Check(!walkers::StepShake(st, s, 6, 2, 0.1f, true) && st.bar == 2, "two strokes in one frame: both count");
	Check(!walkers::StepShake(st, s, 6, 0, 0.3f, true) && st.bar == 2, "a frame without a stroke: the count stays");
	Check(!walkers::StepShake(st, s, 6, 3, 0.2f, true) && st.bar == 5, "five of six");
	Check(walkers::StepShake(st, s, 6, 1, 0.2f, true) && st.bar == 0, "the sixth: shaken off, the count from 0");
	walkers::Struggle slow;
	walkers::StepShake(slow, s, 6, 4, 0.1f, true);
	walkers::StepShake(slow, s, 6, 0, 0.9f, true);
	Check(!walkers::StepShake(slow, s, 6, 1, 0.05f, true) && slow.bar == 1,
	      "a pause longer than StruggleGapSeconds: the count starts again");
	walkers::Struggle many;
	Check(walkers::StepShake(many, s, 6, 9, 0.1f, true) && many.bar == 0, "more strokes than needed at once: free once");
	walkers::Struggle none;
	Check(!walkers::StepShake(none, s, 0, 5, 0.1f, true) && none.bar == 0, "nothing needed (off): never free");

	const UInt32 left = 1;
	const UInt32 right = 2;
	const UInt32 bad = 3;
	Arm got = Arm::None;
	Check(walkers::ShakeStrokeOf(&left, 4, got) && got == Arm::Left && walkers::ShakeStrokeOf(&right, 4, got) &&
	          got == Arm::Right,
	      "OBVR's stroke: the arm (1 left, 2 right)");
	Check(!walkers::ShakeStrokeOf(&bad, 4, got) && !walkers::ShakeStrokeOf(&left, 3, got) &&
	          !walkers::ShakeStrokeOf(nullptr, 4, got),
	      "no such arm, too short or no data: no stroke");
	const UInt32 on = 1;
	const UInt32 offWord = 0;
	bool vr = false;
	Check(walkers::VrHelloOf(&on, 4, vr) && vr && walkers::VrHelloOf(&offWord, 4, vr) && !vr &&
	          !walkers::VrHelloOf(&on, 2, vr),
	      "OBVR's hello: VR on or off; too short: none");
	Check(walkers::kGrabShakeMessage == 0x4B534757 && walkers::kGrabVrMessage == 0x52564757,
	      "the types read 'WGSK' and 'WGVR' in memory");
}

void TestTexts() {
	std::printf("The messages (flat)\n");
	GrabSettings s;
	char text[128];
	Check(walkers::GrabText(walkers::GrabSay::Grabbed, Arm::Left, 0, 6, s, text, sizeof(text)) &&
	          std::strcmp(text, "A walker grabs your left arm! Strike it or mash Block (6).") == 0,
	      "the first holder: which arm, and how to get free");
	Check(walkers::GrabText(walkers::GrabSay::Rooted, Arm::Right, 0, 6, s, text, sizeof(text)) &&
	          std::strcmp(text, "Two walkers hold you - you cannot move!") == 0,
	      "the second: rooted");
	Check(walkers::GrabText(walkers::GrabSay::Bitten, Arm::Left, 2, 6, s, text, sizeof(text)) &&
	          std::strcmp(text, "Bitten! (2 of 3)") == 0,
	      "a bite: which of how many");
	Check(walkers::GrabText(walkers::GrabSay::LastBite, Arm::Left, 3, 6, s, text, sizeof(text)) &&
	          std::strcmp(text, "The walkers overwhelm you.") == 0,
	      "the last");
	Check(walkers::GrabText(walkers::GrabSay::Free, Arm::Left, 0, 6, s, text, sizeof(text)) &&
	          std::strcmp(text, "You break free.") == 0,
	      "free");
	GrabSettings quiet = s;
	quiet.messages = false;
	Check(!walkers::GrabText(walkers::GrabSay::Bitten, Arm::Left, 1, 6, quiet, text, sizeof(text)),
	      "Messages 0: nothing said");
	GrabSettings mute = s;
	mute.strugglePresses = 0;
	Check(walkers::GrabText(walkers::GrabSay::Grabbed, Arm::Right, 0, 6, mute, text, sizeof(text)) &&
	          std::strcmp(text, "A walker grabs your right arm! Strike it to break free.") == 0,
	      "without the key: only the strike named");
	Check(!walkers::GrabText(walkers::GrabSay::Grabbed, Arm::Left, 0, 8, s, text, sizeof(text), true) &&
	          !walkers::GrabText(walkers::GrabSay::Bitten, Arm::Left, 1, 8, s, text, sizeof(text), true) &&
	          !walkers::GrabText(walkers::GrabSay::Free, Arm::Left, 0, 8, s, text, sizeof(text), true),
	      "in VR: no line at all - OBVR shows it on the body (Nadi, 2026-10-30: the line does not do it)");
	Check(walkers::SayFor(LetGo::Struck) == walkers::GrabSay::Free && walkers::SayFor(LetGo::Struggled) == walkers::GrabSay::Free &&
	          walkers::SayFor(LetGo::TornFree) == walkers::GrabSay::Free && walkers::SayFor(LetGo::Asked) == walkers::GrabSay::Free &&
	          walkers::SayFor(LetGo::Shaken) == walkers::GrabSay::Free &&
	          walkers::SayFor(LetGo::Dead) == walkers::GrabSay::None &&
	          walkers::SayFor(LetGo::PlayerDown) == walkers::GrabSay::None,
	      "free said when the player broke free; a holder cut down or the player down: nothing more");
}

void TestRefusalNames() {
	std::printf("The log's words\n");
	Check(std::strcmp(walkers::LetGoName(LetGo::TornFree), "torn free") == 0 &&
	          std::strcmp(walkers::LetGoName(LetGo::Struck), "struck off") == 0 &&
	          std::strcmp(walkers::LetGoName(LetGo::PlayerDown), "the player is down") == 0 &&
	          std::strcmp(walkers::LetGoName(LetGo::Struggled), "wrenched off") == 0 &&
	          std::strcmp(walkers::LetGoName(LetGo::SwitchedOff), "the grab switched off") == 0 &&
	          std::strcmp(walkers::LetGoName(LetGo::Shaken), "shaken off") == 0 &&
	          std::strcmp(walkers::LetGoName(LetGo::Asked), "freed by OBVR") == 0 &&
	          std::strcmp(walkers::ArmName(Arm::Left), "left") == 0 &&
	          std::strcmp(walkers::RefusalName(GrabRefusal::BothArmsHeld), "both arms held") == 0,
	      "each reason has its words");
}

}  // namespace

int main() {
	TestSide();
	TestGrab();
	TestHold();
	TestBites();
	TestBiteDamage();
	TestLetGo();
	TestBitesForgotten();
	TestMessage();
	TestTestGrab();
	TestStruggle();
	TestNeeded();
	TestShake();
	TestTexts();
	TestRefusalNames();
	std::printf("%s (%d failed)\n", g_failures == 0 ? "PASS" : "FAIL", g_failures);
	return g_failures == 0 ? 0 : 1;
}
