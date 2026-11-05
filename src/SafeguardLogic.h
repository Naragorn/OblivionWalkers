#pragma once

// The safeguard, process-free (docs/walkers-spec.md 4.11b; Nadi,
// 2026-10-26: "als safeguard merkt sich die plugin wer alles gekilled wurde
// von zombies und resurrected den dann irgendwo sicher (default an)"): the
// walkers must lose in the end, or a quest may break - so whoever they kill
// gets up again. Noted in the death path: anyone a walker killed, but the
// walkers themselves, the player, the gate guards (their pool raises its
// own, GuardLogic.h) and those the rise turns into walkers (the rise's
// generic people, whom no quest counts on: RiseSettings onlyGeneric).
// Raised once dead AfterSeconds with no living walker within Radius -
// where they fell, the place being safe then; their AI takes them home.
// The list goes into the co-save, a load keeps it.

#include "Bytes.h"
#include "MissingLogic.h"
#include "Types.h"

#include <cstring>

namespace walkers {

struct SafeguardSettings {
	bool enabled = true;          // [Safeguard] Enabled
	float afterSeconds = 15.0f;   // [Safeguard] AfterSeconds: dead at least this long
	float radius = 3000.0f;       // [Safeguard] Radius: no living walker this near
	// [Safeguard] RaiseAtHome (Nadi, 2026-11-05: "A"): the raised one moved to
	// where it was placed - its start position in its editor cell - when that
	// is an interior the game knows by editor ID; else raised where it lay.
	bool raiseAtHome = true;
	// Test switch (harness/safeguard-home.txt): this reference's death counts
	// as the walkers' doing, whoever killed it (0: none).
	UInt32 testVictim = 0;
};

// Where a raised one is taken (4.11b, RaiseAtHome).
struct HomeFacts {
	bool startKnown = false;       // the engine handed a start position
	UInt32 homeCell = 0;           // its editor cell (ExtraPersistentCell), 0 none
	bool homeInterior = false;
	bool homeHasEditorId = false;  // the cell keeps its editor ID (ExtraEditorID)
	UInt32 cellNow = 0;            // the cell it lies in
};

enum class HomeMove : UInt8 {
	PositionCell,     // moved into its home cell, onto its start position
	StayOff,          // RaiseAtHome=0
	StayUnknown,      // no start or no home cell known
	StayHome,         // it lies in its home cell already
	StayExterior,     // its home is outdoors: no editor ID to move by
	StayNoEditorId,   // an interior the game keeps no editor ID for
};

inline HomeMove HomeMoveFor(bool raiseAtHome, const HomeFacts& h) {
	if (!raiseAtHome) {
		return HomeMove::StayOff;
	}
	if (!h.startKnown || h.homeCell == 0) {
		return HomeMove::StayUnknown;
	}
	if (h.homeCell == h.cellNow) {
		return HomeMove::StayHome;
	}
	if (!h.homeInterior) {
		return HomeMove::StayExterior;
	}
	return h.homeHasEditorId ? HomeMove::PositionCell : HomeMove::StayNoEditorId;
}

inline const char* HomeMoveText(HomeMove m) {
	switch (m) {
		case HomeMove::PositionCell: return "moved home";
		case HomeMove::StayOff: return "where it lay (RaiseAtHome=0)";
		case HomeMove::StayUnknown: return "where it lay (no start or home cell known)";
		case HomeMove::StayHome: return "where it lay (at home already)";
		case HomeMove::StayExterior: return "where it lay (its home is outdoors)";
		case HomeMove::StayNoEditorId: return "where it lay (its home cell has no editor ID)";
	}
	return "?";
}

inline constexpr UInt32 kSafeguardMost = 128;

struct SafeguardEntry {
	UInt32 refId = 0;
	float seconds = 0.0f;  // dead this long
	float missingSince = -1.0f;  // the game day it was first not found, -1 held (MissingLogic.h)
};

struct SafeguardList {
	SafeguardEntry entries[kSafeguardMost];
	UInt32 count = 0;
};

// What the death path knows of one who died.
struct Victim {
	bool killedByWalker = false;
	// No killer handed over at all (gate-guards.txt 2026-10-26: a named woman
	// died among the walkers with killer 00000000 - a bleed, a fall, a
	// script's damage).
	bool killerUnknown = false;
	bool walker = false;
	bool player = false;
	bool gateGuard = false;
	bool rises = false;  // the rise's verdict: Waits
};

inline bool SafeguardTakes(const SafeguardSettings& s, const Victim& v) {
	return s.enabled && v.killedByWalker && !v.walker && !v.player && !v.gateGuard && !v.rises;
}

// A death without a killer that the safeguard asks about: it counts as the
// walkers' when a living walker stands within kSafeguardNearUnits of the
// body at the next frame (the caller looks - not inside the death path).
inline constexpr float kSafeguardNearUnits = 1000.0f;

inline bool SafeguardAsks(const SafeguardSettings& s, const Victim& v) {
	return s.enabled && v.killerUnknown && !v.walker && !v.player && !v.gateGuard && !v.rises;
}

// Noted once; false when the list is full (the caller says so in the log).
inline bool NoteVictim(SafeguardList& list, UInt32 refId) {
	for (UInt32 i = 0; i < list.count; ++i) {
		if (list.entries[i].refId == refId) {
			return true;
		}
	}
	if (list.count == kSafeguardMost) {
		return false;
	}
	list.entries[list.count++] = SafeguardEntry{refId, 0.0f};
	return true;
}

// Gone: the ID is no actor any more. Missing: not found right now - its
// cell freed from memory (MissingLogic.h).
enum class VictimNow : UInt8 { Gone, Missing, Alive, Dead };

// One step: the clocks run on, those up again or gone from the game leave
// the list - handed to `leftOf(id, now, seconds)` for the log (the Windows
// run of 2026-10-26: a Black Guar left the list unseen, why unknown) - and
// the due ones - dead AfterSeconds, `safeOf` true - leave it into `due`, at
// most `most` (the rest the next step). `stateOf(id)` and `safeOf(id)` read
// the game. A missing one is kept, its time dead standing, until
// kForgetMissingDays have passed on `nowDays` (the game's clock in days, -1
// not read), then handed to `leftOf` as missing. Off: nothing, the list kept.
template <typename StateFn, typename SafeFn, typename LeftFn>
UInt32 StepSafeguard(SafeguardList& list, const SafeguardSettings& s, float dt, StateFn stateOf, SafeFn safeOf,
                     LeftFn leftOf, UInt32* due, UInt32 most, float nowDays = -1.0f) {
	if (!s.enabled) {
		return 0;
	}
	UInt32 count = 0;
	UInt32 kept = 0;
	for (UInt32 i = 0; i < list.count; ++i) {
		SafeguardEntry entry = list.entries[i];
		const VictimNow now = stateOf(entry.refId);
		if (now == VictimNow::Missing) {
			if (KeepMissing(entry.missingSince, nowDays)) {
				list.entries[kept++] = entry;
			} else {
				leftOf(entry.refId, now, entry.seconds);
			}
			continue;
		}
		if (now != VictimNow::Dead) {
			leftOf(entry.refId, now, entry.seconds);
			continue;
		}
		entry.missingSince = -1.0f;
		entry.seconds += dt;
		if (count < most && entry.seconds >= s.afterSeconds && safeOf(entry.refId)) {
			due[count++] = entry.refId;
			continue;
		}
		list.entries[kept++] = entry;
	}
	list.count = kept;
	return count;
}

// The co-save record: type "SAFE", version 1 - the count, then reference
// and seconds dead per entry; version 2 (2026-10-27) adds the missing day.
inline constexpr UInt32 kSafeguardRecordType = 0x53414645;  // 'SAFE'
inline constexpr UInt32 kSafeguardRecordVersion = 2;
inline constexpr UInt32 kSafeguardEntryBytesV1 = 8;
inline constexpr UInt32 kSafeguardEntryBytes = 12;
inline constexpr UInt32 kSafeguardImageMost = 4 + kSafeguardMost * kSafeguardEntryBytes;

// Returns the bytes written, 0 when the image does not fit.
inline UInt32 SafeguardImage(const SafeguardList& list, UInt8* out, UInt32 capacity) {
	const UInt32 needed = 4 + list.count * kSafeguardEntryBytes;
	if (capacity < needed) {
		return 0;
	}
	Put32(out, list.count);
	for (UInt32 i = 0; i < list.count; ++i) {
		UInt32 seconds = 0;
		UInt32 missing = 0;
		std::memcpy(&seconds, &list.entries[i].seconds, sizeof(seconds));
		std::memcpy(&missing, &list.entries[i].missingSince, sizeof(missing));
		Put32(out + 4 + i * kSafeguardEntryBytes, list.entries[i].refId);
		Put32(out + 8 + i * kSafeguardEntryBytes, seconds);
		Put32(out + 12 + i * kSafeguardEntryBytes, missing);
	}
	return needed;
}

// As ReadWaitingImage: `list` emptied first, an unresolved reference
// dropped, false when cut short or claiming more than the list holds.
// `version` 1 (a save before 2026-10-27): no missing day, nobody missing.
template <typename ResolveFn>
bool ReadSafeguardImage(SafeguardList& list, const UInt8* in, UInt32 length, ResolveFn resolve,
                        UInt32 version = kSafeguardRecordVersion) {
	list.count = 0;
	if (length < 4) {
		return false;
	}
	const UInt32 entryBytes = version == 1 ? kSafeguardEntryBytesV1 : kSafeguardEntryBytes;
	const UInt32 count = Get32(in);
	if (count > kSafeguardMost) {
		return false;
	}
	const bool whole = length >= 4 + count * entryBytes;
	const UInt32 readable = whole ? count : (length - 4) / entryBytes;
	for (UInt32 i = 0; i < readable; ++i) {
		UInt32 refId = 0;
		if (!resolve(Get32(in + 4 + i * entryBytes), refId)) {
			continue;
		}
		const UInt32 bits = Get32(in + 8 + i * entryBytes);
		SafeguardEntry& entry = list.entries[list.count++];
		entry.refId = refId;
		std::memcpy(&entry.seconds, &bits, sizeof(bits));
		entry.missingSince = -1.0f;
		if (version != 1) {
			const UInt32 missing = Get32(in + 12 + i * entryBytes);
			std::memcpy(&entry.missingSince, &missing, sizeof(missing));
		}
	}
	return whole;
}

}  // namespace walkers
