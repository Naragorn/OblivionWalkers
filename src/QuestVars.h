#pragma once

// Quest script variables in the running game (QuestVarLogic.h has the
// layout and the pure lookup).

#include "Types.h"

namespace walkers {

// The value of `name` in the event list of the quest `questFormId`;
// nullptr when the quest, its script, its event list or the variable is not
// there. The engine keeps every script variable as a double.
double* QuestVariable(UInt32 questFormId, const char* name);

}  // namespace walkers
