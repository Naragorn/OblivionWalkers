// Every flow of the onboarding (docs/walkers-spec.md 4.24a; Nadi,
// 2026-11-20): the pages and their buttons, every answer on every page, the
// two shortcuts on the first page, "Choose again", the third page folding to
// two buttons without walkers, the summary lines, the text lengths, when the
// first box may open and the test answers.

#include "OnboardingLogic.h"

#include <cstdio>
#include <cstring>
#include <string>

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf("  %s  %s\n", condition ? "ok  " : "FAIL", what);
	if (!condition) {
		++g_failures;
	}
}

using walkers::DaedraMode;
using walkers::OnboardingNext;
using walkers::OnboardingPage;
using walkers::OnboardingState;
using walkers::WalkersMode;

OnboardingState At(OnboardingPage page, WalkersMode w = WalkersMode::Zombies, DaedraMode d = DaedraMode::Off) {
	OnboardingState s;
	s.page = page;
	s.walkers = w;
	s.daedra = d;
	return s;
}

std::string Labels(const OnboardingState& s) {
	const char* labels[3] = {};
	const UInt32 n = walkers::OnboardingButtons(s, labels);
	std::string text;
	for (UInt32 i = 0; i < n; ++i) {
		text += (i > 0 ? " | " : "") + std::string(labels[i]);
	}
	return text;
}

void TestStart() {
	std::printf("The start\n");
	const OnboardingState s;
	Check(s.page == OnboardingPage::Welcome && s.walkers == WalkersMode::Zombies && s.daedra == DaedraMode::Off,
	      "the first page, the mod's own defaults: the game's zombies, no Daedra");
	walkers::OnboardingSettings settings;
	Check(settings.show && settings.testAnswers[0] == '\0', "[Onboarding] Show=1 by default, no test answers");
}

void TestButtons() {
	std::printf("The buttons of each page\n");
	Check(Labels(At(OnboardingPage::Welcome)) == "Choose | Just the game's zombies | Turn the mod off" &&
	          walkers::OnboardingButtonCount(At(OnboardingPage::Welcome)) == 3,
	      "page 1: Choose / Just the game's zombies / Turn the mod off");
	Check(Labels(At(OnboardingPage::Walkers)) == "Zombies only | Apocalypse | No walkers" &&
	          walkers::OnboardingButtonCount(At(OnboardingPage::Walkers)) == 3,
	      "page 2: Zombies only / Apocalypse / No walkers");
	Check(Labels(At(OnboardingPage::Daedra, WalkersMode::Zombies)) ==
	          "No Daedra | Daedra, enemies of the walkers | Daedra, allies of the walkers",
	      "page 3 with Zombies: three buttons");
	Check(Labels(At(OnboardingPage::Daedra, WalkersMode::Apocalypse)) ==
	          "No Daedra | Daedra, enemies of the walkers | Daedra, allies of the walkers" &&
	          walkers::OnboardingButtonCount(At(OnboardingPage::Daedra, WalkersMode::Apocalypse)) == 3,
	      "page 3 with the apocalypse: three buttons");
	Check(Labels(At(OnboardingPage::Daedra, WalkersMode::Off)) == "No Daedra | Daedra" &&
	          walkers::OnboardingButtonCount(At(OnboardingPage::Daedra, WalkersMode::Off)) == 2,
	      "page 3 with no walkers: folded to No Daedra / Daedra");
	Check(Labels(At(OnboardingPage::World)) == "Start | Choose again" &&
	          walkers::OnboardingButtonCount(At(OnboardingPage::World)) == 2,
	      "page 5: Start / Choose again");
}

