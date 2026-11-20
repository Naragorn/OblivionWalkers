#include "Necro.h"

#include <cstdio>

#include "DungeonCandidates.h"
#include "Dungeons.h"
#include "News.h"

namespace walkers {
namespace {

const NecroSettings* g_settings = nullptr;
const InvaderSettings* g_invaders = nullptr;
NecroRollFn g_roll = nullptr;
LogFn g_log = nullptr;
char g_testHome[64] = {};
NecroState g_state;
float g_clock = 0.0f;
bool g_waitLogged = false;
bool g_testLogged = false;
UInt32 g_bandLogsLeft = 40;

struct Roll {
	UInt32 operator()() { return g_roll != nullptr ? g_roll() : 0u; }
};

constexpr float kStepSeconds = 1.0f;
constexpr UInt32 kEventsMost = 32;

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

bool IsHome(UInt32 boss, UInt32 candidate) {
	return boss < g_state.count && boss < kNecroBossesMost && g_state.bosses[boss].home == candidate;
}

}  // namespace

void ConfigureNecro(const NecroSettings* settings, const InvaderSettings* invaders, NecroRollFn roll,
                    const char* testHome, LogFn log) {
	g_settings = settings;
	g_invaders = invaders;
	g_roll = roll;
	g_log = log;
	std::snprintf(g_testHome, sizeof(g_testHome), "%s", testHome != nullptr ? testHome : "");
	ConfigureDungeonsNecro(&OnBossFell, &OnPlaceGone, &IsHome);
}

NecroState& CurrentNecro() { return g_state; }

void ResetNecroInGame() {
	g_state = NecroState{};
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
			case NecroEventKind::BandDue:
				if (g_bandLogsLeft > 0) {
					--g_bandLogsLeft;
					log("Necromancers: boss %u's band is due (%.2f a day now) - the bands march with build step 4",
					    e.boss, NecroBandsPerDay(s, g_state));
				}
				break;
			default:
				break;
		}
	}
}

}  // namespace walkers
