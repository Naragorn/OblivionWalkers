// Every flow of LINK's in-game configuration (docs/walkers-spec.md 4.18;
// Nadi, 2026-10-27: "ok passt für mich so"): the settings in the menu, their
// values both ways, what a game load does, and the two files LINK reads.

#include "LinkLogic.h"

#include <cmath>
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
using walkers::LinkSetting;

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
	Check(kLinkSettingCount == 166,
	      "166 settings: the 26 agreed, NoLevelingXP, NoSkillGain, the grab's three, the struggle's four, the 31 "
	      "advanced ones, the VR shake's two, the 45 replacement categories, 4.17's three, the siege's seven, the grab's reach and lunge, NoRiseInCities and RaiseAtHome, the night's six, the noise's three, the camps' fifteen, the camps' count, the grab's shock sound, the slow aggression's five, KeepQuestLists, the camps' things and the escort's five - every INI setting but Name, CampSpacingUnits, "
	      "StruggleControl, the lists and the test and proof switches");
	bool categories = walkers::kLinkCategoryFirst == 68;
	for (UInt32 k = 0; k < walkers::kReplaceCategoryCount; ++k) {
		std::string var = "ini_cat_";
		for (const char* p = walkers::kReplaceCategories[k].key; *p != '\0'; ++p) {
			var += static_cast<char>(*p >= 'A' && *p <= 'Z' ? *p + 32 : *p);
		}
		const LinkSetting& s = kLinkSettings[walkers::kLinkCategoryFirst + k];
		categories = categories && var == s.var && s.kind == walkers::LinkKind::Toggle &&
		             std::strcmp(s.title, walkers::kReplaceCategories[k].title) == 0;
	}
	Check(categories,
	      "from 68 on a toggle per category, in the table's order: ini_cat_<key in small letters>, its title");
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
	static char cfg[1 << 17];
	Check(walkers::LinkCfgText(cfg, sizeof(cfg)) > 0, "the whole menu fits the 128 KB Rise.cpp writes it from (64 KB was passed at 128 settings)");
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
	          walkers::LinkValue(rise, replace, grabChance) == 66.0 && walkers::LinkValue(rise, replace, bites) == 3.0,
	      "the grab: on, a chance of 66 (Nadi, 2026-11-04), three bites - the INI's defaults");
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
	const int beheaded = IndexOf("ini_die_only_beheaded");
	const int again = IndexOf("ini_rise_again");
	const int againJitter = IndexOf("ini_rise_again_jitter");
	Check(beheaded == 113 && again == 114 && againJitter == 115 && walkers::LinkValue(rise, replace, beheaded) == 1.0 &&
	          walkers::LinkValue(rise, replace, again) == 20.0 && walkers::LinkValue(rise, replace, againJitter) == 10.0,
	      "4.17 after the categories: DieOnlyBeheaded on, 20 s and 10 s - the INI's defaults");
	Check(walkers::SetLinkValue(rise, replace, beheaded, 0.0) && !rise.end.dieOnlyBeheaded && replace.headlessWalkers &&
	          walkers::SetLinkValue(rise, replace, again, 45.0) && rise.end.riseAgainSeconds == 45.0f &&
	          walkers::SetLinkValue(rise, replace, againJitter, 5.0) && rise.end.riseAgainJitterSeconds == 5.0f,
	      "set from the menu; the rule off lets the headless walkers into the spawns again (at the next start)");
	const int siegeOn = IndexOf("ini_siege_enabled");
	const int atGate = IndexOf("ini_siege_at_gate");
	const int toll = IndexOf("ini_siege_toll");
	Check(siegeOn == 116 && atGate == 117 && toll == 122 && walkers::LinkValue(rise, replace, siegeOn) == 1.0 &&
	          walkers::LinkValue(rise, replace, atGate) == 24.0 &&
	          walkers::LinkValue(rise, replace, IndexOf("ini_siege_chance")) == 10.0 &&
	          walkers::LinkValue(rise, replace, IndexOf("ini_siege_min")) == 40.0 &&
	          walkers::LinkValue(rise, replace, IndexOf("ini_siege_max")) == 120.0 &&
	          walkers::LinkValue(rise, replace, IndexOf("ini_siege_pause")) == 3.0 &&
	          walkers::LinkValue(rise, replace, toll) == 2.0,
	      "4.12 last: on, 24 at the gate, 10 percent a day, 40 to 120, a pause of 3 days, 2 a game hour");
	Check(walkers::SetLinkValue(rise, replace, atGate, 5.0) && rise.siege.walkersAtGate == 12 &&
	          walkers::SetLinkValue(rise, replace, toll, 0.0) && rise.siege.guardKillsPerHour == 0.0f &&
	          walkers::SetLinkValue(rise, replace, siegeOn, 0.0) && !rise.siege.enabled,
	      "set from the menu, held to the range (12 at the gate at least)");
	const int reach = IndexOf("ini_grab_reach");
	const int lunge = IndexOf("ini_grab_lunge");
	const int lungeTime = IndexOf("ini_grab_lunge_seconds");
	Check(reach == 123 && lunge == 124 && lungeTime == 125 && walkers::LinkValue(rise, replace, reach) == 37.0 &&
	          walkers::LinkValue(rise, replace, lunge) == 100.0 &&
	          std::fabs(walkers::LinkValue(rise, replace, lungeTime) - 0.3) < 1e-6 &&
	          walkers::SetLinkValue(rise, replace, lunge, 0.0) && rise.grab.lungeUnits == 0.0f,
	      "the arm's reach 37, the lunge 100 units (0.3 s for 60); the lunge 0 from the menu");
	const int cities = IndexOf("ini_rise_no_cities");
	Check(cities == 126 && walkers::LinkValue(rise, replace, cities) == 0.0 &&
	          walkers::SetLinkValue(rise, replace, cities, 1.0) && rise.noRiseInCities,
	      "no rise in cities, off by default, on from the menu");
	const int atHome = IndexOf("ini_safeguard_at_home");
	Check(atHome == 127 && walkers::LinkValue(rise, replace, atHome) == 1.0,
	      "raised at home, on by default");
	const int nightOn = IndexOf("ini_night_enabled");
	const int nightSpeed = IndexOf("ini_night_speed");
	const int nightSpawns = IndexOf("ini_night_spawns");
	const int nightLead = IndexOf("ini_night_horde_lead");
	const int nightStart = IndexOf("ini_night_start");
	const int nightEnd = IndexOf("ini_night_end");
	Check(nightOn == 128 && nightSpeed == 129 && nightSpawns == 130 && nightLead == 131 && nightStart == 132 &&
	          nightEnd == 133 && walkers::LinkValue(rise, replace, nightOn) == 1.0 &&
	          std::fabs(walkers::LinkValue(rise, replace, nightSpeed) - 1.3) < 1e-6 &&
	          walkers::LinkValue(rise, replace, nightSpawns) == 150.0 &&
	          walkers::LinkValue(rise, replace, nightLead) == 150.0 &&
	          walkers::LinkValue(rise, replace, nightStart) == 20.0 && walkers::LinkValue(rise, replace, nightEnd) == 6.0,
	      "the nights last: on, x1.3, 150 %, 150 %, 20:00 to 6:00 - the INI's defaults");
	Check(walkers::SetLinkValue(rise, replace, nightOn, 0.0) && !rise.night.enabled &&
	          walkers::SetLinkValue(rise, replace, nightSpeed, 1.6) && std::fabs(rise.night.speedMultiplier - 1.6f) < 1e-6f &&
	          walkers::SetLinkValue(rise, replace, nightStart, 22.0) && rise.night.startHour == 22.0f &&
	          kLinkSettings[nightOn].effect == walkers::LinkEffect::Live &&
	          kLinkSettings[nightSpeed].effect == walkers::LinkEffect::Live,
	      "the nights off and quicker from the menu, live: the night's step applies what changed");
	const int noiseOn = IndexOf("ini_noise_enabled");
	const int noiseRadius = IndexOf("ini_noise_radius");
	const int noiseSearch = IndexOf("ini_noise_search");
	Check(noiseOn == 134 && noiseRadius == 135 && noiseSearch == 136 && walkers::LinkValue(rise, replace, noiseOn) == 1.0 &&
	          walkers::LinkValue(rise, replace, noiseRadius) == 1200.0 &&
	          walkers::LinkValue(rise, replace, noiseSearch) == 30.0,
	      "the noise last: on, heard within 1200, searched 30 s - the INI's defaults");
	Check(walkers::SetLinkValue(rise, replace, noiseOn, 0.0) && !rise.horde.noise.enabled &&
	          walkers::SetLinkValue(rise, replace, noiseRadius, 2500.0) && rise.horde.noise.radius == 2500.0f &&
	          walkers::SetLinkValue(rise, replace, noiseSearch, 45.0) && rise.horde.noise.searchSeconds == 45.0f &&
	          kLinkSettings[noiseOn].effect == walkers::LinkEffect::Horde &&
	          kLinkSettings[noiseRadius].effect == walkers::LinkEffect::Horde,
	      "the noise off, farther, longer from the menu: the horde configured anew with it");
	const char* campVars[] = {"ini_camps_enabled", "ini_camps_people", "ini_camps_women", "ini_camps_trader",
	                          "ini_camps_fame", "ini_camps_fame_points", "ini_camps_gold", "ini_camps_gold_amount",
	                          "ini_camps_attack_chance", "ini_camps_walkers_min", "ini_camps_walkers_max",
	                          "ini_camps_at_once", "ini_camps_kills", "ini_camps_loss", "ini_camps_return"};
	const double campDefaults[] = {1, 6, 50, 1, 1, 1, 1, 150, 15, 6, 12, 1, 2, 25, 10};
	bool campsInOrder = true;
	for (int k = 0; k < 15; ++k) {
		const int at = IndexOf(campVars[k]);
		campsInOrder = campsInOrder && at == 137 + k && walkers::LinkValue(rise, replace, at) == campDefaults[k] &&
		               kLinkSettings[at].effect == walkers::LinkEffect::Live;
	}
	Check(campsInOrder, "the camps last, 137..151: on, 6 refugees, 50 % women, a trader, 1 fame, 150 gold, 15 % a day, "
	                    "6..12 walkers, one at once, 2 kills and 25 % lost an hour away, 10 % back a day - live");
	Check(walkers::SetLinkValue(rise, replace, 137, 0.0) && !rise.camps.enabled &&
	          walkers::SetLinkValue(rise, replace, 138, 4.0) && rise.camps.peoplePerCamp == 4 &&
	          walkers::SetLinkValue(rise, replace, 139, 70.0) && rise.camps.womenPercent == 70 &&
	          walkers::SetLinkValue(rise, replace, 140, 0.0) && !rise.camps.trader &&
	          walkers::SetLinkValue(rise, replace, 141, 0.0) && !rise.camps.rewardFame &&
	          walkers::SetLinkValue(rise, replace, 142, 3.0) && rise.camps.famePoints == 3 &&
	          walkers::SetLinkValue(rise, replace, 143, 0.0) && !rise.camps.rewardGold &&
	          walkers::SetLinkValue(rise, replace, 144, 300.0) && rise.camps.gold == 300 &&
	          walkers::SetLinkValue(rise, replace, 145, 40.0) && rise.camps.attackChancePercent == 40 &&
	          walkers::SetLinkValue(rise, replace, 146, 3.0) && rise.camps.walkersMin == 3 &&
	          walkers::SetLinkValue(rise, replace, 147, 20.0) && rise.camps.walkersMax == 20 &&
	          walkers::SetLinkValue(rise, replace, 148, 2.0) && rise.camps.campsAttackedAtOnce == 2 &&
	          walkers::SetLinkValue(rise, replace, 149, 3.5) && rise.camps.refugeeKillsPerHour == 3.5f &&
	          walkers::SetLinkValue(rise, replace, 150, 50.0) && rise.camps.lossPercentPerHour == 50 &&
	          walkers::SetLinkValue(rise, replace, 151, 30.0) && rise.camps.returnChancePercent == 30,
	      "each of the camps' fifteen set from the menu");
	const int campCount = IndexOf("ini_camps_count");
	Check(campCount == 152 && walkers::LinkValue(rise, replace, campCount) == 64.0 &&
	          kLinkSettings[campCount].min == 1 && kLinkSettings[campCount].max == 64 &&
	          kLinkSettings[campCount].effect == walkers::LinkEffect::Live &&
	          walkers::SetLinkValue(rise, replace, campCount, 20.0) && rise.camps.campCount == 20,
	      "the camps' count, 152: 64 by default, 1..64, live; 20 set from the menu");
	const int shock = IndexOf("ini_grab_shock_sound");
	Check(shock == 153 && walkers::LinkValue(rise, replace, shock) == 1.0 && kLinkSettings[shock].min == 0 &&
	          kLinkSettings[shock].max == 3 && kLinkSettings[shock].effect == walkers::LinkEffect::Live &&
	          walkers::SetLinkValue(rise, replace, shock, 3.0) && rise.grab.shockSound == 3,
	      "the grab's shock sound, 153: the first by default, 0..3, live; the third set from the menu");
	const int slow = IndexOf("ini_slow_aggression");
	const int slowLevel = IndexOf("ini_slow_aggression_level");
	const int slowMin = IndexOf("ini_slow_aggression_seconds_min");
	const int slowMax = IndexOf("ini_slow_aggression_seconds_max");
	const int slowUnits = IndexOf("ini_slow_aggression_units");
	Check(slow == 154 && slowLevel == 155 && slowMin == 156 && slowMax == 157 && slowUnits == 158 &&
	          walkers::LinkValue(rise, replace, slowLevel) == 1.0 && kLinkSettings[slowLevel].min == 0 &&
	          kLinkSettings[slowLevel].max == 3 && kLinkSettings[slowLevel].effect == walkers::LinkEffect::Live &&
	          IndexOf("ini_slow_aggression_seconds") < 0 && walkers::LinkValue(rise, replace, slow) == 1.0 &&
	          walkers::LinkValue(rise, replace, slowMin) == 5.0 && walkers::LinkValue(rise, replace, slowMax) == 10.0 &&
	          walkers::LinkValue(rise, replace, slowUnits) == 1000.0 &&
	          kLinkSettings[slow].effect == walkers::LinkEffect::Live &&
	          kLinkSettings[slowMin].effect == walkers::LinkEffect::Live &&
	          kLinkSettings[slowMax].effect == walkers::LinkEffect::Live &&
	          kLinkSettings[slow].kind == walkers::LinkKind::Toggle,
	      "the slow aggression last, 154..158: on, medium, own seconds 5 to 10, 1000 units by default, live");
	Check(walkers::SetLinkValue(rise, replace, slowLevel, 2.0) &&
	          rise.slowAggression.level == walkers::SlowAggressionLevel::High &&
	          walkers::SetLinkValue(rise, replace, slowLevel, 0.0) &&
	          rise.slowAggression.level == walkers::SlowAggressionLevel::Low,
	      "the level set from the menu: high, then low");
	Check(walkers::SetLinkValue(rise, replace, slow, 0.0) && !rise.slowAggression.enabled &&
	          walkers::SetLinkValue(rise, replace, slowMin, 3.0) && rise.slowAggression.secondsMin == 3.0f &&
	          walkers::SetLinkValue(rise, replace, slowMax, 12.0) && rise.slowAggression.secondsMax == 12.0f &&
	          walkers::SetLinkValue(rise, replace, slowUnits, 2000.0) && rise.slowAggression.units == 2000.0f,
	      "off, 3 to 12 s and 2000 units set from the menu");
	const int questLists = IndexOf("ini_keep_quest_lists");
	Check(questLists == 159 && walkers::LinkValue(rise, replace, questLists) == 1.0 &&
	          kLinkSettings[questLists].kind == walkers::LinkKind::Toggle &&
	          kLinkSettings[questLists].effect == walkers::LinkEffect::Spawns &&
	          walkers::SetLinkValue(rise, replace, questLists, 0.0) && !replace.keepQuestLists,
	      "KeepQuestLists, 159: on by default, the spawns redone when switched; off from the menu");
	const int things = IndexOf("ini_camps_things");
	const int escort = IndexOf("ini_camps_escort");
	const int escortFame = IndexOf("ini_camps_escort_fame");
	const int escortGold = IndexOf("ini_camps_escort_gold");
	const int escortBehind = IndexOf("ini_camps_escort_behind");
	const int escortDays = IndexOf("ini_camps_escort_days");
	Check(things == 160 && escort == 161 && escortFame == 162 && escortGold == 163 && escortBehind == 164 &&
	          escortDays == 165 && walkers::LinkValue(rise, replace, things) == 1.0 &&
	          walkers::LinkValue(rise, replace, escort) == 1.0 && walkers::LinkValue(rise, replace, escortFame) == 2.0 &&
	          walkers::LinkValue(rise, replace, escortGold) == 300.0 &&
	          walkers::LinkValue(rise, replace, escortBehind) == 5000.0 &&
	          walkers::LinkValue(rise, replace, escortDays) == 7.0 && kLinkSettings[escort].effect == walkers::LinkEffect::Live,
	      "the camps' things and the escort last, 160..165: on, on, fame 2, 300 gold, 5000 units, 7 days, live");
	Check(walkers::SetLinkValue(rise, replace, things, 0.0) && !rise.camps.rewardThings &&
	          walkers::SetLinkValue(rise, replace, escort, 0.0) && !rise.camps.escort &&
	          walkers::SetLinkValue(rise, replace, escortFame, 4.0) && rise.camps.escortFame == 4 &&
	          walkers::SetLinkValue(rise, replace, escortGold, 500.0) && rise.camps.escortGold == 500 &&
	          walkers::SetLinkValue(rise, replace, escortBehind, 8000.0) && rise.camps.escortBehindUnits == 8000.0f &&
	          walkers::SetLinkValue(rise, replace, escortDays, 3.0) && rise.camps.escortStayDays == 3.0f,
	      "each set from the menu");
	const int pull = IndexOf("ini_horde_city_pull");
	const int match = IndexOf("ini_rise_match_corpse");
	const int rename = IndexOf("ini_rename_to_walker");
	Check(pull >= 0 && walkers::SetLinkValue(rise, replace, pull, 0.25) && rise.horde.cityPull == 0.25f &&
	          match >= 0 && walkers::SetLinkValue(rise, replace, match, 1.0) && rise.matchCorpse == 1 &&
	          kLinkSettings[rename].effect == walkers::LinkEffect::NextStart &&
	          kLinkSettings[pull].effect == walkers::LinkEffect::Horde,
	      "an advanced one each way: the city pull (configured anew with the horde), the corpse match; the rename at "
	      "the next start");
	const int skeletons = IndexOf("ini_cat_skeletons");
	const int bandits = IndexOf("ini_cat_bandits");
	const UInt32 skeletonIndex = static_cast<UInt32>(skeletons) - walkers::kLinkCategoryFirst;
	const UInt32 banditIndex = static_cast<UInt32>(bandits) - walkers::kLinkCategoryFirst;
	Check(skeletons >= 0 && bandits >= 0 && walkers::LinkValue(rise, replace, skeletons) == 1.0 &&
	          walkers::LinkValue(rise, replace, bandits) == 0.0,
	      "the categories read off the settings: Skeletons on, Bandits off - the defaults");
	Check(walkers::SetLinkValue(rise, replace, skeletons, 0.0) && !replace.categories[skeletonIndex] &&
	          walkers::SetLinkValue(rise, replace, bandits, 1.0) && replace.categories[banditIndex] &&
	          !walkers::SetLinkValue(rise, replace, bandits, 1.0),
	      "a category switched from the menu, the same again no change");
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
	Check(kLinkSettings[IndexOf("ini_replace_enabled")].effect == LinkEffect::Spawns &&
	          kLinkSettings[IndexOf("ini_cat_skeletons")].effect == LinkEffect::Spawns &&
	          kLinkSettings[IndexOf("ini_cat_bandits")].effect == LinkEffect::Spawns,
	      "the replacement and its categories: the lists' entries pointed anew at once (Nadi, 2026-10-29: \"eig will "
	      "ich sofort\")");
	Check(kLinkSettings[IndexOf("ini_extra_percent")].effect == LinkEffect::NextStart,
	      "the extra spawns: at the next start (entries added once a session)");
	bool counted = true;
	for (UInt32 i = 0; i < kLinkSettingCount; ++i) {
		counted = counted && static_cast<UInt32>(kLinkSettings[i].effect) < walkers::kLinkEffectCount;
	}
	Check(counted && walkers::kLinkEffectCount == 6, "six effects, each setting's among them");
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
	static char text[1 << 17];
	const UInt32 ini = walkers::LinkIniText(rise, replace, text, sizeof(text));
	const std::string iniText(text, ini);
	Check(ini > 0 && iniText.find("set Walkers.ini_health_percent to 15\r\n") != std::string::npos &&
	          iniText.find("set Walkers.ini_speed_multiplier to 1.00\r\n") != std::string::npos,
	      "the INI: a set line per setting, the plugin INI's values (LINK's defaults button)");
	Check(iniText.find("set Walkers.ini_version to 27\r\n") != std::string::npos, "and the version last");
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
