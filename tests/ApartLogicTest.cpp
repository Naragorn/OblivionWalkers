// Every flow of keeping the corpse's ragdoll out of the walker's
// (docs/walkers-spec.md 4.1, the swap's look): the engine's collision rule
// as mirrored, the corpse's filter word moved into the walker's group, and
// the check that the two then never meet while the world still holds the
// corpse.

#include "ApartLogic.h"

#include <cstdio>

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf("  %s  %s\n", condition ? "ok  " : "FAIL", what);
	if (!condition) {
		++g_failures;
	}
}

using walkers::kFilterTableWords;
using walkers::RuleCollides;

// The tables as the engine's 0x008A83C0 fills them (read with REA
// 2026-10-25): the layer matrix all ones, then rows cut back; the bone
// table (by part number) zero, then rows set.
struct Tables {
	UInt32 words[kFilterTableWords];
};

Tables EngineTables() {
	Tables t{};
	for (UInt32 i = 0; i < 32; ++i) {
		t.words[i] = 0xFFFFFFFFu;
	}
	const UInt32 rows[][2] = {
		{1, 0xbfdb6fff},  {2, 0xfffb6fff},  {3, 0xbbfb6f3f},  {4, 0x77db7fff},  {5, 0x77db7fff},
		{6, 0x37da7f37},  {7, 0xb0da7f37},  {8, 0xf7cb7fff},  {9, 0xbfdb6fff},  {10, 0xf7db6fff},
		{11, 0x39db6fff}, {12, 0x70f051f1}, {13, 0xbfdb6fff}, {14, 0xf7fb7fff}, {15, 0x82c00001},
		{16, 0xf7fb6f3f}, {17, 0xbfdb6fff}, {18, 0x82c00001}, {19, 0xbfdb6fff}, {20, 0xf3fb7eff},
		{21, 0x30d1500d}, {24, 0x36db6f7f}, {25, 0xb7dfe77f}, {26, 0x37cb6777}, {27, 0x38ca2a0f},
		{28, 0xbffb7fff}, {29, 0xbffb7fff}, {30, 0x00d15535}, {31, 0xb2dfe78f},
	};
	for (const auto& row : rows) {
		t.words[row[0]] &= row[1];
	}
	const UInt32 parts[][2] = {
		{1, 0x10230c0},  {2, 0x14030c0},  {3, 0x14030c0},  {4, 0x14230c0},  {5, 0x1403000},  {6, 0x142791e},
		{7, 0x141ff1e},  {8, 0x14df0c0},  {9, 0x14de080},  {10, 0x14ce080}, {11, 0x10000c0}, {12, 0x10241fe},
		{13, 0x109c7fe}, {14, 0x14837c0}, {15, 0x1482780}, {16, 0x1482380}, {17, 0x1001052}, {18, 0x1000700},
		{19, 0x141e700}, {22, 0x109c7fc}, {23, 0x1000000}, {24, 0xcffffe},
	};
	for (const auto& part : parts) {
		t.words[32 + part[0]] |= part[1];
	}
	return t;
}

// A filter word: group, part, layer and the two flags.
UInt32 Filter(UInt32 group, UInt32 part, UInt32 layer, bool linked = false, bool off = false) {
	return (group << 16) | (part << 8) | layer | (linked ? walkers::kFilterLinked : 0u) |
	       (off ? walkers::kFilterNoCollision : 0u);
}

