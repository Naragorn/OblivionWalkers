#pragma once

// The gate guards, process-free (docs/walkers-spec.md 4.11b; Nadi,
// 2026-10-26): guards come out of a city's gate to fight the walkers there,
// one for every ZombiesPerGuard of them, each with HealthFactor times a
// walker's health; whenever one dies a new one comes out behind the line,
// so the walkers always lose in the end.
//
// The guards are each city's pool of twelve persistent, initially disabled
// references in the ESP's holding cell (tools/esp-write.py): sent out to a
// gate (moved there, enabled, raised when dead), taken in once no walker has
// been near for ClearSeconds (disabled). No reference is made at runtime -
// actors made with PlaceAtMe stay in the save for good (the CS wiki's
// PlaceAtMe page), and a guard that never stops coming back would make one
// every time. The pool is all there is: at most twelve per city out at
// once (Nadi, 2026-10-26: "größerer Pool, 12 pro Stadt"), a dead one raised
// and sent out again.

#include "CityGates.h"
#include "Types.h"

#include <cmath>

namespace walkers {

inline constexpr UInt32 kGuardCities = static_cast<UInt32>(City::Count);
inline constexpr UInt32 kGuardPool = 12;  // tools/esp-write.py GUARD_POOL
// The pool's references come in banks of six per city: the first bank where
// the six-pool of 2026-10-26 morning had them (0x83F + city x 6), the next
// after it - a save made with the six-pool keeps its guards.
inline constexpr UInt32 kGuardBank = 6;
inline constexpr UInt32 kGuardPoolFirstLocalId = 0x83F;
inline constexpr UInt32 kGuardGatesMost = 32;
inline constexpr UInt32 kGuardsOutPerStep = 2;  // per gate and step, so they come out one after another
inline constexpr float kGuardSpacing = 70.0f;   // between two guards side by side at the way out
inline constexpr UInt32 kGuardsPerRow = 6;      // a row across the way out; the next row farther out

struct GuardSettings {
	bool enabled = true;           // [Guards] Enabled
	float healthFactor = 5.0f;     // [Guards] HealthFactor: a guard's health, times a walker's
	UInt32 zombiesPerGuard = 5;    // [Guards] ZombiesPerGuard
	float radius = 4000.0f;        // [Guards] Radius: walkers this near a gate's door count
	float clearSeconds = 30.0f;    // [Guards] ClearSeconds: no walker near this long - the guards go in
	// [Guards] TestKillSeconds: a test switch, off (0) - a guard out this
	// long is killed where it stands, so the harness sees the fallen
	// replaced and raised (a console kill of a pool guard does not take
	// through the harness's console lines).
	float testKillSeconds = 0.0f;
};

struct GuardSlot {
	bool out = false;
	int gate = -1;
	float outFor = 0.0f;  // standing out this long (TestKillSeconds)
};

struct GuardState {
	GuardSlot slots[kGuardCities][kGuardPool];
	float clearFor[kGuardGatesMost] = {};
};

struct GuardOrder {
	enum Kind : UInt8 { Deploy, Recall, Kill };  // Kill: TestKillSeconds only
	Kind kind = Deploy;
	UInt32 city = 0;
	UInt32 slot = 0;
	int gate = -1;       // Deploy: where to; Recall: where from
	bool raise = false;  // Deploy: it is dead - raise it first
};

// How many guards the walkers at a gate ask for.
inline UInt32 GuardsWanted(UInt32 zombies, UInt32 zombiesPerGuard) {
	if (zombies == 0) {
		return 0;
	}
	const UInt32 per = zombiesPerGuard == 0 ? 1u : zombiesPerGuard;
	const UInt32 wanted = (zombies + per - 1) / per;
	return wanted < kGuardPool ? wanted : kGuardPool;
}

// The ESP's local form ID of pool guard `slot` of `city` (tools/esp-write.py
// writes them so).
inline constexpr UInt32 PoolLocalId(UInt32 city, UInt32 slot) {
	return kGuardPoolFirstLocalId + (slot / kGuardBank) * kGuardCities * kGuardBank + city * kGuardBank +
	       slot % kGuardBank;
}

// A guard's health: HealthFactor times a walker's, rounded, at least 1.
inline UInt32 GuardHealth(float factor, UInt32 walkerHealth) {
	const float health = std::floor(factor * static_cast<float>(walkerHealth) + 0.5f);
	return health < 1.0f ? 1u : static_cast<UInt32>(health);
}

// Where guard `slot` comes out at `gate`: on the spot its way back out of
// the city puts one, in rows of kGuardsPerRow side by side across that way,
// kGuardSpacing apart, each row kGuardSpacing farther out than the last -
// still behind the walkers, who hold farther out (HordeSettings
// gateHoldUnits, 800).
inline void GuardSpot(const CityGate& gate, UInt32 slot, float& x, float& y) {
	float ax = gate.outX - gate.x;
	float ay = gate.outY - gate.y;
	const float length = std::sqrt(ax * ax + ay * ay);
	ax = length > 0.01f ? ax / length : 1.0f;
	ay = length > 0.01f ? ay / length : 0.0f;
	const float place = static_cast<float>(slot % kGuardsPerRow);
	const float row = static_cast<float>(slot / kGuardsPerRow);
	const float side = (place - (static_cast<float>(kGuardsPerRow) - 1.0f) * 0.5f) * kGuardSpacing;
	const float ahead = row * kGuardSpacing;
	x = gate.outX - ay * side + ax * ahead;
	y = gate.outY + ax * side + ay * ahead;
}

// After a load the pool is read off the world: an enabled guard is out, at
// its own city's gate nearest to where it stands; the rest wait.
inline void RecoverGuardState(GuardState& state, const bool (*enabled)[kGuardPool], const float (*xs)[kGuardPool],
                              const float (*ys)[kGuardPool], const CityGate* gates, UInt32 gateCount) {
	state = GuardState{};
	for (UInt32 c = 0; c < kGuardCities; ++c) {
		for (UInt32 s = 0; s < kGuardPool; ++s) {
			if (!enabled[c][s]) {
				continue;
			}
			int best = -1;
			float bestDistance = 0.0f;
			for (UInt32 g = 0; g < gateCount && g < kGuardGatesMost; ++g) {
				if (static_cast<UInt32>(gates[g].city) != c) {
					continue;
				}
				const float dx = gates[g].x - xs[c][s];
				const float dy = gates[g].y - ys[c][s];
				const float d = dx * dx + dy * dy;
				if (best < 0 || d < bestDistance) {
					best = static_cast<int>(g);
					bestDistance = d;
				}
			}
			state.slots[c][s].out = best >= 0;
			state.slots[c][s].gate = best;
		}
	}
}

// The living walkers within `radius` of (x, y).
inline UInt32 WalkersNear(float x, float y, float radius, const float* xs, const float* ys, const bool* alive,
                          UInt32 n) {
	UInt32 count = 0;
	for (UInt32 i = 0; i < n; ++i) {
		const float dx = xs[i] - x;
		const float dy = ys[i] - y;
		count += alive[i] && dx * dx + dy * dy <= radius * radius ? 1u : 0u;
	}
	return count;
}

// One step: guards out where walkers stand at a gate, in where none have
// for ClearSeconds. `gateCity` gives each gate's city, `zombies` the
// walkers near each, `alive` each pool guard's state. Returns the orders.
inline UInt32 StepGuards(GuardState& state, const GuardSettings& s, const UInt32* gateCity, UInt32 gateCount,
                         const UInt32* zombies, const bool (*alive)[kGuardPool], float dt, GuardOrder* orders,
                         UInt32 most) {
	if (!s.enabled) {
		return 0;
	}
	UInt32 count = 0;
	for (UInt32 c = 0; c < kGuardCities; ++c) {
		for (UInt32 i = 0; i < kGuardPool; ++i) {
			GuardSlot& slot = state.slots[c][i];
			if (!slot.out || !alive[c][i]) {
				continue;
			}
			slot.outFor += dt;
			if (s.testKillSeconds > 0.0f && slot.outFor >= s.testKillSeconds && count < most) {
				GuardOrder& o = orders[count++];
				o.kind = GuardOrder::Kill;
				o.city = c;
				o.slot = i;
				o.gate = slot.gate;
				slot.outFor = 0.0f;
			}
		}
	}
	const UInt32 gates = gateCount < kGuardGatesMost ? gateCount : kGuardGatesMost;
	for (UInt32 g = 0; g < gates; ++g) {
		const UInt32 city = gateCity[g];
		if (city >= kGuardCities) {
			continue;
		}
		GuardSlot* pool = state.slots[city];
		if (zombies[g] > 0) {
			state.clearFor[g] = 0.0f;
			UInt32 have = 0;
			for (UInt32 i = 0; i < kGuardPool; ++i) {
				have += pool[i].out && pool[i].gate == static_cast<int>(g) && alive[city][i] ? 1u : 0u;
			}
			const UInt32 wanted = GuardsWanted(zombies[g], s.zombiesPerGuard);
			bool sentNow[kGuardPool] = {};  // `alive` is the step's start: a dead one sent stays dead in it
			for (UInt32 sent = 0; have < wanted && sent < kGuardsOutPerStep && count < most; ++sent) {
				// A waiting one first; else a dead one, wherever it fell.
				UInt32 pick = kGuardPool;
				for (UInt32 i = 0; i < kGuardPool && pick == kGuardPool; ++i) {
					pick = !pool[i].out && alive[city][i] ? i : kGuardPool;
				}
				for (UInt32 i = 0; i < kGuardPool && pick == kGuardPool; ++i) {
					pick = !alive[city][i] && !sentNow[i] ? i : kGuardPool;
				}
				if (pick == kGuardPool) {
					break;
				}
				GuardOrder& o = orders[count++];
				o.kind = GuardOrder::Deploy;
				o.city = city;
				o.slot = pick;
				o.gate = static_cast<int>(g);
				o.raise = !alive[city][pick];
				pool[pick].out = true;
				pool[pick].gate = static_cast<int>(g);
				pool[pick].outFor = 0.0f;
				sentNow[pick] = true;
				++have;
			}
			continue;
		}
		bool any = false;
		for (UInt32 i = 0; i < kGuardPool; ++i) {
			any = any || (pool[i].out && pool[i].gate == static_cast<int>(g));
		}
		if (!any) {
			state.clearFor[g] = 0.0f;
			continue;
		}
		state.clearFor[g] += dt;
		if (state.clearFor[g] < s.clearSeconds) {
			continue;
		}
		state.clearFor[g] = 0.0f;
		for (UInt32 i = 0; i < kGuardPool && count < most; ++i) {
			if (pool[i].out && pool[i].gate == static_cast<int>(g)) {
				GuardOrder& o = orders[count++];
				o.kind = GuardOrder::Recall;
				o.city = city;
				o.slot = i;
				o.gate = static_cast<int>(g);
				pool[i] = GuardSlot{};
			}
		}
	}
	return count;
}

}  // namespace walkers
