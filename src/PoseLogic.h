#pragma once

// The walker taking the corpse's place, process-free (docs/walkers-spec.md
// 4.1, the swap's look; Nadi, 2026-10-25: both ways, the pose by default).
// When the walker has settled unseen beside the corpse, its ragdoll's
// bodies are laid where the corpse lies before the two change places:
//
// - Place (MatchCorpse=1): the whole walker moved so its pelvis lies on the
//   corpse's, and turned about the vertical so its head points the way the
//   corpse's does. The limbs keep the walker's own fall.
// - Pose (MatchCorpse=2): every body on a bone the corpse has too takes that
//   bone's place and turn - the corpse's pose; a body on a bone the corpse
//   lacks is moved and turned as in Place, so the body holds together.
//
// A body's place is its node's world transform: the engine's own
// node-to-Havok push (0x0089EAE0) hands the node's world translation and
// rotation to the body wrapper's SetTranslationAndRotation (vtable +0xA0),
// so Pose.cpp hands it the same. Rotations here are Gamebryo's NiMatrix3 as
// it stores them (m[row][column], world = rotation * local); a point turns
// by rotation * point.

#include <cmath>

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
// onto the corpse's spot, 2 (the default) every bone in the corpse's pose.
enum class MatchCorpse : UInt8 { Off = 0, Place = 1, Pose = 2 };

inline MatchCorpse MatchCorpseFrom(int value) {
	return value <= 0 ? MatchCorpse::Off : (value == 1 ? MatchCorpse::Place : MatchCorpse::Pose);
}

// What one of the walker's ragdoll bodies takes.
enum class BoneSource : UInt8 { Leave, Placed, Copied };

inline BoneSource SourceOf(MatchCorpse mode, bool planOk, bool corpseHasBone) {
	if (mode == MatchCorpse::Off) {
		return BoneSource::Leave;
	}
	if (mode == MatchCorpse::Pose && corpseHasBone) {
		return BoneSource::Copied;
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
