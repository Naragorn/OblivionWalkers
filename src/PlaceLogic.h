#pragma once

// Actors placed for a while and taken out again, process-free
// (docs/walkers-spec.md 4.22a step 5; Nadi, 2026-11-16: "go 5a 5b 5c 6").
// A Daedra siege, a Daedra camp attack and a Daedra outpost bring their
// Daedra by PlaceAtMe of a list at the ESP's place marker and delete them
// when they are done - no pool of their own. The placing runs in stages
// over three looks (Placer.cpp): the marker moved, the list placed, the new
// actors found near it; one request at a time, the others queued.
//
// A placed actor is deleted in two looks: disabled, then - once the game
// has it disabled (a console Disable takes after the frame,
// oblivion-console-disable-deferred) - xOBSE's DeleteReference, which takes
// a disabled reference made in the game (FF), actors too (xOBSE
// Commands_MiscReference.cpp, Cmd_DeleteReference_Execute: mod index FF,
// not in a container, disabled; read 2026-11-16 through deepwiki).

#include "Types.h"

namespace walkers {

inline constexpr UInt32 kPlaceMarkerLocalId = 0xA96;  // the ESP's WalkersPlaceMarker (tools/esp-write.py)
inline constexpr UInt32 kPlaceQueueMost = 8;
inline constexpr UInt32 kPlaceCountMost = 24;  // actors of one request
inline constexpr float kPlaceFoundUnits = 1500.0f;

// Who asked: the placed ones are handed back to it with its tag.
enum class PlaceOwner : UInt8 { Siege, Camp, Dungeon, Test };

struct PlaceRequest {
	PlaceOwner owner = PlaceOwner::Test;
	UInt8 tag = 0;       // the owner's own: a city, a camp, a site
	UInt32 list = 0;     // the leveled list's form ID
	UInt32 count = 0;    // held to 1..kPlaceCountMost
	UInt32 anchor = 0;   // the marker is moved onto this reference (form ID; 0x14 the player)...
	float dx = 0.0f, dy = 0.0f, dz = 0.0f;  // ...this far from it
};

struct PlaceQueue {
	PlaceRequest items[kPlaceQueueMost];
	UInt32 count = 0;
};

// Queued at the end; false when the queue is full or the request names no
// list, no anchor or no actor.
inline bool PushPlace(PlaceQueue& q, PlaceRequest r) {
	if (q.count >= kPlaceQueueMost || r.list == 0 || r.anchor == 0 || r.count == 0) {
		return false;
	}
	r.count = r.count > kPlaceCountMost ? kPlaceCountMost : r.count;
	q.items[q.count++] = r;
	return true;
}

// The first request taken off the queue; false when it is empty.
inline bool PopPlace(PlaceQueue& q, PlaceRequest& out) {
	if (q.count == 0) {
		return false;
	}
	out = q.items[0];
	for (UInt32 i = 1; i < q.count; ++i) {
		q.items[i - 1] = q.items[i];
	}
	--q.count;
	return true;
}

// How many actors an owner's tag has on the way (queued, or the request
// under way when `current` is one of its).
inline UInt32 PlacesPending(const PlaceQueue& q, const PlaceRequest* current, PlaceOwner owner, UInt8 tag) {
	UInt32 n = current != nullptr && current->owner == owner && current->tag == tag ? current->count : 0;
	for (UInt32 i = 0; i < q.count; ++i) {
		n += q.items[i].owner == owner && q.items[i].tag == tag ? q.items[i].count : 0;
	}
	return n;
}

// Every queued request of an owner's tag dropped (its siege ended, the
// player left); returns how many.
inline UInt32 DropPlaces(PlaceQueue& q, PlaceOwner owner, UInt8 tag) {
	UInt32 kept = 0;
	UInt32 dropped = 0;
	for (UInt32 i = 0; i < q.count; ++i) {
		if (q.items[i].owner == owner && q.items[i].tag == tag) {
			++dropped;
		} else {
			q.items[kept++] = q.items[i];
		}
	}
	q.count = kept;
	return dropped;
}

// The actors near the marker now that were not before the PlaceAtMe, at
// most `want` of them, into `out`; returns how many.
inline UInt32 NewlyPlaced(const UInt32* before, UInt32 beforeCount, const UInt32* now, UInt32 nowCount, UInt32 want,
                          UInt32* out) {
	UInt32 n = 0;
	for (UInt32 i = 0; i < nowCount && n < want; ++i) {
		bool old = false;
		for (UInt32 k = 0; k < beforeCount && !old; ++k) {
			old = before[k] == now[i];
		}
		for (UInt32 k = 0; k < n && !old; ++k) {
			old = out[k] == now[i];
		}
		if (!old) {
			out[n++] = now[i];
		}
	}
	return n;
}

// Placed actors on their way out: disabled at the first look, deleted at
// the next.
inline constexpr UInt32 kRetireMost = 128;

struct RetireList {
	UInt32 refs[kRetireMost] = {};
	bool disabled[kRetireMost] = {};
	UInt32 count = 0;
};

// Only references made in the game (FF): the others are the game's or a
// plugin's and stay. False when full, not one, or already on the way.
inline bool NoteRetire(RetireList& r, UInt32 refId) {
	if ((refId >> 24) != 0xFF || r.count >= kRetireMost) {
		return false;
	}
	for (UInt32 i = 0; i < r.count; ++i) {
		if (r.refs[i] == refId) {
			return false;
		}
	}
	r.refs[r.count] = refId;
	r.disabled[r.count] = false;
	++r.count;
	return true;
}

// One look: those not yet disabled into `disable` (and marked so), those
// disabled at an earlier look into `remove` (and off the list).
inline void StepRetire(RetireList& r, UInt32* disable, UInt32& disableCount, UInt32* remove, UInt32& removeCount) {
	disableCount = 0;
	removeCount = 0;
	UInt32 kept = 0;
	for (UInt32 i = 0; i < r.count; ++i) {
		if (r.disabled[i]) {
			remove[removeCount++] = r.refs[i];
			continue;
		}
		disable[disableCount++] = r.refs[i];
		r.refs[kept] = r.refs[i];
		r.disabled[kept] = true;
		++kept;
	}
	r.count = kept;
}

}  // namespace walkers
