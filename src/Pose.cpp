#include "Pose.h"

#include "ApartLogic.h"
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
// The body's collision filter word (hkRigidBody +0x30; OBVR GrabPhysics.h):
// the layer in the low 6 bits, the part number in bits 8 to 12, 0x4000 no
// collision, 0x8000 linked, the system group in the high 16 bits - the
// engine's rule 0x008A7F70 decides each pair by it (read with REA
// 2026-10-25).
constexpr UInt32 kBodyFilterOffset = 0x30;
// The rule's first bytes (mov edx, [esp+4]; push esi; mov esi, [esp+0xC];
// push edi; mov edi, edx; and edi, 0x3F): RuleCollides mirrors this code,
// so a rule another mod has patched refuses the move.
constexpr UInt32 kFilterRule = 0x008A7F70;
constexpr UInt8 kFilterRuleBytes[15] = {0x8B, 0x54, 0x24, 0x04, 0x56, 0x8B, 0x74, 0x24,
                                        0x0C, 0x57, 0x8B, 0xFA, 0x83, 0xE7, 0x3F};
// The tables the rule reads, the layer matrix then the bone table
// (ApartLogic.h).
constexpr UInt32 kFilterTables = 0x00BA7DB0;
// The wrapper's filter refresh, vtable +0x80: 0x008B0060 hands the body to
// hkWorld::updateCollisionFilterOnEntity (0x0089B630) when it is in a world
// - as the engine's own setters 0x0089F4D0 and 0x0089F520 do after writing
// the word, and OBVR's hand bodies (HandBodies.cpp, Refilter). Three
// vtables carry it (0x00A55DFC, 0x00A5605C, 0x00A980A4).
constexpr UInt32 kRefreshFilterSlot = 0x80;
constexpr UInt32 kRefreshFilter = 0x008B0060;
constexpr UInt32 kHavokLock = 0x00BA7B00;
constexpr UInt32 kHavokLockEnter = 0x0043F2E0;
constexpr UInt32 kHavokLockLeave = 0x0043F300;
constexpr UInt32 kNodeWorldRotationOffset = 0x64;
constexpr UInt32 kNodeWorldPositionOffset = 0x88;
constexpr UInt32 kNodeWorldScaleOffset = 0x94;
// NiAVObject (OBVR src/game/GameTypes.h, checked against the EXE there): the
// name at +0x08, the parent at +0x1C, the local transform at +0x30
// (rotation; position +0x54, scale +0x60).
constexpr UInt32 kNodeNameOffset = 0x08;
constexpr UInt32 kNodeParentOffset = 0x1C;
constexpr UInt32 kNodeLocalRotationOffset = 0x30;
constexpr UInt32 kNodeLocalPositionOffset = 0x54;
constexpr UInt32 kNodeLocalScaleOffset = 0x60;
constexpr UInt32 kRefNiNodeOffset = 0x3C;
// NiNode's children (OBVR GameAddresses.h, kNiChildrenOffset): the array at
// +0xB0, the count beside it at +0xB6; holes in the array are normal.
constexpr UInt32 kNodeChildrenOffset = 0xB0;
constexpr UInt32 kNodeChildCountOffset = 0xB6;
constexpr UInt32 kCensusDepth = 32;
constexpr UInt32 kCensusNodes = 1024;
constexpr float kHavokPerUnit = 0.142877f;
constexpr UInt32 kTextStart = 0x00401000;
constexpr UInt32 kTextEnd = 0x00A27C39;
// The collision object getter's first bytes, as OBVR checks them before it
// calls it: mov eax, [esp+4]; push esi; mov esi, [eax+0xA8].
constexpr UInt8 kCollisionObjectOfBytes[8] = {0x8B, 0x44, 0x24, 0x04, 0x56, 0x8B, 0xB0, 0xA8};

// The bones of the human skeleton the ragdoll's bodies were taken to hang
// on. The swap now takes every body on the skeleton (BodiesOf); the list
// stays for the census, which names the bodies it misses.
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

