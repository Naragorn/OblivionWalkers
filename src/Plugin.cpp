// The DLL's exports and real side effects: the log, the INI, the rise's
// installation. The decisions live in PluginEntry and RiseLogic.

#include "PluginEntry.h"
#include "Rise.h"

#include <windows.h>

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

constexpr const char* kLogFile = "OblivionWalkers.log";
constexpr const char* kIniFile = "OblivionWalkers.ini";

HMODULE g_module = nullptr;
SRWLOCK g_logLock = SRWLOCK_INIT;

void AppendLine(const char* format, va_list args) {
	AcquireSRWLockExclusive(&g_logLock);
	if (FILE* file = std::fopen(kLogFile, "a")) {
		std::vfprintf(file, format, args);
		std::fputc('\n', file);
		std::fclose(file);
	}
	ReleaseSRWLockExclusive(&g_logLock);
}

void Log(const char* format, ...) {
	va_list args;
	va_start(args, format);
	AppendLine(format, args);
	va_end(args);
}

// The last session's log survives one start as OblivionWalkers.log.prev,
// so a player's run can still be read after a harness run took the game.
void ResetLog() {
	AcquireSRWLockExclusive(&g_logLock);
	MoveFileExA(kLogFile, "OblivionWalkers.log.prev", MOVEFILE_REPLACE_EXISTING);
	if (FILE* file = std::fopen(kLogFile, "w")) {
		std::fclose(file);
	}
	ReleaseSRWLockExclusive(&g_logLock);
}

bool IniPath(char* path, size_t length) {
	char module[MAX_PATH] = {};
	if (GetModuleFileNameA(g_module, module, MAX_PATH) == 0) {
		return false;
	}
	char* slash = std::strrchr(module, '\\');
	if (slash == nullptr) {
		return false;
	}
	*(slash + 1) = '\0';
	std::snprintf(path, length, "%s%s", module, kIniFile);
	return true;
}

float IniFloat(const char* section, const char* key, float fallback, const char* path) {
	char text[64] = {};
	GetPrivateProfileStringA(section, key, "", text, sizeof(text), path);
	if (text[0] == '\0') {
		return fallback;
	}
	char* end = nullptr;
	const double value = std::strtod(text, &end);
	return end == text ? fallback : static_cast<float>(value);
}

