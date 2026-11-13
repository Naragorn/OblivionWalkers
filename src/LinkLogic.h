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
// Raised whenever a variable is added (2: NoSkillGain, 2026-10-27; 3: the
// grab's three, 2026-10-28; 4: the struggle's four, 2026-10-28; 5: the
// advanced ones, 2026-10-28; 6: the VR shake's two, 2026-10-28; 7: the 45
// replacement categories, 2026-10-29), so a save of
// the version before takes the plugin INI's values again - the new variable
// would read 0 otherwise.
inline constexpr double kLinkVersion = 38.0;  // 38: the held dungeons' people 3 to 6 by default (a save's 4 to 8 replaced), 2026-11-13; 37: the held dungeons' runner, 2026-11-13; 36: the held dungeons' journal and rumors, 2026-11-13; 35: the held dungeons' WalkersAtOnce, 2026-11-13; 34: the held dungeons' sixteen, 2026-11-13; 33: the refugee dungeons' OnlyChosen, 2026-11-13; 32: the slow aggression's calm time, 2026-11-11; 31: the gates' waiting three (4.11c), 2026-11-11; 30: EscortIntoCity, 2026-11-09; 29: the disguise's two, 2026-11-09; 28: OthersEndPercent, 2026-11-09; 27: the camps' things and the escort's five, 2026-11-08; 26: KeepQuestLists; 25: the slow aggression's level; 24: the slow aggression's time at random, fewest and most; 23: the slow aggression's three, 2026-11-08; 18: the nights' six (4.20.5); 19: the noise's three (4.20.2); 20: the camps' fifteen (4.20.6), 2026-11-05; 21: the camps' count (the dynamic camps); 22: the grab's shock sound, 2026-11-06

enum class LinkKind : UInt8 { Header, Toggle, Slider };

