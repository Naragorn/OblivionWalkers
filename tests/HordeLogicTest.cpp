// Every flow of the horde's bookkeeping (docs/walkers-spec.md 4.11): lone
// walkers lead or join by rank and distance, the leader's marker ahead and
// re-placed when reached or stale, a lost leader replaced, groups that meet
// merged, the unfit dropped, and the packages kept on those who lose them.

#include "HordeLogic.h"

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

using walkers::HordeOrder;
using walkers::HordeSettings;
using walkers::HordeState;
using walkers::HordeWalker;

constexpr UInt32 kTravel = 0x11000806;
constexpr UInt32 kFollow = 0x11000816;

HordeWalker Walker(UInt32 id, float x, float y, UInt32 rank, UInt32 package = 0, bool fit = true) {
	HordeWalker w;
	w.id = id;
	w.x = x;
	w.y = y;
	w.rank = rank;
	w.package = package;
	w.fit = fit;
	return w;
}

struct Run {
	HordeState state;
	HordeSettings settings;
	UInt32 seed = 12345;
	HordeOrder orders[64];
	UInt32 count = 0;

	Run() {
		settings.enabled = true;
		settings.travelBase = kTravel;
		settings.followBase = kFollow;
	}
	UInt32 Step(const HordeWalker* walkers, UInt32 n, float dt = 1.0f) {
		count = walkers::StepHorde(state, walkers, n, settings, dt, seed, orders, 64);
		return count;
	}
	UInt32 Of(HordeOrder::Kind kind, UInt32 walker = 0) const {
		UInt32 found = 0;
		for (UInt32 i = 0; i < count; ++i) {
			found += orders[i].kind == kind && (walker == 0 || orders[i].walker == walker) ? 1u : 0u;
		}
		return found;
	}
	const HordeOrder* First(HordeOrder::Kind kind) const {
		for (UInt32 i = 0; i < count; ++i) {
			if (orders[i].kind == kind) {
				return &orders[i];
			}
		}
		return nullptr;
	}
};

void TestOff() {
	std::printf("Off\n");
	Run run;
	run.settings.enabled = false;
	const HordeWalker w[] = {Walker(1, 0, 0, 5)};
	Check(run.Step(w, 1) == 0 && !run.state.groups[0].used, "[Horde] Enabled=0: no orders, no groups");
}

void TestLeadAndJoin() {
	std::printf("Leading and joining\n");
	Run run;
	const HordeWalker w[] = {Walker(1, 0, 0, 3), Walker(2, 500, 0, 9), Walker(3, 900, 200, 1)};
	run.Step(w, 3);
	Check(run.Of(HordeOrder::Lead, 2) == 1 && run.Of(HordeOrder::Lead) == 1,
	      "the highest rank of those near each other leads (walker 2, rank 9)");
	Check(run.Of(HordeOrder::Follow, 1) == 1 && run.Of(HordeOrder::Follow, 3) == 1,
	      "the others within JoinRadius follow it");
	const HordeOrder* move = run.First(HordeOrder::MoveMarker);
	const float dx = move != nullptr ? move->x - 500.0f : 0.0f;
	const float dy = move != nullptr ? move->y : 0.0f;
	Check(move != nullptr && std::fabs(std::sqrt(dx * dx + dy * dy) - run.settings.leadUnits) < 1.0f,
	      "its marker goes LeadUnits ahead of it, before the lead order");
	const HordeOrder* lead = run.First(HordeOrder::Lead);
	const HordeOrder* follow = run.First(HordeOrder::Follow);
	Check(lead != nullptr && follow != nullptr && lead->package == kTravel + lead->group &&
	          follow->package == kFollow + lead->group && follow->group == lead->group,
	      "the leader gets its group's travel package, the followers its follow package");

	Run far;
	const HordeWalker apart[] = {Walker(1, 0, 0, 3), Walker(2, 5000, 0, 9)};
	far.Step(apart, 2);
	Check(far.Of(HordeOrder::Lead) == 2 && far.Of(HordeOrder::Follow) == 0,
	      "two walkers farther apart than JoinRadius: each leads a group of its own");

	Run full;
	full.settings.groupMost = 1;
	const HordeWalker crowd[] = {Walker(1, 0, 0, 9), Walker(2, 100, 0, 5), Walker(3, 200, 0, 4)};
	full.Step(crowd, 3);
	Check(full.Of(HordeOrder::Follow) == 1 && full.Of(HordeOrder::Lead) == 2,
	      "a full group (GroupMost followers): the next one near leads its own");

	Run unfit;
	const HordeWalker fighting[] = {Walker(1, 0, 0, 9, 0, false)};
	unfit.Step(fighting, 1);
	Check(unfit.count == 0, "an unfit walker (fighting, dead, indoors, far off): no orders");
}

