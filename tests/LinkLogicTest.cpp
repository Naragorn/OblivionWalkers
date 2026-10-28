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
	Check(kLinkSettingCount == 68,
	      "68 settings: the 26 agreed, NoLevelingXP, NoSkillGain, the grab's three, the struggle's four, the 31 "
	      "advanced ones and the VR shake's two - every INI setting but Name, StruggleControl and the test and proof "
	      "switches");
	bool defaultsInRange = true;
	walkers::RiseSettings fresh;
	walkers::ReplaceSettings freshReplace;
	for (UInt32 i = 0; i < kLinkSettingCount; ++i) {
		const double v = walkers::LinkValue(fresh, freshReplace, i);
		defaultsInRange = defaultsInRange && v >= kLinkSettings[i].min && v <= kLinkSettings[i].max;
		if (!(v >= kLinkSettings[i].min && v <= kLinkSettings[i].max)) {
			std::printf("    out of range: %s = %g\n", kLinkSettings[i].var, v);
		}
	}
	Check(defaultsInRange, "every default lies inside its slider's range");
	static char cfg[1 << 16];
	Check(walkers::LinkCfgText(cfg, sizeof(cfg)) > 0, "the whole menu fits the 64 KB Rise.cpp writes it from");
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
	const int grab = IndexOf("ini_grab_enabled");
	const int grabChance = IndexOf("ini_grab_chance");
	const int bites = IndexOf("ini_grab_bites");
	Check(grab >= 0 && grabChance >= 0 && bites >= 0 && walkers::LinkValue(rise, replace, grab) == 1.0 &&
	          walkers::LinkValue(rise, replace, grabChance) == 50.0 && walkers::LinkValue(rise, replace, bites) == 3.0,
	      "the grab: on, a chance of 50, three bites - the INI's defaults");
	Check(walkers::SetLinkValue(rise, replace, grab, 0.0) && !rise.grab.enabled &&
	          walkers::SetLinkValue(rise, replace, grabChance, 80.0) && rise.grab.chancePercent == 80 &&
	          walkers::SetLinkValue(rise, replace, bites, 5.0) && rise.grab.bitesToDie == 5,
	      "the grab's settings set from the menu");
	Check(walkers::SetLinkValue(rise, replace, bites, 0.0) && rise.grab.bitesToDie == 1 &&
	          walkers::SetLinkValue(rise, replace, bites, 50.0) && rise.grab.bitesToDie == 10,
	      "the bites held to 1..10");
	const int presses = IndexOf("ini_grab_struggle_presses");
	const int crowd = IndexOf("ini_grab_struggle_crowd");
	const int fatigue = IndexOf("ini_grab_struggle_fatigue");
	const int messages = IndexOf("ini_grab_messages");
	Check(presses >= 0 && crowd >= 0 && fatigue >= 0 && messages >= 0 &&
	          walkers::LinkValue(rise, replace, presses) == 4.0 && walkers::LinkValue(rise, replace, crowd) == 0.5 &&
	          walkers::LinkValue(rise, replace, fatigue) == 5.0 && walkers::LinkValue(rise, replace, messages) == 1.0,
	      "the struggle: 4 presses, 0.5 per walker more, 5 fatigue a press, the lines on - the INI's defaults");
	Check(walkers::SetLinkValue(rise, replace, presses, 0.0) && rise.grab.strugglePresses == 0 &&
	          walkers::SetLinkValue(rise, replace, crowd, 1.0) && rise.grab.struggleCrowdStep == 1.0f &&
	          walkers::SetLinkValue(rise, replace, fatigue, 12.0) && rise.grab.struggleFatigue == 12.0f &&
	          walkers::SetLinkValue(rise, replace, messages, 0.0) && !rise.grab.messages,
	      "the struggle's settings set from the menu (presses 0: off)");
	const int strokes = IndexOf("ini_grab_shake_strokes");
	const int shakeFatigue = IndexOf("ini_grab_shake_fatigue");
	Check(strokes == 66 && shakeFatigue == 67 && walkers::LinkValue(rise, replace, strokes) == 6.0 &&
	          walkers::LinkValue(rise, replace, shakeFatigue) == 2.0,
	      "the VR shake: 6 strokes, 2 fatigue a stroke - the INI's defaults, after all the others");
	Check(walkers::SetLinkValue(rise, replace, strokes, 0.0) && rise.grab.shakeStrokes == 0 &&
	          walkers::SetLinkValue(rise, replace, shakeFatigue, 4.0) && rise.grab.shakeFatigue == 4.0f,
	      "the shake's settings set from the menu (strokes 0: off)");
	const int pull = IndexOf("ini_horde_city_pull");
	const int match = IndexOf("ini_rise_match_corpse");
	const int rename = IndexOf("ini_rename_to_walker");
	Check(pull >= 0 && walkers::SetLinkValue(rise, replace, pull, 0.25) && rise.horde.cityPull == 0.25f &&
	          match >= 0 && walkers::SetLinkValue(rise, replace, match, 1.0) && rise.matchCorpse == 1 &&
	          kLinkSettings[rename].effect == walkers::LinkEffect::NextStart &&
	          kLinkSettings[pull].effect == walkers::LinkEffect::Horde,
	      "an advanced one each way: the city pull (configured anew with the horde), the corpse match; the rename at "
	      "the next start");
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
	          kLinkSettings[IndexOf("ini_no_skill_gain")].effect == LinkEffect::Live &&
	          kLinkSettings[IndexOf("ini_grab_enabled")].effect == LinkEffect::Live &&
	          kLinkSettings[IndexOf("ini_grab_bites")].effect == LinkEffect::Live,
	      "the rise, the skills and the grab: read where they are used");
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
	Check(iniText.find("set Walkers.ini_version to 6\r\n") != std::string::npos, "and the version last");
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