// What a change of the setting needs in the game, beyond the value itself.
enum class LinkEffect : UInt8 {
	Live,           // read where it is used, at every death or frame
	WalkerRecords,  // health and pace written on the 64 walker records anew
	SpawnCounts,    // the variant list's count slots written anew
	Horde,          // the horde, the gate guards and the news configured anew
	Spawns,         // the lists' entries pointed at the walkers or back at their own anew
	NextStart,      // the leveled lists are changed once a session: at the next start
};
inline constexpr UInt32 kLinkEffectCount = 6;

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
	{0, "The dead rise"}, {8, "Walkers"}, {16, "Spawns"}, {20, "Hordes, gate guards and news"}, {27, "Training"}, {28, "Grabbed"},
	{35, "The dead rise - advanced"}, {41, "Walkers - advanced"}, {44, "Hordes - advanced"},
	{52, "Gate guards - advanced"}, {54, "Safeguard and news - advanced"}, {58, "Grabbed - advanced"},
	{68, "Spawns - which creatures become walkers"}, {68 + kFirstPersonCategory, "Spawns - which people become walkers"},
	{68 + kReplaceCategoryCount, "Only a lost head ends a walker"},
	{68 + kReplaceCategoryCount + 3, "City sieges"},
	{68 + kReplaceCategoryCount + 10, "Grabbed - in reach (VR)"},
	{68 + kReplaceCategoryCount + 15, "Nights are worse"},
	{68 + kReplaceCategoryCount + 21, "Noise draws them"},
	{68 + kReplaceCategoryCount + 24, "Survivors' camps"},
	{68 + kReplaceCategoryCount + 40, "Grabbed - the shock sound"},
	{68 + kReplaceCategoryCount + 41, "Walkers - slow aggression"},
	{68 + kReplaceCategoryCount + 46, "Spawns - quests"},
	{68 + kReplaceCategoryCount + 47, "Survivors' camps - things and the escort"},
	{68 + kReplaceCategoryCount + 53, "Only a lost head ends a walker - others' kills"},
	{68 + kReplaceCategoryCount + 54, "Walker blood as a disguise"},
	{68 + kReplaceCategoryCount + 57, "Hordes - waiting out of sight"},
	{68 + kReplaceCategoryCount + 60, "Walkers - slow aggression, calm again"},
	{68 + kReplaceCategoryCount + 61, "Refugee dungeons"},
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
	{"ini_replace_enabled", "Walkers replace spawns", "The creatures and people of the categories below become walkers.",
	 LinkKind::Toggle, 0, 1, 1, 0, false, LinkEffect::Spawns},
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
	{"ini_grab_enabled", "Walkers grab", "A walker's hit may grab an arm and hold you where you stand; it bites.",
	 LinkKind::Toggle, 0, 1, 1, 0, false, LinkEffect::Live},
	{"ini_grab_chance", "Chance to grab", "How often a walker's hit grabs an arm.", LinkKind::Slider, 0, 100, 5, 0, true,
	 LinkEffect::Live},
	{"ini_grab_bites", "Bites you take", "The last of these bites kills. Strike a holder twice to break free.",
	 LinkKind::Slider, 1, 10, 1, 0, false, LinkEffect::Live},
	{"ini_grab_struggle_presses", "Presses to break free", "Block presses to wrench one walker off, fresh, 0 off.",
	 LinkKind::Slider, 0, 10, 1, 0, false, LinkEffect::Live},
	{"ini_grab_struggle_crowd", "Harder per walker", "Each walker around you beyond the first adds this much.",
	 LinkKind::Slider, 0, 1, 0.1f, 1, false, LinkEffect::Live},
	{"ini_grab_struggle_fatigue", "Fatigue per press", "What each press costs; tired, you need up to twice the presses.",
	 LinkKind::Slider, 0, 20, 1, 0, false, LinkEffect::Live},
	{"ini_grab_messages", "Grab messages", "A line on the screen when a walker grabs or bites you.", LinkKind::Toggle, 0,
	 1, 1, 0, false, LinkEffect::Live},
	// The advanced ones (Nadi, 2026-10-28: every setting LINK can show is in
	// LINK - AGENTS.md), after all the others so their indices stay.
	{"ini_rise_drop_seconds", "Seconds before the drop", "How long the new walker waits, unseen, before it is dropped beside the corpse.", LinkKind::Slider, 0, 5, 0.5f, 1, false, LinkEffect::Live},
	{"ini_rise_lie_seconds", "Seconds lying", "How long the walker lies where the corpse lay before it gets up.", LinkKind::Slider, 0, 5, 0.5f, 1, false, LinkEffect::Live},
	{"ini_rise_match_corpse", "Match the corpse", "0 drop it anywhere, 1 lay it where the corpse lies, 2 also in its pose.", LinkKind::Slider, 0, 2, 1, 0, false, LinkEffect::Live},
	{"ini_rise_settle_seconds", "Seconds to settle", "How long the posed walker settles unseen before it takes the corpse's place.", LinkKind::Slider, 0, 5, 0.5f, 1, false, LinkEffect::Live},
	{"ini_rise_keep_apart", "Keep the corpse apart", "The corpse's body is kept out of the walker's way while they change places.", LinkKind::Toggle, 0, 1, 1, 0, false, LinkEffect::Live},
	{"ini_rise_hide_at_pose", "Hide the corpse early", "The corpse is hidden as soon as the walker is posed, not when they change places.", LinkKind::Toggle, 0, 1, 1, 0, false, LinkEffect::Live},
	{"ini_rename_to_walker", "Call them Walker", "Walkers carry the name Walker (else Zombie). At the next start.", LinkKind::Toggle, 0, 1, 1, 0, false, LinkEffect::NextStart},
	{"ini_rename_vanilla", "Rename every zombie", "Every zombie of the game and of other mods is called Walker too. At the next start.", LinkKind::Toggle, 0, 1, 1, 0, false, LinkEffect::NextStart},
	{"ini_block_moo_resurrect", "Keep MOO from raising them", "With Maskar's Oblivion Overhaul a dead walker loses MOO's token, so MOO leaves it down.", LinkKind::Toggle, 0, 1, 1, 0, false, LinkEffect::Live},
	{"ini_horde_join_radius", "Join radius", "A lone walker joins a leader this near (units).", LinkKind::Slider, 500, 5000, 100, 0, false, LinkEffect::Horde},
	{"ini_horde_lead_units", "Lead distance", "The horde's marker this far ahead of its leader (units).", LinkKind::Slider, 500, 5000, 100, 0, false, LinkEffect::Horde},
	{"ini_horde_stale_seconds", "Seconds before a new heading", "A leader no nearer its marker this long gets a new one.", LinkKind::Slider, 5, 120, 5, 0, false, LinkEffect::Horde},
	{"ini_horde_turn_degrees", "Turn per heading", "The most a new heading turns from the last (degrees).", LinkKind::Slider, 0, 180, 5, 0, false, LinkEffect::Horde},
	{"ini_horde_group_most", "Walkers per leader", "The most followers one leader takes.", LinkKind::Slider, 2, 16, 1, 0, false, LinkEffect::Horde},
	{"ini_horde_city_pull", "Pull toward a city", "How far each heading leans toward the nearest city gate, 0 none, 1 straight there.", LinkKind::Slider, 0, 1, 0.05f, 2, false, LinkEffect::Horde},
	{"ini_horde_city_range", "City range", "A horde heads for a city gate this near (units).", LinkKind::Slider, 5000, 100000, 5000, 0, false, LinkEffect::Horde},
	{"ini_horde_gate_hold", "Hold before the gate", "A horde at a city holds this far before the gate (units).", LinkKind::Slider, 200, 3000, 100, 0, false, LinkEffect::Horde},
	{"ini_guards_radius", "Gate radius", "Walkers this near a gate count for its guards (units).", LinkKind::Slider, 1000, 10000, 500, 0, false, LinkEffect::Horde},
	{"ini_guards_clear_seconds", "Seconds until the guards go in", "No walker near the gate this long and the guards go back in.", LinkKind::Slider, 5, 120, 5, 0, false, LinkEffect::Horde},
	{"ini_safeguard_after", "Safeguard: seconds dead", "The walkers' victims are dead at least this long before they get up again.", LinkKind::Slider, 0, 120, 5, 0, false, LinkEffect::Live},
	{"ini_safeguard_radius", "Safeguard: no walker within", "They get up only with no living walker this near (units).", LinkKind::Slider, 500, 10000, 500, 0, false, LinkEffect::Live},
	{"ini_news_recent_days", "News: days an attack is news", "An attack on a city's gates stays the news this many game days.", LinkKind::Slider, 0, 10, 0.5f, 1, false, LinkEffect::Horde},
	{"ini_news_heading_hold", "News: days a horde is news", "A horde seen heading for a city stays the news this many game days.", LinkKind::Slider, 0, 3, 0.25f, 2, false, LinkEffect::Horde},
	{"ini_grab_bite_seconds", "Seconds between bites", "From the grab to the first bite, and between bites.", LinkKind::Slider, 1, 10, 0.5f, 1, false, LinkEffect::Live},
	{"ini_grab_bite_health", "Bite damage", "What a bite before the last takes, in percent of your base health.", LinkKind::Slider, 0, 100, 5, 0, true, LinkEffect::Live},
	{"ini_grab_struggle_hits", "Strikes to break free", "Your blows on a holder that make it let go.", LinkKind::Slider, 1, 10, 1, 0, false, LinkEffect::Live},
	{"ini_grab_tear_free", "Let-go distance", "Units between you and a holder that make it let go (a door, a teleport).", LinkKind::Slider, 100, 3000, 50, 0, false, LinkEffect::Live},
	{"ini_grab_bite_reset", "Seconds until bites are forgotten", "Free this long, the bites you took are forgotten.", LinkKind::Slider, 0, 120, 5, 0, false, LinkEffect::Live},
	{"ini_grab_crowd_most", "Crowd: most", "The crowd makes breaking free at most this many times harder.", LinkKind::Slider, 1, 5, 0.5f, 1, false, LinkEffect::Live},
	{"ini_grab_crowd_radius", "Crowd: radius", "Walkers this near count for the crowd (units).", LinkKind::Slider, 100, 1000, 50, 0, false, LinkEffect::Live},
	{"ini_grab_struggle_gap", "Seconds between presses", "A longer pause between Block presses starts the count again.", LinkKind::Slider, 0.2f, 2, 0.1f, 1, false, LinkEffect::Live},
	// The VR shake (2026-10-28), last so the indices stay.
	{"ini_grab_shake_strokes", "VR: swings to shake free", "In VR, swings of the held hand that shake one walker off, fresh, 0 off.", LinkKind::Slider, 0, 20, 1, 0, false, LinkEffect::Live},
	{"ini_grab_shake_fatigue", "VR: fatigue per swing", "What each swing of the held hand costs.", LinkKind::Slider, 0, 10, 1, 0, false, LinkEffect::Live},
	// The replacement's categories (4.6a, 2026-10-29), one toggle each in
	// kReplaceCategories' order from kLinkCategoryFirst on.
	{"ini_cat_skeletons", kReplaceCategories[0].title, kReplaceCategories[0].description, LinkKind::Toggle, 0, 1, 1, 0, false, LinkEffect::Spawns},
	{"ini_cat_zombies", kReplaceCategories[1].title, kReplaceCategories[1].description, LinkKind::Toggle, 0, 1, 1, 0, false, LinkEffect::Spawns},
	{"ini_cat_ghosts", kReplaceCategories[2].title, kReplaceCategories[2].description, LinkKind::Toggle, 0, 1, 1, 0, false, LinkEffect::Spawns},
	{"ini_cat_liches", kReplaceCategories[3].title, kReplaceCategories[3].description, LinkKind::Toggle, 0, 1, 1, 0, false, LinkEffect::Spawns},
	{"ini_cat_shambles", kReplaceCategories[4].title, kReplaceCategories[4].description, LinkKind::Toggle, 0, 1, 1, 0, false, LinkEffect::Spawns},
	{"ini_cat_skinnedhounds", kReplaceCategories[5].title, kReplaceCategories[5].description, LinkKind::Toggle, 0, 1, 1, 0, false, LinkEffect::Spawns},
	{"ini_cat_atronachs", kReplaceCategories[6].title, kReplaceCategories[6].description, LinkKind::Toggle, 0, 1, 1, 0, false, LinkEffect::Spawns},
	{"ini_cat_clannfears", kReplaceCategories[7].title, kReplaceCategories[7].description, LinkKind::Toggle, 0, 1, 1, 0, false, LinkEffect::Spawns},
	{"ini_cat_daedroths", kReplaceCategories[8].title, kReplaceCategories[8].description, LinkKind::Toggle, 0, 1, 1, 0, false, LinkEffect::Spawns},
	{"ini_cat_scamps", kReplaceCategories[9].title, kReplaceCategories[9].description, LinkKind::Toggle, 0, 1, 1, 0, false, LinkEffect::Spawns},
	{"ini_cat_spiderdaedra", kReplaceCategories[10].title, kReplaceCategories[10].description, LinkKind::Toggle, 0, 1, 1, 0, false, LinkEffect::Spawns},
	{"ini_cat_xivilai", kReplaceCategories[11].title, kReplaceCategories[11].description, LinkKind::Toggle, 0, 1, 1, 0, false, LinkEffect::Spawns},
	{"ini_cat_hungers", kReplaceCategories[12].title, kReplaceCategories[12].description, LinkKind::Toggle, 0, 1, 1, 0, false, LinkEffect::Spawns},
	{"ini_cat_goblins", kReplaceCategories[13].title, kReplaceCategories[13].description, LinkKind::Toggle, 0, 1, 1, 0, false, LinkEffect::Spawns},
	{"ini_cat_trolls", kReplaceCategories[14].title, kReplaceCategories[14].description, LinkKind::Toggle, 0, 1, 1, 0, false, LinkEffect::Spawns},
	{"ini_cat_ogres", kReplaceCategories[15].title, kReplaceCategories[15].description, LinkKind::Toggle, 0, 1, 1, 0, false, LinkEffect::Spawns},
	{"ini_cat_minotaurs", kReplaceCategories[16].title, kReplaceCategories[16].description, LinkKind::Toggle, 0, 1, 1, 0, false, LinkEffect::Spawns},
	{"ini_cat_landdreughs", kReplaceCategories[17].title, kReplaceCategories[17].description, LinkKind::Toggle, 0, 1, 1, 0, false, LinkEffect::Spawns},
	{"ini_cat_imps", kReplaceCategories[18].title, kReplaceCategories[18].description, LinkKind::Toggle, 0, 1, 1, 0, false, LinkEffect::Spawns},
	{"ini_cat_spriggans", kReplaceCategories[19].title, kReplaceCategories[19].description, LinkKind::Toggle, 0, 1, 1, 0, false, LinkEffect::Spawns},
	{"ini_cat_wisps", kReplaceCategories[20].title, kReplaceCategories[20].description, LinkKind::Toggle, 0, 1, 1, 0, false, LinkEffect::Spawns},
	{"ini_cat_wolves", kReplaceCategories[21].title, kReplaceCategories[21].description, LinkKind::Toggle, 0, 1, 1, 0, false, LinkEffect::Spawns},
	{"ini_cat_bears", kReplaceCategories[22].title, kReplaceCategories[22].description, LinkKind::Toggle, 0, 1, 1, 0, false, LinkEffect::Spawns},
	{"ini_cat_mountainlions", kReplaceCategories[23].title, kReplaceCategories[23].description, LinkKind::Toggle, 0, 1, 1, 0, false, LinkEffect::Spawns},
	{"ini_cat_boars", kReplaceCategories[24].title, kReplaceCategories[24].description, LinkKind::Toggle, 0, 1, 1, 0, false, LinkEffect::Spawns},
	{"ini_cat_rats", kReplaceCategories[25].title, kReplaceCategories[25].description, LinkKind::Toggle, 0, 1, 1, 0, false, LinkEffect::Spawns},
	{"ini_cat_mudcrabs", kReplaceCategories[26].title, kReplaceCategories[26].description, LinkKind::Toggle, 0, 1, 1, 0, false, LinkEffect::Spawns},
	{"ini_cat_tameanimals", kReplaceCategories[27].title, kReplaceCategories[27].description, LinkKind::Toggle, 0, 1, 1, 0, false, LinkEffect::Spawns},
	{"ini_cat_slaughterfish", kReplaceCategories[28].title, kReplaceCategories[28].description, LinkKind::Toggle, 0, 1, 1, 0, false, LinkEffect::Spawns},
	{"ini_cat_elytra", kReplaceCategories[29].title, kReplaceCategories[29].description, LinkKind::Toggle, 0, 1, 1, 0, false, LinkEffect::Spawns},
	{"ini_cat_gnarls", kReplaceCategories[30].title, kReplaceCategories[30].description, LinkKind::Toggle, 0, 1, 1, 0, false, LinkEffect::Spawns},
	{"ini_cat_grummites", kReplaceCategories[31].title, kReplaceCategories[31].description, LinkKind::Toggle, 0, 1, 1, 0, false, LinkEffect::Spawns},
	{"ini_cat_baliwogs", kReplaceCategories[32].title, kReplaceCategories[32].description, LinkKind::Toggle, 0, 1, 1, 0, false, LinkEffect::Spawns},
	{"ini_cat_scalons", kReplaceCategories[33].title, kReplaceCategories[33].description, LinkKind::Toggle, 0, 1, 1, 0, false, LinkEffect::Spawns},
	{"ini_cat_vampires", kReplaceCategories[34].title, kReplaceCategories[34].description, LinkKind::Toggle, 0, 1, 1, 0, false, LinkEffect::Spawns},
	{"ini_cat_necromancers", kReplaceCategories[35].title, kReplaceCategories[35].description, LinkKind::Toggle, 0, 1, 1, 0, false, LinkEffect::Spawns},
	{"ini_cat_conjurers", kReplaceCategories[36].title, kReplaceCategories[36].description, LinkKind::Toggle, 0, 1, 1, 0, false, LinkEffect::Spawns},
	{"ini_cat_bandits", kReplaceCategories[37].title, kReplaceCategories[37].description, LinkKind::Toggle, 0, 1, 1, 0, false, LinkEffect::Spawns},
	{"ini_cat_marauders", kReplaceCategories[38].title, kReplaceCategories[38].description, LinkKind::Toggle, 0, 1, 1, 0, false, LinkEffect::Spawns},
	{"ini_cat_dremora", kReplaceCategories[39].title, kReplaceCategories[39].description, LinkKind::Toggle, 0, 1, 1, 0, false, LinkEffect::Spawns},
	{"ini_cat_mythicdawn", kReplaceCategories[40].title, kReplaceCategories[40].description, LinkKind::Toggle, 0, 1, 1, 0, false, LinkEffect::Spawns},
	{"ini_cat_adventurers", kReplaceCategories[41].title, kReplaceCategories[41].description, LinkKind::Toggle, 0, 1, 1, 0, false, LinkEffect::Spawns},
	{"ini_cat_zealots", kReplaceCategories[42].title, kReplaceCategories[42].description, LinkKind::Toggle, 0, 1, 1, 0, false, LinkEffect::Spawns},
	{"ini_cat_heretics", kReplaceCategories[43].title, kReplaceCategories[43].description, LinkKind::Toggle, 0, 1, 1, 0, false, LinkEffect::Spawns},
	{"ini_cat_knightsoforder", kReplaceCategories[44].title, kReplaceCategories[44].description, LinkKind::Toggle, 0, 1, 1, 0, false, LinkEffect::Spawns},
	{"ini_die_only_beheaded", "Only beheading ends walkers", "With OBVR cutting heads (Dismemberment=1) a fallen walker gets up again until its head is off; otherwise every kill ends it.",
	 LinkKind::Toggle, 0, 1, 1, 0, false, LinkEffect::Live},
	{"ini_rise_again", "Seconds until a walker gets up again", "How long a fallen walker lies.", LinkKind::Slider, 0,
	 120, 1, 0, false, LinkEffect::Live},
	{"ini_rise_again_jitter", "Random extra seconds, walkers", "Up to this many seconds more, at random.",
	 LinkKind::Slider, 0, 60, 1, 0, false, LinkEffect::Live},
	{"ini_siege_enabled", "City sieges", "Hordes besiege a city's gate until the guards and you have cut them down.",
	 LinkKind::Toggle, 0, 1, 1, 0, false, LinkEffect::Live},
	{"ini_siege_at_gate", "Walkers at the gate at once", "How many stand before a besieged gate at a time.",
	 LinkKind::Slider, 12, 100, 1, 0, false, LinkEffect::Live},
	{"ini_siege_chance", "Siege chance per day", "Each city's chance a game day to be besieged.", LinkKind::Slider, 0,
	 100, 1, 0, true, LinkEffect::Live},
	{"ini_siege_min", "Walkers in a siege, fewest", "The fewest walkers a siege brings.", LinkKind::Slider, 1, 300, 5,
	 0, false, LinkEffect::Live},
	{"ini_siege_max", "Walkers in a siege, most", "The most walkers a siege brings.", LinkKind::Slider, 1, 300, 5, 0,
	 false, LinkEffect::Live},
	{"ini_siege_pause", "Days between sieges", "Game days a city has after a siege before the next.",
	 LinkKind::Slider, 0, 30, 0.5f, 1, false, LinkEffect::Live},
	{"ini_siege_toll", "Guards' toll per hour", "Walkers the guards cut down a game hour while you are away.",
	 LinkKind::Slider, 0, 20, 0.5f, 1, false, LinkEffect::Live},
	{"ini_grab_reach", "VR: a walker's reach", "Units from its shoulder to your forearm it grabs within.",
	 LinkKind::Slider, 10, 80, 1, 0, false, LinkEffect::Live},
	{"ini_grab_lunge", "VR: a walker's lunge", "Units more it may lunge in to grab, 0 none.", LinkKind::Slider, 0,
	 120, 5, 0, false, LinkEffect::Live},
	{"ini_grab_lunge_seconds", "VR: the lunge's time", "Seconds its lunge takes.", LinkKind::Slider, 0, 1, 0.05f, 2,
	 false, LinkEffect::Live},
	{"ini_rise_no_cities", "No rising in cities", "Nobody killed in a city's streets rises; the safeguard raises them.",
	 LinkKind::Toggle, 0, 1, 1, 0, false, LinkEffect::Live},
	{"ini_safeguard_at_home", "Raised at home", "Whom the safeguard raises wakes where the game placed them, their house or shop.",
	 LinkKind::Toggle, 0, 1, 1, 0, false, LinkEffect::Live},
	{"ini_night_enabled", "Nights are worse", "From dusk to dawn the walkers are quicker, spawns bring more of them, hordes roam farther.",
	 LinkKind::Toggle, 0, 1, 1, 0, false, LinkEffect::Live},
	{"ini_night_speed", "Night: walker pace", "How much quicker the walkers are at night, 1 as by day.", LinkKind::Slider,
	 0.5f, 2.0f, 0.05f, 2, false, LinkEffect::Live},
	{"ini_night_spawns", "Night: walkers per spawn", "A spawn's walkers at night, in percent of the day's.",
	 LinkKind::Slider, 50, 300, 10, 0, true, LinkEffect::Live},
	{"ini_night_horde_lead", "Night: hordes roam farther", "How far ahead a horde heads at night, in percent of the day's.",
	 LinkKind::Slider, 50, 300, 10, 0, true, LinkEffect::Live},
	{"ini_night_start", "Night begins at", "The hour the night begins (they quicken over the hour after).",
	 LinkKind::Slider, 0, 23, 1, 0, false, LinkEffect::Live},
	{"ini_night_end", "Night ends at", "The hour the night ends (they calm over the hour before).", LinkKind::Slider, 0,
	 23, 1, 0, false, LinkEffect::Live},
	{"ini_noise_enabled", "Noise draws them", "Blows, spells and thrown things draw the hordes near; they search the spot, then go on.",
	 LinkKind::Toggle, 0, 1, 1, 0, false, LinkEffect::Horde},
	{"ini_noise_radius", "Noise: heard within", "Units from the noise a walker must be to hear it.", LinkKind::Slider, 200,
	 5000, 100, 0, false, LinkEffect::Horde},
	{"ini_noise_search", "Noise: search time", "Seconds they search the spot before going on.", LinkKind::Slider, 5, 120,
	 5, 0, false, LinkEffect::Horde},
	{"ini_camps_enabled", "Survivors' camps", "Refugees camp across the land; walkers attack them - defend them for a reward.",
	 LinkKind::Toggle, 0, 1, 1, 0, false, LinkEffect::Live},
	{"ini_camps_people", "Camps: refugees", "Refugees in each camp (a camp that lost some keeps its own until it comes back).",
	 LinkKind::Slider, 2, 6, 1, 0, false, LinkEffect::Live},
	{"ini_camps_women", "Camps: women", "The share of women among the refugees.", LinkKind::Slider, 0, 100, 10, 0, true,
	 LinkEffect::Live},
	{"ini_camps_trader", "Camps: a trader", "Each camp has a trader among its refugees.", LinkKind::Toggle, 0, 1, 1, 0,
	 false, LinkEffect::Live},
	{"ini_camps_fame", "Camps: fame as reward", "A camp you defend gives fame.", LinkKind::Toggle, 0, 1, 1, 0, false,
	 LinkEffect::Live},
	{"ini_camps_fame_points", "Camps: fame points", "The fame a defended camp gives.", LinkKind::Slider, 1, 10, 1, 0,
	 false, LinkEffect::Live},
	{"ini_camps_gold", "Camps: gold as reward", "A camp you defend gives gold.", LinkKind::Toggle, 0, 1, 1, 0, false,
	 LinkEffect::Live},
	{"ini_camps_gold_amount", "Camps: gold", "The gold a defended camp gives.", LinkKind::Slider, 0, 1000, 25, 0, false,
	 LinkEffect::Live},
	{"ini_camps_attack_chance", "Camps: attacks a day", "The chance a camp is attacked each game day.", LinkKind::Slider, 0,
	 100, 5, 0, true, LinkEffect::Live},
	{"ini_camps_walkers_min", "Camps: walkers, fewest", "The fewest walkers in an attack.", LinkKind::Slider, 1, 40, 1, 0,
	 false, LinkEffect::Live},
	{"ini_camps_walkers_max", "Camps: walkers, most", "The most walkers in an attack.", LinkKind::Slider, 1, 40, 1, 0,
	 false, LinkEffect::Live},
	{"ini_camps_at_once", "Camps: attacked at once", "Camps fought out near you at once; the others are decided unseen.",
	 LinkKind::Slider, 1, 2, 1, 0, false, LinkEffect::Live},
	{"ini_camps_kills", "Camps: refugees' kills", "Walkers the refugees cut down each game hour while you are away.",
	 LinkKind::Slider, 0, 10, 0.5f, 1, false, LinkEffect::Live},
	{"ini_camps_loss", "Camps: refugees lost", "The chance each game hour that a refugee dies while you are away.",
	 LinkKind::Slider, 0, 100, 5, 0, true, LinkEffect::Live},
	{"ini_camps_return", "Camps: back again", "The chance each game day that an overrun camp is settled again.",
	 LinkKind::Slider, 0, 100, 5, 0, true, LinkEffect::Live},
	{"ini_camps_count", "Camps: how many", "Camps over the land, placed once per game; more later are placed then.",
	 LinkKind::Slider, 1, 64, 1, 0, false, LinkEffect::Live},
	{"ini_grab_shock_sound", "Grab: shock sound", "The sting when a walker grabs you: one of three, 0 none.",
	 LinkKind::Slider, 0, 3, 1, 0, false, LinkEffect::Live},
	{"ini_slow_aggression", "Slow aggression", "A walker turns on someone only once that one has stayed near it a while.",
	 LinkKind::Toggle, 0, 1, 1, 0, false, LinkEffect::Live},
	{"ini_slow_aggression_level", "Slow aggression: how aggressive",
	 "0 low (10-15 s near before it attacks), 1 medium (5-10 s), 2 high (1-5 s), 3 own (the two below).",
	 LinkKind::Slider, 0, 3, 1, 0, false, LinkEffect::Live},
	{"ini_slow_aggression_seconds_min", "Slow aggression: own fewest seconds",
	 "With the level own: how long one must stay near a walker before it attacks, at the least.",
	 LinkKind::Slider, 0, 30, 1, 0, false, LinkEffect::Live},
	{"ini_slow_aggression_seconds_max", "Slow aggression: own most seconds",
	 "With the level own: how long one must stay near a walker before it attacks, at the most.",
	 LinkKind::Slider, 0, 30, 1, 0, false, LinkEffect::Live},
	{"ini_slow_aggression_units", "Slow aggression: how near", "How near, in game units (1000 is some 14 m).",
	 LinkKind::Slider, 200, 4000, 100, 0, false, LinkEffect::Live},
	{"ini_keep_quest_lists", "Quests keep their foes",
	 "A spawn a quest needs - its boss, its wave, its bears - stays as the game made it, no walker.",
	 LinkKind::Toggle, 0, 1, 1, 0, false, LinkEffect::Spawns},
	{"ini_camps_things", "Camps: things as reward", "A camp you defend or lead to safety gives a thing off the game's loot.",
	 LinkKind::Toggle, 0, 1, 1, 0, false, LinkEffect::Live},
	{"ini_camps_escort", "Camps: escort", "A camp's refugees ask you to lead them to the nearest city.", LinkKind::Toggle, 0, 1,
	 1, 0, false, LinkEffect::Live},
	{"ini_camps_escort_fame", "Escort: fame", "The fame for all brought to the city; fewer alive, less.", LinkKind::Slider,
	 0, 10, 1, 0, false, LinkEffect::Live},
	{"ini_camps_escort_gold", "Escort: gold", "The gold for all brought to the city; fewer alive, less.", LinkKind::Slider,
	 0, 2000, 50, 0, false, LinkEffect::Live},
	{"ini_camps_escort_behind", "Escort: left behind", "Farther than this from you (game units, 5000 is some 70 m), the escort fails.",
	 LinkKind::Slider, 1000, 20000, 500, 0, false, LinkEffect::Live},
	{"ini_camps_escort_days", "Escort: days in the city", "How long they stay in the city before they move on.",
	 LinkKind::Slider, 1, 30, 1, 0, false, LinkEffect::Live},
	{"ini_others_end_percent", "Others' kills end walkers",
	 "The chance a walker felled by a guard, a bandit or a companion stays down - its head comes off (VR).",
	 LinkKind::Slider, 0, 100, 5, 0, true, LinkEffect::Live},
	{"ini_disguise_enabled", "Walker blood as a disguise",
	 "Sneak and activate a fallen walker to rub its blood on: the walkers leave you be for a while.", LinkKind::Toggle, 0,
	 1, 1, 0, false, LinkEffect::Live},
	{"ini_disguise_seconds", "Disguise: seconds", "How long the walker blood lasts, in real seconds.", LinkKind::Slider, 30,
	 600, 30, 0, false, LinkEffect::Live},
	{"ini_camps_escort_into_city", "Escort: into the city",
	 "The refugees you bring to a city go in through its gate; off, they stay before it.", LinkKind::Toggle, 0, 1, 1, 0,
	 false, LinkEffect::Live},
	{"ini_horde_gate_hordes_most", "Hordes a gate at once",
	 "How many hordes may stand before one city gate at once; the rest wait out of sight. 0: any.", LinkKind::Slider, 0,
	 16, 1, 0, false, LinkEffect::Horde},
	{"ini_horde_gate_near_units", "Hold only with you near",
	 "A horde stands before a gate only while you are this near it (units); otherwise it waits out of sight. 0: always.",
	 LinkKind::Slider, 0, 20000, 500, 0, false, LinkEffect::Horde},
	{"ini_horde_loaded_grid", "Your loaded grid",
	 "Set to your uGridsToLoad: waiting hordes stand just outside the cells the game keeps loaded around you.",
	 LinkKind::Slider, 3, 11, 2, 0, false, LinkEffect::Horde},
	{"ini_slow_aggression_calm_seconds", "Slow aggression: calm again after",
	 "A walker that turned on someone fights as any undead until nobody living has been near it this many seconds.",
	 LinkKind::Slider, 2, 60, 1, 0, false, LinkEffect::Live},
	{"ini_dungeons_only_chosen", "Only the chosen dungeons",
	 "Refugees hold only the chosen dungeons - roomy, few cells, spread over the map - not any free one.",
	 LinkKind::Toggle, 0, 1, 1, 0, false, LinkEffect::Live},
	{"ini_dungeons_enabled", "Refugees hold dungeons",
	 "Refugees hole up in dungeons; walkers try to get in.",
	 LinkKind::Toggle, 0, 1, 1, 0, false, LinkEffect::Live},
	{"ini_dungeons_active_min", "Dungeons held at least",
	 "The fewest dungeons active at once.",
	 LinkKind::Slider, 1, 8, 1, 0, false, LinkEffect::Live},
	{"ini_dungeons_active_max", "Dungeons held at most",
	 "The most dungeons active at once.",
	 LinkKind::Slider, 1, 8, 1, 0, false, LinkEffect::Live},
	{"ini_dungeons_weight_refuge", "Dungeons: refuge",
	 "How often a place is a quiet refuge (weight against the other three).",
	 LinkKind::Slider, 0, 100, 5, 0, false, LinkEffect::Live},
	{"ini_dungeons_weight_besieged", "Dungeons: besieged",
	 "How often a place is besieged by walkers (weight).",
	 LinkKind::Slider, 0, 100, 5, 0, false, LinkEffect::Live},
	{"ini_dungeons_weight_to_clear", "Dungeons: to clear",
	 "How often you must clear a place first, the refugees waiting outside (weight).",
	 LinkKind::Slider, 0, 100, 5, 0, false, LinkEffect::Live},
	{"ini_dungeons_weight_walker", "Dungeons: walker dungeon",
	 "How often a place is already overrun by walkers (weight).",
	 LinkKind::Slider, 0, 100, 5, 0, false, LinkEffect::Live},
	{"ini_dungeons_spacing", "Dungeons apart",
	 "The active dungeons' doors at least this far apart (units).",
	 LinkKind::Slider, 0, 60000, 1000, 0, false, LinkEffect::Live},
	{"ini_dungeons_people_min", "Refugees at least",
	 "The fewest refugees in a held dungeon.",
	 LinkKind::Slider, 1, 12, 1, 0, false, LinkEffect::Live},
	{"ini_dungeons_people_max", "Refugees at most",
	 "The most refugees in a held dungeon.",
	 LinkKind::Slider, 1, 12, 1, 0, false, LinkEffect::Live},
	{"ini_dungeons_attack_chance", "Dungeons: attack chance",
	 "The chance each game day that walkers attack a quiet refuge.",
	 LinkKind::Slider, 0, 100, 1, 0, true, LinkEffect::Live},
	{"ini_dungeons_walkers_min", "Dungeon attack: walkers at least",
	 "The fewest walkers in an attack on a dungeon.",
	 LinkKind::Slider, 1, 60, 1, 0, false, LinkEffect::Live},
	{"ini_dungeons_walkers_max", "Dungeon attack: walkers at most",
	 "The most walkers in an attack on a dungeon.",
	 LinkKind::Slider, 1, 60, 1, 0, false, LinkEffect::Live},
	{"ini_dungeons_kills", "Dungeons: refugees' kills",
	 "Walkers the refugees cut down each game hour of a siege while you are away.",
	 LinkKind::Slider, 0, 10, 0.5f, 1, false, LinkEffect::Live},
	{"ini_dungeons_loss", "Dungeons: refugees lost",
	 "The chance each game hour of a siege that a refugee dies while you are away.",
	 LinkKind::Slider, 0, 100, 1, 0, true, LinkEffect::Live},
	{"ini_dungeons_leave_days", "Dungeons: given up after",
	 "Game days a dungeon you never came near stays held; then the refugees move on and another is chosen. 0: never.",
	 LinkKind::Slider, 0, 30, 1, 0, false, LinkEffect::Live},
	{"ini_dungeons_walkers_at_once", "Dungeons: walkers at once",
	 "How many of a siege's or a walker dungeon's walkers are in the place at a time; the rest follow as they fall.",
	 LinkKind::Slider, 1, 12, 1, 0, false, LinkEffect::Live},
	{"ini_dungeons_journal", "Dungeons: journal quests",
	 "Each held dungeon you find or hear of gets a quest in your journal, its map marker the target.",
	 LinkKind::Toggle, 0, 1, 1, 0, false, LinkEffect::Live},
	{"ini_dungeons_rumors", "Dungeons: rumors",
	 "People tell you of held dungeons when you ask for rumors; the place is marked on your map. Needs the journal quests.",
	 LinkKind::Toggle, 0, 1, 1, 0, false, LinkEffect::Live},
	{"ini_dungeons_messenger_hours", "Dungeons: a runner for help",
	 "Game hours between the runners who find you to ask for help - at a siege or a place to clear. 0: nobody comes.",
	 LinkKind::Slider, 0, 168, 1, 0, false, LinkEffect::Live},
};
inline constexpr UInt32 kLinkSettingCount = sizeof(kLinkSettings) / sizeof(kLinkSettings[0]);
inline constexpr UInt32 kLinkCategoryFirst = 68;

