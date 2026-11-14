// Every flow of the held dungeons' journal (docs/walkers-spec.md 4.20.7,
// step 4): which stage of a place's quest the player is told - near it, by
// an event, by hearing of it -, when the quest is taken off "completed"
// first, which stages reward, which place is news, and the quest's name.

#include "DungeonQuestLogic.h"

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

using walkers::DungeonEventKind;
using walkers::DungeonSitesState;
using walkers::DungeonState;
using namespace walkers;

void TestStages() {
	std::printf("the stages\n");
	Check(JournalOfState(DungeonState::Refuge) == kJournalRefuge &&
	          JournalOfState(DungeonState::Besieged) == kJournalBesieged &&
	          JournalOfState(DungeonState::ToClear) == kJournalToClear &&
	          JournalOfState(DungeonState::WalkerDungeon) == kJournalWalkerDungeon &&
	          JournalOfState(DungeonState::Free) == kJournalNone,
	      "a state's stage: 10, 20, 30, 40; none for a free site");
	Check(JournalCompletes(kJournalRefuge) && JournalCompletes(kJournalDefended) &&
	          JournalCompletes(kJournalHeldAlone) && JournalCompletes(kJournalTaken) && JournalCompletes(kJournalLeft),
	      "a quiet refuge, defended, held without him, taken, left: the quest is completed");
	Check(!JournalCompletes(kJournalHeard) && !JournalCompletes(kJournalBesieged) &&
	          !JournalCompletes(kJournalToClear) && !JournalCompletes(kJournalWalkerDungeon) &&
	          !JournalCompletes(kJournalNone),
	      "heard of, besieged, to clear, a walker dungeon: something to do - active");
	Check(JournalKnows(kJournalNone) == DungeonState::Free && JournalKnows(kJournalHeard) == DungeonState::Free &&
	          JournalKnows(kJournalLeft) == DungeonState::Free,
	      "nothing told, only heard of, left: he does not know how it stands");
	Check(JournalKnows(kJournalRefuge) == DungeonState::Refuge && JournalKnows(kJournalDefended) == DungeonState::Refuge &&
	          JournalKnows(kJournalHeldAlone) == DungeonState::Refuge && JournalKnows(kJournalTaken) == DungeonState::Refuge,
	      "refuge, defended, held, taken: he knows it a refuge");
	Check(JournalKnows(kJournalBesieged) == DungeonState::Besieged &&
	          JournalKnows(kJournalToClear) == DungeonState::ToClear &&
	          JournalKnows(kJournalWalkerDungeon) == DungeonState::WalkerDungeon && JournalKnows(77) == DungeonState::Free,
	      "besieged, to clear, a walker dungeon known as such; a stage unknown: nothing");
}

