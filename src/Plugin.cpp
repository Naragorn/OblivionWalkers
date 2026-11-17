// The DLL's exports and real side effects: the log, the INI, the rise's
// installation. The decisions live in PluginEntry and RiseLogic.

#include "IniNames.h"
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

// The log kept open for the session (2026-11-13, StepTiming: a line was an
// open, a write and a close of the file - each a few hundred microseconds
// under Wine); each line flushed, so the OS holds it even if the game
// crashes.
FILE* g_logFile = nullptr;

void AppendLine(const char* format, va_list args) {
	AcquireSRWLockExclusive(&g_logLock);
	if (g_logFile == nullptr) {
		g_logFile = std::fopen(kLogFile, "a");
	}
	if (g_logFile != nullptr) {
		std::vfprintf(g_logFile, format, args);
		std::fputc('\n', g_logFile);
		std::fflush(g_logFile);
	}
	ReleaseSRWLockExclusive(&g_logLock);
}

void Log(const char* format, ...) {
	va_list args;
	va_start(args, format);
	AppendLine(format, args);
	va_end(args);
}

// The last sessions' logs kept in OblivionWalkers-logs, named by the time
// each was last written, the newest kKeptLogs of them (2026-11-10: a day of
// sessions on Windows left only the last two, the one with the bug gone).
constexpr const char* kLogDir = "OblivionWalkers-logs";
constexpr int kKeptLogs = 20;

int CompareNames(const void* a, const void* b) {
	return std::strcmp(static_cast<const char*>(a), static_cast<const char*>(b));
}

void KeepLog() {
	WIN32_FILE_ATTRIBUTE_DATA data;
	if (!GetFileAttributesExA(kLogFile, GetFileExInfoStandard, &data)) {
		return;
	}
	FILETIME local;
	SYSTEMTIME t;
	if (!FileTimeToLocalFileTime(&data.ftLastWriteTime, &local) || !FileTimeToSystemTime(&local, &t)) {
		return;
	}
	CreateDirectoryA(kLogDir, nullptr);
	char kept[MAX_PATH];
	std::snprintf(kept, sizeof(kept), "%s\\OblivionWalkers-%04u-%02u-%02u_%02u-%02u-%02u.log", kLogDir, t.wYear,
	              t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond);
	CopyFileA(kLogFile, kept, FALSE);
	static char names[256][64];
	int count = 0;
	WIN32_FIND_DATAA found;
	char pattern[MAX_PATH];
	std::snprintf(pattern, sizeof(pattern), "%s\\OblivionWalkers-*.log", kLogDir);
	HANDLE find = FindFirstFileA(pattern, &found);
	if (find == INVALID_HANDLE_VALUE) {
		return;
	}
	do {
		if (count < 256 && std::strlen(found.cFileName) < sizeof(names[0])) {
			std::snprintf(names[count++], sizeof(names[0]), "%s", found.cFileName);
		}
	} while (FindNextFileA(find, &found));
	FindClose(find);
	std::qsort(names, count, sizeof(names[0]), CompareNames);
	for (int i = 0; i + kKeptLogs < count; ++i) {
		char old[MAX_PATH];
		std::snprintf(old, sizeof(old), "%s\\%s", kLogDir, names[i]);
		DeleteFileA(old);
	}
}

