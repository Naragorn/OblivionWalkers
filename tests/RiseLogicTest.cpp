// Every flow of the rise's decisions: the verdict on a death, the waiting
// clocks over frames, and the co-save image both ways.

#include "RiseLogic.h"

#include <cstdio>
#include <cstring>
#include <vector>

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf("  %s  %s\n", condition ? "ok  " : "FAIL", what);
	if (!condition) {
		++g_failures;
	}
}

walkers::Fallen Bandit(UInt32 refId = 0xFF000BAF) {
	walkers::Fallen fallen;
	fallen.refId = refId;
	fallen.baseId = 0x00069AD1;
	fallen.character = true;
	fallen.generic = true;
	fallen.racePlayable = true;
	fallen.raceHuman = true;
	return fallen;
}

bool Near(float a, float b) { return a - b < 0.001f && b - a < 0.001f; }

void TestVerdicts() {
	std::printf("The verdict on a death\n");
	using walkers::DeathVerdict;
	walkers::RiseSettings settings;
	walkers::WaitingList list;

	Check(walkers::NoteDeath(list, settings, Bandit(), 0, 0.0f) == DeathVerdict::Waits && list.count == 1,
	      "a generic person: waits");
	Check(Near(list.entries[0].secondsLeft, 20.0f) && list.entries[0].refId == 0xFF000BAF &&
	          list.entries[0].baseId == 0x00069AD1,
	      "with AfterSeconds and no jitter share, reference and base kept");
	Check(walkers::NoteDeath(list, settings, Bandit(), 0, 0.0f) == DeathVerdict::AlreadyWaiting && list.count == 1,
	      "the same reference again: waits already, not twice");
	Check(walkers::NoteDeath(list, settings, Bandit(0xFF000BB0), 0, 1.0f) == DeathVerdict::Waits &&
	          Near(list.entries[1].secondsLeft, 30.0f),
	      "the full jitter share: AfterSeconds plus JitterSeconds");
	Check(walkers::NoteDeath(list, settings, Bandit(0xFF000BB1), 0, 0.5f) == DeathVerdict::Waits &&
	          Near(list.entries[2].secondsLeft, 25.0f),
	      "half the share: halfway");
	Check(walkers::NoteDeath(list, settings, Bandit(0xFF000BB2), 0, 7.0f) == DeathVerdict::Waits &&
	          Near(list.entries[3].secondsLeft, 30.0f),
	      "a share above one is clamped to one");
	Check(walkers::NoteDeath(list, settings, Bandit(0xFF000BB3), 0, -1.0f) == DeathVerdict::Waits &&
	          Near(list.entries[4].secondsLeft, 20.0f),
	      "a share below zero is clamped to zero");

	walkers::RiseSettings off;
	off.enabled = false;
	walkers::WaitingList empty;
	Check(walkers::NoteDeath(empty, off, Bandit(), 0, 0.0f) == DeathVerdict::Disabled && empty.count == 0,
	      "Enabled=0: stays down, nothing listed");

	walkers::Fallen creature = Bandit();
	creature.character = false;
	Check(walkers::NoteDeath(empty, settings, creature, 0, 0.0f) == DeathVerdict::NotAPerson, "a creature: not a person");
	Check(walkers::NoteDeath(empty, settings, walkers::Fallen{}, 0, 0.0f) == DeathVerdict::NotAPerson,
	      "nothing read off the engine: not a person");

	walkers::Fallen essential = Bandit();
	essential.essential = true;
	Check(walkers::NoteDeath(empty, settings, essential, 0, 0.0f) == DeathVerdict::Essential,
	      "an essential person: only unconscious");

	walkers::Fallen named = Bandit();
	named.generic = false;
	Check(walkers::NoteDeath(empty, settings, named, 0, 0.0f) == DeathVerdict::NotGeneric,
	      "a named person with OnlyGeneric=1: stays down");
	walkers::RiseSettings everyone;
	everyone.onlyGeneric = false;
	Check(walkers::NoteDeath(empty, everyone, named, 0, 0.0f) == DeathVerdict::Waits && empty.count == 1,
	      "with OnlyGeneric=0 he waits");
	empty.count = 0;

	walkers::Fallen dremora = Bandit();
	dremora.racePlayable = false;
	Check(walkers::NoteDeath(empty, settings, dremora, 0, 0.0f) == DeathVerdict::NotPlayableRace,
	      "a person of no playable race (a Dremora) with OnlyPlayableRaces=1: stays down");
	walkers::RiseSettings anyRace;
	anyRace.onlyPlayableRaces = false;
	Check(walkers::NoteDeath(empty, anyRace, dremora, 0, 0.0f) == DeathVerdict::Waits && empty.count == 1,
	      "with OnlyPlayableRaces=0 he waits");
	empty.count = 0;

	Check(walkers::IsHumanRace(0x00000907) && walkers::IsHumanRace(0x000224FC) && walkers::IsHumanRace(0x000224FD) &&
	          walkers::IsHumanRace(0x00000D43),
	      "Imperial, Breton, Nord and Redguard are human");
	Check(!walkers::IsHumanRace(0x000223C7) && !walkers::IsHumanRace(0x00038010) && !walkers::IsHumanRace(0),
	      "a Khajiit, a Dremora and no race are not");
	walkers::Fallen khajiit = Bandit();
	khajiit.raceHuman = false;
	Check(walkers::NoteDeath(empty, settings, khajiit, 0, 0.0f) == DeathVerdict::Waits && empty.count == 1,
	      "a Khajiit with the defaults: waits (playable)");
	empty.count = 0;
	walkers::RiseSettings humans;
	humans.onlyHumans = true;
	Check(walkers::NoteDeath(empty, humans, khajiit, 0, 0.0f) == DeathVerdict::NotHuman && empty.count == 0,
	      "with OnlyHumans=1: stays down");
	Check(walkers::NoteDeath(empty, humans, Bandit(), 0, 0.0f) == DeathVerdict::Waits && empty.count == 1,
	      "and the Redguard waits");
	empty.count = 0;

	walkers::RiseSettings half;
	half.chancePercent = 50;
	Check(walkers::NoteDeath(empty, half, Bandit(1), 49, 0.0f) == DeathVerdict::Waits, "ChancePercent=50, roll 49: waits");
	Check(walkers::NoteDeath(empty, half, Bandit(2), 50, 0.0f) == DeathVerdict::ChanceMissed,
	      "roll 50: the chance missed");
	walkers::RiseSettings never;
	never.chancePercent = 0;
	Check(walkers::NoteDeath(empty, never, Bandit(3), 0, 0.0f) == DeathVerdict::ChanceMissed,
	      "ChancePercent=0: always missed");
	Check(walkers::NoteDeath(empty, settings, Bandit(4), 99, 0.0f) == DeathVerdict::Waits,
	      "ChancePercent=100, roll 99: never missed");

	walkers::WaitingList full;
	for (UInt32 i = 0; i < walkers::kWaitingMost; ++i) {
		walkers::NoteDeath(full, settings, Bandit(0x1000 + i), 0, 0.0f);
	}
	Check(full.count == walkers::kWaitingMost &&
	          walkers::NoteDeath(full, settings, Bandit(0x9999), 0, 0.0f) == DeathVerdict::ListFull,
	      "the list full: stays down");

	Check(std::strcmp(walkers::VerdictText(DeathVerdict::Waits), "waits to rise") == 0 &&
	          std::strcmp(walkers::VerdictText(DeathVerdict::ListFull), "stays down (the waiting list is full)") == 0,
	      "every verdict has its words");
	for (int v = 0; v <= static_cast<int>(DeathVerdict::ListFull); ++v) {
		if (std::strcmp(walkers::VerdictText(static_cast<DeathVerdict>(v)), "?") == 0) {
			Check(false, "a verdict without words");
		}
	}
}

