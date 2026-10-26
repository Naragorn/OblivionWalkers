#pragma once

// The walker's death sound held back while it dies unseen, process-free
// (docs/walkers-spec.md 4.1, the swap's look; Nadi, 2026-10-26: "die walker
// machen bereits einen sound wenn sie unsichtbar spawnen das stört. der
// sound kann kommen aber erst wenn sie daliegen und aufstehen").
//
// Read in Oblivion.exe 1.2.416 with REA (2026-10-26):
// - The engine's death, 0x006005F0 thiscall(actor, killer) (its log lines
//   "'%s' (%08X) was killed by '%s'", "has died with no attacker"), plays a
//   creature's Death sound right there: for an actor with a high process,
//   within the distance cut, whose base is a creature (type '$', 0x24), it
//   draws TESCreature::GetSound(8) and makes it at the actor (0x006AE0A0
//   with flags 0x102, placed at the actor, attached to it, started); a
//   person gets the voice of its Death topic instead (the actor's vtable
//   +0x308). The console's KillAll (0x004D5A90) calls it directly; the swap's
//   console Kill leaves the walker dead in the same line.
// - TESCreature::GetSound, 0x0051CEC0 thiscall(creature, type), ret 4: a
//   creature whose flags (+0x28) carry 0x100 has sound lists of its own at
//   +0x100; one without has a sound base there and the walk goes on - the
//   walkers (flags 0240) take CreatureZombie's. The lists are ten heads, one
//   per type (Death = 8); 0x00519900 walks a type's list and takes the first
//   sound whose chance the dice pass.
//
// So for the one console Kill the death list's head of the walker's sound
// lists is emptied and put back: no sound is drawn, none plays. The sound
// it would have drawn is drawn beforehand and played as the walker gets up.

#include "Types.h"

namespace walkers {

inline constexpr UInt32 kCreatureFlagsOffset = 0x28;
inline constexpr UInt32 kCreatureHasSounds = 0x100;
inline constexpr UInt32 kCreatureSoundsOffset = 0x100;
inline constexpr UInt32 kCreatureSoundDeath = 8;
inline constexpr UInt32 kSoundBaseStepsMost = 8;
// A SOUN form's type (xOBSE GameForms.h FormType: Sound follows Race, 9;
// OBVR GameSound.h plays its own by it).
inline constexpr UInt8 kFormTypeSound = 10;

// The sound lists `base` plays from, as GetSound walks to them; 0 when it
// has none (or the sound bases ring - the engine would loop for ever).
template <typename ReadFn>
UInt32 SoundListsOf(UInt32 base, const ReadFn& read) {
	UInt32 creature = base;
	for (UInt32 step = 0; creature != 0 && step < kSoundBaseStepsMost; ++step) {
		if ((read(creature + kCreatureFlagsOffset) & kCreatureHasSounds) != 0) {
			return read(creature + kCreatureSoundsOffset);
		}
		creature = read(creature + kCreatureSoundsOffset);
	}
	return 0;
}

// The address of the Death list's head in `lists`; 0 without lists.
inline UInt32 DeathListSlot(UInt32 lists) { return lists != 0 ? lists + kCreatureSoundDeath * 4 : 0u; }

// [Rise] SoundAtGetUpPercent: the sound held back is played as the walker
// gets up when the roll (0 to 99) falls under the percent (Nadi,
// 2026-10-26: 60); never without a sound drawn.
inline bool GetUpSoundDue(UInt32 percent, UInt32 soundId, UInt32 roll) { return soundId != 0 && roll < percent; }

}  // namespace walkers
