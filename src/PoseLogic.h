#pragma once

// The walker taking the corpse's place, process-free (docs/walkers-spec.md
// 4.1, the swap's look; Nadi, 2026-10-25: both ways; by default the pose,
// settled unseen with the corpse kept apart, then placed).
// When the walker has settled unseen beside the corpse, its ragdoll's
// bodies are laid where the corpse lies before the two change places:
//
// - Place (MatchCorpse=1): the whole walker moved so its pelvis lies on the
//   corpse's, and turned about the vertical so its head points the way the
//   corpse's does. The limbs keep the walker's own fall.
// - Pose (MatchCorpse=2): the walker's pelvis laid on the corpse's, and
//   every bone below retargeted from it - turned as the corpse's bone of the
//   same name, placed at the walker's own bone length (Retargeted). The
//   swap lets it settle unseen and then places the settled body as in Place
//   (Nadi, 2026-10-25: "B dann A"). The pose turned 46 to 83 degrees off
//   while it settled as long as the body at "Bip01 NonAccum" - missed by
//   the bone list - stayed behind; with every body laid it holds to a few
//   degrees (Pose.cpp, BodiesOf).
//
// A body's place is its node's world transform: the engine's own
// node-to-Havok push (0x0089EAE0) hands the node's world translation and
// rotation to the body wrapper's SetTranslationAndRotation (vtable +0xA0),
// so Pose.cpp hands it the same. Rotations here are Gamebryo's NiMatrix3 as
// it stores them (m[row][column], world = rotation * local); a point turns
// by rotation * point.

#include <cmath>
#include <cstring>

#include "Types.h"

