#include "Pose.h"

#include "Engine.h"

#include <windows.h>

#include <cmath>
#include <cstdio>
#include <cstring>

namespace walkers {
namespace {

using namespace engine;

// Read in Oblivion.exe 1.2.416 and proven by OBVR, which places and drives
// ragdoll and hand bodies through the same calls (OBVR src/game/
// GrabPhysics.h, HandBodies.cpp, RagdollHold.cpp; the push 0x0089EAE0 read
// again with REA on 2026-10-25):
// - NiAVObject::GetObjectByName, vtable +0x58, fastcall(self, edx, name).
// - The node's collision object: cdecl 0x0047FAC0(node); its body wrapper
//   (bhkRigidBody) at +0x10, the Havok body (hkRigidBody) at wrapper +0x08.
// - The wrapper's SetTranslationAndRotation, vtable +0xA0, thiscall(wrapper,
//   const hkVector4* position, const hkQuaternion* rotation), both 16-byte
//   aligned, the position in Havok units (game units times 0.142877), the
//   rotation x, y, z, w - the engine's push hands it the node's world
//   transform so, inside the Havok lock (0x00BA7B00; enter 0x0043F2E0,
//   leave 0x0043F300, thiscall on the lock).
// - The body's motion at +0x50: setLinearVelocity vtable +0x54,
//   setAngularVelocity +0x58, thiscall(motion, const hkVector4*); the body
//   woken by 0x008A6410, thiscall(hkRigidBody*).
// - A node's world transform: rotation at +0x64 (m[row][column]), position
//   at +0x88.
constexpr UInt32 kGetObjectSlot = 0x58;
constexpr UInt32 kCollisionObjectOf = 0x0047FAC0;
constexpr UInt32 kCollisionWrapperOffset = 0x10;
constexpr UInt32 kWrapperBodyOffset = 0x08;
constexpr UInt32 kSetTranslationAndRotationSlot = 0xA0;
constexpr UInt32 kBodyMotionOffset = 0x50;
constexpr UInt32 kMotionSetLinearVelocitySlot = 0x54;
constexpr UInt32 kMotionSetAngularVelocitySlot = 0x58;
constexpr UInt32 kActivateBody = 0x008A6410;
constexpr UInt32 kHavokLock = 0x00BA7B00;
constexpr UInt32 kHavokLockEnter = 0x0043F2E0;
constexpr UInt32 kHavokLockLeave = 0x0043F300;
constexpr UInt32 kNodeWorldRotationOffset = 0x64;
constexpr UInt32 kNodeWorldPositionOffset = 0x88;
constexpr UInt32 kRefNiNodeOffset = 0x3C;
constexpr float kHavokPerUnit = 0.142877f;
constexpr UInt32 kTextStart = 0x00401000;
constexpr UInt32 kTextEnd = 0x00A27C39;
// The collision object getter's first bytes, as OBVR checks them before it
// calls it: mov eax, [esp+4]; push esi; mov esi, [eax+0xA8].
constexpr UInt8 kCollisionObjectOfBytes[8] = {0x8B, 0x44, 0x24, 0x04, 0x56, 0x8B, 0xB0, 0xA8};

// The bones of the human skeleton the ragdoll's bodies hang on, and some
// more a walker's may carry; those with no body on the walker are skipped.
constexpr const char* kBones[] = {
	"Bip01 Pelvis",     "Bip01 Spine",     "Bip01 Spine1",     "Bip01 Spine2",    "Bip01 Neck",
	"Bip01 Head",       "Bip01 L Clavicle", "Bip01 R Clavicle", "Bip01 L UpperArm", "Bip01 L Forearm",
	"Bip01 L Hand",     "Bip01 R UpperArm", "Bip01 R Forearm", "Bip01 R Hand",     "Bip01 L Thigh",
	"Bip01 L Calf",     "Bip01 L Foot",     "Bip01 R Thigh",    "Bip01 R Calf",    "Bip01 R Foot",
};
constexpr UInt32 kBoneCount = sizeof(kBones) / sizeof(kBones[0]);

bool InText(UInt32 address) { return address >= kTextStart && address < kTextEnd; }

UInt32 NodeOf(UInt32 root, const char* name) {
	if (!LooksLikeObject(root) || !LooksLikeObject(Read(root))) {
		return 0;
	}
	const UInt32 fn = Read(Read(root) + kGetObjectSlot);
	if (!InText(fn)) {
		return 0;
	}
	using GetObjectFn = UInt32(__fastcall*)(void* self, void* edx, const char* name);
	const UInt32 node = reinterpret_cast<GetObjectFn>(fn)(reinterpret_cast<void*>(root), nullptr, name);
	return LooksLikeObject(node) ? node : 0;
}

// The node's body wrapper; 0 when it carries none or its setter does not read.
UInt32 WrapperOf(UInt32 node) {
	using CollisionOfFn = UInt32(__cdecl*)(UInt32 node);
	const UInt32 collision = reinterpret_cast<CollisionOfFn>(kCollisionObjectOf)(node);
	const UInt32 wrapper = LooksLikeObject(collision) ? Read(collision + kCollisionWrapperOffset) : 0;
	if (!LooksLikeObject(wrapper) || !LooksLikeObject(Read(wrapper)) ||
	    !LooksLikeObject(Read(wrapper + kWrapperBodyOffset))) {
		return 0;
	}
	return InText(Read(Read(wrapper) + kSetTranslationAndRotationSlot)) ? wrapper : 0;
}

Vec3 PositionOf(UInt32 node) {
	const float* p = reinterpret_cast<const float*>(node + kNodeWorldPositionOffset);
	return Vec3{p[0], p[1], p[2]};
}

Mat3 RotationOf(UInt32 node) {
	Mat3 r{};
	std::memcpy(r.m, reinterpret_cast<const void*>(node + kNodeWorldRotationOffset), sizeof(r.m));
	return r;
}

struct Lock {
	Lock() {
		using LockFn = void(__fastcall*)(void* lock, void* edx);
		reinterpret_cast<LockFn>(kHavokLockEnter)(reinterpret_cast<void*>(kHavokLock), nullptr);
	}
	~Lock() {
		using LockFn = void(__fastcall*)(void* lock, void* edx);
		reinterpret_cast<LockFn>(kHavokLockLeave)(reinterpret_cast<void*>(kHavokLock), nullptr);
	}
	Lock(const Lock&) = delete;
	Lock& operator=(const Lock&) = delete;
};

// The body laid at `pos` (game units) turned by `rot`, still and awake.
void Lay(UInt32 wrapper, const Vec3& pos, const Mat3& rot) {
	alignas(16) float place[4] = {pos.x * kHavokPerUnit, pos.y * kHavokPerUnit, pos.z * kHavokPerUnit, 0.0f};
	alignas(16) float turn[4];
	QuaternionOf(rot, turn);
	using PlaceFn = void(__fastcall*)(void* wrapper, void* edx, const float* pos, const float* rot);
	reinterpret_cast<PlaceFn>(Read(Read(wrapper) + kSetTranslationAndRotationSlot))(reinterpret_cast<void*>(wrapper),
	                                                                                 nullptr, place, turn);
	const UInt32 body = Read(wrapper + kWrapperBodyOffset);
	const UInt32 motion = LooksLikeObject(body) ? Read(body + kBodyMotionOffset) : 0;
	if (!LooksLikeObject(motion) || !LooksLikeObject(Read(motion))) {
		return;
	}
	alignas(16) const float still[4] = {0.0f, 0.0f, 0.0f, 0.0f};
	using SetVectorFn = void(__fastcall*)(void* motion, void* edx, const float* v);
	const UInt32 linear = Read(Read(motion) + kMotionSetLinearVelocitySlot);
	const UInt32 angular = Read(Read(motion) + kMotionSetAngularVelocitySlot);
	if (InText(linear)) {
		reinterpret_cast<SetVectorFn>(linear)(reinterpret_cast<void*>(motion), nullptr, still);
	}
	if (InText(angular)) {
		reinterpret_cast<SetVectorFn>(angular)(reinterpret_cast<void*>(motion), nullptr, still);
	}
	using ActivateFn = void(__fastcall*)(void* body, void* edx);
	reinterpret_cast<ActivateFn>(kActivateBody)(reinterpret_cast<void*>(body), nullptr);
}

// The head, or the neck when the skeleton names no head.
UInt32 HeadNodeOf(UInt32 root) {
	const UInt32 head = NodeOf(root, "Bip01 Head");
	return head != 0 ? head : NodeOf(root, "Bip01 Neck");
}

float MicrosecondsSince(const LARGE_INTEGER& start) {
	LARGE_INTEGER now;
	LARGE_INTEGER frequency;
	QueryPerformanceCounter(&now);
	QueryPerformanceFrequency(&frequency);
	return frequency.QuadPart > 0
	           ? static_cast<float>(static_cast<double>(now.QuadPart - start.QuadPart) * 1.0e6 /
	                                static_cast<double>(frequency.QuadPart))
	           : 0.0f;
}

}  // namespace

bool PelvisAndHead(UInt32 ref, Vec3* pelvis, Vec3* head) {
	const UInt32 root = LooksLikeObject(ref) ? Read(ref + kRefNiNodeOffset) : 0;
	const UInt32 pelvisNode = NodeOf(root, "Bip01 Pelvis");
	if (pelvisNode == 0) {
		return false;
	}
	const UInt32 headNode = HeadNodeOf(root);
	*pelvis = PositionOf(pelvisNode);
	*head = headNode != 0 ? PositionOf(headNode) : *pelvis;
	return true;
}

PoseReport MatchWalkerToCorpse(UInt32 walker, UInt32 corpse, MatchCorpse mode, bool firstLog, LogFn log) {
	PoseReport report;
	LARGE_INTEGER start;
	QueryPerformanceCounter(&start);
	if (mode == MatchCorpse::Off) {
		return report;
	}
	if (std::memcmp(reinterpret_cast<const void*>(kCollisionObjectOf), kCollisionObjectOfBytes,
	                sizeof(kCollisionObjectOfBytes)) != 0) {
		log("Pose: the collision object getter at %08X is not the game's own - the walker lies where it fell",
		    kCollisionObjectOf);
		return report;
	}
	const UInt32 walkerRoot = LooksLikeObject(walker) ? Read(walker + kRefNiNodeOffset) : 0;
	const UInt32 corpseRoot = LooksLikeObject(corpse) ? Read(corpse + kRefNiNodeOffset) : 0;
	if (!LooksLikeObject(walkerRoot) || !LooksLikeObject(corpseRoot)) {
		log("Pose: %s has no model - the walker lies where it fell", LooksLikeObject(walkerRoot) ? "the corpse" : "the walker");
		return report;
	}

	const UInt32 walkerPelvis = NodeOf(walkerRoot, "Bip01 Pelvis");
	const UInt32 corpsePelvis = NodeOf(corpseRoot, "Bip01 Pelvis");
	const UInt32 walkerHead = HeadNodeOf(walkerRoot);
	const UInt32 corpseHead = HeadNodeOf(corpseRoot);
	PlacePlan plan = NoPlan();
	if (walkerPelvis != 0 && corpsePelvis != 0) {
		plan = PlanPlace(PositionOf(walkerPelvis), walkerHead != 0 ? PositionOf(walkerHead) : PositionOf(walkerPelvis),
		                 PositionOf(corpsePelvis), corpseHead != 0 ? PositionOf(corpseHead) : PositionOf(corpsePelvis));
	}
	report.planOk = plan.ok;
	if (walkerPelvis != 0 && corpsePelvis != 0) {
		const Vec3 wp = PositionOf(walkerPelvis);
		const Vec3 cp = PositionOf(corpsePelvis);
		const Vec3 wh = walkerHead != 0 ? PositionOf(walkerHead) : wp;
		const Vec3 ch = corpseHead != 0 ? PositionOf(corpseHead) : cp;
		log("Pose: the corpse's pelvis at %.0f %.0f %.0f, head at %.0f %.0f %.0f; the walker's pelvis at %.0f %.0f %.0f, "
		    "head at %.0f %.0f %.0f; the turn %.0f degrees",
		    cp.x, cp.y, cp.z, ch.x, ch.y, ch.z, wp.x, wp.y, wp.z, wh.x, wh.y, wh.z,
		    std::atan2(plan.turn.m[1][0], plan.turn.m[0][0]) * 57.29578f);
	}

	struct Laying {
		UInt32 wrapper;
		Vec3 pos;
		Mat3 rot;
	};
	Laying layings[kBoneCount];
	UInt32 count = 0;
	char found[512] = {};
	UInt32 foundUsed = 0;
	for (UInt32 i = 0; i < kBoneCount; ++i) {
		const UInt32 node = NodeOf(walkerRoot, kBones[i]);
		const UInt32 wrapper = node != 0 ? WrapperOf(node) : 0;
		if (wrapper == 0) {
			continue;
		}
		++report.bodies;
		const UInt32 match = NodeOf(corpseRoot, kBones[i]);
		const BoneSource source = SourceOf(mode, plan.ok, match != 0);
		if (firstLog && foundUsed + 40 < sizeof(found)) {
			foundUsed += static_cast<UInt32>(std::snprintf(found + foundUsed, sizeof(found) - foundUsed, "%s%s%s",
			                                               foundUsed == 0 ? "" : ", ", kBones[i] + 6,
			                                               match != 0 ? "" : " (not on the corpse)"));
		}
		if (source == BoneSource::Leave) {
			++report.left;
			continue;
		}
		Laying& laying = layings[count++];
		laying.wrapper = wrapper;
		if (source == BoneSource::Copied) {
			laying.pos = PositionOf(match);
			laying.rot = RotationOf(match);
			++report.copied;
		} else {
			Placed(plan, PositionOf(node), RotationOf(node), &laying.pos, &laying.rot);
			++report.placed;
		}
	}
	if (count > 0) {
		Lock lock;
		for (UInt32 i = 0; i < count; ++i) {
			Lay(layings[i].wrapper, layings[i].pos, layings[i].rot);
		}
	}
	report.microseconds = MicrosecondsSince(start);
	if (firstLog) {
		log("Pose: the walker's ragdoll bodies on %s", foundUsed > 0 ? found : "none of the bones looked at");
	}
	return report;
}

}  // namespace walkers
