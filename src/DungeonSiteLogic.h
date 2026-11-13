#pragma once

// The held dungeons' situation, process-free (docs/walkers-spec.md 4.20.7,
// Nadi's vision of 2026-11-11; step 2). From the pool (DungeonLogic.h)
// ActiveMin..ActiveMax places are active at once, DungeonSpacingUnits apart,
// each in a variant rolled by its weight:
//   Refuge         held and quiet - attacked by AttackChancePercent a game day
//   Besieged       held, walkers storm it - fought out by the game with the
//                  player near, else counted off hour by hour (the camps'
//                  toll, CampLogic.h StepCamp): the walkers all ended with
//                  someone alive - held (defended when the player saw it),
//                  a refuge again; everyone dead - overrun
//   ToClear        bandits or creatures inside, the refugees wait outside;
//                  cleared by the player, they move in - a refuge
//   WalkerDungeon  overrun - the dead risen; cleared, taken again - a refuge
// A place the player has not come near for LeaveAfterDays is given up (a
// siege is fought out first) and another rolled: the situation changes
// over time, the places stay those of the pool.

#include <cmath>
#include <cstring>

#include "Bytes.h"

#include "DungeonLogic.h"
#include "Types.h"

namespace walkers {

inline constexpr UInt32 kDungeonActiveMost = 8;
inline constexpr UInt16 kDungeonNoCandidate = 0xFFFF;
inline constexpr UInt32 kDungeonTollHoursMost = 24 * 7;

struct DungeonSiteSettings {
	bool enabled = true;
	UInt32 activeMin = 3;                // ActiveMin
	UInt32 activeMax = 5;                // ActiveMax
	UInt32 weightRefuge = 25;            // WeightRefuge
	UInt32 weightBesieged = 25;          // WeightBesieged
	UInt32 weightToClear = 25;           // WeightToClear
	UInt32 weightWalkerDungeon = 25;     // WeightWalkerDungeon
	float spacingUnits = 15000.0f;       // SpacingUnits: the active places' doors at least this far apart
	UInt32 peopleMin = 4;                // PeopleMin
	UInt32 peopleMax = 8;                // PeopleMax
	UInt32 attackChancePercent = 15;     // AttackChancePercent: a refuge and game day
	UInt32 walkersMin = 8;               // AttackWalkersMin
	UInt32 walkersMax = 16;              // AttackWalkersMax
	float refugeeKillsPerHour = 2.0f;    // RefugeeKillsPerHour: a siege away
	UInt32 lossPercentPerHour = 25;      // LossPercentPerHour: a siege away, one lost an hour by this
	float leaveAfterDays = 7.0f;         // LeaveAfterDays: unseen this long - given up; 0 never
	UInt32 walkersAtOnce = 6;            // WalkersAtOnce: of a siege's or a walker dungeon's, in the place at a time
	bool journal = true;                 // Journal: each place's quest in the journal (DungeonQuestLogic.h)
	bool rumors = true;                  // Rumors: people tell of the places (it needs the journal)
	float messengerHours = 24.0f;        // MessengerHours: a runner for help this far apart at the soonest; 0 never
};

// The ESP's pool of walkers for the place the player is at (DungeonKitLogic.h).
inline constexpr UInt32 kDungeonPool = 12;

enum class DungeonState : UInt8 { Free, Refuge, Besieged, ToClear, WalkerDungeon };

struct DungeonSite {
	UInt16 candidate = kDungeonNoCandidate;  // DungeonCandidates.h's index
	DungeonState state = DungeonState::Free;
	UInt8 people = 0;        // alive inside
	UInt8 waiting = 0;       // ToClear: waiting outside
	UInt8 lost = 0;          // lost in this siege
	UInt16 walkersLeft = 0;  // Besieged: storming it; WalkerDungeon: in it
	float dayFrom = 0.0f;    // Refuge: the attack rolled for every whole day since
	float lastSeen = 0.0f;   // the player last near
	float tollFrom = -1.0f;  // Besieged away: the toll counted since
	float carry = 0.0f;      // the part of a kill short of one
	bool seen = false;       // the player near during this siege
	UInt8 cleared = 0;       // ToClear: its cells the player found empty of the living, a bit each
	UInt8 journal = 0;       // the stage of its quest the player was told last (DungeonQuestLogic.h), 0 none
};

struct DungeonSitesState {
	DungeonSite sites[kDungeonActiveMost];
	UInt32 count = 0;
	UInt32 target = 0;  // the active count rolled, 0 not yet
};

enum class DungeonEventKind : UInt8 { Rolled, Attacked, PersonLost, Held, Defended, Overrun, Taken, Left };

struct DungeonEvent {
	DungeonEventKind kind;
	UInt32 site;
};

namespace dungeon_detail {

inline UInt32 Put(DungeonEvent* out, UInt32 n, UInt32 most, DungeonEventKind kind, UInt32 site) {
	if (out != nullptr && n < most) {
		out[n] = {kind, site};
	}
	return n + 1;
}

inline UInt32 Between(UInt32 least, UInt32 most, UInt32 roll) {
	return most <= least ? least : least + roll % (most - least + 1);
}

inline UInt32 Overrun(DungeonSite& d, UInt32 i, DungeonEvent* out, UInt32 n, UInt32 most) {
	d.walkersLeft = static_cast<UInt16>(d.walkersLeft + d.lost);  // the lost rise there
	d.people = 0;
	d.lost = 0;
	d.state = DungeonState::WalkerDungeon;
	return Put(out, n, most, DungeonEventKind::Overrun, i);
}

inline UInt32 Held(DungeonSite& d, UInt32 i, float now, DungeonEvent* out, UInt32 n, UInt32 most) {
	const bool defended = d.seen;
	d.walkersLeft = 0;
	d.lost = 0;
	d.seen = false;
	d.tollFrom = -1.0f;
	d.dayFrom = now;
	d.state = DungeonState::Refuge;
	return Put(out, n, most, defended ? DungeonEventKind::Defended : DungeonEventKind::Held, i);
}

}  // namespace dungeon_detail

inline DungeonState RollVariant(const DungeonSiteSettings& s, UInt32 roll) {
	const UInt32 w[4] = {s.weightRefuge, s.weightBesieged, s.weightToClear, s.weightWalkerDungeon};
	const UInt32 total = w[0] + w[1] + w[2] + w[3];
	if (total == 0) {
		return DungeonState::Refuge;
	}
	UInt32 r = roll % total;
	const DungeonState states[4] = {DungeonState::Refuge, DungeonState::Besieged, DungeonState::ToClear,
	                                DungeonState::WalkerDungeon};
	for (UInt32 k = 0; k < 4; ++k) {
		if (r < w[k]) {
			return states[k];
		}
		r -= w[k];
	}
	return DungeonState::Refuge;
}

inline UInt32 ActiveTarget(const DungeonSiteSettings& s, UInt32 roll) {
	const UInt32 n = dungeon_detail::Between(s.activeMin, s.activeMax, roll);
	return n > kDungeonActiveMost ? kDungeonActiveMost : n;
}

// Rolls new places until the target count is active: each from the pool,
// not active already, SpacingUnits from every active one; its variant, its
// people and walkers rolled. Returns the events (Rolled).
template <typename RollFn>
UInt32 FillDungeons(DungeonSitesState& st, const DungeonSiteSettings& s, const DungeonCandidate* all, UInt32 count,
                    const bool* inPool, float now, RollFn& roll, DungeonEvent* out, UInt32 most) {
	if (!s.enabled) {
		return 0;
	}
	if (st.target == 0) {
		st.target = ActiveTarget(s, roll());
	}
	UInt32 n = 0;
	while (st.count < st.target) {
		UInt32 open = 0;
		UInt32 pick = kDungeonNoCandidate;
		for (UInt32 pass = 0; pass < 2; ++pass) {
			const UInt32 chosen = pass == 1 && open > 0 ? roll() % open : 0;
			UInt32 seen = 0;
			for (UInt32 c = 0; c < count && c < kDungeonNoCandidate; ++c) {
				bool free = inPool[c];
				for (UInt32 k = 0; free && k < st.count; ++k) {
					const DungeonCandidate& a = all[st.sites[k].candidate];
					free = st.sites[k].candidate != c &&
					       std::hypot(a.x - all[c].x, a.y - all[c].y) >= s.spacingUnits;
				}
				if (!free) {
					continue;
				}
				if (pass == 1 && seen == chosen) {
					pick = c;
					break;
				}
				++seen;
			}
			if (pass == 0) {
				open = seen;
				if (open == 0) {
					break;
				}
			}
		}
		if (pick == kDungeonNoCandidate) {
			break;
		}
		DungeonSite d;
		d.candidate = static_cast<UInt16>(pick);
		d.state = RollVariant(s, roll());
		const UInt32 people = dungeon_detail::Between(s.peopleMin, s.peopleMax, roll());
		const UInt32 walkers = dungeon_detail::Between(s.walkersMin, s.walkersMax, roll());
		d.people = static_cast<UInt8>(d.state == DungeonState::Refuge || d.state == DungeonState::Besieged ? people : 0);
		d.waiting = static_cast<UInt8>(d.state == DungeonState::ToClear ? people : 0);
		d.walkersLeft = static_cast<UInt16>(
		    d.state == DungeonState::Besieged || d.state == DungeonState::WalkerDungeon ? walkers : 0);
		d.dayFrom = now;
		d.lastSeen = now;
		d.tollFrom = d.state == DungeonState::Besieged ? now : -1.0f;
		st.sites[st.count] = d;
		n = dungeon_detail::Put(out, n, most, DungeonEventKind::Rolled, st.count);
		++st.count;
	}
	return n;
}

// Once a second for site `i` at game day `now`, the player `near` it or not.
template <typename RollFn>
UInt32 StepDungeonSite(DungeonSite& d, UInt32 i, const DungeonSiteSettings& s, float now, bool playerClose, RollFn& roll,
                       DungeonEvent* out, UInt32 most) {
	if (d.state == DungeonState::Free || now < 0.0f) {
		return 0;
	}
	UInt32 n = 0;
	if (playerClose) {
		d.lastSeen = now;
	}
	if (d.state == DungeonState::Refuge) {
		const float days = std::floor(now - d.dayFrom);
		if (days >= 1.0f) {
			d.dayFrom += days;
			for (UInt32 k = 0; k < static_cast<UInt32>(days) && k < 30; ++k) {
				if (roll() % 100 < s.attackChancePercent) {
					d.state = DungeonState::Besieged;
					d.walkersLeft = static_cast<UInt16>(dungeon_detail::Between(s.walkersMin, s.walkersMax, roll()));
					d.tollFrom = now;
					d.seen = playerClose;
					d.carry = 0.0f;
					d.lost = 0;
					n = dungeon_detail::Put(out, n, most, DungeonEventKind::Attacked, i);
					break;
				}
			}
		}
	}
	if (d.state == DungeonState::Besieged) {
		if (playerClose || d.tollFrom < 0.0f) {
			d.seen = d.seen || playerClose;
			d.tollFrom = now;
			return n;
		}
		if (now < d.tollFrom) {
			// The clock set back (the console's "set gamehour", a script):
			// no toll for hours that never were - counted from now. Left
			// alone the count came out negative, as an unsigned one some
			// four billion hours (measured 2026-11-13: the game stood still).
			d.tollFrom = now;
			return n;
		}
		float hours = std::floor((now - d.tollFrom) * 24.0f);
		if (!(hours >= 1.0f)) {
			return n;
		}
		if (hours > static_cast<float>(kDungeonTollHoursMost)) {
			hours = static_cast<float>(kDungeonTollHoursMost);
		}
		const UInt32 whole = static_cast<UInt32>(hours);
		for (UInt32 h = 0; h < whole; ++h) {
			d.tollFrom += 1.0f / 24.0f;
			if (d.people > 0 && roll() % 100 < s.lossPercentPerHour) {
				--d.people;
				++d.lost;
				n = dungeon_detail::Put(out, n, most, DungeonEventKind::PersonLost, i);
			}
			if (d.people == 0) {
				return dungeon_detail::Overrun(d, i, out, n, most);
			}
			d.carry += s.refugeeKillsPerHour > 0.0f ? s.refugeeKillsPerHour : 0.0f;
			const UInt32 kills = static_cast<UInt32>(d.carry);
			d.carry -= static_cast<float>(kills);
			d.walkersLeft = static_cast<UInt16>(kills >= d.walkersLeft ? 0 : d.walkersLeft - kills);
			if (d.walkersLeft == 0) {
				return dungeon_detail::Held(d, i, now, out, n, most);
			}
		}
		return n;
	}
	if (s.leaveAfterDays > 0.0f && !playerClose && now - d.lastSeen >= s.leaveAfterDays) {
		d = DungeonSite();
		n = dungeon_detail::Put(out, n, most, DungeonEventKind::Left, i);
	}
	return n;
}

// One of site `i`'s people found dead in the game; the last overruns it
// (the rise raises them - the siege's walkers left stay).
inline UInt32 DungeonPersonDied(DungeonSite& d, UInt32 i, DungeonEvent* out, UInt32 most) {
	if (d.people == 0 || d.state == DungeonState::Free || d.state == DungeonState::WalkerDungeon) {
		return 0;
	}
	--d.people;
	return d.people == 0 ? dungeon_detail::Overrun(d, i, out, 0, most) : 0;
}

// Site `i` cleared by the player: to clear - those waiting move in; a
// walker dungeon - taken again, new people come. A refuge again.
template <typename RollFn>
UInt32 DungeonCleared(DungeonSite& d, UInt32 i, const DungeonSiteSettings& s, RollFn& roll, DungeonEvent* out,
                      UInt32 most) {
	if (d.state != DungeonState::ToClear && d.state != DungeonState::WalkerDungeon) {
		return 0;
	}
	const UInt32 people = d.state == DungeonState::ToClear && d.waiting > 0
	                          ? d.waiting
	                          : dungeon_detail::Between(s.peopleMin, s.peopleMax, roll());
	d.people = static_cast<UInt8>(people);
	d.waiting = 0;
	d.walkersLeft = 0;
	d.lost = 0;
	d.cleared = 0;
	d.dayFrom = d.lastSeen;
	d.state = DungeonState::Refuge;
	return dungeon_detail::Put(out, 0, most, DungeonEventKind::Taken, i);
}

// A walker of site `i`'s ended (4.17's end): one off the siege - the last,
// someone alive, holds it (defended when the player was near) - or one off
// a walker dungeon's: the last, and the place is cleared and taken.
template <typename RollFn>
UInt32 DungeonWalkerEnded(DungeonSite& d, UInt32 i, const DungeonSiteSettings& s, RollFn& roll, DungeonEvent* out,
                          UInt32 most) {
	if (d.walkersLeft == 0) {
		return 0;
	}
	if (d.state == DungeonState::Besieged) {
		--d.walkersLeft;
		return d.walkersLeft == 0 && d.people > 0 ? dungeon_detail::Held(d, i, d.lastSeen, out, 0, most) : 0;
	}
	if (d.state == DungeonState::WalkerDungeon) {
		--d.walkersLeft;
		return d.walkersLeft == 0 ? DungeonCleared(d, i, s, roll, out, most) : 0;
	}
	return 0;
}

// The pool walkers a site wants in the place now: of a siege's or a walker
// dungeon's, WalkersAtOnce at a time, never more than the pool.
inline UInt32 DungeonWalkersOut(const DungeonSite& d, UInt32 atOnce) {
	if (d.state != DungeonState::Besieged && d.state != DungeonState::WalkerDungeon) {
		return 0;
	}
	UInt32 n = d.walkersLeft;
	n = n > atOnce ? atOnce : n;
	return n > kDungeonPool ? kDungeonPool : n;
}

// A place to clear: its cell `index` (of `cellCount`) found empty of living
// occupants with the player in it. True once every cell was: cleared.
inline bool NoteCellClear(DungeonSite& d, UInt32 index, UInt32 cellCount) {
	if (d.state != DungeonState::ToClear || cellCount == 0 || cellCount > 8 || index >= cellCount) {
		return false;
	}
	d.cleared = static_cast<UInt8>(d.cleared | (1u << index));
	return d.cleared == static_cast<UInt8>((1u << cellCount) - 1u);
}

// One of those waiting before a place to clear died.
inline void DungeonWaitingDied(DungeonSite& d) {
	if (d.state == DungeonState::ToClear && d.waiting > 0) {
		--d.waiting;
	}
}

// The sites given up taken off the list, the rest in their order.
inline void DropFreeSites(DungeonSitesState& st) {
	UInt32 kept = 0;
	for (UInt32 k = 0; k < st.count; ++k) {
		if (st.sites[k].state != DungeonState::Free) {
			st.sites[kept++] = st.sites[k];
		}
	}
	st.count = kept;
}

// The co-save record: type "DNGN", version 1 - the target and the count,
// then per site of kDungeonActiveMost: the candidate (16 bits), the state,
// seen, people, waiting, lost, the cells cleared, the walkers left (16 bits),
// the journal's stage told last (a byte, 0 in a save of before it; a byte
// unused), and dayFrom, lastSeen, tollFrom, carry as their bits.
inline constexpr UInt32 kDungeonRecordType = 0x444E474E;  // 'DNGN'
inline constexpr UInt32 kDungeonRecordVersion = 1;
inline constexpr UInt32 kDungeonSiteBytes = 28;
inline constexpr UInt32 kDungeonImageBytes = 8 + kDungeonActiveMost * kDungeonSiteBytes;

namespace dungeon_detail {

inline UInt32 Bits(float f) {
	UInt32 b = 0;
	std::memcpy(&b, &f, sizeof b);
	return b;
}

inline float Float(UInt32 b) {
	float f = 0.0f;
	std::memcpy(&f, &b, sizeof f);
	return f;
}

}  // namespace dungeon_detail

inline UInt32 DungeonImage(const DungeonSitesState& st, UInt8* out, UInt32 capacity) {
	if (capacity < kDungeonImageBytes) {
		return 0;
	}
	std::memset(out, 0, kDungeonImageBytes);
	Put32(out, st.target);
	Put32(out + 4, st.count);
	for (UInt32 i = 0; i < kDungeonActiveMost; ++i) {
		const DungeonSite& d = st.sites[i];
		UInt8* at = out + 8 + i * kDungeonSiteBytes;
		at[0] = static_cast<UInt8>(d.candidate & 0xFF);
		at[1] = static_cast<UInt8>(d.candidate >> 8);
		at[2] = static_cast<UInt8>(d.state);
		at[3] = d.seen ? 1 : 0;
		at[4] = d.people;
		at[5] = d.waiting;
		at[6] = d.lost;
		at[7] = d.cleared;
		Put32(at + 8, d.walkersLeft);
		at[10] = d.journal;
		Put32(at + 12, dungeon_detail::Bits(d.dayFrom));
		Put32(at + 16, dungeon_detail::Bits(d.lastSeen));
		Put32(at + 20, dungeon_detail::Bits(d.tollFrom));
		Put32(at + 24, dungeon_detail::Bits(d.carry));
	}
	return kDungeonImageBytes;
}

// `st` emptied first; false when too short (rolled anew then). A site whose
// candidate is not below `candidates` (another build's list) is dropped.
inline bool ReadDungeonImage(DungeonSitesState& st, const UInt8* in, UInt32 length, UInt32 candidates) {
	st = DungeonSitesState{};
	if (length < kDungeonImageBytes) {
		return false;
	}
	st.target = Get32(in);
	if (st.target > kDungeonActiveMost) {
		st.target = kDungeonActiveMost;
	}
	UInt32 count = Get32(in + 4);
	if (count > kDungeonActiveMost) {
		count = kDungeonActiveMost;
	}
	for (UInt32 i = 0; i < count; ++i) {
		const UInt8* at = in + 8 + i * kDungeonSiteBytes;
		DungeonSite d;
		d.candidate = static_cast<UInt16>(at[0] | (at[1] << 8));
		d.state = at[2] <= static_cast<UInt8>(DungeonState::WalkerDungeon) ? static_cast<DungeonState>(at[2])
		                                                                    : DungeonState::Free;
		d.seen = at[3] != 0;
		d.people = at[4];
		d.waiting = at[5];
		d.lost = at[6];
		d.cleared = at[7];
		d.walkersLeft = static_cast<UInt16>(Get32(at + 8));
		d.journal = at[10];
		d.dayFrom = dungeon_detail::Float(Get32(at + 12));
		d.lastSeen = dungeon_detail::Float(Get32(at + 16));
		d.tollFrom = dungeon_detail::Float(Get32(at + 20));
		d.carry = dungeon_detail::Float(Get32(at + 24));
		if (d.candidate < candidates && d.state != DungeonState::Free) {
			st.sites[st.count++] = d;
		}
	}
	return true;
}

}  // namespace walkers
