// Every flow of the walkers' news (docs/walkers-spec.md 4.9): which edition of
// the Black Horse Courier the inns show - the outbreak in general, a horde
// heading for a city, a city whose gates the walkers attacked, or several -
// and the attacks kept in the co-save.

#include "NewsLogic.h"

#include <cmath>
#include <cstdio>

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf("  %s  %s\n", condition ? "ok  " : "FAIL", what);
	if (!condition) {
		++g_failures;
	}
}

using walkers::Edition;
using walkers::EditionKind;
using walkers::NewsState;

constexpr UInt32 kAnvil = 0;
constexpr UInt32 kBruma = 2;
constexpr UInt32 kSkingrad = 6;

bool Is(const Edition& e, EditionKind kind, int city) { return e.kind == kind && e.city == city; }

// The edition for `now` with a horde heading for `heading` seen at `seenAt`
// (-1: none seen), RecentDays 3, HeadingHoldDays 0.5.
Edition Choose(NewsState news, float now, int heading = -1, float seenAt = -1.0f) {
	if (heading != -1) {
		walkers::NoteHeading(news, heading, seenAt);
	}
	return walkers::ChooseEdition(news, now, 3.0f, 0.5f);
}

void TestChoose() {
	std::printf("Which edition\n");
	NewsState news;
	Check(Is(Choose(news, 10.0f), EditionKind::General, -1), "nothing attacked, no horde known: the outbreak in general");
	Check(Is(Choose(news, 10.0f, static_cast<int>(kBruma), 10.0f), EditionKind::Approaching, static_cast<int>(kBruma)),
	      "a horde near the player heading for Bruma: 'a horde seen near Bruma'");
	walkers::NoteCityAttack(news, kAnvil, 9.0f);
	Check(Is(Choose(news, 10.0f, static_cast<int>(kBruma), 10.0f), EditionKind::Attack, static_cast<int>(kAnvil)),
	      "Anvil's gates attacked a day ago: Anvil's edition, whatever horde heads elsewhere");
	Check(Is(Choose(news, 12.0f), EditionKind::Attack, static_cast<int>(kAnvil)),
	      "three days later: still Anvil's (RecentDays reached, not passed)");
	Check(Is(Choose(news, 12.5f), EditionKind::General, -1), "past RecentDays: old news, the outbreak in general again");
	walkers::NoteCityAttack(news, kSkingrad, 12.0f);
	walkers::NoteCityAttack(news, kAnvil, 12.2f);
	Check(Is(Choose(news, 12.5f), EditionKind::Several, -1), "Anvil and Skingrad both attacked lately: 'the cities besieged'");
	NewsState future;
	walkers::NoteCityAttack(future, kBruma, 20.0f);
	Check(Is(Choose(future, 19.0f), EditionKind::Attack, static_cast<int>(kBruma)),
	      "an attack noted later than now (the clock read before the note): counted as recent");
	Check(Is(Choose(news, 12.5f, 99, 12.5f), EditionKind::Several, -1) &&
	          Is(Choose(NewsState{}, 1.0f, 99, 1.0f), EditionKind::General, -1),
	      "a heading city out of range is ignored");
}

void TestHeadingHeld() {
	std::printf("A horde's heading held (Windows run 2026-10-26: the sheets flipped 7 times in minutes)\n");
	const int ic = 7;
	Check(Is(Choose(NewsState{}, 3.2f, ic, 3.0f), EditionKind::Approaching, ic),
	      "the horde out of sight a fifth of a day later: still 'heading for the Imperial City'");
	Check(Is(Choose(NewsState{}, 3.5f, ic, 3.0f), EditionKind::Approaching, ic),
	      "HeadingHoldDays reached, not passed: still held");
	Check(Is(Choose(NewsState{}, 3.6f, ic, 3.0f), EditionKind::General, -1),
	      "past HeadingHoldDays without another sighting: the outbreak in general");
	Check(Is(Choose(NewsState{}, 2.9f, ic, 3.0f), EditionKind::Approaching, ic),
	      "seen later than now (the clock read before the note): held");
	NewsState news;
	walkers::NoteHeading(news, ic, 3.0f);
	walkers::NoteHeading(news, static_cast<int>(kBruma), 3.1f);
	Check(Is(walkers::ChooseEdition(news, 3.2f, 3.0f, 0.5f), EditionKind::Approaching, static_cast<int>(kBruma)),
	      "a newer sighting toward another city: that one");
	walkers::NoteHeading(news, -1, 3.3f);
	Check(news.headingCity == static_cast<int>(kBruma) && news.headingDays == 3.1f,
	      "no horde seen (-1): the last sighting kept, not wiped");
	Check(Is(walkers::ChooseEdition(news, 3.1f, 3.0f, 0.0f), EditionKind::Approaching, static_cast<int>(kBruma)) &&
	          Is(walkers::ChooseEdition(news, 3.11f, 3.0f, 0.0f), EditionKind::General, -1),
	      "HeadingHoldDays 0: only a sighting of this very moment (a hundredth of a day later is past it)");
}