void TestRule() {
	std::printf("The engine's rule (0x008A7F70)\n");
	Tables t = EngineTables();
	Check(!RuleCollides(Filter(5, 0, 1, false, true), Filter(6, 0, 1), t.words),
	      "the first switched off (0x4000): no collision");
	Check(!RuleCollides(Filter(5, 0, 1), Filter(6, 0, 1, false, true), t.words),
	      "the second switched off: no collision");
	Check(RuleCollides(Filter(5, 0, 29, false, true), Filter(6, 0, 1), t.words),
	      "layer 29 first: the switch is not read, the matrix decides");
	Check(RuleCollides(Filter(0, 0, 15), Filter(6, 0, 1), t.words) &&
	          RuleCollides(Filter(6, 0, 1), Filter(0, 0, 15), t.words),
	      "a group of 0 on either side: a collision, the matrix not read (15 meets no static by it)");
	Check(RuleCollides(Filter(5, 2, 8), Filter(6, 2, 8), t.words),
	      "two bipeds of two groups: a collision, whatever the parts");
	Check(!RuleCollides(Filter(5, 2, 8), Filter(5, 3, 8), t.words) &&
	          RuleCollides(Filter(5, 2, 8), Filter(5, 6, 8), t.words),
	      "two bipeds of one group: the bone table decides by the parts (2 and 3 not, 2 and 6 do)");
	Check(RuleCollides(Filter(5, 0, 8), Filter(6, 0, 1), t.words) &&
	          !RuleCollides(Filter(5, 0, 8), Filter(6, 0, 20), t.words),
	      "two groups, not both bipeds: the matrix row of the first decides (biped meets static, not the "
	      "character controller)");
	Check(RuleCollides(Filter(5, 2, 4, true), Filter(5, 6, 4, true), t.words) &&
	          !RuleCollides(Filter(5, 2, 4, true), Filter(5, 3, 4, true), t.words),
	      "one group, both linked: the matrix allows, and parts next to each other do not meet");
	Check(!RuleCollides(Filter(5, 2, 18, true), Filter(5, 6, 4, true), t.words),
	      "one group, both linked, the matrix says no: no collision");
	Check(!RuleCollides(Filter(5, 2, 4, true), Filter(5, 6, 4), t.words) &&
	          !RuleCollides(Filter(5, 2, 8), Filter(5, 6, 4), t.words) &&
	          !RuleCollides(Filter(5, 2, 4), Filter(5, 6, 8), t.words),
	      "one group, not both linked, not both bipeds: no collision, either way round");
}

// The filter words read in the run of 2026-10-25 15:27: every ragdoll body
// a biped (layer 8) with its own part, neither linked, the walker in group
// 0xCF, the corpse in 0xCD.
const UInt32 kWalkerFilters[] = {0x00CF0208, 0x00CF0308, 0x00CF0408, 0x00CF0108, 0x00CF0508, 0x00CF0608,
                                 0x00CF0708, 0x00CF0B08, 0x00CF0C08, 0x00CF0D08, 0x00CF0808, 0x00CF0908,
                                 0x00CF0A08, 0x00CF0E08, 0x00CF0F08, 0x00CF1008};
const UInt32 kCorpseFilters[] = {0x00CD0208, 0x00CD0308, 0x00CD0408, 0x00CD0108, 0x00CD0508, 0x00CD0608,
                                 0x00CD0708, 0x00CD0B08, 0x00CD0C08, 0x00CD0D08, 0x00CD0808, 0x00CD0908,
                                 0x00CD0A08, 0x00CD0E08, 0x00CD0F08, 0x00CD1008};
constexpr UInt32 kRagdollBodies = sizeof(kWalkerFilters) / sizeof(kWalkerFilters[0]);

void TestMeasured() {
	std::printf("The two ragdolls as measured\n");
	Tables t = EngineTables();
	UInt32 meeting = 0;
	for (UInt32 w = 0; w < kRagdollBodies; ++w) {
		for (UInt32 c = 0; c < kRagdollBodies; ++c) {
			meeting += RuleCollides(kWalkerFilters[w], kCorpseFilters[c], t.words) ? 1u : 0u;
		}
	}
	Check(meeting == kRagdollBodies * kRagdollBodies,
	      "as they are, every walker body meets every corpse body (two groups of bipeds) - the push of 13:48");
}

