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
inline constexpr UInt8 kFormTypeCell = 0x30;        // xOBSE GameForms.h; 0x005E1F60 tests it
inline constexpr UInt8 kFormTypeWorldSpace = 0x35;  // xOBSE GameForms.h; 0x005E1F40 tests it
inline constexpr UInt8 kFormTypeGlobal = 0x04;
inline constexpr UInt32 kGlobalValueOffset = 0x24;  // TESGlobal's float
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

// A reference moved and turned without the console (2026-11-13, StepTiming:
// a console MoveTo was ~520 us, a SetAngle ~100 us - each a script compiled
// and run; the grab's lunge sent both every frame). What the engine's own
// SetPos (its handler 0x00508FC0) and SetAngle (0x00509110) do, read off
// Oblivion.exe with objdump:
// - SetPos: thiscall 0x004D8A30(ref, NiPoint3 by value; ret 0Ch) writes
//   +0x2C..+0x34 and marks the reference changed; an Actor (the RTTI cast
//   0x009832E6(ref, 0, 0x00B03054, 0x00B05538, 0)) has its character
//   controller (thiscall 0x0065A2C0(actor)) moved, thiscall 0x00452A10(ctl,
//   &pos; ret 4), unless thiscall 0x0088D370(ctl + 0x1E0) answers 4; then
//   the 3D (vtable +0x154) gets the translate (+0x54..+0x5C), cdecl
//   0x00897A20(node, 1), thiscall 0x00707370(node, 0.0f, 0; ret 8).
// - SetAngle z: thiscall 0x004D8A10(ref, radians; ret 4) writes +0x28; the
//   3D's rotate (+0x30, 9 floats) from thiscall 0x004D7AF0(ref, &matrix),
//   cdecl 0x00897A20(node, 1), and the update unless vtable +0x164 answers.
inline void DirectSetPos(UInt32 ref, float x, float y, float z) {
	using SetPosFn = void(__fastcall*)(void* self, void* edx, float x, float y, float z);
	reinterpret_cast<SetPosFn>(0x004D8A30)(reinterpret_cast<void*>(ref), nullptr, x, y, z);
	using CastFn = void*(__cdecl*)(void* object, UInt32 vfDelta, void* from, void* to, UInt32 isReference);
	void* const actor = reinterpret_cast<CastFn>(0x009832E6)(reinterpret_cast<void*>(ref), 0,
	                                                         reinterpret_cast<void*>(0x00B03054),
	                                                         reinterpret_cast<void*>(0x00B05538), 0);
	float pos[3] = {x, y, z};
	if (actor != nullptr) {
		using ThisFn = void*(__fastcall*)(void* self, void* edx);
		void* const ctl = reinterpret_cast<ThisFn>(0x0065A2C0)(actor, nullptr);
		if (ctl != nullptr) {
			using StateFn = UInt32(__fastcall*)(void* self, void* edx);
			const UInt32 state =
			    reinterpret_cast<StateFn>(0x0088D370)(reinterpret_cast<UInt8*>(ctl) + 0x1E0, nullptr);
			if (state != 4) {
				using MoveFn = void(__fastcall*)(void* self, void* edx, float* pos);
				reinterpret_cast<MoveFn>(0x00452A10)(ctl, nullptr, pos);
			}
		}
	}
	using NodeFn = UInt32(__fastcall*)(void* self, void* edx);
	const UInt32 node = reinterpret_cast<NodeFn>(Read(Read(ref) + 0x154))(reinterpret_cast<void*>(ref), nullptr);
	if (LooksLikeObject(node)) {
		*reinterpret_cast<float*>(node + 0x54) = x;
		*reinterpret_cast<float*>(node + 0x58) = y;
		*reinterpret_cast<float*>(node + 0x5C) = z;
		using MarkFn = void(__cdecl*)(UInt32 node, UInt32 flag);
		reinterpret_cast<MarkFn>(0x00897A20)(node, 1);
		using UpdateFn = void(__fastcall*)(void* self, void* edx, float time, UInt32 flags);
		reinterpret_cast<UpdateFn>(0x00707370)(reinterpret_cast<void*>(node), nullptr, 0.0f, 0);
	}
}

inline void DirectSetAngleZ(UInt32 ref, float radians) {
	using SetFn = void(__fastcall*)(void* self, void* edx, float radians);
	reinterpret_cast<SetFn>(0x004D8A10)(reinterpret_cast<void*>(ref), nullptr, radians);
	using NodeFn = UInt32(__fastcall*)(void* self, void* edx);
	const UInt32 node = reinterpret_cast<NodeFn>(Read(Read(ref) + 0x154))(reinterpret_cast<void*>(ref), nullptr);
	if (!LooksLikeObject(node)) {
		return;
	}
	float matrix[9] = {};
	using MatrixFn = float*(__fastcall*)(void* self, void* edx, float* out);
	const float* m = reinterpret_cast<MatrixFn>(0x004D7AF0)(reinterpret_cast<void*>(ref), nullptr, matrix);
	for (int i = 0; i < 9; ++i) {
		reinterpret_cast<float*>(node + 0x30)[i] = m[i];
	}
	using MarkFn = void(__cdecl*)(UInt32 node, UInt32 flag);
	reinterpret_cast<MarkFn>(0x00897A20)(node, 1);
	using AskFn = UInt32(__fastcall*)(void* self, void* edx);
	if (reinterpret_cast<AskFn>(Read(Read(ref) + 0x164))(reinterpret_cast<void*>(ref), nullptr) == 0) {
		using UpdateFn = void(__fastcall*)(void* self, void* edx, float time, UInt32 flags);
		reinterpret_cast<UpdateFn>(0x00707370)(reinterpret_cast<void*>(node), nullptr, 0.0f, 0);
	}
}

