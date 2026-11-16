// The INI's renamed keys (docs/walkers-spec.md 8b): each new name finds its
// old one, a key of [Walkers] finds [Zombies], a key never renamed none; no
// name twice, none the same as its old one.

#include "IniNames.h"

#include <cstdio>
#include <cstring>

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf("  %s  %s\n", condition ? "ok  " : "FAIL", what);
	if (!condition) {
		++g_failures;
	}
}

void TestNames() {
	std::printf("The renamed keys\n");
	const char* section = nullptr;
	const char* key = nullptr;
	Check(walkers::OldIniName("Walkers", "WalkOnly", section, key) && std::strcmp(section, "Zombies") == 0 &&
	          std::strcmp(key, "Walk") == 0,
	      "[Walkers] WalkOnly was [Zombies] Walk");
	Check(walkers::OldIniName("Replace", "WalkersPerSpawnMin", section, key) && std::strcmp(section, "Replace") == 0 &&
	          std::strcmp(key, "MinCount") == 0,
	      "[Replace] WalkersPerSpawnMin was MinCount");
	Check(walkers::OldIniName("Walkers", "HealthPercent", section, key) && std::strcmp(section, "Zombies") == 0 &&
	          std::strcmp(key, "HealthPercent") == 0,
	      "a key of [Walkers] not renamed: the same key under [Zombies]");
	Check(!walkers::OldIniName("Rise", "Enabled", section, key), "a key never renamed: none");
	Check(!walkers::OldIniName("Replace", "MinCount", section, key), "the old name itself: no older one");
	bool unique = true;
	bool changed = true;
	for (UInt32 i = 0; i < walkers::kIniRenameCount; ++i) {
		const walkers::IniRename& a = walkers::kIniRenames[i];
		changed = changed && (std::strcmp(a.key, a.oldKey) != 0 || std::strcmp(a.section, a.oldSection) != 0);
		for (UInt32 j = i + 1; j < walkers::kIniRenameCount; ++j) {
			const walkers::IniRename& b = walkers::kIniRenames[j];
			unique = unique && !(std::strcmp(a.section, b.section) == 0 && std::strcmp(a.key, b.key) == 0) &&
			         !(std::strcmp(a.oldSection, b.oldSection) == 0 && std::strcmp(a.oldKey, b.oldKey) == 0);
		}
	}
	Check(unique, "no new name and no old name twice");
	Check(changed, "every rename changes something");
	Check(walkers::kIniRenameCount == 40, "forty keys renamed");
}

}  // namespace

int main() {
	TestNames();
	if (g_failures != 0) {
		std::printf("%d failed\n", g_failures);
		return 1;
	}
	std::printf("all passed\n");
	return 0;
}