BoneFrame FrameAt(UInt32 node, UInt32 rotation, UInt32 position, UInt32 scale) {
	BoneFrame f{};
	std::memcpy(f.rot.m, reinterpret_cast<const void*>(node + rotation), sizeof(f.rot.m));
	std::memcpy(&f.pos, reinterpret_cast<const void*>(node + position), sizeof(f.pos));
	f.scale = *reinterpret_cast<const float*>(node + scale);
	return f;
}

BoneFrame LocalOf(UInt32 node) {
	return FrameAt(node, kNodeLocalRotationOffset, kNodeLocalPositionOffset, kNodeLocalScaleOffset);
}

BoneFrame WorldOf(UInt32 node) {
	return FrameAt(node, kNodeWorldRotationOffset, kNodeWorldPositionOffset, kNodeWorldScaleOffset);
}

// The walker's bones laid down from its pelvis (PoseLogic.h Retargeted): the
// pelvis on the corpse's, every bone below at the walker's own length from
// its parent, turned as the corpse's bone of its name. Each bone once.
class Retarget {
public:
	Retarget(UInt32 walkerPelvis, UInt32 corpseRoot, const BoneFrame& pelvisLaid)
		: pelvis_(walkerPelvis), corpseRoot_(corpseRoot), pelvisLaid_(pelvisLaid) {}

	// False when the bone does not hang below the pelvis (within 24 parents)
	// or a node on the way does not read. `turnedAsCorpse` says whether the
	// corpse had the bone.
	bool LaidOf(UInt32 node, BoneFrame* out, bool* turnedAsCorpse) { return Laid(node, out, turnedAsCorpse, 0); }

private:
	struct Known {
		UInt32 node;
		BoneFrame laid;
		bool matched;
	};

	bool Laid(UInt32 node, BoneFrame* out, bool* matched, UInt32 depth) {
		if (node == pelvis_) {
			*out = pelvisLaid_;
			*matched = true;
			return true;
		}
		for (UInt32 i = 0; i < count_; ++i) {
			if (known_[i].node == node) {
				*out = known_[i].laid;
				*matched = known_[i].matched;
				return true;
			}
		}
		if (depth > 24 || !LooksLikeObject(node)) {
			return false;
		}
		BoneFrame parent{};
		bool parentMatched = false;
		if (!Laid(Read(node + kNodeParentOffset), &parent, &parentMatched, depth + 1)) {
			return false;
		}
		const UInt32 name = Read(node + kNodeNameOffset);
		const UInt32 match = LooksLikeObject(name) ? NodeOf(corpseRoot_, reinterpret_cast<const char*>(name)) : 0;
		const Mat3 corpseTurn = match != 0 ? WorldOf(match).rot : IdentityMat();
		*out = Retargeted(parent, LocalOf(node), match != 0 ? &corpseTurn : nullptr);
		*matched = match != 0;
		if (count_ < kKnownMost) {
			known_[count_++] = Known{node, *out, *matched};
		}
		return true;
	}

	static constexpr UInt32 kKnownMost = 64;
	UInt32 pelvis_;
	UInt32 corpseRoot_;
	BoneFrame pelvisLaid_;
	Known known_[kKnownMost] = {};
	UInt32 count_ = 0;
};

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

// The class name from MSVC's RTTI (OBVR FirstPersonHide.cpp, ClassNameOf):
// the vtable's complete-object locator at -4, its type descriptor at +0x0C,
// the mangled ".?AVNiNode@@" at +0x08; "?" when it does not read.
const char* ClassNameOf(UInt32 object) {
	const UInt32 vtable = LooksLikeObject(object) ? Read(object) : 0;
	const UInt32 locator = LooksLikeObject(vtable) ? Read(vtable - 4) : 0;
	const UInt32 descriptor = LooksLikeObject(locator) ? Read(locator + 0x0C) : 0;
	if (!LooksLikeObject(descriptor)) {
		return "?";
	}
	const char* const mangled = reinterpret_cast<const char*>(descriptor + 0x08);
	return mangled[0] == '.' && mangled[1] == '?' && mangled[2] == 'A' ? mangled + 4 : "?";
}