void TestNote() {
	std::printf("Noting attacks\n");
	NewsState news;
	walkers::NoteCityAttack(news, kAnvil, 5.0f);
	walkers::NoteCityAttack(news, kAnvil, 4.0f);
	Check(news.lastAttackDays[kAnvil] == 5.0f, "the latest attack is kept, an earlier note does not win");
	walkers::NoteCityAttack(news, 99, 5.0f);
	bool untouched = true;
	for (UInt32 c = 0; c < walkers::kNewsCities; ++c) {
		untouched = untouched && (c == kAnvil || news.lastAttackDays[c] < 0.0f);
	}
	Check(untouched, "a city out of range changes nothing");
}

void TestIndex() {
	std::printf("Edition numbers\n");
	bool seen[walkers::kEditionCount] = {};
	bool unique = true;
	bool inRange = true;
	auto mark = [&](Edition e) {
		const UInt32 i = walkers::EditionIndex(e);
		inRange = inRange && i < walkers::kEditionCount;
		if (i < walkers::kEditionCount) {
			unique = unique && !seen[i];
			seen[i] = true;
		}
	};
	mark(Edition{EditionKind::General, -1});
	mark(Edition{EditionKind::Several, -1});
	for (int c = 0; c < static_cast<int>(walkers::kNewsCities); ++c) {
		mark(Edition{EditionKind::Approaching, c});
		mark(Edition{EditionKind::Attack, c});
	}
	bool all = true;
	for (bool s : seen) {
		all = all && s;
	}
	Check(unique && inRange && all, "18 editions, each its own number: general 0, approaching 1-8, attack 9-16, several 17");
	Check(walkers::EditionIndex(Edition{EditionKind::General, -1}) == 0 &&
	          walkers::EditionIndex(Edition{EditionKind::Approaching, 0}) == 1 &&
	          walkers::EditionIndex(Edition{EditionKind::Attack, 7}) == 16 &&
	          walkers::EditionIndex(Edition{EditionKind::Several, -1}) == 17,
	      "the numbers tools/esp-write.py writes the books in");
}

void TestGameTime() {
	std::printf("The game's clock\n");
	Check(walkers::GameTimeDays(1.0f, 12.0f) == 1.5f,
	      "GameDaysPassed in whole days plus GameHour: day 1, noon = 1.5");
	Check(std::fabs(walkers::GameTimeDays(1.75f, 18.0f) - 1.75f) < 1e-4f,
	      "GameDaysPassed with its fraction: the fraction not counted twice");
	Check(walkers::GameTimeDays(3.0f, -2.0f) == 3.0f && walkers::GameTimeDays(3.0f, 30.0f) == 3.0f,
	      "an hour off the clock (below 0, 24 or more): the day alone");
	Check(walkers::GameTimeDays(-1.0f, 5.0f) < 0.0f, "no clock (no global): below 0");
}

void TestImage() {
	std::printf("In the co-save\n");
	NewsState news;
	walkers::NoteCityAttack(news, kBruma, 33.5f);
	UInt8 image[walkers::kNewsImageBytes];
	const UInt32 length = walkers::NewsImage(news, image, sizeof(image));
	NewsState back;
	Check(length == walkers::kNewsImageBytes && walkers::ReadNewsImage(back, image, length) &&
	          back.lastAttackDays[kBruma] == 33.5f && back.lastAttackDays[kAnvil] < 0.0f,
	      "read back: the attacks as written");
	Check(back.headingCity == -1 && back.headingDays < 0.0f, "no sighting written: none read");
	walkers::NoteHeading(news, 7, 3.25f);
	NewsState held;
	Check(walkers::NewsImage(news, image, sizeof(image)) == walkers::kNewsImageBytes &&
	          walkers::ReadNewsImage(held, image, walkers::kNewsImageBytes) && held.headingCity == 7 &&
	          held.headingDays == 3.25f && held.lastAttackDays[kBruma] == 33.5f,
	      "a sighting written and read back with the attacks (version 2)");
	NewsState old;
	Check(walkers::ReadNewsImage(old, image, walkers::kNewsImageV1Bytes) && old.lastAttackDays[kBruma] == 33.5f &&
	          old.headingCity == -1,
	      "a version 1 record (the attacks alone): read, no sighting");
	Check(walkers::NewsImage(news, image, 4) == 0, "too small a buffer: nothing written");
	NewsState cut;
	walkers::NoteCityAttack(cut, kAnvil, 1.0f);
	Check(!walkers::ReadNewsImage(cut, image, walkers::kNewsImageV1Bytes - 1) && cut.lastAttackDays[kAnvil] < 0.0f,
	      "cut short: false, and nothing old kept");
}

}  // namespace

int main() {
	TestChoose();
	TestHeadingHeld();
	TestNote();
	TestIndex();
	TestGameTime();
	TestImage();
	if (g_failures != 0) {
		std::printf("%d failed\n", g_failures);
		return 1;
	}
	std::printf("all passed\n");
	return 0;
}
