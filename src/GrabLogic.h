#pragma once

// Grabbed by walkers, process-free (docs/walkers-spec.md 4.19; Nadi,
// 2026-10-28: "wenn von zuvielen zombies umzingelt, greifen oder halten die
// einen fest und iwann ist man tot... also so wie Saints and Sinners VR";
// "go a + b", the bites the player takes settable, 3 by default, no third
// walker at the neck, a grab only at a landed hit).
//
// A walker's landed hit on the player becomes, by the chance, a grab of the
// arm on the walker's side - two arms, so two holders at most, as Saints &
// Sinners has it (Skydance's tech blog, 2020-04-24: "by either arm
// (including two Walkers at once)"). One holder slows the player to a walk,
// two root the player. Each holder bites on its own clock; the player takes
// BitesToDie bites, the last one kills (god mode leaves a point). A holder
// lets go when cut down, struck off (StruggleHits of the player's blows on
// it), when the player gets too far from it (with one holder the player can
// still walk), or when OBVR asks (the shake of the held arm in VR, through
// the message below). Free for BiteResetSeconds, the bites are forgotten.

#include <cmath>
#include <cstdio>

#include "Types.h"

namespace walkers {

inline constexpr UInt32 kGrabHolders = 2;

struct GrabSettings {
	bool enabled = true;
	UInt32 chancePercent = 50;     // of a walker's landed hit on the player
	UInt32 bitesToDie = 3;         // the last bite the player takes kills
	float biteSeconds = 4.0f;      // each holder bites this long after it took hold, and again
	UInt32 biteHealthPercent = 20; // of the player's base health, a bite before the last
	UInt32 struggleHits = 2;       // the player's blows on a holder that free the arm (0 counts as 1)
	float tearFreeDistance = 160.0f;  // units between player and holder that tear the arm free
	float biteResetSeconds = 30.0f;   // free this long, the bites are forgotten
	// A test switch (harness/grab-anims.txt): free this long, the nearest
	// walker is moved before the player and grabs as at a hit - so the
	// animations can be looked at. 0 (the default): never.
	float testGrabSeconds = 0.0f;
	// Flat (Nadi, 2026-10-28, "go A + B"): mashing the Block key wrenches the
	// longest holder off - none more than StruggleGapSeconds apart, each
	// costing StruggleFatigue; how many, StrugglePressesNeeded (0: off).
	// Messages: a line on the screen at a grab, a bite and breaking free.
	UInt32 strugglePresses = 4;         // at the base: one walker, fresh
	float struggleCrowdStep = 0.5f;     // times this more per walker near beyond the first
	float struggleCrowdMost = 3.0f;     // the crowd's factor at most
	float struggleCrowdRadius = 300.0f; // units: the walkers counted
	float struggleGapSeconds = 0.8f;
	UInt32 struggleControl = 6;  // xOBSE GameOSDepend.h MappableControl: kControl_Block
	float struggleFatigue = 5.0f;
	bool messages = true;
};

// Whether the test grab is due: `clock` runs while nobody holds.
inline bool TestGrabDue(const GrabSettings& s, float& clock, float dt, bool anyoneHolds) {
	if (s.testGrabSeconds <= 0.0f || anyoneHolds) {
		clock = 0.0f;
		return false;
	}
	clock += dt;
	if (clock < s.testGrabSeconds) {
		return false;
	}
	clock = 0.0f;
	return true;
}

enum class Arm : UInt8 { None = 0, Left = 1, Right = 2 };

struct Holder {
	UInt32 refId = 0;
	Arm arm = Arm::None;
	float biteClock = 0.0f;
	UInt32 hitsTaken = 0;
};

struct GrabState {
	Holder holders[kGrabHolders];
	UInt32 count = 0;
	UInt32 bites = 0;
	float freeSeconds = 0.0f;
	bool over = false;  // the last bite came: nothing more until a reset
};

inline void ResetGrab(GrabState& g) { g = GrabState{}; }

// The side of the player a walker stands on: `headingRadians` the player's
// heading (Oblivion's z angle: 0 looks along +y, its right is +x), dx and dy
// the walker's place minus the player's.
inline Arm SideOf(float headingRadians, float dx, float dy) {
	// The forward is (sin h, cos h), the right (cos h, -sin h).
	const float s = std::sin(headingRadians);
	const float c = std::cos(headingRadians);
	return dx * c - dy * s >= 0.0f ? Arm::Right : Arm::Left;
}

inline bool Holds(const GrabState& g, UInt32 refId) {
	for (UInt32 i = 0; i < g.count; ++i) {
		if (g.holders[i].refId == refId) {
			return true;
		}
	}
	return false;
}

inline bool ArmHeld(const GrabState& g, Arm arm) {
	for (UInt32 i = 0; i < g.count; ++i) {
		if (g.holders[i].arm == arm) {
			return true;
		}
	}
	return false;
}

// The arm a walker on `side` takes: its own side if free, else the other,
// else none. No side known: the right first.
inline Arm ArmFor(const GrabState& g, Arm side) {
	const Arm first = side == Arm::None ? Arm::Right : side;
	const Arm other = first == Arm::Right ? Arm::Left : Arm::Right;
	if (!ArmHeld(g, first)) {
		return first;
	}
	return ArmHeld(g, other) ? Arm::None : other;
}

enum class GrabRefusal : UInt8 { None, NoWalker, Off, AlreadyHolding, BothArmsHeld, NotRolled };

// A walker's landed hit on the player; `roll` 0..99. None: it holds `*armOut`.
inline GrabRefusal TryGrab(GrabState& g, const GrabSettings& s, UInt32 refId, Arm side, UInt32 roll, Arm* armOut) {
	if (refId == 0) {
		return GrabRefusal::NoWalker;
	}
	if (!s.enabled) {
		return GrabRefusal::Off;
	}
	if (Holds(g, refId)) {
		return GrabRefusal::AlreadyHolding;
	}
	const Arm arm = ArmFor(g, side);
	if (g.count >= kGrabHolders || arm == Arm::None) {
		return GrabRefusal::BothArmsHeld;
	}
	if (roll >= s.chancePercent) {
		return GrabRefusal::NotRolled;
	}
	Holder& h = g.holders[g.count++];
	h = Holder{};
	h.refId = refId;
	h.arm = arm;
	g.freeSeconds = 0.0f;
	if (armOut != nullptr) {
		*armOut = arm;
	}
	return GrabRefusal::None;
}

enum class PlayerHold : UInt8 { Free, Slowed, Rooted };

inline PlayerHold HoldOf(const GrabState& g) {
	return g.count == 0 ? PlayerHold::Free : (g.count == 1 ? PlayerHold::Slowed : PlayerHold::Rooted);
}

// HighProcess movement flags (+0x1FC; xOBSE GameProcess.h kMovement_*), as
// the walk of 4.4 writes them (RiseLogic.h kMovementWalk, kMovementRun).
inline constexpr UInt16 kMoveForward = 0x0001;
inline constexpr UInt16 kMoveBackward = 0x0002;
inline constexpr UInt16 kMoveLeft = 0x0004;
inline constexpr UInt16 kMoveRight = 0x0008;
inline constexpr UInt16 kMoveWalk = 0x0100;
inline constexpr UInt16 kMoveRun = 0x0200;
inline constexpr UInt16 kMoveSteps = kMoveForward | kMoveBackward | kMoveLeft | kMoveRight;

// The player's flags under the hold: slowed, a run becomes a walk; rooted,
// no step at all.
inline UInt16 HeldPlayerFlags(UInt16 flags, PlayerHold hold) {
	switch (hold) {
		case PlayerHold::Slowed:
			return (flags & kMoveRun) != 0 ? static_cast<UInt16>((flags & ~kMoveRun) | kMoveWalk) : flags;
		case PlayerHold::Rooted:
			return static_cast<UInt16>(flags & ~(kMoveSteps | kMoveWalk | kMoveRun));
		default:
			return flags;
	}
}

// A holder stands at the arm it holds.
inline UInt16 HolderFlags(UInt16 flags) { return static_cast<UInt16>(flags & ~(kMoveSteps | kMoveWalk | kMoveRun)); }

enum class LetGo : UInt8 { Dead, TornFree, Struck, Asked, Gone, SwitchedOff, PlayerDown, Struggled };

// A holder as the game has it this frame, in the order of GrabState::holders.
struct HolderView {
	bool found;      // the reference is still there
	bool dead;
	float distance;  // to the player, units
};

struct GrabEvent {
	enum Kind : UInt8 { Bite, Release };
	Kind kind;
	UInt32 refId;
	Arm arm;
	LetGo why;          // a release's
	bool lethal;        // a bite's: the last the player takes
	UInt32 biteNumber;  // a bite's: counted since the bites were last forgotten
};

namespace grab_detail {

inline void RemoveAt(GrabState& g, UInt32 i) {
	for (UInt32 j = i + 1; j < g.count; ++j) {
		g.holders[j - 1] = g.holders[j];
	}
	--g.count;
	g.holders[g.count] = Holder{};
}

}  // namespace grab_detail

// A frame of `dt` seconds: the holders that let go, then the bites. `views`
// holds one per holder (nullptr when nobody holds). Returns the events
// written to `out`, at most `most`.
inline UInt32 StepGrab(GrabState& g, const GrabSettings& s, float dt, const HolderView* views, GrabEvent* out,
                       UInt32 most) {
	UInt32 n = 0;
	if (g.count == 0) {
		g.freeSeconds += dt;
		if (g.freeSeconds >= s.biteResetSeconds) {
			g.bites = 0;
		}
		return 0;
	}
	g.freeSeconds = 0.0f;
	// Lets go, from the back so the views stay in step with the holders.
	bool letGo[kGrabHolders] = {};
	LetGo why[kGrabHolders] = {};
	for (UInt32 i = 0; i < g.count; ++i) {
		const HolderView& v = views[i];
		if (!v.found) {
			letGo[i] = true;
			why[i] = LetGo::Gone;
		} else if (v.dead) {
			letGo[i] = true;
			why[i] = LetGo::Dead;
		} else if (v.distance > s.tearFreeDistance) {
			letGo[i] = true;
			why[i] = LetGo::TornFree;
		}
	}
	for (UInt32 k = g.count; k > 0; --k) {
		const UInt32 i = k - 1;
		if (!letGo[i]) {
			continue;
		}
		if (n < most) {
			out[n++] = GrabEvent{GrabEvent::Release, g.holders[i].refId, g.holders[i].arm, why[i], false, 0};
		}
		grab_detail::RemoveAt(g, i);
	}
	if (g.over) {
		return n;
	}
	for (UInt32 i = 0; i < g.count && n < most; ++i) {
		Holder& h = g.holders[i];
		h.biteClock += dt;
		if (h.biteClock < s.biteSeconds) {
			continue;
		}
		h.biteClock -= s.biteSeconds;
		++g.bites;
		const bool lethal = g.bites >= s.bitesToDie;
		out[n++] = GrabEvent{GrabEvent::Bite, h.refId, h.arm, LetGo::Dead, lethal, g.bites};
		if (lethal) {
			g.over = true;
			break;
		}
	}
	return n;
}

// The player's landed blow on `refId`: true when it frees the arm (the
// holder is gone from the state then).
inline bool OnStruck(GrabState& g, const GrabSettings& s, UInt32 refId) {
	const UInt32 needed = s.struggleHits == 0 ? 1 : s.struggleHits;
	for (UInt32 i = 0; i < g.count; ++i) {
		if (g.holders[i].refId != refId) {
			continue;
		}
		if (++g.holders[i].hitsTaken < needed) {
			return false;
		}
		grab_detail::RemoveAt(g, i);
		return true;
	}
	return false;
}

// Lets `refId` go (OBVR's request); false when it held nothing.
inline bool ReleaseHolder(GrabState& g, UInt32 refId) {
	for (UInt32 i = 0; i < g.count; ++i) {
		if (g.holders[i].refId == refId) {
			grab_detail::RemoveAt(g, i);
			return true;
		}
	}
	return false;
}

// What a bite takes off the player's health: BiteHealthPercent of the base,
// never more than is left; the last bite all that is left. `*kills` when it
// leaves nothing. God mode leaves a point. Health not read (below 0, NaN):
// nothing.
inline float BiteDamage(const GrabSettings& s, float baseHealth, float currentHealth, bool lethal, bool godMode,
                        bool* kills) {
	*kills = false;
	if (!(currentHealth > 0.0f)) {
		return 0.0f;
	}
	const float share = baseHealth > 0.0f ? baseHealth * static_cast<float>(s.biteHealthPercent) / 100.0f : 0.0f;
	float take = lethal ? currentHealth : (share < currentHealth ? share : currentHealth);
	bool dies = take >= currentHealth;
	if (godMode && dies) {
		take = currentHealth > 1.0f ? currentHealth - 1.0f : 0.0f;
		dies = false;
	}
	*kills = dies;
	return take;
}

// To OBVR ('WGRB' in memory): the holders, so it can pull the held hand and
// free it at a shake. The words: the count, then each holder's reference
// and arm (1 left, 2 right). Sent whenever a holder takes hold or lets go.
inline constexpr UInt32 kGrabStateMessage = 0x42524757;
// From OBVR ('WGFR'): one word, the holder the player shook off.
inline constexpr UInt32 kGrabFreeMessage = 0x52464757;
inline constexpr UInt32 kGrabMessageWords = 1 + 2 * kGrabHolders;

// The state message's words; returns its bytes.
inline UInt32 GrabMessage(const GrabState& g, UInt32* words) {
	words[0] = g.count;
	for (UInt32 i = 0; i < g.count; ++i) {
		words[1 + 2 * i] = g.holders[i].refId;
		words[2 + 2 * i] = static_cast<UInt32>(g.holders[i].arm);
	}
	return 4 * (1 + 2 * g.count);
}

inline bool FreeRequestOf(const void* data, UInt32 length, UInt32& refId) {
	if (data == nullptr || length < 4) {
		return false;
	}
	refId = *static_cast<const UInt32*>(data);
	return true;
}

// The struggle by the key: `down` the key's state this frame. True when the
// bar is full - the longest holder lets go. `*pressed` when the key went
// down this frame (a press costs fatigue).
struct Struggle {
	UInt32 bar = 0;
	float sinceLast = 0.0f;
	bool wasDown = false;
};

// The presses that wrench a holder off (Nadi, 2026-10-28: "wieviel ausdauer
// man noch hat und wie viele walker einen umzingeln"): StrugglePresses times
// the crowd - one more half per walker within StruggleCrowdRadius beyond the
// first, at most StruggleCrowdMost - times the tiredness, 1 fresh to 2 with
// no fatigue left. The time is the bites': the last kills. 0: off.
inline UInt32 StrugglePressesNeeded(const GrabSettings& s, UInt32 walkersNear, float fatigueNow, float fatigueBase) {
	if (s.strugglePresses == 0) {
		return 0;
	}
	const UInt32 extra = walkersNear > 1 ? walkersNear - 1 : 0;
	float crowd = 1.0f + s.struggleCrowdStep * static_cast<float>(extra);
	crowd = crowd > s.struggleCrowdMost ? s.struggleCrowdMost : crowd;
	float left = fatigueBase > 0.0f ? fatigueNow / fatigueBase : 1.0f;
	left = left < 0.0f ? 0.0f : (left > 1.0f ? 1.0f : left);
	const float tired = 2.0f - left;
	const float n = static_cast<float>(s.strugglePresses) * crowd * tired;
	return static_cast<UInt32>(n + 0.5f);
}

inline bool StepStruggle(Struggle& st, const GrabSettings& s, UInt32 needed, bool down, float dt, bool held,
                         bool* pressed) {
	*pressed = false;
	const bool press = down && !st.wasDown;
	st.wasDown = down;
	if (!held || s.strugglePresses == 0 || needed == 0) {
		st.bar = 0;
		st.sinceLast = 0.0f;
		return false;
	}
	st.sinceLast += dt;
	if (!press) {
		return false;
	}
	*pressed = true;
	if (st.sinceLast > s.struggleGapSeconds) {
		st.bar = 0;
	}
	st.sinceLast = 0.0f;
	if (++st.bar < needed) {
		return false;
	}
	st.bar = 0;
	return true;
}

enum class GrabSay : UInt8 { None, Grabbed, Rooted, Bitten, LastBite, Free };

inline GrabSay SayFor(LetGo why) {
	return why == LetGo::Struck || why == LetGo::Struggled || why == LetGo::TornFree || why == LetGo::Asked
	           ? GrabSay::Free
	           : GrabSay::None;
}

// The line on the screen; false when nothing is said (Messages 0, None).
inline bool GrabText(GrabSay say, Arm arm, UInt32 bite, UInt32 needed, const GrabSettings& s, char* out,
                     UInt32 capacity) {
	if (!s.messages || capacity == 0) {
		return false;
	}
	switch (say) {
		case GrabSay::Grabbed:
			if (s.strugglePresses > 0 && needed > 0) {
				std::snprintf(out, capacity, "A walker grabs your %s arm! Strike it or mash Block (%u).",
				              arm == Arm::Left ? "left" : "right", needed);
			} else {
				std::snprintf(out, capacity, "A walker grabs your %s arm! Strike it to break free.",
				              arm == Arm::Left ? "left" : "right");
			}
			return true;
		case GrabSay::Rooted: std::snprintf(out, capacity, "Two walkers hold you - you cannot move!"); return true;
		case GrabSay::Bitten: std::snprintf(out, capacity, "Bitten! (%u of %u)", bite, s.bitesToDie); return true;
		case GrabSay::LastBite: std::snprintf(out, capacity, "The walkers overwhelm you."); return true;
		case GrabSay::Free: std::snprintf(out, capacity, "You break free."); return true;
		default: return false;
	}
}

inline const char* ArmName(Arm arm) {
	return arm == Arm::Left ? "left" : (arm == Arm::Right ? "right" : "no");
}

inline const char* LetGoName(LetGo why) {
	switch (why) {
		case LetGo::Dead: return "cut down";
		case LetGo::TornFree: return "torn free";
		case LetGo::Struck: return "struck off";
		case LetGo::Asked: return "shaken off";
		case LetGo::SwitchedOff: return "the grab switched off";
		case LetGo::PlayerDown: return "the player is down";
		case LetGo::Struggled: return "wrenched off";
		default: return "gone";
	}
}

inline const char* RefusalName(GrabRefusal r) {
	switch (r) {
		case GrabRefusal::None: return "grabbed";
		case GrabRefusal::NoWalker: return "no walker";
		case GrabRefusal::Off: return "off";
		case GrabRefusal::AlreadyHolding: return "holds already";
		case GrabRefusal::BothArmsHeld: return "both arms held";
		default: return "not rolled";
	}
}

}  // namespace walkers
