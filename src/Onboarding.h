#pragma once

// The onboarding in the game (docs/walkers-spec.md 4.24a): OnboardingLogic.h's
// chain of message boxes shown one after the other with the game's own box
// (as Disguise.cpp shows its offer), the answers read off the button the
// game keeps, the choice carried out when the player says Start.

#include "OnboardingLogic.h"
#include "Rise.h"

namespace walkers {

using OnboardingConsoleFn = bool (*)(UInt32 ref, const char* line);
// The player pressed Start: the switches to apply to the running settings,
// the LINK variables and the spawn replacement.
using OnboardingChosenFn = void (*)(WalkersMode walkers, DaedraMode daedra);

// Once a session: the live settings (LINK writes into `show`), the plugin
// INI's path (the choice is written into it), the console (the test
// answers), the Start handler, the log.
void ConfigureOnboarding(OnboardingSettings* settings, const char* iniPath, OnboardingConsoleFn console,
                         OnboardingChosenFn chosen, LogFn log);

// Each frame, `dt` real seconds: when the player is in a game with no menu
// open, a few seconds after a load, the first box; then the next after each
// answer.
void StepOnboardingInGame(float dt);

// Whether one of the onboarding's boxes is up (the disguise's box waits).
bool OnboardingBoxUp();

// A load or a new game: no box, the chain from its first page again, the
// wait before the first box starts anew.
void ResetOnboarding();

}  // namespace walkers
