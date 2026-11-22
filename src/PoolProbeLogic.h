#pragma once

// The pool-12 probe's check, process-free (docs/walkers-spec.md 4.23 B, the
// pool-12 crash, read 2026-11-20 from the EXE): Oblivion's small-block pool
// for 12 bytes keeps its free blocks as a doubly linked list {+0 prev, +4
// next}; the crash at 0x004025D2 unlinks a head whose next was overwritten
// after the free. A link is sound when it is 0 or a block of the pool: inside
// base..base+size, on a 12-byte slot of a page's first 341, its page
// committed (the page's use count not 0xFFFF).

#include "Types.h"

namespace walkers {

struct PoolView {
	UInt32 base = 0;       // pool +0x40
	UInt32 size = 0;       // pool +0x110, bytes reserved
	UInt32 blockSize = 0;  // pool +0x100
	const UInt16* pageUses = nullptr;  // pool +0x108, a WORD a 4 KB page
	UInt32 pages = 0;      // pool +0x10C
};

enum class PoolLinkFault : UInt8 { None, NoPool, Outside, Misaligned, NoPage };

inline PoolLinkFault CheckPoolLink(const PoolView& v, UInt32 p) {
	if (p == 0) {
		return PoolLinkFault::None;
	}
	if (v.base == 0 || v.size == 0 || v.blockSize == 0) {
		return PoolLinkFault::NoPool;
	}
	if (p < v.base || p - v.base >= v.size) {
		return PoolLinkFault::Outside;
	}
	const UInt32 offset = p - v.base;
	const UInt32 inPage = offset & 0xFFF;
	const UInt32 perPage = 0x1000 / v.blockSize;
	if (inPage % v.blockSize != 0 || inPage >= perPage * v.blockSize) {
		return PoolLinkFault::Misaligned;
	}
	const UInt32 page = offset >> 12;
	if (v.pageUses != nullptr && page < v.pages && v.pageUses[page] == 0xFFFF) {
		return PoolLinkFault::NoPage;
	}
	return PoolLinkFault::None;
}

inline const char* PoolLinkFaultName(PoolLinkFault f) {
	switch (f) {
		case PoolLinkFault::None:
			return "sound";
		case PoolLinkFault::NoPool:
			return "no pool";
		case PoolLinkFault::Outside:
			return "outside the pool";
		case PoolLinkFault::Misaligned:
			return "not on a block";
		case PoolLinkFault::NoPage:
			return "on a page not committed";
	}
	return "?";
}

}  // namespace walkers
