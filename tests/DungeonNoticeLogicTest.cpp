// Every flow of where the refugees' notice hangs (docs/walkers-spec.md
// 4.20.7; Nadi, 2026-11-13: next to MOO's notice board, else at the city's
// gate): MOO's boards on or off, placed or not yet, closed or open cities,
// the spot on a board and the spot before a gate.

#include "DungeonNoticeLogic.h"

#include <cmath>
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

using namespace walkers;

void TestTable() {
	std::printf("MOO's boards\n");
	UInt32 codes = 0;
	bool keyed = true;
	for (UInt32 c = 0; c < kNoticeCities; ++c) {
		codes |= kNoticeBoards[c].code;
		keyed = keyed && kNoticeBoards[c].key != nullptr && kNoticeBoards[c].anchor != 0;
	}
	Check(kNoticeCities == static_cast<UInt32>(City::Count) && codes == 0xFF && keyed,
	      "a board for each of the eight cities, MOO's eight codes, each with its anchor");
	Check(std::strcmp(kNoticeBoards[static_cast<UInt32>(City::Anvil)].key, "anvil") == 0 &&
	          kNoticeBoards[static_cast<UInt32>(City::Anvil)].code == 1 &&
	          std::strcmp(kNoticeBoards[static_cast<UInt32>(City::ImperialCity)].key, "ic") == 0 &&
	          kNoticeBoards[static_cast<UInt32>(City::ImperialCity)].code == 32 &&
	          kNoticeBoards[static_cast<UInt32>(City::Leyawiin)].code == 64 &&
	          kNoticeBoards[static_cast<UInt32>(City::Skingrad)].code == 128,
	      "in CityGates.h's order: Anvil 1 ... Leyawiin 64, Skingrad 128, the Imperial City 32");
	char name[48];
	Check(MooBoardVariable(name, sizeof(name), static_cast<UInt32>(City::Bruma), 'x') &&
	          std::strcmp(name, "ini_noticeboard_bruma_xpos") == 0 &&
	          MooBoardVariable(name, sizeof(name), static_cast<UInt32>(City::ImperialCity), 'z') &&
	          std::strcmp(name, "ini_noticeboard_ic_zpos") == 0,
	      "MOO's position variables: ini_noticeboard_<city>_xpos, _ypos, _zpos");
	Check(MooBoardSwitch(name, sizeof(name), static_cast<UInt32>(City::Chorrol)) &&
	          std::strcmp(name, "ini_add_noticeboards_chorrol") == 0,
	      "and its switch for a city: ini_add_noticeboards_<city>");
	Check(!MooBoardVariable(name, 8, 0, 'x') && !MooBoardVariable(name, sizeof(name), kNoticeCities, 'x') &&
	          !MooBoardSwitch(name, sizeof(name), kNoticeCities),
	      "too short a buffer, a city past the list: none");
}

void TestWhere() {
	std::printf("where the notice hangs\n");
	const UInt32 anvil = static_cast<UInt32>(City::Anvil);
	const UInt32 ic = static_cast<UInt32>(City::ImperialCity);
	MooBoards none;
	Check(NoticeWhere(none, true, anvil, 0.0f) == NoticeAt::Gate, "MOO not loaded: at the gate");
	MooBoards moo;
	moo.loaded = true;
	moo.boards = true;
	moo.worldspace = 1;
	Check(NoticeWhere(moo, true, anvil, 0.0f) == NoticeAt::Wait, "MOO's board not placed yet: wait");
	Check(NoticeWhere(moo, true, anvil, kNoticeWaitSeconds) == NoticeAt::Gate,
	      "still not placed after a minute: at the gate");
	moo.placed = 1;
	Check(NoticeWhere(moo, true, anvil, 0.0f) == NoticeAt::Board, "its board placed: on the board");
	Check(NoticeWhere(moo, true, static_cast<UInt32>(City::Bravil), 0.0f) == NoticeAt::Wait,
	      "another city's board is not this one's");
	Check(NoticeWhere(moo, false, anvil, 0.0f) == NoticeAt::Wait &&
	          NoticeWhere(moo, false, anvil, kNoticeWaitSeconds) == NoticeAt::Gate,
	      "MOO's board for this city off: at the gate - but only after the minute (right after a load MOO's "
	      "variables still read 0: measured 2026-11-13, all eight went to the gates a second in)");
	moo.boards = false;
	Check(NoticeWhere(moo, true, anvil, 0.0f) == NoticeAt::Wait &&
	          NoticeWhere(moo, true, anvil, kNoticeWaitSeconds) == NoticeAt::Gate,
	      "MOO's boards off altogether: the same");
	moo.boards = true;
	moo.worldspace = 2;
	moo.placed = 0;
	moo.placedOpen = 1;
	Check(NoticeWhere(moo, true, anvil, 0.0f) == NoticeAt::BoardOpen, "open cities: the board in Tamriel");
	Check(NoticeWhere(moo, true, ic, 0.0f) == NoticeAt::Wait, "the Imperial City is always closed: its own bit");
	moo.placed = 32;
	Check(NoticeWhere(moo, true, ic, 0.0f) == NoticeAt::Board, "placed: on its board");
	moo.worldspace = 3;
	moo.placed = 1;
	Check(NoticeWhere(moo, true, anvil, 0.0f) == NoticeAt::Board, "both closed and open: the closed city's board");
	moo.worldspace = 0;
	Check(NoticeWhere(moo, true, anvil, 0.0f) == NoticeAt::Wait &&
	          NoticeWhere(moo, true, anvil, kNoticeWaitSeconds) == NoticeAt::Gate,
	      "neither: at the gate after the minute");
	Check(NoticeWhere(moo, true, kNoticeCities, 0.0f) == NoticeAt::Gate, "a city past the list: the gate's answer");
}

