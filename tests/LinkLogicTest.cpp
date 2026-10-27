// Every flow of LINK's in-game configuration (docs/walkers-spec.md 4.18;
// Nadi, 2026-10-27: "ok passt für mich so"): the settings in the menu, their
// values both ways, what a game load does, and the two files LINK reads.

#include "LinkLogic.h"

#include <cstdio>
#include <cstring>
#include <string>

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf("  %s  %s\n", condition ? "ok  " : "FAIL", what);
	if (!condition) {
		++g_failures;
	}
}

using walkers::kLinkSettingCount;
using walkers::kLinkSettings;

int IndexOf(const char* var) {
	for (UInt32 i = 0; i < kLinkSettingCount; ++i) {
		if (std::strcmp(kLinkSettings[i].var, var) == 0) {
			return static_cast<int>(i);
		}
	}
	return -1;
}

void TestTable() {
	std::printf("The menu's settings\n");
	bool unique = true;
	bool prefixed = true;
	bool ranged = true;
	for (UInt32 i = 0; i < kLinkSettingCount; ++i) {
		for (UInt32 j = i + 1; j < kLinkSettingCount; ++j) {
			unique = unique && std::strcmp(kLinkSettings[i].var, kLinkSettings[j].var) != 0;
		}
		prefixed = prefixed && std::strncmp(kLinkSettings[i].var, "ini_", 4) == 0;
		ranged = ranged && kLinkSettings[i].min < kLinkSettings[i].max;
	}
	Check(kLinkSettingCount == 28, "28 settings: the 26 agreed, NoLevelingXP and NoSkillGain");
	Check(unique && prefixed && ranged, "each its own variable, named ini_..., a range from low to high");
	Check(std::strcmp(walkers::kLinkQuestEditorId, "Walkers") == 0 && walkers::kLinkQuestLocalId == 0x94B &&
	          walkers::kLinkScriptLocalId == 0x94C,
	      "the quest Walkers (0x94B) with its script (0x94C), right after the any-walker list");
}

void TestValues() {
	std::printf("Values both ways\n");
	walkers::RiseSettings rise;
	walkers::ReplaceSettings replace;
	const int health = IndexOf("ini_health_percent");
	const int enabled = IndexOf("ini_rise_enabled");
	const int speed = IndexOf("ini_speed_multiplier");
	const int maxCount = IndexOf("ini_max_count");
	const int courier = IndexOf("ini_courier");
	const int guardsHealth = IndexOf("ini_guards_health_factor");
	Check(health >= 0 && enabled >= 0 && speed >= 0 && maxCount >= 0 && courier >= 0 && guardsHealth >= 0,
	      "the settings are there by name");
	Check(walkers::LinkValue(rise, replace, health) == 15.0 && walkers::LinkValue(rise, replace, enabled) == 1.0 &&
	          walkers::LinkValue(rise, replace, maxCount) == 10.0,
	      "read off the settings: HealthPercent 15, Rise on, MaxCount 10");
	Check(walkers::SetLinkValue(rise, replace, health, 40.0) && rise.healthPercent == 40, "a new value: set, changed");
	Check(!walkers::SetLinkValue(rise, replace, health, 40.0), "the same again: no change");
	Check(walkers::SetLinkValue(rise, replace, health, 250.0) && rise.healthPercent == 100 &&
	          walkers::SetLinkValue(rise, replace, health, -5.0) && rise.healthPercent == 1,
	      "out of the slider's range: held to 1..100");
	Check(walkers::SetLinkValue(rise, replace, health, 33.6) && rise.healthPercent == 34, "a whole number rounded");
	Check(walkers::SetLinkValue(rise, replace, enabled, 0.0) && !rise.enabled &&
	          walkers::SetLinkValue(rise, replace, enabled, 1.0) && rise.enabled,
	      "a toggle: 0 off, 1 on");
	Check(walkers::SetLinkValue(rise, replace, speed, 1.5) && rise.speedMultiplier == 1.5f, "a fraction kept");
	Check(walkers::SetLinkValue(rise, replace, maxCount, 4.0) && replace.maxCount == 4, "a spawn setting");
	Check(walkers::SetLinkValue(rise, replace, courier, 0.0) && !rise.news.courier, "a news setting");
	Check(walkers::SetLinkValue(rise, replace, guardsHealth, 2.5) && rise.guards.healthFactor == 2.5f, "a guard setting");
	Check(!walkers::SetLinkValue(rise, replace, kLinkSettingCount, 1.0) &&
	          walkers::LinkValue(rise, replace, kLinkSettingCount) == 0.0,
	      "no such setting: nothing");
	bool roundTrip = true;
	walkers::RiseSettings other;
	walkers::ReplaceSettings otherReplace;
	for (UInt32 i = 0; i < kLinkSettingCount; ++i) {
		walkers::SetLinkValue(other, otherReplace, i, walkers::LinkValue(rise, replace, i));
		roundTrip = roundTrip && walkers::LinkValue(other, otherReplace, i) == walkers::LinkValue(rise, replace, i);
	}
	Check(roundTrip, "every setting read and set again comes back the same");
}

