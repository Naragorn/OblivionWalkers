#pragma once

// The rise in the game: the death watch through xOBSE's event manager, the
// frame tick through its task queue, the waiting list in the co-save, and
// the raise through the engine's own Resurrect (docs/walkers-spec.md, 4.1).
// The decisions are RiseLogic.h's; this file only reads the engine and calls.

#include "Obse.h"
#include "ReplaceLogic.h"
#include "RiseLogic.h"

namespace walkers {

using LogFn = void (*)(const char* format, ...);

// False when xOBSE lacks an interface the rise needs, or refuses the
// registration; the log says which. `iniPath` is the INI the spawn
// replacement writes its creature lines to (empty: none written).
bool InstallRise(const obse::Interface& xse, const RiseSettings& settings, const ReplaceSettings& replace,
                 const char* iniPath, LogFn log);

}  // namespace walkers
