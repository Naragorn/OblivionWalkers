#include "Disguise.h"

#include "Engine.h"

#include <cstdio>

namespace walkers {

using namespace engine;

namespace {

constexpr UInt32 kPlayerRefId = 0x14;
constexpr UInt32 kRefParentCellOffset = 0x40;
constexpr UInt32 kCellFlagsOffset = 0x24;
constexpr UInt32 kActorProcessOffset = 0x58;
// The process's movement flags, 16 bits at +0x1FC - the field
// BaseProcess::GetMovementFlags (vtable slot 0xAF) only reads, as OBVR reads
// it for its sneak eye (OBVR src/game/GameAddresses.h
// kProcessMovementFlagsOffset, in use since 2026-09). Calling the slot from
// here returned garbage and broke the frame (harness disguise 2026-11-09
// 14:37: flags 006293A5, the disguise ended in the frame it began).
constexpr UInt32 kProcessMovementFlagsOffset = 0x1FC;
// The sky (xOBSE GameObjects.cpp Sky_GetSingleton 0x00542EA0, cdecl), its
// current weather at +0x10, the weather's precipitation at +0x53 (xOBSE
// GameForms.h TESWeather: TESModel at +0x30 is 0x18 long, then eleven bytes
// to precipType; the fog at +0x58 confirms the count).
constexpr UInt32 kSkyGetSingleton = 0x00542EA0;
constexpr UInt32 kSkyWeatherOffset = 0x10;
constexpr UInt32 kWeatherPrecipOffset = 0x53;
// The game's message box (xOBSE GameAPI.cpp for 1.2.416): ShowMessageBox
// (text, callback, base button, buttons..., NULL), its own callback, the
// script the answer belongs to and the button pressed (0xFF none yet) -
// as xOBSE's MessageBoxEx sets them up before it shows one.
constexpr UInt32 kShowMessageBox = 0x00579C10;
constexpr UInt32 kShowMessageBoxCallback = 0x005003E0;
constexpr UInt32 kMessageBoxScriptRef = 0x00B361C8;
constexpr UInt32 kMessageBoxButton = 0x00B09E24;
constexpr UInt8 kNoButton = 0xFF;
constexpr float kTestAfterSeconds = 25.0f;

const DisguiseSettings* g_settings = nullptr;
bool g_test = false;
DisguiseConsoleFn g_console = nullptr;
DisguiseWalkerFn g_walker = nullptr;
DisguiseDeadFn g_dead = nullptr;
DisguiseSendFn g_send = nullptr;
LogFn g_log = nullptr;
UInt32 g_espBase = 0;
DisguiseState g_state;
bool g_offerDue = false;  // OnActivate asked: the dialog shown at the next frame
bool g_begun = false;
bool g_abilityOn = false;
bool g_toldObvr = false;
float g_testClock = 0.0f;
bool g_testDone = false;
UInt32 g_activateLines = 0;

bool Say(UInt32 ref, const char* line) { return LooksLikeObject(ref) && g_console != nullptr && g_console(ref, line); }

void Line(const char* text) {
	char line[200];
	std::snprintf(line, sizeof(line), "Message \"%s\"", text);
	Say(LookupForm(kPlayerRefId), line);
}

UInt32 MovementFlags() {
	const UInt32 player = LookupForm(kPlayerRefId);
	const UInt32 process = LooksLikeObject(player) ? Read(player + kActorProcessOffset) : 0;
	return LooksLikeObject(process) ? *reinterpret_cast<const UInt16*>(process + kProcessMovementFlagsOffset) : 0;
}

bool Outdoors() {
	const UInt32 player = LookupForm(kPlayerRefId);
	const UInt32 cell = LooksLikeObject(player) ? Read(player + kRefParentCellOffset) : 0;
	return LooksLikeObject(cell) && (Read8(cell + kCellFlagsOffset) & 1) == 0;
}

UInt8 Precipitation() {
	using SkyFn = UInt32(__cdecl*)();
	const UInt32 sky = reinterpret_cast<SkyFn>(kSkyGetSingleton)();
	const UInt32 weather = LooksLikeObject(sky) ? Read(sky + kSkyWeatherOffset) : 0;
	return LooksLikeObject(weather) ? Read8(weather + kWeatherPrecipOffset) : 0;
}

UInt32 Ability() { return g_espBase != 0 ? EspFormId(g_espBase, kDisguiseAbilityLocalId) : 0; }

void Show(bool on) {
	char line[48];
	if (on != g_abilityOn && Ability() != 0) {
		std::snprintf(line, sizeof(line), "%s %08X", on ? "AddSpell" : "RemoveSpell", Ability());
		Say(LookupForm(kPlayerRefId), line);
		g_abilityOn = on;
	}
	if (on != g_toldObvr && g_send != nullptr) {
		UInt32 word = 0;
		const UInt32 bytes = DisguiseStateMessage(on, &word);
		g_send(kDisguiseStateMessage, &word, bytes);
		g_toldObvr = on;
	}
}

void Begin(const char* how) {
	BeginDisguise(g_state, *g_settings);
	g_begun = true;
	Show(true);
	Line("You rub the walker's blood over yourself. They will take you for one of theirs - for a while.");
	g_log("Disguise: on (%s) - %.0f real seconds; the walkers leave the player be", how, g_state.left);
}

void End(DisguiseEnd why) {
	if (why == DisguiseEnd::None) {
		return;
	}
	Show(false);
	Line(DisguiseEndText(why));
	g_log("Disguise: off - %s", DisguiseEndText(why));
}

void Ask() {
	*reinterpret_cast<UInt8*>(kMessageBoxButton) = kNoButton;
	*reinterpret_cast<UInt32*>(kMessageBoxScriptRef) = 0;
	using ShowFn = bool(__cdecl*)(const char*, UInt32, UInt32, ...);
	const bool shown = reinterpret_cast<ShowFn>(kShowMessageBox)(
	    "Rub the walker's blood over yourself? The walkers will take you for one of theirs for a while - until you "
	    "strike one, get wet or it wears off.",
	    kShowMessageBoxCallback, 0, "Rub it on", "Leave it", nullptr);
	g_state.asking = shown;
	g_log("Disguise: the dialog %s", shown ? "shown" : "NOT shown (ShowMessageBox answered 0)");
}

void PollAnswer() {
	if (!g_state.asking) {
		return;
	}
	UInt8& button = *reinterpret_cast<UInt8*>(kMessageBoxButton);
	if (button == kNoButton) {
		return;
	}
	const UInt8 pressed = button;
	button = kNoButton;
	g_state.asking = false;
	g_log("Disguise: the dialog answered - button %u", pressed);
	if (pressed == 0) {
		Begin("the dialog's yes");
	}
}

}  // namespace

void ConfigureDisguise(const DisguiseSettings* settings, bool test, DisguiseConsoleFn console, DisguiseWalkerFn walker,
                       DisguiseDeadFn dead, DisguiseSendFn send, LogFn log) {
	g_settings = settings;
	g_test = test;
	g_console = console;
	g_walker = walker;
	g_dead = dead;
	g_send = send;
	g_log = log;
	g_log("Disguise: [Disguise] Enabled=%d Seconds=%.0f Test=%d", settings->enabled ? 1 : 0, settings->seconds,
	      test ? 1 : 0);
}

void ConfigureDisguiseEsp(UInt32 espBase) { g_espBase = espBase; }

void NoteDisguiseActivate(UInt32 a, UInt32 b) {
	if (g_settings == nullptr) {
		return;
	}
	const UInt32 player = LookupForm(kPlayerRefId);
	if (!LooksLikeObject(a) || !LooksLikeObject(b) || (a != player && b != player)) {
		return;
	}
	const UInt32 other = a == player ? b : a;
	const bool walker = g_walker != nullptr && g_walker(other);
	const bool dead = walker && g_dead != nullptr && g_dead(other);
	const UInt32 flags = MovementFlags();
	if (g_activateLines < 16) {
		++g_activateLines;
		g_log("Disguise: OnActivate - %08X and %08X (the player the %s; walker %d, dead %d, movement flags %08X)",
		      FormIdOf(a), FormIdOf(b), a == player ? "first" : "second", walker ? 1 : 0, dead ? 1 : 0, flags);
	}
	if (DisguiseOffered(*g_settings, g_state, true, walker, dead, Sneaking(flags))) {
		g_offerDue = true;
	}
}

void NoteDisguiseHit(UInt32 target, UInt32 attacker) {
	if (!g_state.on || !LooksLikeObject(target) || !LooksLikeObject(attacker)) {
		return;
	}
	const bool byPlayer = FormIdOf(attacker) == kPlayerRefId;
	End(DisguiseHit(g_state, byPlayer, g_walker != nullptr && g_walker(target)));
}

void NoteDisguiseSmear(UInt32 corpse) {
	if (g_settings == nullptr || !g_settings->enabled) {
		return;
	}
	g_log("Disguise: OBVR's gesture smeared from %08X", corpse);
	Begin("OBVR's gesture");
}

void StepDisguiseInGame(float dt) {
	if (g_settings == nullptr) {
		return;
	}
	if (g_test && !g_testDone) {
		g_testClock += dt;
		if (g_testClock >= kTestAfterSeconds) {
			g_testDone = true;
			const UInt32 flags = MovementFlags();
			g_log("Disguise: test - the player's movement flags %08X (sneaking %d, swimming %d), the weather's "
			      "precipitation %u, outdoors %d; the dialog offered as a sneaking activation would",
			      flags, Sneaking(flags) ? 1 : 0, Swimming(flags) ? 1 : 0, Precipitation(), Outdoors() ? 1 : 0);
			g_offerDue = g_settings->enabled;
		}
	}
	if (g_offerDue) {
		g_offerDue = false;
		Ask();
	}
	PollAnswer();
	if (!g_state.on) {
		return;
	}
	const UInt32 flags = MovementFlags();
	const DisguiseEnd end = StepDisguise(g_state, *g_settings, dt, Swimming(flags), Raining(Precipitation(), Outdoors()));
	End(end);
}

bool Disguised() { return g_state.on; }

bool TakeDisguiseBegun() {
	const bool begun = g_begun;
	g_begun = false;
	return begun;
}

void ResetDisguise() {
	g_state = DisguiseState{};
	g_offerDue = false;
	g_begun = false;
	g_testClock = 0.0f;
	g_testDone = false;
	// The ability the save may hold taken off at the next frame's Show; OBVR
	// told off.
	g_abilityOn = true;
	g_toldObvr = true;
	if (g_settings != nullptr) {
		Show(false);
	}
}

}  // namespace walkers