struct Presences {
	std::vector<std::pair<UInt32, walkers::Presence>> map;
	walkers::Presence operator()(UInt32 refId) const {
		for (const auto& entry : map) {
			if (entry.first == refId) {
				return entry.second;
			}
		}
		return walkers::Presence::Gone;
	}
};

void TestTick() {
	std::printf("The waiting clocks over frames\n");
	using walkers::Presence;
	walkers::RiseSettings settings;
	settings.afterSeconds = 1.0f;
	settings.jitterSeconds = 0.0f;
	walkers::WaitingList list;
	walkers::NoteDeath(list, settings, Bandit(0xA), 0, 0.0f);
	walkers::NoteDeath(list, settings, Bandit(0xB), 0, 0.0f);
	walkers::NoteDeath(list, settings, Bandit(0xC), 0, 0.0f);
	UInt32 due[8] = {};

	Presences where{{{0xA, Presence::Present}, {0xB, Presence::Absent}, {0xC, Presence::Present}}};
	Check(walkers::TickWaiting(list, 0.4f, where, due, 8) == 0 && list.count == 3, "0.4 s: nobody due yet");
	Check(Near(list.entries[0].secondsLeft, 0.6f) && Near(list.entries[1].secondsLeft, 1.0f) &&
	          Near(list.entries[2].secondsLeft, 0.6f),
	      "the present ones count down, the absent one's clock stands");
	Check(walkers::TickWaiting(list, 0.0f, where, due, 8) == 0 && Near(list.entries[0].secondsLeft, 0.6f),
	      "a frame of zero seconds changes nothing");

	UInt32 n = walkers::TickWaiting(list, 0.7f, where, due, 8);
	Check(n == 2 && due[0] == 0xA && due[1] == 0xC, "0.7 s more: both present ones due, in the list's order");
	Check(list.count == 1 && list.entries[0].refId == 0xB && Near(list.entries[0].secondsLeft, 1.0f),
	      "they left the list, the absent one stays with his clock");

	where.map = {{0xB, Presence::Gone}};
	Check(walkers::TickWaiting(list, 0.1f, where, due, 8) == 0 && list.count == 0,
	      "a reference that is gone leaves the list, nobody due");

	walkers::WaitingList many;
	for (UInt32 i = 0; i < 3; ++i) {
		walkers::NoteDeath(many, settings, Bandit(0x10 + i), 0, 0.0f);
	}
	Presences all{{{0x10, Presence::Present}, {0x11, Presence::Present}, {0x12, Presence::Present}}};
	n = walkers::TickWaiting(many, 2.0f, all, due, 2);
	Check(n == 2 && due[0] == 0x10 && due[1] == 0x11 && many.count == 1 && many.entries[0].refId == 0x12 &&
	          many.entries[0].secondsLeft == 0.0f,
	      "more due than the frame takes: the rest wait at zero");
	n = walkers::TickWaiting(many, 0.0f, all, due, 2);
	Check(n == 1 && due[0] == 0x12 && many.count == 0, "and come the next frame, even one of zero seconds");
}

