// Every flow of the survivors' camps (docs/walkers-spec.md 4.20.6; Nadi,
// 2026-11-05: "go 4.20.6", on by default): the roster and its share of
// women, the days' rolls (an attack, a return), an attack begun at its hour,
// the toll away (kills, losses, held, overrun), the fight near (a walker
// ended, a refugee dead, defended), the camps shown, the spots, off, the
// co-save image.

#include "CampLogic.h"
#include "CityGates.h"

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

using walkers::Camp;
using walkers::CampEvent;
using walkers::CampSettings;
using walkers::CampsState;
using walkers::CampStatus;

// Rolls handed out in turn, the last again once they are used up.
struct Rolls {
	const UInt32* values;
	UInt32 count;
	UInt32 at = 0;
	UInt32 operator()() { return values[at < count ? at++ : count - 1]; }
};

Camp Set(const CampSettings& s) {
	Camp c;
	c.place = 0;
	walkers::CampTakeRoster(c, s);
	return c;
}

void TestRoster() {
	std::printf("The roster\n");
	CampSettings s;
	const UInt16 r = walkers::CampRoster(s);
	Check(r == (0x7 | (0x7 << 6) | (1 << 12)), "6 people, 50 %: three men, three women and the trader");
	s.womenPercent = 0;
	Check(walkers::CampRoster(s) == (0x3F | (1 << 12)), "0 % women: six men");
	s.womenPercent = 100;
	s.trader = false;
	Check(walkers::CampRoster(s) == (0x3F << 6), "100 %, no trader: six women");
	s.peoplePerCamp = 1;
	s.womenPercent = 50;
	Check(walkers::CampRoster(s) == (0x1 | (0x1 << 6)), "PeoplePerCamp 1 held to 2: a man and a woman");
	s.peoplePerCamp = 9;
	Check(walkers::CampBits(walkers::CampRoster(s)) == 6, "PeoplePerCamp 9 held to 6");
	s.peoplePerCamp = 5;
	Check(walkers::CampWomenOf(5, 50) == 3 && walkers::CampBits(walkers::CampRoster(s)) == 5,
	      "5 at 50 %: 3 women (half up), 2 men");
	Camp c;
	CampSettings d;
	Check(walkers::CampTakeRoster(c, d) && c.set && walkers::CampAlive(c) == 7, "a first look sets the camp up: 7 alive");
	Check(!walkers::CampTakeRoster(c, d), "the same settings: no change");
	d.peoplePerCamp = 4;
	Check(walkers::CampTakeRoster(c, d) && walkers::CampAlive(c) == 5, "LINK's 4: the camp takes it, 5 with the trader");
	c.lost = 1;
	d.peoplePerCamp = 6;
	Check(!walkers::CampTakeRoster(c, d) && walkers::CampAlive(c) == 4, "one lost: the roster stays until it comes back");
}

void TestDays() {
	std::printf("The days' rolls\n");
	CampSettings s;
	CampsState st;
	for (Camp& c : st.camps) {
		c = Set(s);
	}
	CampEvent ev[16];
	const UInt32 none[] = {99};
	Rolls quiet{none, 1};
	Check(walkers::StepCampDays(st, s, 3.5f, quiet, ev, 16) == 0 && st.lastDay == 3, "the first look notes the day");
	Check(walkers::StepCampDays(st, s, 3.9f, quiet, ev, 16) == 0, "the same day: nothing rolled");
	// Camp 0 attacked at hour 12 of day 4; the others miss (99).
	const UInt32 hit[] = {14, 12, 99};
	Rolls first{hit, 3};
	walkers::StepCampDays(st, s, 4.1f, first, ev, 16);
	Check(std::fabs(st.camps[0].startAt - 4.5f) < 0.001f && st.camps[1].startAt < 0.0f,
	      "a roll under 15: an attack at that day's hour 12; 99: none");
	const UInt32 early[] = {0, 0, 99};
	Rolls before{early, 3};
	walkers::StepCampDays(st, s, 5.3f, before, ev, 16);
	Check(std::fabs(st.camps[1].startAt - 5.3f) < 0.001f && std::fabs(st.camps[0].startAt - 4.5f) < 0.001f,
	      "hour 0 already past: the attack now; one coming already is not rolled again");
	st.camps[2].status = CampStatus::Overrun;
	st.camps[2].lost = st.camps[2].roster;
	st.camps[2].walkersLeft = 5;
	const UInt32 back[] = {9, 99};  // camps 0 and 1 wait for their attack: not rolled
	Rolls ret{back, 2};
	const UInt32 n = walkers::StepCampDays(st, s, 6.0f, ret, ev, 16);
	Check(n == 1 && ev[0].kind == CampEvent::Return && ev[0].camp == 2 && st.camps[2].status == CampStatus::Living &&
	          st.camps[2].lost == 0 && st.camps[2].walkersLeft == 0 && walkers::CampAlive(st.camps[2]) == 7 &&
	          st.camps[2].place == 0,
	      "an overrun camp, a roll under 10: back, its people set up anew, its walkers gone, its place kept");
	CampsState unplaced;
	unplaced.camps[0] = Set(s);
	unplaced.camps[1] = Set(s);
	unplaced.camps[1].place = walkers::kCampNoPlace;
	walkers::StepCampDays(unplaced, s, 1.0f, quiet, ev, 16);
	const UInt32 always[] = {0};
	Rolls all{always, 1};
	walkers::StepCampDays(unplaced, s, 2.0f, all, ev, 16);
	Check(unplaced.camps[0].startAt >= 0.0f && unplaced.camps[1].startAt < 0.0f,
	      "a camp without a place is not rolled");
	CampSettings few = s;
	few.campCount = 1;
	CampsState two;
	two.camps[0] = Set(s);
	two.camps[1] = Set(s);
	walkers::StepCampDays(two, few, 1.0f, quiet, ev, 16);
	walkers::StepCampDays(two, few, 2.0f, all, ev, 16);
	Check(two.camps[0].startAt >= 0.0f && two.camps[1].startAt < 0.0f, "CampCount 1: the second camp not in play");
	CampSettings off = s;
	off.enabled = false;
	CampsState offState = st;
	Check(walkers::StepCampDays(offState, off, 9.0f, ret, ev, 16) == 0 && offState.lastDay == 9,
	      "off: the day noted, nothing rolled");
}

