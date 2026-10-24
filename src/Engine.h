#pragma once

// Reading the engine of Oblivion 1.2.416: the few accessors and addresses
// Rise.cpp and Replace.cpp share (docs/walkers-spec.md, section 2). Read
// with REA over Oblivion.exe and in xOBSE's GameForms.h / GameData.h.

#include "Types.h"

namespace walkers {
namespace engine {

inline constexpr UInt32 kFormTypeOffset = 0x04;  // TESForm::typeID
inline constexpr UInt32 kFormIdOffset = 0x0C;    // TESForm::refID
inline constexpr UInt8 kFormTypeNpc = 0x23;
inline constexpr UInt8 kFormTypeCreature = 0x24;
inline constexpr UInt8 kFormTypeLeveledCreature = 0x25;
inline constexpr UInt32 kLookupFormByID = 0x0046B250;  // xOBSE GameAPI.cpp, cdecl (UInt32 id)
inline constexpr UInt32 kDataHandlerPointer = 0x00B33A98;
// DataHandler (xOBSE GameData.h): the loaded mods by their runtime index,
// ModEntry::Data (the engine's TESFile) with the file name at +0x1C.
inline constexpr UInt32 kDataHandlerModCountOffset = 0x8D0;
inline constexpr UInt32 kDataHandlerModsByIdOffset = 0x8D4;
inline constexpr UInt32 kModEntryNameOffset = 0x1C;

inline UInt32 Read(UInt32 address) { return *reinterpret_cast<const UInt32*>(address); }
inline UInt8 Read8(UInt32 address) { return *reinterpret_cast<const UInt8*>(address); }
inline float FloatAt(UInt32 address) { return *reinterpret_cast<const float*>(address); }

// Out of the null page and aligned: the least an engine object pointer is.
inline bool LooksLikeObject(UInt32 address) { return address >= 0x00010000 && (address & 3) == 0; }

inline UInt32 LookupForm(UInt32 formId) {
	using LookupFn = UInt32(__cdecl*)(UInt32 id);
	return reinterpret_cast<LookupFn>(kLookupFormByID)(formId);
}

inline UInt32 FormIdOf(UInt32 object) { return LooksLikeObject(object) ? Read(object + kFormIdOffset) : 0; }

// The file name of the mod at a runtime load index; nullptr when none is
// loaded there.
inline const char* ModNameAt(UInt32 index) {
	const UInt32 handler = Read(kDataHandlerPointer);
	if (!LooksLikeObject(handler) || index >= Read(handler + kDataHandlerModCountOffset) || index > 0xFE) {
		return nullptr;
	}
	const UInt32 entry = Read(handler + kDataHandlerModsByIdOffset + index * 4);
	return LooksLikeObject(entry) ? reinterpret_cast<const char*>(entry + kModEntryNameOffset) : nullptr;
}

}  // namespace engine
}  // namespace walkers
