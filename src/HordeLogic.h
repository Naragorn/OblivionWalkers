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

#include "NoiseLogic.h"
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
	// 4.11c: a gate holds at most GateHordesMost groups at once (0: any),
	// and only while the player is outdoors in Tamriel within GateNearUnits
	// of it (0: always); any other group near its gate waits out of the
	// player's loaded grid (LoadedGrid cells across, the engine's
	// uGridsToLoad), where the engine keeps it out of the full update.
	UInt32 gateHordesMost = 1;      // [Horde] GateHordesMost
	float gateNearUnits = 8000.0f;  // [Horde] GateNearUnits
	UInt32 loadedGrid = 5;          // [Horde] LoadedGrid
	bool markerOnGround = true;     // [Horde] MarkerOnGround: the marker on the land, not at the leader's height
	const HordeGate* gates = nullptr;
	UInt32 gateCount = 0;
	NoiseSettings noise;            // 4.20.2: [Noise]
};

struct HordeWalker {
	UInt32 id = 0;
	float x = 0.0f;
	float y = 0.0f;
	UInt32 rank = 0;     // the caller's, stable per walker; ties go to the higher id
	UInt32 package = 0;  // the package it runs now (form ID), 0 none
	bool fit = false;
	bool tamriel = false;  // in the Tamriel worldspace, where the gates are
	UInt32 world = 0;      // its worldspace (4.20.2: a noise is heard in its own only)
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
	bool waiting = false;  // 4.11c: near its gate, not let to hold - waits out of the player's grid
	UInt32 aimedAt = 0;    // whom its follow package was last aimed at (an Aim order)
	// 4.20.2: drawn to a noise at noiseX, noiseY - on the way while
	// searchLeft < 0, then searching it; noiseFresh until its marker is set.
	bool drawn = false;
	bool noiseFresh = false;
	float noiseX = 0.0f;
	float noiseY = 0.0f;
	float drawnSeconds = 0.0f;
	float searchLeft = -1.0f;
	float hopLeft = 0.0f;
};

// Where the player is, for 4.11c; set by the caller before each step.
struct HordePlayer {
	bool tamriel = false;  // outdoors in Tamriel, where the gates are
	float x = 0.0f;
	float y = 0.0f;
};

struct HordeState {
	HordeGroup groups[kHordeGroups];
	HordePlayer player;
};

inline constexpr float kHordeCellUnits = 4096.0f;
inline constexpr float kHordeWaitStepUnits = 512.0f;
inline constexpr float kHordeWaitMostUnits = 65536.0f;

// Whether x, y lies outside the grid of `grid` x `grid` exterior cells the
// engine keeps loaded around the player's cell (uGridsToLoad).
inline bool OutsideLoadedGrid(float px, float py, float x, float y, UInt32 grid) {
	const int half = static_cast<int>(grid / 2);
	const int pcx = static_cast<int>(std::floor(px / kHordeCellUnits));
	const int pcy = static_cast<int>(std::floor(py / kHordeCellUnits));
	const int cx = static_cast<int>(std::floor(x / kHordeCellUnits));
	const int cy = static_cast<int>(std::floor(y / kHordeCellUnits));
	const int dx = cx > pcx ? cx - pcx : pcx - cx;
	const int dy = cy > pcy ? cy - pcy : pcy - cy;
	return dx > half || dy > half;
}

