#pragma once

// The onboarding - a chain of the game's message boxes that explains the mod
// and takes the player's choice of who comes (docs/walkers-spec.md 4.24a;
// Nadi, 2026-11-20: "an onboarding where everything is explained and you
// choose what goes on"), process-free: which page follows which answer, the
// texts and buttons of each page, the summary, what is chosen, when the first
// box may open and how the test answers are read. Rise.cpp / Onboarding.cpp
// show the boxes.
//
// The pages (4.24a; the necromancers' page waits for 4.23 B):
//   Welcome  Choose / Just the game's zombies / Turn the mod off
//   Walkers  Zombies only / Apocalypse / No walkers
//   Daedra   No Daedra / Daedra, enemies / Daedra, allies (with walkers off:
//            No Daedra / Daedra)
//   World    the summary; Start / Choose again

#include <cstdio>
#include <cstring>

#include "KindLogic.h"
#include "Types.h"

namespace walkers {

// [Onboarding] in OblivionWalkers.ini.
struct OnboardingSettings {
	bool show = true;           // Show: 1 by default, set to 0 once the player has chosen
	char testAnswers[48] = {};  // TestAnswers (INI only, a test switch): "1,1,2,0" - the buttons pressed, one a second
};

enum class OnboardingPage : UInt8 { Welcome, Walkers, Daedra, World };

// Where the chain is and what has been chosen so far. The defaults are the
// mod's own (4.24: the game's zombies, no Daedra).
struct OnboardingState {
	OnboardingPage page = OnboardingPage::Welcome;
	WalkersMode walkers = WalkersMode::Zombies;
	DaedraMode daedra = DaedraMode::Off;
};

// What an answer leads to: the next page to show, the same page asked
// again (a button the page does not have), or the end - the choice stands.
enum class OnboardingNext : UInt8 { Show, Again, Finish };

// Whether a box with these buttons is a "Daedra" page without walkers: it
// has two buttons, not three.
inline bool DaedraPageFolds(const OnboardingState& s) { return s.walkers == WalkersMode::Off; }

// The number of buttons of the page now.
inline UInt32 OnboardingButtonCount(const OnboardingState& s) {
	switch (s.page) {
		case OnboardingPage::Welcome: return 3;
		case OnboardingPage::Walkers: return 3;
		case OnboardingPage::Daedra: return DaedraPageFolds(s) ? 2 : 3;
		case OnboardingPage::World: return 2;
	}
	return 0;
}

// The labels of the page's buttons, in the order the game numbers them
// (0 first); returns how many. `labels` takes at least three.
inline UInt32 OnboardingButtons(const OnboardingState& s, const char** labels) {
	switch (s.page) {
		case OnboardingPage::Welcome:
			labels[0] = "Choose";
			labels[1] = "Just the game's zombies";
			labels[2] = "Turn the mod off";
			return 3;
		case OnboardingPage::Walkers:
			labels[0] = "Zombies only";
			labels[1] = "Apocalypse";
			labels[2] = "No walkers";
			return 3;
		case OnboardingPage::Daedra:
			labels[0] = "No Daedra";
			if (DaedraPageFolds(s)) {
				labels[1] = "Daedra";
				return 2;
			}
			labels[1] = "Daedra, enemies of the walkers";
			labels[2] = "Daedra, allies of the walkers";
			return 3;
		case OnboardingPage::World:
			labels[0] = "Start";
			labels[1] = "Choose again";
			return 2;
	}
	return 0;
}

// The pressed button `button` of the page now: the choice and the page move
// on. A button the page does not have changes nothing (Again).
inline OnboardingNext OnboardingAnswer(OnboardingState& s, UInt32 button) {
	if (button >= OnboardingButtonCount(s)) {
		return OnboardingNext::Again;
	}
	switch (s.page) {
		case OnboardingPage::Welcome:
			if (button == 0) {
				s.page = OnboardingPage::Walkers;
				return OnboardingNext::Show;
			}
			// The two shortcuts go straight to the summary.
			s.walkers = button == 1 ? WalkersMode::Zombies : WalkersMode::Off;
			s.daedra = DaedraMode::Off;
			s.page = OnboardingPage::World;
			return OnboardingNext::Show;
		case OnboardingPage::Walkers:
			s.walkers = button == 0 ? WalkersMode::Zombies : (button == 1 ? WalkersMode::Apocalypse : WalkersMode::Off);
			s.page = OnboardingPage::Daedra;
			return OnboardingNext::Show;
		case OnboardingPage::Daedra:
			// Without walkers "enemies" and "allies" are one: the Daedra alone.
			s.daedra = button == 0 ? DaedraMode::Off : (button == 1 ? DaedraMode::Enemies : DaedraMode::Allies);
			s.page = OnboardingPage::World;
			return OnboardingNext::Show;
		case OnboardingPage::World:
			if (button == 0) {
				return OnboardingNext::Finish;
			}
			s.page = OnboardingPage::Welcome;
			return OnboardingNext::Show;
	}
	return OnboardingNext::Again;
}

inline const char* OnboardingWalkersWord(WalkersMode mode) {
	switch (mode) {
		case WalkersMode::Off: return "none";
		case WalkersMode::Zombies: return "zombies only";
		case WalkersMode::Apocalypse: return "apocalypse";
	}
	return "?";
}

inline const char* OnboardingDaedraWord(WalkersMode walkers, DaedraMode mode) {
	switch (mode) {
		case DaedraMode::Off: return "none";
		case DaedraMode::Enemies: return walkers == WalkersMode::Off ? "out of the gates" : "enemies of the walkers";
		case DaedraMode::Allies: return walkers == WalkersMode::Off ? "out of the gates" : "allies of the walkers";
	}
	return "?";
}

// No page may be longer than this: the game's own limit for a message box's
// text is not read yet, so the pages stay well under what a box shows.
inline constexpr UInt32 kOnboardingTextMost = 400;

// The text of the page now (4.24a's texts; each begins with its title and a
// colon). Returns its length, 0 when it does not fit `capacity`.
inline UInt32 OnboardingText(const OnboardingState& s, char* out, UInt32 capacity) {
	int n = 0;
	switch (s.page) {
		case OnboardingPage::Welcome:
			n = std::snprintf(out, capacity,
			                  "OblivionWalkers: This mod changes what threatens Cyrodiil. Pick what you want. You can "
			                  "change it any time in the mod menu (LINK).");
			break;
		case OnboardingPage::Walkers:
			n = std::snprintf(out, capacity,
			                  "Walkers: Walkers are undead that only stay down once their head is off. Until then they "
			                  "get up again. They grab and bite, notice you slowly, and come to noise. Their blood on "
			                  "you hides you from them. Zombies only: the zombies you meet become walkers. "
			                  "Apocalypse: walkers everywhere, the dead rise, hordes march on the cities.");
			break;
		case OnboardingPage::Daedra:
			n = std::snprintf(out, capacity,
			                  "Daedra: Out of every open Oblivion gate a Daedra war band marches on the nearest city. "
			                  "Close the gate and its bands stop.%s",
			                  DaedraPageFolds(s) ? "" : " Do they fight the walkers or stand with them?");
			break;
		case OnboardingPage::World:
			n = std::snprintf(out, capacity,
			                  "Your world: Walkers: %s. Daedra: %s. Change it any time in LINK, page 'Who comes'.",
			                  OnboardingWalkersWord(s.walkers), OnboardingDaedraWord(s.walkers, s.daedra));
			break;
	}
	return n > 0 && static_cast<UInt32>(n) < capacity ? static_cast<UInt32>(n) : 0;
}

// The page's name for the log.
inline const char* OnboardingPageName(OnboardingPage page) {
	switch (page) {
		case OnboardingPage::Welcome: return "Welcome";
		case OnboardingPage::Walkers: return "Walkers";
		case OnboardingPage::Daedra: return "Daedra";
		case OnboardingPage::World: return "Your world";
	}
	return "?";
}

// The page's number as 4.24a counts them (the necromancers' is 4, not built).
inline UInt32 OnboardingPageNumber(OnboardingPage page) {
	switch (page) {
		case OnboardingPage::Welcome: return 1;
		case OnboardingPage::Walkers: return 2;
		case OnboardingPage::Daedra: return 3;
		case OnboardingPage::World: return 5;
	}
	return 0;
}

// The text of a button's answer for the log ("Apocalypse", "Start").
inline const char* OnboardingButtonLabel(const OnboardingState& s, UInt32 button) {
	const char* labels[3] = {};
	const UInt32 count = OnboardingButtons(s, labels);
	return button < count ? labels[button] : "(no such button)";
}

// --- when the chain may open a box -----------------------------------------

// The first box opens this long after a load or a new game (a few seconds of
// play, not the load's own frames); later boxes of the chain a moment after
// the answer. Both are guesses to be set by the harness and the headset.
inline constexpr float kOnboardingAfterLoadSeconds = 5.0f;
inline constexpr float kOnboardingBetweenSeconds = 0.5f;

// The HUD's menus (xOBSE GameMenus.h: HUDMain 0x3EC, HUDInfo 0x3ED, HUDReticle
// 0x3EE, HUDSubtitle 0x3F2) are open all through play; any other menu id in
// the interface's stack (ten dwords, Horde.cpp MenuOpen) is a menu the player
// is in - the race menu, the inventory, a dialogue, a message box, a load.
inline bool IsHudMenu(UInt32 id) { return id == 0x3EC || id == 0x3ED || id == 0x3EE || id == 0x3F2; }

// The first menu in the stack that is not the HUD's; 0 when there is none.
inline UInt32 FirstOtherMenu(const UInt32* ids, UInt32 count) {
	for (UInt32 k = 0; ids != nullptr && k < count; ++k) {
		if (ids[k] != 0 && !IsHudMenu(ids[k])) {
			return ids[k];
		}
	}
	return 0;
}

// Whether the next box of the chain may open now: `show` is set, the player
// is in a game, no menu but the HUD's is open (`openMenu`, FirstOtherMenu),
// the disguise's box is not up, and the wait is over - `seconds` since the
// load for the first box (`chainBegun` false), since the last answer for the
// others.
inline bool OnboardingAskDue(bool show, bool inGame, UInt32 openMenu, bool disguiseBoxUp, bool chainBegun,
                             float seconds) {
	if (!show || !inGame || openMenu != 0 || disguiseBoxUp) {
		return false;
	}
	return seconds >= (chainBegun ? kOnboardingBetweenSeconds : kOnboardingAfterLoadSeconds);
}

// --- the test answers ([Onboarding] TestAnswers) ---------------------------

inline constexpr UInt32 kOnboardingTestAnswersMost = 16;
// The buttons one a second press while a box is up (harness: the plugin
// sends the console's SetButtonPressed, as harness/disguise.txt does).
inline constexpr float kOnboardingTestAnswerSeconds = 1.0f;

// "1, 1,2 ,0" -> 1 1 2 0: single digits 0 to 9 separated by commas; any other
// piece is skipped. Returns how many were read into `out`.
inline UInt32 ParseTestAnswers(const char* text, UInt8* out, UInt32 most) {
	UInt32 n = 0;
	if (text == nullptr) {
		return 0;
	}
	const char* p = text;
	while (*p != '\0' && n < most) {
		const char* end = p;
		while (*end != '\0' && *end != ',') {
			++end;
		}
		const char* a = p;
		const char* b = end;
		while (a < b && (*a == ' ' || *a == '\t')) {
			++a;
		}
		while (b > a && (b[-1] == ' ' || b[-1] == '\t')) {
			--b;
		}
		if (b - a == 1 && *a >= '0' && *a <= '9') {
			out[n++] = static_cast<UInt8>(*a - '0');
		}
		p = *end == ',' ? end + 1 : end;
	}
	return n;
}

// The next test answer is due: some are left and a second passed since the
// box was shown (or the last answer pressed).
inline bool OnboardingTestAnswerDue(UInt32 count, UInt32 index, float sinceShown) {
	return index < count && sinceShown >= kOnboardingTestAnswerSeconds;
}

}  // namespace walkers
