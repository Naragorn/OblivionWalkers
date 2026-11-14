// Every flow of the console lines Walkers carries out by the engine's own
// calls instead of a script compiled and run (ConsoleLogic.h): which lines
// are read, what they hold, and which are left to the console.

#include "ConsoleLogic.h"

#include <cstdio>

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf("  %s  %s\n", condition ? "ok  " : "FAIL", what);
	if (!condition) {
		++g_failures;
	}
}

using walkers::DirectLine;
using walkers::DirectVerb;
using walkers::ParseDirectLine;

void TestMoveTo() {
	std::printf("MoveTo\n");
	DirectLine d;
	Check(ParseDirectLine("MoveTo 0001A2B3 120 -40 15", d) && d.verb == DirectVerb::MoveTo && d.target == 0x0001A2B3 &&
	          !d.targetPlayer && d.x == 120.0f && d.y == -40.0f && d.z == 15.0f,
	      "a reference by its form ID and three whole offsets (the dungeons' and the camps' lines)");
	Check(ParseDirectLine("MoveTo FF000ABC 12.5 -3.0 0.4", d) && d.target == 0xFF000ABC && d.x == 12.5f && d.y == -3.0f &&
	          d.z == 0.4f,
	      "a made reference (FF...) and offsets with a fraction (the horde markers' %.1f)");
	Check(ParseDirectLine("MoveTo player", d) && d.verb == DirectVerb::MoveTo && d.targetPlayer && d.x == 0.0f &&
	          d.y == 0.0f && d.z == 0.0f,
	      "the player without offsets: all three 0, as the console's optional floats");
	Check(ParseDirectLine("MoveTo player 0 150 0", d) && d.targetPlayer && d.y == 150.0f, "the player with offsets");
	Check(ParseDirectLine("MoveTo 00012345 5", d) && d.x == 5.0f && d.y == 0.0f && d.z == 0.0f &&
	          ParseDirectLine("MoveTo 00012345 5 6", d) && d.y == 6.0f && d.z == 0.0f,
	      "one or two offsets: the rest 0, as the console takes them");
	Check(ParseDirectLine("MoveTo 0x00012345 1 2 3", d) && d.target == 0x00012345,
	      "a 0x before the ID: read as the console reads it");
	Check(ParseDirectLine("moveto PLAYER 1 2 3", d) && d.targetPlayer, "the verb and player in any case");
	Check(!ParseDirectLine("MoveTo 0001A2B3 1 2 3 4", d), "a fourth number: left to the console (it would refuse)");
	Check(!ParseDirectLine("MoveTo 0001A2B3 1 x 3", d) && !ParseDirectLine("MoveTo 0001A2B3 1e", d),
	      "a word that is no number: left to the console");
	Check(!ParseDirectLine("MoveTo FortAshDoorRef 1 2 3", d) && !ParseDirectLine("MoveTo 1234567890 0 0 0", d),
	      "an editor ID, or more than eight digits: left to the console (it looks names up)");
	Check(!ParseDirectLine("MoveTo 00000000 1 2 3", d), "form ID 0: left to the console");
	Check(!ParseDirectLine("MoveTo", d) && !ParseDirectLine("MoveTo ", d), "no target: left to the console");
	Check(!ParseDirectLine("MoveToMarker 0001A2B3", d) && !ParseDirectLine("MoveTox 0001A2B3", d),
	      "the long name, or a verb that only begins so: left to the console");
}

void TestModAV() {
	std::printf("ModAV\n");
	DirectLine d;
	Check(ParseDirectLine("ModAV Aggression 100", d) && d.verb == DirectVerb::ModAV && d.av == walkers::kAvAggression &&
	          d.amount == 100,
	      "a walker roused: Aggression up by 100 (xOBSE kActorVal_Aggression 33)");
	Check(ParseDirectLine("ModAV Aggression -100", d) && d.amount == -100, "calmed down: the same down");
	Check(ParseDirectLine("modav aggression 5", d) && d.amount == 5, "the verb and the value in any case");
	Check(ParseDirectLine("ModActorValue Aggression 7", d) && d.amount == 7, "the long name too");
	Check(!ParseDirectLine("ModAV Health 5", d) && !ParseDirectLine("ModAV Confidence 5", d),
	      "another actor value: left to the console (only Aggression is mapped)");
	Check(!ParseDirectLine("ModAV Aggression 2.5", d) && !ParseDirectLine("ModAV Aggression", d) &&
	          !ParseDirectLine("ModAV Aggression 5 6", d),
	      "a fraction (the command's amount is whole), none, or two: left to the console");
	Check(!ParseDirectLine("ModAV2 Aggression 5", d) && !ParseDirectLine("SetAV Aggression 0", d),
	      "ModAV2 and SetAV: left to the console (other calls)");
}

void TestOthers() {
	std::printf("Other lines\n");
	DirectLine d;
	Check(!ParseDirectLine("StartCombat player", d) && !ParseDirectLine("AddScriptPackage 01000827", d) &&
	          !ParseDirectLine("Disable", d) && !ParseDirectLine("", d) && !ParseDirectLine(nullptr, d),
	      "every other line, an empty one, none: left to the console");
	Check(!ParseDirectLine("  MoveTo player", d), "a line that does not begin with the verb: left to the console");
}

}  // namespace

int main() {
	TestMoveTo();
	TestModAV();
	TestOthers();
	std::printf(g_failures == 0 ? "All passed\n" : "%d FAILED\n", g_failures);
	return g_failures == 0 ? 0 : 1;
}