void TestPage1() {
	std::printf("Page 1\n");
	OnboardingState s;
	Check(walkers::OnboardingAnswer(s, 0) == OnboardingNext::Show && s.page == OnboardingPage::Walkers,
	      "Choose: on to the walkers' page");
	s = At(OnboardingPage::Welcome, WalkersMode::Apocalypse, DaedraMode::Allies);
	Check(walkers::OnboardingAnswer(s, 1) == OnboardingNext::Show && s.page == OnboardingPage::World &&
	          s.walkers == WalkersMode::Zombies && s.daedra == DaedraMode::Off,
	      "Just the game's zombies: Walkers 1, the rest off, straight to the summary");
	s = At(OnboardingPage::Welcome, WalkersMode::Apocalypse, DaedraMode::Allies);
	Check(walkers::OnboardingAnswer(s, 2) == OnboardingNext::Show && s.page == OnboardingPage::World &&
	          s.walkers == WalkersMode::Off && s.daedra == DaedraMode::Off,
	      "Turn the mod off: everything 0, straight to the summary");
	s = At(OnboardingPage::Welcome);
	Check(walkers::OnboardingAnswer(s, 3) == OnboardingNext::Again && s.page == OnboardingPage::Welcome,
	      "a button the page does not have: asked again, nothing changed");
}

void TestPage2() {
	std::printf("Page 2\n");
	const WalkersMode expect[3] = {WalkersMode::Zombies, WalkersMode::Apocalypse, WalkersMode::Off};
	const char* names[3] = {"Zombies only: Walkers 1, on to the Daedra", "Apocalypse: Walkers 2, on to the Daedra",
	                        "No walkers: Walkers 0, on to the Daedra"};
	for (UInt32 b = 0; b < 3; ++b) {
		OnboardingState s = At(OnboardingPage::Walkers, WalkersMode::Zombies, DaedraMode::Enemies);
		s.walkers = b == 0 ? WalkersMode::Off : WalkersMode::Zombies;  // so each answer is seen to set it
		Check(walkers::OnboardingAnswer(s, b) == OnboardingNext::Show && s.page == OnboardingPage::Daedra &&
		          s.walkers == expect[b] && s.daedra == DaedraMode::Enemies,
		      names[b]);
	}
	OnboardingState s = At(OnboardingPage::Walkers);
	Check(walkers::OnboardingAnswer(s, 3) == OnboardingNext::Again && s.page == OnboardingPage::Walkers,
	      "a fourth button: asked again");
}

void TestPage3() {
	std::printf("Page 3\n");
	const DaedraMode expect[3] = {DaedraMode::Off, DaedraMode::Enemies, DaedraMode::Allies};
	const char* names[3] = {"No Daedra: Daedra 0, on to the necromancers", "enemies: Daedra 1, on to the necromancers",
	                        "allies: Daedra 2, on to the necromancers"};
	for (UInt32 b = 0; b < 3; ++b) {
		OnboardingState s = At(OnboardingPage::Daedra, WalkersMode::Apocalypse, DaedraMode::Allies);
		s.daedra = b == 0 ? DaedraMode::Allies : DaedraMode::Off;
		Check(walkers::OnboardingAnswer(s, b) == OnboardingNext::Show && s.page == OnboardingPage::Necromancers &&
		          s.daedra == expect[b] && s.walkers == WalkersMode::Apocalypse,
		      names[b]);
	}
	OnboardingState none = At(OnboardingPage::Daedra, WalkersMode::Off);
	Check(walkers::OnboardingAnswer(none, 0) == OnboardingNext::Show && none.daedra == DaedraMode::Off &&
	          none.page == OnboardingPage::Necromancers,
	      "without walkers, No Daedra: Daedra 0");
	none = At(OnboardingPage::Daedra, WalkersMode::Off);
	Check(walkers::OnboardingAnswer(none, 1) == OnboardingNext::Show && none.daedra == DaedraMode::Enemies &&
	          none.page == OnboardingPage::Necromancers,
	      "without walkers, Daedra: Daedra 1 (enemies and allies are one then)");
	none = At(OnboardingPage::Daedra, WalkersMode::Off);
	Check(walkers::OnboardingAnswer(none, 2) == OnboardingNext::Again && none.page == OnboardingPage::Daedra,
	      "without walkers there is no third button: asked again");
}

