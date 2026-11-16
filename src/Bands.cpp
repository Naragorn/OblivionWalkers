#include "Bands.h"

#include <windows.h>

#include <cmath>
#include <cstdio>

#include "CityGates.h"
#include "Engine.h"
#include "GateLogic.h"
#include "OblivionGates.h"
#include "RiseLogic.h"

namespace walkers {
namespace {

using namespace engine;

constexpr UInt32 kFormFlagsOffset = 0x08;  // TESForm::flags (GateLogic.h)
constexpr UInt32 kRefPositionOffset = 0x2C;
constexpr UInt32 kRefParentCellOffset = 0x40;  // TESObjectREFR::parentCell (Horde.cpp)
constexpr UInt32 kVtblCharacter = 0x00A6FC9C;  // Rise.cpp
constexpr UInt32 kVtblCreature = 0x00A710F4;
constexpr UInt32 kActorProcessManager = 0x00B3BD00;
constexpr UInt32 kActorListByLevel = 0x00673A50;
constexpr UInt32 kGameHourId = 0x00000038;  // News.cpp
constexpr UInt32 kGameDaysPassedId = 0x00000039;
constexpr UInt32 kRefBaseOffset = 0x1C;
constexpr UInt32 kNpcLevelOffset = 0x24 + 0x0E;  // TESActorBaseData's level (Camps.cpp)
constexpr UInt32 kVariantLocalId = 0x805;  // the walkers' list (Replace.cpp)
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
	UInt8 band = 0;
	UInt32 before[kNearMost] = {};  // the FF actors near the marker before the PlaceAtMe
	UInt32 beforeCount = 0;
};
Placing g_placing;
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

UInt32 PlacedActorsNear(float x, float y, float units, UInt32* out, UInt32 most);

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
// under way; returns how many, the last one in `last`.
UInt32 TakeNewMembers(UInt32 marker, bool leader, UInt32& last) {
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
		if (!old && !IsBandMember(now[i])) {
			g_members[g_memberCount++] = BandMember{now[i], g_placing.band, leader};
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

// The FF actors (placed at run time) within `units` of x, y on the high
// process list.
UInt32 PlacedActorsNear(float x, float y, float units, UInt32* out, UInt32 most) {
	auto* const manager = reinterpret_cast<void*>(kActorProcessManager);
	using ListFn = void*(__fastcall*)(void* self, void* edx, UInt32 level);
	struct Node {
		void* data;
		Node* next;
	};
	UInt32 n = 0;
	auto* node = static_cast<Node*>(reinterpret_cast<ListFn>(kActorListByLevel)(manager, nullptr, 0));
	for (UInt32 visited = 0; node != nullptr && visited < 1024 && n < most; ++visited) {
		if (!LooksLikeObject(reinterpret_cast<UInt32>(node))) {
			break;
		}
		const UInt32 actor = reinterpret_cast<UInt32>(node->data);
		node = node->next;
		if (!LooksLikeObject(actor) || (Read(actor) != kVtblCreature && Read(actor) != kVtblCharacter)) {
			continue;
		}
		const UInt32 id = Read(actor + kFormIdOffset);
		if ((id >> 24) != 0xFF) {
			continue;
		}
		const float dx = FloatAt(actor + kRefPositionOffset) - x;
		const float dy = FloatAt(actor + kRefPositionOffset + 4) - y;
		if (dx * dx + dy * dy <= units * units) {
			out[n++] = id;
		}
	}
	return n;
}

// The list a band of `group` is placed from.
UInt32 BandList(Group group, const InvaderSettings& invaders) {
	const UInt32 local = group == Group::Daedra ? InvaderDaedraLocalId(invaders) : kVariantLocalId;
	return EspFormId(g_espBase, local);
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
			const Group group = ReplacementGroup(invaders, Random() % 100);
			g_placing.size = e.size;
			g_placing.gate = e.gate;
			g_placing.list = BandList(group, invaders);
			g_placing.daedra = group == Group::Daedra;
			g_placing.band = g_nextBand++;
			std::snprintf(line, sizeof(line), "MoveTo player %.0f %.0f %.0f", g_placing.dx, g_placing.dy, dz);
			const bool moved = console(marker, line);
			g_placing.stage = PlaceStage::MarkerMoved;
			log("Bands: a band of %u (%s, gate %u) comes near - the marker %.0f %.0f from you (\"%s\" answered %d)",
			    e.size, group == Group::Daedra ? "Daedra" : "walkers", e.gate, g_placing.dx, g_placing.dy, line,
			    moved ? 1 : 0);
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
			const UInt32 list = LeaderList(invaders, level);
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
			const UInt32 found = TakeNewMembers(marker, true, last);
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

bool BandsWantReplacement(const BandSettings& bands) {
	return ReplacementOn(bands, g_openGates < 0 ? 0u : static_cast<UInt32>(g_openGates));
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
	g_openGates = -1;
	g_clock = 0.0f;
	g_lastDays = -1.0;
	g_memberCount = 0;
	g_nextBand = 0;
	g_placing = Placing{};
	g_queueCount = 0;
	g_testClock = 0.0f;
	g_testDone = false;
	g_gateClock = kGateLookSeconds;
}

UInt32 SiegePressureOf(UInt32 city) { return SiegePressure(g_state, city); }

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

void StepBandsInGame(float dt, const BandSettings& bands, const InvaderSettings& invaders, ConsoleFn console,
                     DeadFn dead, LogFn log) {
	if (g_espBase == kNoEsp) {
		return;
	}
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

	g_gateClock += elapsed;
	if (bands.origin == InvaderOrigin::Gates && g_gateClock >= kGateLookSeconds) {
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
			    ReplacementOn(bands, count) ? "on" : "off");
		}
		g_openGates = static_cast<int>(count);
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