void TestWalkOnly() {
	std::printf("Walkers walk\n");
	Check(walkers::WalkInsteadOfRun(0x0201) == 0x0101, "a run forward becomes a walk forward");
	Check(walkers::WalkInsteadOfRun(0x0301) == 0x0101, "run and walk both set: the walk stays");
	Check(walkers::WalkInsteadOfRun(0x0101) == 0x0101, "a walk is kept");
	Check(walkers::WalkInsteadOfRun(0x0000) == 0x0000, "standing is kept");
	Check(walkers::WalkInsteadOfRun(0x0230) == 0x0130, "the other bits kept");
}

void TestRequeue() {
	std::printf("A due rise put back\n");
	walkers::WaitingList list;
	Check(walkers::Requeue(list, 0xFF000EEB, 0x0003DB2F, 2.0f) && list.count == 1 &&
	          list.entries[0].secondsLeft == 2.0f && list.entries[0].baseId == 0x0003DB2F,
	      "put back with its seconds and base");
	Check(!walkers::Requeue(list, 0xFF000EEB, 0x0003DB2F, 2.0f) && list.count == 1, "not twice");
	walkers::WaitingList full;
	full.count = walkers::kWaitingMost;
	Check(!walkers::Requeue(full, 0x1, 0x2, 2.0f), "a full list takes none");
}

void TestEspRace() {
	std::printf("The ESP's race by load index and name\n");
	Check(walkers::ZombieRaceFormId(0x0B) == 0x0B000800 && walkers::ZombieRaceFormId(1) == 0x01000800,
	      "local ID 0x800 under the mod index");
	Check(walkers::SameModName("OblivionWalkers.esp", "oblivionwalkers.ESP"), "names compare without case");
	Check(!walkers::SameModName("OblivionWalkers.esp", "OblivionWalkers.esm"), "a different extension is another file");
	Check(!walkers::SameModName("OblivionWalkers.esp", "OblivionWalkers"), "a prefix is not the name");
	Check(!walkers::SameModName("OblivionWalkers", "OblivionWalkers.esp"), "nor the other way");
	Check(walkers::SameModName("", ""), "two empty names are the same");
	char endless[300];
	for (char& c : endless) {
		c = 'a';
	}
	Check(!walkers::SameModName(endless, endless), "a name longer than a path is no name");
}