struct HordeOrder {
	// Aim: the group's follow package aimed at `walker`, its leader - before
	// any Follow of the group that step.
	enum Kind : UInt8 { MoveMarker, Lead, Follow, Aim };
	Kind kind = MoveMarker;
	UInt32 group = 0;
	UInt32 walker = 0;   // Lead, Follow: who; MoveMarker: the leader it goes ahead of; Aim: the leader
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
// The marker GateHoldUnits in front of the group's gate, on the leader's
// side; the group holds there.
inline void HoldAtGate(HordeGroup& g, const HordeWalker& leader, float heading, const HordeSettings& s) {
	const HordeGate& gate = s.gates[g.gate];
	const float d = Distance(leader.x, leader.y, gate.x, gate.y);
	const float outward = d > 1.0f ? std::atan2(leader.y - gate.y, leader.x - gate.x) : heading;
	g.heading = std::atan2(gate.y - leader.y, gate.x - leader.x);
	g.targetX = gate.x + std::cos(outward) * s.gateHoldUnits;
	g.targetY = gate.y + std::sin(outward) * s.gateHoldUnits;
	g.holding = true;
	g.waiting = false;
	g.bestDistance = Distance(leader.x, leader.y, g.targetX, g.targetY);
	g.sinceBetter = 0.0f;
}

// 4.11c: the marker straight away from the player from the leader, at the
// first 512-unit step out of the player's loaded grid; the group waits there
// (the spot set once - a player who walks up to it meets the horde).
inline void WaitAway(HordeGroup& g, const HordeWalker& leader, float heading, const HordeSettings& s,
                     const HordePlayer& p) {
	const float d = Distance(p.x, p.y, leader.x, leader.y);
	const float away = d > 1.0f ? std::atan2(leader.y - p.y, leader.x - p.x) : heading;
	float r = 0.0f;
	while (r < kHordeWaitMostUnits && !OutsideLoadedGrid(p.x, p.y, leader.x + std::cos(away) * r,
	                                                      leader.y + std::sin(away) * r, s.loadedGrid)) {
		r += kHordeWaitStepUnits;
	}
	g.heading = away;
	g.targetX = leader.x + std::cos(away) * r;
	g.targetY = leader.y + std::sin(away) * r;
	g.holding = false;
	g.waiting = true;
	g.bestDistance = r;
	g.sinceBetter = 0.0f;
}

// The groups other than `except` holding at `gate`.
inline UInt32 HoldingAt(const HordeState& state, int gate, UInt32 except) {
	UInt32 n = 0;
	for (UInt32 gi = 0; gi < kHordeGroups; ++gi) {
		const HordeGroup& g = state.groups[gi];
		n += gi != except && g.used && g.holding && g.gate == gate ? 1u : 0u;
	}
	return n;
}

// 4.11c: whether group `gi` may hold at its gate now - the player within
// GateNearUnits of it (or not outdoors in Tamriel: then no walker of it is
// in the player's grid), fewer than GateHordesMost others holding there.
inline bool MayHold(const HordeState& state, UInt32 gi, const HordeSettings& s) {
	const HordeGroup& g = state.groups[gi];
	const HordeGate& gate = s.gates[g.gate];
	const HordePlayer& p = state.player;
	const bool near = s.gateNearUnits <= 0.0f || !p.tamriel || Distance(p.x, p.y, gate.x, gate.y) <= s.gateNearUnits;
	return near && (s.gateHordesMost == 0 || HoldingAt(state, g.gate, gi) < s.gateHordesMost);
}

inline bool HasGate(const HordeGroup& g, const HordeSettings& s) {
	return g.gate >= 0 && static_cast<UInt32>(g.gate) < s.gateCount && s.gates != nullptr;
}

inline void PlaceMarker(HordeState& state, UInt32 gi, const HordeWalker& leader, float heading,
                        const HordeSettings& s) {
	HordeGroup& g = state.groups[gi];
	g.waiting = false;
	if (HasGate(g, s)) {
		const HordeGate& gate = s.gates[g.gate];
		const float d = Distance(leader.x, leader.y, gate.x, gate.y);
		const float toGate = std::atan2(gate.y - leader.y, gate.x - leader.x);
		if (d - s.leadUnits <= s.gateHoldUnits) {
			if (MayHold(state, gi, s)) {
				HoldAtGate(g, leader, heading, s);
			} else {
				WaitAway(g, leader, heading, s, state.player);
			}
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

inline bool IsFollowPackage(UInt32 package, const HordeSettings& s) {
	return package >= s.followBase && package < s.followBase + kHordeGroups;
}

inline constexpr UInt32 kHordeOrdersMost = 288;

// The follow packages, last (gate-guards.txt, 2026-10-26: the game froze
// twice right after a step that aimed a group's follow package at a leader
// still running a follow package - one following itself, or leaders
// following each other round). A group's package is aimed at its leader
// only once that leader runs no follow package of its own, and before any
// of its followers is given it; until then the group's Follow orders wait
// (its Lead is in the orders, or given again). The orders come out marker
// moves first, then the aims, then the packages; past `most` the last ones
// are left for the next step.
inline UInt32 SettleFollows(HordeState& state, const HordeWalker* walkers, UInt32 n, const HordeSettings& s,
                            HordeOrder* orders, UInt32 count, UInt32 most) {
	bool ready[kHordeGroups] = {};
	HordeOrder aims[kHordeGroups];
	UInt32 aimCount = 0;
	for (UInt32 gi = 0; gi < kHordeGroups; ++gi) {
		HordeGroup& g = state.groups[gi];
		const HordeWalker* leader = g.used ? Find(walkers, n, g.leader) : nullptr;
		ready[gi] = leader != nullptr && !IsFollowPackage(leader->package, s);
		if (ready[gi] && g.aimedAt != g.leader) {
			HordeOrder& a = aims[aimCount++];
			a.kind = HordeOrder::Aim;
			a.group = gi;
			a.walker = g.leader;
			g.aimedAt = g.leader;
		}
	}
	static HordeOrder sorted[kHordeOrdersMost + kHordeGroups];
	UInt32 total = 0;
	const UInt32 seen = count < kHordeOrdersMost ? count : kHordeOrdersMost;
	for (UInt32 i = 0; i < seen; ++i) {
		if (orders[i].kind == HordeOrder::MoveMarker) {
			sorted[total++] = orders[i];
		}
	}
	for (UInt32 i = 0; i < aimCount; ++i) {
		sorted[total++] = aims[i];
	}
	for (UInt32 i = 0; i < seen; ++i) {
		const HordeOrder& o = orders[i];
		if (o.kind == HordeOrder::MoveMarker || (o.kind == HordeOrder::Follow && !ready[o.group])) {
			continue;
		}
		sorted[total++] = o;
	}
	const UInt32 kept = total < most ? total : most;
	for (UInt32 i = 0; i < kept; ++i) {
		orders[i] = sorted[i];
	}
	// An aim cut off past `most` is aimed again the next step.
	for (UInt32 i = kept; i < total; ++i) {
		if (sorted[i].kind == HordeOrder::Aim) {
			state.groups[sorted[i].group].aimedAt = 0;
		}
	}
	return kept;
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

// The follow package's target is a raw pointer to the leader, as xOBSE's
// SetPackageTarget writes it; the engine reads it for every follower
// (0x0064F650 reads the reference's flags, 0x0064F840 and 0x0063D0C0 call
// through it without a null check - read with REA 2026-11-10). Two Windows
// crashes of 2026-11-10 read it after the leader was gone (eip 0x0064F67C,
// packages 37000816 and 37000817, the pointer in freed memory). It holds only
// while the leader is the very reference the game keeps under his ID
// (`held`), alive, enabled and loaded; otherwise the package is aimed at the
// group's own marker - a persistent reference the game never frees - frames
// before the engine can clean the leader up.
inline bool FollowTargetHolds(bool held, bool dead, bool disabled, bool loaded) {
	return held && !dead && !disabled && loaded;
}

// The z offset for the marker's MoveTo onto the leader: the marker on the
// land under its spot (`landZ`, when `haveLand`) when `onGround`, else at
// the leader's height (0). Whitmond Farm, 2026-11-11: groups on the hill
// stood still for minutes, their markers 2000 units downhill at the hill's
// height.
inline float MarkerHeightOffset(bool onGround, bool haveLand, float landZ, float leaderZ) {
	return onGround && haveLand ? landZ - leaderZ : 0.0f;
}

// The gate the biggest group heads for (the news, docs/walkers-spec.md 4.9),
// the first group's of two as big; -1 when no group heads for one.
inline int HordeHeadingGate(const HordeState& state) {
	int gate = -1;
	UInt32 most = 0;
	for (const HordeGroup& g : state.groups) {
		if (g.used && g.gate >= 0 && g.count + 1 > most) {
			most = g.count + 1;
			gate = g.gate;
		}
	}
	return gate;
}

// A noise at x, y in `world` (4.20.2): every group with a fit walker
// within the noise's Radius is drawn to it (the fighters themselves are
// unfit - they drop out of their group anyway) - a group drawn already to a spot within
// SearchUnits searches it anew instead. Returns how many groups heard it.
// The markers and packages follow in StepHorde.
inline UInt32 HearNoise(HordeState& state, const HordeWalker* walkers, UInt32 n, const HordeSettings& s, float x,
                        float y, UInt32 world) {
	using namespace horde_detail;
	if (!s.enabled || !s.noise.enabled) {
		return 0;
	}
	UInt32 heard = 0;
	for (HordeGroup& g : state.groups) {
		if (!g.used) {
			continue;
		}
		const HordeWalker* leader = Find(walkers, n, g.leader);
		bool hears = false;
		for (UInt32 i = 0; i <= g.count && !hears; ++i) {
			const HordeWalker* w = i == g.count ? leader : Find(walkers, n, g.followers[i]);
			hears = w != nullptr && w->fit && w->world == world && Distance(w->x, w->y, x, y) <= s.noise.radius;
		}
		if (!hears || leader == nullptr) {
			continue;
		}
		++heard;
		if (g.drawn && Distance(g.noiseX, g.noiseY, x, y) <= s.noise.searchUnits) {
			if (g.searchLeft >= 0.0f) {
				g.searchLeft = s.noise.searchSeconds;
			}
			continue;
		}
		g.drawn = true;
		g.noiseFresh = true;
		g.holding = false;
		g.waiting = false;
		g.noiseX = x;
		g.noiseY = y;
		g.targetX = x;
		g.targetY = y;
		g.drawnSeconds = 0.0f;
		g.searchLeft = -1.0f;
		g.bestDistance = Distance(leader->x, leader->y, x, y);
		g.sinceBetter = 0.0f;
	}
	return heard;
}

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
			PlaceMarker(state, gi, w, Random01(seed) * 2.0f * kHordePi, s);
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
		if (g.drawn) {
			// 4.20.2: to the noise, searching it, then on as before.
			bool lead = false;
			if (g.noiseFresh) {
				g.noiseFresh = false;
				lead = true;
			} else if (g.searchLeft < 0.0f) {
				g.drawnSeconds += dt;
				if (d <= s.reachUnits || g.drawnSeconds >= s.noise.travelSeconds) {
					g.searchLeft = s.noise.searchSeconds;
					g.hopLeft = s.noise.hopSeconds;
				}
			} else {
				g.searchLeft -= dt;
				g.hopLeft -= dt;
				if (g.searchLeft <= 0.0f) {
					g.drawn = false;
					g.searchLeft = -1.0f;
					PlaceMarker(state, gi, *leader, g.heading, s);
					lead = true;
				} else if (g.hopLeft <= 0.0f) {
					g.hopLeft = s.noise.hopSeconds;
					const float angle = Random01(seed) * 2.0f * kHordePi;
					const float reach = Random01(seed) * s.noise.searchUnits;
					g.targetX = g.noiseX + std::cos(angle) * reach;
					g.targetY = g.noiseY + std::sin(angle) * reach;
					g.bestDistance = Distance(leader->x, leader->y, g.targetX, g.targetY);
					g.sinceBetter = 0.0f;
					lead = true;
				}
			}
			if (lead) {
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
		if (g.holding || g.waiting) {
			// At the gate, or waiting out of sight: the marker stays; only a
			// leader drawn far off it (a fight) is sent back. 4.11c: a holding
			// group no longer let to hold goes off to wait, a waiting one let
			// to hold moves up to its gate.
			const bool may = HasGate(g, s) && MayHold(state, gi, s);
			if (g.holding != may && HasGate(g, s)) {
				if (may) {
					HoldAtGate(g, *leader, g.heading, s);
				} else {
					WaitAway(g, *leader, g.heading, s, state.player);
				}
				out.Add(HordeOrder::MoveMarker, gi, g.leader, 0, g.targetX, g.targetY);
				out.Add(HordeOrder::Lead, gi, g.leader, s.travelBase + gi);
			} else if (d > s.leadUnits) {
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
			PlaceMarker(state, gi, *leader, heading, s);
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
	return SettleFollows(state, walkers, n, s, orders, out.count, most);
}

}  // namespace walkers
