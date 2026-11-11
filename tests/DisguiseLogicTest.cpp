// Every flow of walker blood as a disguise (docs/walkers-spec.md 4.20.1;
// Nadi, 2026-11-09: "go 4.20.1" - real minutes, a confirm dialog, the
// walkers attacking stop, any rain ends it): when the dialog is offered, the
// begin, the clock, what ends it, what the walkers may do meanwhile, the
// sky's rain, the movement flags and the messages to and from OBVR.

#include "DisguiseLogic.h"

#include <cstdio>

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf("  %s  %s\n", condition ? "ok  " : "FAIL", what);
	if (!condition) {
		++g_failures;
	}
}

using walkers::DisguiseEnd;
using walkers::DisguiseSettings;
using walkers::DisguiseState;

void TestOffer() {
	std::printf("The dialog is offered\n");
	DisguiseSettings s;
	DisguiseState st;
	Check(walkers::DisguiseOffered(s, st, true, true, true, true), "the player, sneaking, activates a dead walker: asked");
	Check(!walkers::DisguiseOffered(s, st, false, true, true, true), "someone else activates it: no");
	Check(!walkers::DisguiseOffered(s, st, true, false, true, true), "not a walker: no");
	Check(!walkers::DisguiseOffered(s, st, true, true, false, true), "a walker alive: no");
	Check(!walkers::DisguiseOffered(s, st, true, true, true, false), "not sneaking (looting as ever): no");
	DisguiseSettings off = s;
	off.enabled = false;
	Check(!walkers::DisguiseOffered(off, st, true, true, true, true), "[Disguise] Enabled=0: no");
	st.asking = true;
	Check(!walkers::DisguiseOffered(s, st, true, true, true, true), "a dialog open already: not a second");
	st.asking = false;
	st.on = true;
	Check(walkers::DisguiseOffered(s, st, true, true, true, true), "disguised already: asked again (to renew it)");
}

void TestBeginAndClock() {
	std::printf("The begin and the clock\n");
	DisguiseSettings s;
	DisguiseState st;
	st.asking = true;
	walkers::BeginDisguise(st, s);
	Check(st.on && !st.asking && st.left == 180.0f, "yes: on, 180 real seconds (3 minutes)");
	Check(walkers::StepDisguise(st, s, 100.0f, false, false) == DisguiseEnd::None && st.left == 80.0f,
	      "100 s on: 80 left");
	walkers::BeginDisguise(st, s);
	Check(st.left == 180.0f, "smeared again: the full time anew");
	Check(walkers::StepDisguise(st, s, 179.0f, false, false) == DisguiseEnd::None, "179 s: still on");
	Check(walkers::StepDisguise(st, s, 1.5f, false, false) == DisguiseEnd::Time && !st.on, "past 180 s: ended, time");
	Check(walkers::StepDisguise(st, s, 1.0f, true, true) == DisguiseEnd::None, "off: nothing ends");
	DisguiseSettings minute = s;
	minute.seconds = 60.0f;
	walkers::BeginDisguise(st, minute);
	Check(st.left == 60.0f, "Seconds=60: a minute");
}

void TestEnds() {
	std::printf("What ends it\n");
	DisguiseSettings s;
	DisguiseState st;
	walkers::BeginDisguise(st, s);
	DisguiseState swim = st;
	Check(walkers::StepDisguise(swim, s, 0.1f, true, false) == DisguiseEnd::Water && !swim.on, "swimming: washed off");
	DisguiseState rain = st;
	Check(walkers::StepDisguise(rain, s, 0.1f, false, true) == DisguiseEnd::Rain && !rain.on, "rain: washed off");
	DisguiseState hit = st;
	Check(walkers::DisguiseHit(hit, true, true) == DisguiseEnd::HitWalker && !hit.on,
	      "the player strikes a walker: it knows him");
	DisguiseState other = st;
	Check(walkers::DisguiseHit(other, false, true) == DisguiseEnd::None && other.on, "someone else strikes it: on");
	Check(walkers::DisguiseHit(other, true, false) == DisguiseEnd::None && other.on, "the player strikes another: on");
	DisguiseSettings off = s;
	off.enabled = false;
	DisguiseState turned = st;
	Check(walkers::StepDisguise(turned, off, 0.1f, false, false) == DisguiseEnd::Off && !turned.on,
	      "switched off in LINK: ended");
	Check(walkers::DisguiseEndText(DisguiseEnd::Rain)[0] != '\0' && walkers::DisguiseEndText(DisguiseEnd::Time)[0] != '\0',
	      "each end has its line");
}

