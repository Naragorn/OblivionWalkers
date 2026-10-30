#pragma once

// Grabbed by walkers in the game (docs/walkers-spec.md 4.19): GrabLogic.h's
// decisions carried out - the grab at a walker's landed hit (xOBSE's OnHit),
// the holder kept standing and the player rooted on the
// HighProcess movement flags every frame, the grab's animations played
// through the engine's idle queue as xOBSE's PlayIdle does it, the bites as
// damage, the holders sent to OBVR and let go at its request.

#include "GrabLogic.h"
#include "Obse.h"
#include "Rise.h"

namespace walkers {

// What the rise lends the grab: a console line on a reference, whether an
// actor is dead, whether a reference is a walker (and of MOO's skeleton), an
// actor's high process (0 for none), a draw 0..99.
struct GrabServices {
	bool (*console)(UInt32 ref, const char* line);
	bool (*dead)(UInt32 actor);
	bool (*isWalker)(UInt32 ref);
	bool (*onMooSkeleton)(UInt32 ref);
	UInt32 (*highProcess)(UInt32 actor);
	UInt32 (*roll)();
	UInt32 (*nearestWalker)(float radius);  // the test switch's walker, 0 for none
	UInt32 (*walkersNear)(float radius);    // living walkers within `radius` of the player
	// OBVR's other messages (4.17's 'WGDM' and 'WGHD'), handed on as they come.
	void (*otherObvrMessage)(UInt32 type, const void* data, UInt32 length);
	LogFn log;
};

// Once a session, at the plugin's load: the live settings (LINK writes into
// them), the services, xOBSE's messaging for OBVR (nullptr: none).
void ConfigureGrab(const GrabSettings* settings, const GrabServices& services, obse::MessagingInterface* messaging,
                   UInt32 pluginHandle);

// The ESP's base (RiseLogic.h EspFormId), once it is known (its grab
// idles, 0x94D..0x952).
void ConfigureGrabIdles(UInt32 espBase);

// xOBSE's OnHit, as the rise hands it on: `target` hit by `attacker`.
void GrabOnHit(UInt32 target, UInt32 attacker);

// Every frame.
void StepGrabInGame(float dt);

// A load or a new game: nobody holds (the flags are written each frame
// only, so nothing of a hold is in the save).
void ResetGrabInGame();

}  // namespace walkers
