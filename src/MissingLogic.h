#pragma once

// A body the game does not hold right now, process-free (docs/walkers-spec.md
// 4.1 and 4.11b). rise-after-away.txt 2026-10-27 10:22: four interiors on,
// the hall left the game's cell buffer and its references were freed - the
// waiting body's ID found nothing - and back in the hall the same reference
// lay there again. So a reference not found is kept, its clock standing, for
// kForgetMissingDays game days - about when the game resets a cell and clears
// its dead away (from memory, not read) - and only then forgotten.

namespace walkers {

inline constexpr float kForgetMissingDays = 3.0f;

// One look at an entry whose reference is not found, `nowDays` the game's
// clock in days (-1: not read). `missingSince` is the entry's own note: -1
// while it is held, else the day it was first missed. True: keep it.
inline bool KeepMissing(float& missingSince, float nowDays) {
	if (nowDays < 0.0f) {
		return true;
	}
	if (missingSince < 0.0f || nowDays < missingSince) {
		missingSince = nowDays;  // first missed, or an older save's clock: from now
		return true;
	}
	return nowDays - missingSince < kForgetMissingDays;
}

}  // namespace walkers
