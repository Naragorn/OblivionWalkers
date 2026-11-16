#pragma once

// The INI's keys by clearer names (docs/walkers-spec.md 8b; Nadi,
// 2026-11-15: "ein humanize run auf alle ini + LINK settings um texte und
// schlüssel zu humanizen", 2026-11-16 "go 8"). A renamed key is read under
// its new name; an INI that still has only the old one keeps its value - the
// reader falls back to the old name (Plugin.cpp IniInt, IniString,
// IniFloat). [Zombies] is [Walkers] now: every key of the old section is
// looked for there too (kSectionRenames).

#include <cstring>

#include "Types.h"

namespace walkers {

struct IniRename {
	const char* section;     // the new name
	const char* key;
	const char* oldSection;  // the name an older INI still has
	const char* oldKey;
};

inline constexpr IniRename kIniRenames[] = {
	{"Walkers", "WalkOnly", "Zombies", "Walk"},
	{"Walkers", "PaceSpreadPercent", "Zombies", "SpeedRandomPercent"},
	{"Walkers", "KillsPerFamePoint", "Zombies", "FamePerKills"},
	{"Walkers", "RenameEveryZombie", "Zombies", "RenameVanilla"},
	{"Walkers", "RiseAgainRandomSeconds", "Zombies", "RiseAgainJitterSeconds"},
	{"Walkers", "OthersKillsEndPercent", "Zombies", "OthersEndPercent"},
	{"Walkers", "SlowAggressionDistance", "Zombies", "SlowAggressionUnits"},
	{"Rise", "RandomExtraSeconds", "Rise", "JitterSeconds"},
	{"Rise", "OnlyNamelessPeople", "Rise", "OnlyGeneric"},
	{"Rise", "WalkerRace", "Rise", "ZombieRace"},
	{"Rise", "GroanChancePercent", "Rise", "SoundAtGetUpPercent"},
	{"Horde", "JoinDistance", "Horde", "JoinRadius"},
	{"Horde", "LeadDistance", "Horde", "LeadUnits"},
	{"Horde", "NewHeadingAfterSeconds", "Horde", "StaleSeconds"},
	{"Horde", "WalkersPerLeader", "Horde", "GroupMost"},
	{"Horde", "CityDistance", "Horde", "CityRange"},
	{"Horde", "HoldBeforeGateDistance", "Horde", "GateHoldUnits"},
	{"Horde", "HordesPerGateMost", "Horde", "GateHordesMost"},
	{"Horde", "HoldOnlyWithinDistance", "Horde", "GateNearUnits"},
	{"Guards", "WalkersPerGuard", "Guards", "ZombiesPerGuard"},
	{"Guards", "GateDistance", "Guards", "Radius"},
	{"Guards", "GoInAfterSeconds", "Guards", "ClearSeconds"},
	{"Grab", "HarderPerWalkerPercent", "Grab", "StruggleCrowdStep"},
	{"Grab", "CrowdMost", "Grab", "StruggleCrowdMost"},
	{"Grab", "CrowdDistance", "Grab", "StruggleCrowdRadius"},
	{"Grab", "SecondsBetweenPresses", "Grab", "StruggleGapSeconds"},
	{"Grab", "BitesForgottenAfterSeconds", "Grab", "BiteResetSeconds"},
	{"Corpses", "TakeAwayBeyond", "Corpses", "CorpseUnits"},
	{"Corpses", "TakeAwayWithThingsBeyond", "Corpses", "CorpseLootUnits"},
	{"Night", "TwilightHours", "Night", "RampHours"},
	{"Night", "HordeRoamPercent", "Night", "HordeLeadPercent"},
	{"Noise", "HeardWithin", "Noise", "Radius"},
	{"Safeguard", "NoWalkerWithin", "Safeguard", "Radius"},
	{"Survivors", "CampDistanceApart", "Survivors", "CampSpacingUnits"},
	{"Survivors", "LeadStraysHomeBeyond", "Survivors", "LeashUnits"},
	{"Survivors", "EscortLeftBehindDistance", "Survivors", "EscortBehindUnits"},
	{"Dungeons", "DistanceApart", "Dungeons", "SpacingUnits"},
	{"Replace", "WalkersPerSpawnMin", "Replace", "MinCount"},
	{"Replace", "WalkersPerSpawnMax", "Replace", "MaxCount"},
	{"Replace", "ExtraSpawnPercent", "Replace", "ExtraPercent"},
};
inline constexpr UInt32 kIniRenameCount = sizeof(kIniRenames) / sizeof(kIniRenames[0]);

struct SectionRename {
	const char* section;
	const char* oldSection;
};
inline constexpr SectionRename kSectionRenames[] = {{"Walkers", "Zombies"}};

// The older name of `section`/`key`, false when it has none: a renamed key,
// or a key of a renamed section under its own name.
inline bool OldIniName(const char* section, const char* key, const char*& oldSection, const char*& oldKey) {
	for (const IniRename& r : kIniRenames) {
		if (std::strcmp(r.section, section) == 0 && std::strcmp(r.key, key) == 0) {
			oldSection = r.oldSection;
			oldKey = r.oldKey;
			return true;
		}
	}
	for (const SectionRename& r : kSectionRenames) {
		if (std::strcmp(r.section, section) == 0) {
			oldSection = r.oldSection;
			oldKey = key;
			return true;
		}
	}
	return false;
}

}  // namespace walkers
