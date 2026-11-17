// Every flow of the boss guard's clean copies (docs/walkers-spec.md 4.6c,
// BossCopyLogic.h): which entry of a boss list is pointed at a copy, which
// goes back to its own list, what a copy names, how deep copies go.

#include "BossCopyLogic.h"

#include <cstdio>

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf("  %s  %s\n", condition ? "ok  " : "FAIL", what);
	if (!condition) {
		++g_failures;
	}
}

using walkers::BossEntryAction;
using walkers::EntryAction;
using walkers::EntryTarget;

EntryTarget List() {
	EntryTarget t;
	t.isList = true;
	return t;
}

void TestAction() {
	std::printf("What a boss list's entry gets\n");
	Check(BossEntryAction(true, List()) == EntryAction::Redirect,
	      "an ordinary nested list (MOO's region list): pointed at its clean copy");
	EntryTarget actor;
	Check(BossEntryAction(true, actor) == EntryAction::Keep, "a creature or person: kept, the pass already keeps it");
	EntryTarget boss = List();
	boss.isBossList = true;
	Check(BossEntryAction(true, boss) == EntryAction::Keep, "a nested boss list: kept, it guards its own entries");
	EntryTarget ours = List();
	ours.isOurs = true;
	Check(BossEntryAction(true, ours) == EntryAction::Keep, "the walkers' or the Daedra's list: kept (not ours to copy)");
	EntryTarget copy = List();
	copy.isCopy = true;
	Check(BossEntryAction(true, copy) == EntryAction::Keep, "already a clean copy: kept");
	Check(BossEntryAction(false, copy) == EntryAction::Restore, "guard off (KeepBosses=0): a copy goes back to its own list");
	Check(BossEntryAction(false, List()) == EntryAction::Keep, "guard off: an ordinary nested list stays as it is");
	Check(BossEntryAction(false, actor) == EntryAction::Keep, "guard off: a creature stays as it is");
}

void TestCopyTarget() {
	std::printf("What a copy's entry names\n");
	Check(walkers::CopySourceForm(0x2B000805, true, 0x0001FCB2) == 0x0001FCB2,
	      "an entry the pass pointed at the walkers: the creature it named before");
	Check(walkers::CopySourceForm(0x0001FCB2, false, 0) == 0x0001FCB2, "an entry the pass left: as it is");
	Check(walkers::CopySourceForm(0x0001FCB2, true, 0) == 0x0001FCB2, "an original not known (0): as it is");
}

void TestDepth() {
	std::printf("How deep copies go\n");
	Check(walkers::CopyNestedAt(0), "a list in a boss list: copied");
	Check(walkers::CopyNestedAt(walkers::kBossCopyDepthMost - 1), "one level short of the most: copied");
	Check(!walkers::CopyNestedAt(walkers::kBossCopyDepthMost), "at the most: not copied deeper (a cycle or a deep tree)");
}

void TestRebuild() {
	std::printf("When a copy is built anew\n");
	Check(walkers::CopyNeedsBuild(false, 0, 0), "no copy yet: built");
	Check(!walkers::CopyNeedsBuild(true, 0x1234, 0x1234), "the source as when copied: left");
	Check(walkers::CopyNeedsBuild(true, 0x1234, 0x9999), "the source changed (MOO refilled its region list): built anew");
}

}  // namespace

int main() {
	TestAction();
	TestCopyTarget();
	TestDepth();
	TestRebuild();
	if (g_failures != 0) {
		std::printf("%d failed\n", g_failures);
		return 1;
	}
	std::printf("all passed\n");
	return 0;
}