void TestEffects() {
	std::printf("What a change in the menu does\n");
	using walkers::LinkEffect;
	Check(kLinkSettings[IndexOf("ini_health_percent")].effect == LinkEffect::WalkerRecords &&
	          kLinkSettings[IndexOf("ini_speed_multiplier")].effect == LinkEffect::WalkerRecords,
	      "health and pace: written on the walker records anew");
	Check(kLinkSettings[IndexOf("ini_min_count")].effect == LinkEffect::SpawnCounts &&
	          kLinkSettings[IndexOf("ini_max_count")].effect == LinkEffect::SpawnCounts,
	      "the counts: the variant list's slots anew");
	Check(kLinkSettings[IndexOf("ini_replace_enabled")].effect == LinkEffect::NextStart &&
	          kLinkSettings[IndexOf("ini_extra_percent")].effect == LinkEffect::NextStart,
	      "the replacement itself and the extra spawns: at the next start (the lists are changed once a session)");
	Check(kLinkSettings[IndexOf("ini_horde_enabled")].effect == LinkEffect::Horde &&
	          kLinkSettings[IndexOf("ini_guards_enabled")].effect == LinkEffect::Horde &&
	          kLinkSettings[IndexOf("ini_courier")].effect == LinkEffect::Horde,
	      "horde, guards and news: configured anew");
	Check(kLinkSettings[IndexOf("ini_rise_enabled")].effect == LinkEffect::Live &&
	          kLinkSettings[IndexOf("ini_no_skill_gain")].effect == LinkEffect::Live,
	      "the rise and the skills: read where they are used");
}

void TestStart() {
	std::printf("A game load\n");
	using walkers::LinkStart;
	Check(walkers::StartOf(false, 1.0) == LinkStart::WriteIni && walkers::StartOf(false, 0.0) == LinkStart::WriteIni,
	      "no LINK: the INI's values into the variables, every load - the INI decides");
	Check(walkers::StartOf(true, 0.0) == LinkStart::WriteIni,
	      "LINK, a save without the variables (or a new game): the INI's values, once");
	Check(walkers::StartOf(true, walkers::kLinkVersion) == LinkStart::ReadLink,
	      "LINK and the variables set: LINK's values (the save's) decide");
	Check(walkers::StartOf(true, walkers::kLinkVersion + 1) == LinkStart::WriteIni,
	      "a version not this one: the INI's values again");
}

void TestFiles() {
	std::printf("The files LINK reads\n");
	walkers::RiseSettings rise;
	walkers::ReplaceSettings replace;
	static char text[1 << 16];
	const UInt32 ini = walkers::LinkIniText(rise, replace, text, sizeof(text));
	const std::string iniText(text, ini);
	Check(ini > 0 && iniText.find("set Walkers.ini_health_percent to 15\r\n") != std::string::npos &&
	          iniText.find("set Walkers.ini_speed_multiplier to 1.00\r\n") != std::string::npos,
	      "the INI: a set line per setting, the plugin INI's values (LINK's defaults button)");
	Check(iniText.find("set Walkers.ini_version to 2\r\n") != std::string::npos, "and the version last");
	const UInt32 cfg = walkers::LinkCfgText(text, sizeof(text));
	const std::string cfgText(text, cfg);
	Check(cfg > 0 && cfgText.find("set LINK.cfg_setting_name to sv_Construct \"Walkers.ini_health_percent\"") !=
	                     std::string::npos,
	      "the CFG: each setting by its variable");
	Check(cfgText.find("set LINK.cfg_setting_type to 0\r\nSetStage LINK 10") != std::string::npos &&
	          cfgText.find("set LINK.cfg_setting_type to 1\r\n") != std::string::npos &&
	          cfgText.find("set LINK.cfg_setting_type to 2\r\n") != std::string::npos,
	      "headers, toggles and sliders, each closed by SetStage LINK 10");
	Check(cfgText.find("\"Walkers.ini_version\"") == std::string::npos, "the version is not in the menu");
	Check(walkers::LinkCfgText(text, 16) == 0 && walkers::LinkIniText(rise, replace, text, 16) == 0,
	      "too small a buffer: nothing written");
}

}  // namespace

int main() {
	TestTable();
	TestValues();
	TestEffects();
	TestStart();
	TestFiles();
	if (g_failures != 0) {
		std::printf("%d failed\n", g_failures);
		return 1;
	}
	std::printf("all passed\n");
	return 0;
}