void TestPage4() {
	std::printf("Page 4 (the necromancers, 4.23 B)\n");
	Check(Labels(At(OnboardingPage::Necromancers)) == "No necromancers | Necromancers" &&
	          walkers::OnboardingButtonCount(At(OnboardingPage::Necromancers)) == 2,
	      "two buttons");
	OnboardingState s = At(OnboardingPage::Necromancers, WalkersMode::Off);
	Check(walkers::OnboardingAnswer(s, 1) == OnboardingNext::Show && s.necromancers && s.page == OnboardingPage::World,
	      "Necromancers: on, on to the summary");
	s = At(OnboardingPage::Necromancers);
	s.necromancers = true;
	Check(walkers::OnboardingAnswer(s, 0) == OnboardingNext::Show && !s.necromancers && s.page == OnboardingPage::World,
	      "No necromancers: off, on to the summary");
	s = At(OnboardingPage::Necromancers);
	Check(walkers::OnboardingAnswer(s, 2) == OnboardingNext::Again && s.page == OnboardingPage::Necromancers,
	      "a third button: asked again");
	s = At(OnboardingPage::Welcome);
	s.necromancers = true;
	walkers::OnboardingAnswer(s, 1);
	Check(!s.necromancers && s.page == OnboardingPage::World, "a shortcut of page 1: no necromancers");
}

void TestPage5() {
	std::printf("Page 5\n");
	OnboardingState s = At(OnboardingPage::World, WalkersMode::Apocalypse, DaedraMode::Enemies);
	Check(walkers::OnboardingAnswer(s, 0) == OnboardingNext::Finish && s.walkers == WalkersMode::Apocalypse &&
	          s.daedra == DaedraMode::Enemies,
	      "Start: the chain ends, the choice stands");
	s = At(OnboardingPage::World, WalkersMode::Apocalypse, DaedraMode::Enemies);
	Check(walkers::OnboardingAnswer(s, 1) == OnboardingNext::Show && s.page == OnboardingPage::Welcome,
	      "Choose again: back to the first page");
	Check(walkers::OnboardingAnswer(s, 0) == OnboardingNext::Show && s.page == OnboardingPage::Walkers,
	      "and Choose goes on from there");
	s = At(OnboardingPage::World);
	Check(walkers::OnboardingAnswer(s, 2) == OnboardingNext::Again && s.page == OnboardingPage::World,
	      "a third button: asked again");
}

void TestPaths() {
	std::printf("Whole paths\n");
	OnboardingState s;
	walkers::OnboardingAnswer(s, 0);
	walkers::OnboardingAnswer(s, 1);
	walkers::OnboardingAnswer(s, 1);
	walkers::OnboardingAnswer(s, 0);
	Check(s.page == OnboardingPage::World && s.walkers == WalkersMode::Apocalypse && s.daedra == DaedraMode::Enemies &&
	          !s.necromancers && walkers::OnboardingAnswer(s, 0) == OnboardingNext::Finish,
	      "Choose, Apocalypse, Daedra enemies, No necromancers, Start: Walkers 2, Daedra 1, Necromancers 0");
	s = OnboardingState{};
	walkers::OnboardingAnswer(s, 0);
	walkers::OnboardingAnswer(s, 2);
	walkers::OnboardingAnswer(s, 0);
	walkers::OnboardingAnswer(s, 1);
	Check(s.page == OnboardingPage::World && s.walkers == WalkersMode::Off && s.daedra == DaedraMode::Off &&
	          s.necromancers && walkers::OnboardingAnswer(s, 0) == OnboardingNext::Finish,
	      "Choose, No walkers, No Daedra, Necromancers, Start: the necromancers alone");
	s = OnboardingState{};
	walkers::OnboardingAnswer(s, 2);
	Check(walkers::OnboardingAnswer(s, 0) == OnboardingNext::Finish && s.walkers == WalkersMode::Off &&
	          s.daedra == DaedraMode::Off,
	      "Turn the mod off, Start: all 0");
	s = OnboardingState{};
	walkers::OnboardingAnswer(s, 0);
	walkers::OnboardingAnswer(s, 2);
	walkers::OnboardingAnswer(s, 1);
	walkers::OnboardingAnswer(s, 0);
	Check(s.walkers == WalkersMode::Off && s.daedra == DaedraMode::Enemies && !s.necromancers,
	      "Choose, No walkers, Daedra, No necromancers: the Daedra alone (Walkers 0, Daedra 1)");
	walkers::OnboardingAnswer(s, 1);  // choose again
	walkers::OnboardingAnswer(s, 1);  // the shortcut
	Check(s.page == OnboardingPage::World && s.walkers == WalkersMode::Zombies && s.daedra == DaedraMode::Off,
	      "Choose again, Just the game's zombies: the shortcut overwrites the earlier choice");
}

