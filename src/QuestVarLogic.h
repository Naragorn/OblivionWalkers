#pragma once

// A quest script's variable by its name, process-free. The engine side
// (QuestVars.cpp) lists the script's variables - Script::varList at +0x48,
// each VariableInfo with its index at +0x00 and its name (BSStringT) at
// +0x18, xOBSE's Script.h - and reads the value from the quest's event list
// (TESQuest::scriptEventList at +0x58, xOBSE's GameForms.h) by that index.
// Used for Ultimate Leveling's experience (LevelingLogic.h, the check in the
// log) and for LINK's settings.

#include "Types.h"

namespace walkers {

struct VarName {
	UInt32 idx;
	const char* name;  // nullptr: a variable without a name
};

inline bool SameVariableName(const char* a, const char* b) {
	for (; *a != '\0' && *b != '\0'; ++a, ++b) {
		const char ca = (*a >= 'A' && *a <= 'Z') ? static_cast<char>(*a - 'A' + 'a') : *a;
		const char cb = (*b >= 'A' && *b <= 'Z') ? static_cast<char>(*b - 'A' + 'a') : *b;
		if (ca != cb) {
			return false;
		}
	}
	return *a == *b;
}

// The first of `count` variables named `name`, case aside; false when none is.
inline bool FindVariableIndex(const VarName* vars, UInt32 count, const char* name, UInt32& idx) {
	if (name == nullptr || name[0] == '\0') {
		return false;
	}
	for (UInt32 i = 0; i < count; ++i) {
		if (vars[i].name != nullptr && SameVariableName(vars[i].name, name)) {
			idx = vars[i].idx;
			return true;
		}
	}
	return false;
}

}  // namespace walkers