void TestBegin() {
	std::printf("An attack begins\n");
	CampSettings s;
	Camp c = Set(s);
	c.startAt = 4.5f;
	CampEvent ev[4];
	const UInt32 r[] = {3};
	Rolls roll{r, 1};
	Check(walkers::StepCamp(c, 0, s, 4.4f, false, roll, ev, 4) == 0 && c.status == CampStatus::Living,
	      "before its hour: nothing");
	Check(walkers::StepCamp(c, 0, s, 4.5f, true, roll, ev, 4) == 1 && ev[0].kind == CampEvent::Begin &&
	          c.status == CampStatus::Attacked && c.walkersLeft == 9 && c.seen && c.startAt < 0.0f,
	      "its hour: attacked, 6 + 3 walkers, the player near seen");
	Camp empty = Set(s);
	empty.lost = empty.roster;
	Check(walkers::BeginCampAttack(empty, 0, s, 1.0f, false, 0, ev, 4) == 0 && empty.status == CampStatus::Living,
	      "nobody alive: no attack");
	CampSettings odd = s;
	odd.walkersMin = 10;
	odd.walkersMax = 4;
	Camp o = Set(odd);
	walkers::BeginCampAttack(o, 0, odd, 1.0f, false, 6, ev, 4);
	Check(o.walkersLeft == 10, "min and max swapped: 4..10 taken");
}

void TestTollAway() {
	std::printf("The toll away\n");
	CampSettings s;
	Camp c = Set(s);
	CampEvent ev[32];
	walkers::BeginCampAttack(c, 0, s, 1.0f, false, 2, ev, 32);  // 8 walkers
	const UInt32 none[] = {99};
	Rolls safe{none, 1};
	Check(walkers::StepCamp(c, 0, s, 1.0f + 0.5f / 24.0f, false, safe, ev, 32) == 0 && c.walkersLeft == 8,
	      "half an hour: nothing yet");
	walkers::StepCamp(c, 0, s, 1.0f + 2.0f / 24.0f + 0.0001f, false, safe, ev, 32);
	Check(c.walkersLeft == 4 && walkers::CampAlive(c) == 7, "two hours: 2 a hour cut down, nobody lost");
	const UInt32 held[] = {99};
	Rolls h{held, 1};
	const UInt32 n = walkers::StepCamp(c, 0, s, 1.0f + 5.0f / 24.0f, false, h, ev, 32);
	Check(n == 1 && ev[0].kind == CampEvent::HeldAway && c.status == CampStatus::Living && c.walkersLeft == 0,
	      "the last cut down: held away - no reward");
	Camp f = Set(s);
	f.lost = static_cast<UInt16>(f.roster & ~1u);  // one man left
	walkers::BeginCampAttack(f, 3, s, 2.0f, false, 6, ev, 32);  // 12
	const UInt32 lose[] = {10, 0};
	Rolls l{lose, 2};
	const UInt32 m = walkers::StepCamp(f, 3, s, 2.0f + 1.0f / 24.0f, false, l, ev, 32);
	Check(m == 2 && ev[0].kind == CampEvent::PersonLost && ev[0].slot == 0 && ev[1].kind == CampEvent::Overrun &&
	          ev[1].camp == 3 && f.status == CampStatus::Overrun && f.walkersLeft == 10 + 7,
	      "an hour, a roll under 25: the last one lost - overrun, the 10 left and the 7 lost at the camp");
	CampSettings half = s;
	half.refugeeKillsPerHour = 0.5f;
	Camp k = Set(half);
	walkers::BeginCampAttack(k, 0, half, 1.0f, false, 0, ev, 32);  // 6
	walkers::StepCamp(k, 0, half, 1.0f + 3.0f / 24.0f + 0.0001f, false, safe, ev, 32);
	Check(k.walkersLeft == 5 && std::fabs(k.carry - 0.5f) < 0.001f, "0.5 an hour: one in three hours, a half carried");
	Camp w = Set(s);
	walkers::BeginCampAttack(w, 0, s, 1.0f, false, 0, ev, 32);
	walkers::StepCamp(w, 0, s, 30.0f, false, safe, ev, 32);
	Check(w.status == CampStatus::Living, "a month waited: rolled through, held");
	Camp n2 = Set(s);
	walkers::BeginCampAttack(n2, 0, s, 1.0f, false, 0, ev, 32);
	walkers::StepCamp(n2, 0, s, 1.5f, true, safe, ev, 32);
	Check(n2.walkersLeft == 6 && n2.seen && std::fabs(n2.tollFrom - 1.5f) < 0.001f,
	      "the player near: no toll, seen, the toll counted from now");
	walkers::StepCamp(n2, 0, s, 1.5f + 1.0f / 24.0f + 0.0001f, false, safe, ev, 32);
	Check(n2.walkersLeft == 4, "left again: the toll from when he went");
}

