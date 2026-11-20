#pragma once

// The necromancers' realms as numbers, process-free (docs/walkers-spec.md
// 4.23 B, build step 1; designed with Nadi 2026-11-20). A few rival bosses,
// each in a free dungeon of the held dungeons' pool, spread over the world;
// each sends walker bands toward the cities and takes a further free place
// near his own every few days. The pace is the world's, shared among the
// living: the fewer they are, the more each one does. A boss's fall stops his
// realm; the last fall either ends it or brings a new set after some days.
// What the bands and places do in the game is the caller's (Necro.cpp).

#include <cmath>
#include <cstring>

#include "Bytes.h"
#include "DungeonLogic.h"
#include "DungeonSiteLogic.h"
#include "Types.h"

namespace walkers {

inline constexpr UInt32 kNecroBossesMost = 5;
// Bands due at once for one boss (a long time away: the rest dropped).
inline constexpr UInt32 kNecroBandsAtOnceMost = 8;
// The start: each next home among this many free places farthest from the
// homes taken (read 3: five rolled blindly are spread in one start of five).
inline constexpr UInt32 kNecroFarthestFew = 3;

// [Necromancers] (the spec's proposed keys; WalkersMin/Max and RealmMost mine).
struct NecroSettings {
	bool on = false;                // Mode: 0 off, 1 on
	UInt32 bossesMin = 3;           // BossesMin
	UInt32 bossesMax = 5;           // BossesMax
	float bandsPerDay = 1.0f;       // BandsPerDay: a boss's, at the middle of BossesMin..BossesMax
	UInt32 bandSizeMin = 4;         // BandSizeMin
	UInt32 bandSizeMax = 9;         // BandSizeMax
	float newPlaceEveryDays = 3.0f; // NewPlaceEveryDays: 0 never
	UInt8 teams[kNecroBossesMost] = {1, 2, 3, 4, 5};  // Teams: the same number allies
	bool comeBack = true;           // ComeBack
	float comeBackDays = 3.0f;      // ComeBackDays
	bool paceByLiving = true;       // PaceByLiving
	float riseWithin = 8000.0f;     // RiseWithin
	UInt32 walkersMin = 16;         // WalkersMin: a hotspot's walkers
	UInt32 walkersMax = 30;         // WalkersMax
	UInt32 realmMost = 6;           // RealmMost: a boss's places, his home among them
	// A band's pace on the map and where it comes into the world: the gates'
	// bands' (BandLogic.h BandSettings unitsPerHour, nearUnits; not in the INI).
	float bandUnitsPerHour = 10000.0f;
	float bandNearUnits = 6000.0f;
};

// "1,1,2,3,3": a team a boss in the order they rose. An entry missing, empty,
// not a number, 0 or above 255 - that boss on a team of his own (his place's
// number, as the default 1,2,3,4,5).
inline void ParseNecroTeams(const char* text, UInt8* out) {
	for (UInt32 b = 0; b < kNecroBossesMost; ++b) {
		out[b] = static_cast<UInt8>(b + 1);
	}
	if (text == nullptr) {
		return;
	}
	UInt32 b = 0;
	const char* p = text;
	while (b < kNecroBossesMost) {
		while (*p == ' ' || *p == '\t') {
			++p;
		}
		UInt32 value = 0;
		UInt32 digits = 0;
		bool junk = false;
		while (*p != '\0' && *p != ',') {
			if (*p >= '0' && *p <= '9') {
				value = value > 1000 ? value : value * 10 + static_cast<UInt32>(*p - '0');
				++digits;
			} else if (*p != ' ' && *p != '\t') {
				junk = true;
			}
			++p;
		}
		if (!junk && digits > 0 && value >= 1 && value <= 255) {
			out[b] = static_cast<UInt8>(value);
		}
		++b;
		if (*p != ',') {
			break;
		}
		++p;
	}
}

inline bool NecroAllied(UInt8 teamA, UInt8 teamB) { return teamA == teamB; }

struct NecroBoss {
	UInt16 home = kDungeonNoCandidate;  // DungeonCandidates.h's index
	UInt8 team = 0;
	bool alive = false;
	float bandFrom = 0.0f;   // bands counted from this game day
	float bandCarry = 0.0f;  // the part of a band short of one
	float placeFrom = 0.0f;  // a new place counted from this game day
	UInt8 places = 0;        // the places of his realm, his home among them
};

struct NecroState {
	NecroBoss bosses[kNecroBossesMost];
	UInt32 count = 0;          // risen in this set
	float comeBackAt = -1.0f;  // all fell: the game day a new set rises; -1 none due
	bool over = false;         // all fell with ComeBack=0
};

enum class NecroEventKind : UInt8 { Rose, BandDue, PlaceDue, Fell, AllFell, CameBack };

struct NecroEvent {
	NecroEventKind kind;
	UInt8 boss;
	UInt16 place;  // Rose: his home; PlaceDue: the place to take
};

namespace necro_detail {

inline UInt32 Put(NecroEvent* out, UInt32 n, UInt32 most, NecroEventKind kind, UInt32 boss, UInt32 place) {
	if (out != nullptr && n < most) {
		out[n] = {kind, static_cast<UInt8>(boss), static_cast<UInt16>(place)};
	}
	return n + 1;
}

inline float Dist(const DungeonCandidate& a, const DungeonCandidate& b) { return std::hypot(a.x - b.x, a.y - b.y); }

}  // namespace necro_detail

inline UInt32 NecroAlive(const NecroState& st) {
	UInt32 n = 0;
	for (UInt32 b = 0; b < st.count && b < kNecroBossesMost; ++b) {
		n += st.bosses[b].alive ? 1u : 0u;
	}
	return n;
}

inline float NecroMiddle(const NecroSettings& s) {
	return (static_cast<float>(s.bossesMin) + static_cast<float>(s.bossesMax)) / 2.0f;
}

// What the pace is shared by: the living (PaceByLiving=1, decided 9), or the
// set that rose (0: a death changes nothing for the others).
inline UInt32 NecroPaceShare(const NecroSettings& s, const NecroState& st) {
	return NecroAlive(st) == 0 ? 0u : (s.paceByLiving ? NecroAlive(st) : st.count);
}

// A living boss's bands a game day: BandsPerDay at the middle of
// BossesMin..BossesMax, the world's pace shared (decided 3: one boss of the
// default 3..5 alone sends 4 a day).
inline float NecroBandsPerDay(const NecroSettings& s, const NecroState& st) {
	const UInt32 share = NecroPaceShare(s, st);
	return share == 0 || s.bandsPerDay <= 0.0f ? 0.0f : s.bandsPerDay * NecroMiddle(s) / static_cast<float>(share);
}

// The game days between a living boss's new places, by the same rule; 0 never.
inline float NecroNewPlaceDays(const NecroSettings& s, const NecroState& st) {
	const UInt32 share = NecroPaceShare(s, st);
	const float middle = NecroMiddle(s);
	return share == 0 || s.newPlaceEveryDays <= 0.0f || middle <= 0.0f
	           ? 0.0f
	           : s.newPlaceEveryDays * static_cast<float>(share) / middle;
}

// The boss of a realm's place (step 3), as an actor - the outpost's way
// (KindLogic.h OutpostBossActor; MOO may rewrite the lists): at a home one of
// Oblivion.esm's LL1NecromancerBoss100 (0004B949), at a place he took one of
// LL1Necromancer100 (00034092), his under-boss. Read off Oblivion.esm
// 2026-11-20.
inline constexpr UInt32 kNecroBossActors[] = {0x0004B946, 0x0004B945, 0x0004B947, 0x0004B948};
inline constexpr UInt32 kNecroUnderBossActors[] = {0x0004B939, 0x0004B93A, 0x0004B93B, 0x0004B93C, 0x0001E5E5,
                                                   0x0004B936, 0x0004B937, 0x0004B938, 0x00069ADC, 0x00069ADD};
inline UInt32 NecroBossActor(bool home, UInt32 roll) {
	return home ? kNecroBossActors[roll % (sizeof(kNecroBossActors) / sizeof(kNecroBossActors[0]))]
	            : kNecroUnderBossActors[roll % (sizeof(kNecroUnderBossActors) / sizeof(kNecroUnderBossActors[0]))];
}

// The chest at a home's boss (the outpost's chest, 4.22a 5c): the game's own
// necromancer boss chests, the Ayleid one in a ruin (DungNecroBossChestAlyied01
// 0003753F), else the fort's (DungNecroBossChestFort 00037542).
inline UInt32 NecroBossChest(DungeonKind kind) { return kind == DungeonKind::Ayleid ? 0x0003753Fu : 0x00037542u; }

inline UInt32 NecroHotspotWalkers(const NecroSettings& s, UInt32 roll) {
	return dungeon_detail::Between(s.walkersMin, s.walkersMax, roll);
}

inline UInt32 NecroBandSize(const NecroSettings& s, UInt32 roll) {
	return dungeon_detail::Between(s.bandSizeMin, s.bandSizeMax, roll);
}

// Up to `want` homes among the places `free` (in the pool, no site there):
// the first at random, each next among the kNecroFarthestFew free places
// farthest from their nearest home taken (read 3). Returns how many.
template <typename RollFn>
UInt32 PickNecroHomes(const DungeonCandidate* all, UInt32 count, const bool* free, UInt32 want, RollFn& roll,
                      UInt16* out) {
	if (want > kNecroBossesMost) {
		want = kNecroBossesMost;
	}
	UInt32 n = 0;
	UInt32 freeCount = 0;
	for (UInt32 c = 0; c < count; ++c) {
		freeCount += free[c] ? 1u : 0u;
	}
	if (freeCount == 0 || want == 0) {
		return 0;
	}
	const UInt32 first = roll() % freeCount;
	for (UInt32 c = 0, seen = 0; c < count; ++c) {
		if (free[c] && seen++ == first) {
			out[n++] = static_cast<UInt16>(c);
			break;
		}
	}
	while (n < want) {
		// the few farthest, kept in order: [0] the farthest
		UInt32 best[kNecroFarthestFew];
		float far[kNecroFarthestFew];
		UInt32 have = 0;
		for (UInt32 c = 0; c < count; ++c) {
			bool taken = !free[c];
			for (UInt32 k = 0; !taken && k < n; ++k) {
				taken = out[k] == c;
			}
			if (taken) {
				continue;
			}
			float nearest = -1.0f;
			for (UInt32 k = 0; k < n; ++k) {
				const float d = necro_detail::Dist(all[c], all[out[k]]);
				nearest = nearest < 0.0f || d < nearest ? d : nearest;
			}
			UInt32 at = have;
			while (at > 0 && far[at - 1] < nearest) {
				--at;
			}
			if (at >= kNecroFarthestFew) {
				continue;
			}
			for (UInt32 k = (have < kNecroFarthestFew ? have : kNecroFarthestFew - 1); k > at; --k) {
				best[k] = best[k - 1];
				far[k] = far[k - 1];
			}
			best[at] = c;
			far[at] = nearest;
			have = have < kNecroFarthestFew ? have + 1 : have;
		}
		if (have == 0) {
			break;
		}
		out[n++] = static_cast<UInt16>(best[roll() % have]);
	}
	return n;
}

// The nearest place free (and not in `taken`) to boss b's home that lies in
// his share: nearer his home than any other living boss's (a tie the
// lower boss's). kDungeonNoCandidate when none.
inline UInt32 NecroNextPlace(const NecroState& st, UInt32 b, const DungeonCandidate* all, UInt32 count,
                             const bool* free, const UInt16* taken, UInt32 takenCount) {
	const NecroBoss& me = st.bosses[b];
	if (me.home >= count) {
		return kDungeonNoCandidate;
	}
	UInt32 pick = kDungeonNoCandidate;
	float pickDist = 0.0f;
	for (UInt32 c = 0; c < count; ++c) {
		bool open = free[c];
		for (UInt32 k = 0; open && k < takenCount; ++k) {
			open = taken[k] != c;
		}
		if (!open) {
			continue;
		}
		const float mine = necro_detail::Dist(all[c], all[me.home]);
		bool share = true;
		for (UInt32 o = 0; share && o < st.count && o < kNecroBossesMost; ++o) {
			const NecroBoss& other = st.bosses[o];
			if (o == b || !other.alive || other.home >= count) {
				continue;
			}
			const float theirs = necro_detail::Dist(all[c], all[other.home]);
			share = !(theirs < mine || (theirs == mine && o < b));
		}
		if (share && (pick == kDungeonNoCandidate || mine < pickDist)) {
			pick = c;
			pickDist = mine;
		}
	}
	return pick;
}

namespace necro_detail {

template <typename RollFn>
UInt32 Rise(NecroState& st, const NecroSettings& s, const DungeonCandidate* all, UInt32 count, const bool* free,
            float now, RollFn& roll, NecroEvent* out, UInt32 n, UInt32 most, bool comeBack) {
	UInt32 want = dungeon_detail::Between(s.bossesMin, s.bossesMax, roll());
	want = want == 0 ? 1u : (want > kNecroBossesMost ? kNecroBossesMost : want);
	UInt16 homes[kNecroBossesMost] = {};
	const UInt32 picked = PickNecroHomes(all, count, free, want, roll, homes);
	if (picked == 0) {
		return n;  // no free place: tried again at the next step
	}
	st.count = picked;
	st.comeBackAt = -1.0f;
	if (comeBack) {
		n = Put(out, n, most, NecroEventKind::CameBack, 0, kDungeonNoCandidate);
	}
	for (UInt32 b = 0; b < kNecroBossesMost; ++b) {
		NecroBoss boss;
		if (b < picked) {
			boss.home = homes[b];
			boss.team = s.teams[b];
			boss.alive = true;
			boss.bandFrom = now;
			boss.placeFrom = now;
			boss.places = 1;
			n = Put(out, n, most, NecroEventKind::Rose, b, homes[b]);
		}
		st.bosses[b] = boss;
	}
	return n;
}

}  // namespace necro_detail

// Once a game second at game day `now`: the first set risen (or a new one
// after the last fall, ComeBack), and for every living boss the bands and the
// new place due. `free`: the places in the pool with no site there. Returns
// the events; the caller sends the bands, takes the places
// (NecroPlaceTaken) and places the hotspots.
template <typename RollFn>
UInt32 StepNecromancers(NecroState& st, const NecroSettings& s, const DungeonCandidate* all, UInt32 count,
                        const bool* free, float now, RollFn& roll, NecroEvent* out, UInt32 most) {
	if (!s.on || st.over) {
		return 0;
	}
	UInt32 n = 0;
	if (st.count == 0) {
		return necro_detail::Rise(st, s, all, count, free, now, roll, out, n, most, false);
	}
	if (NecroAlive(st) == 0) {
		if (st.comeBackAt >= 0.0f && now >= st.comeBackAt) {
			return necro_detail::Rise(st, s, all, count, free, now, roll, out, n, most, true);
		}
		return 0;
	}
	const float rate = NecroBandsPerDay(s, st);
	const float placeDays = NecroNewPlaceDays(s, st);
	UInt16 taken[kNecroBossesMost];
	UInt32 takenCount = 0;
	for (UInt32 b = 0; b < st.count && b < kNecroBossesMost; ++b) {
		NecroBoss& boss = st.bosses[b];
		if (!boss.alive) {
			continue;
		}
		if (now < boss.bandFrom) {
			boss.bandFrom = now;  // the clock set back: counted from now
		} else {
			boss.bandCarry += (now - boss.bandFrom) * rate;
			boss.bandFrom = now;
			const float whole = std::floor(boss.bandCarry);
			boss.bandCarry -= whole;
			const UInt32 due = whole >= static_cast<float>(kNecroBandsAtOnceMost) ? kNecroBandsAtOnceMost
			                                                                     : static_cast<UInt32>(whole);
			for (UInt32 k = 0; k < due; ++k) {
				n = necro_detail::Put(out, n, most, NecroEventKind::BandDue, b, boss.home);
			}
		}
		if (placeDays <= 0.0f || boss.places >= s.realmMost) {
			continue;
		}
		if (now < boss.placeFrom) {
			boss.placeFrom = now;
			continue;
		}
		if (now - boss.placeFrom < placeDays) {
			continue;
		}
		boss.placeFrom = now;
		const UInt32 pick = NecroNextPlace(st, b, all, count, free, taken, takenCount);
		if (pick != kDungeonNoCandidate) {
			taken[takenCount++] = static_cast<UInt16>(pick);
			n = necro_detail::Put(out, n, most, NecroEventKind::PlaceDue, b, pick);
		}
	}
	return n;
}

// Boss b fell (decided 7, 8): his realm stops. The last of the set: over, or
// a new set after ComeBackDays.
inline UInt32 NecroBossFell(NecroState& st, const NecroSettings& s, UInt32 b, float now, NecroEvent* out, UInt32 most) {
	if (b >= st.count || b >= kNecroBossesMost || !st.bosses[b].alive) {
		return 0;
	}
	st.bosses[b].alive = false;
	UInt32 n = necro_detail::Put(out, 0, most, NecroEventKind::Fell, b, st.bosses[b].home);
	if (NecroAlive(st) == 0) {
		n = necro_detail::Put(out, n, most, NecroEventKind::AllFell, b, kDungeonNoCandidate);
		if (s.comeBack) {
			st.comeBackAt = now + (s.comeBackDays > 0.0f ? s.comeBackDays : 0.0f);
		} else {
			st.over = true;
			st.comeBackAt = -1.0f;
		}
	}
	return n;
}

inline void NecroPlaceTaken(NecroState& st, UInt32 b) {
	if (b < st.count && b < kNecroBossesMost && st.bosses[b].places < 255) {
		++st.bosses[b].places;
	}
}

inline void NecroPlaceLost(NecroState& st, UInt32 b) {
	if (b < st.count && b < kNecroBossesMost && st.bosses[b].places > 0) {
		--st.bosses[b].places;
	}
}

// --- the bands far away (build step 4) -------------------------------------
// A boss's band leaves his home toward the nearest city, the way an open
// gate's does (BandLogic.h StepBands): far from the player a number only,
// within bandNearUnits of him the caller puts its walkers into the world
// (Near) and they are the hordes'; one that reaches its city is the city's
// siege pressure (Arrived). A boss's fall leaves his bands under way.
inline constexpr UInt32 kNecroBandsMost = 16;

struct NecroBand {
	UInt8 boss = 0;
	UInt8 team = 0;
	UInt8 city = 0;  // the index into the caller's cities
	UInt8 size = 0;
	float x = 0.0f;
	float y = 0.0f;
};

struct NecroCity {
	float x, y;
};

struct NecroBands {
	NecroBand bands[kNecroBandsMost];
	UInt32 count = 0;
};

enum class NecroBandEventKind : UInt8 { Formed, Near, Arrived, Dropped, Met, Destroyed };

struct NecroBandEvent {
	NecroBandEventKind kind;
	NecroBand band;
};

namespace necro_detail {

inline UInt8 NearestCity(const NecroCity* cities, UInt32 count, float x, float y) {
	UInt32 best = 0;
	for (UInt32 c = 1; c < count; ++c) {
		if (std::hypot(cities[c].x - x, cities[c].y - y) < std::hypot(cities[best].x - x, cities[best].y - y)) {
			best = c;
		}
	}
	return static_cast<UInt8>(best);
}

inline UInt32 PutBand(NecroBandEvent* out, UInt32 n, UInt32 room, NecroBandEventKind kind, const NecroBand& b) {
	if (out != nullptr && n < room) {
		out[n] = {kind, b};
	}
	return n + 1;
}

}  // namespace necro_detail

// Boss `boss` of team `team` sends a band of `size` from (x, y), his home's
// door: toward the nearest city, at once Near when the player is that close;
// dropped when kNecroBandsMost are on their way already.
inline UInt32 NecroBandFormed(NecroBands& st, const NecroSettings& s, UInt32 boss, UInt8 team, UInt32 size, float x,
                              float y, const NecroCity* cities, UInt32 cityCount, float px, float py,
                              NecroBandEvent* out, UInt32 room) {
	if (cityCount == 0) {
		return 0;
	}
	NecroBand b;
	b.boss = static_cast<UInt8>(boss);
	b.team = team;
	b.city = necro_detail::NearestCity(cities, cityCount, x, y);
	b.size = static_cast<UInt8>(size > 255 ? 255 : size);
	b.x = x;
	b.y = y;
	UInt32 n = necro_detail::PutBand(out, 0, room, NecroBandEventKind::Formed, b);
	if (std::hypot(x - px, y - py) < s.bandNearUnits) {
		return necro_detail::PutBand(out, n, room, NecroBandEventKind::Near, b);
	}
	if (st.count >= kNecroBandsMost) {
		return necro_detail::PutBand(out, n, room, NecroBandEventKind::Dropped, b);
	}
	st.bands[st.count++] = b;
	return n;
}

// `hours` game hours on: every far band walks toward its city; Near once
// the player (px, py) is within bandNearUnits, Arrived at its city.
inline UInt32 StepNecroBands(NecroBands& st, const NecroSettings& s, const NecroCity* cities, UInt32 cityCount,
                             float hours, float px, float py, NecroBandEvent* out, UInt32 room) {
	if (cityCount == 0) {
		return 0;
	}
	const float step = s.bandUnitsPerHour * (hours > 0.0f ? hours : 0.0f);
	UInt32 n = 0;
	for (UInt32 i = 0; i < st.count;) {
		NecroBand& b = st.bands[i];
		const NecroCity& c = cities[b.city < cityCount ? b.city : 0];
		const float left = std::hypot(c.x - b.x, c.y - b.y);
		NecroBandEventKind done = NecroBandEventKind::Formed;  // none
		if (left <= step) {
			b.x = c.x;
			b.y = c.y;
			done = NecroBandEventKind::Arrived;
		} else {
			if (step > 0.0f && left > 0.0f) {
				b.x += (c.x - b.x) / left * step;
				b.y += (c.y - b.y) / left * step;
			}
			if (std::hypot(b.x - px, b.y - py) < s.bandNearUnits) {
				done = NecroBandEventKind::Near;
			}
		}
		if (done == NecroBandEventKind::Formed) {
			++i;
			continue;
		}
		n = necro_detail::PutBand(out, n, room, done, b);
		for (UInt32 k = i + 1; k < st.count; ++k) {
			st.bands[k - 1] = st.bands[k];
		}
		--st.count;
	}
	return n;
}

// --- the teams (build step 5) ----------------------------------------------
// A team is the plugin's, a reference's (read 2: a faction on a reference is
// one on its base): every walker of a necromancer's place or band carries his
// team. The engine's own factions do not decide a walker's fights (measured
// 2026-11-17), so the plugin does, twice a second, for the walkers near:
//  - a walker fighting an ally's walker, its own or an ally's necromancer,
//    stops; one of a team fighting a free walker (none's) or a free walker
//    fighting one of a team stops - the free walkers fight nobody's walkers
//    (decided 11);
//  - one of a team not fighting is set on an enemy team's walker or
//    necromancer within reach (StartCombat).
// With [Daedra] Mode=2 a necromancer and a Daedra stop their fight
// (decided 12); the walkers stand to the Daedra as the free ones do.

// Whether a walker of team `mine` (0 a free walker) fighting one of team
// `theirs` (0 a free walker) - a walker or a necromancer - lets go.
inline bool NecroFightStops(UInt8 mine, UInt8 theirs) {
	if (mine == 0 && theirs == 0) {
		return false;  // free walkers among themselves: as ever
	}
	if (mine == 0 || theirs == 0) {
		return true;  // decided 11
	}
	return NecroAllied(mine, theirs);
}

// Whether a walker of team `mine`, not fighting, is set on one of team
// `theirs` `distance` away (a walker or a necromancer of that team).
inline bool NecroSetOn(UInt8 mine, UInt8 theirs, bool fighting, float distance, float reach) {
	return !fighting && mine != 0 && theirs != 0 && !NecroAllied(mine, theirs) && distance >= 0.0f &&
	       distance <= reach;
}

// How near an enemy team's walker must be to be set on (units; a guess - the
// walkers' own sight is slow, 4.21).
inline constexpr float kNecroSetOnUnits = 1500.0f;

// Enemy bands far away that meet - within `meetUnits` - fight as numbers:
// each loses by the other's size, between half and all of it by the roll
// (the sizes before the fight); a band down to none is gone (Destroyed).
// Allied bands pass each other. Returns the events (Met, Destroyed).
template <typename RollFn>
UInt32 NecroBandsMeet(NecroBands& st, float meetUnits, RollFn& roll, NecroBandEvent* out, UInt32 room) {
	UInt32 n = 0;
	for (UInt32 i = 0; i < st.count; ++i) {
		for (UInt32 j = i + 1; j < st.count; ++j) {
			NecroBand& a = st.bands[i];
			NecroBand& b = st.bands[j];
			if (a.size == 0 || b.size == 0 || NecroAllied(a.team, b.team) ||
			    std::hypot(a.x - b.x, a.y - b.y) > meetUnits) {
				continue;
			}
			const UInt32 lossA = static_cast<UInt32>(b.size) * (50 + roll() % 51) / 100;
			const UInt32 lossB = static_cast<UInt32>(a.size) * (50 + roll() % 51) / 100;
			a.size = static_cast<UInt8>(lossA >= a.size ? 0 : a.size - lossA);
			b.size = static_cast<UInt8>(lossB >= b.size ? 0 : b.size - lossB);
			n = necro_detail::PutBand(out, n, room, NecroBandEventKind::Met, a);
			n = necro_detail::PutBand(out, n, room, NecroBandEventKind::Met, b);
		}
	}
	for (UInt32 i = 0; i < st.count;) {
		if (st.bands[i].size == 0) {
			n = necro_detail::PutBand(out, n, room, NecroBandEventKind::Destroyed, st.bands[i]);
			for (UInt32 k = i + 1; k < st.count; ++k) {
				st.bands[k - 1] = st.bands[k];
			}
			--st.count;
		} else {
			++i;
		}
	}
	return n;
}

// The co-save record: type "NCRO", version 2 - the count, comeBackAt's bits,
// over; then per boss of kNecroBossesMost: the home (16 bits), the team,
// alive, bandFrom, bandCarry, placeFrom as their bits, the places; then the
// far bands' count and per band of kNecroBandsMost: boss, team, city, size,
// x and y as their bits.
inline constexpr UInt32 kNecroRecordType = 0x4E43524F;  // 'NCRO'
inline constexpr UInt32 kNecroRecordVersion = 2;
inline constexpr UInt32 kNecroBossBytes = 20;
inline constexpr UInt32 kNecroBandBytes = 12;
inline constexpr UInt32 kNecroImageBytes =
    12 + kNecroBossesMost * kNecroBossBytes + 4 + kNecroBandsMost * kNecroBandBytes;

inline UInt32 NecroImage(const NecroState& st, const NecroBands& bands, UInt8* out, UInt32 capacity) {
	if (capacity < kNecroImageBytes) {
		return 0;
	}
	std::memset(out, 0, kNecroImageBytes);
	Put32(out, st.count);
	Put32(out + 4, dungeon_detail::Bits(st.comeBackAt));
	out[8] = st.over ? 1 : 0;
	for (UInt32 b = 0; b < kNecroBossesMost; ++b) {
		const NecroBoss& boss = st.bosses[b];
		UInt8* at = out + 12 + b * kNecroBossBytes;
		at[0] = static_cast<UInt8>(boss.home & 0xFF);
		at[1] = static_cast<UInt8>(boss.home >> 8);
		at[2] = boss.team;
		at[3] = boss.alive ? 1 : 0;
		Put32(at + 4, dungeon_detail::Bits(boss.bandFrom));
		Put32(at + 8, dungeon_detail::Bits(boss.bandCarry));
		Put32(at + 12, dungeon_detail::Bits(boss.placeFrom));
		at[16] = boss.places;
	}
	UInt8* far = out + 12 + kNecroBossesMost * kNecroBossBytes;
	Put32(far, bands.count);
	for (UInt32 i = 0; i < kNecroBandsMost; ++i) {
		const NecroBand& b = bands.bands[i];
		UInt8* at = far + 4 + i * kNecroBandBytes;
		at[0] = b.boss;
		at[1] = b.team;
		at[2] = b.city;
		at[3] = b.size;
		Put32(at + 4, dungeon_detail::Bits(b.x));
		Put32(at + 8, dungeon_detail::Bits(b.y));
	}
	return kNecroImageBytes;
}

// `st` emptied first; false when too short (a new set rises then). A boss
// whose home is not below `candidates` (another build's list) counts fallen.
inline bool ReadNecroImage(NecroState& st, NecroBands& bands, const UInt8* in, UInt32 length, UInt32 candidates) {
	st = NecroState{};
	bands = NecroBands{};
	if (length < kNecroImageBytes) {
		return false;
	}
	const UInt32 count = Get32(in);
	st.count = count > kNecroBossesMost ? kNecroBossesMost : count;
	st.comeBackAt = dungeon_detail::Float(Get32(in + 4));
	st.over = in[8] != 0;
	for (UInt32 b = 0; b < kNecroBossesMost; ++b) {
		const UInt8* at = in + 12 + b * kNecroBossBytes;
		NecroBoss& boss = st.bosses[b];
		boss.home = static_cast<UInt16>(at[0] | (at[1] << 8));
		boss.team = at[2];
		boss.alive = at[3] != 0 && boss.home < candidates;
		boss.bandFrom = dungeon_detail::Float(Get32(at + 4));
		boss.bandCarry = dungeon_detail::Float(Get32(at + 8));
		boss.placeFrom = dungeon_detail::Float(Get32(at + 12));
		boss.places = at[16];
	}
	const UInt8* far = in + 12 + kNecroBossesMost * kNecroBossBytes;
	const UInt32 farCount = Get32(far);
	bands.count = farCount > kNecroBandsMost ? kNecroBandsMost : farCount;
	for (UInt32 i = 0; i < bands.count; ++i) {
		const UInt8* at = far + 4 + i * kNecroBandBytes;
		NecroBand& b = bands.bands[i];
		b.boss = at[0];
		b.team = at[1];
		b.city = at[2];
		b.size = at[3];
		b.x = dungeon_detail::Float(Get32(at + 4));
		b.y = dungeon_detail::Float(Get32(at + 8));
	}
	return true;
}

}  // namespace walkers