const char* NodeNameOf(UInt32 node) {
	const UInt32 name = Read(node + kNodeNameOffset);
	return LooksLikeObject(name) ? reinterpret_cast<const char*>(name) : "";
}

// Every body on a skeleton: the tree under the reference's root walked
// down, each node asked for its body. The bone list (kBones) missed two the
// census of 2026-10-25 18:29 found - "Bip01 NonAccum" on walker and corpse,
// "SideWeapon" on the corpse - so the swap goes by the walk, not by names.
struct BodyAt {
	UInt32 node;
	UInt32 wrapper;
};
constexpr UInt32 kBodiesMost = 48;

struct Bodies {
	BodyAt at[kBodiesMost];
	UInt32 count = 0;
	UInt32 nodes = 0;    // looked at
	bool full = false;   // more bodies than kBodiesMost
};

void CollectBodies(UInt32 node, UInt32 depth, Bodies& bodies) {
	if (!LooksLikeObject(node) || !LooksLikeObject(Read(node)) || bodies.nodes >= kCensusNodes) {
		return;
	}
	++bodies.nodes;
	const UInt32 wrapper = WrapperOf(node);
	if (wrapper != 0) {
		if (bodies.count < kBodiesMost) {
			bodies.at[bodies.count++] = BodyAt{node, wrapper};
		} else {
			bodies.full = true;
		}
	}
	if (depth >= kCensusDepth || !ClassIsNode(ClassNameOf(node))) {
		return;
	}
	const UInt32 children = Read(node + kNodeChildrenOffset);
	const UInt32 count = *reinterpret_cast<const UInt16*>(node + kNodeChildCountOffset);
	if (!LooksLikeObject(children) || count > 512) {
		return;
	}
	for (UInt32 i = 0; i < count; ++i) {
		CollectBodies(Read(children + 4 * i), depth + 1, bodies);
	}
}

void BodiesOf(UInt32 root, Bodies& bodies) {
	bodies.count = 0;
	bodies.nodes = 0;
	bodies.full = false;
	CollectBodies(root, 0, bodies);
}

UInt32 FilterOf(UInt32 wrapper);

// The census for the log: how many bodies, how many the old bone list
// names, and the others with their filter words.
void LogCensus(const char* whose, const Bodies& bodies, LogFn log) {
	char off[384] = {};
	UInt32 used = 0;
	UInt32 listed = 0;
	for (UInt32 i = 0; i < bodies.count; ++i) {
		const char* const name = NodeNameOf(bodies.at[i].node);
		if (NamedIn(name, kBones, kBoneCount)) {
			++listed;
		} else if (used + 48 < sizeof(off)) {
			used += static_cast<UInt32>(std::snprintf(off + used, sizeof(off) - used, "%s%.24s %08X",
			                                          used == 0 ? "" : ", ", name, FilterOf(bodies.at[i].wrapper)));
		}
	}
	log("Pose: %s skeleton carries %u bodies%s, %u on the bone list; off it: %s (%u nodes looked at)", whose,
	    bodies.count, bodies.full ? " (more not taken)" : "", listed, used > 0 ? off : "none", bodies.nodes);
}

// The collision filter word of the wrapper's body; 0 when it has none.
UInt32 FilterOf(UInt32 wrapper) {
	const UInt32 body = wrapper != 0 ? Read(wrapper + kWrapperBodyOffset) : 0;
	return LooksLikeObject(body) ? Read(body + kBodyFilterOffset) : 0;
}

