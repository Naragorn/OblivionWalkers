#pragma once

// Maskar's LINK - the settings in the game's own menu, process-free
// (docs/walkers-spec.md 4.18; Nadi, 2026-10-27: "ok passt für mich so").
//
// What LINK does (LINK 1.4's readme, 3.2 and 3.3, and its ESP's script
// strings, read 2026-10-27): for a mod "X.esp" it reads Data\X.cfg - lines
// that describe each setting, each closed by "SetStage LINK 10" - and the
// settings are a quest's script variables, "Quest.variable"; Data\X.ini is a
// batch script of "set Quest.variable to value" lines, run for the menu's
// defaults button. What the player sets stays in the save. LINK++ (Nexus
// 53352) reads mods made for LINK the same way.
//
// So the ESP holds the quest Walkers with a script that declares the
// variables below (tools/esp-write.py), the plugin writes both files at the
// start from its own INI's values (LinkCfgText, LinkIniText) and, once a
// second, reads the variables back and applies what changed. At a game load
// StartOf decides who wins: without LINK the plugin INI, always; with LINK
// the save's values, once the plugin has written the INI's into them.

#include <cmath>
#include <cstdarg>
#include <cstdio>

#include "ReplaceLogic.h"
#include "RiseLogic.h"
#include "Types.h"