void TestNear() {
	std::printf("the player near\n");
	Check(JournalNear(kJournalNone, DungeonState::Refuge) == kJournalRefuge &&
	          JournalNear(kJournalHeard, DungeonState::Refuge) == kJournalRefuge,
	      "a refuge found, or come to after hearing of it: told a refuge");
	Check(JournalNear(kJournalRefuge, DungeonState::Refuge) == kJournalNone &&
	          JournalNear(kJournalDefended, DungeonState::Refuge) == kJournalNone &&
	          JournalNear(kJournalHeldAlone, DungeonState::Refuge) == kJournalNone &&
	          JournalNear(kJournalTaken, DungeonState::Refuge) == kJournalNone,
	      "a refuge he knows as one: nothing new");
	Check(JournalNear(kJournalBesieged, DungeonState::Refuge) == kJournalHeldAlone,
	      "he knew it besieged and finds it a refuge: they held it without him");
	Check(JournalNear(kJournalToClear, DungeonState::Refuge) == kJournalTaken &&
	          JournalNear(kJournalWalkerDungeon, DungeonState::Refuge) == kJournalTaken,
	      "he knew it to clear or fallen and finds a refuge: taken");
	Check(JournalNear(kJournalNone, DungeonState::Besieged) == kJournalBesieged &&
	          JournalNear(kJournalRefuge, DungeonState::Besieged) == kJournalBesieged &&
	          JournalNear(kJournalDefended, DungeonState::Besieged) == kJournalBesieged &&
	          JournalNear(kJournalBesieged, DungeonState::Besieged) == kJournalNone,
	      "a siege: told once, again after it was a refuge");
	Check(JournalNear(kJournalHeard, DungeonState::ToClear) == kJournalToClear &&
	          JournalNear(kJournalToClear, DungeonState::ToClear) == kJournalNone,
	      "a place to clear: told once");
	Check(JournalNear(kJournalRefuge, DungeonState::WalkerDungeon) == kJournalWalkerDungeon &&
	          JournalNear(kJournalBesieged, DungeonState::WalkerDungeon) == kJournalWalkerDungeon &&
	          JournalNear(kJournalWalkerDungeon, DungeonState::WalkerDungeon) == kJournalNone,
	      "a walker dungeon: told once - also the refuge or siege he knew, fallen");
	Check(JournalNear(kJournalBesieged, DungeonState::Free) == kJournalNone, "a free site: nothing");
	Check(JournalSees(DungeonState::Refuge, 0.0f) && JournalSees(DungeonState::Besieged, 0.0f) &&
	          JournalSees(DungeonState::WalkerDungeon, 0.0f) && JournalSees(DungeonState::ToClear, 0.0f),
	      "inside the place: he sees how it stands");
	Check(!JournalSees(DungeonState::Refuge, 300.0f) && !JournalSees(DungeonState::Besieged, 300.0f) &&
	          !JournalSees(DungeonState::WalkerDungeon, 1.0f),
	      "before the door of a refuge, a siege, a walker dungeon: not yet");
	Check(JournalSees(DungeonState::ToClear, 300.0f) && JournalSees(DungeonState::ToClear, walkers::kDungeonMeetUnits) &&
	          !JournalSees(DungeonState::ToClear, walkers::kDungeonMeetUnits + 1.0f),
	      "a place to clear: its people wait outside - met within 1500 units of the door");
	Check(!JournalSees(DungeonState::ToClear, -1.0f) && !JournalSees(DungeonState::Refuge, -1.0f), "away: never");
}

void TestEvents() {
	std::printf("the events\n");
	Check(JournalOnEvent(kJournalBesieged, DungeonEventKind::Defended) == kJournalDefended &&
	          JournalOnEvent(kJournalNone, DungeonEventKind::Defended) == kJournalDefended,
	      "defended: told, whatever he knew");
	Check(JournalOnEvent(kJournalToClear, DungeonEventKind::Taken) == kJournalTaken &&
	          JournalOnEvent(kJournalWalkerDungeon, DungeonEventKind::Taken) == kJournalTaken,
	      "cleared and taken: told");
	Check(JournalOnEvent(kJournalHeard, DungeonEventKind::Left) == kJournalLeft &&
	          JournalOnEvent(kJournalBesieged, DungeonEventKind::Left) == kJournalLeft &&
	          JournalOnEvent(kJournalToClear, DungeonEventKind::Left) == kJournalLeft &&
	          JournalOnEvent(kJournalWalkerDungeon, DungeonEventKind::Left) == kJournalLeft,
	      "given up with the quest still open: closed with 'left'");
	Check(JournalOnEvent(kJournalNone, DungeonEventKind::Left) == kJournalNone &&
	          JournalOnEvent(kJournalRefuge, DungeonEventKind::Left) == kJournalNone &&
	          JournalOnEvent(kJournalDefended, DungeonEventKind::Left) == kJournalNone &&
	          JournalOnEvent(kJournalLeft, DungeonEventKind::Left) == kJournalNone,
	      "given up unknown, or its quest completed already: nothing");
	Check(JournalOnEvent(kJournalRefuge, DungeonEventKind::Attacked) == kJournalNone &&
	          JournalOnEvent(kJournalBesieged, DungeonEventKind::Held) == kJournalNone &&
	          JournalOnEvent(kJournalBesieged, DungeonEventKind::Overrun) == kJournalNone &&
	          JournalOnEvent(kJournalBesieged, DungeonEventKind::PersonLost) == kJournalNone &&
	          JournalOnEvent(kJournalNone, DungeonEventKind::Rolled) == kJournalNone,
	      "attacked, held away, overrun, one lost, rolled: no stage by the event (he learns it near or by news)");
}

