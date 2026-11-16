// Every flow of placing actors for a while and taking them out again
// (docs/walkers-spec.md 4.22a step 5): the queue, the new actors found at
// the marker, the two looks of a deletion.

#include "PlaceLogic.h"

#include <cstdio>

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf("  %s  %s\n", condition ? "ok  " : "FAIL", what);
	if (!condition) {
		++g_failures;
	}
}

using walkers::PlaceOwner;
using walkers::PlaceQueue;
using walkers::PlaceRequest;

PlaceRequest Request(PlaceOwner owner, UInt8 tag, UInt32 count) {
	PlaceRequest r;
	r.owner = owner;
	r.tag = tag;
	r.list = 0x01000A93;
	r.count = count;
	r.anchor = 0x14;
	return r;
}

void TestQueue() {
	std::printf("The queue\n");
	PlaceQueue q;
	Check(walkers::PushPlace(q, Request(PlaceOwner::Siege, 3, 5)) && q.count == 1, "queued");
	PlaceRequest bad = Request(PlaceOwner::Siege, 3, 5);
	bad.list = 0;
	Check(!walkers::PushPlace(q, bad), "no list: refused");
	bad = Request(PlaceOwner::Siege, 3, 0);
	Check(!walkers::PushPlace(q, bad), "nobody to place: refused");
	bad = Request(PlaceOwner::Siege, 3, 5);
	bad.anchor = 0;
	Check(!walkers::PushPlace(q, bad), "no anchor: refused");
	walkers::PushPlace(q, Request(PlaceOwner::Camp, 1, 99));
	Check(q.items[1].count == walkers::kPlaceCountMost, "99 held to 24");
	walkers::PushPlace(q, Request(PlaceOwner::Siege, 3, 2));
	Check(walkers::PlacesPending(q, nullptr, PlaceOwner::Siege, 3) == 7, "the siege's two requests: 7 on the way");
	PlaceRequest under = Request(PlaceOwner::Siege, 3, 4);
	Check(walkers::PlacesPending(q, &under, PlaceOwner::Siege, 3) == 11, "with the one under way: 11");
	Check(walkers::PlacesPending(q, &under, PlaceOwner::Siege, 4) == 0, "another city's: none");
	PlaceRequest out;
	Check(walkers::PopPlace(q, out) && out.owner == PlaceOwner::Siege && out.count == 5 && q.count == 2,
	      "the first off, the rest moved up");
	Check(walkers::DropPlaces(q, PlaceOwner::Siege, 3) == 1 && q.count == 1 && q.items[0].owner == PlaceOwner::Camp,
	      "the siege's dropped, the camp's kept");
	walkers::PopPlace(q, out);
	Check(!walkers::PopPlace(q, out), "empty: nothing");
	for (UInt32 i = 0; i < walkers::kPlaceQueueMost; ++i) {
		walkers::PushPlace(q, Request(PlaceOwner::Test, 0, 1));
	}
	Check(!walkers::PushPlace(q, Request(PlaceOwner::Test, 0, 1)), "full: refused");
}

void TestNewlyPlaced() {
	std::printf("The new ones at the marker\n");
	const UInt32 before[2] = {0xFF000001, 0xFF000002};
	const UInt32 now[5] = {0xFF000001, 0xFF000003, 0xFF000002, 0xFF000004, 0xFF000003};
	UInt32 out[8];
	Check(walkers::NewlyPlaced(before, 2, now, 5, 8, out) == 2 && out[0] == 0xFF000003 && out[1] == 0xFF000004,
	      "those not there before, each once");
	Check(walkers::NewlyPlaced(before, 2, now, 5, 1, out) == 1, "never more than asked");
	Check(walkers::NewlyPlaced(before, 2, before, 2, 8, out) == 0, "nothing new: none");
}

void TestRetire() {
	std::printf("Taken out in two looks\n");
	walkers::RetireList r;
	Check(walkers::NoteRetire(r, 0xFF000010) && r.count == 1, "a placed one noted");
	Check(!walkers::NoteRetire(r, 0xFF000010), "noted twice: once");
	Check(!walkers::NoteRetire(r, 0x01000953), "the ESP's or the game's: never");
	UInt32 dis[walkers::kRetireMost];
	UInt32 rem[walkers::kRetireMost];
	UInt32 nd = 0;
	UInt32 nr = 0;
	walkers::StepRetire(r, dis, nd, rem, nr);
	Check(nd == 1 && dis[0] == 0xFF000010 && nr == 0 && r.count == 1, "the first look: disabled");
	walkers::NoteRetire(r, 0xFF000011);
	walkers::StepRetire(r, dis, nd, rem, nr);
	Check(nd == 1 && dis[0] == 0xFF000011 && nr == 1 && rem[0] == 0xFF000010 && r.count == 1,
	      "the next: the first deleted, the new one disabled");
	walkers::StepRetire(r, dis, nd, rem, nr);
	Check(nd == 0 && nr == 1 && r.count == 0, "then the last deleted, the list empty");
	walkers::RetireList full;
	for (UInt32 i = 0; i < walkers::kRetireMost; ++i) {
		walkers::NoteRetire(full, 0xFF001000 + i);
	}
	Check(!walkers::NoteRetire(full, 0xFF00FFFF), "full: refused");
}

}  // namespace

int main() {
	TestQueue();
	TestNewlyPlaced();
	TestRetire();
	std::printf(g_failures == 0 ? "PASS\n" : "FAIL (%d)\n", g_failures);
	return g_failures == 0 ? 0 : 1;
}
