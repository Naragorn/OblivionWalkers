#pragma once

// What the plugin does when xOBSE queries and loads it - with the side
// effects behind a Host so every flow runs in a test.

#include "Obse.h"
#include "RiseLogic.h"
#include "Types.h"

namespace walkers {

constexpr const char* kPluginName = "OblivionWalkers";
constexpr UInt32 kPluginVersion = 1;

// obse_common/obse_version.h: MAKE_OBLIVION_VERSION(1, 2, 416). Every engine
// address the plugin reads or calls is of this build and of no other.
constexpr UInt32 kOblivionVersion_1_2_416 = 0x010201A0;

// The event manager and the task interface came with OBSE 19; xOBSE 21 is
// the first version the plugin was ever run with.
constexpr UInt32 kObseVersionLeast = 21;

struct Settings {
	RiseSettings rise;
};

struct Host {
	void (*resetLog)();
	void (*log)(const char* format, ...);
	// Reads the INI next to the DLL into *settings; false if there is no INI
	// (the defaults then stand).
	bool (*readSettings)(Settings* settings);
	// Installs the death watch, the frame tick and the co-save; false when
	// xOBSE lacks an interface it needs (the installer logs which).
	bool (*installRise)(const obse::Interface& xse, const RiseSettings& rise);
};

bool Query(const obse::Interface& xse, obse::PluginInfo* info, const Host& host);
// Always true: refusing to load would make nothing safer, and the log says
// what was skipped.
bool Load(const obse::Interface& xse, const Host& host);

// The state Load left; exposed for tests.
const Settings& LoadedSettings();
void ResetForTest();

}  // namespace walkers