void TestHeard() {
	std::printf("hearing of a place\n");
	Check(JournalHeard(kJournalNone, DungeonState::Refuge) == kJournalHeard, "a refuge he does not know: heard of");
	Check(JournalHeard(kJournalHeard, DungeonState::Refuge) == kJournalNone &&
	          JournalHeard(kJournalRefuge, DungeonState::Refuge) == kJournalNone &&
	          JournalHeard(kJournalTaken, DungeonState::Refuge) == kJournalNone,
	      "a refuge heard of already or known: no news");
	Check(JournalHeard(kJournalBesieged, DungeonState::Refuge) == kJournalHeldAlone,
	      "the siege he knew of is over: they held it");
	Check(JournalHeard(kJournalNone, DungeonState::Besieged) == kJournalBesieged &&
	          JournalHeard(kJournalHeard, DungeonState::Besieged) == kJournalBesieged &&
	          JournalHeard(kJournalRefuge, DungeonState::Besieged) == kJournalBesieged &&
	          JournalHeard(kJournalBesieged, DungeonState::Besieged) == kJournalNone,
	      "a siege: news until he knows of it");
	Check(JournalHeard(kJournalNone, DungeonState::ToClear) == kJournalToClear &&
	          JournalHeard(kJournalNone, DungeonState::WalkerDungeon) == kJournalWalkerDungeon &&
	          JournalHeard(kJournalDefended, DungeonState::WalkerDungeon) == kJournalWalkerDungeon,
	      "a place to clear, a fallen place: news");
}

void TestNews() {
	std::printf("the place that is news\n");
	DungeonSitesState st;
	Check(DungeonNewsSite(st) == -1 && DungeonNewsKind(st, -1) == 0, "no site: no news");
	st.count = 4;
	st.sites[0].candidate = 3;
	st.sites[0].state = DungeonState::Refuge;
	st.sites[1].candidate = 4;
	st.sites[1].state = DungeonState::WalkerDungeon;
	st.sites[2].candidate = 5;
	st.sites[2].state = DungeonState::ToClear;
	st.sites[3].candidate = 6;
	st.sites[3].state = DungeonState::Besieged;
	Check(DungeonNewsSite(st) == 3 && DungeonNewsKind(st, 3) == 2, "a siege first (kind 2)");
	st.sites[3].journal = kJournalBesieged;
	Check(DungeonNewsSite(st) == 2 && DungeonNewsKind(st, 2) == 3, "then a place to clear (kind 3)");
	st.sites[2].journal = kJournalToClear;
	Check(DungeonNewsSite(st) == 1 && DungeonNewsKind(st, 1) == 4, "then a fallen place (kind 4)");
	st.sites[1].journal = kJournalWalkerDungeon;
	Check(DungeonNewsSite(st) == 0 && DungeonNewsKind(st, 0) == 1, "then a refuge he has not heard of (kind 1)");
	st.sites[0].journal = kJournalHeard;
	Check(DungeonNewsSite(st) == -1, "everything known: no news");
	st.sites[3].state = DungeonState::Refuge;
	Check(DungeonNewsSite(st) == 3 && DungeonNewsKind(st, 3) == 1,
	      "the siege he knew of over: news of a refuge (they held it)");
	st.sites[3].candidate = walkers::kDungeonNoCandidate;
	Check(DungeonNewsSite(st) == -1, "a site without a candidate: never news");
	Check(DungeonNewsKind(st, 9) == 0, "a site past the list: no kind");
}

