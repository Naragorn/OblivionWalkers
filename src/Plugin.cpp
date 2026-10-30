// The DLL's exports and real side effects: the log, the INI, the rise's
// installation. The decisions live in PluginEntry and RiseLogic.

#include "PluginEntry.h"
#include "PoseLogic.h"
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
	rise.blockMooResurrect = GetPrivateProfileIntA("Zombies", "BlockMooResurrect", 1, path) != 0;
	rise.noLevelingXp = GetPrivateProfileIntA("Zombies", "NoLevelingXP", 1, path) != 0;
	rise.noSkillGain = GetPrivateProfileIntA("Zombies", "NoSkillGain", 1, path) != 0;
	rise.speedMultiplier = IniFloat("Zombies", "SpeedMultiplier", 1.0f, path);
	const int spread = GetPrivateProfileIntA("Zombies", "SpeedRandomPercent", 30, path);
	rise.speedRandomPercent = spread < 0 ? 0u : (spread > 100 ? 100u : static_cast<UInt32>(spread));
	const int fame = GetPrivateProfileIntA("Zombies", "FamePerKills", 0, path);
	rise.famePerKills = fame < 0 ? 0u : static_cast<UInt32>(fame);
	rise.avoidChapels = GetPrivateProfileIntA("Zombies", "AvoidChapels", 1, path) != 0;
	rise.renameToWalker = GetPrivateProfileIntA("Zombies", "RenameToWalker", 1, path) != 0;
	GetPrivateProfileStringA("Zombies", "Name", "Walker", rise.walkerName, sizeof(rise.walkerName), path);
	if (rise.walkerName[0] == '\0') {
		std::snprintf(rise.walkerName, sizeof(rise.walkerName), "%s", "Walker");
	}
	rise.renameVanilla = GetPrivateProfileIntA("Zombies", "RenameVanilla", 0, path) != 0;
	walkers::ReplaceSettings& replace = settings->replace;
	replace.enabled = GetPrivateProfileIntA("Replace", "Enabled", 1, path) != 0;
	const int least = GetPrivateProfileIntA("Replace", "MinCount", 1, path);
	const int most = GetPrivateProfileIntA("Replace", "MaxCount", 10, path);
	replace.minCount = least < 1 ? 1u : static_cast<UInt32>(least);
	replace.maxCount = most < 1 ? 1u : static_cast<UInt32>(most);
	const int extra = GetPrivateProfileIntA("Replace", "ExtraPercent", 0, path);
	replace.extraPercent = extra < 0 ? 0u : static_cast<UInt32>(extra);
	char categories[1024];
	UInt32 categoriesUsed = 0;
	for (UInt32 k = 0; k < walkers::kReplaceCategoryCount; ++k) {
		const walkers::ReplaceCategory& c = walkers::kReplaceCategories[k];
		replace.categories[k] = GetPrivateProfileIntA("ReplaceCategories", c.key, c.defaultOn ? 1 : 0, path) != 0;
		const int n = std::snprintf(categories + categoriesUsed, sizeof(categories) - categoriesUsed, "%s%s=%d",
		                            k == 0 ? "" : " ", c.key, replace.categories[k] ? 1 : 0);
		if (n > 0 && categoriesUsed + static_cast<UInt32>(n) < sizeof(categories)) {
			categoriesUsed += static_cast<UInt32>(n);
		}
	}
	categories[categoriesUsed] = '\0';
	Log("Settings from %s: [Replace] Enabled=%d MinCount=%u MaxCount=%u; [ReplaceCategories] %s", path,
	    replace.enabled ? 1 : 0, replace.minCount, replace.maxCount, categories);
	std::snprintf(settings->iniPath, sizeof(settings->iniPath), "%s", path);
	rise.dropSeconds = IniFloat("Rise", "DropSeconds", 1.5f, path);
	if (rise.dropSeconds < 0.0f) {
		rise.dropSeconds = 0.0f;
	}
	const int match = GetPrivateProfileIntA("Rise", "MatchCorpse", 2, path);
	rise.matchCorpse = static_cast<UInt8>(walkers::MatchCorpseFrom(match));
	rise.settleSeconds = IniFloat("Rise", "SettleSeconds", 1.5f, path);
	if (!(rise.settleSeconds >= 0.0f)) {
		rise.settleSeconds = 0.0f;
	}
	rise.hideCorpseAtPose = GetPrivateProfileIntA("Rise", "HideCorpseAtPose", 0, path) != 0;
	rise.keepCorpseApart = GetPrivateProfileIntA("Rise", "KeepCorpseApart", 1, path) != 0;
	rise.holdDeathSound = GetPrivateProfileIntA("Rise", "HoldDeathSound", 1, path) != 0;
	rise.horde.enabled = GetPrivateProfileIntA("Horde", "Enabled", 1, path) != 0;
	rise.horde.joinRadius = IniFloat("Horde", "JoinRadius", 1500.0f, path);
	rise.horde.leadUnits = IniFloat("Horde", "LeadUnits", 2000.0f, path);
	rise.horde.staleSeconds = IniFloat("Horde", "StaleSeconds", 30.0f, path);
	rise.horde.turnDegrees = IniFloat("Horde", "TurnDegrees", 60.0f, path);
	rise.horde.towardCities = GetPrivateProfileIntA("Horde", "TowardCities", 1, path) != 0;
	rise.horde.cityPull = IniFloat("Horde", "CityPull", 0.5f, path);
	rise.horde.cityPull = rise.horde.cityPull < 0.0f ? 0.0f : (rise.horde.cityPull > 1.0f ? 1.0f : rise.horde.cityPull);
	rise.horde.cityRange = IniFloat("Horde", "CityRange", 40000.0f, path);
	rise.horde.gateHoldUnits = IniFloat("Horde", "GateHoldUnits", 800.0f, path);
	const int groupMost = GetPrivateProfileIntA("Horde", "GroupMost", 8, path);
	rise.horde.groupMost = groupMost < 0 ? 0u : static_cast<UInt32>(groupMost);
	rise.guards.enabled = GetPrivateProfileIntA("Guards", "Enabled", 1, path) != 0;
	rise.guards.healthFactor = IniFloat("Guards", "HealthFactor", 5.0f, path);
	rise.guards.healthFactor = rise.guards.healthFactor < 0.0f ? 0.0f : rise.guards.healthFactor;
	const int perGuard = GetPrivateProfileIntA("Guards", "ZombiesPerGuard", 5, path);
	rise.guards.zombiesPerGuard = perGuard < 1 ? 1u : static_cast<UInt32>(perGuard);
	rise.guards.radius = IniFloat("Guards", "Radius", 4000.0f, path);
	rise.guards.clearSeconds = IniFloat("Guards", "ClearSeconds", 30.0f, path);
	rise.guards.testKillSeconds = IniFloat("Guards", "TestKillSeconds", 0.0f, path);
	rise.safeguard.enabled = GetPrivateProfileIntA("Safeguard", "Enabled", 1, path) != 0;
	rise.safeguard.afterSeconds = IniFloat("Safeguard", "AfterSeconds", 15.0f, path);
	rise.safeguard.afterSeconds = rise.safeguard.afterSeconds < 0.0f ? 0.0f : rise.safeguard.afterSeconds;
	rise.safeguard.radius = IniFloat("Safeguard", "Radius", 3000.0f, path);
	rise.news.courier = GetPrivateProfileIntA("News", "Courier", 1, path) != 0;
	rise.news.recentDays = IniFloat("News", "RecentDays", 3.0f, path);
	rise.news.recentDays = rise.news.recentDays < 0.0f ? 0.0f : rise.news.recentDays;
	rise.news.headingHoldDays = IniFloat("News", "HeadingHoldDays", 0.5f, path);
	rise.news.headingHoldDays = rise.news.headingHoldDays < 0.0f ? 0.0f : rise.news.headingHoldDays;
	rise.grab.enabled = GetPrivateProfileIntA("Grab", "Enabled", 1, path) != 0;
	const int grabChance = GetPrivateProfileIntA("Grab", "ChancePercent", 50, path);
	rise.grab.chancePercent = grabChance < 0 ? 0u : (grabChance > 100 ? 100u : static_cast<UInt32>(grabChance));
	const int bites = GetPrivateProfileIntA("Grab", "BitesToDie", 3, path);
	rise.grab.bitesToDie = bites < 1 ? 1u : static_cast<UInt32>(bites);
	rise.grab.biteSeconds = IniFloat("Grab", "BiteSeconds", 4.0f, path);
	rise.grab.biteSeconds = rise.grab.biteSeconds < 0.5f ? 0.5f : rise.grab.biteSeconds;
	const int biteHealth = GetPrivateProfileIntA("Grab", "BiteHealthPercent", 20, path);
	rise.grab.biteHealthPercent = biteHealth < 0 ? 0u : (biteHealth > 100 ? 100u : static_cast<UInt32>(biteHealth));
	const int struggle = GetPrivateProfileIntA("Grab", "StruggleHits", 2, path);
	rise.grab.struggleHits = struggle < 1 ? 1u : static_cast<UInt32>(struggle);
	rise.grab.tearFreeDistance = IniFloat("Grab", "TearFreeDistance", 1000.0f, path);
	rise.grab.biteResetSeconds = IniFloat("Grab", "BiteResetSeconds", 30.0f, path);
	rise.grab.testGrabSeconds = IniFloat("Grab", "TestGrabSeconds", 0.0f, path);
	const int presses = GetPrivateProfileIntA("Grab", "StrugglePresses", 4, path);
	rise.grab.strugglePresses = presses < 0 ? 0u : static_cast<UInt32>(presses);
	rise.grab.struggleGapSeconds = IniFloat("Grab", "StruggleGapSeconds", 0.8f, path);
	rise.grab.struggleCrowdStep = IniFloat("Grab", "StruggleCrowdStep", 0.5f, path);
	rise.grab.struggleCrowdMost = IniFloat("Grab", "StruggleCrowdMost", 3.0f, path);
	rise.grab.struggleCrowdMost = rise.grab.struggleCrowdMost < 1.0f ? 1.0f : rise.grab.struggleCrowdMost;
	rise.grab.struggleCrowdRadius = IniFloat("Grab", "StruggleCrowdRadius", 300.0f, path);
	rise.grab.struggleFatigue = IniFloat("Grab", "StruggleFatigue", 5.0f, path);
	rise.grab.messages = GetPrivateProfileIntA("Grab", "Messages", 1, path) != 0;
	const int strokes = GetPrivateProfileIntA("Grab", "ShakeStrokes", 6, path);
	rise.grab.shakeStrokes = strokes < 0 ? 0u : static_cast<UInt32>(strokes);
	rise.grab.shakeFatigue = IniFloat("Grab", "ShakeFatigue", 2.0f, path);
	const int control = GetPrivateProfileIntA("Grab", "StruggleControl", 6, path);
	rise.grab.struggleControl = control < 0 || control > 28 ? 6u : static_cast<UInt32>(control);
	const int getUpSound = GetPrivateProfileIntA("Rise", "SoundAtGetUpPercent", 60, path);
	rise.soundAtGetUpPercent = getUpSound < 0 ? 0u : static_cast<UInt32>(getUpSound);
	rise.lieSeconds = IniFloat("Rise", "LieSeconds", 1.0f, path);
	if (rise.lieSeconds < 0.0f) {
		rise.lieSeconds = 0.0f;
	}
	if (rise.afterSeconds < 0.0f) {
		rise.afterSeconds = 0.0f;
	}
	if (rise.jitterSeconds < 0.0f) {
		rise.jitterSeconds = 0.0f;
	}
	Log("Settings from %s: [Rise] Enabled=%d AfterSeconds=%.1f JitterSeconds=%.1f ChancePercent=%u OnlyGeneric=%d "
	    "OnlyPlayableRaces=%d OnlyHumans=%d AsCreature=%d DropSeconds=%.1f LieSeconds=%.1f MatchCorpse=%u SettleSeconds=%.1f HideCorpseAtPose=%d KeepCorpseApart=%d SoundAtGetUpPercent=%u HoldDeathSound=%d CloneBase=%d ZombieRace=%d "
	    "LookAfterSeconds=%.1f; [Zombies] Walk=%d SpeedMultiplier=%.2f SpeedRandomPercent=%u FamePerKills=%u AvoidChapels=%d RenameToWalker=%d Name=%s RenameVanilla=%d BlockMooResurrect=%d NoLevelingXP=%d NoSkillGain=%d; "
	    "[Replace] ExtraPercent=%u; [Horde] Enabled=%d JoinRadius=%.0f LeadUnits=%.0f StaleSeconds=%.0f TurnDegrees=%.0f GroupMost=%u TowardCities=%d CityPull=%.2f CityRange=%.0f GateHoldUnits=%.0f; "
	    "[Guards] Enabled=%d HealthFactor=%.2f ZombiesPerGuard=%u Radius=%.0f ClearSeconds=%.0f TestKillSeconds=%.0f; [Safeguard] Enabled=%d "
	    "AfterSeconds=%.0f Radius=%.0f; [News] Courier=%d RecentDays=%.1f HeadingHoldDays=%.2f",
	    path, rise.enabled ? 1 : 0, rise.afterSeconds, rise.jitterSeconds, rise.chancePercent,
	    rise.onlyGeneric ? 1 : 0, rise.onlyPlayableRaces ? 1 : 0, rise.onlyHumans ? 1 : 0, rise.asCreature ? 1 : 0,
	    rise.dropSeconds, rise.lieSeconds, static_cast<UInt32>(rise.matchCorpse), rise.settleSeconds, rise.hideCorpseAtPose ? 1 : 0, rise.keepCorpseApart ? 1 : 0, rise.soundAtGetUpPercent, rise.holdDeathSound ? 1 : 0, rise.cloneBase ? 1 : 0, rise.zombieRace ? 1 : 0, rise.lookAfterSeconds,
	    rise.walkOnly ? 1 : 0, rise.speedMultiplier, rise.speedRandomPercent, rise.famePerKills,
	    rise.avoidChapels ? 1 : 0, rise.renameToWalker ? 1 : 0, rise.walkerName, rise.renameVanilla ? 1 : 0, rise.blockMooResurrect ? 1 : 0,
	    rise.noLevelingXp ? 1 : 0, rise.noSkillGain ? 1 : 0,
	    replace.extraPercent, rise.horde.enabled ? 1 : 0, rise.horde.joinRadius, rise.horde.leadUnits,
	    rise.horde.staleSeconds, rise.horde.turnDegrees, rise.horde.groupMost, rise.horde.towardCities ? 1 : 0,
	    rise.horde.cityPull, rise.horde.cityRange, rise.horde.gateHoldUnits, rise.guards.enabled ? 1 : 0,
	    rise.guards.healthFactor, rise.guards.zombiesPerGuard, rise.guards.radius, rise.guards.clearSeconds,
	    rise.guards.testKillSeconds,
	    rise.safeguard.enabled ? 1 : 0, rise.safeguard.afterSeconds, rise.safeguard.radius,
	    rise.news.courier ? 1 : 0, rise.news.recentDays, rise.news.headingHoldDays);
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
