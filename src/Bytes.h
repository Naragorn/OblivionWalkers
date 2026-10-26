#pragma once

// Little-endian words in a byte image, like the engine's own (the co-save
// records of RiseLogic.h and SafeguardLogic.h).

#include "Types.h"

namespace walkers {

inline void Put32(UInt8* out, UInt32 value) {
	out[0] = static_cast<UInt8>(value & 0xFF);
	out[1] = static_cast<UInt8>((value >> 8) & 0xFF);
	out[2] = static_cast<UInt8>((value >> 16) & 0xFF);
	out[3] = static_cast<UInt8>((value >> 24) & 0xFF);
}

inline UInt32 Get32(const UInt8* in) {
	return static_cast<UInt32>(in[0]) | (static_cast<UInt32>(in[1]) << 8) | (static_cast<UInt32>(in[2]) << 16) |
	       (static_cast<UInt32>(in[3]) << 24);
}

}  // namespace walkers
