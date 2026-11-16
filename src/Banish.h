#pragma once

// Dead Daedra of the plugin's back to Oblivion in the game (BanishLogic.h;
// docs/walkers-spec.md 4.22 idea 8, 4.22a step 5b): the shader played, the
// things moved into a LootBag placed at the body (its reference found by
// its ID, Banish.cpp PlaceBag), the body taken out through the placer (disabled,
// deleted); the bags taken away once the player is far - across a save too
// (the co-save's 'PILE').

#include "BanishLogic.h"
#include "Horde.h"
#include "Rise.h"

namespace walkers {

void ConfigureBanish(const BanishSettings* settings, ConsoleFn console, DeadFn dead, LogFn log);

// The ESP's base: its quest's siege_lock reads how many things a body and
// its bag hold (the log's proof).
void ConfigureBanishEsp(UInt32 espBase);

// A Daedra the plugin placed: banished once dead.
void WatchDaedra(UInt32 refId);
// The Daedra the plugin placed, watched now (the gate guards' foes beside
// the walkers).
bool IsWatchedDaedra(UInt32 refId);
UInt32 WatchedDaedraIds(UInt32* out, UInt32 most);

// Once a frame; looked at once a second.
void StepBanishInGame(float dt);

// The piles in the co-save ('PILE', BanishLogic.h): written, and read
// back after a load (ResetBanish first) - how many read.
UInt32 BanishPileImage(UInt8* out, UInt32 capacity);
UInt32 ReadBanishPileImage(const UInt8* in, UInt32 length, bool (*resolve)(UInt32, UInt32&));
UInt32 BanishPileCount();

// A load or a new game: nothing watched (the placer deletes the save's
// placed ones; a band's members are watched again as the bands read them).
void ResetBanish();

}  // namespace walkers
