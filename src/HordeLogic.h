#pragma once

// The horde's bookkeeping, process-free (docs/walkers-spec.md 4.11; Nadi,
// 2026-10-26: "go horde"). Walkers out of combat do not stand around: one of
// them leads, walking to a marker the plugin keeps ahead of it, and those
// near it follow; the marker moves on when it is reached or the leader stops
// coming nearer, so the group drifts through the land.
//
// - The packages are the ESP's (tools/esp-write.py): group NN's leader runs
//   WalkersHordeTravelNN (travel to WalkersHordeMarkerNN, must reach it),
//   its followers WalkersHordeFollowNN (follow, must complete), whose target
//   the plugin sets to the leader. Group NN's packages are travelBase + NN
//   and followBase + NN.
// - The spec's "a walker that sees another with a higher random rank follows
//   it" is taken by distance, not by sight: a lone walker joins the nearest
//   leader within JoinRadius (the walkers are handed out highest rank first,
//   so of two lone walkers near each other the higher leads). Groups whose
//   leaders come within half JoinRadius merge into the higher rank's.
// - Who is fit (alive, a high process, outdoors, not fighting, not in a
//   swap) is the caller's to say; the unfit drop out and are handed out again
//   once fit. The engine's combat overrides any package anyway.

#include <cmath>

#include "Types.h"

namespace walkers {

inline constexpr UInt32 kHordeGroups = 16;  // tools/esp-write.py HORDE_GROUPS
inline constexpr UInt32 kHordeGroupMost = 16;
inline constexpr float kHordePi = 3.14159265f;
// A leader must come this much nearer its marker to count as progress.
inline constexpr float kHordeProgressUnits = 50.0f;

// A city gate as the horde knows it: the Tamriel-side door's position
// (CityGates.h).
struct HordeGate {
	float x = 0.0f;
	float y = 0.0f;
};

struct HordeSettings {
	bool enabled = false;           // [Horde] Enabled
	UInt32 travelBase = 0;          // group 0's travel package; NN is +NN
	UInt32 followBase = 0;          // group 0's follow package
	float joinRadius = 1500.0f;     // [Horde] JoinRadius: a lone walker joins a leader this near
	float leadUnits = 2000.0f;      // [Horde] LeadUnits: the marker this far ahead of the leader
	float reachUnits = 300.0f;      // the marker reached within this: it moves on
	float staleSeconds = 30.0f;     // [Horde] StaleSeconds: no nearer for this long - a new heading
	float turnDegrees = 60.0f;      // [Horde] TurnDegrees: the most a new marker turns from the last
	UInt32 groupMost = 8;           // [Horde] GroupMost: followers per leader
	// 4.11b: toward the cities. A group in Tamriel takes the nearest gate
	// within CityRange; each marker leans CityPull of the way toward it; near
	// it the marker goes GateHoldUnits in front of the door and stays.
	bool towardCities = true;       // [Horde] TowardCities
	float cityPull = 0.5f;          // [Horde] CityPull, 0 to 1
	float cityRange = 40000.0f;     // [Horde] CityRange
	float gateHoldUnits = 800.0f;   // [Horde] GateHoldUnits
	const HordeGate* gates = nullptr;
	UInt32 gateCount = 0;
};

struct HordeWalker {
	UInt32 id = 0;
	float x = 0.0f;
	float y = 0.0f;
	UInt32 rank = 0;     // the caller's, stable per walker; ties go to the higher id
	UInt32 package = 0;  // the package it runs now (form ID), 0 none
	bool fit = false;
	bool tamriel = false;  // in the Tamriel worldspace, where the gates are
};

struct HordeGroup {
	bool used = false;
	UInt32 leader = 0;
	float heading = 0.0f;  // radians
	float targetX = 0.0f;
	float targetY = 0.0f;
	float bestDistance = 0.0f;
	float sinceBetter = 0.0f;
	UInt32 followers[kHordeGroupMost] = {};
	UInt32 count = 0;
	int gate = -1;         // the gate it heads for, -1 none
	bool holding = false;  // at its gate: the marker stays
};

struct HordeState {
	HordeGroup groups[kHordeGroups];
};

struct HordeOrder {
	enum Kind : UInt8 { MoveMarker, Lead, Follow };
	Kind kind = MoveMarker;
	UInt32 group = 0;
	UInt32 walker = 0;   // Lead, Follow: who; MoveMarker: the leader it goes ahead of
	UInt32 package = 0;  // Lead, Follow: the package to give
	float x = 0.0f;      // MoveMarker: where
	float y = 0.0f;
	bool again = false;  // Lead, Follow: a package lost and given again (the engine may hold these back a while)
};

namespace horde_detail {

inline float Random01(UInt32& seed) {
	seed = seed * 1664525u + 1013904223u;
	return static_cast<float>(seed >> 8) / 16777216.0f;
}

inline const HordeWalker* Find(const HordeWalker* walkers, UInt32 n, UInt32 id) {
	for (UInt32 i = 0; i < n; ++i) {
		if (walkers[i].id == id) {
			return &walkers[i];
		}
	}
	return nullptr;
}

inline bool Higher(const HordeWalker& a, const HordeWalker& b) {
	return a.rank != b.rank ? a.rank > b.rank : a.id > b.id;
}

inline float Distance(float ax, float ay, float bx, float by) {
	const float dx = ax - bx;
	const float dy = ay - by;
	return std::sqrt(dx * dx + dy * dy);
}

struct Orders {
	HordeOrder* out;
	UInt32 most;
	UInt32 count = 0;
	UInt32 given[128] = {};
	UInt32 givenCount = 0;