void TestKeepAlive() {
	std::printf("Keeping the packages on\n");
	Run run;
	const HordeWalker w[] = {Walker(1, 0, 0, 9), Walker(2, 300, 0, 1)};
	run.Step(w, 2);
	const UInt32 g = run.First(HordeOrder::Lead)->group;
	const HordeWalker on[] = {Walker(1, 10, 0, 9, kTravel + g), Walker(2, 300, 0, 1, kFollow + g)};
	run.Step(on, 2);
	Check(run.count == 0, "both still run their packages, the leader far from its marker: nothing to do");
	const HordeWalker lost[] = {Walker(1, 20, 0, 9, 0), Walker(2, 300, 0, 1, 0x000AAAAA)};
	run.Step(lost, 2);
	Check(run.Of(HordeOrder::Lead, 1) == 1 && run.Of(HordeOrder::Follow, 2) == 1 && run.Of(HordeOrder::MoveMarker) == 0,
	      "a package lost (after a fight, an interrupt): given again, the marker left where it is");
}

void TestMarker() {
	std::printf("The marker ahead\n");
	Run run;
	run.settings.leadUnits = 1000.0f;
	const HordeWalker w[] = {Walker(1, 0, 0, 9)};
	run.Step(w, 1);
	const UInt32 g = run.First(HordeOrder::Lead)->group;
	const float tx = run.state.groups[g].targetX;
	const float ty = run.state.groups[g].targetY;
	const HordeWalker there[] = {Walker(1, tx - 100.0f, ty, 9, kTravel + g)};
	run.Step(there, 1);
	const HordeOrder* move = run.First(HordeOrder::MoveMarker);
	Check(move != nullptr && run.Of(HordeOrder::Lead, 1) == 1,
	      "the leader within ReachUnits of its marker: the marker moves on and the lead is given again");
	const float dx = move != nullptr ? move->x - (tx - 100.0f) : 0.0f;
	const float dy = move != nullptr ? move->y - ty : 0.0f;
	const float turn = std::fabs(std::remainder(std::atan2(dy, dx) - std::atan2(ty, tx), 6.2831853f));
	Check(move != nullptr && turn <= run.settings.turnDegrees * 3.14159265f / 180.0f + 0.01f,
	      "the new heading turns no more than TurnDegrees from the last - the horde drifts on");

	Run stale;
	stale.settings.staleSeconds = 10.0f;
	const HordeWalker start[] = {Walker(1, 0, 0, 9)};
	stale.Step(start, 1);
	const UInt32 sg = stale.First(HordeOrder::Lead)->group;
	const HordeWalker stuck[] = {Walker(1, 0, 0, 9, kTravel + sg)};
	UInt32 moves = 0;
	for (int t = 0; t < 9; ++t) {
		stale.Step(stuck, 1, 1.0f);
		moves += stale.Of(HordeOrder::MoveMarker);
	}
	Check(moves == 0, "9 s without coming nearer: still the same marker");
	stale.Step(stuck, 1, 1.0f);
	Check(stale.Of(HordeOrder::MoveMarker) == 1 && stale.Of(HordeOrder::Lead, 1) == 1,
	      "StaleSeconds without coming nearer (a rock, a river): a new heading and marker");

	Run progress;
	progress.settings.staleSeconds = 10.0f;
	progress.settings.leadUnits = 3000.0f;
	const HordeWalker go[] = {Walker(1, 0, 0, 9)};
	progress.Step(go, 1);
	const UInt32 pg = progress.First(HordeOrder::Lead)->group;
	const float hx = std::cos(progress.state.groups[pg].heading);
	const float hy = std::sin(progress.state.groups[pg].heading);
	UInt32 restarts = 0;
	for (int t = 1; t <= 15; ++t) {
		const HordeWalker walking[] = {Walker(1, hx * 100.0f * t, hy * 100.0f * t, 9, kTravel + pg)};
		progress.Step(walking, 1, 1.0f);
		restarts += progress.Of(HordeOrder::MoveMarker);
	}
	Check(restarts == 0, "a leader that comes nearer every second is not stale, however long it walks");
}

