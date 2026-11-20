#include "Necro.h"

#include <cmath>
#include <cstdio>
#include <unordered_map>

#include "Bands.h"
#include "CityGates.h"
#include "DaedricDisguise.h"
#include "DungeonCandidates.h"
#include "Dungeons.h"
#include "Engine.h"
#include "Horde.h"
#include "News.h"

namespace walkers {
using namespace engine;
namespace {

const NecroSettings* g_settings = nullptr;
const InvaderSettings* g_invaders = nullptr;
NecroRollFn g_roll = nullptr;
NecroConsoleFn g_console = nullptr;
NecroDeadFn g_dead = nullptr;
NecroIsWalkerFn g_isWalker = nullptr;
// Step 5: the team of every necromancer's walker known (its form ID): the
// bands' members as placed, the pool's while it stands at one of their places.
std::unordered_map<UInt32, UInt8> g_teams;
UInt32 g_poolTeamed[16] = {};
UInt32 g_poolTeamedCount = 0;
float g_teamClock = 0.0f;
UInt32 g_teamLogsLeft = 40;
LogFn g_log = nullptr;
char g_testHome[64] = {};
NecroState g_state;
NecroBands g_bands;
float g_lastDays = -1.0f;  // the game day of the bands' last step
float g_clock = 0.0f;
bool g_waitLogged = false;
bool g_testLogged = false;
constexpr float kStepSeconds = 1.0f;
constexpr UInt32 kEventsMost = 32;
UInt32 g_bandLogsLeft = 60;
// Far bands this near each other meet (a guess: within sight of each other).
constexpr float kNecroBandsMeetUnits = 3000.0f;
// [Necromancers] TestBandsSeconds (INI only, a test switch; harness
// necro-teams.txt): this many seconds after the pool settled two bands of 3,
// of the first two bosses' teams (or 1 and 2), come near the player from the
// same side - they meet. 0: off.
float g_testBandsSeconds = 0.0f;
float g_testBandsClock = 0.0f;
bool g_testBandsDone = false;

// The cities the bands head for: the gates' outside spots (CityGates.h), as
// the gates' bands.
NecroCity g_cities[kCityGateCount];
void FillCities() {
	for (UInt32 c = 0; c < kCityGateCount; ++c) {
		g_cities[c] = NecroCity{kCityGates[c].outX, kCityGates[c].outY};
	}
}

void BandLog(LogFn log, const char* what, const NecroBand& b) {
	if (g_bandLogsLeft == 0) {
		return;
	}
	--g_bandLogsLeft;
	log("Necromancers: a band of %u walkers (boss %u, team %u) %s, heading for %s; %u far bands", b.size, b.boss,
	    b.team, what, kCityGates[b.city < kCityGateCount ? b.city : 0].name, g_bands.count);
}

void CarryBandEvents(const NecroBandEvent* ev, UInt32 n, LogFn log) {
	for (UInt32 k = 0; k < n && k < kEventsMost; ++k) {
		const NecroBand& b = ev[k].band;
		switch (ev[k].kind) {
			case NecroBandEventKind::Formed:
				BandLog(log, "left its home", b);
				break;
			case NecroBandEventKind::Near:
				QueueWalkerBand(b.x, b.y, b.size, b.team);
				BandLog(log, "comes near you - into the world", b);
				break;
			case NecroBandEventKind::Arrived:
				AddCityPressureNow(static_cast<UInt8>(kCityGates[b.city < kCityGateCount ? b.city : 0].city));
				BandLog(log, "reached its city - the city's siege pressure up", b);
				break;
			case NecroBandEventKind::Dropped:
				BandLog(log, "dropped - the most are on their way", b);
				break;
			case NecroBandEventKind::Met:
				BandLog(log, "met an enemy band - fought as numbers, now this size", b);
				break;
			case NecroBandEventKind::Destroyed:
				BandLog(log, "destroyed by an enemy band", b);
				break;
		}
	}
}

struct Roll {
	UInt32 operator()() { return g_roll != nullptr ? g_roll() : 0u; }
};


// The settings as they stand now: on with the switch.
NecroSettings Now() {
	NecroSettings s = g_settings != nullptr ? *g_settings : NecroSettings{};
	s.on = g_invaders != nullptr && g_invaders->necromancers;
	return s;
}

int TestHomeCandidate() {
	if (g_testHome[0] == '\0') {
		return -1;
	}
	for (UInt32 c = 0; c < kDungeonCandidateCount; ++c) {
		if (SameName(kDungeonCandidates[c].name, g_testHome)) {
			return static_cast<int>(c);
		}
	}
	return -1;
}

void LogEvents(const NecroEvent* ev, UInt32 n, LogFn log) {
	for (UInt32 k = 0; k < n && k < kEventsMost; ++k) {
		const NecroEvent& e = ev[k];
		const NecroBoss& b = g_state.bosses[e.boss < kNecroBossesMost ? e.boss : 0];
		switch (e.kind) {
			case NecroEventKind::Fell:
				log("Necromancers: boss %u (team %u) fell - his realm stops: no more bands or places; his %u place(s) "
				    "stay full of walkers until cleared",
				    e.boss, b.team, b.places);
				break;
			case NecroEventKind::AllFell:
				log("Necromancers: the last of them fell - %s", g_state.over ? "it is over (ComeBack=0)" : "a new set rises later");
				if (!g_state.over) {
					log("Necromancers: a new set rises at game day %.2f", g_state.comeBackAt);
				}
				break;
			default:
				break;
		}
	}
}

void OnBossFell(UInt32 boss, UInt32 candidate) {
	if (g_log == nullptr) {
		return;
	}
	NecroEvent ev[4];
	const UInt32 n = NecroBossFell(g_state, Now(), boss, GameDaysNow(), ev, 4);
	g_log("Necromancers: the necromancer of %s (boss %u) is dead%s", DungeonCandidateName(candidate), boss,
	      n == 0 ? " - he was down already" : "");
	LogEvents(ev, n, g_log);
}

void OnPlaceGone(UInt32 boss, UInt32 candidate, bool won) {
	NecroPlaceLost(g_state, boss);
	if (g_log != nullptr) {
		g_log("Necromancers: %s is no longer boss %u's - %s; he holds %u place(s) now", DungeonCandidateName(candidate),
		      boss, won ? "cleared, a refuge won" : "given up (a quest uses it)",
		      boss < kNecroBossesMost ? g_state.bosses[boss].places : 0u);
	}
}

void NoteTeam(UInt32 refId, UInt8 team) {
	if (team == 0) {
		g_teams.erase(refId);
	} else {
		g_teams[refId] = team;
	}
}

UInt8 BossTeam(int boss) {
	return boss >= 0 && static_cast<UInt32>(boss) < g_state.count && boss < static_cast<int>(kNecroBossesMost)
	           ? g_state.bosses[boss].team
	           : 0;
}

bool IsHome(UInt32 boss, UInt32 candidate) {
	return boss < g_state.count && boss < kNecroBossesMost && g_state.bosses[boss].home == candidate;
}

}  // namespace

void ConfigureNecro(const NecroSettings* settings, const InvaderSettings* invaders, NecroRollFn roll,
                    NecroConsoleFn console, NecroDeadFn dead, NecroIsWalkerFn isWalker, const char* testHome,
                    float testBandsSeconds, LogFn log) {
	g_testBandsSeconds = testBandsSeconds;
	g_settings = settings;
	g_invaders = invaders;
	g_roll = roll;
	g_console = console;
	g_dead = dead;
	g_isWalker = isWalker;
	ConfigureBandsTeams(&NoteTeam);
	g_log = log;
	std::snprintf(g_testHome, sizeof(g_testHome), "%s", testHome != nullptr ? testHome : "");
	ConfigureDungeonsNecro(&OnBossFell, &OnPlaceGone, &IsHome);
}

NecroState& CurrentNecro() { return g_state; }
NecroBands& CurrentNecroBands() { return g_bands; }

void ResetNecroInGame() {
	g_state = NecroState{};
	g_bands = NecroBands{};
	g_teams.clear();  // the bands' members went with the load (Bands.cpp keeps its own)
	g_testBandsClock = 0.0f;
	g_testBandsDone = false;
	g_poolTeamedCount = 0;
	g_lastDays = -1.0f;
	g_clock = kStepSeconds;  // the first step at once
	g_waitLogged = false;
	g_testLogged = false;
}

void StepNecroInGame(float dt, LogFn log) {
	g_log = log;
	g_clock += dt;
	if (g_clock < kStepSeconds) {
		return;
	}
	g_clock = 0.0f;
	NecroSettings s = Now();
	if (!s.on) {
		return;
	}
	const float now = GameDaysNow();
	if (!(now > 0.0f)) {
		return;
	}
	if (!DungeonPoolSettledNow()) {
		if (!g_waitLogged) {
			log("Necromancers: waiting for the dungeon pool to settle after the load");
			g_waitLogged = true;
		}
		return;
	}
	FillCities();
	float px = 0.0f;
	float py = 0.0f;
	const bool playerKnown = DungeonPlayerPlace(px, py);
	if (g_testBandsSeconds > 0.0f && !g_testBandsDone && playerKnown) {
		g_testBandsClock += kStepSeconds;
		if (g_testBandsClock >= g_testBandsSeconds) {
			g_testBandsDone = true;
			const UInt8 a = g_state.count > 0 ? g_state.bosses[0].team : 1;
			const UInt8 b = g_state.count > 1 ? g_state.bosses[1].team : static_cast<UInt8>(a == 1 ? 2 : 1);
			QueueWalkerBand(px + 10000.0f, py, 3, a);
			QueueWalkerBand(px + 10000.0f, py, 3, b);
			log("Necromancers: test - TestBandsSeconds=%.0f: two bands of 3, teams %u and %u, near you from the same side",
			    g_testBandsSeconds, a, b);
		}
	}
	if (!playerKnown) {
		px = py = 1.0e9f;  // nowhere near: no band comes into the world
	}
	// The far bands walk on.
	{
		const float hours = g_lastDays >= 0.0f && now > g_lastDays ? (now - g_lastDays) * 24.0f : 0.0f;
		g_lastDays = now;
		NecroBandEvent bev[kEventsMost];
		const UInt32 bn = StepNecroBands(g_bands, s, g_cities, kCityGateCount, hours, px, py, bev, kEventsMost);
		CarryBandEvents(bev, bn, log);
		Roll meetRoll;
		const UInt32 mn = NecroBandsMeet(g_bands, kNecroBandsMeetUnits, meetRoll, bev, kEventsMost);
		CarryBandEvents(bev, mn, log);
	}
	bool free[kDungeonCandidateCount];
	DungeonFreePlaces(free);
	// [Necromancers] TestHome: one boss, at that place, for the first set.
	const int test = g_state.count == 0 && !g_state.over ? TestHomeCandidate() : -1;
	if (test >= 0) {
		for (UInt32 c = 0; c < kDungeonCandidateCount; ++c) {
			free[c] = free[c] && static_cast<int>(c) == test;
		}
		s.bossesMin = 1;
		s.bossesMax = 1;
		if (!g_testLogged) {
			log("Necromancers: test - TestHome=%s: one boss there (%s)", kDungeonCandidates[test].name,
			    free[test] ? "free" : "not free yet - a site holds it or it is out of the pool");
			g_testLogged = free[test];
		}
	}
	Roll roll;
	NecroEvent ev[kEventsMost];
	const UInt32 n = StepNecromancers(g_state, s, kDungeonCandidates, kDungeonCandidateCount, free, now, roll, ev,
	                                  kEventsMost);
	for (UInt32 k = 0; k < n && k < kEventsMost; ++k) {
		const NecroEvent& e = ev[k];
		NecroBoss& b = g_state.bosses[e.boss < kNecroBossesMost ? e.boss : 0];
		switch (e.kind) {
			case NecroEventKind::CameBack:
				log("Necromancers: they come back - a new set rises");
				break;
			case NecroEventKind::Rose: {
				const UInt32 walkers = NecroHotspotWalkers(s, roll());
				const bool placed = AddNecroPlace(e.place, e.boss, walkers, now);
				log("Necromancers: boss %u (team %u) rose at %s (%s) - %u walkers in it%s", e.boss, b.team,
				    DungeonCandidateName(e.place), DungeonKindName(kDungeonCandidates[e.place].kind), walkers,
				    placed ? "" : " - no site free for it: his realm is numbers only");
				break;
			}
			case NecroEventKind::PlaceDue: {
				const UInt32 walkers = NecroHotspotWalkers(s, roll());
				if (AddNecroPlace(e.place, e.boss, walkers, now)) {
					NecroPlaceTaken(g_state, e.boss);
					log("Necromancers: boss %u took %s - an under-boss and %u walkers in it; %u place(s) now", e.boss,
					    DungeonCandidateName(e.place), walkers, b.places);
				} else {
					log("Necromancers: boss %u would take %s - no site free for it", e.boss, DungeonCandidateName(e.place));
				}
				break;
			}
			case NecroEventKind::BandDue: {
				const DungeonCandidate& home = kDungeonCandidates[e.place < kDungeonCandidateCount ? e.place : 0];
				NecroBandEvent bev[4];
				const UInt32 bn = NecroBandFormed(g_bands, s, e.boss, b.team, NecroBandSize(s, roll()), home.x, home.y,
				                                  g_cities, kCityGateCount, px, py, bev, 4);
				CarryBandEvents(bev, bn, log);
				break;
			}
			default:
				break;
		}
	}
}

namespace {

constexpr UInt32 kRefPositionOffset = 0x2C;
constexpr UInt32 kRefParentCellOffset = 0x40;
constexpr UInt32 kPlayerId = 0x14;
constexpr UInt32 kFightersMost = 64;

struct Fighter {
	UInt32 id;
	UInt32 actor;
	UInt8 team;
	bool boss;
};

float Distance(UInt32 a, UInt32 b) {
	if (Read(a + kRefParentCellOffset) == 0 || Read(b + kRefParentCellOffset) == 0) {
		return -1.0f;
	}
	const float dx = FloatAt(a + kRefPositionOffset) - FloatAt(b + kRefPositionOffset);
	const float dy = FloatAt(a + kRefPositionOffset + 4) - FloatAt(b + kRefPositionOffset + 4);
	const float dz = FloatAt(a + kRefPositionOffset + 8) - FloatAt(b + kRefPositionOffset + 8);
	return std::sqrt(dx * dx + dy * dy + dz * dz);
}

// The team of the one `targetId`: its team, a necromancer's; `known` false
// when it is neither a team's walker, a necromancer placed nor a free walker
// (a living one, a Daedra: fought as ever).
UInt8 TeamOfTarget(UInt32 targetId, const Fighter* f, UInt32 n, bool& known) {
	for (UInt32 k = 0; k < n; ++k) {
		if (f[k].id == targetId) {
			known = true;
			return f[k].team;
		}
	}
	const auto it = g_teams.find(targetId);
	if (it != g_teams.end()) {
		known = true;
		return it->second;
	}
	const UInt32 target = LookupForm(targetId);
	known = g_isWalker != nullptr && g_isWalker(target);
	return 0;
}

}  // namespace

bool NecroTeamed(UInt32 refId) {
	if (g_teams.find(refId) != g_teams.end()) {
		return true;
	}
	int boss = -1;
	const UInt32 placed = NecroBossPlacedNow(boss);
	return placed != 0 && placed == refId;
}

void StepNecroTeams(float dt, LogFn log) {
	g_teamClock += dt;
	if (g_teamClock < 0.5f) {
		return;
	}
	g_teamClock = 0.0f;
	if (g_invaders == nullptr || !g_invaders->necromancers || g_console == nullptr) {
		return;
	}
	// The pool's walkers carry the team of the place they stand at.
	{
		UInt32 refs[16];
		int boss = -1;
		const UInt32 n = DungeonPoolAtNecro(refs, 16, boss);
		const UInt8 team = BossTeam(boss);
		for (UInt32 k = 0; k < g_poolTeamedCount; ++k) {
			NoteTeam(g_poolTeamed[k], 0);
		}
		g_poolTeamedCount = 0;
		for (UInt32 k = 0; k < n && team != 0; ++k) {
			NoteTeam(refs[k], team);
			g_poolTeamed[g_poolTeamedCount++] = refs[k];
		}
	}
	Fighter f[kFightersMost];
	UInt32 n = 0;
	for (auto it = g_teams.begin(); it != g_teams.end() && n < kFightersMost;) {
		const UInt32 actor = LookupForm(it->first);
		if (!LooksLikeObject(actor)) {
			it = g_teams.erase(it);  // gone from the game
			continue;
		}
		if (Read(actor + kRefParentCellOffset) != 0 && (g_dead == nullptr || !g_dead(actor))) {
			f[n++] = Fighter{it->first, actor, it->second, false};
		}
		++it;
	}
	int boss = -1;
	const UInt32 bossId = NecroBossPlacedNow(boss);
	const UInt32 bossActor = bossId != 0 ? LookupForm(bossId) : 0;
	if (LooksLikeObject(bossActor) && n < kFightersMost && (g_dead == nullptr || !g_dead(bossActor))) {
		f[n++] = Fighter{bossId, bossActor, BossTeam(boss), true};
	}
	const bool daedraAllies = g_invaders->daedra == DaedraMode::Allies;
	UInt32 stopped = 0;
	UInt32 set = 0;
	for (UInt32 i = 0; i < n; ++i) {
		const bool fighting = ActorInCombat(f[i].id);
		const UInt32 target = fighting ? ActorCombatTargetId(f[i].id) : 0;
		if (fighting && target != 0 && target != kPlayerId) {
			bool known = false;
			const UInt8 theirs = TeamOfTarget(target, f, n, known);
			const bool daedra = !known && f[i].boss && daedraAllies && IsDaedraActor(LookupForm(target));
			if (((known && NecroFightStops(f[i].team, theirs)) || daedra) && g_console(f[i].actor, "StopCombat")) {
				++stopped;
			}
			continue;
		}
		if (fighting || f[i].boss) {
			continue;
		}
		// Not fighting: the nearest enemy team's within reach.
		int best = -1;
		float bestDistance = kNecroSetOnUnits;
		for (UInt32 k = 0; k < n; ++k) {
			const float d = k == i ? -1.0f : Distance(f[i].actor, f[k].actor);
			if (NecroSetOn(f[i].team, f[k].team, false, d, bestDistance)) {
				best = static_cast<int>(k);
				bestDistance = d;
			}
		}
		if (best >= 0) {
			char line[32];
			std::snprintf(line, sizeof(line), "StartCombat %08X", f[best].id);
			if (g_console(f[i].actor, line)) {
				++set;
			}
		}
	}
	if ((stopped > 0 || set > 0) && g_teamLogsLeft > 0) {
		--g_teamLogsLeft;
		log("Necromancers: teams - %u of theirs near; %u told StopCombat (an ally, a free walker or a Daedra of "
		    "Daedra Mode 2), %u set on an enemy team's",
		    n, stopped, set);
	}
}

}  // namespace walkers
