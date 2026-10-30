// Every flow of the walkers' looks (docs/walkers-spec.md 4.1, "Twenty looks"
// and "MOO's looks"): which records there are, which the game may show, which
// a fallen of a race rises as, and which entries the variant list keeps.

#include "ReplaceLogic.h"
#include "WalkerLooks.h"

#include <cstdio>

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf("  %s  %s\n", condition ? "ok  " : "FAIL", what);
	if (!condition) {
		++g_failures;
	}
}

using walkers::LookFamily;

constexpr UInt32 kArgonian = 0x00023FE9;
constexpr UInt32 kOrc = 0x000191C0;
constexpr UInt32 kHighElf = 0x00019204;
constexpr UInt32 kDarkElf = 0x000191C1;
constexpr UInt32 kNord = 0x000224FD;

UInt32 CountOf(LookFamily family) {
	UInt32 n = 0;
	for (const walkers::WalkerLook& look : walkers::kWalkerLooks) {
		n += look.family == family ? 1u : 0u;
	}
	return n;
}

void TestTable() {
	std::printf("The records\n");
	bool firstFour = true;
	for (UInt32 i = 0; i < 4; ++i) {
		firstFour = firstFour && walkers::kWalkerLooks[i].localId == 0x801 + i;
	}
	Check(firstFour, "the first four at 0x801..0x804 as always, so a save's walkers keep their base");
	bool unique = true;
	bool pastEditions = true;
	bool vanillaFirst = true;
	for (UInt32 i = 0; i < walkers::kWalkerCount; ++i) {
		for (UInt32 j = i + 1; j < walkers::kWalkerCount; ++j) {
			unique = unique && walkers::kWalkerLooks[i].localId != walkers::kWalkerLooks[j].localId;
		}
		pastEditions = pastEditions && (i < 4 || walkers::kWalkerLooks[i].localId > 0x90D);
		vanillaFirst = vanillaFirst &&
		               ((i < walkers::kVanillaLooks) == (walkers::kWalkerLooks[i].family == LookFamily::Vanilla));
	}
	Check(unique && pastEditions, "none twice, the new ones past the Courier's last edition (0x90D)");
	Check(vanillaFirst && walkers::kWalkerLooks[19].localId == 0x91D && walkers::kWalkerLooks[20].localId == 0x91E,
	      "the twenty vanilla looks first (0x801..0x804, 0x90E..0x91D), MOO's from 0x91E on");
	Check(CountOf(LookFamily::Vanilla) == 20 && CountOf(LookFamily::MooDefault) == 8 &&
	          CountOf(LookFamily::Argonian) == 10 && CountOf(LookFamily::Orc) == 10 &&
	          CountOf(LookFamily::HighElf) == 8 && CountOf(LookFamily::DarkElf) == 8 && walkers::kWalkerCount == 64,
	      "20 vanilla, 8 MOO default, 10 Argonian, 10 Orc, 8 High Elf, 8 Dark Elf: 64");
	Check(walkers::kWalkerLooks[63].localId == 0x949, "the last at 0x949");
	Check(walkers::kVariantEntriesMost == walkers::kVariantSlots && walkers::kAnyWalkerEntriesMost == 2 * 64,
	      "the variant list holds ten count slots, the any-walker list every walker at most twice: 128 entries");
	LookFamily none = LookFamily::Vanilla;
	Check(walkers::kAnyWalkerLocalId == 0x94A && !walkers::FamilyOfLocal(walkers::kAnyWalkerLocalId, none),
	      "the any-walker list right after the last walker, at 0x94A - no walker's ID");
}

void TestRaces() {
	std::printf("Races\n");
	Check(walkers::RaceFamily(kArgonian) == LookFamily::Argonian && walkers::RaceFamily(kOrc) == LookFamily::Orc &&
	          walkers::RaceFamily(kHighElf) == LookFamily::HighElf &&
	          walkers::RaceFamily(kDarkElf) == LookFamily::DarkElf,
	      "Oblivion.esm's Argonian, Orc, High Elf and Dark Elf");
	Check(walkers::RaceFamily(kNord) == LookFamily::Vanilla && walkers::RaceFamily(0) == LookFamily::Vanilla,
	      "a Nord, or no race (a creature): no look of its own");
	Check(walkers::LookUsable(LookFamily::Vanilla, false) && !walkers::LookUsable(LookFamily::Orc, false) &&
	          !walkers::LookUsable(LookFamily::MooDefault, false) && walkers::LookUsable(LookFamily::Orc, true),
	      "without MOO only the vanilla looks (MOO's meshes come with its archives)");
}

// Two of each family, as found.
struct Found {
	UInt32 bases[12] = {};
	LookFamily families[12] = {};
	bool headless[12] = {};
	UInt32 count = 0;
	void Add(UInt32 base, LookFamily family, bool noHead = false) {
		bases[count] = base;
		families[count] = family;
		headless[count] = noHead;
		++count;
	}
};

bool OnlyOf(const UInt32* out, UInt32 n, UInt32 low, UInt32 high) {
	for (UInt32 i = 0; i < n; ++i) {
		if (out[i] < low || out[i] > high) {
			return false;
		}
	}
	return n > 0;
}