void TestNear() {
	std::printf("The fight near\n");
	CampSettings s;
	Camp c = Set(s);
	CampEvent ev[4];
	walkers::BeginCampAttack(c, 5, s, 1.0f, true, 0, ev, 4);  // 6, seen
	for (int i = 0; i < 5; ++i) {
		walkers::CampWalkerEnded(c, 5, ev, 4);
	}
	Check(c.walkersLeft == 1 && c.status == CampStatus::Attacked, "five ended: one left");
	Check(walkers::CampPersonDied(c, 5, 0, ev, 4) == 0 && walkers::CampAlive(c) == 6, "a man dead: six left");
	Check(walkers::CampPersonDied(c, 5, 0, ev, 4) == 0 && walkers::CampAlive(c) == 6, "dead again: counted once");
	Check(walkers::CampPersonDied(c, 5, 5, ev, 4) == 0 && walkers::CampAlive(c) == 6, "a slot not in the roster: nothing");
	const UInt32 n = walkers::CampWalkerEnded(c, 5, ev, 4);
	Check(n == 1 && ev[0].kind == CampEvent::Defended && ev[0].camp == 5 && c.status == CampStatus::Living,
	      "the last walker ended, the player near: defended");
	Check(walkers::CampWalkerEnded(c, 5, ev, 4) == 0, "a walker more: nothing");
	Camp d = Set(s);
	walkers::BeginCampAttack(d, 1, s, 1.0f, true, 0, ev, 4);
	walkers::CampWalkerEnded(d, 1, ev, 4);
	UInt32 last = 0;
	for (UInt32 slot = 0; slot < walkers::kCampSlots; ++slot) {
		last += walkers::CampPersonDied(d, 1, slot, ev, 4);
	}
	Check(last == 1 && ev[0].kind == CampEvent::Overrun && d.status == CampStatus::Overrun && d.walkersLeft == 5,
	      "all seven dead: overrun, the five walkers left stay");
	walkers::CampWalkerEnded(d, 1, ev, 4);
	Check(d.walkersLeft == 4 && d.status == CampStatus::Overrun, "one of the overrun camp's walkers ended: four");
	Check(walkers::CampPersonDied(d, 1, 0, ev, 4) == 0, "overrun: a death counts no more");
	Camp quiet = Set(s);
	UInt32 q = 0;
	for (UInt32 slot = 0; slot < walkers::kCampSlots; ++slot) {
		q += walkers::CampPersonDied(quiet, 2, slot, ev, 4);
	}
	Check(q == 1 && quiet.status == CampStatus::Overrun && quiet.walkersLeft == 0,
	      "no attack, all killed (a horde, the player): overrun, no walkers at the camp");
}

void TestShown() {
	std::printf("The camps shown\n");
	CampSettings s;
	CampsState st;
	float distance[walkers::kCampMost];
	for (float& d : distance) {
		d = -1.0f;
	}
	UInt32 shown[walkers::kCampShownMost];
	st.camps[3].status = CampStatus::Attacked;
	st.camps[4].status = CampStatus::Attacked;
	st.camps[5].status = CampStatus::Overrun;
	st.camps[5].walkersLeft = 3;
	st.camps[6].status = CampStatus::Overrun;
	distance[3] = 5000.0f;
	distance[4] = 2000.0f;
	distance[5] = 1000.0f;
	distance[6] = 100.0f;
	UInt32 n = walkers::ChooseShownCamps(st, s, distance, shown);
	Check(n == 2 && shown[0] == 4 && shown[1] == 5,
	      "one attacked at once: the nearest attacked, then the overrun one with walkers; none without");
	s.campsAttackedAtOnce = 2;
	n = walkers::ChooseShownCamps(st, s, distance, shown);
	Check(n == 2 && shown[0] == 4 && shown[1] == 3, "two at once: both attacked first");
	s.campsAttackedAtOnce = 9;
	distance[4] = 7000.0f;
	n = walkers::ChooseShownCamps(st, s, distance, shown);
	Check(n == 2 && shown[0] == 3 && shown[1] == 5, "9 held to 2; past kCampSeenUnits not shown");
	s.enabled = false;
	Check(walkers::ChooseShownCamps(st, s, distance, shown) == 0, "off: none");
	Camp c;
	c.walkersLeft = 30;
	Check(walkers::CampWalkersOut(c) == 12, "30 left: 12 out at once");
}