bool ReadSettings(walkers::Settings* settings) {
	char path[MAX_PATH] = {};
	if (!IniPath(path, sizeof(path)) || GetFileAttributesA(path) == INVALID_FILE_ATTRIBUTES) {
		return false;
	}
	walkers::RiseSettings& rise = settings->rise;
	rise.enabled = GetPrivateProfileIntA("Rise", "Enabled", 1, path) != 0;
	rise.afterSeconds = IniFloat("Rise", "AfterSeconds", 20.0f, path);
	rise.jitterSeconds = IniFloat("Rise", "JitterSeconds", 10.0f, path);
	const int chance = GetPrivateProfileIntA("Rise", "ChancePercent", 100, path);
	rise.chancePercent = chance < 0 ? 0u : (chance > 100 ? 100u : static_cast<UInt32>(chance));
	rise.onlyGeneric = GetPrivateProfileIntA("Rise", "OnlyGeneric", 1, path) != 0;
	rise.onlyPlayableRaces = GetPrivateProfileIntA("Rise", "OnlyPlayableRaces", 1, path) != 0;
	rise.onlyHumans = GetPrivateProfileIntA("Rise", "OnlyHumans", 0, path) != 0;
	rise.cloneBase = GetPrivateProfileIntA("Rise", "CloneBase", 0, path) != 0;
	rise.zombieRace = GetPrivateProfileIntA("Rise", "ZombieRace", 1, path) != 0;
	rise.lookAfterSeconds = IniFloat("Rise", "LookAfterSeconds", 3.0f, path);
	if (rise.lookAfterSeconds < 0.0f) {
		rise.lookAfterSeconds = 0.0f;
	}
	rise.asCreature = GetPrivateProfileIntA("Rise", "AsCreature", 1, path) != 0;
	rise.walkOnly = GetPrivateProfileIntA("Zombies", "Walk", 1, path) != 0;
	const int health = GetPrivateProfileIntA("Zombies", "HealthPercent", 15, path);
	rise.healthPercent = health < 1 ? 1u : (health > 100 ? 100u : static_cast<UInt32>(health));
	rise.noLoot = GetPrivateProfileIntA("Zombies", "NoLoot", 1, path) != 0;
	rise.speedMultiplier = IniFloat("Zombies", "SpeedMultiplier", 1.0f, path);
	const int spread = GetPrivateProfileIntA("Zombies", "SpeedRandomPercent", 30, path);
	rise.speedRandomPercent = spread < 0 ? 0u : (spread > 100 ? 100u : static_cast<UInt32>(spread));
	const int fame = GetPrivateProfileIntA("Zombies", "FamePerKills", 0, path);
	rise.famePerKills = fame < 0 ? 0u : static_cast<UInt32>(fame);
	rise.avoidChapels = GetPrivateProfileIntA("Zombies", "AvoidChapels", 1, path) != 0;
	walkers::ReplaceSettings& replace = settings->replace;
	replace.enabled = GetPrivateProfileIntA("Replace", "Enabled", 1, path) != 0;
	const int least = GetPrivateProfileIntA("Replace", "MinCount", 1, path);
	const int most = GetPrivateProfileIntA("Replace", "MaxCount", 10, path);
	replace.minCount = least < 1 ? 1u : static_cast<UInt32>(least);
	replace.maxCount = most < 1 ? 1u : static_cast<UInt32>(most);
	const int extra = GetPrivateProfileIntA("Replace", "ExtraPercent", 0, path);
	replace.extraPercent = extra < 0 ? 0u : static_cast<UInt32>(extra);
	std::snprintf(settings->iniPath, sizeof(settings->iniPath), "%s", path);
	rise.dropSeconds = IniFloat("Rise", "DropSeconds", 1.5f, path);
	if (rise.dropSeconds < 0.0f) {
		rise.dropSeconds = 0.0f;
	}
	if (rise.afterSeconds < 0.0f) {
		rise.afterSeconds = 0.0f;
	}
	if (rise.jitterSeconds < 0.0f) {
		rise.jitterSeconds = 0.0f;
	}
	Log("Settings from %s: [Rise] Enabled=%d AfterSeconds=%.1f JitterSeconds=%.1f ChancePercent=%u OnlyGeneric=%d "
	    "OnlyPlayableRaces=%d OnlyHumans=%d AsCreature=%d DropSeconds=%.1f CloneBase=%d ZombieRace=%d "
	    "LookAfterSeconds=%.1f; [Zombies] Walk=%d SpeedMultiplier=%.2f SpeedRandomPercent=%u FamePerKills=%u AvoidChapels=%d; "
	    "[Replace] ExtraPercent=%u",
	    path, rise.enabled ? 1 : 0, rise.afterSeconds, rise.jitterSeconds, rise.chancePercent,
	    rise.onlyGeneric ? 1 : 0, rise.onlyPlayableRaces ? 1 : 0, rise.onlyHumans ? 1 : 0, rise.asCreature ? 1 : 0,
	    rise.dropSeconds, rise.cloneBase ? 1 : 0, rise.zombieRace ? 1 : 0, rise.lookAfterSeconds,
	    rise.walkOnly ? 1 : 0, rise.speedMultiplier, rise.speedRandomPercent, rise.famePerKills,
	    rise.avoidChapels ? 1 : 0, replace.extraPercent);
	return true;
}

bool InstallRiseSideEffect(const walkers::obse::Interface& xse, const walkers::Settings& settings) {
	return walkers::InstallRise(xse, settings.rise, settings.replace, settings.iniPath, &Log);
}

const walkers::Host kHost{&ResetLog, &Log, &ReadSettings, &InstallRiseSideEffect};

}  // namespace

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID) {
	if (reason == DLL_PROCESS_ATTACH) {
		g_module = instance;
		DisableThreadLibraryCalls(instance);
	}
	return TRUE;
}

extern "C" {

__declspec(dllexport) bool OBSEPlugin_Query(const walkers::obse::Interface* xse, walkers::obse::PluginInfo* info) {
	return walkers::Query(*xse, info, kHost);
}

__declspec(dllexport) bool OBSEPlugin_Load(const walkers::obse::Interface* xse) {
	return walkers::Load(*xse, kHost);
}

}  // extern "C"