	bool Given(UInt32 walker) const {
		for (UInt32 i = 0; i < givenCount; ++i) {
			if (given[i] == walker) {
				return true;
			}
		}
		return false;
	}
	void Add(HordeOrder::Kind kind, UInt32 group, UInt32 walker, UInt32 package, float x = 0.0f, float y = 0.0f,
	         bool again = false) {
		if (count >= most) {
			return;
		}
		HordeOrder& o = out[count++];
		o.kind = kind;
		o.group = group;
		o.walker = walker;
		o.package = package;
		o.x = x;
		o.y = y;
		o.again = again;
		if (kind != HordeOrder::MoveMarker && givenCount < 128) {
			given[givenCount++] = walker;
		}
	}
};

// The nearest gate within CityRange of the walker, -1 none.
inline int NearestGate(const HordeWalker& w, const HordeSettings& s) {
	if (!s.towardCities || !w.tamriel || s.gates == nullptr) {
		return -1;
	}
	int best = -1;
	float nearest = 0.0f;
	for (UInt32 i = 0; i < s.gateCount; ++i) {
		const float d = Distance(w.x, w.y, s.gates[i].x, s.gates[i].y);
		if (d <= s.cityRange && (best < 0 || d < nearest)) {
			best = static_cast<int>(i);
			nearest = d;
		}
	}
	return best;
}

// A new marker for the group: at its gate (GateHoldUnits in front of the
// door, on the leader's side) once the next step would bring it that near;
// else LeadUnits ahead along `heading`, leaned CityPull toward the gate.
inline void PlaceMarker(HordeGroup& g, const HordeWalker& leader, float heading, const HordeSettings& s) {
	if (g.gate >= 0 && static_cast<UInt32>(g.gate) < s.gateCount) {
		const HordeGate& gate = s.gates[g.gate];
		const float d = Distance(leader.x, leader.y, gate.x, gate.y);
		const float toGate = std::atan2(gate.y - leader.y, gate.x - leader.x);
		if (d - s.leadUnits <= s.gateHoldUnits) {
			const float outward = d > 1.0f ? std::atan2(leader.y - gate.y, leader.x - gate.x) : heading;
			g.heading = toGate;
			g.targetX = gate.x + std::cos(outward) * s.gateHoldUnits;
			g.targetY = gate.y + std::sin(outward) * s.gateHoldUnits;
			g.holding = true;
			g.bestDistance = Distance(leader.x, leader.y, g.targetX, g.targetY);
			g.sinceBetter = 0.0f;
			return;
		}
		const float vx = (1.0f - s.cityPull) * std::cos(heading) + s.cityPull * std::cos(toGate);
		const float vy = (1.0f - s.cityPull) * std::sin(heading) + s.cityPull * std::sin(toGate);
		heading = vx * vx + vy * vy > 1e-6f ? std::atan2(vy, vx) : toGate;
	}
	g.heading = heading;
	g.targetX = leader.x + std::cos(heading) * s.leadUnits;
	g.targetY = leader.y + std::sin(heading) * s.leadUnits;
	g.bestDistance = s.leadUnits;
	g.sinceBetter = 0.0f;
}

inline bool InGroup(const HordeState& state, UInt32 id) {
	for (const HordeGroup& g : state.groups) {
		if (!g.used) {
			continue;
		}
		if (g.leader == id) {
			return true;
		}
		for (UInt32 i = 0; i < g.count; ++i) {
			if (g.followers[i] == id) {
				return true;
			}
		}
	}
	return false;
}

}  // namespace horde_detail

// One step: the groups kept up to date with the walkers as they are, the
// orders for the engine (marker moves first, then the packages) into
// `orders`. Returns how many orders.
inline UInt32 StepHorde(HordeState& state, const HordeWalker* walkers, UInt32 n, const HordeSettings& s, float dt,
                        UInt32& seed, HordeOrder* orders, UInt32 most) {
	using namespace horde_detail;
	if (!s.enabled) {
		return 0;
	}
	Orders out{orders, most};
	bool fresh[kHordeGroups] = {};

	// The unfit and the gone drop out; a lost leader is replaced by the
	// follower of the highest rank, or the group ends.
	for (UInt32 gi = 0; gi < kHordeGroups; ++gi) {
		HordeGroup& g = state.groups[gi];
		if (!g.used) {
			continue;
		}
		UInt32 kept = 0;
		for (UInt32 i = 0; i < g.count; ++i) {
			const HordeWalker* w = Find(walkers, n, g.followers[i]);
			if (w != nullptr && w->fit) {
				g.followers[kept++] = g.followers[i];
			}
		}
		g.count = kept;
		const HordeWalker* leader = Find(walkers, n, g.leader);
		if (leader != nullptr && leader->fit) {
			continue;
		}
		UInt32 best = kHordeGroupMost;
		for (UInt32 i = 0; i < g.count; ++i) {
			const HordeWalker* w = Find(walkers, n, g.followers[i]);
			if (best == kHordeGroupMost || Higher(*w, *Find(walkers, n, g.followers[best]))) {
				best = i;
			}
		}
		if (best == kHordeGroupMost) {
			g = HordeGroup{};
			continue;
		}
		g.leader = g.followers[best];
		g.followers[best] = g.followers[--g.count];
		const HordeWalker* now = Find(walkers, n, g.leader);
		g.bestDistance = Distance(now->x, now->y, g.targetX, g.targetY);
		g.sinceBetter = 0.0f;
		out.Add(HordeOrder::Lead, gi, g.leader, s.travelBase + gi);
		for (UInt32 i = 0; i < g.count; ++i) {
			out.Add(HordeOrder::Follow, gi, g.followers[i], s.followBase + gi);
		}
	}

	// Groups whose leaders meet: the lower rank's into the higher rank's.
	for (UInt32 a = 0; a < kHordeGroups; ++a) {
		for (UInt32 b = 0; b < kHordeGroups; ++b) {
			HordeGroup& ga = state.groups[a];
			HordeGroup& gb = state.groups[b];
			if (a == b || !ga.used || !gb.used) {
				continue;
			}
			const HordeWalker* la = Find(walkers, n, ga.leader);
			const HordeWalker* lb = Find(walkers, n, gb.leader);
			if (!Higher(*la, *lb) || Distance(la->x, la->y, lb->x, lb->y) > s.joinRadius * 0.5f ||
			    ga.count + 1 + gb.count > s.groupMost) {
				continue;
			}
			ga.followers[ga.count++] = gb.leader;
			out.Add(HordeOrder::Follow, a, gb.leader, s.followBase + a);
			for (UInt32 i = 0; i < gb.count; ++i) {
				ga.followers[ga.count++] = gb.followers[i];
				out.Add(HordeOrder::Follow, a, gb.followers[i], s.followBase + a);
			}
			gb = HordeGroup{};
		}
	}

	// The fit walkers in no group, highest rank first: join the nearest
	// leader with room within JoinRadius, or lead a group of their own.
	bool handed[128] = {};
	for (;;) {
		UInt32 pick = n;
		for (UInt32 i = 0; i < n && i < 128; ++i) {
			if (!handed[i] && walkers[i].fit && !InGroup(state, walkers[i].id) &&
			    (pick == n || Higher(walkers[i], walkers[pick]))) {
				pick = i;
			}
		}
		if (pick == n) {
			break;
		}
		handed[pick] = true;
		const HordeWalker& w = walkers[pick];
		UInt32 near = kHordeGroups;
		float nearest = 0.0f;
		for (UInt32 gi = 0; gi < kHordeGroups; ++gi) {
			const HordeGroup& g = state.groups[gi];
			const HordeWalker* leader = g.used ? Find(walkers, n, g.leader) : nullptr;
			if (leader == nullptr || g.count >= s.groupMost || g.count >= kHordeGroupMost) {
				continue;
			}
			const float d = Distance(w.x, w.y, leader->x, leader->y);
			if (d <= s.joinRadius && (near == kHordeGroups || d < nearest)) {
				near = gi;
				nearest = d;
			}
		}
		if (near != kHordeGroups) {
			HordeGroup& g = state.groups[near];
			g.followers[g.count++] = w.id;
			out.Add(HordeOrder::Follow, near, w.id, s.followBase + near);
			continue;
		}
		for (UInt32 gi = 0; gi < kHordeGroups; ++gi) {
			HordeGroup& g = state.groups[gi];
			if (g.used) {
				continue;
			}
			g = HordeGroup{};
			g.used = true;
			g.leader = w.id;
			g.gate = NearestGate(w, s);
			PlaceMarker(g, w, Random01(seed) * 2.0f * kHordePi, s);
			fresh[gi] = true;
			out.Add(HordeOrder::MoveMarker, gi, w.id, 0, g.targetX, g.targetY);
			out.Add(HordeOrder::Lead, gi, w.id, s.travelBase + gi);
			break;
		}
	}

	// The markers: moved on when reached or stale; and the packages given
	// again to those who lost theirs.
	for (UInt32 gi = 0; gi < kHordeGroups; ++gi) {
		HordeGroup& g = state.groups[gi];
		if (!g.used || fresh[gi]) {
			continue;
		}
		const HordeWalker* leader = Find(walkers, n, g.leader);
		const float d = Distance(leader->x, leader->y, g.targetX, g.targetY);
		if (d < g.bestDistance - kHordeProgressUnits) {
			g.bestDistance = d;
			g.sinceBetter = 0.0f;
		} else {
			g.sinceBetter += dt;
		}
		if (g.holding) {
			// At the gate: the marker stays; only a leader drawn far off it (a
			// fight) is sent back.
			if (d > s.leadUnits) {
				out.Add(HordeOrder::MoveMarker, gi, g.leader, 0, g.targetX, g.targetY);
				out.Add(HordeOrder::Lead, gi, g.leader, s.travelBase + gi);
			} else if (leader->package != s.travelBase + gi && !out.Given(g.leader)) {
				out.Add(HordeOrder::Lead, gi, g.leader, s.travelBase + gi, 0.0f, 0.0f, true);
			}
			for (UInt32 i = 0; i < g.count; ++i) {
				const HordeWalker* w = Find(walkers, n, g.followers[i]);
				if (w->package != s.followBase + gi && !out.Given(w->id)) {
					out.Add(HordeOrder::Follow, gi, w->id, s.followBase + gi, 0.0f, 0.0f, true);
				}
			}
			continue;
		}
		const bool reached = d <= s.reachUnits;
		if (reached || g.sinceBetter >= s.staleSeconds) {
			const float turn = s.turnDegrees * kHordePi / 180.0f;
			const float heading = reached ? g.heading + (Random01(seed) * 2.0f - 1.0f) * turn
			                              : Random01(seed) * 2.0f * kHordePi;
			PlaceMarker(g, *leader, heading, s);
			out.Add(HordeOrder::MoveMarker, gi, g.leader, 0, g.targetX, g.targetY);
			out.Add(HordeOrder::Lead, gi, g.leader, s.travelBase + gi);
		} else if (leader->package != s.travelBase + gi && !out.Given(g.leader)) {
			out.Add(HordeOrder::Lead, gi, g.leader, s.travelBase + gi, 0.0f, 0.0f, true);
		}
		for (UInt32 i = 0; i < g.count; ++i) {
			const HordeWalker* w = Find(walkers, n, g.followers[i]);
			if (w->package != s.followBase + gi && !out.Given(w->id)) {
				out.Add(HordeOrder::Follow, gi, w->id, s.followBase + gi, 0.0f, 0.0f, true);
			}
		}
	}
	return out.count;
}

}  // namespace walkers
