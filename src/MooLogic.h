#pragma once

// Keeping Maskar's Oblivion Overhaul from raising dead walkers, process-free
// (docs/walkers-spec.md 7a; Nadi, 2026-10-25: "go" for taking MOO's brain
// and token off a dead walker, so MOO's INI need not be changed).
//
// Read in MOO's ESP (its script sources, 2026-10-25):
// - MOOInitNpcTokenForCreatureFunctionScript gives a creature whose model
//   path holds "\Zombie\" - every walker, copied from CreatureZombie1..4 -
//   the brain MOOZombieBrain (INGR, local 0F8B6D) at ini_zombie_resurrect
//   percent (MOOAddZombieBrainFunctionScript).
// - Its OnDeath handler (SetEventHandler "OnDeath"
//   MOOOnDeathByCreatureFunctionScript) gives a dead "\Zombie\Skeleton.nif"
//   the token MOOTokenZombie (CLOT, local 042C18).
// - The token's script waits until GetTimeDead passes 0.003 * TimeScale *
//   Rand(1, 5) game hours - 10.8 to 54 s of real time at any time scale -
//   then raises the dead body (Resurrect 1) if it still carries the brain
//   (ini_zombie_brain, on by default) or at ini_zombie_resurrect percent,
//   and removes itself.
//
// So a dead walker loses the token (and the brain with it): the token's
// script never runs. Twice - at the next tick, after MOO's handler of the
// same death has run, and 2 s on in case it ran after ours - both well
// before the token's first look.

#include "Types.h"

namespace walkers {

inline constexpr const char* kMooEsp = "Maskar's Oblivion Overhaul.esp";
inline constexpr UInt32 kMooTokenZombieLocalId = 0x042C18;
inline constexpr UInt32 kMooZombieBrainLocalId = 0x0F8B6D;
// The record types of the two (TESForm typeID at +0x04): Clothing and
// Ingredient, counted in xOBSE's FormType enum from NPC at 0x23 (derived;
// the plugin refuses and logs the types read when they differ).
inline constexpr UInt8 kFormTypeClothing = 0x16;
inline constexpr UInt8 kFormTypeIngredient = 0x19;
inline constexpr float kMooSecondPassSeconds = 2.0f;
inline constexpr UInt32 kMooStripMost = 32;

// [Zombies] BlockMooResurrect: a dead walker loses MOO's token and brain.
inline bool StripsMoo(bool setting, bool mooLoaded, bool isWalker) { return setting && mooLoaded && isWalker; }

// One of MOO's forms at its load index; 0 when MOO is not loaded (an index
// above 0xFF).
inline UInt32 MooFormId(UInt32 modIndex, UInt32 localId) {
	return modIndex > 0xFF ? 0u : (modIndex << 24) | (localId & 0x00FFFFFF);
}

// The dead walkers still to be stripped, each with its seconds since noted
// and its passes done.
struct MooStrip {
	UInt32 ids[kMooStripMost] = {};
	float seconds[kMooStripMost] = {};
	UInt8 passes[kMooStripMost] = {};
	UInt32 count = 0;
};

// False when full (the caller logs it); a walker already noted stays once.
inline bool NoteMooStrip(MooStrip& strip, UInt32 id) {
	for (UInt32 i = 0; i < strip.count; ++i) {
		if (strip.ids[i] == id) {
			return true;
		}
	}
	if (strip.count >= kMooStripMost) {
		return false;
	}
	strip.ids[strip.count] = id;
	strip.seconds[strip.count] = 0.0f;
	strip.passes[strip.count] = 0;
	++strip.count;
	return true;
}

// The walkers due a pass this tick, at most `most` into `due` (and, with
// `pass`, which pass each is, 1 or 2); those beyond wait for the next tick.
// A walker is dropped after its second pass.
inline UInt32 StepMooStrip(MooStrip& strip, float dt, UInt32* due, UInt32 most, UInt8* pass = nullptr) {
	UInt32 handed = 0;
	UInt32 kept = 0;
	for (UInt32 i = 0; i < strip.count; ++i) {
		strip.seconds[i] += dt;
		const bool isDue = strip.passes[i] == 0 || strip.seconds[i] >= kMooSecondPassSeconds;
		if (isDue && handed < most) {
			++strip.passes[i];
			if (pass != nullptr) {
				pass[handed] = strip.passes[i];
			}
			due[handed++] = strip.ids[i];
		}
		if (strip.passes[i] < 2) {
			strip.ids[kept] = strip.ids[i];
			strip.seconds[kept] = strip.seconds[i];
			strip.passes[kept] = strip.passes[i];
			++kept;
		}
	}
	strip.count = kept;
	return handed;
}

}  // namespace walkers