// The last session's log survives one start as OblivionWalkers.log.prev,
// so a player's run can still be read after a harness run took the game;
// the last kKeptLogs in OblivionWalkers-logs.
void ResetLog() {
	AcquireSRWLockExclusive(&g_logLock);
	if (g_logFile != nullptr) {
		std::fclose(g_logFile);
		g_logFile = nullptr;
	}
	KeepLog();
	MoveFileExA(kLogFile, "OblivionWalkers.log.prev", MOVEFILE_REPLACE_EXISTING);
	g_logFile = std::fopen(kLogFile, "w");
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

// Whether `key` stands in `section`, with any value (an empty one too).
bool HasIniKey(const char* section, const char* key, const char* path) {
	char text[4] = {};
	GetPrivateProfileStringA(section, key, "\x01", text, sizeof(text), path);
	return !(text[0] == '\x01' && text[1] == '\0');
}

// The name a setting is read under (IniNames.h): its own, or the older one
// when an INI still has only that.
void NameToRead(const char*& section, const char*& key, const char* path) {
	const char* oldSection = nullptr;
	const char* oldKey = nullptr;
	if (!HasIniKey(section, key, path) && walkers::OldIniName(section, key, oldSection, oldKey) &&
	    HasIniKey(oldSection, oldKey, path)) {
		section = oldSection;
		key = oldKey;
	}
}

UINT IniInt(const char* section, const char* key, int fallback, const char* path) {
	NameToRead(section, key, path);
	return GetPrivateProfileIntA(section, key, fallback, path);
}

DWORD IniString(const char* section, const char* key, const char* fallback, char* out, DWORD size, const char* path) {
	NameToRead(section, key, path);
	return GetPrivateProfileStringA(section, key, fallback, out, size, path);
}

float IniFloat(const char* section, const char* key, float fallback, const char* path) {
	char text[64] = {};
	IniString(section, key, "", text, sizeof(text), path);
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
	rise.enabled = IniInt("Rise", "Enabled", 1, path) != 0;
	rise.afterSeconds = IniFloat("Rise", "AfterSeconds", 20.0f, path);
	rise.jitterSeconds = IniFloat("Rise", "RandomExtraSeconds", 10.0f, path);
	const int chance = IniInt("Rise", "ChancePercent", 100, path);
	rise.chancePercent = chance < 0 ? 0u : (chance > 100 ? 100u : static_cast<UInt32>(chance));
	rise.onlyGeneric = IniInt("Rise", "OnlyNamelessPeople", 1, path) != 0;
	rise.onlyPlayableRaces = IniInt("Rise", "OnlyPlayableRaces", 1, path) != 0;
	rise.onlyHumans = IniInt("Rise", "OnlyHumans", 0, path) != 0;
	rise.cloneBase = IniInt("Rise", "CloneBase", 0, path) != 0;
	rise.zombieRace = IniInt("Rise", "WalkerRace", 1, path) != 0;
	rise.lookAfterSeconds = IniFloat("Rise", "LookAfterSeconds", 3.0f, path);
	if (rise.lookAfterSeconds < 0.0f) {
		rise.lookAfterSeconds = 0.0f;
	}
	rise.asCreature = IniInt("Rise", "AsCreature", 1, path) != 0;
	rise.walkOnly = IniInt("Walkers", "WalkOnly", 1, path) != 0;
	const int health = IniInt("Walkers", "HealthPercent", 15, path);
	rise.healthPercent = health < 1 ? 1u : (health > 100 ? 100u : static_cast<UInt32>(health));
	rise.noLoot = IniInt("Walkers", "NoLoot", 1, path) != 0;
	rise.blockMooResurrect = IniInt("Walkers", "BlockMooResurrect", 1, path) != 0;
	rise.noLevelingXp = IniInt("Walkers", "NoLevelingXP", 1, path) != 0;
	rise.noSkillGain = IniInt("Walkers", "NoSkillGain", 1, path) != 0;
	rise.slowAggression.enabled = IniInt("Walkers", "SlowAggression", 1, path) != 0;
	char level[16] = {};
	IniString("Walkers", "SlowAggressionLevel", "Medium", level, sizeof(level), path);
	rise.slowAggression.level = walkers::ParseSlowAggressionLevel(level);
	rise.slowAggression.secondsMin = IniFloat("Walkers", "SlowAggressionSecondsMin", 5.0f, path);
	if (rise.slowAggression.secondsMin < 0.0f) {
		rise.slowAggression.secondsMin = 0.0f;
	}
	rise.slowAggression.secondsMax = IniFloat("Walkers", "SlowAggressionSecondsMax", 10.0f, path);
	if (rise.slowAggression.secondsMax < 0.0f) {
		rise.slowAggression.secondsMax = 0.0f;
	}
	rise.slowAggression.units = IniFloat("Walkers", "SlowAggressionDistance", 1000.0f, path);
	rise.slowAggression.calmSeconds = IniFloat("Walkers", "SlowAggressionCalmSeconds", 10.0f, path);
	rise.dungeonsOnlyChosen = IniInt("Dungeons", "OnlyChosen", 0, path) != 0;
	rise.dungeons.walkersAtOnce = static_cast<UInt32>(IniInt("Dungeons", "WalkersAtOnce", 6, path));
	rise.dungeons.journal = IniInt("Dungeons", "Journal", 1, path) != 0;
	rise.dungeons.rumors = IniInt("Dungeons", "Rumors", 1, path) != 0;
	rise.dungeons.messengerHours = IniFloat("Dungeons", "MessengerHours", 24.0f, path);
	rise.dungeons.beds = static_cast<UInt32>(IniInt("Dungeons", "Beds", 3, path));
	rise.dungeons.walkersOutsidePercent =
	    static_cast<UInt32>(IniInt("Dungeons", "WalkersOutsidePercent", 50, path));
	rise.dungeonsTestSiteState = static_cast<UInt32>(IniInt("Dungeons", "TestSiteState", 1, path));
	rise.stepTiming = IniInt("Debug", "StepTiming", 0, path) != 0;
	rise.combatTrace = IniInt("Debug", "CombatTrace", 0, path) != 0;
	rise.testLinkPage = static_cast<float>(IniInt("Debug", "TestLinkPage", 0, path));
	IniString("Debug", "SampleLine", "", rise.sampleLine, sizeof(rise.sampleLine), path);
	IniString("Dungeons", "TestSite", "", rise.dungeonsTestSite, sizeof(rise.dungeonsTestSite), path);
	rise.dungeons.testDaedra = IniInt("Dungeons", "TestDaedra", 0, path) != 0;
	rise.dungeons.enabled = IniInt("Dungeons", "Enabled", 1, path) != 0;
	rise.dungeons.activeMin = static_cast<UInt32>(IniInt("Dungeons", "ActiveMin", 3, path));
	rise.dungeons.activeMax = static_cast<UInt32>(IniInt("Dungeons", "ActiveMax", 5, path));
	rise.dungeons.weightRefuge = static_cast<UInt32>(IniInt("Dungeons", "WeightRefuge", 25, path));
	rise.dungeons.weightBesieged = static_cast<UInt32>(IniInt("Dungeons", "WeightBesieged", 25, path));
	rise.dungeons.weightToClear = static_cast<UInt32>(IniInt("Dungeons", "WeightToClear", 25, path));
	rise.dungeons.weightWalkerDungeon = static_cast<UInt32>(IniInt("Dungeons", "WeightWalkerDungeon", 25, path));
	rise.dungeons.spacingUnits = IniFloat("Dungeons", "DistanceApart", 15000.0f, path);
	rise.dungeons.newPlacesWithin = std::max(0.0f, IniFloat("Dungeons", "NewPlacesWithin", 60000.0f, path));
	rise.dungeons.peopleMin = static_cast<UInt32>(IniInt("Dungeons", "PeopleMin", 3, path));
	rise.dungeons.peopleMax = static_cast<UInt32>(IniInt("Dungeons", "PeopleMax", 6, path));
	rise.dungeons.attackChancePercent = static_cast<UInt32>(IniInt("Dungeons", "AttackChancePercent", 15, path));
	rise.dungeons.walkersMin = static_cast<UInt32>(IniInt("Dungeons", "AttackWalkersMin", 8, path));
	rise.dungeons.walkersMax = static_cast<UInt32>(IniInt("Dungeons", "AttackWalkersMax", 16, path));
	rise.dungeons.refugeeKillsPerHour = IniFloat("Dungeons", "RefugeeKillsPerHour", 2.0f, path);
	rise.dungeons.lossPercentPerHour = static_cast<UInt32>(IniInt("Dungeons", "LossPercentPerHour", 25, path));
	rise.dungeons.leaveAfterDays = IniFloat("Dungeons", "LeaveAfterDays", 7.0f, path);
	if (rise.slowAggression.units < 100.0f) {
		rise.slowAggression.units = 100.0f;
	}
	rise.speedMultiplier = IniFloat("Walkers", "SpeedMultiplier", 1.0f, path);
	const int spread = IniInt("Walkers", "PaceSpreadPercent", 30, path);
	rise.speedRandomPercent = spread < 0 ? 0u : (spread > 100 ? 100u : static_cast<UInt32>(spread));
	const int fame = IniInt("Walkers", "KillsPerFamePoint", 0, path);
	rise.famePerKills = fame < 0 ? 0u : static_cast<UInt32>(fame);
	rise.avoidChapels = IniInt("Walkers", "AvoidChapels", 1, path) != 0;
	rise.renameToWalker = IniInt("Walkers", "RenameToWalker", 1, path) != 0;
	IniString("Walkers", "Name", "Walker", rise.walkerName, sizeof(rise.walkerName), path);
	if (rise.walkerName[0] == '\0') {
		std::snprintf(rise.walkerName, sizeof(rise.walkerName), "%s", "Walker");
	}
	rise.renameVanilla = IniInt("Walkers", "RenameEveryZombie", 0, path) != 0;
	rise.noRiseInCities = IniInt("Rise", "NoRiseInCities", 0, path) != 0;
	rise.end.dieOnlyBeheaded = IniInt("Walkers", "DieOnlyBeheaded", 1, path) != 0;
	rise.end.riseAgainSeconds = IniFloat("Walkers", "RiseAgainSeconds", 20.0f, path);
	rise.end.riseAgainJitterSeconds = IniFloat("Walkers", "RiseAgainRandomSeconds", 10.0f, path);
	{
		const int others = IniInt("Walkers", "OthersKillsEndPercent", 100, path);
		rise.end.othersEndPercent = static_cast<UInt32>(others < 0 ? 0 : (others > 100 ? 100 : others));
	}
	// 4.22 [Invaders] (KindLogic.h): the spawn replacement and the rise follow it.
	rise.invaders.kind = walkers::KindFromIni(IniInt("Invaders", "Mode", 0, path));
	rise.invaders.daedraPercent = static_cast<UInt32>(std::max(0, std::min(100, static_cast<int>(IniInt("Invaders", "DaedraPercent", 50, path)))));
	rise.invaders.summons = IniInt("Invaders", "Summons", 0, path) != 0;
	rise.invaders.afterMainQuest = IniInt("Invaders", "DaedraAfterMainQuest", 0, path) != 0;
	rise.march.enabled = IniInt("Invaders", "MarchWeather", 0, path) != 0;
	rise.march.units = std::max(0.0f, IniFloat("Invaders", "MarchWeatherDistance", 12000.0f, path));
	// 4.22 step 3 (BandLogic.h): only out of the open gates, and the bands.
	rise.bands.origin = walkers::OriginFromIni(IniInt("Invaders", "OnlyFromOblivionGates", 0, path));
	rise.bands.everywhereUntilGatesOpen = IniInt("Invaders", "EverywhereUntilGatesOpen", 0, path) != 0;
	rise.bands.everyHours = static_cast<float>(std::max(1, std::min(72, static_cast<int>(IniInt("Invaders", "BandEveryHours", 12, path)))));
	rise.bands.sizeMin = static_cast<UInt32>(std::max(1, std::min(20, static_cast<int>(IniInt("Invaders", "BandSizeMin", 4, path)))));
	rise.bands.sizeMax = static_cast<UInt32>(std::max(1, std::min(30, static_cast<int>(IniInt("Invaders", "BandSizeMax", 9, path)))));
	rise.bands.siegePercentPerBand = static_cast<UInt32>(std::max(0, std::min(50, static_cast<int>(IniInt("Invaders", "BandSiegePercent", 5, path)))));
	rise.bands.siegeFadePercentPerDay = static_cast<float>(std::max(0, std::min(50, static_cast<int>(IniInt("Invaders", "BandSiegeFadePerDay", 5, path)))));
	rise.bands.testBandSeconds = static_cast<float>(IniInt("Invaders", "TestBandSeconds", 0, path));
	rise.bands.testKillLeaderSeconds = static_cast<float>(IniInt("Invaders", "TestKillLeaderSeconds", 0, path));
	rise.bands.testGateOpen = static_cast<int>(IniInt("Invaders", "TestGateOpen", -1, path));
	rise.bands.testGateOpenSeconds = static_cast<float>(IniInt("Invaders", "TestGateOpenSeconds", 0, path));
	// 4.22a step 5b: the plugin's dead Daedra back to Oblivion.
	rise.banish.seconds = static_cast<float>(std::max(0, std::min(600, static_cast<int>(IniInt("Invaders", "BanishSeconds", 10, path)))));
	char shader[16] = {};
	IniString("Invaders", "BanishShader", "0018B576", shader, sizeof(shader), path);
	rise.banish.shader = static_cast<UInt32>(std::strtoul(shader, nullptr, 16));
	rise.banish.shader = rise.banish.shader != 0 ? rise.banish.shader : walkers::kBanishShaderDefault;
	rise.bands.bandsMost = static_cast<UInt32>(std::max(1, std::min(static_cast<int>(walkers::kBandsMost), static_cast<int>(IniInt("Invaders", "BandsMost", 32, path)))));
	rise.headshots.arrows = IniInt("Headshots", "Arrows", 1, path) != 0;
	rise.headshots.bolts = IniInt("Headshots", "Bolts", 1, path) != 0;
	rise.headshots.headRadius = IniFloat("Headshots", "HeadRadius", 30.0f, path);
	rise.headshots.overkill = IniInt("Headshots", "Overkill", 1, path) != 0;
	rise.headshots.overkillPercent = std::min(500.0f, std::max(10.0f, IniFloat("Headshots", "OverkillPercent", 100.0f, path)));
	rise.headshots.fire = IniInt("Headshots", "Fire", 1, path) != 0;
	rise.headshots.fireSeconds = IniFloat("Headshots", "FireSeconds", 6.0f, path);
	rise.corpses.enabled = IniInt("Corpses", "Enabled", 1, path) != 0;
	rise.corpses.units = IniFloat("Corpses", "TakeAwayBeyond", 4000.0f, path);
	rise.corpses.lootUnits = IniFloat("Corpses", "TakeAwayWithThingsBeyond", 8000.0f, path);
	rise.corpses.countSeconds = IniFloat("Corpses", "LogSeconds", 60.0f, path);
	if (rise.end.riseAgainSeconds < 0.0f) {
		rise.end.riseAgainSeconds = 0.0f;
	}
	if (rise.end.riseAgainJitterSeconds < 0.0f) {
		rise.end.riseAgainJitterSeconds = 0.0f;
	}
	walkers::ReplaceSettings& replace = settings->replace;
	replace.enabled = IniInt("Replace", "Enabled", 1, path) != 0;
	replace.keepQuestLists = IniInt("Replace", "KeepQuestLists", 1, path) != 0;
	replace.bosses.keepBosses = IniInt("Replace", "KeepBosses", 1, path) != 0;
	replace.bosses.keepHard = IniInt("Replace", "KeepHardLists", 0, path) != 0;
	replace.bosses.keepHigh = IniInt("Replace", "KeepHighLists", 0, path) != 0;
	char testList[16];
	IniString("Replace", "TestLogList", "0", testList, sizeof(testList), path);
	replace.testLogList = static_cast<UInt32>(std::strtoul(testList, nullptr, 16));
	IniString("Replace", "BossListWords", replace.bosses.listWords, replace.bosses.listWords,
	                         sizeof(replace.bosses.listWords), path);
	IniString("Replace", "BossActorWords", replace.bosses.actorWords, replace.bosses.actorWords,
	                         sizeof(replace.bosses.actorWords), path);
	replace.headlessWalkers = walkers::SpawnsHeadless(rise.end.dieOnlyBeheaded);
	const int least = IniInt("Replace", "WalkersPerSpawnMin", 1, path);
	const int most = IniInt("Replace", "WalkersPerSpawnMax", 10, path);
	replace.minCount = least < 1 ? 1u : static_cast<UInt32>(least);
	replace.maxCount = most < 1 ? 1u : static_cast<UInt32>(most);
	const int extra = IniInt("Replace", "ExtraSpawnPercent", 0, path);
	replace.extraPercent = extra < 0 ? 0u : static_cast<UInt32>(extra);
	char categories[1024];
	UInt32 categoriesUsed = 0;
	for (UInt32 k = 0; k < walkers::kReplaceCategoryCount; ++k) {
		const walkers::ReplaceCategory& c = walkers::kReplaceCategories[k];
		replace.categories[k] = IniInt("ReplaceCategories", c.key, c.defaultOn ? 1 : 0, path) != 0;
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
	const int match = IniInt("Rise", "MatchCorpse", 2, path);
	rise.matchCorpse = static_cast<UInt8>(walkers::MatchCorpseFrom(match));
	rise.settleSeconds = IniFloat("Rise", "SettleSeconds", 1.5f, path);
	if (!(rise.settleSeconds >= 0.0f)) {
		rise.settleSeconds = 0.0f;
	}
	rise.hideCorpseAtPose = IniInt("Rise", "HideCorpseAtPose", 0, path) != 0;
	rise.keepCorpseApart = IniInt("Rise", "KeepCorpseApart", 1, path) != 0;
	rise.holdDeathSound = IniInt("Rise", "HoldDeathSound", 1, path) != 0;
	rise.horde.enabled = IniInt("Horde", "Enabled", 1, path) != 0;
	rise.horde.joinRadius = IniFloat("Horde", "JoinDistance", 1500.0f, path);
	rise.horde.leadUnits = IniFloat("Horde", "LeadDistance", 2000.0f, path);
	rise.horde.staleSeconds = IniFloat("Horde", "NewHeadingAfterSeconds", 30.0f, path);
	rise.horde.turnDegrees = IniFloat("Horde", "TurnDegrees", 60.0f, path);
	rise.horde.towardCities = IniInt("Horde", "TowardCities", 1, path) != 0;
	rise.horde.cityPull = IniFloat("Horde", "CityPull", 0.5f, path);
	rise.horde.cityPull = rise.horde.cityPull < 0.0f ? 0.0f : (rise.horde.cityPull > 1.0f ? 1.0f : rise.horde.cityPull);
	rise.horde.cityRange = IniFloat("Horde", "CityDistance", 40000.0f, path);
	rise.horde.gateHoldUnits = IniFloat("Horde", "HoldBeforeGateDistance", 800.0f, path);
	// 4.11c: a gate holds GateHordesMost groups, only with the player within
	// GateNearUnits; the rest wait out of the loaded grid (LoadedGrid).
	const int gateHordesMost = IniInt("Horde", "HordesPerGateMost", 1, path);
	rise.horde.gateHordesMost = gateHordesMost < 0 ? 0u : static_cast<UInt32>(gateHordesMost);
	rise.horde.gateNearUnits = IniFloat("Horde", "HoldOnlyWithinDistance", 8000.0f, path);
	const int loadedGrid = IniInt("Horde", "LoadedGrid", 5, path);
	rise.horde.loadedGrid = loadedGrid < 1 ? 1u : static_cast<UInt32>(loadedGrid);
	rise.horde.markerOnGround = IniInt("Horde", "MarkerOnGround", 1, path) != 0;
	const int groupMost = IniInt("Horde", "WalkersPerLeader", 8, path);
	rise.horde.groupMost = groupMost < 0 ? 0u : static_cast<UInt32>(groupMost);
	// 4.20.2: noise draws the hordes.
	walkers::NoiseSettings& noise = rise.horde.noise;
	noise.enabled = IniInt("Noise", "Enabled", 1, path) != 0;
	noise.radius = std::max(0.0f, IniFloat("Noise", "HeardWithin", 1200.0f, path));
	noise.searchSeconds = std::max(0.0f, IniFloat("Noise", "SearchSeconds", 30.0f, path));
	noise.travelSeconds = std::max(0.0f, IniFloat("Noise", "TravelSeconds", 60.0f, path));
	// 4.20.6: the survivors' camps.
	walkers::CampSettings& camps = rise.camps;
	const auto whole = [path](const char* key, int fallback, int least, int most) {
		const int v = IniInt("Survivors", key, fallback, path);
		return static_cast<UInt32>(v < least ? least : (v > most ? most : v));
	};
	camps.enabled = IniInt("Survivors", "Enabled", 1, path) != 0;
	camps.campCount = whole("CampCount", static_cast<int>(walkers::kCampMost), 1, static_cast<int>(walkers::kCampMost));
	camps.spacingUnits = std::max(0.0f, IniFloat("Survivors", "CampDistanceApart", 10000.0f, path));
	camps.peoplePerCamp = whole("PeoplePerCamp", 6, 2, 6);
	camps.womenPercent = whole("WomenPercent", 50, 0, 100);
	camps.trader = IniInt("Survivors", "Trader", 1, path) != 0;
	camps.rewardFame = IniInt("Survivors", "RewardFame", 1, path) != 0;
	camps.famePoints = whole("FamePoints", 1, 0, 100);
	camps.rewardGold = IniInt("Survivors", "RewardGold", 1, path) != 0;
	camps.gold = whole("Gold", 150, 0, 100000);
	camps.attackChancePercent = whole("AttackChancePercent", 15, 0, 100);
	camps.walkersMin = whole("AttackWalkersMin", 6, 1, 100);
	camps.walkersMax = whole("AttackWalkersMax", 12, 1, 100);
	camps.campsAttackedAtOnce = whole("CampsAttackedAtOnce", 1, 1, 2);
	camps.refugeeKillsPerHour = std::max(0.0f, IniFloat("Survivors", "RefugeeKillsPerHour", 2.0f, path));
	camps.lossPercentPerHour = whole("LossPercentPerHour", 25, 0, 100);
	camps.returnChancePercent = whole("ReturnChancePercent", 10, 0, 100);
	camps.rewardThings = IniInt("Survivors", "RewardThings", 1, path) != 0;
	camps.escort = IniInt("Survivors", "Escort", 1, path) != 0;
	camps.escortFame = whole("EscortFame", 2, 0, 100);
	camps.escortGold = whole("EscortGold", 300, 0, 100000);
	camps.escortBehindUnits = static_cast<float>(whole("EscortLeftBehindDistance", 5000, 500, 100000));
	camps.escortStayDays = static_cast<float>(whole("EscortStayDays", 7, 1, 365));
	camps.escortIntoCity = IniInt("Survivors", "EscortIntoCity", 1, path) != 0;
	camps.leashUnits = std::max(0.0f, IniFloat("Survivors", "LeadStraysHomeBeyond", 1500.0f, path));
	camps.testCamp = IniInt("Survivors", "TestCamp", -1, path);
	camps.testDaedra = IniInt("Survivors", "TestDaedra", 0, path) != 0;
	camps.testLeash = IniInt("Survivors", "TestLeash", -1, path);
	rise.disguise.enabled = IniInt("Disguise", "Enabled", 1, path) != 0;
	rise.disguise.seconds = std::max(1.0f, IniFloat("Disguise", "Seconds", 180.0f, path));
	rise.daedric.enabled = IniInt("Disguise", "Daedric", 1, path) != 0;
	rise.daedric.nearUnits = std::max(0.0f, IniFloat("Disguise", "DaedricNearDistance", 200.0f, path));
	rise.daedric.forgetSeconds = std::max(0.0f, IniFloat("Disguise", "DaedricForgetSeconds", 60.0f, path));
	rise.disguiseTest = IniInt("Disguise", "Test", 0, path) != 0;
	camps.testEscort = IniInt("Survivors", "TestEscort", -1, path);
	camps.testEscortBehind = IniInt("Survivors", "TestEscortBehind", 0, path) != 0;
	camps.escortFollowSeconds = std::max(0.0f, IniFloat("Survivors", "EscortFollowSeconds", 5.0f, path));
	rise.guards.enabled = IniInt("Guards", "Enabled", 1, path) != 0;
	rise.guards.healthFactor = IniFloat("Guards", "HealthFactor", 5.0f, path);
	rise.guards.healthFactor = rise.guards.healthFactor < 0.0f ? 0.0f : rise.guards.healthFactor;
	const int perGuard = IniInt("Guards", "WalkersPerGuard", 5, path);
	rise.guards.zombiesPerGuard = perGuard < 1 ? 1u : static_cast<UInt32>(perGuard);
	rise.guards.radius = IniFloat("Guards", "GateDistance", 4000.0f, path);
	rise.guards.clearSeconds = IniFloat("Guards", "GoInAfterSeconds", 30.0f, path);
	rise.guards.testKillSeconds = IniFloat("Guards", "TestKillSeconds", 0.0f, path);
	rise.guards.legionUnits = std::max(0.0f, IniFloat("Guards", "LegionDistance", 16000.0f, path));
	walkers::SiegeSettings& siege = rise.siege;
	siege.enabled = IniInt("Siege", "Enabled", 1, path) != 0;
	siege.walkersAtGate = static_cast<UInt32>(std::max(12, std::min(100, static_cast<int>(IniInt("Siege", "WalkersAtGate", 24, path)))));
	siege.chancePerDayPercent = static_cast<UInt32>(std::max(0, std::min(100, static_cast<int>(IniInt("Siege", "ChancePerDayPercent", 10, path)))));
	siege.walkersMin = static_cast<UInt32>(std::max(1, static_cast<int>(IniInt("Siege", "WalkersMin", 40, path))));
	siege.walkersMax = static_cast<UInt32>(std::max(1, static_cast<int>(IniInt("Siege", "WalkersMax", 120, path))));
	siege.pauseDays = std::max(0.0f, IniFloat("Siege", "PauseDays", 3.0f, path));
	siege.guardKillsPerHour = std::max(0.0f, IniFloat("Siege", "GuardKillsPerHour", 2.0f, path));
	siege.testGate = static_cast<int>(IniInt("Siege", "TestGate", -1, path));
	siege.testGate = siege.testGate >= static_cast<int>(walkers::kCityGateCount) ? -1 : siege.testGate;
	siege.testEndSeconds = std::max(0.0f, IniFloat("Siege", "TestEndSeconds", 0.0f, path));
	char door[16] = {};
	IniString("Siege", "TestReadDoor", "0", door, sizeof(door), path);
	siege.testReadDoor = static_cast<UInt32>(std::strtoul(door, nullptr, 16));
	// 4.22a step 5a: a Daedra siege's counts.
	siege.daedraAtGate = static_cast<UInt32>(std::max(1, std::min(static_cast<int>(walkers::kSiegeDaedraAtGateMost), static_cast<int>(IniInt("Siege", "DaedraAtGate", 8, path)))));
	siege.daedraMin = static_cast<UInt32>(std::max(1, std::min(300, static_cast<int>(IniInt("Siege", "DaedraMin", 10, path)))));
	siege.daedraMax = static_cast<UInt32>(std::max(1, std::min(300, static_cast<int>(IniInt("Siege", "DaedraMax", 30, path)))));
	siege.testDaedra = IniInt("Siege", "TestDaedra", 0, path) != 0;
	rise.safeguard.enabled = IniInt("Safeguard", "Enabled", 1, path) != 0;
	rise.safeguard.raiseAtHome = IniInt("Safeguard", "RaiseAtHome", 1, path) != 0;
	walkers::NightSettings& night = rise.night;
	night.enabled = IniInt("Night", "Enabled", 1, path) != 0;
	night.speedMultiplier = std::max(0.1f, std::min(5.0f, IniFloat("Night", "SpeedMultiplier", 1.3f, path)));
	night.spawnPercent = static_cast<UInt32>(std::max(10, std::min(500, static_cast<int>(IniInt("Night", "SpawnPercent", 150, path)))));
	night.hordeLeadPercent = static_cast<UInt32>(std::max(10, std::min(500, static_cast<int>(IniInt("Night", "HordeRoamPercent", 150, path)))));
	night.startHour = IniFloat("Night", "StartHour", 20.0f, path);
	night.endHour = IniFloat("Night", "EndHour", 6.0f, path);
	night.rampHours = std::max(0.0f, IniFloat("Night", "TwilightHours", 1.0f, path));
	char victim[16] = {};
	IniString("Safeguard", "TestVictim", "0", victim, sizeof(victim), path);
	rise.safeguard.testVictim = static_cast<UInt32>(std::strtoul(victim, nullptr, 16));
	rise.safeguard.afterSeconds = IniFloat("Safeguard", "AfterSeconds", 15.0f, path);
	rise.safeguard.afterSeconds = rise.safeguard.afterSeconds < 0.0f ? 0.0f : rise.safeguard.afterSeconds;
	rise.safeguard.radius = IniFloat("Safeguard", "NoWalkerWithin", 3000.0f, path);
	rise.news.courier = IniInt("News", "Courier", 1, path) != 0;
	rise.news.recentDays = IniFloat("News", "RecentDays", 3.0f, path);
	rise.news.recentDays = rise.news.recentDays < 0.0f ? 0.0f : rise.news.recentDays;
	rise.news.headingHoldDays = IniFloat("News", "HeadingHoldDays", 0.5f, path);
	rise.news.headingHoldDays = rise.news.headingHoldDays < 0.0f ? 0.0f : rise.news.headingHoldDays;
	rise.grab.enabled = IniInt("Grab", "Enabled", 1, path) != 0;
	const int grabChance = IniInt("Grab", "ChancePercent", 66, path);
	rise.grab.chancePercent = grabChance < 0 ? 0u : (grabChance > 100 ? 100u : static_cast<UInt32>(grabChance));
	const int bites = IniInt("Grab", "BitesToDie", 3, path);
	rise.grab.bitesToDie = bites < 1 ? 1u : static_cast<UInt32>(bites);
	rise.grab.biteSeconds = IniFloat("Grab", "BiteSeconds", 4.0f, path);
	rise.grab.biteSeconds = rise.grab.biteSeconds < 0.5f ? 0.5f : rise.grab.biteSeconds;
	const int biteHealth = IniInt("Grab", "BiteHealthPercent", 20, path);
	rise.grab.biteHealthPercent = biteHealth < 0 ? 0u : (biteHealth > 100 ? 100u : static_cast<UInt32>(biteHealth));
	const int struggle = IniInt("Grab", "StruggleHits", 2, path);
	rise.grab.struggleHits = struggle < 1 ? 1u : static_cast<UInt32>(struggle);
	rise.grab.tearFreeDistance = IniFloat("Grab", "TearFreeDistance", 1000.0f, path);
	rise.grab.reachUnits = std::max(0.0f, IniFloat("Grab", "ReachUnits", 37.0f, path));
	rise.grab.lungeUnits = std::max(0.0f, IniFloat("Grab", "LungeUnits", 100.0f, path));
	rise.grab.lungeSeconds = std::max(0.0f, IniFloat("Grab", "LungeSeconds", 0.3f, path));
	rise.grab.biteResetSeconds = IniFloat("Grab", "BitesForgottenAfterSeconds", 30.0f, path);
	rise.grab.testGrabSeconds = IniFloat("Grab", "TestGrabSeconds", 0.0f, path);
	const int shock = IniInt("Grab", "ShockSound", 1, path);
	rise.grab.shockSound = shock < 0 ? 0u : (shock > 3 ? 3u : static_cast<UInt32>(shock));
	const int presses = IniInt("Grab", "StrugglePresses", 4, path);
	rise.grab.strugglePresses = presses < 0 ? 0u : static_cast<UInt32>(presses);
	rise.grab.struggleGapSeconds = IniFloat("Grab", "SecondsBetweenPresses", 0.8f, path);
	rise.grab.struggleCrowdStep = IniFloat("Grab", "HarderPerWalkerPercent", 0.5f, path);
	rise.grab.struggleCrowdMost = IniFloat("Grab", "CrowdMost", 3.0f, path);
	rise.grab.struggleCrowdMost = rise.grab.struggleCrowdMost < 1.0f ? 1.0f : rise.grab.struggleCrowdMost;
	rise.grab.struggleCrowdRadius = IniFloat("Grab", "CrowdDistance", 300.0f, path);
	rise.grab.struggleFatigue = IniFloat("Grab", "StruggleFatigue", 5.0f, path);
	rise.grab.messages = IniInt("Grab", "Messages", 1, path) != 0;
	const int strokes = IniInt("Grab", "ShakeStrokes", 6, path);
	rise.grab.shakeStrokes = strokes < 0 ? 0u : static_cast<UInt32>(strokes);
	rise.grab.shakeFatigue = IniFloat("Grab", "ShakeFatigue", 2.0f, path);
	const int control = IniInt("Grab", "StruggleControl", 6, path);
	rise.grab.struggleControl = control < 0 || control > 28 ? 6u : static_cast<UInt32>(control);
	const int getUpSound = IniInt("Rise", "GroanChancePercent", 60, path);
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
	    "OnlyPlayableRaces=%d OnlyHumans=%d NoRiseInCities=%d AsCreature=%d DropSeconds=%.1f LieSeconds=%.1f MatchCorpse=%u SettleSeconds=%.1f HideCorpseAtPose=%d KeepCorpseApart=%d SoundAtGetUpPercent=%u HoldDeathSound=%d CloneBase=%d ZombieRace=%d "
	    "LookAfterSeconds=%.1f; [Zombies] Walk=%d SpeedMultiplier=%.2f SpeedRandomPercent=%u FamePerKills=%u AvoidChapels=%d RenameToWalker=%d Name=%s RenameVanilla=%d BlockMooResurrect=%d NoLevelingXP=%d NoSkillGain=%d DieOnlyBeheaded=%d RiseAgainSeconds=%.0f RiseAgainJitterSeconds=%.0f; "
	    "[Replace] ExtraPercent=%u; [Horde] Enabled=%d JoinRadius=%.0f LeadUnits=%.0f StaleSeconds=%.0f TurnDegrees=%.0f GroupMost=%u TowardCities=%d CityPull=%.2f CityRange=%.0f GateHoldUnits=%.0f; "
	    "[Guards] Enabled=%d HealthFactor=%.2f ZombiesPerGuard=%u Radius=%.0f ClearSeconds=%.0f TestKillSeconds=%.0f "
	    "LegionDistance=%.0f; [Safeguard] Enabled=%d "
	    "AfterSeconds=%.0f Radius=%.0f; [News] Courier=%d RecentDays=%.1f HeadingHoldDays=%.2f",
	    path, rise.enabled ? 1 : 0, rise.afterSeconds, rise.jitterSeconds, rise.chancePercent,
	    rise.onlyGeneric ? 1 : 0, rise.onlyPlayableRaces ? 1 : 0, rise.onlyHumans ? 1 : 0, rise.noRiseInCities ? 1 : 0, rise.asCreature ? 1 : 0,
	    rise.dropSeconds, rise.lieSeconds, static_cast<UInt32>(rise.matchCorpse), rise.settleSeconds, rise.hideCorpseAtPose ? 1 : 0, rise.keepCorpseApart ? 1 : 0, rise.soundAtGetUpPercent, rise.holdDeathSound ? 1 : 0, rise.cloneBase ? 1 : 0, rise.zombieRace ? 1 : 0, rise.lookAfterSeconds,
	    rise.walkOnly ? 1 : 0, rise.speedMultiplier, rise.speedRandomPercent, rise.famePerKills,
	    rise.avoidChapels ? 1 : 0, rise.renameToWalker ? 1 : 0, rise.walkerName, rise.renameVanilla ? 1 : 0, rise.blockMooResurrect ? 1 : 0,
	    rise.noLevelingXp ? 1 : 0, rise.noSkillGain ? 1 : 0, rise.end.dieOnlyBeheaded ? 1 : 0,
	    rise.end.riseAgainSeconds, rise.end.riseAgainJitterSeconds,
	    replace.extraPercent, rise.horde.enabled ? 1 : 0, rise.horde.joinRadius, rise.horde.leadUnits,
	    rise.horde.staleSeconds, rise.horde.turnDegrees, rise.horde.groupMost, rise.horde.towardCities ? 1 : 0,
	    rise.horde.cityPull, rise.horde.cityRange, rise.horde.gateHoldUnits, rise.guards.enabled ? 1 : 0,
	    rise.guards.healthFactor, rise.guards.zombiesPerGuard, rise.guards.radius, rise.guards.clearSeconds,
	    rise.guards.testKillSeconds, rise.guards.legionUnits,
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