void TestSpots() {
	std::printf("The spots\n");
	const float place = 0.0f;  // the road to the north
	float dx = 0.0f;
	float dy = 0.0f;
	walkers::CampWalkerSpot(place, true, 0, dx, dy);
	Check(std::fabs(dx) < 1.0f && std::fabs(dy + walkers::kCampAttackUnits) < 1.0f,
	      "the first attacker: kCampAttackUnits out, away from the road");
	float dx1 = 0.0f;
	float dy1 = 0.0f;
	float dx2 = 0.0f;
	float dy2 = 0.0f;
	walkers::CampWalkerSpot(place, true, 1, dx1, dy1);
	walkers::CampWalkerSpot(place, true, 2, dx2, dy2);
	Check(std::fabs(dx1 + dx2) < 1.0f && std::fabs(dy1 - dy2) < 1.0f && std::fabs(dx1) > 100.0f,
	      "the next two to either side alike");
	walkers::CampWalkerSpot(place, false, 3, dx, dy);
	Check(std::fabs(std::sqrt(dx * dx + dy * dy) - walkers::kCampNestUnits) < 1.0f, "an overrun camp's among the tents");
	walkers::CampTentSpot(0.0f, -1.0f, dx, dy);
	Check(std::fabs(dx - 620.0f * std::sin(3.14159265f * 0.75f)) < 1.0f && dy < -400.0f,
	      "the first tent: behind the fire from a road to the north, 45 degrees to the side, 620 out");
	walkers::CampTentSpot(0.0f, 1.0f, dx1, dy1);
	Check(std::fabs(dx + dx1) < 1.0f && std::fabs(dy - dy1) < 1.0f, "the second the mirror of the first");
	const float roof[] = {700.0f, 640.0f, 520.0f, 300.0f, 300.0f, 260.0f, 120.0f, 100.0f, 100.0f};
	Check(walkers::CampFallStop(roof, 9) == 300.0f, "a fall: the first stop counts, not where it slid to");
	const float falling[] = {700.0f, 600.0f, 400.0f};
	Check(walkers::CampFallStop(falling, 3) < -1e8f && walkers::CampFallStop(falling, 0) < -1e8f,
	      "still falling, or no heights: no stop");
	const float shallow[] = {100.0f, 90.0f, 90.0f};
	Check(walkers::CampFallStop(shallow, 3) < -1e8f, "a stop before 20 units of fall does not count");
}

void TestImage() {
	std::printf("The co-save image\n");
	CampSettings s;
	CampsState st;
	st.lastDay = 41;
	st.camps[0] = Set(s);
	st.camps[0].lost = 3;
	st.camps[0].status = CampStatus::Attacked;
	st.camps[0].walkersLeft = 7;
	st.camps[0].seen = true;
	st.camps[0].tollFrom = 41.25f;
	st.camps[0].carry = 0.5f;
	st.camps[0].place = 517;
	st.camps[63].status = CampStatus::Overrun;
	st.camps[63].startAt = 3.0f;
	UInt8 image[walkers::kCampImageBytes];
	Check(walkers::CampImage(st, image, sizeof(image) - 1) == 0, "too small a buffer: nothing written");
	Check(walkers::CampImage(st, image, sizeof(image)) == walkers::kCampImageBytes, "written whole");
	CampsState back;
	Check(walkers::ReadCampImage(back, image, sizeof(image)), "read back");
	const Camp& c = back.camps[0];
	Check(back.lastDay == 41 && c.status == CampStatus::Attacked && c.set && c.seen && c.roster == st.camps[0].roster &&
	          c.lost == 3 && c.walkersLeft == 7 && c.tollFrom == 41.25f && c.carry == 0.5f && c.place == 517,
	      "every field of an attacked camp, its place too");
	Check(back.camps[63].status == CampStatus::Overrun && back.camps[63].startAt == 3.0f &&
	          back.camps[63].place == walkers::kCampNoPlace,
	      "the last of the 64 too; no place stays none");
	Check(walkers::kCampImageBytes == 4 + 64 * 28 + 20 && walkers::kCampRecordVersion == 3,
	      "version 3: 64 camps of 28 bytes, then the escort's 20");
	Check(walkers::kCampImageV2Bytes == 4 + 64 * 28, "version 2's image: the camps alone");
	image[4] = 9;
	image[8] = 0xFF;
	image[9] = 0xFF;
	image[10] = 0xFF;
	image[11] = 0xFF;
	walkers::ReadCampImage(back, image, sizeof(image));
	Check(back.camps[0].status == CampStatus::Living && back.camps[0].roster == 0x1FFF &&
	          back.camps[0].lost == 0x1FFF,
	      "a status out of range read as living; masks held to the 13 slots");
	Check(!walkers::ReadCampImage(back, image, 10) && back.lastDay == -1, "cut short: nothing read");
}