void TestLeaderLost() {
	std::printf("A leader lost\n");
	Run run;
	const HordeWalker w[] = {Walker(1, 0, 0, 9), Walker(2, 300, 0, 4), Walker(3, 600, 0, 7)};
	run.Step(w, 3);
	const UInt32 g = run.First(HordeOrder::Lead)->group;
	const HordeWalker gone[] = {Walker(2, 300, 0, 4, kFollow + g), Walker(3, 600, 0, 7, kFollow + g)};
	run.Step(gone, 2);
	Check(run.Of(HordeOrder::Lead, 3) == 1 && run.state.groups[g].leader == 3,
	      "the leader gone (dead, unloaded): the follower of the highest rank leads the same group");
	Check(run.Of(HordeOrder::Follow, 2) == 1, "the rest are given the follow again - it now goes after the new leader");
	const HordeWalker alone[] = {Walker(2, 300, 0, 4, 0, false), Walker(3, 600, 0, 7, kTravel + g, false)};
	run.Step(alone, 2);
	Check(!run.state.groups[g].used, "no one fit left: the group's slot is free again");
}

void TestMerge() {
	std::printf("Groups that meet\n");
	Run run;
	const HordeWalker w[] = {Walker(1, 0, 0, 9), Walker(2, 4000, 0, 3), Walker(3, 4100, 0, 1)};
	run.Step(w, 3);
	const UInt32 big = run.state.groups[0].leader == 1 ? 0u : 1u;
	const UInt32 small = 1u - big;
	Check(run.state.groups[big].leader == 1 && run.state.groups[small].leader == 2, "two groups, far apart");
	const HordeWalker met[] = {Walker(1, 0, 0, 9, kTravel + big), Walker(2, 400, 0, 3, kTravel + small),
	                           Walker(3, 450, 0, 1, kFollow + small)};
	run.Step(met, 3);
	Check(!run.state.groups[small].used && run.Of(HordeOrder::Follow, 2) == 1 && run.Of(HordeOrder::Follow, 3) == 1,
	      "leaders within half JoinRadius: the lower rank's group follows the higher rank's");
}

void TestSlots() {
	std::printf("Group slots\n");
	Run run;
	HordeWalker many[walkers::kHordeGroups + 2];
	for (UInt32 i = 0; i < walkers::kHordeGroups + 2; ++i) {
		many[i] = Walker(i + 1, i * 10000.0f, 0, 100 - i);
	}
	run.Step(many, walkers::kHordeGroups + 2);
	Check(run.Of(HordeOrder::Lead) == walkers::kHordeGroups,
	      "more lone walkers than groups: as many lead as there are slots, the rest keep their own AI");
}

}  // namespace

int main() {
	TestOff();
	TestLeadAndJoin();
	TestKeepAlive();
	TestMarker();
	TestLeaderLost();
	TestMerge();
	TestSlots();
	if (g_failures != 0) {
		std::printf("%d failed\n", g_failures);
		return 1;
	}
	std::printf("all passed\n");
	return 0;
}
