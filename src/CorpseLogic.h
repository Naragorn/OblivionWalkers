#pragma once

// Ended walkers' bodies taken out of the world (Nadi, 2026-11-14: "viele
// walker leichen ... liegen ewig, kostet performance", "go A + B"). Nothing
// took them away before: a walker made by the corpse swap is a PlaceAtMe
// reference, which the engine never removes (CS wiki PlaceAtMe; DeleteReference
// refuses actors); a spawned one goes only when its cell resets. A body
// ended for good is disabled once the player is CorpseUnits away - out of
// his sight, so none vanishes before his eyes; one that carries the fallen's
// things (a risen walker) only at CorpseLootUnits. Only references made in
// the game (FF...) - the ones nothing else removes: a placed spawn goes at
// its cell's reset (a disabled placed reference might stay disabled through
// it - not read, so not touched), and the pools' walkers (sieges, camps,
// held dungeons - the ESP's references) are put back by their own
// bookkeeping. Walkers that will get up again are left alone.

#include "Types.h"

namespace walkers {

// [Corpses] in OblivionWalkers.ini.
struct CorpseSettings {
	bool enabled = true;            // Enabled
	float units = 4000.0f;          // CorpseUnits
	float lootUnits = 8000.0f;      // CorpseLootUnits
	float countSeconds = 60.0f;     // LogSeconds: how often the count is logged (A)
};

struct CorpseFacts {
	bool dead = false;
	bool disabled = false;
	bool waiting = false;     // on the list to get up again
	bool made = false;        // made in the game (an FF... reference)
	bool carriesLoot = false; // risen from someone: his things on it
	float distance = -1.0f;   // from the player, -1 not known (another worldspace, no 3D)
};

// A reference made in the game (PlaceAtMe): its ID's load index is FF.
inline bool MadeInGame(UInt32 refId) { return (refId >> 24) == 0xFF; }

// Whether the body goes now.
inline bool CorpseGoes(const CorpseSettings& s, const CorpseFacts& f) {
	if (!s.enabled || !f.made || !f.dead || f.disabled || f.waiting || f.distance < 0.0f) {
		return false;
	}
	return f.distance >= (f.carriesLoot ? s.lootUnits : s.units);
}

// Whether the body counts among those lying (the log's count): ended, in
// the world.
inline bool CorpseLies(const CorpseFacts& f) { return f.dead && !f.disabled && !f.waiting; }

}  // namespace walkers