walkers::CampCandidate At(float x, float y) {
	walkers::CampCandidate c{};
	c.x = x;
	c.y = y;
	return c;
}

void TestPlaces() {
	std::printf("The places rolled\n");
	CampSettings s;
	s.campCount = 3;
	s.spacingUnits = 1000.0f;
	// Five on a line 600 apart: 0, 600, 1200, 1800, 2400.
	const walkers::CampCandidate line[] = {At(0, 0), At(600, 0), At(1200, 0), At(1800, 0), At(2400, 0)};
	CampsState st;
	const UInt32 zero[] = {0};
	Rolls first{zero, 1};
	Check(walkers::RollCampPlaces(st, s, line, 5, first) == 3 && st.camps[0].place == 0 && st.camps[1].place == 2 &&
	          st.camps[2].place == 4,
	      "rolls of 0: in order, each 1000 from those taken - 0, 1200, 2400");
	Check(walkers::CampInPlay(st, s, 2) && !walkers::CampInPlay(st, s, 3), "three in play, the fourth not");
	Check(walkers::RollCampPlaces(st, s, line, 5, first) == 0, "all placed: nothing rolled again");
	s.campCount = 4;
	Check(walkers::RollCampPlaces(st, s, line, 5, first) == 0 && st.camps[3].place == walkers::kCampNoPlace &&
	          !walkers::CampInPlay(st, s, 3),
	      "a fourth wanted, none far enough: left without, not in play; the three kept");
	s.spacingUnits = 500.0f;
	Check(walkers::RollCampPlaces(st, s, line, 5, first) == 1 && st.camps[3].place == 1,
	      "500 apart: the fourth takes 600, the earlier kept");
	CampsState other;
	other.camps[0].place = 4;
	s.campCount = 2;
	s.spacingUnits = 1000.0f;
	const UInt32 picks[] = {4, 0};
	Rolls r{picks, 2};
	Check(walkers::RollCampPlaces(other, s, line, 5, r) == 1 && other.camps[1].place == 1,
	      "an earlier camp at 2400 kept: drawn first it is taken, the next draw (600) far enough");
	CampSettings three;
	three.campCount = 3;
	three.spacingUnits = 1000.0f;
	CampsState retry;
	const UInt32 shortFirst[] = {1, 2, 0};  // 600 and 1800 first: two only
	Rolls sf{shortFirst, 3};
	Check(walkers::RollCampPlaces(retry, three, line, 5, sf) == 3 && retry.camps[0].place == 0 &&
	          retry.camps[2].place == 4,
	      "a roll short of three (600, 1800) tried again: the next order places all three");
	CampsState once;
	Rolls sf2{shortFirst, 3};
	Check(walkers::RollCampPlacesOnce(once, three, line, 5, sf2) == 2 && once.camps[0].place == 1 &&
	          once.camps[1].place == 3,
	      "the one roll alone: 600 and 1800, two");
	CampsState none;
	Check(walkers::RollCampPlaces(none, s, line, 0, first) == 0, "no candidates: none placed");
	CampSettings big;
	big.campCount = 200;
	Check(walkers::CampActive(big) == 64, "CampCount 200 held to 64");
	big.campCount = 0;
	Check(walkers::CampActive(big) == 1, "0 held to 1");
}

void TestKits() {
	std::printf("The kits\n");
	int kits[walkers::kCampKits] = {-1, -1, -1, -1};
	bool moved[walkers::kCampKits];
	float distance[walkers::kCampMost];
	for (float& d : distance) {
		d = -1.0f;
	}
	distance[5] = 9000.0f;
	distance[2] = 3000.0f;
	distance[9] = 20000.0f;
	walkers::ChooseCampKits(kits, distance, 64, moved);
	Check(kits[0] == 2 && kits[1] == 5 && kits[2] == -1 && moved[0] && moved[1] && !moved[2],
	      "the nearest first take the kits; 20000 off none");
	Check(walkers::KitOf(kits, 5) == 1 && walkers::KitOf(kits, 9) == -1, "the kit of a camp");
	walkers::ChooseCampKits(kits, distance, 64, moved);
	Check(kits[0] == 2 && kits[1] == 5 && !moved[0] && !moved[1], "the same again: kept, nothing moved");
	distance[5] = 16000.0f;
	walkers::ChooseCampKits(kits, distance, 64, moved);
	Check(kits[1] == 5 && !moved[1], "16000: past the setting up, within the keeping - kept");
	distance[5] = 19000.0f;
	walkers::ChooseCampKits(kits, distance, 64, moved);
	Check(kits[1] == -1, "19000: freed");
	for (UInt32 i = 10; i < 16; ++i) {
		distance[i] = 1000.0f + static_cast<float>(i);
	}
	walkers::ChooseCampKits(kits, distance, 64, moved);
	Check(kits[0] == 2 && kits[1] == 10 && kits[2] == 11 && kits[3] == 12 && moved[1] && moved[3] && !moved[0],
	      "six near, three kits free: the three nearest; the kept one stays");
	walkers::ChooseCampKits(kits, distance, 11, moved);
	Check(kits[2] == -1 && kits[3] == -1 && kits[1] == 10, "CampCount 11: the kits at 11 and 12 freed");
	distance[10] = -1.0f;
	walkers::ChooseCampKits(kits, distance, 64, moved);
	Check(kits[1] != 10, "a camp not in Tamriel's distance: its kit freed");
}


