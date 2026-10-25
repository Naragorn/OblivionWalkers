// Every flow of the walker taking the corpse's place (docs/walkers-spec.md
// 4.1, the swap's look): the setting, which source each of the walker's
// ragdoll bodies takes, the placement's move and turn, and the rotation as
// Havok's quaternion.

#include "PoseLogic.h"

#include <cmath>
#include <cstdio>

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf("  %s  %s\n", condition ? "ok  " : "FAIL", what);
	if (!condition) {
		++g_failures;
	}
}

using walkers::BoneSource;
using walkers::Mat3;
using walkers::MatchCorpse;
using walkers::Vec3;

bool Near(float a, float b, float slack = 0.001f) { return a - b < slack && b - a < slack; }

bool NearVec(const Vec3& a, const Vec3& b) { return Near(a.x, b.x) && Near(a.y, b.y) && Near(a.z, b.z); }

bool NearMat(const Mat3& a, const Mat3& b) {
	for (int r = 0; r < 3; ++r) {
		for (int c = 0; c < 3; ++c) {
			if (!Near(a.m[r][c], b.m[r][c])) {
				return false;
			}
		}
	}
	return true;
}

Mat3 TurnAboutZ(float degrees) {
	const float a = degrees * 3.14159265f / 180.0f;
	Mat3 r = walkers::IdentityMat();
	r.m[0][0] = std::cos(a);
	r.m[0][1] = -std::sin(a);
	r.m[1][0] = std::sin(a);
	r.m[1][1] = std::cos(a);
	return r;
}

void TestSetting() {
	std::printf("[Rise] MatchCorpse\n");
	Check(walkers::MatchCorpseFrom(0) == MatchCorpse::Off, "0: the walker lies where it fell");
	Check(walkers::MatchCorpseFrom(1) == MatchCorpse::Place, "1: moved and turned onto the corpse's spot");
	Check(walkers::MatchCorpseFrom(2) == MatchCorpse::Pose, "2: every bone into the corpse's pose");
	Check(walkers::MatchCorpseFrom(-3) == MatchCorpse::Off, "below 0: off");
	Check(walkers::MatchCorpseFrom(9) == MatchCorpse::Pose, "above 2: the pose");
}

void TestSource() {
	std::printf("Where each of the walker's bodies goes\n");
	Check(walkers::SourceOf(MatchCorpse::Off, true, true) == BoneSource::Leave, "off: every body left as it fell");
	Check(walkers::SourceOf(MatchCorpse::Pose, true, true) == BoneSource::Retargeted,
	      "pose, both pelvises read: retargeted down from the pelvis");
	Check(walkers::SourceOf(MatchCorpse::Pose, true, false) == BoneSource::Placed,
	      "pose without a bone chain to go down: moved and turned with the whole instead");
	Check(walkers::SourceOf(MatchCorpse::Place, true, true) == BoneSource::Placed,
	      "place: moved and turned with the whole, the corpse's bones not used");
	Check(walkers::SourceOf(MatchCorpse::Place, false, true) == BoneSource::Leave,
	      "place without a plan (no pelvis to go by): left");
	Check(walkers::SourceOf(MatchCorpse::Pose, false, false) == BoneSource::Leave,
	      "pose, neither a chain nor a plan: left");
}

void TestHeading() {
	std::printf("The way a lying body points\n");
	float heading = 0.0f;
	Check(walkers::HeadingOf(Vec3{0, 0, 0}, Vec3{10, 0, 1}, &heading) && Near(heading, 0.0f),
	      "head along +x: heading 0");
	Check(walkers::HeadingOf(Vec3{5, 5, 0}, Vec3{5, 25, 0}, &heading) && Near(heading, 1.5707963f),
	      "head along +y: a quarter turn");
	Check(!walkers::HeadingOf(Vec3{0, 0, 0}, Vec3{0.2f, 0.1f, 30.0f}, &heading),
	      "head straight above the pelvis: no heading to read");
}

void TestPlace() {
	std::printf("The walker moved and turned onto the corpse\n");
	// The walker lies along +x with its pelvis at (10, 0, 0); the corpse
	// along +y with its pelvis at (100, 50, 0): a quarter turn and a move.
	const walkers::PlacePlan plan =
		walkers::PlanPlace(Vec3{10, 0, 0}, Vec3{40, 0, 0}, Vec3{100, 50, 0}, Vec3{100, 80, 0});
	Check(plan.ok, "both pelvises read: a plan");
	Vec3 pos{};
	Mat3 rot{};
	walkers::Placed(plan, Vec3{10, 0, 0}, walkers::IdentityMat(), &pos, &rot);
	Check(NearVec(pos, Vec3{100, 50, 0}), "the walker's pelvis lands on the corpse's");
	walkers::Placed(plan, Vec3{40, 0, 0}, walkers::IdentityMat(), &pos, &rot);
	Check(NearVec(pos, Vec3{100, 80, 0}), "its head where the corpse's head lies");
	Check(NearMat(rot, TurnAboutZ(90.0f)), "each body turned the same quarter turn");
	walkers::Placed(plan, Vec3{10, 0, 12}, TurnAboutZ(30.0f), &pos, &rot);
	Check(NearVec(pos, Vec3{100, 50, 12}) && NearMat(rot, TurnAboutZ(120.0f)),
	      "the height kept, a body's own turn added to");

	const walkers::PlacePlan upright =
		walkers::PlanPlace(Vec3{0, 0, 0}, Vec3{0, 0, 30}, Vec3{5, 5, 0}, Vec3{5, 25, 0});
	walkers::Placed(upright, Vec3{0, 0, 30}, walkers::IdentityMat(), &pos, &rot);
	Check(upright.ok && NearVec(pos, Vec3{5, 5, 30}) && NearMat(rot, walkers::IdentityMat()),
	      "a heading that does not read (the walker's head above its pelvis): moved, not turned");

	const walkers::PlacePlan none = walkers::NoPlan();
	Check(!none.ok, "no pelvis on one side: no plan");
}

