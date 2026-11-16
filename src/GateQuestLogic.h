#pragma once

// "Close the gate", process-free (docs/walkers-spec.md 4.22a step 6; Nadi,
// 2026-11-16: "6: passt, aber gerüchte für alle"). While Daedra are in play
// and an Oblivion gate stands open, everyone with rumors tells of the gate
// the Daedra march out of (the Rumors topic, any person). Heard, the
// journal quest WalkersCloseTheGate begins: its target the open gate
// nearest the player (the quest's one target rewritten at run time -
// journal text is read off the file, a quest per gate would take 103 IDs),
// where it lies said in words (north-east of Chorrol). Done when that gate
// no longer reads open (its door destroyed, GateLogic.h): the camps'
// reward. Then it may be heard of again for the next gate.

#include <cmath>

#include "Bytes.h"
#include "Types.h"

namespace walkers {

inline constexpr UInt32 kGateRumorGlobalLocalId = 0xA98;  // 1 while the rumor is said
inline constexpr UInt32 kGateHeardGlobalLocalId = 0xA99;  // the rumor's result script sets it 1
inline constexpr UInt32 kGateRumorLocalId = 0xA9A;        // the INFO in the Rumors topic
inline constexpr UInt32 kGateQuestLocalId = 0xA9B;        // WalkersCloseTheGate
inline constexpr UInt8 kGateQuestTold = 10;
inline constexpr UInt8 kGateQuestDone = 20;

struct GateQuest {
	int gate = -1;     // the gate the quest names, -1 none running
	UInt8 stage = 0;   // the stage set last: 0, told, done
};

enum class GateQuestAction : UInt8 { None, Start, Done };

struct GateQuestLook {
	bool daedraInPlay = false;  // [Invaders] Mode 1-3, not stopped
	bool heard = false;         // the rumor's global set
	bool gateOpen = false;      // the quest's gate still open
	int nearestOpen = -1;       // the open gate nearest the player, -1 none
};

// Whether the rumor is said now: Daedra in play, a gate open, no quest
// running.
inline bool GateRumorWanted(const GateQuest& q, const GateQuestLook& look) {
	return q.gate < 0 && look.daedraInPlay && look.nearestOpen >= 0;
}

// One look: a running quest done once its gate is shut; none running, the
// rumor heard and a gate open - begun at the nearest.
inline GateQuestAction StepGateQuest(GateQuest& q, const GateQuestLook& look) {
	if (q.gate >= 0) {
		if (look.gateOpen) {
			return GateQuestAction::None;
		}
		q.gate = -1;
		q.stage = kGateQuestDone;
		return GateQuestAction::Done;
	}
	if (look.heard && GateRumorWanted(q, look)) {
		q.gate = look.nearestOpen;
		q.stage = kGateQuestTold;
		return GateQuestAction::Start;
	}
	return GateQuestAction::None;
}

// Of `count` gates at x, y with `open` set, the one nearest px, py; -1 none.
inline int NearestOpenGate(const float* xs, const float* ys, const bool* open, UInt32 count, float px, float py) {
	int best = -1;
	float bestD = 0.0f;
	for (UInt32 i = 0; i < count; ++i) {
		if (!open[i]) {
			continue;
		}
		const float d = (xs[i] - px) * (xs[i] - px) + (ys[i] - py) * (ys[i] - py);
		if (best < 0 || d < bestD) {
			best = static_cast<int>(i);
			bestD = d;
		}
	}
	return best;
}

// The compass word for the way from a place to another (+y north, +x east).
inline const char* CompassWord(float dx, float dy) {
	if (dx * dx + dy * dy < 1.0f) {
		return "at";
	}
	static const char* const kWords[8] = {"north", "north-east", "east", "south-east",
	                                      "south", "south-west", "west", "north-west"};
	float a = std::atan2(dx, dy) * 57.2957795f;  // 0 north, 90 east
	a = a < 0.0f ? a + 360.0f : a;
	const UInt32 sector = static_cast<UInt32>((a + 22.5f) / 45.0f) % 8;
	return kWords[sector];
}

// The co-save record 'GATQ' v1: the gate (as a signed 32-bit), the stage.
inline constexpr UInt32 kGateQuestRecordType = 0x47415451;  // 'GATQ'
inline constexpr UInt32 kGateQuestRecordVersion = 1;
inline constexpr UInt32 kGateQuestImageBytes = 8;

inline UInt32 GateQuestImage(const GateQuest& q, UInt8* out, UInt32 capacity) {
	if (capacity < kGateQuestImageBytes) {
		return 0;
	}
	Put32(out, static_cast<UInt32>(q.gate));
	Put32(out + 4, q.stage);
	return kGateQuestImageBytes;
}

// `gates`: the gates known now - a gate past them (another build's list)
// reads as none.
inline bool ReadGateQuestImage(GateQuest& q, const UInt8* in, UInt32 length, UInt32 gates) {
	q = GateQuest{};
	if (length < kGateQuestImageBytes) {
		return false;
	}
	const int gate = static_cast<int>(Get32(in));
	q.gate = gate >= 0 && static_cast<UInt32>(gate) < gates ? gate : -1;
	q.stage = static_cast<UInt8>(Get32(in + 4));
	return true;
}

}  // namespace walkers
