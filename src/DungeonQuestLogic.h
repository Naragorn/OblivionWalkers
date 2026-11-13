#pragma once

// The held dungeons' journal, process-free (docs/walkers-spec.md 4.20.7,
// step 4). Every candidate has a quest of its own in the ESP
// (WalkersDungeonNNN, its name and log entries naming the place, its map
// marker the target); a site remembers the stage the player was told last:
//    5  heard of - people hold the place                 (something to do)
//   10  a refuge, quiet                                  (completes)
//   20  besieged - help them                             (something to do)
//   30  to clear - they wait outside                     (something to do)
//   40  a walker dungeon - the place fallen              (something to do)
//   50  defended                                         (completes, rewards)
//   55  the siege he knew of beaten off without him      (completes)
//   60  cleared and taken                                (completes, rewards)
//   90  the refugees have left, the quest still open     (completes)
// The quests allow repeated stages (a place is besieged more than once);
// one completed before is taken off "completed" (xOBSE's UncompleteQuest)
// when there is something to do again.
// He is told near the place (in it, or by its door), by an event (defended,
// taken, given up), or by hearing of it - a place whose state he does not
// know is news.

#include <cstdio>

#include "DungeonSiteLogic.h"

namespace walkers {

inline constexpr UInt8 kJournalNone = 0;
inline constexpr UInt8 kJournalHeard = 5;
inline constexpr UInt8 kJournalRefuge = 10;
inline constexpr UInt8 kJournalBesieged = 20;
inline constexpr UInt8 kJournalToClear = 30;
inline constexpr UInt8 kJournalWalkerDungeon = 40;
inline constexpr UInt8 kJournalDefended = 50;
inline constexpr UInt8 kJournalHeldAlone = 55;
inline constexpr UInt8 kJournalTaken = 60;
inline constexpr UInt8 kJournalLeft = 90;

// The quests' local IDs: down from the last a light plugin may hold, so a
// longer list of candidates moves no quest (tools/esp-write.py).
inline constexpr UInt32 kDungeonQuestLastLocalId = 0xFFF;
inline constexpr UInt32 DungeonQuestLocalId(UInt32 candidate) { return kDungeonQuestLastLocalId - candidate; }

// The dialogue's and the news' records, after the held dungeons' pool
// (DungeonKitLogic.h, ending at 0xA5F) and before the grab's shock sounds
// (0xAA4): the refugees' dialogue quest, three globals - how the place the
// kit stands at is (DungeonTalkState), the news' kind (DungeonNewsKind), and
// "heard" (set 1 by a line that tells the news; the plugin takes it back) -,
// the two topics, their eleven lines (four greetings, three "What
// happened?", four "How can I help?") and the four rumors.
inline constexpr UInt32 kDungeonTalkQuestLocalId = 0xA60;
inline constexpr UInt32 kDungeonTalkGlobalLocalId = 0xA61;
inline constexpr UInt32 kDungeonNewsGlobalLocalId = 0xA62;
inline constexpr UInt32 kDungeonHeardGlobalLocalId = 0xA63;
inline constexpr UInt32 kDungeonTopicWhatLocalId = 0xA64;
inline constexpr UInt32 kDungeonTopicHelpLocalId = 0xA65;
inline constexpr UInt32 kDungeonLineFirstLocalId = 0xA66;
inline constexpr UInt32 kDungeonLineCount = 11;
inline constexpr UInt32 kDungeonRumorFirstLocalId = kDungeonLineFirstLocalId + kDungeonLineCount;  // 0xA71
inline constexpr UInt32 kDungeonRumorCount = 4;
inline constexpr UInt32 kDungeonTalkEnd = kDungeonRumorFirstLocalId + kDungeonRumorCount;  // 0xA75

// What the kit's people say, by the place they stand at: 1 a quiet refuge,
// 2 besieged, 3 waiting before a place to clear, 4 a refuge the player just
// defended or cleared (their thanks); 0 nothing of ours.
inline UInt8 DungeonTalkState(DungeonState s, bool thanked) {
	switch (s) {
		case DungeonState::Refuge:
			return thanked ? 4 : 1;
		case DungeonState::Besieged:
			return 2;
		case DungeonState::ToClear:
			return 3;
		default:
			return 0;
	}
}

// The candidate the player is thanked at (he defended or cleared it), -1
// none: forgotten once the kit stands at another place, or that place is
// no refuge any more.
inline int ThankedStill(int thanked, int kitCandidate, DungeonState kitState) {
	if (thanked < 0 || (kitCandidate >= 0 && kitCandidate != thanked) ||
	    (kitCandidate == thanked && kitState != DungeonState::Refuge)) {
		return -1;
	}
	return thanked;
}

inline UInt8 JournalOfState(DungeonState s) {
	switch (s) {
		case DungeonState::Refuge:
			return kJournalRefuge;
		case DungeonState::Besieged:
			return kJournalBesieged;
		case DungeonState::ToClear:
			return kJournalToClear;
		case DungeonState::WalkerDungeon:
			return kJournalWalkerDungeon;
		default:
			return kJournalNone;
	}
}

inline bool JournalCompletes(UInt8 stage) {
	return stage == kJournalRefuge || stage == kJournalDefended || stage == kJournalHeldAlone ||
	       stage == kJournalTaken || stage == kJournalLeft;
}

// How the player knows the place to stand, by the stage told last; Free:
// he does not know.
inline DungeonState JournalKnows(UInt8 told) {
	switch (told) {
		case kJournalRefuge:
		case kJournalDefended:
		case kJournalHeldAlone:
		case kJournalTaken:
			return DungeonState::Refuge;
		case kJournalBesieged:
			return DungeonState::Besieged;
		case kJournalToClear:
			return DungeonState::ToClear;
		case kJournalWalkerDungeon:
			return DungeonState::WalkerDungeon;
		default:
			return DungeonState::Free;
	}
}

// The player near the place: the stage to tell now, kJournalNone when he
// knows it as it stands.
inline UInt8 JournalNear(UInt8 told, DungeonState now) {
	if (now == DungeonState::Free || JournalKnows(told) == now) {
		return kJournalNone;
	}
	if (now != DungeonState::Refuge) {
		return JournalOfState(now);
	}
	switch (JournalKnows(told)) {
		case DungeonState::Besieged:
			return kJournalHeldAlone;
		case DungeonState::ToClear:
		case DungeonState::WalkerDungeon:
			return kJournalTaken;
		default:
			return kJournalRefuge;
	}
}

// Near enough to learn how it stands: inside it, or - a place to clear,
// whose people wait before the door - within kDungeonMeetUnits of the door
// outside. `closeness` as Dungeons.cpp's: 0 inside, the distance from the
// door outside, below 0 away.
inline constexpr float kDungeonMeetUnits = 1500.0f;
inline bool JournalSees(DungeonState now, float closeness) {
	return closeness == 0.0f ||
	       (now == DungeonState::ToClear && closeness > 0.0f && closeness <= kDungeonMeetUnits);
}

// An event of the site: defended and taken are told as they happen (the
// reward's), a place given up closes a quest still open.
inline UInt8 JournalOnEvent(UInt8 told, DungeonEventKind kind) {
	switch (kind) {
		case DungeonEventKind::Defended:
			return kJournalDefended;
		case DungeonEventKind::Taken:
			return kJournalTaken;
		case DungeonEventKind::Left:
			return told != kJournalNone && !JournalCompletes(told) ? kJournalLeft : kJournalNone;
		default:
			return kJournalNone;
	}
}

// The player hears of the place (a rumor, the messenger, a notice): as near
// it, but a quiet refuge is only heard of - and that once.
inline UInt8 JournalHeard(UInt8 told, DungeonState now) {
	const UInt8 there = JournalNear(told, now);
	if (there != kJournalRefuge) {
		return there;
	}
	return told == kJournalNone ? kJournalHeard : kJournalNone;
}

// The site there is news of, -1 none: a siege first, then a place to clear,
// a fallen place, a refuge; the first of each.
inline int DungeonNewsSite(const DungeonSitesState& st) {
	static constexpr DungeonState order[4] = {DungeonState::Besieged, DungeonState::ToClear, DungeonState::WalkerDungeon,
	                                         DungeonState::Refuge};
	for (DungeonState wanted : order) {
		for (UInt32 i = 0; i < st.count && i < kDungeonActiveMost; ++i) {
			const DungeonSite& d = st.sites[i];
			if (d.state == wanted && d.candidate != kDungeonNoCandidate && JournalHeard(d.journal, d.state) != kJournalNone) {
				return static_cast<int>(i);
			}
		}
	}
	return -1;
}

// The news' kind for the rumors' global: 1 a refuge, 2 a siege, 3 a place
// to clear, 4 a fallen place; 0 none.
inline UInt8 DungeonNewsKind(const DungeonSitesState& st, int site) {
	if (site < 0 || static_cast<UInt32>(site) >= st.count || site >= static_cast<int>(kDungeonActiveMost)) {
		return 0;
	}
	return static_cast<UInt8>(st.sites[site].state);
}

// Before `stage` is set: whether the quest is taken off "completed" first -
// something to do again, and an earlier stage (this time's or, at a place's
// first word, an earlier time's) may have completed it.
inline bool JournalUncompletesFirst(UInt8 told, UInt8 stage) {
	return !JournalCompletes(stage) && (told == kJournalNone || JournalCompletes(told));
}

inline bool JournalRewards(UInt8 stage) { return stage == kJournalDefended || stage == kJournalTaken; }

// The place's map marker is shown at the first word of it.
inline bool JournalShowsMap(UInt8 told) { return told == kJournalNone; }

// One of the held dungeons' own quests by its editor ID (each place's, and
// the dialogue's): they name every place's map marker - their targets - and
// must not count as a quest that needs the place (DungeonLogic.h's A).
inline bool IsDungeonQuestEditorId(const char* editorId) {
	static constexpr char prefix[] = "WalkersDungeon";
	if (editorId == nullptr) {
		return false;
	}
	for (UInt32 k = 0; k + 1 < sizeof(prefix); ++k) {
		if (editorId[k] != prefix[k]) {
			return false;
		}
	}
	return true;
}

// "WalkersDungeonNNN"; false when it does not fit.
inline bool DungeonQuestEditorId(UInt32 candidate, char* out, UInt32 size) {
	if (candidate > 999 || size < 18) {
		return false;
	}
	std::snprintf(out, size, "WalkersDungeon%03u", candidate);
	return true;
}

}  // namespace walkers
