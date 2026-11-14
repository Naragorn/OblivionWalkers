#pragma once

// A look inside one slow console line, process-free ([Debug] SampleLine, INI
// only, a test switch). StepTiming shows that a line was slow - SetStage on
// a held dungeon's quest took 20 to 41 ms in 2026-11-13/14's runs (1 to 4 ms
// with the player in Anvil) - not where in the engine the time went. While
// such a line runs, a second thread stops the main thread every few hundred
// microseconds and notes where it stands: its instruction pointer and every
// return address on its stack (a value inside Oblivion.exe's code that a call
// instruction ends just before). Counted over the samples, the functions the
// time went through stand out by how often their return addresses were seen.

#include "Types.h"

namespace walkers {

// Oblivion.exe's .text (objdump -h, 1.2.416): 0x00401000, 0x00626C39 bytes.
inline constexpr UInt32 kExeCodeFirst = 0x00401000;
inline constexpr UInt32 kExeCodeEnd = 0x00401000 + 0x00626C39;

inline bool InExeCode(UInt32 address) { return address >= kExeCodeFirst && address < kExeCodeEnd; }

// Whether the six bytes before an address (`before[5]` the byte just before
// it) end in a call: E8 rel32; FF 15 abs32; FF /2 through a register (FF D0
// to D7) or [reg] (FF 10 to 17), through [reg+disp8] (FF 50 to 57 and a byte, FF 54 with a SIB
// byte), through [reg+disp32] (FF 90 to 97 and four bytes).
inline bool EndsInCall(const UInt8* before) {
	if (before[0] == 0xE8 || (before[0] == 0xFF && before[1] == 0x15)) {
		return true;  // the five-byte call, or the six-byte one through an import
	}
	if (before[4] == 0xFF && ((before[5] >= 0xD0 && before[5] <= 0xD7) ||
	                          (before[5] >= 0x10 && before[5] <= 0x17 && before[5] != 0x14 && before[5] != 0x15))) {
		return true;  // through a register, or through [reg]
	}
	if (before[3] == 0xFF && before[4] >= 0x50 && before[4] <= 0x57 && before[4] != 0x54) {
		return true;
	}
	if (before[2] == 0xFF && before[3] == 0x54) {
		return true;
	}
	return before[0] == 0xFF && before[1] >= 0x90 && before[1] <= 0x97 && before[1] != 0x94;
}

inline constexpr UInt32 kSampleSlots = 1024;

// How often each address was seen, in a table of fixed size (open
// addressing); an address that finds no room is counted in `lost`.
struct SampleCounts {
	UInt32 address[kSampleSlots] = {};
	UInt32 count[kSampleSlots] = {};
	UInt32 used = 0;
	UInt32 lost = 0;
	UInt32 samples = 0;
};

inline void CountAddress(SampleCounts& c, UInt32 address) {
	if (address == 0) {
		return;
	}
	UInt32 slot = (address * 2654435761u) % kSampleSlots;
	for (UInt32 k = 0; k < kSampleSlots; ++k, slot = (slot + 1) % kSampleSlots) {
		if (c.address[slot] == address) {
			++c.count[slot];
			return;
		}
		if (c.address[slot] == 0) {
			if (c.used * 4 >= kSampleSlots * 3) {
				break;  // three quarters full: no more new addresses
			}
			c.address[slot] = address;
			c.count[slot] = 1;
			++c.used;
			return;
		}
	}
	++c.lost;
}

// One sample: the instruction pointer, then the return addresses found in
// `stack` (`words` dwords from the stack pointer up), each counted once a
// sample. `before` gives the six bytes before an address in the code.
template <typename BytesBefore>
inline UInt32 CountSample(SampleCounts& c, UInt32 eip, const UInt32* stack, UInt32 words, BytesBefore before) {
	++c.samples;
	CountAddress(c, eip);
	UInt32 seen[64];
	UInt32 found = 0;
	for (UInt32 i = 0; i < words && found < 64; ++i) {
		const UInt32 v = stack[i];
		if (!InExeCode(v) || v < kExeCodeFirst + 6 || v == eip || !EndsInCall(before(v))) {
			continue;
		}
		bool again = false;
		for (UInt32 k = 0; k < found && !again; ++k) {
			again = seen[k] == v;
		}
		if (!again) {
			seen[found++] = v;
			CountAddress(c, v);
		}
	}
	return found;
}

// The `most` addresses seen most often, the most first; their number.
inline UInt32 TopAddresses(const SampleCounts& c, UInt32* out, UInt32* counts, UInt32 most) {
	UInt32 n = 0;
	for (UInt32 s = 0; s < kSampleSlots; ++s) {
		if (c.address[s] == 0) {
			continue;
		}
		UInt32 at = n < most ? n++ : most;
		if (at == most) {
			if (most == 0 || c.count[s] <= counts[most - 1]) {
				continue;
			}
			at = most - 1;
		}
		while (at > 0 && counts[at - 1] < c.count[s]) {
			out[at] = out[at - 1];
			counts[at] = counts[at - 1];
			--at;
		}
		out[at] = c.address[s];
		counts[at] = c.count[s];
	}
	return n;
}

}  // namespace walkers
