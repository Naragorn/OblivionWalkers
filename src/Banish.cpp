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

const BanishSettings* g_settings = nullptr;
ConsoleFn g_console = nullptr;
DeadFn g_dead = nullptr;
LogFn g_log = nullptr;
BanishList g_list;
PileList g_piles;
float g_clock = 0.0f;
UInt32 g_logsLeft = 200;

// The bag a PlaceAtMe on `corpse` made. A console line cannot hand a
// reference back (harness daedra-siege.txt 2026-11-16: "set Walkers.placed_ref
// to PlaceAtMe ..." left the variable 0), so the new reference is found by
// its ID: the game numbers the references it makes upward (FF000F94, F95,
// ... read in the same run), so the bag is the newest LootBag reference at
// or past the last ID seen - scanned upward until kBagMissesMost IDs in a row
// hold nothing (BanishLogic.h NextBagScan).
UInt32 g_scanFrom = 0xFF000000;

UInt32 PlaceBag(UInt32 refId, UInt32 corpse) {
	char line[48];
	std::snprintf(line, sizeof(line), "PlaceAtMe %08X 1", kLootBag);
	const bool placed = g_console(corpse, line);
	const UInt32 bag = NewestPlacedOf(kLootBag, g_scanFrom > refId ? g_scanFrom : refId);
	g_scanFrom = NextBagScan(g_scanFrom, refId, bag);
	if (g_logsLeft > 0) {
		--g_logsLeft;
		g_log("Banish: \"%s\" on %08X answered %d - bag %08X found by its ID", line, refId, placed ? 1 : 0, bag);
	}
	return bag;
}

// How many kinds of things `ref` holds (xOBSE GetNumItems, asked into the
// ESP quest's siege_lock as Siege.cpp asks a door), -1 not read.
UInt32 g_quest = 0;
double NumItems(UInt32 ref) {
	double* var = g_quest != 0 ? QuestVariable(g_quest, "siege_lock") : nullptr;
	if (var == nullptr) {
		return -1.0;
	}
	*var = -1.0;
	g_console(ref, "set Walkers.siege_lock to GetNumItems");
	return *var;
}

void Banish(UInt32 refId, UInt32 corpse) {
	const UInt32 bag = PlaceBag(refId, corpse);
	double had = -1.0;
	double held = -1.0;
	if (bag != 0) {
		char line[48];
		std::snprintf(line, sizeof(line), "RemoveAllItems %08X", bag);
		had = NumItems(corpse);
		g_console(corpse, line);
		const UInt32 bagRef = LookupForm(bag);
		held = LooksLikeObject(bagRef) ? NumItems(bagRef) : -1.0;
		if (!NotePile(g_piles, bag, false)) {
			RetirePlaced(bag);
		}
	}
	RetirePlaced(refId);
	if (g_logsLeft > 0) {
		--g_logsLeft;
		g_log("Banish: Daedra %08X back to Oblivion - its things into bag %08X (%.0f kinds on the body, %.0f in the "
		      "bag after), the body taken out",
		      refId, bag, had, held);
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
	for (UInt32 i = 0; i < g_piles.count;) {
		const UInt32 id = g_piles.ids[i];
		const UInt32 bag = LookupForm(id);
		if (!LooksLikeObject(bag)) {
			DropPile(g_piles, i);
			continue;
		}
		if (!g_piles.checked[i]) {
			const double held = NumItems(bag);
			if (g_logsLeft > 0) {
				--g_logsLeft;
				g_log("Banish: bag %08X holds %.0f kinds a second later%s", id, held,
				      held == 0.0 ? " - empty, taken away" : "");
			}
			if (held == 0.0) {
				RetirePlaced(id);  // nothing came over (a creature's own things only): no empty sack
				DropPile(g_piles, i);
				continue;
			}
			g_piles.checked[i] = true;
		}
		if (PileGoes(*g_settings, DistanceFromPlayer(bag))) {
			RetirePlaced(id);
			DropPile(g_piles, i);
			continue;
		}
		++i;
	}
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

UInt32 BanishPileImage(UInt8* out, UInt32 capacity) { return PileImage(g_piles, out, capacity); }

UInt32 ReadBanishPileImage(const UInt8* in, UInt32 length, bool (*resolve)(UInt32, UInt32&)) {
	return ReadPileImage(g_piles, in, length, resolve);
}

UInt32 BanishPileCount() { return g_piles.count; }

void ResetBanish() {
	g_list = BanishList{};
	g_piles = PileList{};
	g_clock = 0.0f;
	g_scanFrom = 0xFF000000;
}

}  // namespace walkers
