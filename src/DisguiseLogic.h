#pragma once

// Walker blood as a disguise, process-free (docs/walkers-spec.md 4.20.1;
// designed with Nadi 2026-11-05, "go 4.20.1" 2026-11-09 with: the time in
// real minutes - "echte" - a confirm dialog - "ok mit confirm dialog, also
// iwas wie do you want to rub ..." - the walkers attacking stop - "ok" - and
// any rain ends it - "jo").
//
// The player, sneaking, activates a dead walker: a dialog asks whether he
// rubs its blood on; yes, and for Seconds real seconds no walker turns on
// him, none grabs him, those fighting him stop. It ends when the time runs
// out, when he strikes a walker, swims or stands in the rain. In VR OBVR's
// gesture (a hand in the corpse, then over the chest) smears without the
// dialog ('WGBL'), and OBVR shows it while it lasts ('WGDS').

#include "Types.h"

namespace walkers {

// The ability "Walker Blood" in the ESP (tools/esp-write.py reads this line):
// shown among the player's effects while it lasts - after the camps'
// extras (CampLogic.h kCampExtraEnd 0xA45), before the grab's shock sounds.
inline constexpr UInt32 kDisguiseAbilityLocalId = 0xA45;

// [Disguise] in OblivionWalkers.ini.
struct DisguiseSettings {
	bool enabled = true;     // Enabled
	float seconds = 180.0f;  // Seconds: real seconds (3 minutes)
};

struct DisguiseState {
	bool on = false;
	bool asking = false;  // the dialog open
	float left = 0.0f;    // real seconds left
};

enum class DisguiseEnd : UInt8 { None, Time, HitWalker, Water, Rain, Off };

inline const char* DisguiseEndText(DisguiseEnd end) {
	switch (end) {
		case DisguiseEnd::Time: return "The walker blood has dried - they know you again.";
		case DisguiseEnd::HitWalker: return "You struck one of them - the walkers know you again.";
		case DisguiseEnd::Water: return "The water washed the walker blood off.";
		case DisguiseEnd::Rain: return "The rain washed the walker blood off.";
		case DisguiseEnd::Off: return "The walker blood is gone.";
		case DisguiseEnd::None: break;
	}
	return "";
}

// Whether activating asks: the player activated it, sneaking, a dead walker,
// the rule on, no dialog open. Disguised already, it asks again (renewed).
inline bool DisguiseOffered(const DisguiseSettings& s, const DisguiseState& st, bool byPlayer, bool walker, bool dead,
                            bool sneaking) {
	return s.enabled && !st.asking && byPlayer && walker && dead && sneaking;
}

// OBVR's smear gesture ('WGBL'): only a dead walker's blood disguises - the
// corpse it names is checked here, as the dialog's (2026-11-11: a person
// beaten in a hand-to-hand fight fell by the player's hands and the disguise
// came on).
inline bool SmearAccepted(const DisguiseSettings& s, bool walker, bool dead) { return s.enabled && walker && dead; }

// Yes (or OBVR's gesture): on, the full time.
inline void BeginDisguise(DisguiseState& st, const DisguiseSettings& s) {
	st.asking = false;
	st.on = true;
	st.left = s.seconds;
}

namespace disguise_detail {
inline DisguiseEnd End(DisguiseState& st, DisguiseEnd why) {
	st.on = false;
	st.left = 0.0f;
	return why;
}
}  // namespace disguise_detail

// Each frame, `dt` real seconds: the water or the rain wash it off, then the
// time runs out; switched off, it ends.
inline DisguiseEnd StepDisguise(DisguiseState& st, const DisguiseSettings& s, float dt, bool swimming, bool raining) {
	if (!st.on) {
		return DisguiseEnd::None;
	}
	if (!s.enabled) {
		return disguise_detail::End(st, DisguiseEnd::Off);
	}
	if (swimming) {
		return disguise_detail::End(st, DisguiseEnd::Water);
	}
	if (raining) {
		return disguise_detail::End(st, DisguiseEnd::Rain);
	}
	st.left -= dt > 0.0f ? dt : 0.0f;
	return st.left <= 0.0f ? disguise_detail::End(st, DisguiseEnd::Time) : DisguiseEnd::None;
}

// A hit (xOBSE's OnHit, OBVR's blows among them): the player striking a
// walker ends it.
inline DisguiseEnd DisguiseHit(DisguiseState& st, bool byPlayer, bool onWalker) {
	return st.on && byPlayer && onWalker ? disguise_detail::End(st, DisguiseEnd::HitWalker) : DisguiseEnd::None;
}

// The slow aggression's rule (4.21): no walker turns on the player while he
// is disguised.
inline bool WalkerMayTurnOn(bool disguised, bool targetIsPlayer) { return !(disguised && targetIsPlayer); }

// The walker records' aggression: 0 with the slow aggression on (4.21) or
// while disguised, their own otherwise (with the slow aggression off the
// engine's own rule makes them attack - 0 holds it back).
inline UInt32 DisguisedAggression(bool slow, bool disguised, UInt32 own) { return slow || disguised ? 0u : own; }

// The current weather's precipitation (TESWeather +0x53, xOBSE GameForms.h:
// 1 pleasant, 2 cloudy, 3 rainy, 4 snow): rain only outdoors.
inline bool Raining(UInt8 precipitation, bool outdoors) { return outdoors && precipitation == 3; }

// The process's movement flags (16 bits at +0x1FC; xOBSE GameProcess.h
// BaseProcess kMovementFlag_*): 0x400 sneaking, overridden by 0x800
// swimming.
inline bool Swimming(UInt32 flags) { return (flags & 0x800) != 0; }
inline bool Sneaking(UInt32 flags) { return (flags & 0x400) != 0 && !Swimming(flags); }

// To OBVR ('WGDS'): one word, 1 disguised, 0 no more - its dark rim and
// blood on the hands.
inline constexpr UInt32 kDisguiseStateMessage = 0x53444757;
inline UInt32 DisguiseStateMessage(bool on, UInt32* word) {
	*word = on ? 1u : 0u;
	return 4;
}

// From OBVR ('WGBL'): one word, the corpse its gesture smeared from.
inline constexpr UInt32 kSmearMessage = 0x4C424757;
inline bool SmearOf(const void* data, UInt32 length, UInt32& corpse) {
	if (data == nullptr || length < 4) {
		return false;
	}
	corpse = *static_cast<const UInt32*>(data);
	return true;
}

}  // namespace walkers
