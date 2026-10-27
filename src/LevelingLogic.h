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

// [Zombies] NoLevelingXP: a walker's death gives Ultimate Leveling nothing.
inline bool HoldsLevelingXp(bool setting, bool levelingLoaded, bool isWalker) {
	return setting && levelingLoaded && isWalker;
}

}  // namespace walkers