void TestThings() {
	std::printf("The things reward: a pick off a leveled list\n");
	const UInt16 levels[] = {1, 1, 3, 6, 9, 12, 16, 20};
	Check(walkers::PickLeveled(levels, 8, 0, 0) == -1, "level 0, the lowest entry at 1: nothing");
	Check(walkers::PickLeveled(levels, 0, 5, 0) == -1, "an empty list: nothing");
	Check(walkers::PickLeveled(levels, 8, 1, 0) == 0 && walkers::PickLeveled(levels, 8, 1, 1) == 1,
	      "level 1: the two at 1, the roll picks");
	Check(walkers::PickLeveled(levels, 8, 2, 2) == 0, "level 2: still only the two at 1 (the roll wraps)");
	Check(walkers::PickLeveled(levels, 8, 10, 0) == 0 && walkers::PickLeveled(levels, 8, 10, 4) == 4,
	      "level 10: the top 9, down to 1 (9 - 8): five, from the first");
	Check(walkers::PickLeveled(levels, 8, 30, 0) == 5 && walkers::PickLeveled(levels, 8, 30, 2) == 7,
	      "level 30: the top 20, down to 12: three");
	Check(walkers::CampThings(true, 6, 6, false) == 1 && walkers::CampThings(true, 1, 6, false) == 1,
	      "a defended camp: one thing however many live");
	Check(walkers::CampThings(true, 6, 6, true) == 3 && walkers::CampThings(true, 5, 6, true) == 3 &&
	          walkers::CampThings(true, 1, 6, true) == 1,
	      "an escort: one thing for each two who made it, rounded up");
	Check(walkers::CampThings(false, 6, 6, true) == 0 && walkers::CampThings(true, 0, 6, true) == 0,
	      "RewardThings off, or nobody made it: none");
}

void TestScaled() {
	std::printf("The escort's reward scales with those who made it\n");
	Check(walkers::ScaledReward(300, 6, 6) == 300 && walkers::ScaledReward(300, 3, 6) == 150,
	      "all six: the whole 300; three of six: half");
	Check(walkers::ScaledReward(2, 1, 6) == 0 && walkers::ScaledReward(2, 2, 6) == 1 && walkers::ScaledReward(2, 5, 6) == 2,
	      "fame 2: a sixth rounds to 0, a third to 1 (half up), five sixths to 2");
	Check(walkers::ScaledReward(300, 0, 6) == 0 && walkers::ScaledReward(300, 4, 0) == 0, "none alive, none set out: 0");
}

