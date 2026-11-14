#pragma once

// The console lines Walkers carries out by the engine's own calls, process-free
// (Nadi, 2026-11-13: "baust du walkers extrem performant"). A console line is
// a script compiled and run; a cheap one (Disable) costs some 20 us so. The
// engine's MoveTo handler (0x00512870, command 0x109E) only reads its
// arguments and hands them to cdecl 0x00512540(this, target, x, y, z);
// ModActorValue's (0x005017E0, 0x1010) calls the actor's vtable +0x2A8 or
// +0x2A0 (av, amount, 0) - Engine.h DirectMoveTo / DirectModAV. Measured
// 2026-11-14 (harness fight, StepTiming, ten-second periods): MoveTo 3 us a
// line in one period (8 lines), 126 and 280 us in the periods the pool was
// placed into the dungeon (the console's 2026-11-13/14: 206 us a line) - the
// move itself, not the console; ModAV 2 us a line (the console's ~80 us). A line read
// here goes so; every other line still goes to the console. StartCombat
// (0x00514660) and AddScriptPackage (0x005123A0) stay console lines: their
// handlers do far more after reading their arguments (crime, alarm, the
// process's package and the high process list), nothing to call on its own.

#include "Types.h"

namespace walkers {

inline constexpr UInt32 kAvAggression = 33;  // xOBSE kActorVal_Aggression

enum class DirectVerb : UInt8 { None, MoveTo, ModAV };

struct DirectLine {
	DirectVerb verb = DirectVerb::None;
	UInt32 target = 0;          // MoveTo: the form ID moved to (0 with targetPlayer)
	bool targetPlayer = false;  // MoveTo player
	float x = 0.0f;             // MoveTo: the offsets from the target, 0 when not given
	float y = 0.0f;
	float z = 0.0f;
	UInt32 av = 0;  // ModAV: the actor value
	int amount = 0;
};

namespace console_detail {

inline char Lower(char c) { return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c; }

// Whether the word [begin, end) is `word`, case aside.
inline bool WordIs(const char* begin, const char* end, const char* word) {
	for (; begin < end && *word != '\0'; ++begin, ++word) {
		if (Lower(*begin) != *word) {
			return false;
		}
	}
	return begin == end && *word == '\0';
}

// The line cut into up to `most` words at spaces; false when there are more.
inline bool Words(const char* line, const char** begins, const char** ends, UInt32 most, UInt32& count) {
	count = 0;
	const char* p = line;
	while (*p != '\0') {
		while (*p == ' ') {
			++p;
		}
		if (*p == '\0') {
			break;
		}
		if (count == most) {
			return false;
		}
		begins[count] = p;
		while (*p != '\0' && *p != ' ') {
			++p;
		}
		ends[count++] = p;
	}
	return true;
}

// A number as the console's scripts write it: a sign, digits, a point and
// digits. `whole` refuses the point.
inline bool Number(const char* begin, const char* end, bool whole, double& out) {
	const char* p = begin;
	bool negative = false;
	if (p < end && (*p == '-' || *p == '+')) {
		negative = *p == '-';
		++p;
	}
	double value = 0.0;
	UInt32 digits = 0;
	for (; p < end && *p >= '0' && *p <= '9'; ++p, ++digits) {
		value = value * 10.0 + (*p - '0');
	}
	if (p < end && *p == '.' && !whole) {
		double scale = 0.1;
		for (++p; p < end && *p >= '0' && *p <= '9'; ++p, ++digits, scale *= 0.1) {
			value += (*p - '0') * scale;
		}
	}
	if (p != end || digits == 0) {
		return false;
	}
	out = negative ? -value : value;
	return true;
}

// A form ID as the console takes one: up to eight hex digits, 0x allowed.
inline bool FormId(const char* begin, const char* end, UInt32& out) {
	if (end - begin > 2 && begin[0] == '0' && Lower(begin[1]) == 'x') {
		begin += 2;
	}
	if (begin == end || end - begin > 8) {
		return false;
	}
	UInt32 value = 0;
	for (const char* p = begin; p < end; ++p) {
		const char c = Lower(*p);
		const UInt32 digit = c >= '0' && c <= '9' ? static_cast<UInt32>(c - '0')
		                     : c >= 'a' && c <= 'f' ? static_cast<UInt32>(c - 'a' + 10)
		                                            : 16u;
		if (digit == 16u) {
			return false;
		}
		value = value * 16u + digit;
	}
	out = value;
	return value != 0;
}

}  // namespace console_detail

// Whether `line` is one the engine's calls carry out, and what it holds.
inline bool ParseDirectLine(const char* line, DirectLine& out) {
	using namespace console_detail;
	out = DirectLine{};
	if (line == nullptr || *line == ' ') {
		return false;
	}
	const char* begins[5];
	const char* ends[5];
	UInt32 count = 0;
	if (!Words(line, begins, ends, 5, count) || count < 2) {
		return false;
	}
	if (WordIs(begins[0], ends[0], "moveto")) {
		DirectLine d;
		d.verb = DirectVerb::MoveTo;
		d.targetPlayer = WordIs(begins[1], ends[1], "player");
		if (!d.targetPlayer && !FormId(begins[1], ends[1], d.target)) {
			return false;
		}
		float* const offsets[3] = {&d.x, &d.y, &d.z};
		for (UInt32 k = 2; k < count; ++k) {
			double v = 0.0;
			if (!Number(begins[k], ends[k], false, v)) {
				return false;
			}
			*offsets[k - 2] = static_cast<float>(v);
		}
		out = d;
		return true;
	}
	if (WordIs(begins[0], ends[0], "modav") || WordIs(begins[0], ends[0], "modactorvalue")) {
		double v = 0.0;
		if (count != 3 || !WordIs(begins[1], ends[1], "aggression") || !Number(begins[2], ends[2], true, v)) {
			return false;
		}
		out.verb = DirectVerb::ModAV;
		out.av = kAvAggression;
		out.amount = static_cast<int>(v);
		return true;
	}
	return false;
}

}  // namespace walkers