void TestTelling() {
	std::printf("telling a stage\n");
	Check(JournalUncompletesFirst(kJournalNone, kJournalHeard) && JournalUncompletesFirst(kJournalNone, kJournalBesieged),
	      "a place's first word, something to do: an earlier time's 'completed' taken off first");
	Check(JournalUncompletesFirst(kJournalDefended, kJournalBesieged) &&
	          JournalUncompletesFirst(kJournalRefuge, kJournalWalkerDungeon),
	      "a completed quest opened again: taken off first");
	Check(!JournalUncompletesFirst(kJournalHeard, kJournalBesieged) &&
	          !JournalUncompletesFirst(kJournalBesieged, kJournalWalkerDungeon),
	      "an open quest going on: nothing to take off");
	Check(!JournalUncompletesFirst(kJournalNone, kJournalRefuge) &&
	          !JournalUncompletesFirst(kJournalBesieged, kJournalDefended),
	      "a stage that completes: nothing to take off");
	Check(JournalRewards(kJournalDefended) && JournalRewards(kJournalTaken), "defended, taken: the reward");
	Check(!JournalRewards(kJournalHeldAlone) && !JournalRewards(kJournalRefuge) && !JournalRewards(kJournalLeft) &&
	          !JournalRewards(kJournalBesieged),
	      "held without him, a refuge found, left, a siege: none");
	Check(JournalShowsMap(kJournalNone) && !JournalShowsMap(kJournalHeard) && !JournalShowsMap(kJournalRefuge),
	      "the map marker shown at the first word only");
	char id[32];
	Check(DungeonQuestEditorId(7, id, sizeof(id)) && std::strcmp(id, "WalkersDungeon007") == 0 &&
	          DungeonQuestEditorId(90, id, sizeof(id)) && std::strcmp(id, "WalkersDungeon090") == 0,
	      "the quest's editor ID: WalkersDungeon and the candidate's index, three digits");
	Check(!DungeonQuestEditorId(7, id, 8) && !DungeonQuestEditorId(1000, id, sizeof(id)),
	      "too short a buffer, an index past three digits: none");
	Check(IsDungeonQuestEditorId("WalkersDungeon007") && IsDungeonQuestEditorId("WalkersDungeonTalk"),
	      "a place's quest and the dialogue's: the dungeons' own");
	Check(!IsDungeonQuestEditorId("Walkers") && !IsDungeonQuestEditorId("WalkersEscortQuest") &&
	          !IsDungeonQuestEditorId("MQ01") && !IsDungeonQuestEditorId("") && !IsDungeonQuestEditorId(nullptr) &&
	          !IsDungeonQuestEditorId("walkersdungeon007"),
	      "LINK's, the escort's, the game's, none, another case: not");
	Check(DungeonQuestLocalId(0) == 0xFFF && DungeonQuestLocalId(90) == 0xFA5,
	      "the quest's local ID: down from 0xFFF by the candidate's index");
}

