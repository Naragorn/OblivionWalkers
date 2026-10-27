// Every flow of finding a quest script's variable by name (QuestVarLogic.h):
// the variable's index in the script's list, as the engine's names hold it.

#include "QuestVarLogic.h"

#include <cstdio>

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf("  %s  %s\n", condition ? "ok  " : "FAIL", what);
	if (!condition) {
		++g_failures;
	}
}

void TestFind() {
	std::printf("A variable by its name\n");
	const walkers::VarName vars[] = {{1, "ini_version"}, {2, "XP"}, {3, "XPmod"}, {7, "xp"}};
	UInt32 idx = 0;
	Check(walkers::FindVariableIndex(vars, 4, "XPmod", idx) && idx == 3, "found: its index");
	Check(walkers::FindVariableIndex(vars, 4, "xpMOD", idx) && idx == 3,
	      "case does not matter, as for the game's scripts");
	Check(walkers::FindVariableIndex(vars, 4, "xp", idx) && idx == 2, "two alike: the first in the list");
	Check(!walkers::FindVariableIndex(vars, 4, "XPm", idx) && !walkers::FindVariableIndex(vars, 4, "XPmodX", idx),
	      "a part or a longer name: not found");
	Check(!walkers::FindVariableIndex(vars, 0, "XP", idx), "no variables: not found");
	const walkers::VarName holes[] = {{4, nullptr}, {5, "Walk"}};
	Check(walkers::FindVariableIndex(holes, 2, "walk", idx) && idx == 5, "a variable without a name is passed over");
	Check(!walkers::FindVariableIndex(vars, 4, nullptr, idx) && !walkers::FindVariableIndex(vars, 4, "", idx),
	      "no name asked: not found");
}

}  // namespace

int main() {
	TestFind();
	if (g_failures != 0) {
		std::printf("%d failed\n", g_failures);
		return 1;
	}
	std::printf("all passed\n");
	return 0;
}
