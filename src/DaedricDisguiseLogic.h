#pragma once

// The Daedric disguise, process-free (docs/walkers-spec.md 4.22 idea 7; Nadi,
// 2026-11-16: "go 7", and once they know the player, "60 s ohne Daedra"):
// in a full set of Daedric armor - helmet, cuirass, greaves, gauntlets and
// boots, any armor whose model lies in a folder named Daedric... (read at
// run time, DaedricModelSlots - the DLCs' and mods' pieces too; Nadi,
// 2026-11-18: "mach laufzeit") - the Daedra take the player for one of
// theirs, until he strikes one or comes within NearDistance. Then they know
// him; with none of them within kDaedricForgetUnits for ForgetSeconds they
// forget, and the full set fools them again. Taking a piece off ends it.
//
// How (DaedricDisguise.cpp): while fooled the player stands in
// Oblivion.esm's DaedraFaction (00053FED) and DremoraFaction (0003E9A6),
// which react +100 to each other and to themselves (FACT XNAM, read
// 2026-11-16) - a Daedra attacks when its disposition falls more than five
// points below its aggression (UESP, Oblivion:Aggression), so a friend of
// +100 is left alone. Every Daedra in those factions is fooled, the game's
// own (a gate's) as well as the plugin's. With [Daedra] Mode 2 (allies) the walkers
// stand in DaedraFaction too (Replace.cpp) - they are fooled as well.

#include "Types.h"

namespace walkers {

inline constexpr UInt32 kDaedraFactionId = 0x00053FED;
inline constexpr UInt32 kDremoraFactionId = 0x0003E9A6;
inline constexpr UInt8 kDaedricFullSlots = 0x3D;  // head, upper and lower body, hands, feet
// A Daedra this near keeps them knowing the player (some 58 m, a cell).
inline constexpr float kDaedricForgetUnits = 4096.0f;

// [Disguise] in OblivionWalkers.ini.
struct DaedricSettings {
	bool enabled = true;          // Daedric
	float nearUnits = 200.0f;     // DaedricNearDistance: closer, they see through it (some 3 m)
	float forgetSeconds = 60.0f;  // DaedricForgetSeconds: known, none near this long - forgotten
};

struct DaedricState {
	bool on = false;     // fooled: the player in their factions
	bool known = false;  // they know him
	float forgetLeft = 0.0f;
};

enum class DaedricChange : UInt8 { None, Fooled, Struck, TooNear, SetOff, Off };

inline bool FullDaedricSet(UInt8 slots) { return (slots & kDaedricFullSlots) == kDaedricFullSlots; }

// The body slots a worn armor counts for in the set: its biped slots
// (TESBipedModelForm partMask, 0x01 head .. 0x20 feet; the hair slot and a
// shield's 0x2000 dropped) when its male model lies in a folder whose name
// begins with "daedric", any case - Oblivion.esm's Armor\Daedric\ (62
// pieces read 2026-11-16 by the file table this replaces), the Daedric
// bracers, a mod's Armor\X\DaedricPlate\. 0 for any other.
inline UInt8 DaedricModelSlots(const char* path, UInt16 partMask) {
	if (path == nullptr) {
		return 0;
	}
	static constexpr char kWord[] = "daedric";
	bool atFolderStart = true;
	for (const char* p = path; *p != '\0'; ++p) {
		if (atFolderStart) {
			UInt32 i = 0;
			while (kWord[i] != '\0' && p[i] != '\0' && (p[i] | 0x20) == kWord[i]) {
				++i;
			}
			if (kWord[i] == '\0') {
				// a folder, not the file: a slash follows somewhere in this name
				for (const char* q = p + i; *q != '\0'; ++q) {
					if (*q == '\\' || *q == '/') {
						return static_cast<UInt8>(partMask & kDaedricFullSlots);
					}
				}
				return 0;
			}
		}
		atFolderStart = *p == '\\' || *p == '/';
	}
	return 0;
}

inline bool DaedricJoins(DaedricChange c) { return c == DaedricChange::Fooled; }
inline bool DaedricLeaves(DaedricChange c) {
	return c == DaedricChange::Struck || c == DaedricChange::TooNear || c == DaedricChange::SetOff ||
	       c == DaedricChange::Off;
}

inline const char* DaedricChangeText(DaedricChange c) {
	switch (c) {
		case DaedricChange::Fooled: return "In your Daedric armor the Daedra take you for one of theirs.";
		case DaedricChange::Struck: return "You struck one of them - the Daedra know you for what you are.";
		case DaedricChange::TooNear: return "Too close - the Daedra see through your armor.";
		case DaedricChange::SetOff: return "Without the full Daedric armor the Daedra know you again.";
		case DaedricChange::Off: return "The Daedra know you again.";
		case DaedricChange::None: break;
	}
	return "";
}

// One look, `dt` seconds since the last: `fullSet` the player wears it,
// `nearest` the nearest living Daedra's distance (-1 none), `struck` the
// player struck a Daedra since the last look.
inline DaedricChange StepDaedric(DaedricState& st, const DaedricSettings& s, bool fullSet, float nearest, bool struck,
                                 float dt) {
	if (!s.enabled) {
		const bool was = st.on;
		st = DaedricState{};
		return was ? DaedricChange::Off : DaedricChange::None;
	}
	if (struck) {
		const bool was = st.on;
		st.on = false;
		st.known = true;
		st.forgetLeft = s.forgetSeconds;
		return was ? DaedricChange::Struck : DaedricChange::None;
	}
	if (st.known) {
		if (nearest >= 0.0f && nearest <= kDaedricForgetUnits) {
			st.forgetLeft = s.forgetSeconds;
			return DaedricChange::None;
		}
		st.forgetLeft -= dt > 0.0f ? dt : 0.0f;
		if (st.forgetLeft > 0.0f) {
			return DaedricChange::None;
		}
		st.known = false;
		st.forgetLeft = 0.0f;
	}
	if (!fullSet) {
		const bool was = st.on;
		st.on = false;
		return was ? DaedricChange::SetOff : DaedricChange::None;
	}
	if (nearest >= 0.0f && nearest <= s.nearUnits) {
		if (!st.on) {
			return DaedricChange::None;
		}
		st.on = false;
		st.known = true;
		st.forgetLeft = s.forgetSeconds;
		return DaedricChange::TooNear;
	}
	if (st.on) {
		return DaedricChange::None;
	}
	st.on = true;
	return DaedricChange::Fooled;
}

// A blow that gives the player away: his, on a Daedra - or on a walker while
// the walkers are the Daedra's allies ([Daedra] Mode=2 (allies); Nadi, 2026-11-17:
// "ja!").
inline bool DaedricStruck(bool byPlayer, bool onDaedra, bool onAlliedWalker) {
	return byPlayer && (onDaedra || onAlliedWalker);
}

}  // namespace walkers