void TestTalk() {
	std::printf("what the kit's people say\n");
	Check(DungeonTalkState(DungeonState::Refuge, false) == 1 && DungeonTalkState(DungeonState::Besieged, false) == 2 &&
	          DungeonTalkState(DungeonState::ToClear, false) == 3,
	      "a refuge 1, besieged 2, waiting before a place to clear 3");
	Check(DungeonTalkState(DungeonState::Refuge, true) == 4, "a refuge he just defended or cleared: their thanks, 4");
	Check(DungeonTalkState(DungeonState::Besieged, true) == 2 && DungeonTalkState(DungeonState::ToClear, true) == 3,
	      "thanks never during a siege or before a place to clear");
	Check(DungeonTalkState(DungeonState::WalkerDungeon, false) == 0 && DungeonTalkState(DungeonState::Free, true) == 0,
	      "a walker dungeon, no place: nothing of ours");
	Check(ThankedStill(7, 7, DungeonState::Refuge) == 7, "thanked at the place the kit stands at, a refuge: still");
	Check(ThankedStill(7, -1, DungeonState::Free) == 7, "the kit nowhere yet (it moves in after the clearing): still");
	Check(ThankedStill(7, 9, DungeonState::Refuge) == -1, "the kit at another place: forgotten");
	Check(ThankedStill(7, 7, DungeonState::Besieged) == -1, "the place attacked again: forgotten");
	Check(ThankedStill(-1, 7, DungeonState::Refuge) == -1, "never thanked: none");
	Check(walkers::kDungeonTalkQuestLocalId == 0xA60 && walkers::kDungeonTalkEnd == 0xA75 &&
	          walkers::kDungeonTalkEnd <= 0xAA4,
	      "the dialogue's records: 0xA60..0xA74, before the grab's shock sounds");
}

