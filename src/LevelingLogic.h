#pragma once

// Walker kills give no experience in Maskar's Ultimate Leveling, process-free
// (docs/walkers-spec.md 4.3b; Nadi, 2026-10-27: "füge eine option hinzu,
// dass für diese mod zombie kills keine erfahrung bringen").
//
// Read in Ultimate Leveling.esp's script sources (2026-10-27):
// - ULVLGetGameLoadedEventHandlersFunctionScript sets the OnDeath handler
//   ULVLOnDeathFunctionScript when its experience system is on
//   (ini_leveling_system 2), at every game load.
// - The handler's experience is basexp * levelmod * toughnessmod * share
//   (ULVL2StatsOnDeathGetXPFunctionScript); the toughness modifier
//   multiplies by (3 * Confidence + Aggression) / 400 of the dead
//   (ULVL2StatsOnDeathGetToughnessModifierFunctionScript, GetAV at the
//   time of the handler), and ULVL2StatsOnDeathFunctionScript gives the
//   experience only "if ULVL.XPmod > 0".
// So the plugin's own OnDeath handler sets a dead walker's Confidence and
// Aggression to 0: the experience comes out 0, none is given, no message.
// That needs the plugin's handler to run first - xOBSE calls an event's
// handlers in the order they were set (EventManager's CallbackList, via
// deepwiki on llde/xOBSE), and the plugin sets its own at the main menu,
// before any game load (derived, not yet seen in a log).

#include "Types.h"

namespace walkers {

inline constexpr const char* kLevelingEsp = "Ultimate Leveling.esp";
inline constexpr const char* kLevelingHoldLines[] = {"SetAV Confidence 0", "SetAV Aggression 0"};
inline constexpr UInt32 kLevelingHoldLineCount = 2;
// The check in the log: Ultimate Leveling's quest ULVL (its local 000ED3,
// script ULVLScript, read in Ultimate Leveling.esp 2026-10-27) keeps the
// experience gathered in its variable XP.
inline constexpr UInt32 kLevelingQuestLocalId = 0x000ED3;
inline constexpr const char* kLevelingXpVariable = "XP";

// Oblivion XP Update 4.5 (Nexus 51817), read in its ESP's script sources
// 2026-10-27: its OnDeath handler OBXPfnOnDeath (set by ObXPMainScript at
// the game load, so after the plugin's) gives a kill by the player
// baseXP * bonusXP * ..., bonusXP = the dead's base Health + Agility +
// Endurance + Intelligence + Personality + Speed + Strength + Willpower over
// the player's, times multXPKillBonus (1 by default). With the dead
// walker's eight at 0 the kill gives 0 XP. Unlike Ultimate Leveling it
// still shows its line ("Gained 0.00 XP. Killed Walker") - it has no check
// for nothing gained. Its quest ObXPMain (local 000ED3) keeps currentXP.
inline constexpr const char* kObXpEsp = "Oblivion XP.esp";
inline constexpr const char* kObXpHoldLines[] = {"SetAV Health 0",      "SetAV Agility 0",     "SetAV Endurance 0",
                                                 "SetAV Intelligence 0", "SetAV Personality 0", "SetAV Speed 0",
                                                 "SetAV Strength 0",     "SetAV Willpower 0"};
inline constexpr UInt32 kObXpLineCount = 8;
inline constexpr UInt32 kObXpQuestLocalId = 0x000ED3;
inline constexpr const char* kObXpXpVariable = "currentXP";

// [Zombies] NoLevelingXP: a walker's death gives Ultimate Leveling and
// Oblivion XP nothing - one switch for both.
inline bool HoldsLevelingXp(bool setting, bool levelingLoaded, bool isWalker) {
	return setting && levelingLoaded && isWalker;
}

}  // namespace walkers
