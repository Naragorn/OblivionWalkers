#pragma once

// The marching weather in the game (MarchWeatherLogic.h; docs/walkers-spec.md
// 4.22 idea 9): the gates' red sky set while a Daedra siege is near the
// player outdoors in Tamriel, the weather before set again when it goes.

#include "Horde.h"
#include "MarchWeatherLogic.h"
#include "Rise.h"

namespace walkers {

void ConfigureMarchWeather(const MarchWeatherSettings* settings, ConsoleFn console, DeadFn dead, LogFn log);

// Once a frame; looked at once a second.
void StepMarchWeatherInGame(float dt);

// A load or a new game: nothing forced (the game's weather is the save's).
void ResetMarchWeather();

}  // namespace walkers