void TestWalkers() {
	std::printf("What the walkers may do\n");
	Check(!walkers::WalkerMayTurnOn(true, true) && walkers::WalkerMayTurnOn(true, false) &&
	          walkers::WalkerMayTurnOn(false, true),
	      "disguised: no walker turns on the player; on others as ever; undisguised as ever");
	Check(walkers::DisguisedAggression(false, true, 100) == 0 && walkers::DisguisedAggression(false, false, 100) == 100 &&
	          walkers::DisguisedAggression(true, false, 100) == 0,
	      "slow aggression off: the records' aggression 0 while disguised, their own after; on: 0 always");
}

void TestSky() {
	std::printf("Rain and movement\n");
	Check(walkers::Raining(3, true) && !walkers::Raining(3, false) && !walkers::Raining(1, true) &&
	          !walkers::Raining(4, true) && !walkers::Raining(2, true),
	      "the weather's precipitation 3 (rainy, xOBSE TESWeather) outdoors: rain; snow, clouds, fair, indoors: no");
	Check(walkers::Sneaking(0x400) && !walkers::Sneaking(0x400 | 0x800) && !walkers::Sneaking(0),
	      "movement flag 0x400 sneaking, unless swimming (0x800) overrides it");
	Check(walkers::Swimming(0x800) && !walkers::Swimming(0x400), "0x800 swimming");
}

void TestMessages() {
	std::printf("OBVR's messages\n");
	UInt32 word = 0;
	Check(walkers::DisguiseStateMessage(true, &word) == 4 && word == 1 && walkers::kDisguiseStateMessage == 0x53444757,
	      "to OBVR, 'WGDS': 1 disguised");
	walkers::DisguiseStateMessage(false, &word);
	Check(word == 0, "0 no more");
	const UInt32 corpse = 0xFF001234;
	UInt32 got = 0;
	Check(walkers::SmearOf(&corpse, 4, got) && got == corpse && walkers::kSmearMessage == 0x4C424757,
	      "from OBVR, 'WGBL': the corpse its hand smeared from (the gesture - no dialog)");
	Check(!walkers::SmearOf(&corpse, 3, got) && !walkers::SmearOf(nullptr, 4, got), "too short or none: refused");
}

}  // namespace

// OBVR's smear gesture ('WGBL') names the corpse; only a dead walker's
// blood disguises (Nadi, 2026-11-11 on Windows: a hand-to-hand fight, the
// beaten NPC 00023502 fell by the hands - the disguise came on).
void TestSmear() {
	std::printf("OBVR's smear\n");
	walkers::DisguiseSettings s;
	s.enabled = true;
	Check(walkers::SmearAccepted(s, true, true), "a dead walker's corpse: the disguise on");
	Check(!walkers::SmearAccepted(s, false, true), "a dead person, no walker: nothing");
	Check(!walkers::SmearAccepted(s, false, false), "a person knocked down, alive (the hand-to-hand fight): nothing");
	Check(!walkers::SmearAccepted(s, true, false), "a walker alive (down, getting up): nothing");
	s.enabled = false;
	Check(!walkers::SmearAccepted(s, true, true), "the disguise off: nothing");
}

int main() {
	TestSmear();
	TestOffer();
	TestBeginAndClock();
	TestEnds();
	TestWalkers();
	TestSky();
	TestMessages();
	std::printf(g_failures == 0 ? "PASS\n" : "FAIL (%d)\n", g_failures);
	return g_failures == 0 ? 0 : 1;
}
