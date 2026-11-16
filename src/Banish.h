#pragma once

// Dead Daedra of the plugin's back to Oblivion in the game (BanishLogic.h;
// docs/walkers-spec.md 4.22 idea 8, 4.22a step 5b): the shader played, the
// things moved into a LootBag placed at the body (its reference found by
// its ID, Banish.cpp PlaceBag), the body taken out through the placer (disabled,
// deleted); the bags taken away once the player is far.

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

// Once a frame; looked at once a second.
void StepBanishInGame(float dt);

// A load or a new game: nothing watched (the placer deletes the save's
// placed ones; a band's members are watched again as the bands read them).
void ResetBanish();

}  // namespace walkers