void TestSwap() {
	std::printf("The swap in flight\n");
	// Nadi, 2026-10-24: the walker fell onto the spot and got up, the corpse
	// gone - it should take the corpse's place and get up from there. So the
	// walker drops unseen, the corpse stays until the walker lies, then the
	// two change places and the walker lies a moment before it gets up.
	walkers::Swap idle;
	Check(walkers::StepSwap(idle, 1.0f, 0xFF000BB2, true, 1.5f, 0.0f, 1.0f) == walkers::kSwapWait && !idle.active,
	      "no swap in flight: nothing, whatever stands there");

	walkers::Swap swap;
	swap.active = true;
	swap.corpseId = 0xFF000BAF;
	swap.walkerBase = 0x0B000801;
	Check(walkers::StepSwap(swap, 0.5f, 0, false, 1.5f, 0.0f, 1.0f) == walkers::kSwapWait &&
	          swap.stage == walkers::SwapStage::Placed,
	      "placed, no walker yet: wait");
	Check(walkers::StepSwap(swap, 0.5f, 0xFF000BB2, false, 1.5f, 0.0f, 1.0f) == walkers::kSwapFound &&
	          swap.stage == walkers::SwapStage::Loading && swap.zombieId == 0xFF000BB2 && swap.seconds == 0.0f,
	      "the walker's reference exists: wait for its model, the clock restarts");
	Check(walkers::StepSwap(swap, 3.0f, 0, false, 1.5f, 0.0f, 1.0f) == walkers::kSwapWait && swap.active,
	      "its model still loading: wait, however long DropSeconds is");
	Check(walkers::StepSwap(swap, 0.5f, 0, true, 1.5f, 0.0f, 1.0f) == walkers::kSwapDropUnseen &&
	          swap.stage == walkers::SwapStage::Dropped && swap.seconds == 0.0f,
	      "the model is there: made unseen and killed, the corpse still lying, the clock restarts");
	Check(walkers::StepSwap(swap, 1.0f, 0, true, 1.5f, 0.0f, 1.0f) == walkers::kSwapWait && swap.active,
	      "the unseen ragdoll settling: wait");
	Check(walkers::StepSwap(swap, 0.5f, 0, true, 1.5f, 0.0f, 1.0f) == walkers::kSwapPose &&
	          swap.stage == walkers::SwapStage::Posed && swap.active,
	      "DropSeconds over: the unseen walker laid into the corpse's place and pose (MatchCorpse)");
	Check(walkers::StepSwap(swap, 0.0f, 0, true, 1.5f, 0.0f, 1.0f) == walkers::kSwapReveal &&
	          swap.stage == walkers::SwapStage::Lying && swap.seconds == 0.0f && swap.active,
	      "the next frame, its bones moved: the corpse hidden, the walker shown, the items over");
	Check(walkers::StepSwap(swap, 0.9f, 0, true, 1.5f, 0.0f, 1.0f) == walkers::kSwapWait && swap.active,
	      "the walker lying in the corpse's place: wait");
	Check(walkers::StepSwap(swap, 0.1f, 0, true, 1.5f, 0.0f, 1.0f) == walkers::kSwapRaise && !swap.active &&
	          swap.stage == walkers::SwapStage::Done,
	      "LieSeconds over: it gets up - the swap ends");
	Check(walkers::StepSwap(swap, 1.0f, 0, true, 1.5f, 0.0f, 1.0f) == walkers::kSwapWait, "and nothing after");

	// MatchCorpse=2 (Nadi, 2026-10-25: "B dann A"): laid into the corpse's
	// pose, left unseen SettleSeconds for its joints to settle it, then the
	// settled walker moved and turned whole onto the corpse's place and shown
	// in the same frame, the corpse hidden (the collision test of 13:31: the
	// corpse lying on spoils the placement),
	// frame later shown.
	walkers::Swap settle;
	settle.active = true;
	walkers::StepSwap(settle, 0.1f, 0xFF000BB5, false, 1.5f, 1.5f, 1.0f);
	walkers::StepSwap(settle, 0.1f, 0, true, 1.5f, 1.5f, 1.0f);
	Check(walkers::StepSwap(settle, 1.5f, 0, true, 1.5f, 1.5f, 1.0f) == walkers::kSwapPose &&
	          settle.stage == walkers::SwapStage::Posed,
	      "settling: first laid into the corpse's pose");
	Check(walkers::StepSwap(settle, 0.016f, 0, true, 1.5f, 1.5f, 1.0f) == walkers::kSwapWait && settle.active,
	      "then left unseen for its joints to settle - not shown yet");
	Check(walkers::StepSwap(settle, 1.5f, 0, true, 1.5f, 1.5f, 1.0f) == walkers::kSwapPlace &&
	          settle.stage == walkers::SwapStage::Lying && settle.seconds == 0.0f,
	      "SettleSeconds over: the settled walker moved and turned onto the corpse's place, and the two change "
	      "places in that same frame - the corpse gone before the next physics step can push the walker");
	Check(walkers::StepSwap(settle, 0.9f, 0, true, 1.5f, 1.5f, 1.0f) == walkers::kSwapWait && settle.active,
	      "lying in the corpse's place");
	Check(walkers::StepSwap(settle, 0.1f, 0, true, 1.5f, 1.5f, 1.0f) == walkers::kSwapRaise && !settle.active,
	      "LieSeconds on: up");

	walkers::Swap nobody;
	nobody.active = true;
	Check(walkers::StepSwap(nobody, 1.9f, 0, false, 1.5f, 0.0f, 1.0f) == walkers::kSwapWait,
	      "no walker for 1.9 s: still waiting");
	Check(walkers::StepSwap(nobody, 0.1f, 0, false, 1.5f, 0.0f, 1.0f) == walkers::kSwapGaveUp && !nobody.active,
	      "none within two seconds: given up, the man gets up as himself");

	walkers::Swap noModel;
	noModel.active = true;
	Check(walkers::StepSwap(noModel, 0.1f, 0xFF000BB4, false, 1.5f, 0.0f, 1.0f) == walkers::kSwapFound,
	      "found, waiting for the model");
	Check(walkers::StepSwap(noModel, 14.9f, 0, false, 1.5f, 0.0f, 1.0f) == walkers::kSwapWait && noModel.active,
	      "the model 14.9 s on the way: still waiting");
	Check(walkers::StepSwap(noModel, 0.1f, 0, false, 1.5f, 0.0f, 1.0f) == walkers::kSwapAbandon && !noModel.active,
	      "none within fifteen seconds: the walker abandoned, the man gets up as himself");

	walkers::Swap quick;
	quick.active = true;
	Check(walkers::StepSwap(quick, 0.016f, 0xFF000BB3, true, 0.0f, 0.0f, 0.0f) == walkers::kSwapFound,
	      "found at once, model there: still one frame for the kill");
	Check(walkers::StepSwap(quick, 0.016f, 0, true, 0.0f, 0.0f, 0.0f) == walkers::kSwapDropUnseen,
	      "the next frame: unseen and killed");
	Check(walkers::StepSwap(quick, 0.016f, 0, true, 0.0f, 0.0f, 0.0f) == walkers::kSwapPose,
	      "DropSeconds=0: laid into the corpse's place the frame after");
	Check(walkers::StepSwap(quick, 0.016f, 0, true, 0.0f, 0.0f, 0.0f) == walkers::kSwapReveal,
	      "and shown the frame after that");
	Check(walkers::StepSwap(quick, 0.016f, 0, true, 0.0f, 0.0f, 0.0f) == walkers::kSwapRaise,
	      "LieSeconds=0: up the frame after that");

	walkers::Swap identity;
	const UInt32 before[2] = {0xFF000EE4, 0xFF000F00};
	walkers::NoteKnownWalkers(identity, before, 2);
	Check(identity.knownCount == 2 && walkers::IsKnownWalker(identity, 0xFF000F00) &&
	          !walkers::IsKnownWalker(identity, 0xFF000F33),
	      "the walkers before the placement are known, a new one is not");
	const UInt32 after[3] = {0xFF000EE4, 0xFF000F33, 0xFF000F00};
	Check(walkers::NewWalkerAmong(identity, after, 3) == 0xFF000F33, "the one not known before is the new walker");
	Check(walkers::NewWalkerAmong(identity, before, 2) == 0, "nothing new among the old ones");
	const UInt32 zeros[2] = {0, 0};
	Check(walkers::NewWalkerAmong(identity, zeros, 2) == 0, "zero is no reference");
	UInt32 many[20];
	for (UInt32 i = 0; i < 20; ++i) {
		many[i] = 0x1000 + i;
	}
	walkers::NoteKnownWalkers(identity, many, 20);
	Check(identity.knownCount == walkers::kSwapKnownMost, "more known than the swap holds: the first sixteen");

	const UInt32 bases[3] = {0x0B000801, 0x0B000802, 0x0B000803};
	Check(walkers::PickWalker(bases, 3, 0) == 0x0B000801 && walkers::PickWalker(bases, 3, 4) == 0x0B000802 &&
	          walkers::PickWalker(bases, 3, 0xFFFFFFFF) == 0x0B000801,
	      "one of the walkers by the random number");
	Check(walkers::PickWalker(bases, 0, 7) == 0, "none without walkers");
	Check(walkers::EspFormId(0x0B, 0x801) == 0x0B000801 && walkers::kWalkerLocalIds[3] == 0x804,
	      "the walkers' IDs under the ESP's index");
}