void TestTexts() {
	std::printf("The texts\n");
	char text[512];
	const OnboardingState pages[] = {At(OnboardingPage::Welcome),
	                                 At(OnboardingPage::Walkers),
	                                 At(OnboardingPage::Daedra, WalkersMode::Zombies),
	                                 At(OnboardingPage::Daedra, WalkersMode::Off),
	                                 At(OnboardingPage::Necromancers),
	                                 At(OnboardingPage::World, WalkersMode::Apocalypse, DaedraMode::Allies),
	                                 At(OnboardingPage::World, WalkersMode::Off, DaedraMode::Enemies)};
	bool short_enough = true;
	for (const OnboardingState& p : pages) {
		const UInt32 n = walkers::OnboardingText(p, text, sizeof(text));
		short_enough = short_enough && n > 0 && n < walkers::kOnboardingTextMost && std::strlen(text) == n;
	}
	Check(short_enough, "every page's text is there and under 400 characters");
	walkers::OnboardingText(At(OnboardingPage::Welcome), text, sizeof(text));
	Check(std::strstr(text, "This mod changes what threatens Cyrodiil.") != nullptr &&
	          std::strstr(text, "LINK") != nullptr,
	      "page 1 says what the mod does and where to change it");
	walkers::OnboardingText(At(OnboardingPage::Walkers), text, sizeof(text));
	Check(std::strstr(text, "Zombies only: the zombies you meet become walkers.") != nullptr &&
	          std::strstr(text, "Apocalypse: walkers everywhere, the dead rise, hordes march on the cities.") != nullptr,
	      "page 2 explains the two modes");
	walkers::OnboardingText(At(OnboardingPage::Daedra, WalkersMode::Zombies), text, sizeof(text));
	Check(std::strstr(text, "Do they fight the walkers or stand with them?") != nullptr &&
	          std::strstr(text, "Close the gate and its bands stop.") != nullptr,
	      "page 3 with walkers asks whether they fight or stand with them");
	walkers::OnboardingText(At(OnboardingPage::Daedra, WalkersMode::Off), text, sizeof(text));
	Check(std::strstr(text, "Do they fight") == nullptr && std::strstr(text, "Close the gate and its bands stop.") != nullptr,
	      "page 3 without walkers does not ask");
	walkers::OnboardingText(At(OnboardingPage::World, WalkersMode::Zombies, DaedraMode::Off), text, sizeof(text));
	Check(std::strstr(text, "Walkers: zombies only. Daedra: none.") != nullptr &&
	          std::strstr(text, "Change it any time in LINK, page 'Who comes'.") != nullptr,
	      "page 5: one line a switch, zombies only and no Daedra");
	walkers::OnboardingText(At(OnboardingPage::World, WalkersMode::Apocalypse, DaedraMode::Enemies), text, sizeof(text));
	Check(std::strstr(text, "Walkers: apocalypse. Daedra: enemies of the walkers.") != nullptr, "page 5: the apocalypse, enemies");
	walkers::OnboardingText(At(OnboardingPage::World, WalkersMode::Zombies, DaedraMode::Allies), text, sizeof(text));
	Check(std::strstr(text, "Daedra: allies of the walkers.") != nullptr, "page 5: allies");
	walkers::OnboardingText(At(OnboardingPage::World, WalkersMode::Off, DaedraMode::Enemies), text, sizeof(text));
	Check(std::strstr(text, "Walkers: none. Daedra: out of the gates.") != nullptr,
	      "page 5 without walkers: the Daedra out of the gates");
	walkers::OnboardingText(At(OnboardingPage::World, WalkersMode::Off, DaedraMode::Allies), text, sizeof(text));
	Check(std::strstr(text, "Daedra: out of the gates.") != nullptr, "page 5 without walkers, allies: the same");
	walkers::OnboardingText(At(OnboardingPage::Necromancers), text, sizeof(text));
	Check(std::strstr(text, "Necromancers: A few rival necromancers hold dungeons full of walkers.") != nullptr &&
	          std::strstr(text, "Kill one and refugees move into his place.") != nullptr,
	      "page 4 says what the necromancers do");
	OnboardingState world = At(OnboardingPage::World, WalkersMode::Off, DaedraMode::Off);
	world.necromancers = true;
	walkers::OnboardingText(world, text, sizeof(text));
	Check(std::strstr(text, "Walkers: none. Daedra: none. Necromancers: their realms.") != nullptr,
	      "page 5 names the necromancers' choice");
	walkers::OnboardingText(At(OnboardingPage::World), text, sizeof(text));
	Check(std::strstr(text, "Necromancers: none.") != nullptr, "and their absence");
	char small[8];
	Check(walkers::OnboardingText(At(OnboardingPage::Walkers), small, sizeof(small)) == 0,
	      "a buffer too small: nothing, not half a text");
}

