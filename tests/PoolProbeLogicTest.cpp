// Every flow of the pool-12 probe's check (docs/walkers-spec.md 4.23 B, the
// pool-12 crash): whether a free-list link points at a block of the pool.

#include "PoolProbeLogic.h"

#include <cstdio>

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf("  %s  %s\n", condition ? "ok  " : "FAIL", what);
	if (!condition) {
		++g_failures;
	}
}

void TestLink() {
	std::printf("A free-list link of pool 12\n");
	walkers::PoolView v;
	v.base = 0x10000000;
	v.size = 0x3000;  // three pages
	v.blockSize = 12;
	UInt16 uses[3] = {5, 0xFFFF, 0};
	v.pageUses = uses;
	v.pages = 3;
	using walkers::PoolLinkFault;
	Check(walkers::CheckPoolLink(v, 0) == PoolLinkFault::None, "0: the list's end - fine");
	Check(walkers::CheckPoolLink(v, 0x10000000 + 12 * 7) == PoolLinkFault::None, "a block of the first page: fine");
	Check(walkers::CheckPoolLink(v, 0x6EFEF8A0) == PoolLinkFault::Outside, "2026-11-20's next, 6EFEF8A0: outside the pool");
	Check(walkers::CheckPoolLink(v, 0x0FFFFFF4) == PoolLinkFault::Outside, "below the base: outside");
	Check(walkers::CheckPoolLink(v, 0x10003000) == PoolLinkFault::Outside, "at base + size: outside");
	Check(walkers::CheckPoolLink(v, 0x10000000 + 12 * 7 + 2) == PoolLinkFault::Misaligned,
	      "2 past a block (the November crashes' heads): not on a block");
	Check(walkers::CheckPoolLink(v, 0x10000000 + 4092) == PoolLinkFault::Misaligned, "past the 341 blocks of a page: no block");
	Check(walkers::CheckPoolLink(v, 0x10001000 + 24) == PoolLinkFault::NoPage, "a page not committed (use 0xFFFF): no block");
	Check(walkers::CheckPoolLink(v, 0x10002000) == PoolLinkFault::None, "an empty committed page's first block: fine");
	walkers::PoolView none;
	Check(walkers::CheckPoolLink(none, 0x10000000) == PoolLinkFault::NoPool, "no pool read: nothing to check against");
	Check(walkers::PoolLinkFaultName(PoolLinkFault::Outside)[0] != '\0', "a name for the log");
}

}  // namespace

int main() {
	TestLink();
	std::printf(g_failures == 0 ? "All passed\n" : "%d FAILED\n", g_failures);
	return g_failures == 0 ? 0 : 1;
}