void TestRisenWatch() {
	std::printf("The watch on a risen one\n");
	walkers::RisenWatch watch;
	watch.refId = 0xFF000BAF;
	watch.startMs = 1000;
	watch.x = 100.0f;
	watch.y = 200.0f;
	watch.z = 0.0f;
	const UInt32 noLook = 3000;
	Check(walkers::StepRisenWatch(watch, true, 100.0f, 200.0f, 0.0f, 1016, noLook) == 0, "dead and still: nothing");
	Check(walkers::StepRisenWatch(watch, true, 105.0f, 200.0f, 0.0f, 1032, noLook) == 0,
	      "a shift under eight units: nothing");
	Check(walkers::StepRisenWatch(watch, false, 105.0f, 200.0f, 0.0f, 3400, noLook) == walkers::kRisenAlive &&
	          watch.aliveSeen,
	      "counted alive: the alive event, once");
	Check(walkers::StepRisenWatch(watch, false, 105.0f, 200.0f, 0.0f, 3416, noLook) == 0, "and not again");
	Check(walkers::StepRisenWatch(watch, false, 100.0f, 210.0f, 0.0f, 4100, noLook) ==
	          (walkers::kRisenMoved | walkers::kRisenDone),
	      "ten units off the spot: the move event, and done with both seen and no look wanted");

	walkers::RisenWatch slow;
	slow.startMs = 1000;
	Check(walkers::StepRisenWatch(slow, true, 0.0f, 0.0f, 0.0f, 15999, noLook) == 0,
	      "still dead just before the time is up");
	Check(walkers::StepRisenWatch(slow, true, 0.0f, 0.0f, 0.0f, 16000, noLook) == walkers::kRisenDone && !slow.aliveSeen,
	      "the time up: done, nothing seen");
	walkers::RisenWatch moveFirst;
	moveFirst.startMs = 0;
	Check(walkers::StepRisenWatch(moveFirst, true, 20.0f, 0.0f, 0.0f, 500, noLook) == walkers::kRisenMoved,
	      "moved while still dead (the ragdoll settling): the move event alone, not done");
	Check(walkers::StepRisenWatch(moveFirst, false, 20.0f, 0.0f, 0.0f, 900, noLook) ==
	          (walkers::kRisenAlive | walkers::kRisenDone),
	      "then alive: done");

	walkers::RisenWatch look;
	look.startMs = 1000;
	look.lookWanted = true;
	Check(walkers::StepRisenWatch(look, false, 20.0f, 0.0f, 0.0f, 1500, 3000) ==
	          (walkers::kRisenAlive | walkers::kRisenMoved),
	      "a look wanted: alive and moved seen, but not done before the look");
	Check(walkers::StepRisenWatch(look, false, 20.0f, 0.0f, 0.0f, 3999, 3000) == 0, "just before LookAfterSeconds: nothing");
	Check(walkers::StepRisenWatch(look, false, 20.0f, 0.0f, 0.0f, 4000, 3000) ==
	          (walkers::kRisenLook | walkers::kRisenDone) &&
	          look.lookDone,
	      "at LookAfterSeconds: the look event, and done");
	Check(walkers::StepRisenWatch(look, false, 20.0f, 0.0f, 0.0f, 4016, 3000) == walkers::kRisenDone,
	      "the look only once; done stays");
	walkers::RisenWatch lookNow;
	lookNow.startMs = 0;
	lookNow.lookWanted = true;
	Check(walkers::StepRisenWatch(lookNow, true, 0.0f, 0.0f, 0.0f, 16, 0) == walkers::kRisenLook,
	      "LookAfterSeconds=0: the look at the first step, still dead and still: not done");
	walkers::RisenWatch lookLate;
	lookLate.startMs = 0;
	lookLate.lookWanted = true;
	Check(walkers::StepRisenWatch(lookLate, false, 20.0f, 0.0f, 0.0f, 15000, 20000) ==
	          (walkers::kRisenAlive | walkers::kRisenMoved | walkers::kRisenDone) &&
	          !lookLate.lookDone,
	      "a look set past the watch's time: the watch ends without it");
}

bool ResolveSame(UInt32 id, UInt32& out) {
	out = id;
	return true;
}

