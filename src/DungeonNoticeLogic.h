#pragma once

// Where the refugees' notice hangs, process-free (docs/walkers-spec.md
// 4.20.7; Nadi, 2026-11-13: next to MOO's notice board, else at the city's
// gate). One notice a city - a paper as the Courier's, not to be taken;
// reading it tells of a held dungeon (DungeonQuestLogic.h PaperOpened).
//
// MOO's boards (read off Maskar's Oblivion Overhaul.esp's scripts,
// 2026-11-13): MOOPlaceNoticeBoardsFunctionScript puts a board in each city
// at MOO.ini_noticeboard_<city>_xpos/_ypos/_zpos (its placer moved to a
// marker of the city first, then PlaceAtMe - the board is a new reference
// each game), when MOO.ini_add_noticeboards and ini_add_noticeboards_<city>
// are on; MOO.ini_noticeboard_worldspace says where (1 the closed cities, 2
// open cities - Tamriel, by DrakeloweMapMarker; the Imperial City always
// closed), and MOO.placednoticeboards / placednoticeboardsopen keep a bit a
// board placed (Anvil 1, Bravil 2, Bruma 4, Cheydinhal 8, Chorrol 16, the
// Imperial City 32, Leyawiin 64, Skingrad 128). Its three notices hang at
// the board's place LESS an offset a notice and city
// (MOOPlaceNoticeFunctionScript), 25 to 50 units above the board's z, turned
// 90 about x, an angle of the city's about y, 175 to 185 about z
// (MOOPlaceNoticeToMarkerFunctionScript). Ours hangs between the board's
// middle notice and one beside it, at the middle of that height.

#include <cmath>
#include <cstdio>

#include "CityGates.h"
#include "Types.h"