void TestEscort() {
	std::printf("The escort\n");
	CampSettings s;
	CampsState st;
	st.camps[0] = Set(s);
	st.camps[1] = Set(s);
	CampEvent ev[8];
	Check(walkers::CampOffersEscort(st, s, 0), "a living camp, nobody escorted: it asks");
	CampSettings no = s;
	no.escort = false;
	Check(!walkers::CampOffersEscort(st, no, 0), "Escort off: it does not");
	st.camps[1].status = CampStatus::Attacked;
	Check(!walkers::CampOffersEscort(st, s, 1), "an attacked camp does not ask");
	st.camps[1].status = CampStatus::Living;
	st.camps[0].startAt = 5.5f;
	Check(walkers::BeginEscort(st, s, 0, 3, 5.0f, ev, 8) == 1 && ev[0].kind == CampEvent::EscortBegin &&
	          st.camps[0].status == CampStatus::Escort && st.escort.camp == 0 && st.escort.gate == 3 &&
	          st.escort.started == 7 && st.camps[0].startAt < 0.0f,
	      "begun: the camp on the way, the gate, seven set out, its coming attack off");
	Check(!walkers::CampOffersEscort(st, s, 1) && walkers::BeginEscort(st, s, 1, 2, 5.0f, ev, 8) == 0,
	      "one escort at a time: the other camp neither asks nor begins");
	Check(walkers::StepEscort(st, s, 5.1f, -1.0f, -1.0f, ev, 8) == 0, "nothing known (indoors): nothing");
	Check(walkers::StepEscort(st, s, 5.1f, 4000.0f, 20000.0f, ev, 8) == 0, "4000 behind, far from the gate: on");
	CampsState behind = st;
	Check(walkers::StepEscort(behind, s, 5.2f, 5001.0f, 20000.0f, ev, 8) == 1 && ev[0].kind == CampEvent::EscortBehind &&
	          behind.camps[0].status == CampStatus::Living && behind.escort.camp == -1,
	      "past EscortBehindUnits: failed, they go back to their camp, living");
	CampsState dead = st;
	dead.camps[0].lost = dead.camps[0].roster;
	Check(walkers::StepEscort(dead, s, 5.2f, -1.0f, -1.0f, ev, 8) == 1 && ev[0].kind == CampEvent::EscortDead &&
	          dead.camps[0].status == CampStatus::Left && dead.escort.camp == -1,
	      "all dead on the way: failed, the camp empty");
	Check(walkers::CampPersonDied(st.camps[0], 0, 0, ev, 8) == 0 && walkers::CampAlive(st.camps[0]) == 6 &&
	          st.camps[0].status == CampStatus::Escort,
	      "one dies on the way: lost, the escort goes on");
	Check(walkers::StepEscort(st, s, 5.3f, 200.0f, walkers::kEscortArriveUnits, ev, 8) == 1 &&
	          ev[0].kind == CampEvent::EscortArrived && st.camps[0].status == CampStatus::Gone &&
	          st.escort.arrivedAt == 5.3f && st.escort.camp == 0,
	      "within reach of the gate: arrived, in the city");
	Check(walkers::StepEscort(st, s, 6.0f, 9000.0f, 9000.0f, ev, 8) == 0, "in the city: the distances no longer count");
	Check(walkers::StepEscort(st, s, 12.29f, -1.0f, -1.0f, ev, 8) == 0, "not yet 7 days: they stay");
	Check(walkers::StepEscort(st, s, 12.31f, -1.0f, -1.0f, ev, 8) == 1 && ev[0].kind == CampEvent::EscortLeft &&
	          st.camps[0].status == CampStatus::Left && st.escort.camp == -1,
	      "7 game days on: they leave the game, the camp empty");
	Check(walkers::CampOffersEscort(st, s, 1), "the escort over: the other camp asks again");
	const UInt32 back[] = {5};
	Rolls ret{back, 1};
	st.lastDay = 12;
	Check(walkers::StepCampDays(st, s, 13.0f, ret, ev, 8) >= 1 && st.camps[0].status == CampStatus::Living &&
	          walkers::CampAlive(st.camps[0]) == 7,
	      "an empty camp comes back by ReturnChancePercent as an overrun one does");
	CampSettings off = s;
	off.enabled = false;
	Check(!walkers::CampOffersEscort(st, off, 1) && walkers::StepEscort(st, off, 14.0f, 9000.0f, 0.0f, ev, 8) == 0,
	      "camps off: no ask, no step");
	Check(walkers::CampPeopleOut(st.camps[1], true) == st.camps[1].roster, "a living camp's people are out");
	Camp walking = st.camps[1];
	walking.status = CampStatus::Escort;
	walking.lost = 1;
	Check(walkers::CampPeopleOut(walking, true) == (walking.roster & ~1u), "on the way: the living out");
	walking.status = CampStatus::Gone;
	Check(walkers::CampPeopleOut(walking, true) == (walking.roster & ~1u), "in the city: still out");
	walking.status = CampStatus::Left;
	Check(walkers::CampPeopleOut(walking, true) == 0, "left: none");
	walking.status = CampStatus::Living;
	Check(walkers::CampPeopleOut(walking, false) == 0, "camps off: none");
}

void TestEscortImage() {
	std::printf("The escort in the co-save\n");
	CampSettings s;
	CampsState st;
	st.camps[2] = Set(s);
	st.camps[2].status = CampStatus::Gone;
	st.escort.camp = 2;
	st.escort.gate = 11;
	st.escort.started = 5;
	st.escort.arrivedAt = 8.25f;
	st.escort.kit = 3;
	UInt8 image[walkers::kCampImageBytes];
	walkers::CampImage(st, image, sizeof(image));
	CampsState back;
	Check(walkers::ReadCampImage(back, image, sizeof(image)) && back.camps[2].status == CampStatus::Gone &&
	          back.escort.camp == 2 && back.escort.gate == 11 && back.escort.started == 5 &&
	          back.escort.arrivedAt == 8.25f && back.escort.kit == 3,
	      "the escort read back: its camp, gate, those set out, the day it arrived, its kit");
	Check(walkers::ReadCampImage(back, image, walkers::kCampImageV2Bytes) && back.escort.camp == -1 &&
	          back.camps[2].status == CampStatus::Gone,
	      "a version 2 image: no escort");
	image[4 + 2 * 28] = static_cast<UInt8>(CampStatus::Left);
	walkers::ReadCampImage(back, image, sizeof(image));
	Check(back.camps[2].status == CampStatus::Left, "the new statuses read");
	st.escort.camp = 70;
	walkers::CampImage(st, image, sizeof(image));
	walkers::ReadCampImage(back, image, sizeof(image));
	Check(back.escort.camp == -1, "an escort's camp out of range: none");
}