void TestApartFilter() {
	std::printf("The corpse's filter in the walker's group\n");
	Check(walkers::ApartFilter(0x00CD0208, 0x00CF0308) == 0x00CF0008,
	      "the walker's group, no part, the layer kept: 00CD0208 with 00CF0308 becomes 00CF0008");
	Check(walkers::ApartFilter(Filter(0xCD, 6, 4, true), 0x00CF0308) == Filter(0xCF, 0, 4),
	      "linked dropped: a linked body would go to the matrix and the parts");
	Check(walkers::ApartFilter(Filter(0xCD, 6, 8, false, true), 0x00CF0308) == Filter(0xCF, 0, 8, false, true),
	      "a body switched off stays off");
	Check(walkers::ApartFilter(Filter(0xCD, 6, 8) | 0x20E0, 0x00CF0308) == (Filter(0xCF, 0, 8) | 0x20E0),
	      "the bits the rule does not read (5 to 7, 13) kept");
}

void TestKeptApart() {
	std::printf("Kept apart\n");
	Tables t = EngineTables();
	UInt32 apart[kRagdollBodies];
	for (UInt32 c = 0; c < kRagdollBodies; ++c) {
		apart[c] = walkers::ApartFilter(kCorpseFilters[c], kWalkerFilters[0]);
	}
	bool all = true;
	for (UInt32 c = 0; c < kRagdollBodies; ++c) {
		all = all && walkers::KeptApart(kWalkerFilters, kRagdollBodies, apart[c], t.words);
	}
	Check(all, "every corpse body, moved over, meets no walker body either way round (part 0 is in no bone row)");
	bool self = false;
	for (UInt32 a = 0; a < kRagdollBodies; ++a) {
		for (UInt32 b = 0; b < kRagdollBodies; ++b) {
			self = self || RuleCollides(apart[a], apart[b], t.words);
		}
	}
	Check(!self, "the corpse's own bodies no longer meet each other (one group, part 0) - it lies settled");
	const UInt32 stat = Filter(0x0002, 0, 1);
	const UInt32 ground = Filter(0x0003, 0, 17);
	const UInt32 other = Filter(0x00D1, 2, 8);
	bool world = true;
	for (UInt32 c = 0; c < kRagdollBodies; ++c) {
		world = world && RuleCollides(apart[c], stat, t.words) == RuleCollides(kCorpseFilters[c], stat, t.words) &&
		        RuleCollides(stat, apart[c], t.words) == RuleCollides(stat, kCorpseFilters[c], t.words) &&
		        RuleCollides(apart[c], ground, t.words) && RuleCollides(apart[c], other, t.words) &&
		        !RuleCollides(apart[c], Filter(0x0009, 0, 20), t.words);
	}
	Check(world, "the world meets it as before: statics and ground, another actor's ragdoll; the player's "
	             "controller not, as before");
	Check(!walkers::KeptApart(kWalkerFilters, kRagdollBodies, kCorpseFilters[0], t.words),
	      "the corpse's own filter: not kept apart");
	Tables changed = EngineTables();
	changed.words[32 + 2] |= 1u;
	Check(!walkers::KeptApart(kWalkerFilters, kRagdollBodies, apart[0], changed.words),
	      "a bone row that meets part 0 (another mod's table): not kept apart - the check refuses it");
	const UInt32 noGroup[] = {0x00000208};
	Check(!walkers::KeptApart(noGroup, 1, walkers::ApartFilter(kCorpseFilters[0], noGroup[0]), t.words),
	      "a walker in group 0: everything meets it - not kept apart");
	Check(!walkers::KeptApart(kWalkerFilters, 0, apart[0], t.words), "no walker body read: nothing to go by");
}

}  // namespace

int main() {
	TestRule();
	TestMeasured();
	TestApartFilter();
	TestKeptApart();
	if (g_failures != 0) {
		std::printf("%d failed\n", g_failures);
		return 1;
	}
	std::printf("all passed\n");
	return 0;
}