void TestImage() {
	std::printf("The co-save image\n");
	walkers::RiseSettings settings;
	walkers::WaitingList list;
	walkers::NoteDeath(list, settings, Bandit(0xFF000BAF), 0, 0.25f);
	walkers::NoteDeath(list, settings, Bandit(0xFF000BB0), 0, 0.0f);
	UInt8 image[walkers::kWaitingImageMost] = {};
	const UInt32 length = walkers::WaitingImage(list, image, sizeof(image));
	Check(length == 4 + 2 * walkers::kWaitingEntryBytes, "two entries: the count and twelve bytes each");
	Check(image[0] == 2 && image[4] == 0xAF && image[7] == 0xFF, "little-endian, the count first");

	walkers::WaitingList back;
	Check(walkers::ReadWaitingImage(back, image, length, &ResolveSame) && back.count == 2, "read back: two");
	Check(back.entries[0].refId == 0xFF000BAF && back.entries[0].baseId == 0x00069AD1 &&
	          Near(back.entries[0].secondsLeft, 22.5f) && back.entries[1].refId == 0xFF000BB0,
	      "reference, base and seconds as written");

	walkers::WaitingList none;
	const UInt32 emptyLength = walkers::WaitingImage(none, image, sizeof(image));
	Check(emptyLength == 4 && walkers::ReadWaitingImage(back, image, emptyLength, &ResolveSame) && back.count == 0,
	      "an empty list: four bytes, read back empty");

	UInt8 small[8];
	Check(walkers::WaitingImage(list, small, sizeof(small)) == 0, "a buffer too small: nothing written");

	walkers::WaitingImage(list, image, sizeof(image));
	Check(!walkers::ReadWaitingImage(back, image, length - 1, &ResolveSame) && back.count == 1,
	      "cut short by a byte: false, the whole entries before it kept");
	Check(!walkers::ReadWaitingImage(back, image, 3, &ResolveSame) && back.count == 0, "shorter than the count: false, empty");
	UInt8 bloated[8] = {0xFF, 0xFF, 0, 0, 0, 0, 0, 0};
	Check(!walkers::ReadWaitingImage(back, bloated, sizeof(bloated), &ResolveSame),
	      "a count above the list's size: false");

	auto dropSecond = [](UInt32 id, UInt32& out) {
		out = id + 1;
		return id != 0xFF000BB0;
	};
	Check(walkers::ReadWaitingImage(back, image, length, dropSecond) && back.count == 1 &&
	          back.entries[0].refId == 0xFF000BB0,
	      "a reference the game cannot resolve is dropped, the others take the resolved ID");
}

void TestRisenLoot() {
	std::printf("The risen keep their loot\n");
	walkers::RisenSet risen;
	Check(!walkers::IsRisen(risen, 0xFF000C01), "nobody risen yet");
	walkers::NoteRisen(risen, 0xFF000C01);
	walkers::NoteRisen(risen, 0xFF000C01);
	Check(walkers::IsRisen(risen, 0xFF000C01) && risen.count == 1, "noted once, however often the swap says so");
	walkers::NoteRisen(risen, 0);
	Check(risen.count == 1, "no reference: nothing noted");

	Check(walkers::BlocksLoot(true, true, false), "NoLoot, a walker from a spawn: no loot");
	Check(!walkers::BlocksLoot(true, true, true), "NoLoot, a risen walker: lootable all the same");
	Check(!walkers::BlocksLoot(false, true, false), "NoLoot off: lootable");
	Check(!walkers::BlocksLoot(true, false, false), "no walker: never touched");

	walkers::RisenSet full;
	for (UInt32 i = 0; i < walkers::kRisenMost; ++i) {
		walkers::NoteRisen(full, 0xFF001000 + i);
	}
	walkers::NoteRisen(full, 0xFF00FFFF);
	Check(full.count == walkers::kRisenMost && !walkers::IsRisen(full, 0xFF001000) &&
	          walkers::IsRisen(full, 0xFF001001) && walkers::IsRisen(full, 0xFF00FFFF),
	      "full: the oldest goes for the new one");

	auto onlyEven = [](UInt32 id) { return (id & 1) == 0; };
	walkers::RisenSet some;
	walkers::NoteRisen(some, 0xFF000C02);
	walkers::NoteRisen(some, 0xFF000C03);
	walkers::NoteRisen(some, 0xFF000C04);
	Check(walkers::KeepRisen(some, onlyEven) == 1 && some.count == 2 && walkers::IsRisen(some, 0xFF000C02) &&
	          walkers::IsRisen(some, 0xFF000C04) && !walkers::IsRisen(some, 0xFF000C03),
	      "those gone from the game are dropped, the order of the rest kept");

	UInt8 image[walkers::kRisenImageMost] = {};
	const UInt32 length = walkers::RisenImage(some, image, sizeof(image));
	Check(length == 4 + 2 * 4 && image[0] == 2 && image[4] == 0x02 && image[7] == 0xFF,
	      "the image: the count, then four bytes each, little-endian");
	walkers::RisenSet back;
	Check(walkers::ReadRisenImage(back, image, length, &ResolveSame) && back.count == 2 &&
	          walkers::IsRisen(back, 0xFF000C04),
	      "read back as written");
	auto dropFirst = [](UInt32 id, UInt32& out) {
		out = id;
		return id != 0xFF000C02;
	};
	Check(walkers::ReadRisenImage(back, image, length, dropFirst) && back.count == 1 &&
	          walkers::IsRisen(back, 0xFF000C04),
	      "a reference the game cannot resolve is dropped");
	Check(!walkers::ReadRisenImage(back, image, length - 1, &ResolveSame) && back.count == 1,
	      "cut short: false, the whole entries before it kept");
	Check(!walkers::ReadRisenImage(back, image, 3, &ResolveSame) && back.count == 0, "shorter than the count: false");
	UInt8 bloated[8] = {0xFF, 0xFF, 0, 0, 0, 0, 0, 0};
	Check(!walkers::ReadRisenImage(back, bloated, sizeof(bloated), &ResolveSame), "a count above the set's size: false");
	UInt8 small[6];
	Check(walkers::RisenImage(some, small, sizeof(small)) == 0, "a buffer too small: nothing written");
}

