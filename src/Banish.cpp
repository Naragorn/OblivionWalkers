#include "Banish.h"

#include <cmath>
#include <cstdio>
#include <cstring>

#include "Engine.h"
#include "LinkLogic.h"
#include "Placer.h"
#include "QuestVars.h"
#include "RiseLogic.h"

namespace walkers {
namespace {

using namespace engine;

constexpr UInt32 kRefPositionOffset = 0x2C;
constexpr UInt32 kRefParentCellOffset = 0x40;
constexpr float kStepSeconds = 1.0f;
constexpr UInt32 kPilesMost = 64;

const BanishSettings* g_settings = nullptr;
ConsoleFn g_console = nullptr;
DeadFn g_dead = nullptr;
LogFn g_log = nullptr;
UInt32 g_quest = 0;
BanishList g_list;
UInt32 g_piles[kPilesMost] = {};
UInt32 g_pileCount = 0;
float g_clock = 0.0f;
UInt32 g_logsLeft = 200;

// The bag a PlaceAtMe on `corpse` made, read back through placed_ref; 0
// when the line did not run or the variable holds none.
UInt32 PlaceBag(UInt32 corpse) {
	double* var = g_quest != 0 ? QuestVariable(g_quest, "placed_ref") : nullptr;
	if (var == nullptr) {
		return 0;
	}
	*var = 0.0;
	char line[80];
	std::snprintf(line, sizeof(line), "set Walkers.placed_ref to PlaceAtMe %08X 1", kLootBag);
	g_console(corpse, line);
	UInt64 bits = 0;
	std::memcpy(&bits, var, sizeof(bits));
	const UInt32 bag = RefFromVariable(bits, *var);
	if (g_logsLeft > 0) {
		--g_logsLeft;
		g_log("Banish: \"%s\" - placed_ref reads %.0f (bits %08X%08X): bag %08X", line, *var,
		      static_cast<UInt32>(bits >> 32), static_cast<UInt32>(bits), bag);
	}
	return bag;
}

void Banish(UInt32 refId, UInt32 corpse) {
	const UInt32 bag = PlaceBag(corpse);
	bool moved = false;
	if (bag != 0) {
		char line[48];
		std::snprintf(line, sizeof(line), "RemoveAllItems %08X", bag);
		moved = g_console(corpse, line);
		if (g_pileCount < kPilesMost) {
			g_piles[g_pileCount++] = bag;
		} else {
			RetirePlaced(bag);
		}
	}
	RetirePlaced(refId);
	if (g_logsLeft > 0) {
		--g_logsLeft;
		g_log("Banish: Daedra %08X back to Oblivion - its things into bag %08X (%d), the body taken out", refId, bag,
		      moved ? 1 : 0);
	}
}

// Distance from the player in the same cell or worldspace's open air; -1
// when not comparable (another cell, no cell).
float DistanceFromPlayer(UInt32 ref) {
	const UInt32 player = LookupForm(0x14);
	if (!LooksLikeObject(player) || !LooksLikeObject(ref)) {
		return -1.0f;
	}
	const UInt32 cell = Read(ref + kRefParentCellOffset);
	const UInt32 playerCell = Read(player + kRefParentCellOffset);
	if (!LooksLikeObject(cell) || !LooksLikeObject(playerCell)) {
		return -1.0f;
	}
	const bool interior = (Read8(cell + 0x24) & 1) != 0;
	const bool playerInterior = (Read8(playerCell + 0x24) & 1) != 0;
	if (interior != playerInterior || (interior && cell != playerCell)) {
		return interior ? 1.0e9f : -1.0f;  // the player left that interior: far
	}
	const float dx = FloatAt(ref + kRefPositionOffset) - FloatAt(player + kRefPositionOffset);
	const float dy = FloatAt(ref + kRefPositionOffset + 4) - FloatAt(player + kRefPositionOffset + 4);
	return std::sqrt(dx * dx + dy * dy);
}

void StepPiles() {
	UInt32 kept = 0;
	for (UInt32 i = 0; i < g_pileCount; ++i) {
		const UInt32 bag = LookupForm(g_piles[i]);
		if (!LooksLikeObject(bag)) {
			continue;
		}
		if (PileGoes(*g_settings, DistanceFromPlayer(bag))) {
			RetirePlaced(g_piles[i]);
			continue;
		}
		g_piles[kept++] = g_piles[i];
	}
	g_pileCount = kept;
}

}  // namespace

void ConfigureBanish(const BanishSettings* settings, ConsoleFn console, DeadFn dead, LogFn log) {
	g_settings = settings;
	g_console = console;
	g_dead = dead;
	g_log = log;
	log("Banish: [Invaders] BanishSeconds=%.0f BanishShader=%08X", settings->seconds, settings->shader);
}

void ConfigureBanishEsp(UInt32 espBase) { g_quest = espBase == kNoEsp ? 0 : EspFormId(espBase, kLinkQuestLocalId); }

void WatchDaedra(UInt32 refId) { WatchForBanish(g_list, refId); }

void StepBanishInGame(float dt) {
	if (g_settings == nullptr || g_console == nullptr) {
		return;
	}
	g_clock += dt;
	if (g_clock < kStepSeconds) {
		return;
	}
	const float elapsed = g_clock;
	g_clock = 0.0f;
	bool drop[kBanishMost] = {};
	for (UInt32 i = 0; i < g_list.count; ++i) {
		BanishEntry& e = g_list.items[i];
		const UInt32 ref = LookupForm(e.ref);
		const bool exists = LooksLikeObject(ref);
		const bool dead = exists && g_dead != nullptr && g_dead(ref);
		switch (StepBanishOne(e, *g_settings, exists, dead, elapsed)) {
			case BanishAction::None: break;
			case BanishAction::Forget: drop[i] = true; break;
			case BanishAction::Shader: {
				char line[64];
				std::snprintf(line, sizeof(line), "PlayMagicShaderVisuals %08X %.0f", g_settings->shader,
				              g_settings->fadeSeconds + 1.0f);
				const bool played = g_console(ref, line);
				if (g_logsLeft > 0) {
					--g_logsLeft;
					g_log("Banish: Daedra %08X dead %.0f s - \"%s\" answered %d", e.ref, e.down, line, played ? 1 : 0);
				}
				break;
			}
			case BanishAction::Banish:
				Banish(e.ref, ref);
				drop[i] = true;
				break;
		}
	}
	DropBanished(g_list, drop);
	StepPiles();
}

void ResetBanish() {
	g_list = BanishList{};
	g_pileCount = 0;
	g_clock = 0.0f;
}

}  // namespace walkers
