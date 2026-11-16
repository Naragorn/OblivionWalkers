#pragma once

// The Daedric disguise in the game (DaedricDisguiseLogic.h; docs/walkers-spec.md
// 4.22 idea 7): the player's worn armor read off his inventory, the nearest
// Daedra off the high process list, his blows on them from xOBSE's OnHit;
// fooled, he stands in the Daedra's factions.

#include "DaedricDisguiseLogic.h"
#include "Horde.h"
#include "Rise.h"

namespace walkers {

void ConfigureDaedricDisguise(const DaedricSettings* settings, ConsoleFn console, DeadFn dead, LogFn log);

// xOBSE's OnHit: `target` hit by `attacker`.
void NoteDaedricHit(UInt32 target, UInt32 attacker);

// Once a frame; looked at twice a second.
void StepDaedricDisguiseInGame(float dt);

// Whether the Daedra take the player for one of theirs now.
bool DaedricDisguised();

// A load or a new game: not fooled, not known; the player taken out of the
// factions at the next look (a save made while fooled holds him in them).
void ResetDaedricDisguise();

}  // namespace walkers
