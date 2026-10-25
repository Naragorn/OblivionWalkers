#pragma once

// The walker laid into the corpse's place in the game (docs/walkers-spec.md
// 4.1, the swap's look). The decisions are PoseLogic.h's; this file reads
// the two skeletons and moves the walker's ragdoll bodies.

#include "PoseLogic.h"
#include "Rise.h"

namespace walkers {

struct PoseReport {
	UInt32 bodies = 0;  // the walker's ragdoll bodies found on the bones looked at
	UInt32 copied = 0;   // turned as the corpse's bone of the same name, at the walker's own bone length
	UInt32 carried = 0;  // a bone the corpse lacks, carried along by its parent
	UInt32 placed = 0;  // moved and turned with the pelvis
	UInt32 left = 0;    // left where they fell
	bool planOk = false;
	float microseconds = 0.0f;  // the whole of it, for the log
	// Where the corpse's lowest spine bone and head lay, when read - so a
	// later placement can go there once the corpse is gone (HideCorpseAtPose).
	bool corpseRead = false;
	Vec3 corpsePivot{0.0f, 0.0f, 0.0f};
	Vec3 corpseHead{0.0f, 0.0f, 0.0f};
};

// The pelvis's and the head's world positions of `ref` (game units); false
// when it has no model or no pelvis. For the log: where a body lies.
bool PelvisAndHead(UInt32 ref, Vec3* pelvis, Vec3* head);

// The walker's ragdoll bodies laid where the corpse lies, as `mode` says;
// both references must have their 3D. Inside the engine's Havok lock, each
// body through its wrapper's SetTranslationAndRotation, its velocities set
// to nothing and the body woken. `firstLog` lists the bones found once.
// With `pivotTarget` and `headTarget` (Place only) the walker is placed
// there and the corpse is not read - it may be gone.
PoseReport MatchWalkerToCorpse(UInt32 walker, UInt32 corpse, MatchCorpse mode, bool firstLog, LogFn log,
                               const Vec3* pivotTarget = nullptr, const Vec3* headTarget = nullptr);

}  // namespace walkers