void TestFame() {
	std::printf("Fame for walkers cut down\n");
	Check(walkers::FameKill(true, true), "a walker the player killed: counts");
	Check(!walkers::FameKill(true, false), "a walker someone else killed (or the console): not");
	Check(!walkers::FameKill(false, true), "anyone else the player killed: not");

	walkers::FameCount count;
	bool earned = false;
	for (UInt32 i = 0; i < 49; ++i) {
		earned = earned || walkers::CountWalkerKill(count, 50);
	}
	Check(!earned && count.kills == 49, "49 of 50: no fame yet");
	Check(walkers::CountWalkerKill(count, 50) && count.kills == 0, "the 50th: a point, the count starts over");
	Check(!walkers::CountWalkerKill(count, 50) && count.kills == 1, "and the next one counts toward the next point");

	walkers::FameCount every;
	Check(walkers::CountWalkerKill(every, 1) && walkers::CountWalkerKill(every, 1) && every.kills == 0,
	      "FamePerKills=1: a point for every walker");
	walkers::FameCount off;
	Check(!walkers::CountWalkerKill(off, 0) && off.kills == 0, "FamePerKills=0: off, nothing counted");
	walkers::FameCount lowered;
	lowered.kills = 40;
	Check(walkers::CountWalkerKill(lowered, 10) && lowered.kills == 0,
	      "a count past a lowered threshold (40 kept, now 10): a point at the next kill, not four");

	UInt8 image[walkers::kFameImageBytes] = {};
	walkers::FameCount saved;
	saved.kills = 0x01020304;
	Check(walkers::FameImage(saved, image, sizeof(image)) == 4 && image[0] == 0x04 && image[3] == 0x01,
	      "the image: the count, four bytes little-endian");
	walkers::FameCount back;
	Check(walkers::ReadFameImage(back, image, 4) && back.kills == 0x01020304, "read back as written");
	walkers::FameCount untouched;
	untouched.kills = 7;
	Check(!walkers::ReadFameImage(untouched, image, 3) && untouched.kills == 0, "cut short: false, the count at 0");
	UInt8 tiny[3];
	Check(walkers::FameImage(saved, tiny, sizeof(tiny)) == 0, "a buffer too small: nothing written");
}

void TestChapels() {
	std::printf("No rise in a chapel\n");
	Check(walkers::IsChapelCell("AnvilChapelHall", 0x000223BC), "a chapel by name and by its owner, the Nine");
	Check(walkers::IsChapelCell("SomeModTemple", 0x000223BC), "a cell the Nine own, whatever its name");
	Check(walkers::IsChapelCell("KvatchChapelofAkatosh", 0), "Kvatch's chapel, owned by nobody: by its name");
	Check(walkers::IsChapelCell("weynonpriorychapel", 0x000274AB), "the name without case (Weynon's, the Blades')");
	Check(!walkers::IsChapelCell("AnvilFightersGuild", 0x00022296), "a guild hall: no chapel");
	Check(!walkers::IsChapelCell("CadlewChapel", 0) && !walkers::IsChapelCell("cadlewchapel", 0),
	      "Cadlew Chapel, the crypt: no chapel (Nadi, 2026-10-24 evening)");
	Check(!walkers::IsChapelCell(nullptr, 0), "no editor ID, no owner: no chapel");
	Check(!walkers::IsChapelCell("", 0), "an empty editor ID: no chapel");

	using walkers::DeathVerdict;
	walkers::RiseSettings settings;
	walkers::WaitingList list;
	walkers::Fallen inside = Bandit();
	inside.inChapel = true;
	Check(walkers::NoteDeath(list, settings, inside, 0, 0.0f) == DeathVerdict::InChapel && list.count == 0,
	      "AvoidChapels=1, a death in a chapel: stays down");
	Check(std::strcmp(walkers::VerdictText(DeathVerdict::InChapel), "stays down (in a chapel, AvoidChapels=1)") == 0,
	      "and the log says why");
	walkers::RiseSettings anywhere;
	anywhere.avoidChapels = false;
	Check(walkers::NoteDeath(list, anywhere, inside, 0, 0.0f) == DeathVerdict::Waits && list.count == 1,
	      "AvoidChapels=0: he rises there too");

	// Nadi, 2026-10-24 evening: none of ours spawns in a chapel - one that
	// walks in after the player may.
	Check(walkers::SpawnedInChapel(true, true, false, false),
	      "AvoidChapels=1, a live walker first seen in a chapel: spawned there, taken away");
	Check(!walkers::SpawnedInChapel(true, true, true, false),
	      "one seen outside before, now in a chapel: it walked in, left alone");
	Check(!walkers::SpawnedInChapel(true, false, false, false), "outside a chapel: left alone");
	Check(!walkers::SpawnedInChapel(false, true, false, false), "AvoidChapels=0: left alone");
	Check(!walkers::SpawnedInChapel(true, true, false, true), "taken away already: not again");
}

void TestApartDue() {
	std::printf("KeepCorpseApart - when the corpse is moved out of the walker's way\n");
	using walkers::ApartDue;
	using walkers::SwapStage;
	Check(ApartDue(true, SwapStage::Dropped, false), "the walker's model there, dropping unseen: now");
	Check(ApartDue(true, SwapStage::Posed, false), "laid, settling unseen, not yet done: still");
	Check(!ApartDue(false, SwapStage::Dropped, false), "the setting off: never");
	Check(!ApartDue(true, SwapStage::Dropped, true), "done for this walker: not again");
	Check(!ApartDue(true, SwapStage::Placed, false) && !ApartDue(true, SwapStage::Loading, false),
	      "no walker model yet: nothing to go by");
	Check(!ApartDue(true, SwapStage::Lying, false) && !ApartDue(true, SwapStage::Done, false),
	      "the two changed places: the corpse is gone");
}

