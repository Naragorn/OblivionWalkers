#include "News.h"

#include "CourierRefs.h"
#include "Engine.h"
#include "RiseLogic.h"

namespace walkers {

using namespace engine;

namespace {

// GameHour and GameDaysPassed, the GLOBs 0x38 and 0x39 of Oblivion.esm
// (read 2026-10-26); a TESGlobal's float at +0x24 (xOBSE GameForms.h: name
// 018, type 020, data 024).
constexpr UInt32 kGameHourId = 0x00000038;
constexpr UInt32 kGameDaysPassedId = 0x00000039;
constexpr UInt32 kGlobalValueOffset = 0x24;
constexpr UInt8 kFormTypeGlobal = 0x04;
constexpr UInt8 kFormTypeBook = 0x15;
constexpr UInt32 kRefBaseFormOffset = 0x1C;
// The ESP's editions (tools/esp-write.py), NewsLogic.h EditionIndex order.
constexpr UInt32 kEditionLocalId = 0x8FC;
constexpr float kStepSeconds = 1.0f;

NewsSettings g_settings;
NewsState g_news;
bool g_ready = false;
UInt32 g_editions[kEditionCount] = {};
float g_clock = 0.0f;
// The edition last set on the loaded broadsheets, so a line is logged when
// it changes or new broadsheets come in.
UInt32 g_lastEdition = kEditionCount;
UInt32 g_lastSet = 0;

float GlobalValue(UInt32 id) {
	const UInt32 global = LookupForm(id);
	if (!LooksLikeObject(global) || Read8(global + kFormTypeOffset) != kFormTypeGlobal) {
		return -1.0f;
	}
	return FloatAt(global + kGlobalValueOffset);
}

float GameTime() { return GameTimeDays(GlobalValue(kGameDaysPassedId), GlobalValue(kGameHourId)); }

// CityGates.h City order.
constexpr const char* kCityNames[kNewsCities] = {"Anvil",   "Bravil",   "Bruma",    "Cheydinhal",
                                                 "Chorrol", "Leyawiin", "Skingrad", "the Imperial City"};

const char* EditionText(const Edition& edition) {
	switch (edition.kind) {
		case EditionKind::Approaching:
			return "a horde heading for";
		case EditionKind::Attack:
			return "walkers at the gates of";
		case EditionKind::Several:
			return "several cities attacked";
		case EditionKind::General:
		default:
			return "the outbreak";
	}
}

}  // namespace

float GameDaysNow() { return GameTime(); }

void ConfigureNews(const NewsSettings& settings, UInt32 espBase, LogFn log) {
	g_settings = settings;
	g_ready = false;
	if (!settings.courier) {
		return;
	}
	if (espBase == kNoEsp) {
		log("News: no ESP - the Courier keeps its old news");
		return;
	}
	UInt32 missing = 0;
	for (UInt32 i = 0; i < kEditionCount; ++i) {
		const UInt32 book = LookupForm(EspFormId(espBase, kEditionLocalId + i));
		g_editions[i] = LooksLikeObject(book) && Read8(book + kFormTypeOffset) == kFormTypeBook ? book : 0;
		missing += g_editions[i] == 0 ? 1u : 0u;
	}
	if (missing != 0) {
		log("News: %u of the ESP's %u editions are missing (an older OblivionWalkers.esp?) - the Courier keeps its "
		    "old news",
		    missing, kEditionCount);
		return;
	}
	g_ready = true;
	log("News: on - %u editions on the %u broadsheets of the inns and the Courier's office; an attack on a city's "
	    "gates is news for %.1f game days",
	    kEditionCount, kCourierRefCount, settings.recentDays);
}

void NoteGateAttack(UInt32 city) {
	const float now = GameTime();
	if (now >= 0.0f) {
		NoteCityAttack(g_news, city, now);
	}
}

NewsState& CurrentNews() { return g_news; }

void StepNewsInGame(float dt, int headingCity, LogFn log) {
	if (!g_ready) {
		return;
	}
	g_clock += dt;
	if (g_clock < kStepSeconds) {
		return;
	}
	g_clock = 0.0f;
	const float now = GameTime();
	if (now < 0.0f) {
		return;
	}
	NoteHeading(g_news, headingCity, now);
	const Edition edition = ChooseEdition(g_news, now, g_settings.recentDays, g_settings.headingHoldDays);
	const UInt32 index = EditionIndex(edition);
	const UInt32 book = g_editions[index];
	UInt32 set = 0;
	UInt32 loaded = 0;
	for (const CourierRef& sheet : kCourierRefs) {
		const UInt32 ref = LookupForm(sheet.ref);
		if (!LooksLikeObject(ref)) {
			continue;
		}
		const UInt32 base = Read(ref + kRefBaseFormOffset);
		// Only a broadsheet as Oblivion.esm placed it, or one of ours: a
		// reference another mod gave a different base is left alone.
		bool ours = FormIdOf(base) == sheet.sheet;
		for (UInt32 i = 0; i < kEditionCount && !ours; ++i) {
			ours = base == g_editions[i];
		}
		if (!ours) {
			continue;
		}
		++loaded;
		if (base != book) {
			*reinterpret_cast<UInt32*>(ref + kRefBaseFormOffset) = book;
			++set;
		}
	}
	if (set > 0 && (index != g_lastEdition || set != g_lastSet)) {
		const bool named = edition.city >= 0 && edition.city < static_cast<int>(kNewsCities);
		log("News: %u broadsheet%s set to edition %u (%s%s%s) - %u loaded; game day %.3f (GameDaysPassed %.3f, "
		    "GameHour %.3f)",
		    set, set == 1 ? "" : "s", index, EditionText(edition), named ? " " : "",
		    named ? kCityNames[edition.city] : "", loaded, now, GlobalValue(kGameDaysPassedId),
		    GlobalValue(kGameHourId));
	}
	if (set > 0) {
		g_lastEdition = index;
		g_lastSet = set;
	}
}

}  // namespace walkers
