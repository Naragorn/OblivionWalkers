// Every flow of "Close the gate" (docs/walkers-spec.md 4.22a step 6): when
// the rumor is said, the quest begun at the nearest open gate, done when it
// shuts, the way told in words, the co-save.

#include "GateQuestLogic.h"

#include <cstdio>
#include <cstring>

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf("  %s  %s\n", condition ? "ok  " : "FAIL", what);
	if (!condition) {
		++g_failures;
	}
}

using walkers::GateQuest;
using walkers::GateQuestAction;
using walkers::GateQuestLook;

void TestRumor() {
	std::printf("The rumor\n");
	GateQuest q;
	GateQuestLook look;
	look.daedraInPlay = true;
	look.nearestOpen = 4;
	Check(walkers::GateRumorWanted(q, look), "Daedra in play, a gate open: said");
	look.daedraInPlay = false;
	Check(!walkers::GateRumorWanted(q, look), "Mode 0 or stopped: not");
	look.daedraInPlay = true;
	look.nearestOpen = -1;
	Check(!walkers::GateRumorWanted(q, look), "no gate open: not");
	look.nearestOpen = 4;
	q.gate = 2;
	Check(!walkers::GateRumorWanted(q, look), "a quest running: not");
}

void TestQuest() {
	std::printf("The quest\n");
	GateQuest q;
	GateQuestLook look;
	look.daedraInPlay = true;
	look.nearestOpen = 7;
	Check(walkers::StepGateQuest(q, look) == GateQuestAction::None && q.gate < 0, "not heard: nothing");
	look.heard = true;
	Check(walkers::StepGateQuest(q, look) == GateQuestAction::Start && q.gate == 7 && q.stage == walkers::kGateQuestTold,
	      "heard: begun at the nearest open gate, told");
	look.gateOpen = true;
	look.nearestOpen = 3;
	Check(walkers::StepGateQuest(q, look) == GateQuestAction::None && q.gate == 7, "its gate open: it runs on, the same gate");
	look.gateOpen = false;
	Check(walkers::StepGateQuest(q, look) == GateQuestAction::Done && q.gate < 0 && q.stage == walkers::kGateQuestDone,
	      "its gate shut: done");
	look.heard = false;
	Check(walkers::StepGateQuest(q, look) == GateQuestAction::None, "after it, not heard again: nothing");
	look.heard = true;
	Check(walkers::StepGateQuest(q, look) == GateQuestAction::Start && q.gate == 3, "heard again: the next gate");
	GateQuest none;
	GateQuestLook stopped;
	stopped.heard = true;
	stopped.nearestOpen = 1;
	Check(walkers::StepGateQuest(none, stopped) == GateQuestAction::None, "heard with the Daedra stopped: not begun");
}

void TestNearest() {
	std::printf("The nearest open gate\n");
	const float xs[3] = {0.0f, 100.0f, 50.0f};
	const float ys[3] = {0.0f, 0.0f, 0.0f};
	const bool open[3] = {true, true, false};
	Check(walkers::NearestOpenGate(xs, ys, open, 3, 60.0f, 0.0f) == 1, "the nearer of the open ones");
	const bool none[3] = {false, false, false};
	Check(walkers::NearestOpenGate(xs, ys, none, 3, 0.0f, 0.0f) == -1, "none open: -1");
}

void TestCompass() {
	std::printf("The way in words\n");
	Check(std::strcmp(walkers::CompassWord(0.0f, 10.0f), "north") == 0, "north");
	Check(std::strcmp(walkers::CompassWord(10.0f, 10.0f), "north-east") == 0, "north-east");
	Check(std::strcmp(walkers::CompassWord(10.0f, 0.0f), "east") == 0, "east");
	Check(std::strcmp(walkers::CompassWord(0.0f, -10.0f), "south") == 0, "south");
	Check(std::strcmp(walkers::CompassWord(-10.0f, -10.0f), "south-west") == 0, "south-west");
	Check(std::strcmp(walkers::CompassWord(-10.0f, 1.0f), "west") == 0, "west, a little north still west");
	Check(std::strcmp(walkers::CompassWord(-10.0f, 10.0f), "north-west") == 0, "north-west");
	Check(std::strcmp(walkers::CompassWord(0.0f, 0.0f), "at") == 0, "the same place: at");
}

void TestImage() {
	std::printf("The co-save\n");
	GateQuest q;
	q.gate = 42;
	q.stage = walkers::kGateQuestTold;
	UInt8 image[walkers::kGateQuestImageBytes];
	Check(walkers::GateQuestImage(q, image, sizeof(image)) == walkers::kGateQuestImageBytes, "written");
	GateQuest back;
	Check(walkers::ReadGateQuestImage(back, image, sizeof(image), 103) && back.gate == 42 && back.stage == 10,
	      "read back");
	Check(walkers::ReadGateQuestImage(back, image, sizeof(image), 40) && back.gate == -1, "a gate past the list: none");
	Check(!walkers::ReadGateQuestImage(back, image, 3, 103) && back.gate == -1, "too short: none");
	GateQuest idle;
	walkers::GateQuestImage(idle, image, sizeof(image));
	walkers::ReadGateQuestImage(back, image, sizeof(image), 103);
	Check(back.gate == -1, "none running: none");
}

}  // namespace

int main() {
	TestRumor();
	TestQuest();
	TestNearest();
	TestCompass();
	TestImage();
	std::printf(g_failures == 0 ? "PASS\n" : "FAIL (%d)\n", g_failures);
	return g_failures == 0 ? 0 : 1;
}
