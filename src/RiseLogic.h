#pragma once

// The rise's decisions, process-free: who waits after a death, how the
// waiting clocks run while their cells are loaded, and the co-save image of
// the list (docs/walkers-spec.md, 4.1). Rise.cpp reads the engine and calls
// in here; the tests run every flow without a game.

#include <cstring>

#include "Types.h"

namespace walkers {

// [Rise] in OblivionWalkers.ini.
struct RiseSettings {
	bool enabled = true;
	float afterSeconds = 20.0f;
	float jitterSeconds = 10.0f;
	UInt32 chancePercent = 100;
	bool onlyGeneric = true;
	// Only people of a playable race (men, mer and the beast folk): a Dremora
	// or a Golden Saint is a person to the engine too, and no zombie.
	bool onlyPlayableRaces = true;
	// Narrower still: only the four human races (Imperial, Breton, Nord,
	// Redguard). Nadi, 2026-10-23: an extra option, off by default.
	bool onlyHumans = false;
	// The risen one gets a clone of his base record before he gets up (the
	// record that will carry the zombie race, 4.1 step 3) - a proof switch
	// until the race exists, so default off.
	bool cloneBase = false;
	// With the clone: its race becomes the ESP's Walker race
	// (OblivionWalkers.esp, tools/esp-write.py), when the ESP is loaded.
	bool zombieRace = true;
	// Seconds after the raise before the risen one's 3D is built anew from
	// the clone (xOBSE's Update3D) so the race shows - after the get-up,
	// which Resurrect plays on the old model. 0: at once.
	float lookAfterSeconds = 3.0f;
	// The fallen rises as the game's own zombie (Nadi, 2026-10-23 evening):
	// a Walker creature of the ESP placed on the corpse, the corpse hidden,
	// the zombie dropped, the items moved over, the zombie raised at the
	// spot. 0: the person himself gets up (the clone and race paths above).
	bool asCreature = true;
	// Seconds between dropping the placed zombie and raising it: its ragdoll
	// settles where the man lay, and the get-up starts from there.
	float dropSeconds = 1.5f;
	// [Zombies] Walk: the walkers never run (4.4) - a run their AI asks for
	// becomes a walk, every frame, on the HighProcess movement flags.
	bool walkOnly = true;
	// [Zombies] HealthPercent: 1 to 100 percent of the walker records'
	// health, written on the records in memory at the start (4.5) - never
	// per reference, so no save carries a scaled value.
	UInt32 healthPercent = 15;
	// [Zombies] NoLoot: a dead walker cannot be activated - no container
	// opens (Nadi, 2026-10-24; the engine's SetDestroyed on the corpse, which
	// the CS wiki gives as "prevents the actor from being activated for
	// conversation, looting, or pick pocketing"). Never a risen one
	// (BlocksLoot).
	bool noLoot = true;
	// [Zombies] SpeedMultiplier and SpeedRandomPercent: the walkers' pace,
	// spread over the four walker records (ReplaceLogic.h WalkerSpeed).
	float speedMultiplier = 1.0f;
	UInt32 speedRandomPercent = 30;
	// [Zombies] FamePerKills: a point of fame for every this many walkers
	// the player cuts down (28 Days gave one per 50). 0, the default (Nadi,
	// 2026-10-24 evening): no fame.
	UInt32 famePerKills = 0;
	// [Zombies] AvoidChapels (4.7): nobody rises in a chapel - 28 Days' safe
	// zones (IsChapelCell).
	bool avoidChapels = true;
};

// HighProcess movement flags (+0x1FC; xOBSE GameProcess.h kMovement_Walk
// 0x100, kMovement_Run 0x200; the actor's speed follows them - OBVR's slow
// approach reads the same, 2026-10-09): a run turned into a walk, anything
// else kept.
inline constexpr UInt16 kMovementWalk = 0x0100;
inline constexpr UInt16 kMovementRun = 0x0200;

inline UInt16 WalkInsteadOfRun(UInt16 flags) {
	return (flags & kMovementRun) != 0 ? static_cast<UInt16>((flags & ~kMovementRun) | kMovementWalk) : flags;
}

// The creature path's one swap in flight, stepped by the frame tick:
// placed (the walker asked for, its reference not yet seen), loading (the
// reference is there, its model still on the way - Nadi's game of
// 2026-10-23 20:57 took more than two seconds for a zombie's first load),
// dropped (the model is there and the walker killed in that very frame,
// before its AI takes a step, the corpse hidden, the ragdoll settling),
// done. A kill before the model exists leaves a "dead" walker sliding at
// walking speed (the trace of 2026-10-24 07:10: 400 units/s in a line), so
// the kill waits for the model.
enum class SwapStage : UInt8 { Placed, Loading, Dropped, Done };

struct Swap {
	bool active = false;
	SwapStage stage = SwapStage::Placed;
	UInt32 corpseId = 0;
	UInt32 zombieId = 0;
	UInt32 walkerBase = 0;
	float x = 0.0f;  // where the man lay
	float y = 0.0f;
	float z = 0.0f;
	float seconds = 0.0f;  // in the stage
	// The walkers of this base that existed before the PlaceAtMe: the new one
	// is the first not among them. Identity, not distance - the engine puts a
	// placed creature where it likes in an exterior (Nadi's game, 2026-10-23
	// 21:23: a few metres off, outside any radius).
	UInt32 known[16] = {};
	UInt32 knownCount = 0;
};

inline constexpr UInt32 kSwapKnownMost = 16;

inline void NoteKnownWalkers(Swap& swap, const UInt32* refs, UInt32 count) {
	swap.knownCount = 0;
	for (UInt32 i = 0; i < count && swap.knownCount < kSwapKnownMost; ++i) {
		swap.known[swap.knownCount++] = refs[i];
	}
}

inline bool IsKnownWalker(const Swap& swap, UInt32 ref) {
	for (UInt32 i = 0; i < swap.knownCount; ++i) {
		if (swap.known[i] == ref) {
			return true;
		}
	}
	return false;
}

// The first of `refs` not known before the placement; 0 when all are.
inline UInt32 NewWalkerAmong(const Swap& swap, const UInt32* refs, UInt32 count) {
	for (UInt32 i = 0; i < count; ++i) {
		if (refs[i] != 0 && !IsKnownWalker(swap, refs[i])) {
			return refs[i];
		}
	}
	return 0;
}

enum : UInt32 {
	kSwapWait = 0,      // nothing this frame
	kSwapFound = 1,        // the walker's reference is there, its model on the way
	kSwapDropAndHide = 2,  // its model is there: kill it this frame, hide the corpse
	kSwapGaveUp = 4,    // no reference appeared in time: the man gets up as himself
	kSwapRaise = 8,     // the ragdoll settled: move the items over, raise the walker
	kSwapAbandon = 16,  // the model never came: hide the walker, the man gets up as himself
};
inline constexpr float kSwapFindSeconds = 2.0f;
inline constexpr float kSwapLoadSeconds = 15.0f;

// One frame of `dt` seconds; `zombieFound` is the new walker's reference ID
// near the corpse (0 while none exists), `zombieHas3D` whether the walker's
// model is in the world. The swap ends with kSwapRaise, kSwapGaveUp or
// kSwapAbandon.
inline UInt32 StepSwap(Swap& swap, float dt, UInt32 zombieFound, bool zombieHas3D, float dropSeconds) {
	if (!swap.active) {
		return kSwapWait;
	}
	swap.seconds += dt;
	if (swap.stage == SwapStage::Placed) {
		if (zombieFound != 0) {
			swap.zombieId = zombieFound;
			swap.stage = SwapStage::Loading;
			swap.seconds = 0.0f;
			return kSwapFound;
		}
		if (swap.seconds >= kSwapFindSeconds) {
			swap.active = false;
			swap.stage = SwapStage::Done;
			return kSwapGaveUp;
		}
		return kSwapWait;
	}
	if (swap.stage == SwapStage::Loading) {
		if (zombieHas3D) {
			swap.stage = SwapStage::Dropped;
			swap.seconds = 0.0f;
			return kSwapDropAndHide;
		}
		if (swap.seconds >= kSwapLoadSeconds) {
			swap.active = false;
			swap.stage = SwapStage::Done;
			return kSwapAbandon;
		}
		return kSwapWait;
	}
	if (swap.stage == SwapStage::Dropped && swap.seconds >= dropSeconds) {
		swap.active = false;
		swap.stage = SwapStage::Done;
		return kSwapRaise;
	}
	return kSwapWait;
}

// One of the ESP's walkers by the caller's random number; 0 without any.
inline UInt32 PickWalker(const UInt32* bases, UInt32 count, UInt32 random) {
	return count == 0 ? 0u : bases[random % count];
}

// The ESP's records sit at local IDs from 0x800 (tools/esp-write.py): the
// race first, then the four Walker creatures. The mod index is the ESP's
// load index at runtime.
inline constexpr UInt32 kZombieRaceLocalId = 0x800;
inline constexpr UInt32 kWalkerLocalIds[4] = {0x801, 0x802, 0x803, 0x804};
inline UInt32 ZombieRaceFormId(UInt32 modIndex) { return (modIndex << 24) | kZombieRaceLocalId; }
inline UInt32 EspFormId(UInt32 modIndex, UInt32 localId) { return (modIndex << 24) | localId; }

// Mod file names compare without case, as the engine's own list does; a
// name longer than a Windows path is no name.
inline bool SameModName(const char* a, const char* b) {
	for (UInt32 i = 0; i < 260; ++i) {
		char x = a[i];
		char y = b[i];
		if (x >= 'A' && x <= 'Z') {
			x = static_cast<char>(x - 'A' + 'a');
		}
		if (y >= 'A' && y <= 'Z') {
			y = static_cast<char>(y - 'A' + 'a');
		}
		if (x != y) {
			return false;
		}
		if (x == '\0') {
			return true;
		}
	}
	return false;
}

// A fallen actor as the death callback read him off the engine.
struct Fallen {
	UInt32 refId = 0;
	UInt32 baseId = 0;
	bool character = false;     // a Character reference: a person in the world, not a creature
	bool essential = false;     // the base's essential flag: he only fell unconscious
	bool generic = false;       // the base's no-low-level-processing flag, which the leveled
	                            // lists' people carry (719 of Oblivion.esm's 2482, the bandits
	                            // among them) and most named people do not
	bool racePlayable = false;  // his race's playable byte (TESRace+0x70)
	bool raceHuman = false;     // one of Oblivion.esm's four human races (IsHumanRace)
	bool inChapel = false;      // he fell in a chapel (IsChapelCell of his cell)
};

// A chapel (4.7): a cell the Nine Divines own (Oblivion.esm's NineDivines
// faction, 000223BC - the city chapels, their halls and undercrofts), or one
// whose editor ID says "chapel" (Kvatch's, owned by nobody; Weynon Priory's,
// the Blades'; a mod's). Read off Oblivion.esm on 2026-10-24: 29 cells say
// chapel, 24 of them the Nine's or Kvatch's or Weynon's; the others are
// Cadlew Chapel (a crypt), the Brethren's chapel at Hackdirt, Newt Cave
// Chapels of Light and a dummy - no rise there either, except Cadlew
// Chapel: a crypt full of the dead, no chapel (Nadi, 2026-10-24 evening:
// "Cadlew Chapel ist eine Ausnahme"). A cell keeps its editor ID in the game
// (ExtraEditorID; xOBSE TESForm::GetEditorID).
inline constexpr UInt32 kNineDivinesFaction = 0x000223BC;

// `word` (lower case) somewhere in `text`, the case of `text` aside.
inline bool HasWord(const char* text, const char* word) {
	for (const char* p = text; *p != '\0'; ++p) {
		UInt32 i = 0;
		for (; word[i] != '\0'; ++i) {
			const char c = p[i];
			if ((c >= 'A' && c <= 'Z' ? c + 32 : c) != word[i]) {
				break;
			}
		}
		if (word[i] == '\0') {
			return true;
		}
	}
	return false;
}

inline bool IsChapelCell(const char* editorId, UInt32 ownerFormId) {
	if (ownerFormId == kNineDivinesFaction) {
		return true;
	}
	return editorId != nullptr && HasWord(editorId, "chapel") && !HasWord(editorId, "cadlew");
}

// None of ours spawns in a chapel (Nadi, 2026-10-24 evening: "nur keine von
// unser mod spawnen oder aufstehen lassen"): a live walker the frame tick
// first meets in a chapel - a spawn of a mod's list there, one a script
// placed - was made there and is taken out of the world, once. One met
// outside before walked in (after the player, through the door) and stays.
// Met is per session: one that walked in, saved and loaded in the chapel,
// counts as made there.
inline bool SpawnedInChapel(bool avoidChapels, bool inChapel, bool seenOutside, bool banishedAlready) {
	return avoidChapels && inChapel && !seenOutside && !banishedAlready;
}

// Oblivion.esm's human races: Imperial, Breton, Nord, Redguard (read off the
// RACE records on 2026-10-23). A mod's own human race does not count.
inline bool IsHumanRace(UInt32 raceId) {
	return raceId == 0x00000907 || raceId == 0x000224FC || raceId == 0x000224FD || raceId == 0x00000D43;
}

enum class DeathVerdict : UInt8 {
	Waits,            // on the list, the clock set
	Disabled,         // [Rise] Enabled=0
	NotAPerson,       // a creature, or no actor at all
	Essential,        // unconscious, not dead
	NotGeneric,       // OnlyGeneric=1 and the base is a named or quest person
	NotPlayableRace,  // OnlyPlayableRaces=1 and his race is not one a player can be
	NotHuman,         // OnlyHumans=1 and his race is not one of the four human ones
	AlreadyWaiting,   // the same reference died again before rising
	ChanceMissed,     // the ChancePercent roll
	InChapel,         // AvoidChapels=1 and he fell in a chapel
	ListFull,         // kWaitingMost references wait already
};

inline const char* VerdictText(DeathVerdict verdict) {
	switch (verdict) {
		case DeathVerdict::Waits:
			return "waits to rise";
		case DeathVerdict::Disabled:
			return "stays down (Rise Enabled=0)";
		case DeathVerdict::NotAPerson:
			return "stays down (not a person)";
		case DeathVerdict::Essential:
			return "stays down (essential, only unconscious)";
		case DeathVerdict::NotGeneric:
			return "stays down (a named person, OnlyGeneric=1)";
		case DeathVerdict::NotPlayableRace:
			return "stays down (not of a playable race, OnlyPlayableRaces=1)";
		case DeathVerdict::NotHuman:
			return "stays down (not human, OnlyHumans=1)";
		case DeathVerdict::AlreadyWaiting:
			return "waits already";
		case DeathVerdict::ChanceMissed:
			return "stays down (the chance roll)";
		case DeathVerdict::InChapel:
			return "stays down (in a chapel, AvoidChapels=1)";
		case DeathVerdict::ListFull:
			return "stays down (the waiting list is full)";
	}
	return "?";
}

struct Waiting {
	UInt32 refId;
	UInt32 baseId;
	float secondsLeft;
};

inline constexpr UInt32 kWaitingMost = 128;

struct WaitingList {
	Waiting entries[kWaitingMost] = {};
	UInt32 count = 0;
};

inline int FindWaiting(const WaitingList& list, UInt32 refId) {
	for (UInt32 i = 0; i < list.count; ++i) {
		if (list.entries[i].refId == refId) {
			return static_cast<int>(i);
		}
	}
	return -1;
}

// A death. `chanceRoll` is the caller's random number in 0..99, `jitterUnit`
// one in 0..1: the clock is AfterSeconds plus that share of JitterSeconds.
// A due rise put back for `seconds` (a swap already in flight takes one
// corpse at a time - the outdoor run of 2026-10-24 08:09 had a second man
// get up as himself instead). False when the list is full or he waits already.
inline bool Requeue(WaitingList& list, UInt32 refId, UInt32 baseId, float seconds) {
	if (list.count >= kWaitingMost || FindWaiting(list, refId) >= 0) {
		return false;
	}
	list.entries[list.count++] = Waiting{refId, baseId, seconds};
	return true;
}

inline DeathVerdict NoteDeath(WaitingList& list, const RiseSettings& settings, const Fallen& fallen,
                              UInt32 chanceRoll, float jitterUnit) {
	if (!settings.enabled) {
		return DeathVerdict::Disabled;
	}
	if (!fallen.character) {
		return DeathVerdict::NotAPerson;
	}
	if (fallen.essential) {
		return DeathVerdict::Essential;
	}
	if (settings.onlyGeneric && !fallen.generic) {
		return DeathVerdict::NotGeneric;
	}
	if (settings.onlyPlayableRaces && !fallen.racePlayable) {
		return DeathVerdict::NotPlayableRace;
	}
	if (settings.onlyHumans && !fallen.raceHuman) {
		return DeathVerdict::NotHuman;
	}
	if (settings.avoidChapels && fallen.inChapel) {
		return DeathVerdict::InChapel;
	}
	if (FindWaiting(list, fallen.refId) >= 0) {
		return DeathVerdict::AlreadyWaiting;
	}
	if (chanceRoll >= settings.chancePercent) {
		return DeathVerdict::ChanceMissed;
	}
	if (list.count >= kWaitingMost) {
		return DeathVerdict::ListFull;
	}
	const float unit = jitterUnit < 0.0f ? 0.0f : (jitterUnit > 1.0f ? 1.0f : jitterUnit);
	list.entries[list.count++] =
		Waiting{fallen.refId, fallen.baseId, settings.afterSeconds + settings.jitterSeconds * unit};
	return DeathVerdict::Waits;
}

// What the engine says of a waiting reference each frame.
enum class Presence : UInt8 {
	Gone,     // no such reference any more: leaves the list, nobody rises
	Absent,   // its cell is not loaded (no 3D): the clock stands
	Present,  // in the world: the clock runs
};

// One frame of `dt` seconds. The references whose clock ran out go to `due`
// (at most dueMost; any beyond that keep a clock of zero for the next frame)
// and leave the list; returns how many are due. The list keeps its order.
template <typename PresentFn>
UInt32 TickWaiting(WaitingList& list, float dt, PresentFn present, UInt32* due, UInt32 dueMost) {
	UInt32 dueCount = 0;
	UInt32 kept = 0;
	for (UInt32 i = 0; i < list.count; ++i) {
		Waiting entry = list.entries[i];
		const Presence where = present(entry.refId);
		if (where == Presence::Gone) {
			continue;
		}
		if (where == Presence::Present) {
			entry.secondsLeft -= dt;
			if (entry.secondsLeft <= 0.0f) {
				if (dueCount < dueMost) {
					due[dueCount++] = entry.refId;
					continue;
				}
				entry.secondsLeft = 0.0f;
			}
		}
		list.entries[kept++] = entry;
	}
	list.count = kept;
	return dueCount;
}

// After the engine's Resurrect: when the engine counts him alive again and
// when he first moves off the spot where he lay - the get-up animation's
// length, read off the game (harness/rise-getup.txt). One watch at a time.
struct RisenWatch {
	UInt32 refId = 0;
	UInt32 startMs = 0;
	float x = 0.0f;
	float y = 0.0f;
	float z = 0.0f;
	bool aliveSeen = false;
	bool moveSeen = false;
	bool lookWanted = false;  // his base changed under him: build his 3D anew
	bool lookDone = false;
};

enum : UInt32 {
	kRisenAlive = 1,  // counted alive now
	kRisenMoved = 2,  // first off the spot now
	kRisenDone = 4,   // everything seen and done, or the watch's time is up
	kRisenLook = 8,   // now: build his 3D anew (LookAfterSeconds are over)
};
inline constexpr float kRisenMoveUnits = 8.0f;
inline constexpr UInt32 kRisenWatchMs = 15000;

inline UInt32 StepRisenWatch(RisenWatch& watch, bool dead, float x, float y, float z, UInt32 nowMs,
                             UInt32 lookAfterMs) {
	UInt32 events = 0;
	if (!watch.aliveSeen && !dead) {
		watch.aliveSeen = true;
		events |= kRisenAlive;
	}
	if (!watch.moveSeen) {
		const float dx = x - watch.x;
		const float dy = y - watch.y;
		const float dz = z - watch.z;
		if (dx * dx + dy * dy + dz * dz > kRisenMoveUnits * kRisenMoveUnits) {
			watch.moveSeen = true;
			events |= kRisenMoved;
		}
	}
	if (watch.lookWanted && !watch.lookDone && nowMs - watch.startMs >= lookAfterMs) {
		watch.lookDone = true;
		events |= kRisenLook;
	}
	const bool lookSettled = !watch.lookWanted || watch.lookDone;
	if ((watch.aliveSeen && watch.moveSeen && lookSettled) || nowMs - watch.startMs >= kRisenWatchMs) {
		events |= kRisenDone;
	}
	return events;
}

// The co-save record: type "WAIT", version 1 - the count, then reference,
// base and seconds left per entry, little-endian like the engine's own.
inline constexpr UInt32 kWaitingRecordType = 0x57414954;
inline constexpr UInt32 kWaitingRecordVersion = 1;
inline constexpr UInt32 kWaitingEntryBytes = 12;
inline constexpr UInt32 kWaitingImageMost = 4 + kWaitingMost * kWaitingEntryBytes;

inline void Put32(UInt8* out, UInt32 value) {
	out[0] = static_cast<UInt8>(value & 0xFF);
	out[1] = static_cast<UInt8>((value >> 8) & 0xFF);
	out[2] = static_cast<UInt8>((value >> 16) & 0xFF);
	out[3] = static_cast<UInt8>((value >> 24) & 0xFF);
}

inline UInt32 Get32(const UInt8* in) {
	return static_cast<UInt32>(in[0]) | (static_cast<UInt32>(in[1]) << 8) | (static_cast<UInt32>(in[2]) << 16) |
	       (static_cast<UInt32>(in[3]) << 24);
}

// Returns the bytes written, 0 when the image does not fit.
inline UInt32 WaitingImage(const WaitingList& list, UInt8* out, UInt32 capacity) {
	const UInt32 needed = 4 + list.count * kWaitingEntryBytes;
	if (capacity < needed) {
		return 0;
	}
	Put32(out, list.count);
	UInt32 at = 4;
	for (UInt32 i = 0; i < list.count; ++i) {
		const Waiting& entry = list.entries[i];
		UInt32 seconds = 0;
		std::memcpy(&seconds, &entry.secondsLeft, sizeof(seconds));
		Put32(out + at, entry.refId);
		Put32(out + at + 4, entry.baseId);
		Put32(out + at + 8, seconds);
		at += kWaitingEntryBytes;
	}
	return at;
}

// Reads an image into `list` (emptied first). `resolve(id, out)` maps a saved
// reference ID to the loaded game's (xOBSE's ResolveRefID); an entry whose
// reference cannot be resolved is dropped. False when the image is cut short
// or claims more entries than the list holds; what was read before stays.
template <typename ResolveFn>
bool ReadWaitingImage(WaitingList& list, const UInt8* in, UInt32 length, ResolveFn resolve) {
	list.count = 0;
	if (length < 4) {
		return false;
	}
	const UInt32 count = Get32(in);
	if (count > kWaitingMost) {
		return false;
	}
	const bool whole = length >= 4 + count * kWaitingEntryBytes;
	const UInt32 readable = whole ? count : (length - 4) / kWaitingEntryBytes;
	UInt32 at = 4;
	for (UInt32 i = 0; i < readable; ++i, at += kWaitingEntryBytes) {
		UInt32 refId = 0;
		if (!resolve(Get32(in + at), refId)) {
			continue;
		}
		const UInt32 bits = Get32(in + at + 8);
		float seconds = 0.0f;
		std::memcpy(&seconds, &bits, sizeof(seconds));
		list.entries[list.count++] = Waiting{refId, Get32(in + at + 4), seconds};
	}
	return whole;
}

// The walkers risen from a corpse. Nadi, 2026-10-24: they stay lootable
// whatever NoLoot says - the fallen's gear and quest items went over to them.
// Noted when the swap finds its placed walker, before the kill that drops it.
inline constexpr UInt32 kRisenMost = 256;

struct RisenSet {
	UInt32 ids[kRisenMost] = {};
	UInt32 count = 0;
};

inline bool IsRisen(const RisenSet& set, UInt32 refId) {
	for (UInt32 i = 0; i < set.count; ++i) {
		if (set.ids[i] == refId) {
			return true;
		}
	}
	return false;
}

// Full, the oldest goes: the likeliest to be gone from the world.
inline void NoteRisen(RisenSet& set, UInt32 refId) {
	if (refId == 0 || IsRisen(set, refId)) {
		return;
	}
	if (set.count == kRisenMost) {
		std::memmove(set.ids, set.ids + 1, (kRisenMost - 1) * sizeof(set.ids[0]));
		--set.count;
	}
	set.ids[set.count++] = refId;
}

// NoLoot takes a dead walker, never a risen one.
inline bool BlocksLoot(bool noLoot, bool isWalker, bool risen) { return noLoot && isWalker && !risen; }

// Drops those `exists(id)` no longer finds, the order of the rest kept;
// returns how many went.
template <typename ExistsFn>
UInt32 KeepRisen(RisenSet& set, ExistsFn exists) {
	UInt32 kept = 0;
	for (UInt32 i = 0; i < set.count; ++i) {
		if (exists(set.ids[i])) {
			set.ids[kept++] = set.ids[i];
		}
	}
	const UInt32 gone = set.count - kept;
	set.count = kept;
	return gone;
}

// The co-save record of the risen: the count, then each reference ID.
inline constexpr UInt32 kRisenRecordType = 0x5249534E;  // 'RISN'
inline constexpr UInt32 kRisenRecordVersion = 1;
inline constexpr UInt32 kRisenImageMost = 4 + kRisenMost * 4;

// Returns the bytes written, 0 when the image does not fit.
inline UInt32 RisenImage(const RisenSet& set, UInt8* out, UInt32 capacity) {
	const UInt32 needed = 4 + set.count * 4;
	if (capacity < needed) {
		return 0;
	}
	Put32(out, set.count);
	for (UInt32 i = 0; i < set.count; ++i) {
		Put32(out + 4 + i * 4, set.ids[i]);
	}
	return needed;
}

// As ReadWaitingImage: `set` emptied first, an unresolved reference dropped,
// false when cut short or claiming more than the set holds.
template <typename ResolveFn>
bool ReadRisenImage(RisenSet& set, const UInt8* in, UInt32 length, ResolveFn resolve) {
	set.count = 0;
	if (length < 4) {
		return false;
	}
	const UInt32 count = Get32(in);
	if (count > kRisenMost) {
		return false;
	}
	const bool whole = length >= 4 + count * 4;
	const UInt32 readable = whole ? count : (length - 4) / 4;
	for (UInt32 i = 0; i < readable; ++i) {
		UInt32 refId = 0;
		if (resolve(Get32(in + 4 + i * 4), refId)) {
			set.ids[set.count++] = refId;
		}
	}
	return whole;
}

// Fame for walkers cut down (4.8): a walker's death counts when the player
// is its killer (xOBSE's OnDeath hands the killer over; the console's Kill
// without one hands none).
inline bool FameKill(bool walker, bool killedByPlayer) { return walker && killedByPlayer; }

struct FameCount {
	UInt32 kills = 0;  // toward the next point
};

// One walker more: true when it completes `perKills` - a point of fame, the
// count starts over. A count already at or past the threshold (FamePerKills
// lowered between two sessions) earns one point, not the lot. 0: off.
inline bool CountWalkerKill(FameCount& count, UInt32 perKills) {
	if (perKills == 0) {
		return false;
	}
	++count.kills;
	if (count.kills < perKills) {
		return false;
	}
	count.kills = 0;
	return true;
}

// The count in the co-save, record 'FAME' version 1: four bytes.
inline constexpr UInt32 kFameRecordType = 0x46414D45;  // 'FAME'
inline constexpr UInt32 kFameRecordVersion = 1;
inline constexpr UInt32 kFameImageBytes = 4;

inline UInt32 FameImage(const FameCount& count, UInt8* out, UInt32 capacity) {
	if (capacity < kFameImageBytes) {
		return 0;
	}
	Put32(out, count.kills);
	return kFameImageBytes;
}

// False when cut short; the count then starts at 0.
inline bool ReadFameImage(FameCount& count, const UInt8* in, UInt32 length) {
	count.kills = length >= kFameImageBytes ? Get32(in) : 0;
	return length >= kFameImageBytes;
}

}  // namespace walkers
