#pragma once

// The held dungeons' people and occupants, process-free (docs/walkers-spec.md
// 4.20.7, step 3b). One kit of people - the camps' bases, six men, six women
// and a trader, persistent references of the ESP's holding cell - stands at
// the held site (a refuge, a besieged place) the player is near; elsewhere
// the sites are numbers only (DungeonSiteLogic.h). A held place's own
// occupants - what the game placed or spawned in its cells - are taken out
// (disabled) when the player enters a cell of it, each noted with its cell,
// and put back when he enters that cell and the place is held no more.
// Costs (Nadi, 2026-11-13, "extrem performant"): nothing a frame but the
// player's cell compared with the last one; a cell is read through once,
// when he enters it; the kit is placed one person a frame.

#include <cstring>

#include "Bytes.h"
#include "DungeonSiteLogic.h"

namespace walkers {

// The ESP's kit (tools/esp-write.py reads these): slots 0..5 men, 6..11
// women, 12 the trader - CampLogic.h's bases by CampBaseIndex(0, slot).
inline constexpr UInt32 kDungeonKitFirstLocalId = 0xA47;
inline constexpr UInt32 kDungeonKitSlots = 13;
inline constexpr UInt32 kDungeonKitTraderSlot = 12;
inline constexpr UInt32 kDungeonKitPeopleMost = 12;
// The kit stays where it is while another held site is less than this
// nearer (two sites near at once: no going back and forth).
inline constexpr float kDungeonKitKeepUnits = 2000.0f;

// The ESP's pool of walkers for a siege or a walker dungeon (kDungeonPool of
// them, DungeonSiteLogic.h): right after the kit.
inline constexpr UInt32 kDungeonPoolFirstLocalId = kDungeonKitFirstLocalId + kDungeonKitSlots;
// Those coming in stand on the spots nearest the door, this many of them.
inline constexpr UInt32 kDungeonEntrySpots = 4;

// People live inside.
inline bool DungeonHeld(DungeonState s) { return s == DungeonState::Refuge || s == DungeonState::Besieged; }
// The game's own occupants are out: people or walkers have the place.
inline bool OccupantsOut(DungeonState s) { return DungeonHeld(s) || s == DungeonState::WalkerDungeon; }

// The site the kit should stand at, -1 none. `closeness` a site: -1 the
// player not near it, 0 inside it, else his distance from its door outside.
template <typename WantsFn>
int NearSite(const DungeonSitesState& st, const float* closeness, bool enabled, int current, WantsFn wants) {
	if (!enabled) {
		return -1;
	}
	int best = -1;
	for (UInt32 i = 0; i < st.count; ++i) {
		if (!wants(st.sites[i]) || closeness[i] < 0.0f) {
			continue;
		}
		if (best < 0 || closeness[i] < closeness[best]) {
			best = static_cast<int>(i);
		}
	}
	if (best >= 0 && current >= 0 && static_cast<UInt32>(current) < st.count && current != best &&
	    wants(st.sites[current]) && closeness[current] > 0.0f && closeness[best] > 0.0f &&
	    closeness[current] - closeness[best] < kDungeonKitKeepUnits) {
		return current;
	}
	return best;
}

// The kit waits outside, before the door.
inline bool KitOutside(DungeonState s) { return s == DungeonState::ToClear; }

inline int KitSite(const DungeonSitesState& st, const float* closeness, bool enabled, int current) {
	return NearSite(st, closeness, enabled, current, [](const DungeonSite& d) {
		return DungeonHeld(d.state) || (KitOutside(d.state) && d.waiting > 0);
	});
}

// The site the pool of walkers should be at, -1 none: the nearest whose
// siege or walker dungeon has walkers left, by the kit's rules.
inline int WalkerSite(const DungeonSitesState& st, const float* closeness, bool enabled, int current) {
	return NearSite(st, closeness, enabled, current,
	                [](const DungeonSite& d) { return DungeonWalkersOut(d, kDungeonPool) > 0; });
}

inline bool KitTrader(DungeonState s) { return s == DungeonState::Refuge; }

// The slots shown for `people` alive: a man, a woman, a man ... skipping
// the `dead` (those who died while the kit stood here); the trader with
// anyone alive, unless dead.
inline UInt16 KitWanted(UInt32 people, bool trader, UInt16 dead) {
	UInt16 mask = 0;
	UInt32 taken = 0;
	for (UInt32 k = 0; k < kDungeonKitPeopleMost && taken < people; ++k) {
		const UInt32 slot = k % 2 == 0 ? k / 2 : 6 + k / 2;
		const UInt16 bit = static_cast<UInt16>(1u << slot);
		if ((dead & bit) == 0) {
			mask = static_cast<UInt16>(mask | bit);
			++taken;
		}
	}
	const UInt16 traderBit = static_cast<UInt16>(1u << kDungeonKitTraderSlot);
	if (trader && taken > 0 && (dead & traderBit) == 0) {
		mask = static_cast<UInt16>(mask | traderBit);
	}
	return mask;
}

// Where the kit's `slot` stands, as an offset from the door inside: the
// candidate's spots in turn; more people than spots share them a step aside.
// `besieged`: from the far end - the spots by the door are the walkers'.
inline DungeonSpot KitSpot(const DungeonCandidate& c, UInt32 slot, bool besieged = false) {
	if (c.spotCount == 0) {
		return DungeonSpot{0, static_cast<short>(70 * (slot + 1)), 0};
	}
	const UInt32 at = slot % c.spotCount;
	DungeonSpot s = c.spots[besieged ? c.spotCount - 1 - at : at];
	if (slot >= c.spotCount) {
		s.dx = static_cast<short>(s.dx + (slot % 2 == 0 ? 50 : -50));
		s.dy = static_cast<short>(s.dy + ((slot / c.spotCount) % 2 == 0 ? -50 : 50));
	}
	return s;
}

// Where the kit's `slot` waits outside, an offset from the door outside.
inline DungeonSpot OutsideSpot(const DungeonCandidate& c, UInt32 slot) {
	if (c.outsideCount == 0) {
		return DungeonSpot{0, 0, 0};
	}
	DungeonSpot s = c.outside[slot % c.outsideCount];
	if (slot >= c.outsideCount) {
		s.dx = static_cast<short>(s.dx + (slot % 2 == 0 ? 50 : -50));
		s.dy = static_cast<short>(s.dy + ((slot / c.outsideCount) % 2 == 0 ? -50 : 50));
	}
	return s;
}

// Where the kit's `n`th stands when it comes in by the door: the people's
// spots from the door on, in turn.
inline DungeonSpot DoorSpot(const DungeonCandidate& c, UInt32 n) { return KitSpot(c, n); }

// The slots shown at a site: its people inside (the trader in a quiet
// refuge), or those waiting before a place to clear when it has spots
// outside; `dead` those of the kit that died there.
inline UInt16 KitWantedFor(const DungeonSite& d, const DungeonCandidate& c, UInt16 dead) {
	if (DungeonHeld(d.state)) {
		return KitWanted(d.people, KitTrader(d.state), dead);
	}
	if (KitOutside(d.state) && c.outsideCount > 0) {
		return KitWanted(d.waiting, false, dead);
	}
	return 0;
}

// Where the pool's walker `k` stands: `comingIn` (a siege's, and every one
// raised again) on the kDungeonEntrySpots nearest the door, a step aside
// when they share one; else (a walker dungeon found so) spread over all.
inline DungeonSpot WalkerSpot(const DungeonCandidate& c, UInt32 k, bool comingIn) {
	if (!comingIn || c.spotCount == 0) {
		return KitSpot(c, k);
	}
	const UInt32 entry = c.spotCount < kDungeonEntrySpots ? c.spotCount : kDungeonEntrySpots;
	DungeonSpot s = c.spots[k % entry];
	if (k >= entry) {
		s.dx = static_cast<short>(s.dx + (k % 2 == 0 ? 45 : -45));
		s.dy = static_cast<short>(s.dy + ((k / entry) % 2 == 0 ? -45 : 45));
	}
	return s;
}

// The pool's place has no walkers left (the siege held, the walker dungeon
// taken): those ended lie where they fell while the player is still near -
// the pool goes back in when he is gone, or when it is wanted elsewhere.
inline bool PoolStays(int wantSite, float closenessOfCurrent) { return wantSite < 0 && closenessOfCurrent >= 0.0f; }

// The kit put into a place while the player stands in it (he cleared it,
// they move in) comes in by the door, not onto its spots out of nowhere;
// right after a load it stands where it belongs. `closenessOfWant` 0: inside.
inline bool KitByTheDoor(float closenessOfWant, bool afterLoad) { return !afterLoad && closenessOfWant == 0.0f; }

// What Dungeons.cpp reads off a reference of a held place's cell.
struct OccupantFacts {
	bool actor = false;     // a Character or a Creature
	bool player = false;
	bool ours = false;      // one of the kit
	bool follower = false;  // on the player's follower list (a companion, a summon)
	bool dead = false;
	bool disabled = false;  // already - a quest's doing or ours
	bool risen = false;     // a walker the rise made (of one of its people)
};

inline bool TakesOut(const OccupantFacts& f) {
	return f.actor && !f.player && !f.ours && !f.follower && !f.dead && !f.disabled && !f.risen;
}

// Those taken out, each with the cell it stood in.
struct DungeonOccupant {
	UInt32 cell;
	UInt32 ref;
};
inline constexpr UInt32 kDungeonOccupantsMost = 512;

struct DungeonOccupants {
	DungeonOccupant list[kDungeonOccupantsMost];
	UInt32 count = 0;
};

inline bool AddOccupant(DungeonOccupants& o, UInt32 cell, UInt32 ref) {
	if (cell == 0 || ref == 0 || o.count >= kDungeonOccupantsMost) {
		return false;
	}
	for (UInt32 i = 0; i < o.count; ++i) {
		if (o.list[i].ref == ref) {
			return false;
		}
	}
	o.list[o.count++] = DungeonOccupant{cell, ref};
	return true;
}

inline UInt32 OccupantsOf(const DungeonOccupants& o, UInt32 cell) {
	UInt32 n = 0;
	for (UInt32 i = 0; i < o.count; ++i) {
		n += o.list[i].cell == cell ? 1u : 0u;
	}
	return n;
}

// The cell's taken off the list into `out` (at most `most`; the rest stay
// listed); how many.
inline UInt32 TakeOccupantsOf(DungeonOccupants& o, UInt32 cell, UInt32* out, UInt32 most) {
	UInt32 taken = 0;
	UInt32 kept = 0;
	for (UInt32 i = 0; i < o.count; ++i) {
		if (o.list[i].cell == cell && taken < most) {
			out[taken++] = o.list[i].ref;
		} else {
			o.list[kept++] = o.list[i];
		}
	}
	o.count = kept;
	return taken;
}

// The co-save record: type "DNGO", version 1 - the count, then cell and
// reference each (form IDs as saved; read back through xOBSE's ResolveRefID,
// one whose mod is gone dropped).
inline constexpr UInt32 kDungeonOccupantRecordType = 0x444E474F;  // 'DNGO'
inline constexpr UInt32 kDungeonOccupantRecordVersion = 1;
inline constexpr UInt32 kDungeonOccupantImageMost = 4 + kDungeonOccupantsMost * 8;

inline UInt32 OccupantImage(const DungeonOccupants& o, UInt8* out, UInt32 capacity) {
	const UInt32 length = 4 + o.count * 8;
	if (capacity < length) {
		return 0;
	}
	Put32(out, o.count);
	for (UInt32 i = 0; i < o.count; ++i) {
		Put32(out + 4 + i * 8, o.list[i].cell);
		Put32(out + 8 + i * 8, o.list[i].ref);
	}
	return length;
}

using ResolveFormFn = bool (*)(UInt32 saved, UInt32& loaded);

inline bool ReadOccupantImage(DungeonOccupants& o, const UInt8* in, UInt32 length, ResolveFormFn resolve) {
	o.count = 0;
	if (length < 4) {
		return false;
	}
	const UInt32 count = Get32(in);
	if (count > kDungeonOccupantsMost || length < 4 + count * 8) {
		return false;
	}
	for (UInt32 i = 0; i < count; ++i) {
		UInt32 cell = 0;
		UInt32 ref = 0;
		if (resolve(Get32(in + 4 + i * 8), cell) && resolve(Get32(in + 8 + i * 8), ref)) {
			AddOccupant(o, cell, ref);
		}
	}
	return true;
}

// The candidate a cell belongs to, -1 none.
inline int CandidateOfCell(const DungeonCandidate* all, UInt32 count, UInt32 cell) {
	for (UInt32 c = 0; cell != 0 && c < count; ++c) {
		for (UInt32 k = 0; k < all[c].cellCount && k < kDungeonCellsMost; ++k) {
			if (all[c].cells[k] == cell) {
				return static_cast<int>(c);
			}
		}
	}
	return -1;
}

inline int SiteOfCandidate(const DungeonSitesState& st, UInt32 candidate) {
	for (UInt32 i = 0; i < st.count; ++i) {
		if (st.sites[i].candidate == candidate) {
			return static_cast<int>(i);
		}
	}
	return -1;
}

// Whether `cell` is a held site's: its occupants are out.
inline bool CellHeld(const DungeonSitesState& st, const DungeonCandidate* all, UInt32 count, UInt32 cell,
                     bool enabled) {
	const int c = enabled ? CandidateOfCell(all, count, cell) : -1;
	const int site = c >= 0 ? SiteOfCandidate(st, static_cast<UInt32>(c)) : -1;
	return site >= 0 && OccupantsOut(st.sites[site].state);
}

// [Dungeons] TestSite and TestSiteState (INI only, test switches): with no
// site yet, the candidate named so becomes the first, in `state` - a refuge
// or a siege of PeopleMax, a siege's or a walker dungeon's AttackWalkersMax,
// a place to clear with PeopleMax waiting.
inline bool ForceSite(DungeonSitesState& st, const DungeonCandidate* all, UInt32 count, const bool* inPool,
                      const char* name, DungeonState state, const DungeonSiteSettings& s, float now) {
	if (st.count != 0 || name == nullptr || name[0] == '\0' || state == DungeonState::Free) {
		return false;
	}
	const UInt32 people = s.peopleMax > kDungeonKitPeopleMost ? kDungeonKitPeopleMost : s.peopleMax;
	for (UInt32 c = 0; c < count; ++c) {
		if (!inPool[c] || !SameName(all[c].name, name)) {
			continue;
		}
		DungeonSite d;
		d.candidate = static_cast<UInt16>(c);
		d.state = state;
		d.people = static_cast<UInt8>(DungeonHeld(state) ? people : 0);
		d.waiting = static_cast<UInt8>(state == DungeonState::ToClear ? people : 0);
		d.walkersLeft = static_cast<UInt16>(state == DungeonState::WalkerDungeon ? s.walkersMax : 0);
		if (state == DungeonState::Besieged) {
			dungeon_detail::Storm(d, s.walkersMax, s);
		}
		d.tollFrom = state == DungeonState::Besieged ? now : -1.0f;
		d.dayFrom = now;
		d.lastSeen = now;
		st.sites[0] = d;
		st.count = 1;
		return true;
	}
	return false;
}

}  // namespace walkers