void TestMessenger() {
	std::printf("the messenger and the papers\n");
	using walkers::DungeonMessenger;
	using walkers::MessengerDoes;
	using walkers::MessengerPhase;
	DungeonMessenger m;
	Check(MessengerSends(m, 2, 24.0f, 10.0f, true) && MessengerSends(m, 3, 24.0f, 10.0f, true),
	      "a siege, a place to clear: someone runs for help");
	Check(!MessengerSends(m, 1, 24.0f, 10.0f, true) && !MessengerSends(m, 4, 24.0f, 10.0f, true) &&
	          !MessengerSends(m, 0, 24.0f, 10.0f, true),
	      "a quiet refuge, a fallen place, no news: nobody runs");
	Check(!MessengerSends(m, 2, 0.0f, 10.0f, true), "MessengerHours 0: never");
	Check(!MessengerSends(m, 2, 24.0f, 10.0f, false), "the player not in the open land: not now");
	Check(walkers::MessengerLand(false, 0x0000003C), "the open land: Tamriel's own worldspace, out of doors");
	Check(!walkers::MessengerLand(true, 0) && !walkers::MessengerLand(true, 0x0000003C), "indoors: none");
	Check(!walkers::MessengerLand(false, 0x0001C31A) && !walkers::MessengerLand(false, 0x00009F18) &&
	          !walkers::MessengerLand(false, 0),
	      "a city's streets (a worldspace of its own - Nadi's Windows log, 2026-11-13: sent in Anvil, he never came "
	      "up), another land, none known: none");
	walkers::MessengerSent(m, 10.0f);
	Check(m.phase == MessengerPhase::Coming && m.lastSentDay == 10.0f && m.seconds == 0.0f, "sent: coming");
	Check(!MessengerSends(m, 2, 24.0f, 12.0f, true), "one on his way: no second");
	Check(!walkers::MessengerGreetsNow(m), "the second he is put out: not yet spoken to");
	Check(walkers::StepMessenger(m, false, 500.0f) == MessengerDoes::Nothing && m.phase == MessengerPhase::Coming,
	      "coming, nothing told yet: on");
	Check(walkers::MessengerGreetsNow(m), "a second on: told to speak to the player");
	{
		DungeonMessenger again = m;
		UInt32 greets = 0;
		for (UInt32 k = 0; k < 25; ++k) {
			walkers::StepMessenger(again, false, 500.0f);
			greets += walkers::MessengerGreetsNow(again) ? 1u : 0u;
		}
		Check(greets == 2, "and every ten seconds again (twice in the next 25)");
	}
	Check(walkers::StepMessenger(m, true, 100.0f) == MessengerDoes::Leave && m.phase == MessengerPhase::Leaving &&
	          m.seconds == 0.0f,
	      "he told his news: he leaves");
	Check(walkers::StepMessenger(m, false, 300.0f) == MessengerDoes::Nothing, "leaving, the player by him: he stays in sight");
	Check(walkers::StepMessenger(m, false, walkers::kMessengerGoneUnits + 1.0f) == MessengerDoes::Go &&
	          m.phase == MessengerPhase::Away,
	      "the player has walked on: taken away");
	Check(!MessengerSends(m, 2, 24.0f, 10.9f, true) && MessengerSends(m, 2, 24.0f, 11.0f, true),
	      "the next one MessengerHours later at the soonest");
	DungeonMessenger late;
	walkers::MessengerSent(late, 3.0f);
	MessengerDoes did = MessengerDoes::Nothing;
	for (UInt32 k = 0; k < walkers::kMessengerComingSeconds; ++k) {
		did = walkers::StepMessenger(late, false, 400.0f);
	}
	Check(did == MessengerDoes::Leave && late.phase == MessengerPhase::Leaving,
	      "a minute without reaching the player: he gives up and leaves");
	for (UInt32 k = 0; k < walkers::kMessengerLeavingSeconds; ++k) {
		did = walkers::StepMessenger(late, false, 300.0f);
	}
	Check(did == MessengerDoes::Go && late.phase == MessengerPhase::Away, "ninety seconds leaving: taken away at last");
	DungeonMessenger away;
	Check(walkers::StepMessenger(away, true, 0.0f) == MessengerDoes::Nothing, "none out: nothing");
	using walkers::MessengerFit;
	using walkers::RunnerFit;
	Check(MessengerFit(false, false) == RunnerFit::Fit, "the runner alive: he goes");
	Check(MessengerFit(true, false) == RunnerFit::Raise, "the runner dead: raised and dressed again, then he goes");
	Check(MessengerFit(false, true) == RunnerFit::Lost && MessengerFit(true, true) == RunnerFit::Lost,
	      "the runner risen as a walker himself (the rise without a creature): nobody comes");
	const UInt32 papers[3] = {0x25000A75, 0x25000A76, 0x25000A84};
	Check(walkers::PaperOpened(0x25000A75, 0, papers, 3) && walkers::PaperOpened(0x25000A76, 0x0006D6F3, papers, 3) &&
	          walkers::PaperOpened(0x25000A84, 0, papers, 3),
	      "the dungeons' edition, the notice at a gate or the one on a board opened: read");
	Check(!walkers::PaperOpened(0x25000A75, 0x25000A75, papers, 3), "still open: not read again");
	Check(!walkers::PaperOpened(0x0006D6F3, 0, papers, 3) && !walkers::PaperOpened(0, 0x25000A75, papers, 3) &&
	          !walkers::PaperOpened(0, 0, papers, 0) && !walkers::PaperOpened(0x25000A75, 0, nullptr, 3),
	      "another book, the book closed, no book and no papers: nothing");
	Check(walkers::IsPaper(0x25000A84, papers, 3) && !walkers::IsPaper(0, papers, 3) && !walkers::IsPaper(7, papers, 3),
	      "a paper of ours is one of the three; no book is none");
	Check(walkers::kDungeonEditionLocalId == walkers::kDungeonTalkEnd && walkers::kDungeonNoticeLocalId == 0xA76 &&
	          walkers::kDungeonMessengerLineFirstLocalId + walkers::kDungeonMessengerLineCount <= 0xAA4,
	      "the edition right after the rumors, the notice, the messenger - before the grab's shock sounds");
	Check(walkers::kDungeonNoticeRefFirstLocalId == 0xA7C && walkers::kDungeonBoardNoticeLocalId == 0xA84 &&
	          walkers::kDungeonBoardNoticeRefFirstLocalId == 0xA85 &&
	          walkers::kDungeonBoardNoticeRefFirstLocalId + walkers::kDungeonNoticeRefCount <= 0xAA4,
	      "the gates' eight notices 0xA7C..0xA83, the boards' book 0xA84 and its eight 0xA85..0xA8C, before the "
	      "shock sounds too");
}

