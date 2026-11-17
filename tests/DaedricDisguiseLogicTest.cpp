// Every flow of the Daedric disguise (docs/walkers-spec.md 4.22 idea 7;
// Nadi, 2026-11-16: "go 7", known again after "60 s ohne Daedra"): the full
// set fools them, a blow or coming too near ends it, they forget you after
// ForgetSeconds with none of them near, the set taken off ends it.

#include "DaedricArmor.h"
#include "DaedricDisguiseLogic.h"

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

using walkers::DaedricChange;
using walkers::DaedricSettings;
using walkers::DaedricState;

constexpr float kNone = -1.0f;
constexpr float kFar = 9000.0f;

void TestSet() {
	std::printf("The full set\n");
	Check(walkers::FullDaedricSet(0x3D), "helmet, cuirass, greaves, gauntlets, boots");
	Check(walkers::FullDaedricSet(0x3F), "the helmet's hair slot beside it: still full");
	Check(!walkers::FullDaedricSet(0x39), "no cuirass: not full");
	Check(!walkers::FullDaedricSet(0x1D), "no boots: not full");
	Check(!walkers::FullDaedricSet(0x00), "nothing: not full");
	UInt8 all = 0;
	bool shield = false;
	for (UInt32 i = 0; i < walkers::kDaedricPieceCount; ++i) {
		all |= walkers::kDaedricPieces[i].slots;
		shield = shield || walkers::kDaedricPieces[i].slots == 0;
	}
	Check(walkers::FullDaedricSet(all), "the pieces read off Oblivion.esm make a full set");
	Check(!shield, "no piece without a slot (the shields left out)");
	Check(walkers::DaedricSlotsOf(0x0003635B) == 0x04 && walkers::DaedricSlotsOf(0x00036357) == 0x01,
	      "the Daedric cuirass the upper body, the helmet the head");
	Check(walkers::DaedricSlotsOf(0x0003635C) == 0 && walkers::DaedricSlotsOf(0x00012345) == 0,
	      "the Daedric shield and any other armor: none");
}

void TestFooled() {
	std::printf("Fooled\n");
	DaedricSettings s;
	DaedricState st;
	Check(walkers::StepDaedric(st, s, false, kFar, false, 1.0f) == DaedricChange::None && !st.on,
	      "no full set: nothing");
	Check(walkers::StepDaedric(st, s, true, kFar, false, 1.0f) == DaedricChange::Fooled && st.on,
	      "the full set on: they take the player for one of theirs");
	Check(walkers::StepDaedric(st, s, true, kNone, false, 1.0f) == DaedricChange::None && st.on, "it holds");
	DaedricState close;
	Check(walkers::StepDaedric(close, s, true, 100.0f, false, 1.0f) == DaedricChange::None && !close.on,
	      "put on with a Daedra at arm's length: not fooled");
	Check(walkers::StepDaedric(close, s, true, 250.0f, false, 1.0f) == DaedricChange::Fooled,
	      "a step back: fooled");
}

void TestEnds() {
	std::printf("It ends\n");
	DaedricSettings s;
	DaedricState st;
	walkers::StepDaedric(st, s, true, kFar, false, 1.0f);
	Check(walkers::StepDaedric(st, s, true, kFar, true, 1.0f) == DaedricChange::Struck && !st.on && st.known,
	      "the player strikes a Daedra: known");
	DaedricState near;
	walkers::StepDaedric(near, s, true, kFar, false, 1.0f);
	Check(walkers::StepDaedric(near, s, true, s.nearUnits, false, 1.0f) == DaedricChange::TooNear && !near.on &&
	          near.known,
	      "within NearDistance: they see through it");
	DaedricState off;
	walkers::StepDaedric(off, s, true, kFar, false, 1.0f);
	Check(walkers::StepDaedric(off, s, false, kFar, false, 1.0f) == DaedricChange::SetOff && !off.on && !off.known,
	      "a piece taken off: over, not known");
	Check(walkers::StepDaedric(off, s, true, kFar, false, 1.0f) == DaedricChange::Fooled, "put on again: fooled");
	DaedricSettings disabled = s;
	disabled.enabled = false;
	Check(walkers::StepDaedric(off, disabled, true, kFar, false, 1.0f) == DaedricChange::Off && !off.on,
	      "switched off (LINK): over");
	Check(walkers::StepDaedric(off, disabled, true, kFar, false, 1.0f) == DaedricChange::None && !off.on,
	      "off: nothing");
}

