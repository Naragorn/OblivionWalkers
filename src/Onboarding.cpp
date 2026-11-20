#include "Onboarding.h"

#include <windows.h>

#include <cstdio>
#include <vector>

#include "Disguise.h"
#include "Engine.h"
#include "IniEditLogic.h"

namespace walkers {

using namespace engine;

namespace {

constexpr UInt32 kPlayerRefId = 0x14;
constexpr UInt32 kRefParentCellOffset = 0x40;
// The game's message box, as Disguise.cpp has it (xOBSE GameAPI.cpp for
// 1.2.416): ShowMessageBox (text, callback, base button, buttons..., NULL),
// its own callback, the script the answer belongs to and the button pressed
// (0xFF none yet).
constexpr UInt32 kShowMessageBox = 0x00579C10;
constexpr UInt32 kShowMessageBoxCallback = 0x005003E0;
constexpr UInt32 kMessageBoxScriptRef = 0x00B361C8;
constexpr UInt32 kMessageBoxButton = 0x00B09E24;
constexpr UInt8 kNoButton = 0xFF;
// The interface's open menus: ten menu IDs at InterfaceManager +0xE0, the
// manager at [0x00B3A6E0] (Horde.cpp MenuOpen; OBVR GameAddresses.h).
constexpr UInt32 kInterfaceManagerPointer = 0x00B3A6E0;
constexpr UInt32 kInterfaceMenuStackOffset = 0xE0;
constexpr UInt32 kInterfaceMenuStackSlots = 10;
// A box the game would not show is tried again after this long.
constexpr float kRetrySeconds = 5.0f;

OnboardingSettings* g_settings = nullptr;
const char* g_iniPath = nullptr;
OnboardingConsoleFn g_console = nullptr;
OnboardingChosenFn g_chosen = nullptr;
LogFn g_log = nullptr;

OnboardingState g_state;
bool g_boxUp = false;
bool g_chainBegun = false;   // a box of this chain was shown since the load or the last Start
float g_sinceLoad = 0.0f;    // play seconds since the load
float g_sinceAnswer = 0.0f;  // seconds since the last answer, between two boxes
float g_testClock = 0.0f;    // seconds the box has been up
UInt8 g_testAnswers[kOnboardingTestAnswersMost] = {};
UInt32 g_testCount = 0;
UInt32 g_testIndex = 0;
UInt32 g_blockedLogged = 0;  // the menu that held the first box back, logged once
UInt32 g_failLines = 3;

bool InGame() {
	const UInt32 player = LookupForm(kPlayerRefId);
	return LooksLikeObject(player) && LooksLikeObject(Read(player + kRefParentCellOffset));
}

UInt32 OpenMenu() {
	const UInt32 manager = Read(kInterfaceManagerPointer);
	if (!LooksLikeObject(manager)) {
		return 0;
	}
	return FirstOtherMenu(reinterpret_cast<const UInt32*>(manager + kInterfaceMenuStackOffset), kInterfaceMenuStackSlots);
}

void Ask() {
	char text[kOnboardingTextMost + 16];
	const char* labels[3] = {};
	const UInt32 buttons = OnboardingButtons(g_state, labels);
	if (OnboardingText(g_state, text, sizeof(text)) == 0 || buttons < 2) {
		g_log("Onboarding: page %u %s has no text - skipped", OnboardingPageNumber(g_state.page),
		      OnboardingPageName(g_state.page));
		return;
	}
	*reinterpret_cast<UInt8*>(kMessageBoxButton) = kNoButton;
	*reinterpret_cast<UInt32*>(kMessageBoxScriptRef) = 0;
	using ShowFn = bool(__cdecl*)(const char*, UInt32, UInt32, ...);
	const ShowFn show = reinterpret_cast<ShowFn>(kShowMessageBox);
	const bool shown = buttons == 3
	                       ? show(text, kShowMessageBoxCallback, 0, labels[0], labels[1], labels[2], nullptr)
	                       : show(text, kShowMessageBoxCallback, 0, labels[0], labels[1], nullptr);
	if (!shown) {
		if (g_failLines > 0) {
			--g_failLines;
			g_log("Onboarding: page %u %s NOT shown (ShowMessageBox answered 0) - again in %.0f s",
			      OnboardingPageNumber(g_state.page), OnboardingPageName(g_state.page), kRetrySeconds);
		}
		// A failed first box waits like a load; a failed later one like an answer.
		g_sinceLoad = 0.0f;
		g_sinceAnswer = 0.0f;
		g_chainBegun = false;
		return;
	}
	g_boxUp = true;
	g_chainBegun = true;
	g_testClock = 0.0f;
	g_log("Onboarding: page %u %s shown (%u chars, %u buttons)", OnboardingPageNumber(g_state.page),
	      OnboardingPageName(g_state.page), static_cast<UInt32>(std::strlen(text)), buttons);
}

// The choice into the plugin INI: [Walkers] Mode, [Daedra] Mode and
// [Onboarding] Show=0, everything else of the file as it was (IniEditLogic.h).
bool WriteChoiceToIni(WalkersMode walkers, DaedraMode daedra) {
	if (g_iniPath == nullptr || g_iniPath[0] == '\0') {
		return false;
	}
	std::vector<char> text;
	if (FILE* in = std::fopen(g_iniPath, "rb")) {
		char chunk[65536];
		for (size_t got; (got = std::fread(chunk, 1, sizeof(chunk), in)) > 0;) {
			text.insert(text.end(), chunk, chunk + got);
		}
		std::fclose(in);
	} else {
		return false;
	}
	struct Key {
		const char* section;
		const char* key;
		char value[2];
	};
	Key keys[3] = {{"Walkers", "Mode", {static_cast<char>('0' + static_cast<UInt32>(walkers)), '\0'}},
	               {"Daedra", "Mode", {static_cast<char>('0' + static_cast<UInt32>(daedra)), '\0'}},
	               {"Onboarding", "Show", {'0', '\0'}}};
	for (const Key& k : keys) {
		std::vector<char> out(text.size() + 128);
		const UInt32 used = SetIniKey(text.data(), static_cast<UInt32>(text.size()), k.section, k.key, k.value, out.data(),
		                              static_cast<UInt32>(out.size()));
		if (used == 0) {
			return false;
		}
		out.resize(used);
		text.swap(out);
	}
	char tempPath[MAX_PATH];
	if (std::snprintf(tempPath, sizeof(tempPath), "%s.new", g_iniPath) >= static_cast<int>(sizeof(tempPath))) {
		return false;
	}
	FILE* file = std::fopen(tempPath, "wb");
	if (file == nullptr) {
		return false;
	}
	const bool whole = std::fwrite(text.data(), 1, text.size(), file) == text.size();
	const bool closed = std::fclose(file) == 0;
	if (!whole || !closed) {
		DeleteFileA(tempPath);
		return false;
	}
	if (MoveFileExA(tempPath, g_iniPath, MOVEFILE_REPLACE_EXISTING) == 0) {
		DeleteFileA(tempPath);
		return false;
	}
	return true;
}

void Finish() {
	const WalkersMode walkers = g_state.walkers;
	const DaedraMode daedra = g_state.daedra;
	g_settings->show = false;
	if (g_chosen != nullptr) {
		g_chosen(walkers, daedra);
	}
	const bool written = WriteChoiceToIni(walkers, daedra);
	g_log("Onboarding: chosen Walkers %u Daedra %u, %s", static_cast<UInt32>(walkers), static_cast<UInt32>(daedra),
	      written ? "written to the INI" : "NOT written to the INI (the file could not be read or replaced)");
	g_state = OnboardingState{};
	g_chainBegun = false;
}

void PollAnswer() {
	UInt8& button = *reinterpret_cast<UInt8*>(kMessageBoxButton);
	if (button == kNoButton) {
		return;
	}
	const UInt32 pressed = button;
	button = kNoButton;
	g_boxUp = false;
	g_sinceAnswer = 0.0f;
	g_log("Onboarding: answered %u - %s", pressed, OnboardingButtonLabel(g_state, pressed));
	switch (OnboardingAnswer(g_state, pressed)) {
		case OnboardingNext::Show:
		case OnboardingNext::Again:
			break;
		case OnboardingNext::Finish:
			Finish();
			break;
	}
}

void PressTestAnswer(float dt) {
	g_testClock += dt;
	if (g_console == nullptr || !OnboardingTestAnswerDue(g_testCount, g_testIndex, g_testClock)) {
		return;
	}
	char line[32];
	std::snprintf(line, sizeof(line), "SetButtonPressed %u", static_cast<UInt32>(g_testAnswers[g_testIndex]));
	const bool said = g_console(LookupForm(kPlayerRefId), line);
	g_log("Onboarding: test - \"%s\" (answered %d)", line, said ? 1 : 0);
	++g_testIndex;
	g_testClock = 0.0f;
}

}  // namespace

void ConfigureOnboarding(OnboardingSettings* settings, const char* iniPath, OnboardingConsoleFn console,
                         OnboardingChosenFn chosen, LogFn log) {
	g_settings = settings;
	g_iniPath = iniPath;
	g_console = console;
	g_chosen = chosen;
	g_log = log;
	g_testCount = ParseTestAnswers(settings->testAnswers, g_testAnswers, kOnboardingTestAnswersMost);
	g_testIndex = 0;
	g_log("Onboarding: [Onboarding] Show=%d, %u test answers", settings->show ? 1 : 0, g_testCount);
}

void StepOnboardingInGame(float dt) {
	if (g_settings == nullptr) {
		return;
	}
	const bool inGame = InGame();
	if (inGame) {
		g_sinceLoad += dt;
	}
	if (g_boxUp) {
		PressTestAnswer(dt);
		PollAnswer();
		return;
	}
	if (!g_settings->show) {
		// Switched off (by the INI, or by LINK while a chain waited): the chain starts over when it is on again.
		g_state = OnboardingState{};
		g_chainBegun = false;
		return;
	}
	g_sinceAnswer += dt;
	const UInt32 menu = inGame ? OpenMenu() : 0;
	const float waited = g_chainBegun ? g_sinceAnswer : g_sinceLoad;
	if (OnboardingAskDue(true, inGame, menu, DisguiseAsking(), g_chainBegun, waited)) {
		Ask();
		return;
	}
	// Held back by a menu once the wait is over: said once for each menu, so the log shows what held it.
	if (inGame && menu != 0 && waited >= (g_chainBegun ? kOnboardingBetweenSeconds : kOnboardingAfterLoadSeconds) &&
	    menu != g_blockedLogged) {
		g_blockedLogged = menu;
		g_log("Onboarding: waiting - the menu %03X is open", menu);
	}
}

bool OnboardingBoxUp() { return g_boxUp; }

void ResetOnboarding() {
	g_state = OnboardingState{};
	g_boxUp = false;
	g_chainBegun = false;
	g_sinceLoad = 0.0f;
	g_sinceAnswer = 0.0f;
	g_testClock = 0.0f;
	g_testIndex = 0;
	g_blockedLogged = 0;
	g_failLines = 3;
}

}  // namespace walkers
