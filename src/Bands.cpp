#include "Bands.h"

#include <windows.h>

#include <cmath>
#include <cstdio>

#include "Camps.h"
#include "CityGates.h"
#include "GateQuestLogic.h"
#include "Engine.h"
#include "GateLogic.h"
#include "OblivionGates.h"
#include "Banish.h"
#include "Placer.h"
#include "RiseLogic.h"

namespace walkers {
namespace {

using namespace engine;

constexpr UInt32 kFormFlagsOffset = 0x08;  // TESForm::flags (GateLogic.h)
constexpr UInt32 kRefPositionOffset = 0x2C;
constexpr UInt32 kRefParentCellOffset = 0x40;  // TESObjectREFR::parentCell (Horde.cpp)
constexpr UInt32 kGameHourId = 0x00000038;  // News.cpp
constexpr UInt32 kGameDaysPassedId = 0x00000039;
constexpr UInt32 kRefBaseOffset = 0x1C;
constexpr UInt32 kNpcLevelOffset = 0x24 + 0x0E;  // TESActorBaseData's level (Camps.cpp)
constexpr UInt32 kVariantLocalId = 0x805;  // the walkers' list (Replace.cpp)
constexpr UInt32 kAnyWalkerLocalId = 0x94A;  // one walker a pick (ReplaceLogic.h kAnyWalkerLocalId)
constexpr float kStepSeconds = 1.0f;
// A member is one of ours when it stands this near the marker the second
// after the PlaceAtMe and was not there before.
constexpr float kMemberFoundUnits = 1500.0f;
constexpr UInt32 kNearMost = 64;

UInt32 g_espBase = kNoEsp;
BandState g_state;
int g_openGates = -1;
float g_clock = 0.0f;
float g_gateClock = kGateLookSeconds;  // the first look at once
double g_lastDays = -1.0;
UInt32 g_seed = 0x2545F491u;
UInt32 g_logsLeft = 400;
float g_testClock = 0.0f;
bool g_testDone = false;
float g_killClock = 0.0f;  // TestKillLeaderSeconds: since a leader was found
bool g_killDone = false;

BandMember g_members[kBandMembersMost] = {};
UInt32 g_memberCount = 0;
UInt8 g_nextBand = 0;  // the band count of the session, for its members

// A Near band's placing over three looks: the marker moved, the actors
// placed, then the new ones found.
enum class PlaceStage : UInt8 { None, MarkerMoved, Placed, LeaderPlaced };
struct Placing {
	PlaceStage stage = PlaceStage::None;
	float dx = 0.0f, dy = 0.0f;
	UInt32 size = 0;
	UInt32 list = 0;  // the list's form ID
	UInt16 gate = 0;
	bool daedra = false;  // a Daedra band gets a leader (idea 1)
	UInt8 team = 0;       // a necromancer's band (4.23 B step 4): walkers of his team, no leader
	UInt8 band = 0;
	UInt32 before[kNearMost] = {};  // the FF actors near the marker before the PlaceAtMe
	UInt32 beforeCount = 0;
	UInt32 leaderBase = 0;  // the leader's actor: only a new one of this base is taken for him
};
Placing g_placing;
TeamMemberFn g_onTeamMember = nullptr;
const BandSettings* g_lastBands = nullptr;
// Near bands waiting for the marker (one is placed at a time).
BandEvent g_queue[8];
UInt32 g_queueCount = 0;

float GlobalValue(UInt32 id) {
	const UInt32 global = LookupForm(id);
	if (!LooksLikeObject(global) || Read8(global + kFormTypeOffset) != kFormTypeGlobal) {
		return -1.0f;
	}
	return FloatAt(global + kGlobalValueOffset);
}

// The game's clock in days (GameDaysPassed counts whole days, GameHour the
// hour of the current one), -1 when not readable.
double GameDays() {
	const float days = GlobalValue(kGameDaysPassedId);
	const float hour = GlobalValue(kGameHourId);
	if (days < 0.0f || hour < 0.0f) {
		return -1.0;
	}
	return std::floor(static_cast<double>(days)) + static_cast<double>(hour) / 24.0;
}

UInt32 PlayerLevel() {
	const UInt32 player = LookupForm(0x14);
	const UInt32 base = LooksLikeObject(player) ? Read(player + kRefBaseOffset) : 0;
	if (!LooksLikeObject(base)) {
		return 1;
	}
	const SInt16 level = *reinterpret_cast<const SInt16*>(base + kNpcLevelOffset);
	return level < 1 ? 1u : static_cast<UInt32>(level);
}

// The new FF actors near the marker since `before`, made members of the band
// under way; returns how many, the last one in `last`. `onlyBase` (not 0):
// only actors of that base - the leader, not a creature that came by
// meanwhile (a MOO spawn was taken for him, daedra-leader.txt 2026-11-17).
UInt32 TakeNewMembers(UInt32 marker, bool leader, UInt32& last, UInt32 onlyBase = 0) {
	const float mx = FloatAt(marker + kRefPositionOffset);
	const float my = FloatAt(marker + kRefPositionOffset + 4);
	UInt32 now[kNearMost];
	const UInt32 n = PlacedActorsNear(mx, my, kMemberFoundUnits, now, kNearMost);
	UInt32 found = 0;
	for (UInt32 i = 0; i < n && g_memberCount < kBandMembersMost; ++i) {
		bool old = false;
		for (UInt32 k = 0; k < g_placing.beforeCount; ++k) {
			old = old || g_placing.before[k] == now[i];
		}
		if (!old && onlyBase != 0) {
			const UInt32 ref = LookupForm(now[i]);
			const UInt32 base = LooksLikeObject(ref) ? Read(ref + kRefBaseOffset) : 0;
			old = !LooksLikeObject(base) || FormIdOf(base) != onlyBase;
		}
		if (!old && !IsBandMember(now[i])) {
			g_members[g_memberCount++] = BandMember{now[i], g_placing.band, leader};
			if (g_placing.daedra) {
				WatchDaedra(now[i]);  // 4.22a step 5b: banished once dead
			}
			if (g_placing.team != 0 && g_onTeamMember != nullptr) {
				g_onTeamMember(now[i], g_placing.team);
			}
			last = now[i];
			++found;
		}
	}
	return found;
}

void NoteBefore(UInt32 marker) {
	g_placing.beforeCount = PlacedActorsNear(FloatAt(marker + kRefPositionOffset),
	                                         FloatAt(marker + kRefPositionOffset + 4), kMemberFoundUnits,
	                                         g_placing.before, kNearMost);
}

UInt32 Random() {
	g_seed ^= g_seed << 13;
	g_seed ^= g_seed >> 17;
	g_seed ^= g_seed << 5;
	return g_seed;
}

bool OpenNow(const OblivionGateRef& g) {
	const UInt32 door = LookupForm(g.door);
	if (!LooksLikeObject(door)) {
		return false;
	}
	const UInt32 parent = g.parent != 0 ? LookupForm(g.parent) : 0;
	return GateOpen(Read(door + kFormFlagsOffset), LooksLikeObject(parent),
	                LooksLikeObject(parent) ? Read(parent + kFormFlagsOffset) : 0);
}

// The list a band is placed from: always the Daedra's (4.22, 4.24: a gate's
// band is Daedra).
UInt32 BandList(const InvaderSettings& invaders) {
	return EspFormId(g_espBase, InvaderDaedraLocalId(invaders));
}

// "Close the gate" (4.22a step 6, GateQuestLogic.h).
GateQuest g_gateQuest;
UInt32 g_gateQuestLogs = 60;

UInt32 EspGlobal(UInt32 local) {
	const UInt32 global = LookupForm(EspFormId(g_espBase, local));
	return LooksLikeObject(global) && Read8(global + kFormTypeOffset) == kFormTypeGlobal ? global : 0;
}

void SetEspGlobal(UInt32 local, float value) {
	const UInt32 global = EspGlobal(local);
	if (global != 0) {
		*reinterpret_cast<float*>(global + kGlobalValueOffset) = value;
	}
}

const char* const kCityNames[] = {"Anvil", "Bravil", "Bruma", "Cheydinhal", "Chorrol", "Leyawiin", "Skingrad",
                                  "the Imperial City"};

// The quest's one target set to `gate`'s door: TESQuest::targetList (+0x48,
// a tList whose first node is inline: its data a TargetEntry), the entry's
// target reference at +0x0C (xOBSE GameForms.h, read 2026-11-16 through
// deepwiki). True when written.
bool AimQuestAt(UInt32 quest, UInt32 door, LogFn log) {
	const UInt32 entry = Read(quest + 0x48);
	if (!LooksLikeObject(entry) || !LooksLikeObject(door)) {
		log("Gate quest: no target entry (%08X) or no door (%08X) - the target stays", entry, door);
		return false;
	}
	const UInt32 before = Read(entry + 0x0C);
	*reinterpret_cast<UInt32*>(entry + 0x0C) = door;
	log("Gate quest: the target %08X -> %08X (entry %08X)", FormIdOf(before), FormIdOf(door), entry);
	return true;
}

float g_testGateClock = 0.0f;

void StepGateQuestInGame(bool* open, UInt32 n, const BandSettings& bands, const InvaderSettings& invaders, float px,
                         float py, ConsoleFn console, LogFn log) {
	if (bands.testGateOpen >= 0 && static_cast<UInt32>(bands.testGateOpen) < n) {
		g_testGateClock += kGateLookSeconds;
		open[bands.testGateOpen] = g_testGateClock <= bands.testGateOpenSeconds;
		if (g_gateQuestLogs > 0) {
			--g_gateQuestLogs;
			log("Gate quest: test - gate %d read %s (%.0f of TestGateOpenSeconds=%.0f)", bands.testGateOpen,
			    open[bands.testGateOpen] ? "open" : "shut", g_testGateClock, bands.testGateOpenSeconds);
		}
	}
	const UInt32 quest = LookupForm(EspFormId(g_espBase, kGateQuestLocalId));
	if (!LooksLikeObject(quest)) {
		return;  // an older ESP
	}
	static float xs[kBandGatesMost];
	static float ys[kBandGatesMost];
	for (UInt32 g = 0; g < n; ++g) {
		xs[g] = kOblivionGates[g].x;
		ys[g] = kOblivionGates[g].y;
	}
	GateQuestLook look;
	look.daedraInPlay = DaedraOn(invaders);
	const UInt32 heardGlobal = EspGlobal(kGateHeardGlobalLocalId);
	look.heard = heardGlobal != 0 && FloatAt(heardGlobal + kGlobalValueOffset) >= 1.0f;
	look.gateOpen = g_gateQuest.gate >= 0 && static_cast<UInt32>(g_gateQuest.gate) < n && open[g_gateQuest.gate];
	look.nearestOpen = NearestOpenGate(xs, ys, open, n, px, py);
	const GateQuestAction action = StepGateQuest(g_gateQuest, look);
	SetEspGlobal(kGateRumorGlobalLocalId, GateRumorWanted(g_gateQuest, look) ? 1.0f : 0.0f);
	const UInt32 player = LookupForm(0x14);
	char line[200];
	if (action == GateQuestAction::Start) {
		SetEspGlobal(kGateHeardGlobalLocalId, 0.0f);
		const OblivionGateRef& gate = kOblivionGates[g_gateQuest.gate];
		if ((Read8(quest + 0x3C) & kQuestCompleted) != 0) {
			console(player, "UncompleteQuest WalkersCloseTheGate");
		}
		AimQuestAt(quest, LookupForm(gate.door), log);
		console(player, "SetStage WalkersCloseTheGate 10");
		UInt32 city = 0;
		float best = -1.0f;
		for (UInt32 c = 0; c < kCityGateCount; ++c) {
			const float d = (kCityGates[c].outX - gate.x) * (kCityGates[c].outX - gate.x) +
			                (kCityGates[c].outY - gate.y) * (kCityGates[c].outY - gate.y);
			if (best < 0.0f || d < best) {
				best = d;
				city = static_cast<UInt32>(kCityGates[c].city);
			}
		}
		const CityGate* cityGate = nullptr;
		for (UInt32 c = 0; c < kCityGateCount && cityGate == nullptr; ++c) {
			cityGate = static_cast<UInt32>(kCityGates[c].city) == city ? &kCityGates[c] : nullptr;
		}
		std::snprintf(line, sizeof(line), "Message \"The Oblivion gate lies %s of %s.\"",
		              cityGate != nullptr ? CompassWord(gate.x - cityGate->outX, gate.y - cityGate->outY) : "near",
		              city < sizeof(kCityNames) / sizeof(kCityNames[0]) ? kCityNames[city] : "a city");
		console(player, line);
		log("Gate quest: begun - gate %d (%08X), %s (stage 10, the rumor heard; the quest's flags %02X, stage %u)",
		    g_gateQuest.gate, gate.door, line, Read8(quest + 0x3C), Read8(quest + 0x5C));
	} else if (action == GateQuestAction::Done) {
		console(player, "SetStage WalkersCloseTheGate 20");
		char what[96] = "";
		GiveRefugeeReward(5, what, sizeof(what));
		std::snprintf(line, sizeof(line), "Message \"The Oblivion gate is closed%s.\"", what);
		console(player, line);
		log("Gate quest: done - its gate shut (stage 20)%s; the quest's flags %02X, stage %u", what, Read8(quest + 0x3C),
		    Read8(quest + 0x5C));
	} else if (g_gateQuestLogs > 0 && look.heard && !look.daedraInPlay) {
		--g_gateQuestLogs;
		log("Gate quest: heard, but no Daedra in play - not begun");
	}
}

void Queue(const BandEvent& e) {
	if (g_queueCount < sizeof(g_queue) / sizeof(g_queue[0])) {
		g_queue[g_queueCount++] = e;
	}
}

// One look of the placing: a waiting band begun, or the one under way
// carried a stage on.
void StepPlacing(const InvaderSettings& invaders, ConsoleFn console, LogFn log) {
	const UInt32 marker = LookupForm(EspFormId(g_espBase, kBandMarkerLocalId));
	const UInt32 player = LookupForm(0x14);
	if (!LooksLikeObject(marker) || !LooksLikeObject(player)) {
		return;
	}
	char line[96];
	switch (g_placing.stage) {
		case PlaceStage::None: {
			if (g_queueCount == 0) {
				return;
			}
			const BandEvent e = g_queue[0];
			for (UInt32 i = 1; i < g_queueCount; ++i) {
				g_queue[i - 1] = g_queue[i];
			}
			--g_queueCount;
			const float px = FloatAt(player + kRefPositionOffset);
			const float py = FloatAt(player + kRefPositionOffset + 4);
			const float pz = FloatAt(player + kRefPositionOffset + 8);
			BandPlaceOffset(e.x, e.y, px, py, g_placing.dx, g_placing.dy);
			float land = 0.0f;
			const float dz = LandHeightAt(px + g_placing.dx, py + g_placing.dy, land) ? land - pz + 64.0f : 64.0f;
			g_placing.size = e.size;
			g_placing.gate = e.gate;
			g_placing.team = e.team;
			// a gate's band is Daedra; a necromancer's one walker a pick: WalkersAnyWalker
			// (the variant list 0x805 gives up to MaxCount each - 3 asked came as 15,
			// necro-band 2026-11-20)
			g_placing.list = e.team != 0 ? EspFormId(g_espBase, kAnyWalkerLocalId) : BandList(invaders);
			g_placing.daedra = e.team == 0;
			g_placing.band = g_nextBand++;
			std::snprintf(line, sizeof(line), "MoveTo player %.0f %.0f %.0f", g_placing.dx, g_placing.dy, dz);
			const bool moved = console(marker, line);
			g_placing.stage = PlaceStage::MarkerMoved;
			if (e.team != 0) {
				log("Bands: a band of %u walkers (a necromancer's, team %u) comes near - the marker %.0f %.0f from you "
				    "(\"%s\" answered %d)",
				    e.size, e.team, g_placing.dx, g_placing.dy, line, moved ? 1 : 0);
			} else {
				log("Bands: a band of %u (Daedra, gate %u) comes near - the marker %.0f %.0f from you (\"%s\" answered %d)",
				    e.size, e.gate, g_placing.dx, g_placing.dy, line, moved ? 1 : 0);
			}
			return;
		}
		case PlaceStage::MarkerMoved: {
			const float mx = FloatAt(marker + kRefPositionOffset);
			const float my = FloatAt(marker + kRefPositionOffset + 4);
			NoteBefore(marker);
			std::snprintf(line, sizeof(line), "PlaceAtMe %08X %u", g_placing.list, g_placing.size);
			const bool placed = console(marker, line);
			g_placing.stage = PlaceStage::Placed;
			log("Bands: \"%s\" at the marker (%.0f %.0f) answered %d", line, mx, my, placed ? 1 : 0);
			return;
		}
		case PlaceStage::Placed: {
			UInt32 last = 0;
			const UInt32 found = TakeNewMembers(marker, false, last);
			log("Bands: %u new members found at the marker (%u asked; %u members this session)", found,
			    g_placing.size, g_memberCount);
			if (!g_placing.daedra) {
				g_placing.stage = PlaceStage::None;
				return;
			}
			// Idea 1: a Dremora two ranks above the men leads them.
			const UInt32 level = PlayerLevel();
			const UInt32 list = LeaderActor(invaders, level, Random());  // an actor: MOO rewrites the rank lists
			g_placing.leaderBase = list;
			NoteBefore(marker);
			std::snprintf(line, sizeof(line), "PlaceAtMe %08X 1", list);
			const bool placed = console(marker, line);
			g_placing.stage = PlaceStage::LeaderPlaced;
			log("Bands: the band's leader - rank %u of 7 at level %u (\"%s\" answered %d)", LeaderRank(level) + 1,
			    level, line, placed ? 1 : 0);
			return;
		}
		case PlaceStage::LeaderPlaced: {
			UInt32 last = 0;
			const UInt32 before = g_memberCount;
			const UInt32 found = TakeNewMembers(marker, true, last, g_placing.leaderBase);
			for (UInt32 i = before; i < g_memberCount; ++i) {
				const UInt32 ref = LookupForm(g_members[i].ref);
				const UInt32 base = LooksLikeObject(ref) ? Read(ref + kRefBaseOffset) : 0;
				log("Bands: taken for the leader: %08X (base %08X)", g_members[i].ref,
				    LooksLikeObject(base) ? FormIdOf(base) : 0u);
			}
			log("Bands: %s (%08X)", found > 0 ? "the leader found at the marker" : "no leader found", last);
			g_placing.stage = PlaceStage::None;
			return;
		}
	}
}

}  // namespace

void ConfigureBands(UInt32 espBase, LogFn log) {
	g_espBase = espBase;
	log("Bands: %s (marker %08X, %u gates known)", espBase == kNoEsp ? "no ESP - none" : "ready",
	    espBase == kNoEsp ? 0u : EspFormId(espBase, kBandMarkerLocalId), kOblivionGateCount);
}

int OpenGateCount() { return g_openGates; }

bool BandsWantReplacement(const BandSettings& bands, const InvaderSettings& invaders) {
	return ReplacementOn(bands, invaders, g_openGates < 0 ? 0u : static_cast<UInt32>(g_openGates));
}

bool DaedraEverywhereNow(const BandSettings& bands, const InvaderSettings& invaders) {
	return DaedraEverywhere(bands, invaders, g_openGates < 0 ? 0u : static_cast<UInt32>(g_openGates));
}

bool IsBandMember(UInt32 refId) {
	for (UInt32 i = 0; i < g_memberCount; ++i) {
		if (g_members[i].ref == refId) {
			return true;
		}
	}
	return false;
}

UInt32 AppendBandMembers(HordeCandidate* out, UInt32 count, UInt32 most, DeadFn dead) {
	for (UInt32 i = 0; i < g_memberCount && count < most; ++i) {
		const UInt32 ref = LookupForm(g_members[i].ref);
		if (!LooksLikeObject(ref)) {
			continue;
		}
		HordeCandidate& c = out[count++];
		c.ref = ref;
		c.alive = dead == nullptr || !dead(ref);
		c.busy = false;
		c.leads = g_members[i].leader;
	}
	return count;
}

void ResetBands() {
	g_state = BandState{};
	g_gateQuest = GateQuest{};
	g_testGateClock = 0.0f;
	g_openGates = -1;
	g_clock = 0.0f;
	g_lastDays = -1.0;
	g_memberCount = 0;
	g_nextBand = 0;
	g_placing = Placing{};
	g_queueCount = 0;
	g_testClock = 0.0f;
	g_testDone = false;
	g_killClock = 0.0f;
	g_killDone = false;
	g_gateClock = kGateLookSeconds;
}

UInt32 SiegePressureOf(UInt32 city) { return SiegePressure(g_state, city); }

void QueueWalkerBand(float x, float y, UInt32 size, UInt8 team) {
	BandEvent e{BandEventKind::Near, 0, 0, static_cast<UInt8>(size > 255 ? 255 : size), x, y};
	e.team = team == 0 ? 1 : team;
	Queue(e);
}

void AddCityPressureNow(UInt8 city) {
	if (g_lastBands != nullptr) {
		AddSiegePressure(g_state, *g_lastBands, city);
	}
}

void ConfigureBandsTeams(TeamMemberFn onMember) { g_onTeamMember = onMember; }

UInt32 BandsImage(UInt8* out, UInt32 capacity) { return BandImage(g_state, g_members, g_memberCount, out, capacity); }

bool ReadBandsImage(const UInt8* in, UInt32 length, UInt32 version, bool (*resolve)(UInt32, UInt32&)) {
	const bool whole = ReadBandImage(g_state, g_members, g_memberCount, in, length, version, resolve);
	g_nextBand = 0;
	for (UInt32 i = 0; i < g_memberCount; ++i) {
		g_nextBand = g_members[i].band >= g_nextBand ? static_cast<UInt8>(g_members[i].band + 1) : g_nextBand;
	}
	return whole;
}

UInt32 FarBandCount() { return g_state.count; }

UInt32 GateQuestImageNow(UInt8* out, UInt32 capacity) { return GateQuestImage(g_gateQuest, out, capacity); }
void ReadGateQuestNow(const UInt8* in, UInt32 length) { ReadGateQuestImage(g_gateQuest, in, length, kOblivionGateCount); }
int GateQuestGate() { return g_gateQuest.gate; }
UInt32 BandMemberCount() { return g_memberCount; }

namespace {
// Idea 1: a band whose leader is dead breaks up - its men are no longer led
// (the horde lets them go), each to its own AI. Members gone from the game
// are dropped.
void BreakUpLeaderless(DeadFn dead, LogFn log) {
	for (UInt32 i = 0; i < g_memberCount; ++i) {
		if (!g_members[i].leader) {
			continue;
		}
		const UInt32 ref = LookupForm(g_members[i].ref);
		if (LooksLikeObject(ref) && (dead == nullptr || !dead(ref))) {
			continue;
		}
		const UInt8 band = g_members[i].band;
		UInt32 kept = 0;
		UInt32 freed = 0;
		for (UInt32 k = 0; k < g_memberCount; ++k) {
			if (g_members[k].band == band) {
				++freed;
			} else {
				g_members[kept++] = g_members[k];
			}
		}
		g_memberCount = kept;
		log("Bands: band %u's leader %08X is dead - %u of them break up", band, g_members[i].ref, freed);
		return;  // the list changed: the next look goes on
	}
}
}  // namespace

namespace {
// [Daedra] TestKillLeaderSeconds (BandLogic.h): the first living leader
// killed by console that many seconds after he was found.
void StepTestKillLeader(float elapsed, const BandSettings& bands, ConsoleFn console, LogFn log) {
	if (bands.testKillLeaderSeconds <= 0.0f || g_killDone) {
		return;
	}
	for (UInt32 i = 0; i < g_memberCount; ++i) {
		if (!g_members[i].leader) {
			continue;
		}
		g_killClock += elapsed;
		if (!TestKillLeaderDue(bands.testKillLeaderSeconds, g_killClock, g_killDone)) {
			return;
		}
		g_killDone = true;
		const UInt32 ref = LookupForm(g_members[i].ref);
		const bool killed = LooksLikeObject(ref) && console(ref, "Kill");
		log("Bands: TestKillLeaderSeconds=%.0f - band %u's leader %08X killed by console (answered %d)",
		    bands.testKillLeaderSeconds, g_members[i].band, g_members[i].ref, killed ? 1 : 0);
		return;
	}
}
}  // namespace

void StepBandsInGame(float dt, const BandSettings& bands, const InvaderSettings& invaders, ConsoleFn console,
                     DeadFn dead, LogFn log) {
	if (g_espBase == kNoEsp) {
		return;
	}
	g_lastBands = &bands;
	g_clock += dt;
	if (g_clock < kStepSeconds) {
		return;
	}
	const float elapsed = g_clock;
	g_clock = 0.0f;
	const UInt32 player = LookupForm(0x14);
	// In a game only: at the main menu the gates' flags are the file's, not
	// a save's (daedra-band.txt 2026-11-16: gate 0 read open there, closed
	// after the load) - the player has no cell then.
	if (!LooksLikeObject(player) || !LooksLikeObject(Read(player + kRefParentCellOffset))) {
		return;
	}
	const float px = FloatAt(player + kRefPositionOffset);
	const float py = FloatAt(player + kRefPositionOffset + 4);

	if (bands.testBandSeconds > 0.0f && !g_testDone) {
		g_testClock += elapsed;
		if (g_testClock >= bands.testBandSeconds) {
			g_testDone = true;
			const BandEvent e{BandEventKind::Near, 0, 0, static_cast<UInt8>(BandSize(bands.sizeMin, bands.sizeMax,
			                                                                         Random())),
			                  px, py + kBandPlaceLeastUnits};
			Queue(e);
			log("Bands: TestBandSeconds=%.0f - a band of %u placed before you, whatever the gates",
			    bands.testBandSeconds, e.size);
		}
	}

	StepTestKillLeader(elapsed, bands, console, log);

	g_gateClock += elapsed;
	const bool gateQuest = DaedraOn(invaders) || g_gateQuest.gate >= 0;
	static bool s_rumorCleared = false;
	if (!gateQuest && !s_rumorCleared) {
		SetEspGlobal(kGateRumorGlobalLocalId, 0.0f);  // Mode 0, or the Daedra stopped: the rumor no more
	}
	s_rumorCleared = !gateQuest;
	if (gateQuest && g_gateClock >= kGateLookSeconds) {
		g_gateClock = 0.0f;
		static bool open[kBandGatesMost];
		static BandGate gates[kBandGatesMost];
		UInt32 count = 0;
		const UInt32 n = kOblivionGateCount < kBandGatesMost ? kOblivionGateCount : kBandGatesMost;
		for (UInt32 g = 0; g < n; ++g) {
			open[g] = OpenNow(kOblivionGates[g]);
			gates[g] = BandGate{kOblivionGates[g].x, kOblivionGates[g].y};
			count += open[g] ? 1 : 0;
		}
		if (static_cast<int>(count) != g_openGates && g_logsLeft > 0) {
			--g_logsLeft;
			log("Bands: %u of %u Oblivion gates open (were %d) - the spawn replacement %s", count, n, g_openGates,
			    ReplacementOn(bands, invaders, count) ? "on" : "off");
		}
		g_openGates = static_cast<int>(count);
		StepGateQuestInGame(open, n, bands, invaders, px, py, console, log);
		if (!BandsOn(invaders, count)) {
			BreakUpLeaderless(dead, log);
			StepPlacing(invaders, console, log);
			return;  // the gates read for the quest only: no bands
		}
		const double days = GameDays();
		float hours = 0.0f;
		if (days >= 0.0 && g_lastDays >= 0.0 && days > g_lastDays) {
			hours = static_cast<float>((days - g_lastDays) * 24.0);
		}
		g_lastDays = days;
		BandCity cities[kCityGateCount];
		for (UInt32 c = 0; c < kCityGateCount; ++c) {
			cities[c] = BandCity{kCityGates[c].outX, kCityGates[c].outY, static_cast<UInt8>(kCityGates[c].city)};
		}
		BandEvent events[kBandEventsMost];
		const UInt32 e = StepBands(g_state, bands, gates, open, n, cities, kCityGateCount, hours, px, py, Random(),
		                           events, kBandEventsMost);
		for (UInt32 i = 0; i < e; ++i) {
			const BandEvent& ev = events[i];
			if (ev.kind == BandEventKind::Near) {
				Queue(ev);
			}
			if (g_logsLeft > 0) {
				--g_logsLeft;
				const char* what = ev.kind == BandEventKind::Formed    ? "formed at"
				                   : ev.kind == BandEventKind::Near    ? "comes near you from"
				                   : ev.kind == BandEventKind::Arrived ? "reached its city from"
				                                                       : "ended - its gate closed:";
				log("Bands: a band of %u %s gate %u (%08X), heading for %s; %u far bands", ev.size, what, ev.gate,
				    ev.gate < n ? kOblivionGates[ev.gate].door : 0u, kCityGates[ev.city].name, g_state.count);
			}
		}
	}
	BreakUpLeaderless(dead, log);
	StepPlacing(invaders, console, log);
}

}  // namespace walkers