void TestForget() {
	std::printf("Known, then forgotten\n");
	DaedricSettings s;
	DaedricState st;
	walkers::StepDaedric(st, s, true, kFar, false, 1.0f);
	walkers::StepDaedric(st, s, true, kFar, true, 1.0f);
	Check(walkers::StepDaedric(st, s, true, 3000.0f, false, 50.0f) == DaedricChange::None && st.known,
	      "a Daedra within ForgetDistance: still known, the clock held");
	Check(walkers::StepDaedric(st, s, true, kNone, false, 59.0f) == DaedricChange::None && st.known,
	      "59 s with none near: still known");
	Check(walkers::StepDaedric(st, s, true, kFar, false, 1.0f) == DaedricChange::Fooled && !st.known && st.on,
	      "60 s: forgotten - in the full set, fooled again");
	DaedricState again;
	walkers::StepDaedric(again, s, true, kFar, true, 1.0f);
	Check(again.known && !again.on, "struck without being fooled (the set put on mid-fight): known all the same");
	walkers::StepDaedric(again, s, true, kNone, false, 40.0f);
	walkers::StepDaedric(again, s, true, kNone, true, 1.0f);
	Check(walkers::StepDaedric(again, s, true, kNone, false, 40.0f) == DaedricChange::None && again.known,
	      "another blow while known: the clock from the start");
	DaedricState bare;
	walkers::StepDaedric(bare, s, true, kFar, false, 1.0f);
	walkers::StepDaedric(bare, s, true, kFar, true, 1.0f);
	Check(walkers::StepDaedric(bare, s, false, kNone, false, 60.0f) == DaedricChange::None && !bare.known,
	      "forgotten without the set: nothing to say");
}

void TestStruck() {
	std::printf("Which blow gives him away\n");
	Check(walkers::DaedricStruck(true, true, false), "the player strikes a Daedra: given away");
	Check(walkers::DaedricStruck(true, false, true), "the player strikes a walker in Mode 3 (the Daedra's ally): given away");
	Check(!walkers::DaedricStruck(true, false, false), "the player strikes anyone else (a walker outside Mode 3, a bandit): not");
	Check(!walkers::DaedricStruck(false, true, true), "someone else strikes them: not the player's doing");
}

void TestText() {
	std::printf("The words\n");
	Check(std::strlen(walkers::DaedricChangeText(DaedricChange::Fooled)) > 0, "fooled");
	Check(std::strlen(walkers::DaedricChangeText(DaedricChange::Struck)) > 0, "struck");
	Check(std::strlen(walkers::DaedricChangeText(DaedricChange::TooNear)) > 0, "too near");
	Check(std::strlen(walkers::DaedricChangeText(DaedricChange::SetOff)) > 0, "set off");
	Check(std::strlen(walkers::DaedricChangeText(DaedricChange::Off)) > 0, "switched off");
	Check(std::strlen(walkers::DaedricChangeText(DaedricChange::None)) == 0, "none: no words");
	Check(walkers::DaedricJoins(DaedricChange::Fooled) && !walkers::DaedricJoins(DaedricChange::None),
	      "only fooled joins the factions");
	Check(walkers::DaedricLeaves(DaedricChange::Struck) && walkers::DaedricLeaves(DaedricChange::TooNear) &&
	          walkers::DaedricLeaves(DaedricChange::SetOff) && walkers::DaedricLeaves(DaedricChange::Off) &&
	          !walkers::DaedricLeaves(DaedricChange::Fooled) && !walkers::DaedricLeaves(DaedricChange::None),
	      "the ends leave them");
}

}  // namespace

int main() {
	TestSet();
	TestFooled();
	TestEnds();
	TestForget();
	TestStruck();
	TestText();
	std::printf(g_failures == 0 ? "PASS\n" : "FAIL (%d)\n", g_failures);
	return g_failures == 0 ? 0 : 1;
}