// The lowest spine bone, or the pelvis when the skeleton names none.
UInt32 SpineOrPelvisOf(UInt32 root) {
	const UInt32 spine = NodeOf(root, "Bip01 Spine");
	return spine != 0 ? spine : NodeOf(root, "Bip01 Pelvis");
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

// The direction from `bone` to `child` in the bone's own axes (unit
// length; zero when they do not read) - equal on two skeletons when their
// bones' axes agree.
Vec3 ChildDirectionInBone(UInt32 root, const char* bone, const char* child) {
	const UInt32 b = NodeOf(root, bone);
	const UInt32 c = NodeOf(root, child);
	if (b == 0 || c == 0) {
		return Vec3{0.0f, 0.0f, 0.0f};
	}
	const Mat3 r = RotationOf(b);
	const Vec3 bp = PositionOf(b);
	const Vec3 cp = PositionOf(c);
	const Vec3 d{cp.x - bp.x, cp.y - bp.y, cp.z - bp.z};
	// The rotation's transpose takes a world direction into the bone's axes.
	const Vec3 local{r.m[0][0] * d.x + r.m[1][0] * d.y + r.m[2][0] * d.z,
	                 r.m[0][1] * d.x + r.m[1][1] * d.y + r.m[2][1] * d.z,
	                 r.m[0][2] * d.x + r.m[1][2] * d.y + r.m[2][2] * d.z};
	const float length = std::sqrt(local.x * local.x + local.y * local.y + local.z * local.z);
	return length > 0.0f ? Vec3{local.x / length, local.y / length, local.z / length} : Vec3{0.0f, 0.0f, 0.0f};
}

// Whether the walker's bones carry the human's axes: the child direction of
// a few bones in their own axes, both skeletons, once a session.
void LogBoneAxes(UInt32 walkerRoot, UInt32 corpseRoot, LogFn log) {
	static const char* const kPairs[][2] = {
		{"Bip01 Pelvis", "Bip01 Spine"},       {"Bip01 Spine", "Bip01 Spine1"},  {"Bip01 Spine2", "Bip01 Neck"},
		{"Bip01 L UpperArm", "Bip01 L Forearm"}, {"Bip01 L Thigh", "Bip01 L Calf"}, {"Bip01 R Thigh", "Bip01 R Calf"},
	};
	for (const auto& pair : kPairs) {
		const Vec3 w = ChildDirectionInBone(walkerRoot, pair[0], pair[1]);
		const Vec3 c = ChildDirectionInBone(corpseRoot, pair[0], pair[1]);
		log("Pose: %s to %s in its own axes - the walker's %.2f %.2f %.2f, the corpse's %.2f %.2f %.2f", pair[0] + 6,
		    pair[1] + 6, w.x, w.y, w.z, c.x, c.y, c.z);
	}
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

ApartReport KeepCorpseApart(UInt32 walker, UInt32 corpse) {
	ApartReport report;
	LARGE_INTEGER start;
	QueryPerformanceCounter(&start);
	if (std::memcmp(reinterpret_cast<const void*>(kCollisionObjectOf), kCollisionObjectOfBytes,
	                sizeof(kCollisionObjectOfBytes)) != 0) {
		report.refused = "the collision object getter is not the game's own";
		return report;
	}
	if (std::memcmp(reinterpret_cast<const void*>(kFilterRule), kFilterRuleBytes, sizeof(kFilterRuleBytes)) != 0) {
		report.refused = "the collision rule at 008A7F70 is not the game's own";
		return report;
	}
	const UInt32 walkerRoot = LooksLikeObject(walker) ? Read(walker + kRefNiNodeOffset) : 0;
	const UInt32 corpseRoot = LooksLikeObject(corpse) ? Read(corpse + kRefNiNodeOffset) : 0;
	if (!LooksLikeObject(walkerRoot) || !LooksLikeObject(corpseRoot)) {
		report.refused = LooksLikeObject(walkerRoot) ? "the corpse has no model" : "the walker has no model";
		return report;
	}
	static Bodies walkerBodies;
	static Bodies corpseBodies;
	BodiesOf(walkerRoot, walkerBodies);
	BodiesOf(corpseRoot, corpseBodies);
	UInt32 walkerFilters[kBodiesMost];
	for (UInt32 i = 0; i < walkerBodies.count; ++i) {
		walkerFilters[report.walkerBodies++] = FilterOf(walkerBodies.at[i].wrapper);
	}
	if (report.walkerBodies == 0) {
		report.refused = "the walker has no ragdoll bodies";
		return report;
	}
	UInt32 tables[kFilterTableWords];
	std::memcpy(tables, reinterpret_cast<const void*>(kFilterTables), sizeof(tables));
	struct Move {
		UInt32 wrapper;
		UInt32 body;
		UInt32 filter;
	};
	Move moves[kBodiesMost];
	UInt32 count = 0;
	for (UInt32 i = 0; i < corpseBodies.count; ++i) {
		const UInt32 wrapper = corpseBodies.at[i].wrapper;
		if (Read(Read(wrapper) + kRefreshFilterSlot) != kRefreshFilter) {
			report.refused = "a corpse body's filter refresh is not the engine's 008B0060";
			return report;
		}
		const UInt32 apart = ApartFilter(FilterOf(wrapper), walkerFilters[0]);
		if (!KeptApart(walkerFilters, report.walkerBodies, apart, tables)) {
			report.refused = "the engine's rule, on its live tables, would still let them meet";
			return report;
		}
		moves[count++] = Move{wrapper, Read(wrapper + kWrapperBodyOffset), apart};
	}
	if (count == 0) {
		report.refused = "the corpse has no ragdoll bodies";
		return report;
	}
	{
		Lock lock;
		using RefreshFn = void(__fastcall*)(void* wrapper, void* edx);
		for (UInt32 i = 0; i < count; ++i) {
			*reinterpret_cast<UInt32*>(moves[i].body + kBodyFilterOffset) = moves[i].filter;
			reinterpret_cast<RefreshFn>(kRefreshFilter)(reinterpret_cast<void*>(moves[i].wrapper), nullptr);
		}
	}
	report.done = true;
	report.corpseBodies = count;
	report.group = walkerFilters[0] >> 16;
	report.microseconds = MicrosecondsSince(start);
	return report;
}

PoseReport MatchWalkerToCorpse(UInt32 walker, UInt32 corpse, MatchCorpse mode, bool firstLog, LogFn log,
                               const Vec3* pivotTarget, const Vec3* headTarget) {
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
	const bool targetGiven = pivotTarget != nullptr && headTarget != nullptr && mode == MatchCorpse::Place;
	if (!LooksLikeObject(walkerRoot) || (!targetGiven && !LooksLikeObject(corpseRoot))) {
		log("Pose: %s has no model - the walker lies where it fell", LooksLikeObject(walkerRoot) ? "the corpse" : "the walker");
		return report;
	}

	const UInt32 walkerPelvis = NodeOf(walkerRoot, "Bip01 Pelvis");
	const UInt32 corpsePelvis = NodeOf(corpseRoot, "Bip01 Pelvis");
	const UInt32 walkerHead = HeadNodeOf(walkerRoot);
	const UInt32 corpseHead = HeadNodeOf(corpseRoot);
	// The placement goes by the lowest spine bone: it carries a body, so its
	// node follows the ragdoll each frame, where the pelvis - no body on
	// these skeletons - lags frames behind a moved ragdoll (measured
	// 2026-10-25: still the old spot a frame after the move).
	const UInt32 walkerPivot = SpineOrPelvisOf(walkerRoot);
	const UInt32 corpsePivot = SpineOrPelvisOf(corpseRoot);
	if (corpsePivot != 0) {
		report.corpseRead = true;
		report.corpsePivot = PositionOf(corpsePivot);
		report.corpseHead = corpseHead != 0 ? PositionOf(corpseHead) : report.corpsePivot;
	}
	PlacePlan plan = NoPlan();
	if (walkerPivot != 0 && (targetGiven || corpsePivot != 0)) {
		plan = PlanPlace(PositionOf(walkerPivot), walkerHead != 0 ? PositionOf(walkerHead) : PositionOf(walkerPivot),
		                 targetGiven ? *pivotTarget : report.corpsePivot,
		                 targetGiven ? *headTarget : report.corpseHead);
	}
	report.planOk = plan.ok;
	static Bodies walkerBodies;
	BodiesOf(walkerRoot, walkerBodies);
	if (firstLog && LooksLikeObject(corpseRoot)) {
		LogBoneAxes(walkerRoot, corpseRoot, log);
		static Bodies corpseBodies;
		BodiesOf(corpseRoot, corpseBodies);
		LogCensus("the walker's", walkerBodies, log);
		LogCensus("the corpse's", corpseBodies, log);
	}
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
	// The pose: the walker's pelvis laid on the corpse's (its turn and place,
	// the walker's own scale), the rest retargeted down from it.
	Retarget retarget(walkerPelvis, corpseRoot,
	                  walkerPelvis != 0 && corpsePelvis != 0
	                      ? BoneFrame{WorldOf(corpsePelvis).rot, PositionOf(corpsePelvis), WorldOf(walkerPelvis).scale}
	                      : BoneFrame{IdentityMat(), Vec3{0.0f, 0.0f, 0.0f}, 1.0f});
	const bool canRetarget = walkerPelvis != 0 && corpsePelvis != 0;
	Laying layings[kBodiesMost];
	UInt32 count = 0;
	char found[512] = {};
	UInt32 foundUsed = 0;
	// Each body's filter word, the walker's then the corpse's (0: none), for
	// keeping the two ragdolls from colliding (spec 4.1).
	char filters[768] = {};
	UInt32 filtersUsed = 0;
	for (UInt32 i = 0; i < walkerBodies.count; ++i) {
		const UInt32 node = walkerBodies.at[i].node;
		const UInt32 wrapper = walkerBodies.at[i].wrapper;
		const char* const name = NodeNameOf(node);
		++report.bodies;
		const UInt32 match = name[0] != '\0' ? NodeOf(corpseRoot, name) : 0;
		BoneFrame laid{};
		bool turnedAsCorpse = false;
		BoneSource source = SourceOf(mode, plan.ok, canRetarget);
		if (source == BoneSource::Retargeted && !retarget.LaidOf(node, &laid, &turnedAsCorpse)) {
			source = SourceOf(mode, plan.ok, false);
		}
		if (firstLog && filtersUsed + 40 < sizeof(filters)) {
			filtersUsed += static_cast<UInt32>(std::snprintf(filters + filtersUsed, sizeof(filters) - filtersUsed,
			                                                 "%s%.24s %08X/%08X", filtersUsed == 0 ? "" : ", ",
			                                                 ShortName(name), FilterOf(wrapper),
			                                                 match != 0 ? FilterOf(WrapperOf(match)) : 0u));
		}
		if (firstLog && foundUsed + 40 < sizeof(found)) {
			foundUsed += static_cast<UInt32>(std::snprintf(found + foundUsed, sizeof(found) - foundUsed, "%s%.24s%s",
			                                               foundUsed == 0 ? "" : ", ", ShortName(name),
			                                               match != 0 ? "" : " (not on the corpse)"));
		}
		if (source == BoneSource::Leave) {
			++report.left;
			continue;
		}
		Laying& laying = layings[count++];
		laying.wrapper = wrapper;
		if (source == BoneSource::Retargeted) {
			laying.pos = laid.pos;
			laying.rot = laid.rot;
			++(turnedAsCorpse ? report.copied : report.carried);
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
		log("Pose: the filters, the walker's/the corpse's: %s", filtersUsed > 0 ? filters : "none read");
	}
	return report;
}

}  // namespace walkers