void TestQuaternion() {
	std::printf("A rotation as Havok's quaternion (x, y, z, w)\n");
	float q[4];
	walkers::QuaternionOf(walkers::IdentityMat(), q);
	Check(Near(q[0], 0) && Near(q[1], 0) && Near(q[2], 0) && Near(q[3], 1), "no turn: (0, 0, 0, 1)");
	walkers::QuaternionOf(TurnAboutZ(90.0f), q);
	Check(Near(q[0], 0) && Near(q[1], 0) && Near(q[2], 0.7071068f) && Near(q[3], 0.7071068f),
	      "a quarter turn about z: (0, 0, sin 45, cos 45)");
	Mat3 flipX = walkers::IdentityMat();
	flipX.m[1][1] = -1.0f;
	flipX.m[2][2] = -1.0f;
	walkers::QuaternionOf(flipX, q);
	Check(Near(std::fabs(q[0]), 1) && Near(q[1], 0) && Near(q[2], 0) && Near(q[3], 0),
	      "half a turn about x (the trace below 0): (1, 0, 0, 0)");
	Mat3 flipY = walkers::IdentityMat();
	flipY.m[0][0] = -1.0f;
	flipY.m[2][2] = -1.0f;
	walkers::QuaternionOf(flipY, q);
	Check(Near(q[0], 0) && Near(std::fabs(q[1]), 1) && Near(q[2], 0) && Near(q[3], 0),
	      "half a turn about y: (0, 1, 0, 0)");
	Mat3 flipZ = walkers::IdentityMat();
	flipZ.m[0][0] = -1.0f;
	flipZ.m[1][1] = -1.0f;
	walkers::QuaternionOf(flipZ, q);
	Check(Near(q[0], 0) && Near(q[1], 0) && Near(std::fabs(q[2]), 1) && Near(q[3], 0),
	      "half a turn about z: (0, 0, 1, 0)");
}

void TestRetarget() {
	std::printf("The corpse's pose along the walker's own bones\n");
	using walkers::BoneFrame;
	// A parent at (10, 0, 0), turned a quarter about z, scale 1; a child 5
	// units along its parent's x.
	const BoneFrame parent{TurnAboutZ(90.0f), Vec3{10, 0, 0}, 1.0f};
	const BoneFrame local{walkers::IdentityMat(), Vec3{5, 0, 0}, 1.0f};
	BoneFrame child = walkers::ChildWorld(parent, local);
	Check(NearVec(child.pos, Vec3{10, 5, 0}) && NearMat(child.rot, TurnAboutZ(90.0f)) && Near(child.scale, 1.0f),
	      "a child's world: along its parent's turned axis, its turn the parent's after its own");
	const BoneFrame bent{TurnAboutZ(30.0f), Vec3{5, 0, 0}, 1.0f};
	child = walkers::ChildWorld(parent, bent);
	Check(NearMat(child.rot, TurnAboutZ(120.0f)), "a bent child: the two turns added");
	const BoneFrame big{walkers::IdentityMat(), Vec3{10, 0, 0}, 2.0f};
	child = walkers::ChildWorld(big, local);
	Check(NearVec(child.pos, Vec3{20, 0, 0}) && Near(child.scale, 2.0f),
	      "a parent of scale 2: the child's offset doubled, its scale the parent's times its own");

	const Mat3 corpseTurn = TurnAboutZ(-45.0f);
	BoneFrame laid = walkers::Retargeted(parent, local, &corpseTurn);
	Check(NearVec(laid.pos, Vec3{10, 5, 0}) && NearMat(laid.rot, corpseTurn),
	      "a bone the corpse has: the corpse's turn, but where the walker's own bone length puts it");
	laid = walkers::Retargeted(parent, bent, nullptr);
	Check(NearVec(laid.pos, Vec3{10, 5, 0}) && NearMat(laid.rot, TurnAboutZ(120.0f)),
	      "a bone the corpse lacks: carried along by its parent as the walker's skeleton holds it");

	// A chain of two, the pelvis laid on the corpse's: the second bone's place
	// follows the first's copied turn, not the corpse's own (longer) bone.
	const BoneFrame pelvis{walkers::IdentityMat(), Vec3{100, 100, 0}, 1.0f};
	const Mat3 up = TurnAboutZ(90.0f);
	const BoneFrame spine = walkers::Retargeted(pelvis, BoneFrame{walkers::IdentityMat(), Vec3{0, 0, 8}, 1.0f}, &up);
	const BoneFrame head = walkers::Retargeted(spine, BoneFrame{walkers::IdentityMat(), Vec3{6, 0, 0}, 1.0f}, nullptr);
	Check(NearVec(spine.pos, Vec3{100, 100, 8}) && NearVec(head.pos, Vec3{100, 106, 8}),
	      "down a chain: each bone at its own length from its parent, turned as the corpse's parent is");
}

}  // namespace

int main() {
	TestSetting();
	TestSource();
	TestHeading();
	TestPlace();
	TestQuaternion();
	TestRetarget();
	if (g_failures != 0) {
		std::printf("%d failed\n", g_failures);
		return 1;
	}
	std::printf("all passed\n");
	return 0;
}
