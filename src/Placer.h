#pragma once

// Actors placed for a while in the game (PlaceLogic.h; docs/walkers-spec.md
// 4.22a step 5): a request's list placed at the ESP's place marker over three
// looks, the new actors handed to their owner; the owner's actors deleted in
// two looks when it is done. The placed ones still in the world are written
// to the co-save ('PLAC') and deleted after a load - their owner places its
// own again.

#include "Horde.h"
#include "PlaceLogic.h"
#include "Rise.h"

namespace walkers {

using PlacedFn = void (*)(UInt8 tag, const UInt32* refs, UInt32 count);

void ConfigurePlacer(UInt32 espBase, ConsoleFn console, LogFn log);

// Where an owner's new actors go.
void SetPlacedHandler(PlaceOwner owner, PlacedFn fn);

// Queued; false when the queue is full or the request is empty.
bool RequestPlace(const PlaceRequest& request);

// An owner's tag: its actors on the way, and those dropped from the queue.
UInt32 PlacesPendingFor(PlaceOwner owner, UInt8 tag);
UInt32 DropPlacesFor(PlaceOwner owner, UInt8 tag);

// A placed actor out of the world: disabled now, deleted at the next look.
void RetirePlaced(UInt32 refId);

// Once a frame; the stages are carried on once a second.
void StepPlacer(float dt);

// The actors on the high process list (creatures and characters), at most
// `most`.
UInt32 ActorsOnHighList(UInt32* out, UInt32 most);

// A reference's distance from the player: in the open air of either the
// 2D distance, in the player's interior the same; another interior far
// (1e9); -1 when not comparable (the player inside, it out; no cell).
float DistanceFromPlayer(UInt32 ref);

// The FF actors (placed at run time) within `units` of x, y on the high
// process list.
UInt32 PlacedActorsNear(float x, float y, float units, UInt32* out, UInt32 most);

// The newest reference made in the game (FF) of base `baseFormId` at or
// past `fromId`: a console line cannot hand a PlaceAtMe's reference back,
// the game numbers the ones it makes upward - scanned until kBagMissesMost
// IDs in a row hold nothing (BanishLogic.h). 0 none.
UInt32 NewestPlacedOf(UInt32 baseFormId, UInt32 fromId);

// The co-save record 'PLAC': the placed ones still in the world.
inline constexpr UInt32 kPlaceRecordType = 0x504C4143;  // 'PLAC'
inline constexpr UInt32 kPlaceRecordVersion = 1;
UInt32 PlacerImage(UInt8* out, UInt32 capacity);
// After a load: each one read out of the world (Retire).
void ReadPlacerImage(const UInt8* in, UInt32 length, bool (*resolve)(UInt32, UInt32&));

// A load or a new game: the queue and the stages forgotten.
void ResetPlacer();

}  // namespace walkers