namespace walkers {

struct Vec3 {
	float x;
	float y;
	float z;
};

struct Mat3 {
	float m[3][3];
};

inline Mat3 IdentityMat() {
	Mat3 r{};
	r.m[0][0] = 1.0f;
	r.m[1][1] = 1.0f;
	r.m[2][2] = 1.0f;
	return r;
}

inline Mat3 Mul(const Mat3& a, const Mat3& b) {
	Mat3 r{};
	for (int i = 0; i < 3; ++i) {
		for (int j = 0; j < 3; ++j) {
			r.m[i][j] = a.m[i][0] * b.m[0][j] + a.m[i][1] * b.m[1][j] + a.m[i][2] * b.m[2][j];
		}
	}
	return r;
}

inline Vec3 Mul(const Mat3& a, const Vec3& v) {
	return Vec3{a.m[0][0] * v.x + a.m[0][1] * v.y + a.m[0][2] * v.z,
	            a.m[1][0] * v.x + a.m[1][1] * v.y + a.m[1][2] * v.z,
	            a.m[2][0] * v.x + a.m[2][1] * v.y + a.m[2][2] * v.z};
}

// [Rise] MatchCorpse: 0 the walker lies where it fell, 1 moved and turned
// onto the corpse's spot, 2 (the default) the corpse's pose, settled, then
// moved and turned onto the corpse's spot.
enum class MatchCorpse : UInt8 { Off = 0, Place = 1, Pose = 2 };

inline MatchCorpse MatchCorpseFrom(int value) {
	return value <= 0 ? MatchCorpse::Off : (value == 1 ? MatchCorpse::Place : MatchCorpse::Pose);
}

// The body census (spec 4.1: the place alone still turned the walker 80
// degrees in one run of two with nothing to push it - are there bodies the
// bone list misses?). Whether an object's class, by its MSVC RTTI name
// (with or without the "@@" tail), is a node that keeps children: every
// NetImmerse node class ends in "Node" - NiNode, BSFadeNode,
// NiBillboardNode - and geometry does not (OBVR FirstPersonHide.cpp,
// ClassIsNode). A bare "Node" is no class of the engine's.
inline bool ClassIsNode(const char* className) {
	if (className == nullptr) {
		return false;
	}
	UInt32 length = 0;
	while (className[length] != '\0' && length < 96) {
		++length;
	}
	while (length > 0 && className[length - 1] == '@') {
		--length;
	}
	return length > 4 && std::strncmp(className + length - 4, "Node", 4) == 0;
}

// Whether `name` is one of `list` - the engine's node names, exactly.
inline bool NamedIn(const char* name, const char* const* list, UInt32 count) {
	if (name == nullptr) {
		return false;
	}
	for (UInt32 i = 0; i < count; ++i) {
		if (std::strcmp(name, list[i]) == 0) {
			return true;
		}
	}
	return false;
}

// A node's name for the log: "Bip01 " dropped, others as they are.
inline const char* ShortName(const char* name) {
	if (name == nullptr) {
		return "";
	}
	return std::strncmp(name, "Bip01 ", 6) == 0 ? name + 6 : name;
}

// What one of the walker's ragdoll bodies takes: left as it fell, moved and
// turned with the whole (Place), or retargeted down from the pelvis (Pose) -
// which needs both pelvises; without them the pose falls back to the place,
// and without a plan either the body is left.
enum class BoneSource : UInt8 { Leave, Placed, Retargeted };

inline BoneSource SourceOf(MatchCorpse mode, bool planOk, bool canRetarget) {
	if (mode == MatchCorpse::Off) {
		return BoneSource::Leave;
	}
	if (mode == MatchCorpse::Pose && canRetarget) {
		return BoneSource::Retargeted;
	}
	return planOk ? BoneSource::Placed : BoneSource::Leave;
}

// The way a lying body points: from its pelvis to its head, about the
// vertical (radians, atan2 of y over x). False when the head stands too
// straight above the pelvis for a direction to read (under a unit apart on
// the ground).
inline bool HeadingOf(const Vec3& pelvis, const Vec3& head, float* radians) {
	const float dx = head.x - pelvis.x;
	const float dy = head.y - pelvis.y;
	if (dx * dx + dy * dy < 1.0f) {
		return false;
	}
	*radians = std::atan2(dy, dx);
	return true;
}

// The placement: points move from `from` to `to` turned by `turn` about the
// vertical through them.
struct PlacePlan {
	bool ok = false;
	Vec3 from{0.0f, 0.0f, 0.0f};
	Vec3 to{0.0f, 0.0f, 0.0f};
	Mat3 turn = IdentityMat();
};

inline PlacePlan NoPlan() { return PlacePlan{}; }

inline PlacePlan PlanPlace(const Vec3& walkerPelvis, const Vec3& walkerHead, const Vec3& corpsePelvis,
                           const Vec3& corpseHead) {
	PlacePlan plan;
	plan.ok = true;
	plan.from = walkerPelvis;
	plan.to = corpsePelvis;
	float walkerHeading = 0.0f;
	float corpseHeading = 0.0f;
	if (HeadingOf(walkerPelvis, walkerHead, &walkerHeading) && HeadingOf(corpsePelvis, corpseHead, &corpseHeading)) {
		const float a = corpseHeading - walkerHeading;
		plan.turn.m[0][0] = std::cos(a);
		plan.turn.m[0][1] = -std::sin(a);
		plan.turn.m[1][0] = std::sin(a);
		plan.turn.m[1][1] = std::cos(a);
	}
	return plan;
}

inline void Placed(const PlacePlan& plan, const Vec3& pos, const Mat3& rot, Vec3* outPos, Mat3* outRot) {
	const Vec3 offset{pos.x - plan.from.x, pos.y - plan.from.y, pos.z - plan.from.z};
	const Vec3 turned = Mul(plan.turn, offset);
	*outPos = Vec3{turned.x + plan.to.x, turned.y + plan.to.y, turned.z + plan.to.z};
	*outRot = Mul(plan.turn, rot);
}

// A bone's transform as Gamebryo keeps it, local (to its parent) or world.
struct BoneFrame {
	Mat3 rot;
	Vec3 pos;
	float scale;
};

// A child's world transform from its parent's world and its own local one
// (NiAVObject's world update): the offset turned and scaled by the parent,
// the turns composed, the scales multiplied.
inline BoneFrame ChildWorld(const BoneFrame& parent, const BoneFrame& local) {
	const Vec3 offset = Mul(parent.rot, Vec3{local.pos.x * parent.scale, local.pos.y * parent.scale,
	                                         local.pos.z * parent.scale});
	return BoneFrame{Mul(parent.rot, local.rot),
	                 Vec3{parent.pos.x + offset.x, parent.pos.y + offset.y, parent.pos.z + offset.z},
	                 parent.scale * local.scale};
}

// The pose by retargeting (MatchCorpse=2, rebuilt 2026-10-25 after the
// first build laid each body on the human bone's own place and the walker's
// joints, of other lengths, pulled the body 55 degrees round within three
// seconds): a bone is placed by its parent's laid transform and its own
// local offset - the walker's bone length - and turned as the corpse's bone
// of the same name is (`sourceRot`), or, where the corpse has none, as the
// walker's skeleton carries it.
inline BoneFrame Retargeted(const BoneFrame& parentLaid, const BoneFrame& local, const Mat3* sourceRot) {
	BoneFrame laid = ChildWorld(parentLaid, local);
	if (sourceRot != nullptr) {
		laid.rot = *sourceRot;
	}
	return laid;
}

// A rotation as a unit quaternion in Havok's order (x, y, z, w), by the
// standard formulas with Shepperd's choice of the largest diagonal term -
// what the engine's NiQuaternion::FromRotation (0x007150F0) gives the push,
// and OBVR's GrabPhysics.h QuaternionFromRotation, which places its hand
// bodies so.
inline void QuaternionOf(const Mat3& r, float q[4]) {
	const float m00 = r.m[0][0];
	const float m11 = r.m[1][1];
	const float m22 = r.m[2][2];
	const float trace = m00 + m11 + m22;
	float x;
	float y;
	float z;
	float w;
	if (trace > 0.0f) {
		const float s = std::sqrt(trace + 1.0f) * 2.0f;
		w = 0.25f * s;
		x = (r.m[2][1] - r.m[1][2]) / s;
		y = (r.m[0][2] - r.m[2][0]) / s;
		z = (r.m[1][0] - r.m[0][1]) / s;
	} else if (m00 > m11 && m00 > m22) {
		const float s = std::sqrt(1.0f + m00 - m11 - m22) * 2.0f;
		w = (r.m[2][1] - r.m[1][2]) / s;
		x = 0.25f * s;
		y = (r.m[0][1] + r.m[1][0]) / s;
		z = (r.m[0][2] + r.m[2][0]) / s;
	} else if (m11 > m22) {
		const float s = std::sqrt(1.0f + m11 - m00 - m22) * 2.0f;
		w = (r.m[0][2] - r.m[2][0]) / s;
		x = (r.m[0][1] + r.m[1][0]) / s;
		y = 0.25f * s;
		z = (r.m[1][2] + r.m[2][1]) / s;
	} else {
		const float s = std::sqrt(1.0f + m22 - m00 - m11) * 2.0f;
		w = (r.m[1][0] - r.m[0][1]) / s;
		x = (r.m[0][2] + r.m[2][0]) / s;
		y = (r.m[1][2] + r.m[2][1]) / s;
		z = 0.25f * s;
	}
	const float length = std::sqrt(x * x + y * y + z * z + w * w);
	const float inv = length > 0.0f ? 1.0f / length : 0.0f;
	q[0] = x * inv;
	q[1] = y * inv;
	q[2] = z * inv;
	q[3] = w * inv;
}

}  // namespace walkers
