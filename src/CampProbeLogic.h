#pragma once

// The dynamic camps' probe (docs/walkers-spec.md 4.20.6, dynamic camps; Nadi,
// 2026-11-06: "go"): can a camp's placed tent and map marker be moved by
// script, the tent's collision going with it? Test switch [Survivors]
// TestProbe=N (camp N, -1 off; harness/camp-probe.txt). The plugin drops the
// player from above onto two spots - where the tent is put ("new") and where
// it stood ("old") - before the move, after it (SetPos) and after a Disable
// and Enable, after an Update3D and after the player went far away and came
// back (the cells loaded anew), and logs where each fall first stopped. A fall stopped well
// above the new spot's own ground (its drop before the move) means the
// collision is there.
//
// Pure: the order of the steps, the stop of a fall read off its heights, the
// verdict.

#include <cstddef>

namespace walkers {

enum class ProbeAction {
	None,        // keep falling, or waiting
	DropAtNew,   // the player put above the new spot
	DropAtOld,   // the player put above the old spot
	MoveTent,    // the tent (and the map marker) SetPos to the new spot
	Reenable,    // the tent disabled and enabled again
	Update3D,    // the tent's 3D rebuilt
	GoAway,      // the player taken far away, the camp's cells unloaded
	ComeBack,    // the player back above the camp, its cells loaded anew
	Done,
};

// The drops in their order and the step each follows.
enum class ProbeDrop : unsigned {
	NewBefore,
	OldBefore,
	NewMoved,
	OldMoved,
	NewReenabled,
	OldReenabled,
	NewUpdated,
	OldUpdated,
	NewReloaded,
	OldReloaded,
	Count
};

inline constexpr float kProbeDropLift = 600.0f;     // units above the spot's tent
inline constexpr float kProbeFallSeconds = 4.0f;    // a drop's heights read this long
inline constexpr float kProbeSettleSeconds = 1.0f;  // after a move, before the next drop
inline constexpr float kProbeAwaySeconds = 10.0f;   // away, and back before the next drop
inline constexpr float kProbeStallUnits = 0.5f;     // a sample this close to the last: stopped
inline constexpr float kProbeFallenUnits = 20.0f;   // fallen at least this far before a stop counts
inline constexpr float kProbeAboveUnits = 40.0f;    // stopped this far above the ground: on the tent
inline constexpr unsigned kProbeSamplesMost = 64;

struct ProbeState {
	unsigned step = 0;  // into kProbePlan
	float clock = 0.0f;
	bool begun = false;
};

// The plan: each action and how long after it the next follows.
struct ProbePlanStep {
	ProbeAction action;
	float seconds;
};

inline constexpr ProbePlanStep kProbePlan[] = {
    {ProbeAction::DropAtNew, kProbeFallSeconds}, {ProbeAction::DropAtOld, kProbeFallSeconds},
    {ProbeAction::MoveTent, kProbeSettleSeconds}, {ProbeAction::DropAtNew, kProbeFallSeconds},
    {ProbeAction::DropAtOld, kProbeFallSeconds},  {ProbeAction::Reenable, kProbeSettleSeconds},
    {ProbeAction::DropAtNew, kProbeFallSeconds},  {ProbeAction::DropAtOld, kProbeFallSeconds},
    {ProbeAction::Update3D, kProbeSettleSeconds}, {ProbeAction::DropAtNew, kProbeFallSeconds},
    {ProbeAction::DropAtOld, kProbeFallSeconds},  {ProbeAction::GoAway, kProbeAwaySeconds},
    {ProbeAction::ComeBack, kProbeAwaySeconds},   {ProbeAction::DropAtNew, kProbeFallSeconds},
    {ProbeAction::DropAtOld, kProbeFallSeconds},
};
inline constexpr unsigned kProbePlanSteps = sizeof(kProbePlan) / sizeof(kProbePlan[0]);

// The action due now, `dt` after the last call: the first at once, each next
// when the one before has had its time; Done once, after the last.
inline ProbeAction NextProbeAction(ProbeState& s, float dt) {
	if (s.step > kProbePlanSteps) {
		return ProbeAction::None;
	}
	if (!s.begun) {
		s.begun = true;
		s.clock = 0.0f;
		return kProbePlan[0].action;
	}
	s.clock += dt;
	if (s.clock < kProbePlan[s.step].seconds) {
		return ProbeAction::None;
	}
	s.clock = 0.0f;
	++s.step;
	if (s.step == kProbePlanSteps) {
		++s.step;
		return ProbeAction::Done;
	}
	return kProbePlan[s.step].action;
}

// Whether the step the plan is in is a drop (its heights to be sampled).
inline bool ProbeInDrop(const ProbeState& s) {
	return s.begun && s.step < kProbePlanSteps &&
	       (kProbePlan[s.step].action == ProbeAction::DropAtNew || kProbePlan[s.step].action == ProbeAction::DropAtOld);
}

// The drop the plan is in (its index among the drops), Count when none.
inline ProbeDrop ProbeDropOf(const ProbeState& s) {
	if (!ProbeInDrop(s)) {
		return ProbeDrop::Count;
	}
	unsigned drops = 0;
	for (unsigned k = 0; k < s.step; ++k) {
		if (kProbePlan[k].action == ProbeAction::DropAtNew || kProbePlan[k].action == ProbeAction::DropAtOld) {
			++drops;
		}
	}
	return static_cast<ProbeDrop>(drops);
}

// Where a fall first stopped: the first height no lower than the one before
// by more than kProbeStallUnits, once the fall has come kProbeFallenUnits
// down from the first; -1e9 when it never stopped (or never fell).
inline float FallStopZ(const float* z, std::size_t n) {
	if (n == 0) {
		return -1e9f;
	}
	for (std::size_t i = 1; i < n; ++i) {
		if (z[0] - z[i] >= kProbeFallenUnits && z[i - 1] - z[i] <= kProbeStallUnits) {
			return z[i];
		}
	}
	return -1e9f;
}

enum class ProbeVerdict { Unknown, OnTent, OnGround };

// A drop's stop against the spot's ground (its drop with no tent there).
inline ProbeVerdict StopVerdict(float stop, float ground) {
	if (stop < -1e8f || ground < -1e8f) {
		return ProbeVerdict::Unknown;
	}
	return stop - ground >= kProbeAboveUnits ? ProbeVerdict::OnTent : ProbeVerdict::OnGround;
}

inline const char* VerdictName(ProbeVerdict v) {
	return v == ProbeVerdict::OnTent ? "on the tent" : v == ProbeVerdict::OnGround ? "on the ground" : "unknown";
}

}  // namespace walkers