void TestEscortKit() {
	std::printf("The escorted kit by the city\n");
	Check(walkers::EscortKitWanted(true, false, true, -1.0f), "into the city, the player in its streets: kept (or put there)");
	Check(walkers::EscortKitWanted(true, false, false, 9000.0f), "into the city, the player outside near the gate: kept");
	Check(!walkers::EscortKitWanted(true, true, false, 19000.0f) && !walkers::EscortKitWanted(true, true, false, -1.0f),
	      "into the city, the player far or elsewhere: freed (they live on in the city, unseen)");
	Check(walkers::EscortKitWanted(false, true, false, 9000.0f) && !walkers::EscortKitWanted(false, false, false, 9000.0f),
	      "by the gate (EscortIntoCity=0): kept while shown and near; once freed, not put back");
	Check(!walkers::EscortKitWanted(false, true, true, -1.0f), "by the gate, the player in the city: no distance to the gate - freed");
	CampSettings s;
	Check(s.escortIntoCity, "EscortIntoCity on by default (Nadi, 2026-11-09)");
}

void TestEscortReset() {
	std::printf("An escorted one held by an engine package is reset\n");
	const UInt32 follow = 0x26000A42;
	using walkers::kEscortResetGapSeconds;
	using walkers::kPackageTypeDialogue;
	using walkers::kPackageTypeGreet;
	const float never = walkers::kEscortNeverReset;
	Check(!walkers::EscortResets(follow, follow, false, 1, never), "running the follow: left alone");
	Check(!walkers::EscortResets(0x26000A43, follow, false, 5, never),
	      "its own wander (the ESP's): AddScriptPackage takes it");
	Check(walkers::EscortResets(0xFF001242, follow, false, kPackageTypeDialogue, never),
	      "an engine-made package (FF..., a talk between two - harness 2026-11-09): reset");
	Check(!walkers::EscortResets(0xFF001242, follow, true, kPackageTypeDialogue, never), "fighting: left to the fight");
	Check(!walkers::EscortResets(0, follow, false, 0xFF, never), "no package known: left alone");
	Check(!walkers::EscortResets(0xFF00E21C, follow, false, kPackageTypeGreet, never),
	      "the engine's greeting (type 20 - Nadi's Windows log, 2026-11-13: seventeen resets in a row freed nobody): "
	      "left to end");
	Check(walkers::EscortResets(0xFF001242, follow, false, 19, never) &&
	          walkers::EscortResets(0xFF001242, follow, false, 0xFF, never),
	      "another engine-made package, one of no type read: reset");
	Check(!walkers::EscortResets(0xFF001242, follow, false, kPackageTypeDialogue, 0.0f) &&
	          !walkers::EscortResets(0xFF001242, follow, false, kPackageTypeDialogue, kEscortResetGapSeconds - 1.0f),
	      "reset within the last half minute: not again yet - no blinking in a row");
	Check(walkers::EscortResets(0xFF001242, follow, false, kPackageTypeDialogue, kEscortResetGapSeconds),
	      "half a minute after the last reset: again");
	Check(kPackageTypeDialogue == 18 && kPackageTypeGreet == 20 && kEscortResetGapSeconds == 30.0f,
	      "the types as the EXE has them (GetCurrentAIPackage's switch): 18 a dialogue, 20 a greeting");
}

void TestNearestGate() {
	std::printf("The nearest city gate\n");
	const walkers::CityGate gates[] = {
		{walkers::City::Anvil, 1, 0, 0, 0, 100.0f, 0.0f, 0.0f, 0, 0.0f, 0.0f, 0.0f, "a"},
		{walkers::City::Bruma, 2, 0, 0, 0, 5000.0f, 5000.0f, 0.0f, 0, 0.0f, 0.0f, 0.0f, "b"},
	};
	Check(walkers::NearestGate(gates, 2, 4000.0f, 4500.0f) == 1 && walkers::NearestGate(gates, 2, 0.0f, 0.0f) == 0,
	      "the gate whose outside spot is nearest");
	Check(walkers::NearestGate(gates, 0, 0.0f, 0.0f) == -1, "no gates: none");
}

}  // namespace

int main() {
	TestRoster();
	TestDays();
	TestBegin();
	TestTollAway();
	TestNear();
	TestShown();
	TestSpots();
	TestImage();
	TestPlaces();
	TestKits();
	TestThings();
	TestScaled();
	TestEscort();
	TestEscortImage();
	TestNearestGate();
	TestEscortReset();
	TestEscortKit();
	std::printf(g_failures == 0 ? "PASS\n" : "FAIL (%d)\n", g_failures);
	return g_failures == 0 ? 0 : 1;
}