// The engine's PlaySound (its handler 0x00509490, objdump 2026-11-13): the
// audio manager at *(*0x00B33398 + 0x24); thiscall 0x006AE0A0(audio,
// sound form ID, 0x101, 0; ret 0Ch) makes the sound, thiscall
// 0x006B7190(sound, 0; ret 4) and 0x006B73E0(sound) start it, cdecl
// 0x00401F20(sound) lets go of the handle. False: no audio, or no sound.
inline bool DirectPlaySound(UInt32 soundFormId) {
	const UInt32 holder = Read(0x00B33398);
	const UInt32 audio = LooksLikeObject(holder) ? Read(holder + 0x24) : 0;
	if (!LooksLikeObject(audio)) {
		return false;
	}
	using MakeFn = void*(__fastcall*)(void* self, void* edx, UInt32 formId, UInt32 flags, UInt32 unk);
	void* const sound = reinterpret_cast<MakeFn>(0x006AE0A0)(reinterpret_cast<void*>(audio), nullptr, soundFormId,
	                                                          0x101, 0);
	if (sound == nullptr) {
		return false;
	}
	using ArgFn = void(__fastcall*)(void* self, void* edx, UInt32 arg);
	reinterpret_cast<ArgFn>(0x006B7190)(sound, nullptr, 0);
	using PlayFn = void(__fastcall*)(void* self, void* edx);
	reinterpret_cast<PlayFn>(0x006B73E0)(sound, nullptr);
	using FreeFn = void(__cdecl*)(void* sound);
	reinterpret_cast<FreeFn>(0x00401F20)(sound);
	return true;
}

// xOBSE's ModAV2 (Commands_Actor.cpp Cmd_ModActorValue2_Execute): the
// actor's DamageAV(av, amount, 0) - Actor vtable slot 0xAA (GameObjects.h,
// counted from Unk_AD), so +0x2A8; the amount whole, negative harms.
inline void DirectModAV2(UInt32 actor, UInt32 av, int amount) {
	using DamageFn = void(__fastcall*)(void* self, void* edx, UInt32 av, UInt32 amount, UInt32 unk);
	reinterpret_cast<DamageFn>(Read(Read(actor) + 0x2A8))(reinterpret_cast<void*>(actor), nullptr, av,
	                                                       static_cast<UInt32>(amount), 0);
}

// The engine's MoveTo (command 0x109E, its handler 0x00512870, objdump
// 2026-11-14): after reading its arguments the handler only calls cdecl
// 0x00512540(this, target, x, y, z) - the offsets from the target, 0 when
// not given (the handler's fldz). The cell change and the move are that
// call's; a console line "MoveTo" did the same after compiling a script.
inline void DirectMoveTo(UInt32 ref, UInt32 target, float x, float y, float z) {
	using MoveToFn = void(__cdecl*)(UInt32 ref, UInt32 target, float x, float y, float z);
	reinterpret_cast<MoveToFn>(0x00512540)(ref, target, x, y, z);
}

// The engine's ModActorValue (command 0x1010, its handler 0x005017E0,
// objdump 2026-11-14): the reference cast to an Actor (0x009832E6, as for
// SetPos); then, as the byte at 0x00B361AC says, the actor's vtable +0x2A8
// or +0x2A0 with (av, amount, 0). False: not an actor.
inline bool DirectModAV(UInt32 ref, UInt32 av, int amount) {
	using CastFn = void*(__cdecl*)(void* object, UInt32 vfDelta, void* from, void* to, UInt32 isReference);
	void* const actor = reinterpret_cast<CastFn>(0x009832E6)(reinterpret_cast<void*>(ref), 0,
	                                                         reinterpret_cast<void*>(0x00B03054),
	                                                         reinterpret_cast<void*>(0x00B03070), 0);
	if (actor == nullptr) {
		return false;
	}
	const UInt32 slot = Read8(0x00B361AC) != 0 ? 0x2A8u : 0x2A0u;
	using ModFn = void(__fastcall*)(void* self, void* edx, UInt32 av, UInt32 amount, UInt32 unk);
	reinterpret_cast<ModFn>(Read(Read(reinterpret_cast<UInt32>(actor)) + slot))(actor, nullptr, av,
	                                                                              static_cast<UInt32>(amount), 0);
	return true;
}

}  // namespace engine
}  // namespace walkers