// The setting's current value as LINK holds it: 0 or 1 for a toggle.
inline double LinkValue(const RiseSettings& rise, const ReplaceSettings& replace, UInt32 i) {
	if (i >= kLinkCategoryFirst && i < kLinkCategoryFirst + kReplaceCategoryCount) {
		return replace.categories[i - kLinkCategoryFirst] ? 1.0 : 0.0;
	}
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
		case 28: return rise.grab.enabled ? 1.0 : 0.0;
		case 29: return rise.grab.chancePercent;
		case 30: return rise.grab.bitesToDie;
		case 31: return rise.grab.strugglePresses;
		case 32: return rise.grab.struggleCrowdStep;
		case 33: return rise.grab.struggleFatigue;
		case 34: return rise.grab.messages ? 1.0 : 0.0;
		case 35: return rise.dropSeconds;
		case 36: return rise.lieSeconds;
		case 37: return rise.matchCorpse;
		case 38: return rise.settleSeconds;
		case 39: return rise.keepCorpseApart ? 1.0 : 0.0;
		case 40: return rise.hideCorpseAtPose ? 1.0 : 0.0;
		case 41: return rise.renameToWalker ? 1.0 : 0.0;
		case 42: return rise.renameVanilla ? 1.0 : 0.0;
		case 43: return rise.blockMooResurrect ? 1.0 : 0.0;
		case 44: return rise.horde.joinRadius;
		case 45: return rise.horde.leadUnits;
		case 46: return rise.horde.staleSeconds;
		case 47: return rise.horde.turnDegrees;
		case 48: return rise.horde.groupMost;
		case 49: return rise.horde.cityPull;
		case 50: return rise.horde.cityRange;
		case 51: return rise.horde.gateHoldUnits;
		case 52: return rise.guards.radius;
		case 53: return rise.guards.clearSeconds;
		case 54: return rise.safeguard.afterSeconds;
		case 55: return rise.safeguard.radius;
		case 56: return rise.news.recentDays;
		case 57: return rise.news.headingHoldDays;
		case 58: return rise.grab.biteSeconds;
		case 59: return rise.grab.biteHealthPercent;
		case 60: return rise.grab.struggleHits;
		case 61: return rise.grab.tearFreeDistance;
		case 62: return rise.grab.biteResetSeconds;
		case 63: return rise.grab.struggleCrowdMost;
		case 64: return rise.grab.struggleCrowdRadius;
		case 65: return rise.grab.struggleGapSeconds;
		case 66: return rise.grab.shakeStrokes;
		case 67: return rise.grab.shakeFatigue;
		case 113: return rise.end.dieOnlyBeheaded ? 1.0 : 0.0;
		case 114: return rise.end.riseAgainSeconds;
		case 115: return rise.end.riseAgainJitterSeconds;
		case 116: return rise.siege.enabled ? 1.0 : 0.0;
		case 117: return rise.siege.walkersAtGate;
		case 118: return rise.siege.chancePerDayPercent;
		case 119: return rise.siege.walkersMin;
		case 120: return rise.siege.walkersMax;
		case 121: return rise.siege.pauseDays;
		case 122: return rise.siege.guardKillsPerHour;
		case 123: return rise.grab.reachUnits;
		case 124: return rise.grab.lungeUnits;
		case 125: return rise.grab.lungeSeconds;
		case 126: return rise.noRiseInCities ? 1.0 : 0.0;
		case 127: return rise.safeguard.raiseAtHome ? 1.0 : 0.0;
		case 128: return rise.night.enabled ? 1.0 : 0.0;
		case 129: return rise.night.speedMultiplier;
		case 130: return rise.night.spawnPercent;
		case 131: return rise.night.hordeLeadPercent;
		case 132: return rise.night.startHour;
		case 133: return rise.night.endHour;
		case 134: return rise.horde.noise.enabled ? 1.0 : 0.0;
		case 135: return rise.horde.noise.radius;
		case 136: return rise.horde.noise.searchSeconds;
		case 137: return rise.camps.enabled ? 1.0 : 0.0;
		case 138: return rise.camps.peoplePerCamp;
		case 139: return rise.camps.womenPercent;
		case 140: return rise.camps.trader ? 1.0 : 0.0;
		case 141: return rise.camps.rewardFame ? 1.0 : 0.0;
		case 142: return rise.camps.famePoints;
		case 143: return rise.camps.rewardGold ? 1.0 : 0.0;
		case 144: return rise.camps.gold;
		case 145: return rise.camps.attackChancePercent;
		case 146: return rise.camps.walkersMin;
		case 147: return rise.camps.walkersMax;
		case 148: return rise.camps.campsAttackedAtOnce;
		case 149: return rise.camps.refugeeKillsPerHour;
		case 150: return rise.camps.lossPercentPerHour;
		case 151: return rise.camps.returnChancePercent;
		case 152: return rise.camps.campCount;
		case 153: return rise.grab.shockSound;
		case 154: return rise.slowAggression.enabled ? 1.0 : 0.0;
		case 155: return static_cast<double>(rise.slowAggression.level);
		case 156: return rise.slowAggression.secondsMin;
		case 157: return rise.slowAggression.secondsMax;
		case 158: return rise.slowAggression.units;
		case 159: return replace.keepQuestLists ? 1.0 : 0.0;
		case 160: return rise.camps.rewardThings ? 1.0 : 0.0;
		case 161: return rise.camps.escort ? 1.0 : 0.0;
		case 162: return rise.camps.escortFame;
		case 163: return rise.camps.escortGold;
		case 164: return rise.camps.escortBehindUnits;
		case 165: return rise.camps.escortStayDays;
		case 166: return rise.end.othersEndPercent;
		case 167: return rise.disguise.enabled ? 1.0 : 0.0;
		case 168: return rise.disguise.seconds;
		case 169: return rise.camps.escortIntoCity ? 1.0 : 0.0;
		case 170: return rise.horde.gateHordesMost;
		case 171: return rise.horde.gateNearUnits;
		case 172: return rise.horde.loadedGrid;
		case 173: return rise.slowAggression.calmSeconds;
		case 174: return rise.dungeonsOnlyChosen ? 1.0 : 0.0;
		case 175: return rise.dungeons.enabled ? 1.0 : 0.0;
		case 176: return rise.dungeons.activeMin;
		case 177: return rise.dungeons.activeMax;
		case 178: return rise.dungeons.weightRefuge;
		case 179: return rise.dungeons.weightBesieged;
		case 180: return rise.dungeons.weightToClear;
		case 181: return rise.dungeons.weightWalkerDungeon;
		case 182: return rise.dungeons.spacingUnits;
		case 183: return rise.dungeons.peopleMin;
		case 184: return rise.dungeons.peopleMax;
		case 185: return rise.dungeons.attackChancePercent;
		case 186: return rise.dungeons.walkersMin;
		case 187: return rise.dungeons.walkersMax;
		case 188: return rise.dungeons.refugeeKillsPerHour;
		case 189: return rise.dungeons.lossPercentPerHour;
		case 190: return rise.dungeons.leaveAfterDays;
		case 191: return rise.dungeons.walkersAtOnce;
		case 192: return rise.dungeons.journal ? 1.0 : 0.0;
		case 193: return rise.dungeons.rumors ? 1.0 : 0.0;
		case 194: return rise.dungeons.messengerHours;
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
	if (i >= kLinkCategoryFirst && i < kLinkCategoryFirst + kReplaceCategoryCount) {
		replace.categories[i - kLinkCategoryFirst] = on;
		return true;
	}
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
		case 28: rise.grab.enabled = on; break;
		case 29: rise.grab.chancePercent = whole; break;
		case 30: rise.grab.bitesToDie = whole; break;
		case 31: rise.grab.strugglePresses = whole; break;
		case 32: rise.grab.struggleCrowdStep = fraction; break;
		case 33: rise.grab.struggleFatigue = fraction; break;
		case 34: rise.grab.messages = on; break;
		case 35: rise.dropSeconds = fraction; break;
		case 36: rise.lieSeconds = fraction; break;
		case 37: rise.matchCorpse = static_cast<UInt8>(whole); break;
		case 38: rise.settleSeconds = fraction; break;
		case 39: rise.keepCorpseApart = on; break;
		case 40: rise.hideCorpseAtPose = on; break;
		case 41: rise.renameToWalker = on; break;
		case 42: rise.renameVanilla = on; break;
		case 43: rise.blockMooResurrect = on; break;
		case 44: rise.horde.joinRadius = fraction; break;
		case 45: rise.horde.leadUnits = fraction; break;
		case 46: rise.horde.staleSeconds = fraction; break;
		case 47: rise.horde.turnDegrees = fraction; break;
		case 48: rise.horde.groupMost = whole; break;
		case 49: rise.horde.cityPull = fraction; break;
		case 50: rise.horde.cityRange = fraction; break;
		case 51: rise.horde.gateHoldUnits = fraction; break;
		case 52: rise.guards.radius = fraction; break;
		case 53: rise.guards.clearSeconds = fraction; break;
		case 54: rise.safeguard.afterSeconds = fraction; break;
		case 55: rise.safeguard.radius = fraction; break;
		case 56: rise.news.recentDays = fraction; break;
		case 57: rise.news.headingHoldDays = fraction; break;
		case 58: rise.grab.biteSeconds = fraction; break;
		case 59: rise.grab.biteHealthPercent = whole; break;
		case 60: rise.grab.struggleHits = whole; break;
		case 61: rise.grab.tearFreeDistance = fraction; break;
		case 62: rise.grab.biteResetSeconds = fraction; break;
		case 63: rise.grab.struggleCrowdMost = fraction; break;
		case 64: rise.grab.struggleCrowdRadius = fraction; break;
		case 65: rise.grab.struggleGapSeconds = fraction; break;
		case 66: rise.grab.shakeStrokes = whole; break;
		case 67: rise.grab.shakeFatigue = fraction; break;
		case 113:
			rise.end.dieOnlyBeheaded = on;
			replace.headlessWalkers = SpawnsHeadless(on);
			break;
		case 114: rise.end.riseAgainSeconds = fraction; break;
		case 115: rise.end.riseAgainJitterSeconds = fraction; break;
		case 116: rise.siege.enabled = on; break;
		case 117: rise.siege.walkersAtGate = whole; break;
		case 118: rise.siege.chancePerDayPercent = whole; break;
		case 119: rise.siege.walkersMin = whole; break;
		case 120: rise.siege.walkersMax = whole; break;
		case 121: rise.siege.pauseDays = fraction; break;
		case 122: rise.siege.guardKillsPerHour = fraction; break;
		case 123: rise.grab.reachUnits = fraction; break;
		case 124: rise.grab.lungeUnits = fraction; break;
		case 125: rise.grab.lungeSeconds = fraction; break;
		case 126: rise.noRiseInCities = on; break;
		case 127: rise.safeguard.raiseAtHome = on; break;
		case 128: rise.night.enabled = on; break;
		case 129: rise.night.speedMultiplier = fraction; break;
		case 130: rise.night.spawnPercent = whole; break;
		case 131: rise.night.hordeLeadPercent = whole; break;
		case 132: rise.night.startHour = fraction; break;
		case 133: rise.night.endHour = fraction; break;
		case 134: rise.horde.noise.enabled = on; break;
		case 135: rise.horde.noise.radius = fraction; break;
		case 136: rise.horde.noise.searchSeconds = fraction; break;
		case 137: rise.camps.enabled = on; break;
		case 138: rise.camps.peoplePerCamp = whole; break;
		case 139: rise.camps.womenPercent = whole; break;
		case 140: rise.camps.trader = on; break;
		case 141: rise.camps.rewardFame = on; break;
		case 142: rise.camps.famePoints = whole; break;
		case 143: rise.camps.rewardGold = on; break;
		case 144: rise.camps.gold = whole; break;
		case 145: rise.camps.attackChancePercent = whole; break;
		case 146: rise.camps.walkersMin = whole; break;
		case 147: rise.camps.walkersMax = whole; break;
		case 148: rise.camps.campsAttackedAtOnce = whole; break;
		case 149: rise.camps.refugeeKillsPerHour = fraction; break;
		case 150: rise.camps.lossPercentPerHour = whole; break;
		case 151: rise.camps.returnChancePercent = whole; break;
		case 152: rise.camps.campCount = whole; break;
		case 153: rise.grab.shockSound = whole; break;
		case 154: rise.slowAggression.enabled = on; break;
		case 155: rise.slowAggression.level = static_cast<SlowAggressionLevel>(whole > 3 ? 1 : whole); break;
		case 156: rise.slowAggression.secondsMin = static_cast<float>(whole); break;
		case 157: rise.slowAggression.secondsMax = static_cast<float>(whole); break;
		case 158: rise.slowAggression.units = static_cast<float>(whole); break;
		case 159: replace.keepQuestLists = on; break;
		case 160: rise.camps.rewardThings = on; break;
		case 161: rise.camps.escort = on; break;
		case 162: rise.camps.escortFame = whole; break;
		case 163: rise.camps.escortGold = whole; break;
		case 164: rise.camps.escortBehindUnits = static_cast<float>(whole); break;
		case 165: rise.camps.escortStayDays = static_cast<float>(whole); break;
		case 166: rise.end.othersEndPercent = whole; break;
		case 167: rise.disguise.enabled = on; break;
		case 168: rise.disguise.seconds = static_cast<float>(whole); break;
		case 169: rise.camps.escortIntoCity = on; break;
		case 170: rise.horde.gateHordesMost = whole; break;
		case 171: rise.horde.gateNearUnits = static_cast<float>(whole); break;
		case 172: rise.horde.loadedGrid = whole; break;
		case 173: rise.slowAggression.calmSeconds = static_cast<float>(whole); break;
		case 174: rise.dungeonsOnlyChosen = on; break;
		case 175: rise.dungeons.enabled = on; break;
		case 176: rise.dungeons.activeMin = whole; break;
		case 177: rise.dungeons.activeMax = whole; break;
		case 178: rise.dungeons.weightRefuge = whole; break;
		case 179: rise.dungeons.weightBesieged = whole; break;
		case 180: rise.dungeons.weightToClear = whole; break;
		case 181: rise.dungeons.weightWalkerDungeon = whole; break;
		case 182: rise.dungeons.spacingUnits = static_cast<float>(whole); break;
		case 183: rise.dungeons.peopleMin = whole; break;
		case 184: rise.dungeons.peopleMax = whole; break;
		case 185: rise.dungeons.attackChancePercent = whole; break;
		case 186: rise.dungeons.walkersMin = whole; break;
		case 187: rise.dungeons.walkersMax = whole; break;
		case 188: rise.dungeons.refugeeKillsPerHour = fraction; break;
		case 189: rise.dungeons.lossPercentPerHour = whole; break;
		case 190: rise.dungeons.leaveAfterDays = static_cast<float>(whole); break;
		case 191: rise.dungeons.walkersAtOnce = whole; break;
		case 192: rise.dungeons.journal = on; break;
		case 193: rise.dungeons.rumors = on; break;
		case 194: rise.dungeons.messengerHours = static_cast<float>(whole); break;
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
// `empty` (LinkLooksEmpty): this version but no values - the INI's go in.
inline LinkStart StartOf(bool linkLoaded, double storedVersion, bool empty = false) {
	return linkLoaded && storedVersion == kLinkVersion && !empty ? LinkStart::ReadLink : LinkStart::WriteIni;
}

// 2026-11-11 (Nadi's Windows log): loads read version 32 and every setting 0.
// Values in which every setting with a least above 0 reads 0 hold nothing
// worth taking. A guard - the cause is open (docs/walkers-spec.md, LINK).
inline bool LinkLooksEmpty(const double* values, UInt32 count) {
	UInt32 checked = 0;
	for (UInt32 i = 0; i < count && i < kLinkSettingCount; ++i) {
		if (kLinkSettings[i].min <= 0.0f) {
			continue;
		}
		++checked;
		if (values[i] != 0.0) {
			return false;
		}
	}
	return checked > 0;
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
