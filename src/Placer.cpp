#include "Placer.h"

#include <cstdio>

#include "BanishLogic.h"
#include "Bytes.h"
#include "Engine.h"
#include "RiseLogic.h"

namespace walkers {
namespace {

using namespace engine;

constexpr UInt32 kRefPositionOffset = 0x2C;
constexpr UInt32 kVtblCharacter = 0x00A6FC9C;  // Rise.cpp
constexpr UInt32 kVtblCreature = 0x00A710F4;
constexpr UInt32 kActorProcessManager = 0x00B3BD00;
constexpr UInt32 kActorListByLevel = 0x00673A50;
constexpr float kStepSeconds = 1.0f;
constexpr UInt32 kNearMost = 64;
constexpr UInt32 kLiveMost = 128;

UInt32 g_espBase = kNoEsp;
ConsoleFn g_console = nullptr;
LogFn g_log = nullptr;
PlacedFn g_handlers[4] = {};
PlaceQueue g_queue;
float g_clock = 0.0f;
UInt32 g_logsLeft = 300;

enum class Stage : UInt8 { None, MarkerMoved, Placed };
Stage g_stage = Stage::None;
PlaceRequest g_current;
UInt32 g_before[kNearMost] = {};
UInt32 g_beforeCount = 0;

// The placed ones still in the world (the co-save's), and those on their
// way out.
UInt32 g_live[kLiveMost] = {};
UInt32 g_liveCount = 0;
RetireList g_retire;

void Log(const char* format, UInt32 a = 0, UInt32 b = 0, UInt32 c = 0, UInt32 d = 0) {
	if (g_log != nullptr && g_logsLeft > 0) {
		--g_logsLeft;
		g_log(format, a, b, c, d);
	}
}

const char* OwnerName(PlaceOwner o) {
	switch (o) {
		case PlaceOwner::Siege: return "siege";
		case PlaceOwner::Camp: return "camp";
		case PlaceOwner::Dungeon: return "dungeon";
		case PlaceOwner::Test: return "test";
	}
	return "?";
}

void Forget(UInt32 refId) {
	for (UInt32 i = 0; i < g_liveCount; ++i) {
		if (g_live[i] == refId) {
			g_live[i] = g_live[--g_liveCount];
			return;
		}
	}
}

void StepStages() {
	const UInt32 marker = LookupForm(EspFormId(g_espBase, kPlaceMarkerLocalId));
	if (!LooksLikeObject(marker)) {
		return;
	}
	char line[96];
	switch (g_stage) {
		case Stage::None: {
			if (!PopPlace(g_queue, g_current)) {
				return;
			}
			std::snprintf(line, sizeof(line), "MoveTo %08X %.0f %.0f %.0f", g_current.anchor, g_current.dx,
			              g_current.dy, g_current.dz);
			const bool moved = g_console(marker, line);
			g_stage = Stage::MarkerMoved;
			if (g_log != nullptr && g_logsLeft > 0) {
				--g_logsLeft;
				g_log("Placer: %u for the %s (%u) - \"%s\" answered %d", g_current.count, OwnerName(g_current.owner),
				      g_current.tag, line, moved ? 1 : 0);
			}
			return;
		}
		case Stage::MarkerMoved: {
			g_beforeCount = PlacedActorsNear(FloatAt(marker + kRefPositionOffset),
			                                 FloatAt(marker + kRefPositionOffset + 4), kPlaceFoundUnits, g_before,
			                                 kNearMost);
			std::snprintf(line, sizeof(line), "PlaceAtMe %08X %u", g_current.list, g_current.count);
			const bool placed = g_console(marker, line);
			g_stage = Stage::Placed;
			if (g_log != nullptr && g_logsLeft > 0) {
				--g_logsLeft;
				g_log("Placer: \"%s\" at %.0f %.0f answered %d", line, FloatAt(marker + kRefPositionOffset),
				      FloatAt(marker + kRefPositionOffset + 4), placed ? 1 : 0);
			}
			return;
		}
		case Stage::Placed: {
			UInt32 now[kNearMost];
			const UInt32 n = PlacedActorsNear(FloatAt(marker + kRefPositionOffset),
			                                  FloatAt(marker + kRefPositionOffset + 4), kPlaceFoundUnits, now, kNearMost);
			UInt32 found[kPlaceCountMost];
			const UInt32 count = NewlyPlaced(g_before, g_beforeCount, now, n, g_current.count, found);
			for (UInt32 i = 0; i < count && g_liveCount < kLiveMost; ++i) {
				g_live[g_liveCount++] = found[i];
			}
			Log("Placer: %u of %u found at the marker (%u placed ones in the world)", count, g_current.count,
			    g_liveCount);
			const PlacedFn fn = g_handlers[static_cast<UInt32>(g_current.owner) & 3];
			if (fn != nullptr && count > 0) {
				fn(g_current.tag, found, count);
			}
			g_stage = Stage::None;
			return;
		}
	}
}

void StepRetiring() {
	UInt32 disable[kRetireMost];
	UInt32 remove[kRetireMost];
	UInt32 nd = 0;
	UInt32 nr = 0;
	StepRetire(g_retire, disable, nd, remove, nr);
	for (UInt32 i = 0; i < nd; ++i) {
		const UInt32 ref = LookupForm(disable[i]);
		if (LooksLikeObject(ref)) {
			g_console(ref, "Disable");
		}
	}
	UInt32 deleted = 0;
	for (UInt32 i = 0; i < nr; ++i) {
		const UInt32 ref = LookupForm(remove[i]);
		if (LooksLikeObject(ref) && g_console(ref, "DeleteReference")) {
			++deleted;
		}
		Forget(remove[i]);
	}
	if (nd + nr > 0) {
		Log("Placer: %u disabled, %u handed to DeleteReference (%u answered)", nd, nr, deleted);
	}
}

}  // namespace

void ConfigurePlacer(UInt32 espBase, ConsoleFn console, LogFn log) {
	g_espBase = espBase;
	g_console = console;
	g_log = log;
	log("Placer: %s (marker %08X)", espBase == kNoEsp ? "no ESP - none" : "ready",
	    espBase == kNoEsp ? 0u : EspFormId(espBase, kPlaceMarkerLocalId));
}

void SetPlacedHandler(PlaceOwner owner, PlacedFn fn) { g_handlers[static_cast<UInt32>(owner) & 3] = fn; }

bool RequestPlace(const PlaceRequest& request) { return g_espBase != kNoEsp && PushPlace(g_queue, request); }

UInt32 PlacesPendingFor(PlaceOwner owner, UInt8 tag) {
	return PlacesPending(g_queue, g_stage != Stage::None ? &g_current : nullptr, owner, tag);
}

UInt32 DropPlacesFor(PlaceOwner owner, UInt8 tag) { return DropPlaces(g_queue, owner, tag); }

void RetirePlaced(UInt32 refId) { NoteRetire(g_retire, refId); }

void StepPlacer(float dt) {
	if (g_espBase == kNoEsp || g_console == nullptr) {
		return;
	}
	g_clock += dt;
	if (g_clock < kStepSeconds) {
		return;
	}
	g_clock = 0.0f;
	StepRetiring();
	StepStages();
}

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

UInt32 NewestPlacedOf(UInt32 baseFormId, UInt32 fromId) {
	UInt32 found = 0;
	UInt32 misses = 0;
	UInt32 id = fromId < 0xFF000000 ? 0xFF000000 : fromId;
	for (UInt32 n = 0; n < kBagScanMost && misses < kBagMissesMost; ++n, ++id) {
		const UInt32 ref = LookupForm(id);
		if (!LooksLikeObject(ref)) {
			++misses;
			continue;
		}
		misses = 0;
		if (FormIdOf(Read(ref + 0x1C)) == baseFormId) {
			found = id;
		}
	}
	return found;
}

UInt32 PlacerImage(UInt8* out, UInt32 capacity) {
	UInt32 n = 0;
	for (UInt32 i = 0; i < g_liveCount && 4 * (n + 1) <= capacity; ++i) {
		Put32(out + 4 * n++, g_live[i]);
	}
	return 4 * n;
}

void ReadPlacerImage(const UInt8* in, UInt32 length, bool (*resolve)(UInt32, UInt32&)) {
	UInt32 read = 0;
	for (UInt32 at = 0; at + 4 <= length; at += 4) {
		UInt32 id = 0;
		if (resolve(Get32(in + at), id) && NoteRetire(g_retire, id)) {
			++read;
		}
	}
	Log("Placer: %u placed ones of the save taken out of the world (the owners place their own again)", read);
}

void ResetPlacer() {
	g_queue = PlaceQueue{};
	g_stage = Stage::None;
	g_clock = 0.0f;
	g_liveCount = 0;
	g_retire = RetireList{};
}

}  // namespace walkers
