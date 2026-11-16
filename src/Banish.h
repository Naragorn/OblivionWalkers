#pragma once

// Dead Daedra of the plugin's back to Oblivion in the game (BanishLogic.h;
// docs/walkers-spec.md 4.22 idea 8, 4.22a step 5b): the shader played, the
// things moved into a LootBag placed at the body (its reference read off the
// ESP quest's placed_ref), the body taken out through the placer (disabled,
// deleted); the bags taken away once the player is far.

#include "BanishLogic.h"
#include "Horde.h"
#include "Rise.h"

namespace walkers {

void ConfigureBanish(const BanishSettings* settings, ConsoleFn console, DeadFn dead, LogFn log);

// The ESP's base (RiseLogic.h EspFormId): the quest whose placed_ref reads
// the bag back.
void ConfigureBanishEsp(UInt32 espBase);

// A Daedra the plugin placed: banished once dead.
void WatchDaedra(UInt32 refId);

// Once a frame; looked at once a second.
void StepBanishInGame(float dt);

// A load or a new game: nothing watched (the placer deletes the save's
// placed ones; a band's members are watched again as the bands read them).
void ResetBanish();

}  // namespace walkers
