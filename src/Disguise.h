#pragma once

// Walker blood as a disguise in the game (docs/walkers-spec.md 4.20.1):
// DisguiseLogic.h's decisions carried out - the dialog, the clock, the
// water and the rain read off the engine, the ability shown, OBVR told.

#include "DisguiseLogic.h"
#include "Rise.h"

namespace walkers {

using DisguiseConsoleFn = bool (*)(UInt32 ref, const char* line);
using DisguiseWalkerFn = bool (*)(UInt32 ref);  // a walker reference
using DisguiseDeadFn = bool (*)(UInt32 actor);
using DisguiseSendFn = bool (*)(UInt32 type, void* data, UInt32 length);  // to OBVR

// Once a session: the live settings (LINK writes into them), the test switch,
// the console, the walker and death checks, OBVR's channel, the log.
void ConfigureDisguise(const DisguiseSettings* settings, bool test, DisguiseConsoleFn console, DisguiseWalkerFn walker,
                       DisguiseDeadFn dead, DisguiseSendFn send, LogFn log);
void ConfigureDisguiseEsp(UInt32 espBase);

// xOBSE's OnActivate, both of its references (the order measured in the
// first run - either may be the player).
void NoteDisguiseActivate(UInt32 a, UInt32 b);
// xOBSE's OnHit: `target` hit by `attacker`.
void NoteDisguiseHit(UInt32 target, UInt32 attacker);
// OBVR's gesture ('WGBL'): smeared from `corpse`, no dialog.
void NoteDisguiseSmear(UInt32 corpse);

// Each frame in the game, `dt` real seconds.
void StepDisguiseInGame(float dt);
// Whether the player is disguised now.
bool Disguised();
// Once, after a begin: true - the walkers fighting the player are to stop.
bool TakeDisguiseBegun();
// A load or a new game: off, no dialog.
void ResetDisguise();

}  // namespace walkers
