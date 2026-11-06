// Every flow of the dynamic camps' probe (CampProbeLogic.h): the plan's
// order and timing, the drops it is in, a fall's stop, the verdict.

#include "CampProbeLogic.h"

#include <cstdio>

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf("  %s  %s\n", condition ? "ok  " : "FAIL", what);
	if (!condition) {
		++g_failures;
	}
}

using walkers::ProbeAction;
using walkers::ProbeDrop;
using walkers::ProbeState;
using walkers::ProbeVerdict;

void Plan() {
	std::printf("the plan\n");
	ProbeState s;
	Check(!walkers::ProbeInDrop(s), "not in a drop before it begins");
	Check(walkers::NextProbeAction(s, 0.0f) == ProbeAction::DropAtNew, "the first drop at once, above the new spot");
	Check(walkers::ProbeInDrop(s) && walkers::ProbeDropOf(s) == ProbeDrop::NewBefore, "in the new spot's drop before");
	Check(walkers::NextProbeAction(s, 3.9f) == ProbeAction::None, "the fall read on before its time is up");
	Check(walkers::NextProbeAction(s, 0.2f) == ProbeAction::DropAtOld, "then the old spot's drop");
	Check(walkers::ProbeDropOf(s) == ProbeDrop::OldBefore, "in the old spot's drop before");
	Check(walkers::NextProbeAction(s, 4.0f) == ProbeAction::MoveTent, "then the tent moved");
	Check(!walkers::ProbeInDrop(s) && walkers::ProbeDropOf(s) == ProbeDrop::Count, "no drop while it settles");
	Check(walkers::NextProbeAction(s, 0.5f) == ProbeAction::None, "settling");
	Check(walkers::NextProbeAction(s, 0.5f) == ProbeAction::DropAtNew, "the new spot again after a second");
	Check(walkers::ProbeDropOf(s) == ProbeDrop::NewMoved, "the new spot's drop after the move");
	Check(walkers::NextProbeAction(s, 4.0f) == ProbeAction::DropAtOld, "the old spot after the move");
	Check(walkers::ProbeDropOf(s) == ProbeDrop::OldMoved, "the old spot's drop after the move");
	Check(walkers::NextProbeAction(s, 4.0f) == ProbeAction::Reenable, "then disabled and enabled");
	Check(walkers::NextProbeAction(s, 1.0f) == ProbeAction::DropAtNew, "the new spot after it");
	Check(walkers::ProbeDropOf(s) == ProbeDrop::NewReenabled, "the new spot's drop after the enable");
	Check(walkers::NextProbeAction(s, 4.0f) == ProbeAction::DropAtOld, "the old spot after it");
	Check(walkers::ProbeDropOf(s) == ProbeDrop::OldReenabled, "the old spot's drop after the enable");
	Check(walkers::NextProbeAction(s, 4.0f) == ProbeAction::Update3D, "then its 3D rebuilt");
	Check(walkers::NextProbeAction(s, 1.0f) == ProbeAction::DropAtNew, "the new spot after it");
	Check(walkers::ProbeDropOf(s) == ProbeDrop::NewUpdated, "the new spot's drop after Update3D");
	Check(walkers::NextProbeAction(s, 4.0f) == ProbeAction::DropAtOld, "the old spot after it");
	Check(walkers::ProbeDropOf(s) == ProbeDrop::OldUpdated, "the old spot's drop after Update3D");
	Check(walkers::NextProbeAction(s, 4.0f) == ProbeAction::GoAway, "then the player taken far away");
	Check(!walkers::ProbeInDrop(s), "no drop while away");
	Check(walkers::NextProbeAction(s, 9.9f) == ProbeAction::None, "away ten seconds");
	Check(walkers::NextProbeAction(s, 0.2f) == ProbeAction::ComeBack, "then back above the camp");
	Check(walkers::NextProbeAction(s, 9.9f) == ProbeAction::None, "ten seconds to load the cells");
	Check(walkers::NextProbeAction(s, 0.2f) == ProbeAction::DropAtNew, "the new spot after the way back");
	Check(walkers::ProbeDropOf(s) == ProbeDrop::NewReloaded, "the new spot's drop after the cells reloaded");
	Check(walkers::NextProbeAction(s, 4.0f) == ProbeAction::DropAtOld, "the old spot after it");
	Check(walkers::ProbeDropOf(s) == ProbeDrop::OldReloaded, "the old spot's drop after the cells reloaded");
	Check(walkers::NextProbeAction(s, 4.0f) == ProbeAction::Done, "done after the last fall");
	Check(!walkers::ProbeInDrop(s), "no drop once done");
	Check(walkers::NextProbeAction(s, 4.0f) == ProbeAction::None, "done only once");
	Check(walkers::NextProbeAction(s, 100.0f) == ProbeAction::None, "nothing after");
}

void Stop() {
	std::printf("a fall's stop\n");
	Check(walkers::FallStopZ(nullptr, 0) < -1e8f, "no heights: never stopped");
	const float standing[] = {100.0f, 100.0f, 100.0f};
	Check(walkers::FallStopZ(standing, 3) < -1e8f, "never fell: no stop");
	const float falling[] = {700.0f, 650.0f, 560.0f, 420.0f, 240.0f};
	Check(walkers::FallStopZ(falling, 5) < -1e8f, "still falling: no stop");
	const float roof[] = {700.0f, 640.0f, 520.0f, 300.0f, 300.0f, 260.0f, 120.0f, 100.0f, 100.0f};
	Check(walkers::FallStopZ(roof, 9) == 300.0f, "the first stop counts, not where it slid to");
	const float ground[] = {700.0f, 600.0f, 400.0f, 101.0f, 100.7f};
	Check(walkers::FallStopZ(ground, 5) == 100.7f, "a stop within half a unit counts");
	const float bounce[] = {700.0f, 600.0f, 400.0f, 100.0f, 104.0f};
	Check(walkers::FallStopZ(bounce, 5) == 104.0f, "a sample above the one before counts as stopped");
	const float shallow[] = {100.0f, 90.0f, 90.0f};
	Check(walkers::FallStopZ(shallow, 3) < -1e8f, "a stop before 20 units of fall does not count");
}

void Verdict() {
	std::printf("the verdict\n");
	Check(walkers::StopVerdict(-1e9f, 100.0f) == ProbeVerdict::Unknown, "no stop: unknown");
	Check(walkers::StopVerdict(100.0f, -1e9f) == ProbeVerdict::Unknown, "no ground: unknown");
	Check(walkers::StopVerdict(140.0f, 100.0f) == ProbeVerdict::OnTent, "40 units above the ground: on the tent");
	Check(walkers::StopVerdict(139.0f, 100.0f) == ProbeVerdict::OnGround, "less: on the ground");
	Check(walkers::StopVerdict(90.0f, 100.0f) == ProbeVerdict::OnGround, "below: on the ground");
	Check(std::printf("%s %s %s\n", walkers::VerdictName(ProbeVerdict::OnTent),
	                  walkers::VerdictName(ProbeVerdict::OnGround), walkers::VerdictName(ProbeVerdict::Unknown)) > 0,
	      "the names");
}

}  // namespace

int main() {
	Plan();
	Stop();
	Verdict();
	std::printf("%s\n", g_failures == 0 ? "all passed" : "FAILURES");
	return g_failures == 0 ? 0 : 1;
}