void TestZombieDefaults() {
	std::printf("The [Zombies] defaults\n");
	walkers::RiseSettings settings;
	Check(settings.speedMultiplier == 1.0f && settings.speedRandomPercent == 30 && settings.famePerKills == 0,
	      "SpeedMultiplier=1.0, SpeedRandomPercent=30, FamePerKills=0 (spec 4.4, 4.8; Nadi: no fame by default)");
	Check(settings.healthPercent == 15 && settings.avoidChapels,
	      "HealthPercent=15 (Nadi, 2026-10-24 evening), AvoidChapels=1");
	Check(settings.settleSeconds == 1.5f && !settings.hideCorpseAtPose,
	      "[Rise] SettleSeconds=1.5 - the pose settles unseen while the corpse still lies, never a gap (Nadi, "
	      "2026-10-25: \"keine Luecke\", after 0 slid seen once in four runs); the test switch HideCorpseAtPose off");
	Check(settings.horde.enabled && settings.horde.joinRadius == 1500.0f && settings.horde.leadUnits == 2000.0f &&
	          settings.horde.staleSeconds == 30.0f && settings.horde.turnDegrees == 60.0f && settings.horde.groupMost == 8,
	      "[Horde] Enabled=1 (the runs of 2026-10-26 09:24 and 09:28 hold), JoinRadius=1500, LeadUnits=2000, StaleSeconds=30, TurnDegrees=60, "
	      "GroupMost=8");
	Check(settings.horde.towardCities && settings.horde.cityPull == 0.5f && settings.horde.cityRange == 40000.0f &&
	          settings.horde.gateHoldUnits == 800.0f,
	      "[Horde] TowardCities=1, CityPull=0.5, CityRange=40000, GateHoldUnits=800 (4.11b)");
	Check(settings.holdDeathSound, "[Rise] HoldDeathSound=1 - the unseen kill's death sound held back (a test switch)");
	Check(settings.soundAtGetUpPercent == 60,
	      "[Rise] SoundAtGetUpPercent=60 - the death sound held back at the unseen kill plays as the walker gets up "
	      "60 times in 100 (Nadi, 2026-10-26: \"percentage chance fuer den sound 60\")");
	Check(settings.blockMooResurrect,
	      "[Zombies] BlockMooResurrect=1 - a dead walker loses MOO's token and brain (Nadi, 2026-10-25: go)");
	Check(settings.keepCorpseApart,
	      "[Rise] KeepCorpseApart=1 (Nadi, 2026-10-25: go after the runs of 18:04 to 18:09)");
	Check(settings.matchCorpse == 2 && settings.lieSeconds == 1.0f,
	      "[Rise] MatchCorpse=2, the pose settled then placed, the corpse kept apart (Nadi, 2026-10-25: go), "
	      "LieSeconds=1");
}

}  // namespace

// Every walker of every kind from one walk of the process lists (found
// 2026-10-26: StepWalkers took at most 16 of each kind a frame, so Walk=1
// and AvoidChapels missed the rest of a horde).
void TestKeepWalkers() {
	std::printf("KeepWalkers\n");
	const UInt32 bases[] = {0x801, 0x802, 0x803, 0x804};
	UInt32 ids[300];
	UInt32 baseOf[300];
	UInt32 n = 0;
	for (UInt32 i = 0; i < 100; ++i) {  // a hundred of one kind
		ids[n] = 0xFF000100 + i;
		baseOf[n++] = 0x801;
	}
	for (UInt32 i = 0; i < 30; ++i) {  // thirty of another, and their doubles
		ids[n] = 0xFF000300 + i;
		baseOf[n++] = 0x803;
		ids[n] = 0xFF000300 + i;
		baseOf[n++] = 0x803;
	}
	for (UInt32 i = 0; i < 20; ++i) {  // others' creatures
		ids[n] = 0xFF000500 + i;
		baseOf[n++] = 0x0002E9F1;
	}
	UInt32 out[256];
	const UInt32 kept = walkers::KeepWalkers(ids, baseOf, n, bases, 4, out, 256);
	bool ours = true;
	bool once = true;
	for (UInt32 i = 0; i < kept; ++i) {
		ours = ours && (out[i] - 0xFF000100 < 100 || out[i] - 0xFF000300 < 30);
		for (UInt32 j = i + 1; j < kept; ++j) {
			once = once && out[i] != out[j];
		}
	}
	Check(kept == 130 && ours && once, "a hundred of one kind and thirty of another: all 130, each once, no stranger");
	Check(walkers::KeepWalkers(ids, baseOf, n, bases, 4, out, 64) == 64, "never more than asked for");
	Check(walkers::KeepWalkers(ids, baseOf, n, bases, 0, out, 256) == 0, "no walker kinds: none");
}

int main() {
	TestVerdicts();
	TestTick();
	TestWalkOnly();
	TestRequeue();
	TestEspRace();
	TestSwap();
	TestRisenWatch();
	TestImage();
	TestRisenLoot();
	TestFame();
	TestChapels();
	TestApartDue();
	TestZombieDefaults();
	TestKeepWalkers();
	if (g_failures != 0) {
		std::printf("%d failed\n", g_failures);
		return 1;
	}
	std::printf("all passed\n");
	return 0;
}
