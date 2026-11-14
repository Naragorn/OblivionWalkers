// Every flow of the look inside a slow console line ([Debug] SampleLine,
// SamplerLogic.h): which values are return addresses, how the samples are
// counted, which addresses come out on top.

#include "SamplerLogic.h"

#include <cstdio>
#include <cstring>

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf("  %s  %s\n", condition ? "ok  " : "FAIL", what);
	if (!condition) {
		++g_failures;
	}
}

bool Ends(UInt8 b0, UInt8 b1, UInt8 b2, UInt8 b3, UInt8 b4, UInt8 b5) {
	const UInt8 before[6] = {b0, b1, b2, b3, b4, b5};
	return walkers::EndsInCall(before);
}

void TestCalls() {
	std::printf("A call just before\n");
	Check(Ends(0xE8, 0x11, 0x22, 0x33, 0x44, 0x55), "E8 rel32 (call 0x...)");
	Check(Ends(0xFF, 0x15, 0x00, 0x10, 0xA2, 0x00), "FF 15 abs32 (call [import])");
	Check(Ends(0x00, 0x00, 0x00, 0x00, 0xFF, 0xD0) && Ends(0x00, 0x00, 0x00, 0x00, 0xFF, 0xD7),
	      "FF D0..D7 (call eax .. call edi)");
	Check(Ends(0x00, 0x00, 0x00, 0x00, 0xFF, 0x10) && Ends(0x00, 0x00, 0x00, 0x00, 0xFF, 0x12),
	      "FF 10..17 (call [eax], call [edx])");
	Check(Ends(0x00, 0x00, 0x00, 0xFF, 0x50, 0x40) && Ends(0x00, 0x00, 0x00, 0xFF, 0x52, 0x08),
	      "FF 50..57 disp8 (call [eax+0x40])");
	Check(Ends(0x00, 0x00, 0xFF, 0x54, 0x24, 0x10), "FF 54 SIB disp8 (call [esp+0x10])");
	Check(Ends(0xFF, 0x90, 0x74, 0x01, 0x00, 0x00) && Ends(0xFF, 0x92, 0x28, 0x02, 0x00, 0x00),
	      "FF 90..97 disp32 (call [eax+0x174], call [edx+0x228])");
	Check(!Ends(0x00, 0x00, 0x00, 0x00, 0x8B, 0xC8) && !Ends(0x00, 0x00, 0x00, 0x00, 0x00, 0x00),
	      "a mov, zeros: no call");
	Check(!Ends(0x00, 0x00, 0x00, 0x00, 0xFF, 0xE0) && !Ends(0x00, 0x00, 0x00, 0xFF, 0x60, 0x08),
	      "FF E0 / FF 60 (jmp, not call): no call");
	Check(!Ends(0x00, 0x00, 0x00, 0x00, 0xFF, 0x14) && !Ends(0x00, 0x00, 0x00, 0x00, 0xFF, 0x15),
	      "FF 14 / FF 15 as the last two bytes (they need more bytes after): no call there");
	Check(!Ends(0x00, 0x00, 0x00, 0xFF, 0x54, 0x00) && !Ends(0xFF, 0x94, 0x00, 0x00, 0x00, 0x00),
	      "FF 54 / FF 94 at the places of the forms without SIB: no call");
	Check(walkers::InExeCode(0x00401000) && walkers::InExeCode(0x00A27C38) && !walkers::InExeCode(0x00A27C39) &&
	          !walkers::InExeCode(0x00400FFF) && !walkers::InExeCode(0x10001000),
	      "Oblivion.exe's .text from 0x00401000 for 0x626C39 bytes; a DLL's address outside");
}

// The code as the test lays it out: a call before every address ending in 5.
UInt8 g_code[6];
const UInt8* Before(UInt32 address) {
	std::memset(g_code, 0, sizeof g_code);
	if ((address & 0xF) == 5) {
		g_code[0] = 0xE8;
	}
	return g_code;
}

void TestCounting() {
	std::printf("Counting the samples\n");
	walkers::SampleCounts c;
	const UInt32 stack1[] = {0x00000005, 0x00512545, 0x12345678, 0x00512545, 0x00529B15, 0x00529B14, 0x10001005};
	const UInt32 found = walkers::CountSample(c, 0x0040ABCD, stack1, 7, Before);
	Check(found == 2, "the return addresses in the exe's code with a call before, each once a sample (2 of 7 words)");
	const UInt32 stack2[] = {0x00529B15};
	walkers::CountSample(c, 0x0040ABCD, stack2, 1, Before);
	walkers::CountSample(c, 0x00700000, stack2, 0, Before);
	Check(c.samples == 3, "three samples");
	UInt32 top[4] = {};
	UInt32 counts[4] = {};
	const UInt32 n = walkers::TopAddresses(c, top, counts, 4);
	const bool twice = (top[0] == 0x00529B15 && top[1] == 0x0040ABCD) || (top[0] == 0x0040ABCD && top[1] == 0x00529B15);
	Check(n == 4 && twice && counts[0] == 2 && counts[1] == 2,
	      "the most seen first: the return address and the instruction pointer seen twice");
	Check(counts[2] == 1 && counts[3] == 1, "then those seen once");
	UInt32 one[1] = {};
	UInt32 oneCount[1] = {};
	Check(walkers::TopAddresses(c, one, oneCount, 1) == 1 && oneCount[0] == 2, "only the most asked for: kept to it");
	walkers::CountAddress(c, 0);
	Check(c.used == 4, "address 0 is not counted");
	walkers::SampleCounts full;
	for (UInt32 a = 1; a <= walkers::kSampleSlots; ++a) {
		walkers::CountAddress(full, a * 16);
	}
	Check(full.used == walkers::kSampleSlots * 3 / 4 && full.lost == walkers::kSampleSlots / 4,
	      "the table filled three quarters: new addresses counted as lost, not looped over");
	walkers::CountAddress(full, 16);
	Check(full.lost == walkers::kSampleSlots / 4, "an address already in a full table: still counted");
	walkers::SampleCounts none;
	UInt32 nothing[2] = {};
	UInt32 nothingCounts[2] = {};
	Check(walkers::TopAddresses(none, nothing, nothingCounts, 2) == 0, "no samples: none on top");
}

}  // namespace

int main() {
	TestCalls();
	TestCounting();
	std::printf(g_failures == 0 ? "All passed\n" : "%d FAILED\n", g_failures);
	return g_failures == 0 ? 0 : 1;
}