void TestRise() {
	std::printf("Who rises as what\n");
	Found f;
	f.Add(0x10, LookFamily::Vanilla);
	f.Add(0x11, LookFamily::Vanilla);
	f.Add(0x20, LookFamily::MooDefault);
	f.Add(0x21, LookFamily::MooDefault);
	f.Add(0x30, LookFamily::Argonian);
	f.Add(0x31, LookFamily::Argonian);
	f.Add(0x40, LookFamily::Orc);
	f.Add(0x41, LookFamily::Orc);
	f.Add(0x50, LookFamily::HighElf);
	f.Add(0x60, LookFamily::DarkElf);
	UInt32 out[24];
	UInt32 n = walkers::RiseCandidates(f.bases, f.families, f.headless, f.count, kArgonian, true, out);
	Check(n == 4 && OnlyOf(out, n, 0x30, 0x31), "an Argonian with MOO: one of the Argonian looks");
	n = walkers::RiseCandidates(f.bases, f.families, f.headless, f.count, kOrc, true, out);
	Check(n == 4 && OnlyOf(out, n, 0x40, 0x41), "an Orc: an Orc look");
	n = walkers::RiseCandidates(f.bases, f.families, f.headless, f.count, kHighElf, true, out);
	const UInt32 high = n == 2 ? out[0] : 0;
	n = walkers::RiseCandidates(f.bases, f.families, f.headless, f.count, kDarkElf, true, out);
	Check(high == 0x50 && n == 2 && out[0] == 0x60, "a High Elf and a Dark Elf: their own");
	n = walkers::RiseCandidates(f.bases, f.families, f.headless, f.count, kNord, true, out);
	Check(n == 8 && OnlyOf(out, n, 0x10, 0x21), "a Nord with MOO: the vanilla and MOO's default looks");
	n = walkers::RiseCandidates(f.bases, f.families, f.headless, f.count, kArgonian, false, out);
	Check(n == 4 && OnlyOf(out, n, 0x10, 0x11), "an Argonian without MOO: the vanilla looks alone");
	n = walkers::RiseCandidates(f.bases, f.families, f.headless, f.count, kNord, false, out);
	Check(n == 4 && OnlyOf(out, n, 0x10, 0x11), "a Nord without MOO: the vanilla looks alone");
	Found vanillaOnly;
	vanillaOnly.Add(0x10, LookFamily::Vanilla);
	n = walkers::RiseCandidates(vanillaOnly.bases, vanillaOnly.families, vanillaOnly.headless, vanillaOnly.count, kOrc, true, out);
	Check(n == 2 && out[0] == 0x10, "an Orc with MOO but no Orc look found (an older ESP): the vanilla ones");
	Check(walkers::RiseCandidates(f.bases, f.families, f.headless, 0, kNord, true, out) == 0, "no walkers found: none");
	// Nadi, 2026-10-27 ("ja" to the headless ones less often): a look with
	// a head stands twice among the candidates, a headless one once - half
	// as likely.
	Found mixed;
	mixed.Add(0x30, LookFamily::Argonian);
	mixed.Add(0x31, LookFamily::Argonian, true);
	n = walkers::RiseCandidates(mixed.bases, mixed.families, mixed.headless, mixed.count, kArgonian, true, out);
	UInt32 withHead = 0;
	UInt32 without = 0;
	for (UInt32 i = 0; i < n; ++i) {
		withHead += out[i] == 0x30 ? 1u : 0u;
		without += out[i] == 0x31 ? 1u : 0u;
	}
	Check(n == 3 && withHead == 2 && without == 1, "an Argonian: the look with a head twice, the headless one once");
	n = walkers::RiseCandidates(mixed.bases, mixed.families, mixed.headless, mixed.count, kArgonian, true, out, false);
	Check(n == 2 && out[0] == 0x30 && out[1] == 0x30,
	      "without the headless (DieOnlyBeheaded, 4.17): only the look with a head");
}

void TestVariant() {
	std::printf("The variant list\n");
	Check(walkers::VariantKeeps(LookFamily::Vanilla, false) && walkers::VariantKeeps(LookFamily::Vanilla, true),
	      "a vanilla walker's entries: kept either way");
	Check(!walkers::VariantKeeps(LookFamily::Argonian, false) && !walkers::VariantKeeps(LookFamily::MooDefault, false),
	      "without MOO the list is cut at the first MOO entry");
	Check(walkers::VariantKeeps(LookFamily::Argonian, true) && walkers::VariantKeeps(LookFamily::MooDefault, true),
	      "with MOO all entries kept");
	// Nadi, 2026-10-27: ten walkers at once were one look - one draw copied
	// count times; spawn-mixed.txt 15:37 with the any-walker list drawn per
	// copy (LVLF 2) gave four placements of all different looks.
	Check(walkers::AnyWalkerTimes(false) == 2 && walkers::AnyWalkerTimes(true) == 1,
	      "in the any-walker list a walker with a head twice, a headless one once: half as often");
	LookFamily family = LookFamily::Vanilla;
	Check(walkers::FamilyOfLocal(0x801, family) && family == LookFamily::Vanilla &&
	          walkers::FamilyOfLocal(0x926, family) && family == LookFamily::Argonian &&
	          walkers::FamilyOfLocal(0x949, family) && family == LookFamily::DarkElf,
	      "an entry's walker known by its local ID");
	Check(!walkers::FamilyOfLocal(0x805, family) && !walkers::FamilyOfLocal(0x94A, family),
	      "a local ID that is no walker (the list itself, past the last): not known, the entry left alone");
}

}  // namespace

int main() {
	TestTable();
	TestRaces();
	TestRise();
	TestVariant();
	if (g_failures != 0) {
		std::printf("%d failed\n", g_failures);
		return 1;
	}
	std::printf("all passed\n");
	return 0;
}