void TestBeds() {
	std::printf("the beds of a refuge\n");
	walkers::DungeonCandidate c{};
	c.spotCount = 13;
	Check(walkers::BedSpotIndex(c, 0, 3) == 11 && walkers::BedSpotIndex(c, 1, 3) == 10 && walkers::BedSpotIndex(c, 2, 3) == 9,
	      "three beds on thirteen spots: from the far end, the last spot left to the trader");
	Check(walkers::BedSpotIndex(c, 3, 3) == -1 && walkers::BedSpotIndex(c, 3, 4) == 8 &&
	          walkers::BedSpotIndex(c, 4, 9) == -1,
	      "no more beds than Beds says, nor than the ESP holds (four)");
	c.spotCount = 5;
	Check(walkers::BedSpotIndex(c, 0, 4) == 3 && walkers::BedSpotIndex(c, 1, 4) == 2 && walkers::BedSpotIndex(c, 2, 4) == -1,
	      "few spots: the two by the door stay free of beds");
	c.spotCount = 2;
	Check(walkers::BedSpotIndex(c, 0, 4) == -1, "two spots or fewer: no bed");
	Check(walkers::BedsCandidate(7, false, 3) == 7, "the kit inside a place: its beds there");
	Check(walkers::BedsLieNow(true, false), "the player inside the place, nothing loading: a bed is laid");
	Check(!walkers::BedsLieNow(false, false),
	      "the player still outside, the kit gone in ahead of him: not yet (laid into the place before it was loaded "
	      "the beds never got their 3D - harness 2026-11-14)");
	Check(!walkers::BedsLieNow(true, true) && !walkers::BedsLieNow(false, true), "behind the loading screen: not yet");
	Check(walkers::BedsCandidate(7, true, 3) == -1, "the kit waiting outside (a place to clear): no beds");
	Check(walkers::BedsCandidate(-1, false, 3) == -1, "no kit out: no beds");
	Check(walkers::BedsCandidate(7, false, 0) == -1, "Beds=0: none");
	Check(walkers::kDungeonBedFirstLocalId == 0xA8F && walkers::kDungeonBedFirstLocalId + walkers::kDungeonBeds <= 0xAA4 &&
	          walkers::kDungeonRestTopicLocalId == 0xA8D && walkers::kDungeonRestLineLocalId == 0xA8E,
	      "the rest topic 0xA8D, its line 0xA8E, the four beds 0xA8F..0xA92 - before the shock sounds");
}

void TestImage() {
	std::printf("the journal in the co-save\n");
	DungeonSitesState st;
	st.count = 2;
	st.target = 2;
	st.sites[0].candidate = 17;
	st.sites[0].state = DungeonState::Refuge;
	st.sites[0].journal = kJournalDefended;
	st.sites[0].walkersLeft = 0;
	st.sites[1].candidate = 3;
	st.sites[1].state = DungeonState::Besieged;
	st.sites[1].journal = kJournalBesieged;
	st.sites[1].walkersLeft = 300;
	UInt8 image[walkers::kDungeonImageBytes];
	const UInt32 n = walkers::DungeonImage(st, image, sizeof(image));
	DungeonSitesState back;
	Check(walkers::ReadDungeonImage(back, image, n, 100) && back.sites[0].journal == kJournalDefended &&
	          back.sites[1].journal == kJournalBesieged && back.sites[1].walkersLeft == 300,
	      "each site's stage kept beside its walkers");
	image[8 + 10] = 0;
	Check(walkers::ReadDungeonImage(back, image, n, 100) && back.sites[0].journal == kJournalNone,
	      "a save of before the journal (the byte 0): nothing told yet");
}

}  // namespace

int main() {
	TestStages();
	TestNear();
	TestEvents();
	TestHeard();
	TestNews();
	TestTelling();
	TestTalk();
	TestMessenger();
	TestBeds();
	TestImage();
	std::printf(g_failures == 0 ? "All passed\n" : "%d FAILED\n", g_failures);
	return g_failures == 0 ? 0 : 1;
}