void TestSpots() {
	std::printf("the spot\n");
	const UInt32 anvil = static_cast<UInt32>(City::Anvil);
	const NoticeSpot onBoard = NoticeOnBoard(anvil, false, -1000.0f, 2000.0f, 300.0f);
	Check(onBoard.anchor == kNoticeBoards[anvil].anchor && std::fabs(onBoard.x - (-1000.0f + 20.0f)) < 0.01f &&
	          std::fabs(onBoard.y - (2000.0f + 25.5f)) < 0.01f && std::fabs(onBoard.z - (300.0f + kNoticeBoardLift)) < 0.01f,
	      "on Anvil's board: MOO's way - the board's place less the offset, between its second and third notice, "
	      "at the notices' height");
	Check(onBoard.ax == 90.0f && onBoard.ay == 237.7f && onBoard.az == 180.0f,
	      "turned as MOO's notices: 90 about x, the city's angle about y, 180 about z");
	const NoticeSpot open = NoticeOnBoard(anvil, true, -1000.0f, 2000.0f, 300.0f);
	Check(open.anchor == kMooOpenAnchor && open.x == onBoard.x && open.ay == onBoard.ay,
	      "open cities: the same spot, MOO's anchor in Tamriel");
	const CityGate gate{City::Bravil, 0x00005605, 1.0f, 2.0f, 3.0f, 66301.9f, -31526.4f, 633.6f, 0x00005603, 0, 0, 0, "Bravil"};
	const NoticeSpot atGate = NoticeAtGate(gate);
	Check(atGate.anchor == 0x00005605 && atGate.x == 66301.9f && atGate.y == -31526.4f &&
	          std::fabs(atGate.z - (633.6f + kNoticeGateLift)) < 0.01f && atGate.ax == 0.0f && atGate.ay == 0.0f,
	      "at a gate: where one comes out of the city, lying on the ground");
	Check(NoticeMainGate(static_cast<UInt32>(City::Anvil)) == 0 && NoticeMainGate(static_cast<UInt32>(City::Bravil)) == 3 &&
	          NoticeMainGate(static_cast<UInt32>(City::ImperialCity)) >= 0 && NoticeMainGate(kNoticeCities) == -1,
	      "a city's gate: its first in CityGates.h; none for a city past the list");
	float dx = 0.0f;
	float dy = 0.0f;
	float dz = 0.0f;
	Check(NoticeNeedsMove(onBoard, onBoard.x + 0.4f, onBoard.y, onBoard.z, false) == false &&
	          NoticeNeedsMove(onBoard, onBoard.x + 3.0f, onBoard.y, onBoard.z, false) &&
	          NoticeNeedsMove(onBoard, onBoard.x, onBoard.y, onBoard.z, true),
	      "there already (within a unit, enabled): left alone; elsewhere or disabled: moved");
	NoticeOffset(onBoard, -900.0f, 1900.0f, 250.0f, dx, dy, dz);
	Check(std::fabs(dx - (onBoard.x + 900.0f)) < 0.01f && std::fabs(dy - (onBoard.y - 1900.0f)) < 0.01f &&
	          std::fabs(dz - (onBoard.z - 250.0f)) < 0.01f,
	      "moved by its offset from the anchor");
}

}  // namespace

int main() {
	TestTable();
	TestWhere();
	TestSpots();
	std::printf(g_failures == 0 ? "All passed\n" : "%d FAILED\n", g_failures);
	return g_failures == 0 ? 0 : 1;
}
