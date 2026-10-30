// Every flow of 4.17, only a lost head ends a walker (docs/walkers-spec.md
// 4.17; Nadi, 2026-10-25: "walker nur sterben wenn geköpft. default an";
// 2026-10-30: "jo" to the build, and without a cutter every kill ends one): a walker's fall - up again or the end -,
// OBVR's messages ('WGDM', 'WGHD'), what a cut head does, the heads noted.

#include "EndLogic.h"

#include <cstdio>

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf("  %s  %s\n", condition ? "ok  " : "FAIL", what);
	if (!condition) {
		++g_failures;
	}
}

using walkers::FallFacts;
using walkers::HeadAction;
using walkers::HeadKind;
using walkers::HeadNotes;
using walkers::WalkerFall;

void TestFall() {
	std::printf("A walker falls\n");
	FallFacts f;
	f.cutterOn = true;
	Check(walkers::WalkerFallVerdict(true, f) == WalkerFall::RisesAgain,
	      "OBVR cuts heads, a plain fall: it gets up again");
	Check(walkers::WalkerFallVerdict(false, f) == WalkerFall::EndedRuleOff,
	      "DieOnlyBeheaded 0: killed once, as before 4.17");
	FallFacts headless = f;
	headless.headless = true;
	Check(walkers::WalkerFallVerdict(true, headless) == WalkerFall::EndedHeadless,
	      "a walker without a head (made with the rule off): ends at once - it could never lose one");
	FallFacts cut = f;
	cut.headNoted = true;
	Check(walkers::WalkerFallVerdict(true, cut) == WalkerFall::EndedHead, "its head off before the death: the end");
	FallFacts flat;
	Check(walkers::WalkerFallVerdict(true, flat) == WalkerFall::EndedNoCutter,
	      "nobody cuts heads (OBVR's Dismemberment=0, flat, no OBVR): every kill ends it, no rise (Nadi, "
	      "2026-10-30: \"bei Dismemberment=0 killt jeder schlag und walker steht auch nicht auf\")");
	flat.headNoted = true;
	Check(walkers::WalkerFallVerdict(true, flat) == WalkerFall::EndedHead, "a head noted ends it, whoever cut");
	Check(walkers::Ends(WalkerFall::EndedRuleOff) && walkers::Ends(WalkerFall::EndedHeadless) &&
	          walkers::Ends(WalkerFall::EndedHead) && walkers::Ends(WalkerFall::EndedNoCutter) &&
	          !walkers::Ends(WalkerFall::RisesAgain),
	      "only RisesAgain is no end");
}

void TestClock() {
	std::printf("The clock to the next rise\n");
	walkers::EndSettings s;
	Check(walkers::RiseAgainIn(s, 0.0f) == 20.0f && walkers::RiseAgainIn(s, 1.0f) == 30.0f &&
	          walkers::RiseAgainIn(s, 0.5f) == 25.0f,
	      "RiseAgainSeconds 20 plus that share of RiseAgainJitterSeconds 10");
	Check(walkers::RiseAgainIn(s, -1.0f) == 20.0f && walkers::RiseAgainIn(s, 2.0f) == 30.0f,
	      "a share outside 0..1 held to it");
}

void TestMessages() {
	std::printf("OBVR's messages\n");
	Check(walkers::kDismemberMessage == 0x4D444757 && walkers::kHeadMessage == 0x44484757,
	      "the types read 'WGDM' and 'WGHD' in memory");
	bool on = false;
	const UInt32 one = 1;
	const UInt32 zero = 0;
	Check(walkers::DismemberOf(&one, 4, on) && on, "'WGDM' 1: OBVR cuts heads");
	Check(walkers::DismemberOf(&zero, 4, on) && !on, "'WGDM' 0: it does not");
	Check(!walkers::DismemberOf(&one, 3, on) && !walkers::DismemberOf(nullptr, 4, on), "too short or none: no answer");
	const UInt32 cut[2] = {0xFF000A01, 1};
	const UInt32 crushed[2] = {0xFF000A02, 2};
	const UInt32 odd[2] = {0xFF000A03, 3};
	UInt32 ref = 0;
	HeadKind kind = HeadKind::Cut;
	Check(walkers::HeadOf(cut, 8, ref, kind) && ref == 0xFF000A01 && kind == HeadKind::Cut, "'WGHD' kind 1: cut off");
	Check(walkers::HeadOf(crushed, 8, ref, kind) && ref == 0xFF000A02 && kind == HeadKind::Crushed,
	      "kind 2: crushed");
	Check(!walkers::HeadOf(odd, 8, ref, kind), "another kind: not taken");
	Check(!walkers::HeadOf(cut, 7, ref, kind) && !walkers::HeadOf(nullptr, 8, ref, kind),
	      "too short or none: not taken");
	const UInt32 nobody[2] = {0, 1};
	Check(!walkers::HeadOf(nobody, 8, ref, kind), "reference 0: not taken");
}

void TestHeadAction() {
	std::printf("What a head off does\n");
	Check(walkers::HeadActionFor(false, false, true) == HeadAction::Ignore,
	      "not a walker, not waiting: nothing more (noted, so its death keeps it down)");
	Check(walkers::HeadActionFor(false, true, true) == HeadAction::EndWaiting,
	      "a person lying to rise as a walker: off the list - no walker with a head from a headless corpse "
	      "(OBVR's dismember-walker.txt, 2026-10-30 19:14)");
	Check(walkers::HeadActionFor(true, true, true) == HeadAction::EndWaiting,
	      "a walker lying to rise again: off the list, the end");
	Check(walkers::HeadActionFor(true, false, false) == HeadAction::Kill,
	      "a walker still alive: killed, its death then finds the head noted");
	Check(walkers::HeadActionFor(true, false, true) == HeadAction::Note,
	      "dead and not waiting (the death watch not yet past, or ended): noted for its death");
}

void TestNotes() {
	std::printf("The heads noted\n");
	HeadNotes n;
	Check(!walkers::HeadNoted(n, 0xFF000B01), "none noted: none");
	walkers::NoteHead(n, 0xFF000B01);
	walkers::NoteHead(n, 0xFF000B01);
	Check(walkers::HeadNoted(n, 0xFF000B01) && n.count == 1, "noted once, however often it came");
	for (UInt32 i = 0; i < walkers::kHeadNotesMost; ++i) {
		walkers::NoteHead(n, 0xFF000C00 + i);
	}
	Check(n.count == walkers::kHeadNotesMost && !walkers::HeadNoted(n, 0xFF000B01) &&
	          walkers::HeadNoted(n, 0xFF000C00 + walkers::kHeadNotesMost - 1),
	      "full: the oldest gives way to the newest");
	walkers::NoteHead(n, 0);
	Check(!walkers::HeadNoted(n, 0), "reference 0 is never noted");
}

void TestSpawns() {
	std::printf("Headless walkers among the spawns\n");
	Check(walkers::SpawnsHeadless(false) && !walkers::SpawnsHeadless(true),
	      "with DieOnlyBeheaded no walker without a head is made (Nadi, 2026-10-27)");
}

}  // namespace

int main() {
	TestFall();
	TestClock();
	TestMessages();
	TestHeadAction();
	TestNotes();
	TestSpawns();
	std::printf(g_failures == 0 ? "PASS\n" : "FAIL (%d)\n", g_failures);
	return g_failures == 0 ? 0 : 1;
}