namespace walkers {

inline constexpr UInt32 kNoticeCities = 8;  // CityGates.h's City order
inline constexpr UInt32 kMooQuestLocalId = 0x000ED3;       // MOO.esp's quest "MOO"
inline constexpr UInt32 kMooOpenAnchor = 0x000177B1;       // DrakeloweMapMarker (Tamriel)
inline constexpr float kNoticeBoardLift = 37.5f;           // the middle of MOO's 25..50
inline constexpr float kNoticeGateLift = 2.0f;
inline constexpr float kNoticeWaitSeconds = 60.0f;

struct NoticeBoard {
	const char* key;  // MOO's name of the city in its variables
	UInt32 code;      // its bit in MOO.placednoticeboards
	UInt32 anchor;    // the marker MOO's placer is moved to (Oblivion.esm)
	float dx;         // ours hangs at the board's place less this
	float dy;
	float yAngle;     // MOO's angle about y for this board's notices
};

inline constexpr NoticeBoard kNoticeBoards[kNoticeCities] = {
    {"anvil", 1, 0x0000BFD3, -20.0f, -25.5f, 237.7f},      // AnvilLandGateMarker; MOO's notices at 26,51 / -4,1 / -36,-52
    {"bravil", 2, 0x00022D61, -25.5f, 5.0f, 337.6f},       // BravilBridgeEast; 62,-28.5 / 7,-7.5 / -58,17.5
    {"bruma", 4, 0x0006E60B, -13.5f, -29.5f, 250.5f},      // BrumaCenter; 16,61 / -4,0 / -23,-59
    {"cheydinhal", 8, 0x0006E6A0, 27.0f, -10.5f, 150.1f},  // CheydinhalMain; -49,31 / 2,3 / 52,-24
    {"chorrol", 16, 0x000231ED, -10.5f, -17.0f, 250.5f},   // ChorrolGreatOakMarker; 15.5,63 / -1.5,13 / -19.5,-47
    {"leyawiin", 64, 0x0002009B, -31.0f, 0.0f, 186.6f},    // LeyawiinMapMarker; 63,13 / -1,4 / -61,-4
    {"skingrad", 128, 0x00029386, 7.0f, -29.0f, 290.0f},   // SkingradChapelStepsMarker; -30,63 / -7,6 / 21,-64
    {"ic", 32, 0x0001C1E8, 22.2f, -6.55f, 154.5f},         // ICMarketMainGateMarker; -57,31.3 / -7,7.4 / 51.4,-20.5
};

// "ini_noticeboard_<city>_<axis>pos"; false when it does not fit.
inline bool MooBoardVariable(char* out, UInt32 size, UInt32 city, char axis) {
	if (city >= kNoticeCities || size < 40) {
		return false;
	}
	std::snprintf(out, size, "ini_noticeboard_%s_%cpos", kNoticeBoards[city].key, axis);
	return true;
}

// "ini_add_noticeboards_<city>".
inline bool MooBoardSwitch(char* out, UInt32 size, UInt32 city) {
	if (city >= kNoticeCities || size < 40) {
		return false;
	}
	std::snprintf(out, size, "ini_add_noticeboards_%s", kNoticeBoards[city].key);
	return true;
}

// What MOO says of its boards (its quest's variables), as read this second.
struct MooBoards {
	bool loaded = false;    // MOO's quest is there
	bool boards = false;    // ini_add_noticeboards
	UInt32 worldspace = 0;  // ini_noticeboard_worldspace
	UInt32 placed = 0;      // placednoticeboards
	UInt32 placedOpen = 0;  // placednoticeboardsopen
};

enum class NoticeAt : UInt8 { Wait, Board, BoardOpen, Gate };

// Where city `city`'s notice goes: on MOO's board once MOO has placed it
// (`cityOn`: its switch for the city), at the gate without one - at once
// when MOO is not loaded, else when no board of the city is there `waited`
// seconds after the load: MOO's variables read 0 until its own start has
// set them (measured 2026-11-13: all 0 a second after the load, its INI
// saying boards on), so "off" is believed only after the wait.
inline NoticeAt NoticeWhere(const MooBoards& m, bool cityOn, UInt32 city, float waited) {
	if (city >= kNoticeCities || !m.loaded) {
		return NoticeAt::Gate;
	}
	const NoticeAt none = waited >= kNoticeWaitSeconds ? NoticeAt::Gate : NoticeAt::Wait;
	if (!m.boards || !cityOn) {
		return none;
	}
	const UInt32 code = kNoticeBoards[city].code;
	const bool closed = (m.worldspace & 1) != 0 || code == 32;  // the Imperial City: always closed
	const bool open = !closed && (m.worldspace & 2) != 0;
	if (closed && (m.placed & code) != 0) {
		return NoticeAt::Board;
	}
	if (open && (m.placedOpen & code) != 0) {
		return NoticeAt::BoardOpen;
	}
	return none;
}

struct NoticeSpot {
	UInt32 anchor;  // a reference of the worldspace it goes into
	float x;
	float y;
	float z;
	float ax;  // degrees
	float ay;
	float az;
};

// On city `city`'s board standing at bx, by, bz.
inline NoticeSpot NoticeOnBoard(UInt32 city, bool open, float bx, float by, float bz) {
	const NoticeBoard& b = kNoticeBoards[city < kNoticeCities ? city : 0];
	return NoticeSpot{open ? kMooOpenAnchor : b.anchor, bx - b.dx, by - b.dy, bz + kNoticeBoardLift, 90.0f, b.yAngle, 180.0f};
}

// Before a gate: where one comes out of the city, lying on the ground.
inline NoticeSpot NoticeAtGate(const CityGate& gate) {
	return NoticeSpot{gate.door, gate.outX, gate.outY, gate.outZ + kNoticeGateLift, 0.0f, 0.0f, 0.0f};
}

// A city's gate for its notice: its first in CityGates.h; -1 none.
inline int NoticeMainGate(UInt32 city) {
	for (UInt32 g = 0; city < kNoticeCities && g < kCityGateCount; ++g) {
		if (static_cast<UInt32>(kCityGates[g].city) == city) {
			return static_cast<int>(g);
		}
	}
	return -1;
}

// Whether the notice, now at x, y, z, has to be moved to its spot.
inline bool NoticeNeedsMove(const NoticeSpot& spot, float x, float y, float z, bool disabled) {
	return disabled || std::fabs(x - spot.x) > 1.0f || std::fabs(y - spot.y) > 1.0f || std::fabs(z - spot.z) > 1.0f;
}

// Its offset from the anchor standing at ax, ay, az (the console's MoveTo
// takes one).
inline void NoticeOffset(const NoticeSpot& spot, float ax, float ay, float az, float& dx, float& dy, float& dz) {
	dx = spot.x - ax;
	dy = spot.y - ay;
	dz = spot.z - az;
}

}  // namespace walkers