namespace walkers {

inline constexpr const char* kLinkEsp = "LINK.esp";
inline constexpr const char* kLinkQuestEditorId = "Walkers";
inline constexpr UInt32 kLinkQuestLocalId = 0x94B;
inline constexpr UInt32 kLinkScriptLocalId = 0x94C;
inline constexpr const char* kLinkVersionVariable = "ini_version";
// Raised whenever a variable is added (2: NoSkillGain, 2026-10-27), so a save
// of the version before takes the plugin INI's values again - the new
// variable would read 0 otherwise.
inline constexpr double kLinkVersion = 2.0;

enum class LinkKind : UInt8 { Header, Toggle, Slider };

// What a change of the setting needs in the game, beyond the value itself.
enum class LinkEffect : UInt8 {
	Live,           // read where it is used, at every death or frame
	WalkerRecords,  // health and pace written on the 64 walker records anew
	SpawnCounts,    // the variant list's count slots written anew
	Horde,          // the horde, the gate guards and the news configured anew
	NextStart,      // the leveled lists are changed once a session: at the next start
};

struct LinkSetting {
	const char* var;  // nullptr: a header
	const char* title;
	const char* description;
	LinkKind kind;
	float min;
	float max;
	float step;
	UInt8 decimals;
	bool percent;
	LinkEffect effect;
};

// The headers stand in the CFG only; kLinkSettings holds the settings.
struct LinkHeader {
	UInt32 before;  // the index of the first setting under it
	const char* title;
};

inline constexpr LinkHeader kLinkHeaders[] = {
	{0, "The dead rise"}, {8, "Walkers"}, {16, "Spawns"}, {20, "Hordes, gate guards and news"}, {27, "Training"},
};

inline constexpr LinkSetting kLinkSettings[] = {
	{"ini_rise_enabled", "The dead rise", "A fallen person gets up as a walker after a while.", LinkKind::Toggle, 0, 1,
	 1, 0, false, LinkEffect::Live},
	{"ini_rise_chance", "Chance to rise", "How often a fallen person gets up as a walker.", LinkKind::Slider, 0, 100,
	 5, 0, true, LinkEffect::Live},
	{"ini_rise_after", "Seconds until rising", "How long the dead lie before they get up.", LinkKind::Slider, 0, 120,
	 1, 0, false, LinkEffect::Live},
	{"ini_rise_jitter", "Random extra seconds", "Up to this many seconds more, at random, so not all rise at once.",
	 LinkKind::Slider, 0, 60, 1, 0, false, LinkEffect::Live},
	{"ini_rise_only_generic", "Only nameless people", "Named people (quest givers, merchants) stay dead.",
	 LinkKind::Toggle, 0, 1, 1, 0, false, LinkEffect::Live},
	{"ini_rise_only_playable", "Only playable races", "Daedra and other non-playable races stay dead.",
	 LinkKind::Toggle, 0, 1, 1, 0, false, LinkEffect::Live},
	{"ini_rise_only_humans", "Only humans", "Elves and beast races stay dead.", LinkKind::Toggle, 0, 1, 1, 0, false,
	 LinkEffect::Live},
	{"ini_rise_sound", "Groan when rising", "How often a walker groans as it gets up.", LinkKind::Slider, 0, 100, 5, 0,
	 true, LinkEffect::Live},
	{"ini_walk", "Walkers only walk", "Walkers never run.", LinkKind::Toggle, 0, 1, 1, 0, false, LinkEffect::Live},
	{"ini_health_percent", "Walker health", "A walker's health, in percent of a zombie's.", LinkKind::Slider, 1, 100, 1,
	 0, true, LinkEffect::WalkerRecords},
	{"ini_speed_multiplier", "Walker pace", "How fast the walkers move, 1 is a zombie's pace.", LinkKind::Slider, 0.2f,
	 2.0f, 0.05f, 2, false, LinkEffect::WalkerRecords},
	{"ini_speed_random", "Pace spread", "How much the walkers' paces differ, in percent.", LinkKind::Slider, 0, 100, 5,
	 0, true, LinkEffect::WalkerRecords},
	{"ini_no_loot", "No loot on walkers", "Dead walkers cannot be searched (the risen keep what they carried).",
	 LinkKind::Toggle, 0, 1, 1, 0, false, LinkEffect::Live},
	{"ini_avoid_chapels", "Chapels are safe", "The dead in a chapel do not rise, walkers stay out.", LinkKind::Toggle, 0,
	 1, 1, 0, false, LinkEffect::Live},
	{"ini_fame_per_kills", "Fame per walker kills", "A point of fame for this many walkers cut down, 0 none.",
	 LinkKind::Slider, 0, 100, 1, 0, false, LinkEffect::Live},
	{"ini_no_leveling_xp", "No experience for walkers", "With Ultimate Leveling or Oblivion XP a walker's death gives no experience.",
	 LinkKind::Toggle, 0, 1, 1, 0, false, LinkEffect::Live},
	{"ini_replace_enabled", "Walkers replace spawns", "Creatures of the leveled lists become walkers. At the next start.",
	 LinkKind::Toggle, 0, 1, 1, 0, false, LinkEffect::NextStart},
	{"ini_min_count", "Walkers per spawn, fewest", "The fewest walkers a spawn point gives.", LinkKind::Slider, 1, 10,
	 1, 0, false, LinkEffect::SpawnCounts},
	{"ini_max_count", "Walkers per spawn, most", "The most walkers a spawn point gives.", LinkKind::Slider, 1, 10, 1, 0,
	 false, LinkEffect::SpawnCounts},
	{"ini_extra_percent", "Extra spawns", "Extra walkers beside the replaced spawns, in percent. At the next start.",
	 LinkKind::Slider, 0, 100, 5, 0, true, LinkEffect::NextStart},
	{"ini_horde_enabled", "Hordes", "Walkers out of combat drift in groups.", LinkKind::Toggle, 0, 1, 1, 0, false,
	 LinkEffect::Horde},
	{"ini_horde_toward_cities", "Hordes head for cities", "Hordes lean toward the cities and hold at the gates.",
	 LinkKind::Toggle, 0, 1, 1, 0, false, LinkEffect::Horde},
	{"ini_guards_enabled", "Gate guards", "Guards come out of a city's gate against the walkers there.",
	 LinkKind::Toggle, 0, 1, 1, 0, false, LinkEffect::Horde},
	{"ini_guards_health_factor", "Gate guard health", "A gate guard's health, in walkers' health.", LinkKind::Slider,
	 0.5f, 10.0f, 0.5f, 1, false, LinkEffect::Horde},
	{"ini_guards_per_walkers", "Walkers per gate guard", "One guard comes out for this many walkers.",
	 LinkKind::Slider, 1, 20, 1, 0, false, LinkEffect::Horde},
	{"ini_safeguard_enabled", "Safeguard", "Whoever the walkers kill gets up again once it is safe, so no quest breaks.",
	 LinkKind::Toggle, 0, 1, 1, 0, false, LinkEffect::Live},
	{"ini_courier", "Black Horse Courier news", "The Courier's broadsheets tell of the walkers.", LinkKind::Toggle, 0, 1,
	 1, 0, false, LinkEffect::Horde},
	{"ini_no_skill_gain", "No skill from walkers", "Blows on and from walkers train no skill (weapon, block, armor).",
	 LinkKind::Toggle, 0, 1, 1, 0, false, LinkEffect::Live},
};
inline constexpr UInt32 kLinkSettingCount = sizeof(kLinkSettings) / sizeof(kLinkSettings[0]);

// The setting's current value as LINK holds it: 0 or 1 for a toggle.
inline double LinkValue(const RiseSettings& rise, const ReplaceSettings& replace, UInt32 i) {
	switch (i) {
		case 0: return rise.enabled ? 1.0 : 0.0;
		case 1: return rise.chancePercent;
		case 2: return rise.afterSeconds;
		case 3: return rise.jitterSeconds;
		case 4: return rise.onlyGeneric ? 1.0 : 0.0;
		case 5: return rise.onlyPlayableRaces ? 1.0 : 0.0;
		case 6: return rise.onlyHumans ? 1.0 : 0.0;
		case 7: return rise.soundAtGetUpPercent;
		case 8: return rise.walkOnly ? 1.0 : 0.0;
		case 9: return rise.healthPercent;
		case 10: return rise.speedMultiplier;
		case 11: return rise.speedRandomPercent;
		case 12: return rise.noLoot ? 1.0 : 0.0;
		case 13: return rise.avoidChapels ? 1.0 : 0.0;
		case 14: return rise.famePerKills;
		case 15: return rise.noLevelingXp ? 1.0 : 0.0;
		case 16: return replace.enabled ? 1.0 : 0.0;
		case 17: return replace.minCount;
		case 18: return replace.maxCount;
		case 19: return replace.extraPercent;
		case 20: return rise.horde.enabled ? 1.0 : 0.0;
		case 21: return rise.horde.towardCities ? 1.0 : 0.0;
		case 22: return rise.guards.enabled ? 1.0 : 0.0;
		case 23: return rise.guards.healthFactor;
		case 24: return rise.guards.zombiesPerGuard;
		case 25: return rise.safeguard.enabled ? 1.0 : 0.0;
		case 26: return rise.news.courier ? 1.0 : 0.0;
		case 27: return rise.noSkillGain ? 1.0 : 0.0;
		default: return 0.0;
	}
}

// A value read from LINK, held to the setting's range (a whole number
// rounded); true when the setting changed.
inline bool SetLinkValue(RiseSettings& rise, ReplaceSettings& replace, UInt32 i, double value) {
	if (i >= kLinkSettingCount || value != value) {
		return false;
	}
	const LinkSetting& s = kLinkSettings[i];
	double v = value < s.min ? s.min : (value > s.max ? s.max : value);
	if (s.kind == LinkKind::Toggle) {
		v = v >= 0.5 ? 1.0 : 0.0;
	} else if (s.decimals == 0) {
		v = std::floor(v + 0.5);
	}
	if (LinkValue(rise, replace, i) == (s.decimals == 0 ? v : static_cast<double>(static_cast<float>(v)))) {
		return false;
	}
	const bool on = v >= 0.5;
	const UInt32 whole = static_cast<UInt32>(v);
	const float fraction = static_cast<float>(v);
	switch (i) {
		case 0: rise.enabled = on; break;
		case 1: rise.chancePercent = whole; break;
		case 2: rise.afterSeconds = fraction; break;
		case 3: rise.jitterSeconds = fraction; break;
		case 4: rise.onlyGeneric = on; break;
		case 5: rise.onlyPlayableRaces = on; break;
		case 6: rise.onlyHumans = on; break;
		case 7: rise.soundAtGetUpPercent = whole; break;
		case 8: rise.walkOnly = on; break;
		case 9: rise.healthPercent = whole; break;
		case 10: rise.speedMultiplier = fraction; break;
		case 11: rise.speedRandomPercent = whole; break;
		case 12: rise.noLoot = on; break;
		case 13: rise.avoidChapels = on; break;
		case 14: rise.famePerKills = whole; break;
		case 15: rise.noLevelingXp = on; break;
		case 16: replace.enabled = on; break;
		case 17: replace.minCount = whole; break;
		case 18: replace.maxCount = whole; break;
		case 19: replace.extraPercent = whole; break;
		case 20: rise.horde.enabled = on; break;
		case 21: rise.horde.towardCities = on; break;
		case 22: rise.guards.enabled = on; break;
		case 23: rise.guards.healthFactor = fraction; break;
		case 24: rise.guards.zombiesPerGuard = whole; break;
		case 25: rise.safeguard.enabled = on; break;
		case 26: rise.news.courier = on; break;
		case 27: rise.noSkillGain = on; break;
		default: return false;
	}
	return true;
}

enum class LinkStart : UInt8 {
	WriteIni,  // the plugin INI's values into the variables, and the version
	ReadLink,  // the variables' values into the settings
};

// At a game load: without LINK the plugin INI decides, always; with LINK the
// save's values do, once the plugin wrote this version's into them.
inline LinkStart StartOf(bool linkLoaded, double storedVersion) {
	return linkLoaded && storedVersion == kLinkVersion ? LinkStart::ReadLink : LinkStart::WriteIni;
}

namespace link_detail {

struct Writer {
	char* out;
	UInt32 capacity;
	UInt32 used = 0;
	bool fits = true;
	void Put(const char* format, ...) {
		if (!fits) {
			return;
		}
		va_list args;
		va_start(args, format);
		const int n = std::vsnprintf(out + used, capacity - used, format, args);
		va_end(args);
		if (n < 0 || used + static_cast<UInt32>(n) >= capacity) {
			fits = false;
			return;
		}
		used += static_cast<UInt32>(n);
	}
	UInt32 Done() const { return fits ? used : 0; }
};

}  // namespace link_detail

// Data\OblivionWalkers.ini for LINK: a set line per setting at the plugin
// INI's values, then the version. Returns the bytes written, 0 when it does
// not fit.
inline UInt32 LinkIniText(const RiseSettings& rise, const ReplaceSettings& replace, char* out, UInt32 capacity) {
	link_detail::Writer w{out, capacity};
	w.Put("; LINK's defaults for Oblivion Walkers, written by OblivionWalkers.dll at every start from\r\n"
	      "; Data\\OBSE\\Plugins\\OblivionWalkers.ini - edit that file, not this one.\r\n\r\n");
	for (UInt32 i = 0; i < kLinkSettingCount; ++i) {
		const LinkSetting& s = kLinkSettings[i];
		w.Put("set %s.%s to %.*f\r\n", kLinkQuestEditorId, s.var, static_cast<int>(s.decimals),
		      LinkValue(rise, replace, i));
	}
	w.Put("\r\n; DO NOT CHANGE THE FOLLOWING SETTING.\r\nset %s.%s to %.0f\r\n", kLinkQuestEditorId,
	      kLinkVersionVariable, kLinkVersion);
	return w.Done();
}

// Data\OblivionWalkers.cfg for LINK: the headers and the settings, as LINK's
// readme (3.2) has them. Returns the bytes written, 0 when it does not fit.
inline UInt32 LinkCfgText(char* out, UInt32 capacity) {
	link_detail::Writer w{out, capacity};
	w.Put("; LINK's menu for Oblivion Walkers, written by OblivionWalkers.dll at every start.\r\n\r\n");
	UInt32 header = 0;
	constexpr UInt32 headers = sizeof(kLinkHeaders) / sizeof(kLinkHeaders[0]);
	for (UInt32 i = 0; i < kLinkSettingCount; ++i) {
		for (; header < headers && kLinkHeaders[header].before == i; ++header) {
			w.Put("set LINK.cfg_setting_title to sv_Construct \"%s\"\r\nset LINK.cfg_setting_type to 0\r\n"
			      "SetStage LINK 10\r\n\r\n",
			      kLinkHeaders[header].title);
		}
		const LinkSetting& s = kLinkSettings[i];
		w.Put("set LINK.cfg_setting_name to sv_Construct \"%s.%s\"\r\n"
		      "set LINK.cfg_setting_description to sv_Construct \"%s\"\r\n"
		      "set LINK.cfg_setting_title to sv_Construct \"%s\"\r\n",
		      kLinkQuestEditorId, s.var, s.description, s.title);
		if (s.kind == LinkKind::Toggle) {
			w.Put("set LINK.cfg_setting_type to 1\r\nset LINK.cfg_setting_options to 2\r\n"
			      "set LINK.cfg_setting_option1_value to 0\r\nset LINK.cfg_setting_option2_value to 1\r\n"
			      "set LINK.cfg_setting_option1_description to sv_Construct \"Off\"\r\n"
			      "set LINK.cfg_setting_option2_description to sv_Construct \"On\"\r\n");
		} else {
			w.Put("set LINK.cfg_setting_type to 2\r\nset LINK.cfg_setting_decimals to %u\r\n"
			      "set LINK.cfg_setting_percent to %u\r\nset LINK.cfg_setting_min to %.*f\r\n"
			      "set LINK.cfg_setting_max to %.*f\r\nset LINK.cfg_setting_step to %.*f\r\n",
			      static_cast<UInt32>(s.decimals), s.percent ? 1u : 0u, static_cast<int>(s.decimals), s.min,
			      static_cast<int>(s.decimals), s.max, static_cast<int>(s.decimals), s.step);
		}
		w.Put("SetStage LINK 10\r\n\r\n");
	}
	return w.Done();
}

}  // namespace walkers