void TestNames() {
	std::printf("The names for the log\n");
	Check(std::strcmp(walkers::OnboardingPageName(OnboardingPage::Welcome), "Welcome") == 0 &&
	          std::strcmp(walkers::OnboardingPageName(OnboardingPage::Walkers), "Walkers") == 0 &&
	          std::strcmp(walkers::OnboardingPageName(OnboardingPage::Daedra), "Daedra") == 0 &&
	          std::strcmp(walkers::OnboardingPageName(OnboardingPage::Necromancers), "Necromancers") == 0 &&
	          std::strcmp(walkers::OnboardingPageName(OnboardingPage::World), "Your world") == 0,
	      "page names");
	Check(walkers::OnboardingPageNumber(OnboardingPage::Welcome) == 1 && walkers::OnboardingPageNumber(OnboardingPage::Walkers) == 2 &&
	          walkers::OnboardingPageNumber(OnboardingPage::Daedra) == 3 &&
	          walkers::OnboardingPageNumber(OnboardingPage::Necromancers) == 4 &&
	          walkers::OnboardingPageNumber(OnboardingPage::World) == 5,
	      "page numbers as 4.24a counts them");
	Check(std::strcmp(walkers::OnboardingButtonLabel(At(OnboardingPage::Walkers), 1), "Apocalypse") == 0 &&
	          std::strcmp(walkers::OnboardingButtonLabel(At(OnboardingPage::Walkers), 5), "(no such button)") == 0,
	      "a button's label, or that there is none");
	Check(std::strcmp(walkers::OnboardingWalkersWord(WalkersMode::Off), "none") == 0 &&
	          std::strcmp(walkers::OnboardingWalkersWord(WalkersMode::Zombies), "zombies only") == 0 &&
	          std::strcmp(walkers::OnboardingWalkersWord(WalkersMode::Apocalypse), "apocalypse") == 0,
	      "the walkers' words");
}

void TestMenus() {
	std::printf("When a menu is open\n");
	const UInt32 none[10] = {};
	const UInt32 hud[10] = {0x3EC, 0x3ED, 0x3EE, 0x3F2};
	const UInt32 inventory[10] = {0x3EC, 0x3ED, 0x3EE, 0x3EA};
	const UInt32 dialogue[10] = {0x3EC, 0x3F1};
	const UInt32 box[10] = {0x3EC, 0x3E9};
	Check(walkers::FirstOtherMenu(none, 10) == 0 && walkers::FirstOtherMenu(hud, 10) == 0 &&
	          walkers::FirstOtherMenu(nullptr, 10) == 0,
	      "no menu, only the HUD's, no stack: none open");
	Check(walkers::FirstOtherMenu(inventory, 10) == 0x3EA && walkers::FirstOtherMenu(dialogue, 10) == 0x3F1 &&
	          walkers::FirstOtherMenu(box, 10) == 0x3E9,
	      "the inventory, a dialogue, a message box: open");
	Check(walkers::IsHudMenu(0x3EC) && walkers::IsHudMenu(0x3ED) && walkers::IsHudMenu(0x3EE) && walkers::IsHudMenu(0x3F2) &&
	          !walkers::IsHudMenu(0x3EF),
	      "the HUD's four; the loading menu is not one");
}

void TestDue() {
	std::printf("When the first box may open\n");
	using walkers::OnboardingAskDue;
	Check(OnboardingAskDue(true, true, 0, false, false, 5.0f), "show, in a game, no menu, no other box, five seconds: due");
	Check(!OnboardingAskDue(false, true, 0, false, false, 60.0f), "Show=0: never");
	Check(!OnboardingAskDue(true, false, 0, false, false, 60.0f), "not in a game (the main menu, a load): not yet");
	Check(!OnboardingAskDue(true, true, 0x3EA, false, false, 60.0f), "a menu open (the inventory): not over it");
	Check(!OnboardingAskDue(true, true, 0, true, false, 60.0f), "the disguise's box up: not over it");
	Check(!OnboardingAskDue(true, true, 0, false, false, 4.9f), "a few seconds after the load: not yet");
	Check(OnboardingAskDue(true, true, 0, false, true, 0.5f) && !OnboardingAskDue(true, true, 0, false, true, 0.4f),
	      "later boxes of the chain: half a second after the answer");
	Check(!OnboardingAskDue(true, true, 0x3E9, false, true, 5.0f), "a menu open between two boxes: waits");
}

void TestAnswers() {
	std::printf("The test answers\n");
	UInt8 out[16] = {};
	Check(walkers::ParseTestAnswers("0,1,2,1", out, 16) == 4 && out[0] == 0 && out[1] == 1 && out[2] == 2 && out[3] == 1,
	      "comma-separated digits");
	Check(walkers::ParseTestAnswers(" 1 , 2 ,0", out, 16) == 3 && out[0] == 1 && out[1] == 2 && out[2] == 0, "blanks around them");
	Check(walkers::ParseTestAnswers("1,x,12,,2", out, 16) == 2 && out[0] == 1 && out[1] == 2, "other pieces are skipped");
	Check(walkers::ParseTestAnswers("", out, 16) == 0 && walkers::ParseTestAnswers(nullptr, out, 16) == 0, "none: none");
	Check(walkers::ParseTestAnswers("1,1,1,1,1", out, 3) == 3, "no more than fit");
	Check(walkers::OnboardingTestAnswerDue(3, 0, 1.0f) && !walkers::OnboardingTestAnswerDue(3, 0, 0.9f) &&
	          !walkers::OnboardingTestAnswerDue(3, 3, 5.0f) && !walkers::OnboardingTestAnswerDue(0, 0, 5.0f),
	      "one a second while some are left");
}

}  // namespace

int main() {
	TestStart();
	TestButtons();
	TestPage1();
	TestPage2();
	TestPage3();
	TestPage4();
	TestPage5();
	TestPaths();
	TestTexts();
	TestNames();
	TestMenus();
	TestDue();
	TestAnswers();
	if (g_failures != 0) {
		std::printf("%d failed\n", g_failures);
		return 1;
	}
	std::printf("all passed\n");
	return 0;
}
