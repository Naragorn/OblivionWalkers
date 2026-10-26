#include "Rise.h"

#include "Engine.h"
#include "MooLogic.h"
#include "SoundLogic.h"
#include "Pose.h"
#include "Replace.h"

#include <windows.h>

#include <cmath>
#include <cstdio>
#include <unordered_set>

namespace walkers {
namespace {

using namespace engine;

// Engine facts of Oblivion 1.2.416, read with REA over Oblivion.exe and in
// xOBSE's GameForms.h / GameObjects.h / GameAPI.cpp on 2026-10-23
// (docs/walkers-spec.md, section 2).
constexpr UInt32 kVtblCharacter = 0x00A6FC9C;   // a person in the world
constexpr UInt32 kVtblCreature = 0x00A710F4;    // a creature in the world
constexpr UInt32 kRefBaseFormOffset = 0x1C;     // TESObjectREFR::baseForm
constexpr UInt32 kRefNiNodeOffset = 0x3C;       // TESObjectREFR::niNode: in the world when set
constexpr UInt32 kActorBaseFlagsOffset = 0x28;  // TESActorBase::actorBaseData.flags
constexpr UInt32 kNpcRacePointerOffset = 0xE8;  // TESNPC::race.race (TESRaceForm at 0xE4)
constexpr UInt32 kRacePlayableOffset = 0x70;    // TESRace::isPlayable, a byte
constexpr UInt32 kRefPositionOffset = 0x2C;     // TESObjectREFR::posX, posY, posZ
constexpr UInt32 kRefParentCellOffset = 0x40;   // TESObjectREFR::parentCell
// TESObjectCELL's ExtraDataList at +0x28, its first BSExtraData at +0x2C;
// a BSExtraData is {vtable, type at +4, next at +8} (xOBSE GameForms.h,
// GameBSExtraData.h). The cell's editor ID (ExtraEditorID, 0x0A: a
// BSStringT, its chars at +0xC) and owner (ExtraOwnership, 0x27: the form
// at +0xC) - what xOBSE's GetEditorID and GetParentCellOwner read.
constexpr UInt32 kCellExtraFirstOffset = 0x2C;
constexpr UInt8 kExtraEditorId = 0x0A;
constexpr UInt8 kExtraOwnership = 0x27;
constexpr UInt32 kActorBaseEssential = 0x2;
constexpr UInt32 kActorBaseNoLowProcessing = 0x200;
constexpr UInt32 kActorVtableIsDead = 0x198;    // as OBVR's ActorRoster reads it: (self, 0) & 1
constexpr UInt32 kActorVtableResurrect = 0x20C; // the console's ResurrectActor: (1, has3D, animate)
// xOBSE's CloneForm, call by call (GameForms.cpp, GameAPI.cpp; the bytes at
// the three addresses read with REA: the type comes off the stack behind the
// SEH frame, the other two take their object in ecx and return with ret 4).
constexpr UInt32 kCreateFormInstance = 0x00447DF0;         // cdecl (UInt8 type)
constexpr UInt32 kAddFormToDataHandler = 0x0044D950;       // thiscall (DataHandler; form)
constexpr UInt32 kAddFormToCreatedBaseList = 0x00459800;   // thiscall (TESSaveLoadGame; form)
constexpr UInt32 kCreatedBaseListPointer = 0x00B33B00;
// TESForm::CopyFrom is virtual 45 of a form, counted from BaseFormComponent's
// four (xOBSE GameForms.h's numbered list; a first reading as 33 skipped the
// base's and sent run 3 of rise-walker-clone into IsOffLimits instead).
constexpr UInt32 kFormVtableCopyFrom = 0xB4;
constexpr UInt8 kFormTypeRace = 0x09;
// The actor process manager's four lists (high to low), as OBVR's roster
// reads them: a node is {data, next}; the list function takes the level.
constexpr UInt32 kActorProcessManager = 0x00B3BD00;
constexpr UInt32 kActorListByLevel = 0x00673A50;
constexpr const char* kPluginEsp = "OblivionWalkers.esp";
constexpr UInt32 kFrameSeconds = 0x00B33E9C;    // the last frame's seconds, a float (OBVR GameAddresses.h)
constexpr UInt32 kTextStart = 0x00401000;
constexpr UInt32 kOpcodeBase = 0x2B00;
constexpr UInt32 kTextEnd = 0x00A27C39;

LogFn g_log = nullptr;
RiseSettings g_settings;
WaitingList g_list;
obse::EventManagerInterface* g_events = nullptr;
obse::TasksInterface* g_tasks = nullptr;
obse::SerializationInterface* g_serialization = nullptr;
obse::ConsoleInterface* g_console = nullptr;
UInt32 g_random = 0;
bool g_deathWatchOn = false;
UInt32 g_registerTries = 0;
RisenWatch g_risen;
bool g_risenWatching = false;
Swap g_swap;
// KeepCorpseApart: the walker whose corpse was moved out of its way, and the
// one whose refusal was logged (once a swap).
UInt32 g_apartWalker = 0;
// The death sound the walker would have made at its unseen kill, drawn
// beforehand and played as it gets up (SoundAtGetUp); 0: none.
UInt32 g_getUpSound = 0;
UInt32 g_apartRefusedFor = 0;
UInt32 g_walkers[4] = {};
UInt32 g_walkerCount = 0;
bool g_walkersLooked = false;
ReplaceSettings g_replace;
char g_iniPath[260] = {};
bool g_spawnsDone = false;
float g_spawnsAgainSeconds = -1.0f;
// NoLoot: the dead walkers noted in the death path, made unusable at the
// next frame tick (nothing of an actor is touched inside the death path).
UInt32 g_lootless[32] = {};
UInt32 g_lootlessCount = 0;
// The walkers risen from a corpse: lootable whatever NoLoot says.
RisenSet g_risenSet;
// BlockMooResurrect: MOO's token and brain at MOO's load index (0 without
// MOO), looked up once a session, and the dead walkers still to strip.
bool g_mooLooked = false;
UInt32 g_mooToken = 0;
UInt32 g_mooBrain = 0;
MooStrip g_mooStrip;
// Fame (4.8): the player's walker kills toward the next point, and the
// points earned in the death path, handed to the engine at the next tick.
FameCount g_fame;
UInt32 g_famePending = 0;
constexpr UInt32 kPlayerRefId = 0x14;

UInt32 NextRandom() {
	g_random ^= g_random << 13;
	g_random ^= g_random >> 17;
	g_random ^= g_random << 5;
	return g_random;
}

bool IsActor(UInt32 ref) {
	const UInt32 vtable = Read(ref);
	return vtable == kVtblCharacter || vtable == kVtblCreature;
}

// The editor ID and the owner of the cell `ref` stands in; nullptr and 0
// for what it does not carry.
void CellFacts(UInt32 ref, const char** editorId, UInt32* ownerId) {
	*editorId = nullptr;
	*ownerId = 0;
	const UInt32 cell = Read(ref + kRefParentCellOffset);
	if (!LooksLikeObject(cell)) {
		return;
	}
	UInt32 visited = 0;
	for (UInt32 extra = Read(cell + kCellExtraFirstOffset); LooksLikeObject(extra) && visited < 64;
	     extra = Read(extra + 8), ++visited) {
		const UInt8 type = Read8(extra + 4);
		if (type == kExtraEditorId && LooksLikeObject(Read(extra + 0xC))) {
			*editorId = reinterpret_cast<const char*>(Read(extra + 0xC));
		} else if (type == kExtraOwnership) {
			*ownerId = FormIdOf(Read(extra + 0xC));
		}
	}
}

bool IsDead(UInt32 actor) {
	const UInt32 fn = Read(Read(actor) + kActorVtableIsDead);
	if (fn < kTextStart || fn >= kTextEnd) {
		return false;
	}
	using DeadFn = UInt8(__fastcall*)(void* self, void* edx, UInt32 arg);
	return (reinterpret_cast<DeadFn>(fn)(reinterpret_cast<void*>(actor), nullptr, 0) & 1) != 0;
}

// xOBSE's OnDeath, inside the engine's death path: only noted, nothing of
// the actor touched in this frame (Zombie Walkers' crash was there).
void __cdecl OnDeath(void* arg0, void* arg1, void* thisObj) {
	const UInt32 ref = reinterpret_cast<UInt32>(arg0);
	Fallen fallen;
	UInt8 type = 0;
	UInt32 flags = 0;
	UInt32 raceId = 0;
	const char* cellId = nullptr;
	UInt32 cellOwner = 0;
	if (LooksLikeObject(ref)) {
		CellFacts(ref, &cellId, &cellOwner);
		fallen.inChapel = IsChapelCell(cellId, cellOwner);
		fallen.refId = Read(ref + kFormIdOffset);
		fallen.character = Read(ref) == kVtblCharacter;
		const UInt32 base = Read(ref + kRefBaseFormOffset);
		if (LooksLikeObject(base)) {
			fallen.baseId = Read(base + kFormIdOffset);
			type = Read8(base + kFormTypeOffset);
			if (IsActor(ref)) {
				flags = Read(base + kActorBaseFlagsOffset);
				fallen.essential = (flags & kActorBaseEssential) != 0;
				fallen.generic = (flags & kActorBaseNoLowProcessing) != 0;
			}
			if (fallen.character && type == kFormTypeNpc) {
				const UInt32 race = Read(base + kNpcRacePointerOffset);
				if (LooksLikeObject(race)) {
					raceId = Read(race + kFormIdOffset);
					fallen.racePlayable = Read8(race + kRacePlayableOffset) != 0;
					fallen.raceHuman = IsHumanRace(raceId);
				}
			}
		}
	}
	bool walker = false;
	for (UInt32 w = 0; type == kFormTypeCreature && w < g_walkerCount; ++w) {
		walker = walker || g_walkers[w] == fallen.baseId;
	}
	if (BlocksLoot(g_settings.noLoot, walker, IsRisen(g_risenSet, fallen.refId)) && g_lootlessCount < 32) {
		g_lootless[g_lootlessCount++] = fallen.refId;
	}
	if (StripsMoo(g_settings.blockMooResurrect, g_mooToken != 0, walker) && !NoteMooStrip(g_mooStrip, fallen.refId)) {
		g_log("MOO: walker %08X dead, but %u wait to be stripped already - MOO may raise this one", fallen.refId,
		      g_mooStrip.count);
	}
	if (FameKill(walker, FormIdOf(reinterpret_cast<UInt32>(arg1)) == kPlayerRefId) &&
	    g_settings.famePerKills > 0) {
		if (CountWalkerKill(g_fame, g_settings.famePerKills)) {
			++g_famePending;
		}
		g_log("Fame: walker %08X cut down by the player - %u of %u toward the next point%s", fallen.refId,
		      g_fame.kills, g_settings.famePerKills, g_famePending > 0 ? ", a point earned" : "");
	}
	const UInt32 roll = NextRandom() % 100;
	const float unit = static_cast<float>(NextRandom() % 10001) / 10000.0f;
	const DeathVerdict verdict = NoteDeath(g_list, g_settings, fallen, roll, unit);
	if (verdict == DeathVerdict::Waits) {
		g_log("Death: %08X (base %08X, type %02X, flags %08X, race %08X%s%s, killer %08X, this %08X, cell %s owned by "
		      "%08X) %s, %.1f s; %u waiting",
		      fallen.refId, fallen.baseId, type, flags, raceId, fallen.racePlayable ? " playable" : "",
		      fallen.raceHuman ? " human" : "", FormIdOf(reinterpret_cast<UInt32>(arg1)),
		      FormIdOf(reinterpret_cast<UInt32>(thisObj)), cellId != nullptr ? cellId : "-", cellOwner,
		      VerdictText(verdict),
		      g_list.entries[g_list.count - 1].secondsLeft, g_list.count);
	} else {
		g_log("Death: %08X (base %08X, type %02X, flags %08X, race %08X%s%s, killer %08X, this %08X, cell %s owned by "
		      "%08X) %s",
		      fallen.refId, fallen.baseId, type, flags, raceId, fallen.racePlayable ? " playable" : "",
		      fallen.raceHuman ? " human" : "", FormIdOf(reinterpret_cast<UInt32>(arg1)),
		      FormIdOf(reinterpret_cast<UInt32>(thisObj)), cellId != nullptr ? cellId : "-", cellOwner,
		      VerdictText(verdict));
	}
}

Presence PresenceOfNow(UInt32 refId) {
	const UInt32 ref = LookupForm(refId);
	if (!LooksLikeObject(ref) || !IsActor(ref)) {
		return Presence::Gone;
	}
	return Read(ref + kRefNiNodeOffset) != 0 ? Presence::Present : Presence::Absent;
}

// The presence as the waiting clocks see it, with a log line whenever a
// waiting reference changes between present, absent and gone (an outdoor
// run of 2026-10-24 07:22 lost two waiting bodies' 3D and never raised
// them - the line says when and where).
Presence PresenceOf(UInt32 refId) {
	static UInt32 s_ids[kWaitingMost] = {};
	static UInt8 s_last[kWaitingMost] = {};
	static UInt32 s_count = 0;
	const Presence now = PresenceOfNow(refId);
	UInt32 slot = s_count;
	for (UInt32 i = 0; i < s_count; ++i) {
		if (s_ids[i] == refId) {
			slot = i;
			break;
		}
	}
	if (slot == s_count) {
		if (s_count == kWaitingMost) {
			slot = refId % kWaitingMost;
		} else {
			++s_count;
		}
		s_ids[slot] = refId;
		s_last[slot] = 0xFF;
	}
	if (s_last[slot] != static_cast<UInt8>(now)) {
		const UInt32 ref = LookupForm(refId);
		const bool object = LooksLikeObject(ref);
		g_log("Waiting: %08X is now %s at %.0f %.0f %.0f (form flags %08X)", refId,
		      now == Presence::Present ? "present" : (now == Presence::Absent ? "absent (no 3D)" : "gone"),
		      object ? FloatAt(ref + kRefPositionOffset) : 0.0f, object ? FloatAt(ref + kRefPositionOffset + 4) : 0.0f,
		      object ? FloatAt(ref + kRefPositionOffset + 8) : 0.0f, object ? Read(ref + 0x08) : 0u);
		s_last[slot] = static_cast<UInt8>(now);
	}
	return now;
}

// A clone of a person's base record as xOBSE's CloneForm makes one: a new
// instance of the type, CopyFrom the original, into the data handler and the
// created-base list (so the save keeps it, under an FF form ID). 0 when the
// engine refuses or the record is not a person's.
UInt32 CloneBaseRecord(UInt32 base) {
	if (Read8(base + kFormTypeOffset) != kFormTypeNpc) {
		g_log("Clone: base %08X is of type %02X, not a person: not cloned", FormIdOf(base),
		      Read8(base + kFormTypeOffset));
		return 0;
	}
	// A line before every engine call: xOBSE swallows an exception in a task
	// (Task<void>::Run's __try) and the log would otherwise say nothing of
	// which call it was.
	g_log("Clone: base %08X (vtable %08X) - creating an instance", FormIdOf(base), Read(base));
	using CreateFn = UInt32(__cdecl*)(UInt8 type);
	const UInt32 clone = reinterpret_cast<CreateFn>(kCreateFormInstance)(kFormTypeNpc);
	if (!LooksLikeObject(clone)) {
		g_log("Clone: the engine made no instance for base %08X", FormIdOf(base));
		return 0;
	}
	const UInt32 copyFrom = Read(Read(clone) + kFormVtableCopyFrom);
	g_log("Clone: instance %08X (vtable %08X, type %02X, id %08X), CopyFrom at %08X - copying", clone, Read(clone),
	      Read8(clone + kFormTypeOffset), FormIdOf(clone), copyFrom);
	if (copyFrom < kTextStart || copyFrom >= kTextEnd) {
		g_log("Clone: no CopyFrom at vtable+0xB4 for base %08X: the instance is left empty and unused",
		      FormIdOf(base));
		return 0;
	}
	using CopyFn = void(__fastcall*)(void* self, void* edx, UInt32 source);
	reinterpret_cast<CopyFn>(copyFrom)(reinterpret_cast<void*>(clone), nullptr, base);
	g_log("Clone: copied (type %02X, id %08X, flags at +0x28 %08X) - into the data handler %08X", Read8(clone + kFormTypeOffset),
	      FormIdOf(clone), Read(clone + kActorBaseFlagsOffset), Read(kDataHandlerPointer));
	using AddFn = UInt32(__fastcall*)(void* self, void* edx, UInt32 form);
	reinterpret_cast<AddFn>(kAddFormToDataHandler)(reinterpret_cast<void*>(Read(kDataHandlerPointer)), nullptr, clone);
	g_log("Clone: in the data handler (id %08X) - into the created-base list %08X", FormIdOf(clone),
	      Read(kCreatedBaseListPointer));
	reinterpret_cast<AddFn>(kAddFormToCreatedBaseList)(reinterpret_cast<void*>(Read(kCreatedBaseListPointer)), nullptr,
	                                                   clone);
	g_log("Clone: base %08X cloned as %08X", FormIdOf(base), FormIdOf(clone));
	return clone;
}

// OblivionWalkers.esp's runtime index from the data handler's mods-by-ID
// table; 0x100 when the ESP is not loaded.
UInt32 FindEspIndex() {
	const UInt32 handler = Read(kDataHandlerPointer);
	if (!LooksLikeObject(handler)) {
		g_log("ESP: no data handler at %08X", kDataHandlerPointer);
		return 0x100;
	}
	const UInt32 count = Read(handler + kDataHandlerModCountOffset);
	for (UInt32 i = 0; i < count && i < 0xFF; ++i) {
		const char* name = ModNameAt(i);
		if (name != nullptr && SameModName(name, kPluginEsp)) {
			return i;
		}
	}
	g_log("ESP: %s is not among the %u loaded mods", kPluginEsp, count);
	return 0x100;
}

// The ESP's Walker race at its local ID 0x800. 0 when the ESP is not loaded
// or the record there is no race; the log says which.
UInt32 FindZombieRace() {
	const UInt32 index = FindEspIndex();
	if (index > 0xFF) {
		g_log("Race: no ESP, the clone keeps his race");
		return 0;
	}
	const UInt32 formId = ZombieRaceFormId(index);
	const UInt32 race = LookupForm(formId);
	if (!LooksLikeObject(race) || Read8(race + kFormTypeOffset) != kFormTypeRace) {
		g_log("Race: %s is mod %02X, but %08X is no race (%s): the clone keeps his race", kPluginEsp, index,
		      formId, LooksLikeObject(race) ? "another type" : "no form");
		return 0;
	}
	g_log("Race: %s is mod %02X, the Walker race %08X found", kPluginEsp, index, formId);
	return race;
}

// The ESP's Walker creatures (local IDs 0x801..0x804), looked up once per
// session; g_walkerCount stays 0 without the ESP.
void FindWalkers() {
	if (g_walkersLooked) {
		return;
	}
	g_walkersLooked = true;
	g_walkerCount = 0;
	const UInt32 index = FindEspIndex();
	if (index > 0xFF) {
		g_log("Walkers: no ESP, the fallen get up as themselves");
		return;
	}
	for (UInt32 local : kWalkerLocalIds) {
		const UInt32 formId = EspFormId(index, local);
		const UInt32 form = LookupForm(formId);
		if (LooksLikeObject(form) && Read8(form + kFormTypeOffset) == kFormTypeCreature) {
			g_walkers[g_walkerCount++] = formId;
		} else {
			g_log("Walkers: %08X is no creature (%s)", formId, LooksLikeObject(form) ? "another type" : "no form");
		}
	}
	g_log("Walkers: %s is mod %02X, %u walker creatures found", kPluginEsp, index, g_walkerCount);
}

// Maskar's Oblivion Overhaul's zombie token and brain, once a session: the
// forms at MOO's load index, each of the type its ESP gives it - or none.
void FindMoo() {
	if (g_mooLooked) {
		return;
	}
	g_mooLooked = true;
	g_mooToken = 0;
	g_mooBrain = 0;
	const UInt32 handler = Read(kDataHandlerPointer);
	const UInt32 count = LooksLikeObject(handler) ? Read(handler + kDataHandlerModCountOffset) : 0;
	UInt32 index = 0x100;
	for (UInt32 i = 0; i < count && i < 0xFF; ++i) {
		const char* name = ModNameAt(i);
		if (name != nullptr && SameModName(name, kMooEsp)) {
			index = i;
			break;
		}
	}
	if (index > 0xFF) {
		g_log("MOO: %s is not loaded - nothing to keep off the walkers", kMooEsp);
		return;
	}
	const UInt32 token = LookupForm(MooFormId(index, kMooTokenZombieLocalId));
	const UInt32 brain = LookupForm(MooFormId(index, kMooZombieBrainLocalId));
	const UInt32 tokenType = LooksLikeObject(token) ? Read8(token + kFormTypeOffset) : 0;
	const UInt32 brainType = LooksLikeObject(brain) ? Read8(brain + kFormTypeOffset) : 0;
	if (tokenType != kFormTypeClothing || brainType != kFormTypeIngredient) {
		g_log("MOO: %s is mod %02X, but its zombie token %08X (type %02X) and brain %08X (type %02X) are not what "
		      "this plugin read in MOO's ESP - a dead walker keeps them",
		      kMooEsp, index, MooFormId(index, kMooTokenZombieLocalId), tokenType,
		      MooFormId(index, kMooZombieBrainLocalId), brainType);
		return;
	}
	g_mooToken = MooFormId(index, kMooTokenZombieLocalId);
	g_mooBrain = MooFormId(index, kMooZombieBrainLocalId);
	g_log("MOO: %s is mod %02X - a dead walker loses its token %08X and brain %08X (BlockMooResurrect=%d)", kMooEsp,
	      index, g_mooToken, g_mooBrain, g_settings.blockMooResurrect ? 1 : 0);
}

// Every Creature reference of `base` on the process lists, with or without
// a model, wherever it stands; returns how many were written to `out`.
UInt32 CollectWalkerRefs(UInt32 base, UInt32* out, UInt32 most) {
	auto* const manager = reinterpret_cast<void*>(kActorProcessManager);
	using ListFn = void*(__fastcall*)(void* self, void* edx, UInt32 level);
	struct Node {
		void* data;
		Node* next;
	};
	UInt32 count = 0;
	for (UInt32 level = 0; level < 4 && count < most; ++level) {
		auto* node = static_cast<Node*>(reinterpret_cast<ListFn>(kActorListByLevel)(manager, nullptr, level));
		for (UInt32 visited = 0; node != nullptr && visited < 1024 && count < most; ++visited) {
			if (!LooksLikeObject(reinterpret_cast<UInt32>(node))) {
				break;
			}
			const UInt32 actor = reinterpret_cast<UInt32>(node->data);
			node = node->next;
			if (!LooksLikeObject(actor) || Read(actor) != kVtblCreature) {
				continue;
			}
			const UInt32 actorBase = Read(actor + kRefBaseFormOffset);
			if (!LooksLikeObject(actorBase) || Read(actorBase + kFormIdOffset) != base) {
				continue;
			}
			const UInt32 refId = Read(actor + kFormIdOffset);
			bool seen = false;
			for (UInt32 i = 0; i < count; ++i) {
				seen = seen || out[i] == refId;
			}
			if (!seen) {
				out[count++] = refId;
			}
		}
	}
	return count;
}

// The walker just placed on the corpse: the first reference of its base
// that was not there before the PlaceAtMe. 0 while none exists.
UInt32 FindNewWalker(const Swap& swap) {
	UInt32 refs[kSwapKnownMost + 4];
	const UInt32 count = CollectWalkerRefs(swap.walkerBase, refs, kSwapKnownMost + 4);
	return NewWalkerAmong(swap, refs, count);
}

// A console line run by xOBSE with the reference as "this" (the proof of
// 2026-10-23, rise-creature-proof.txt: PlaceAtMe, Disable, Kill,
// RemoveAllItems all vanilla).
bool ConsoleOn(UInt32 ref, const char* line) {
	if (g_console == nullptr) {
		return false;
	}
	return g_console->RunScriptLine2(line, reinterpret_cast<void*>(ref), true);
}

// The engine's own raise of the person himself, as the console's "resurrect
// 1" does it: Actor's virtual at vtable+0x20C with (1, has3D, animate=1) -
// the get-up animation, the inventory kept. With CloneBase the reference
// points at a clone of its base first, so the engine rebuilds him from the
// record that will carry the zombie race (4.1 step 3) - and with ZombieRace
// the clone's race is the ESP's Walker race before the raise.
void RaiseAsSelf(UInt32 refId) {
	const UInt32 ref = LookupForm(refId);
	if (!LooksLikeObject(ref) || !IsActor(ref)) {
		g_log("Rise: %08X is gone, nobody gets up", refId);
		return;
	}
	if (!IsDead(ref)) {
		g_log("Rise: %08X is alive already", refId);
		return;
	}
	const UInt32 fn = Read(Read(ref) + kActorVtableResurrect);
	if (fn < kTextStart || fn >= kTextEnd) {
		g_log("Rise: %08X has no Resurrect at vtable+0x20C (%08X), left down", refId, fn);
		return;
	}
	bool lookWanted = false;
	if (g_settings.cloneBase) {
		const UInt32 base = Read(ref + kRefBaseFormOffset);
		const UInt32 clone = LooksLikeObject(base) ? CloneBaseRecord(base) : 0;
		if (clone != 0) {
			*reinterpret_cast<UInt32*>(ref + kRefBaseFormOffset) = clone;
			g_log("Rise: %08X now stands on base %08X (was %08X)", refId, FormIdOf(clone), FormIdOf(base));
			if (g_settings.zombieRace) {
				const UInt32 race = FindZombieRace();
				if (race != 0) {
					const UInt32 before = Read(clone + kNpcRacePointerOffset);
					*reinterpret_cast<UInt32*>(clone + kNpcRacePointerOffset) = race;
					g_log("Rise: the clone's race set to %08X (was %08X)", FormIdOf(race), FormIdOf(before));
					lookWanted = true;
				}
			}
		}
	}
	const UInt32 has3D = Read(ref + kRefNiNodeOffset) != 0 ? 1u : 0u;
	using ResurrectFn = void(__fastcall*)(void* self, void* edx, UInt32 one, UInt32 has3D, UInt32 animate);
	reinterpret_cast<ResurrectFn>(fn)(reinterpret_cast<void*>(ref), nullptr, 1, has3D, 1);
	g_risen = RisenWatch{};
	g_risen.refId = refId;
	g_risen.startMs = GetTickCount();
	g_risen.x = FloatAt(ref + kRefPositionOffset);
	g_risen.y = FloatAt(ref + kRefPositionOffset + 4);
	g_risen.z = FloatAt(ref + kRefPositionOffset + 8);
	g_risen.lookWanted = lookWanted;
	g_risenWatching = true;
	g_log("Rise: %08X (base %08X) gets up at %.0f %.0f %.0f, still dead to the engine: %d; %u waiting", refId,
	      FormIdOf(Read(ref + kRefBaseFormOffset)), g_risen.x, g_risen.y, g_risen.z, IsDead(ref) ? 1 : 0,
	      g_list.count);
}

// A due rise: as a walker (the creature path, 4.1) when the ESP's walkers
// are there and the console interface too - the walker is asked for on the
// corpse and the swap in flight takes it from there (StepSwapInFlight);
// xOBSE's answer to the line is no success flag (the harness read 0 for
// every placeatme that placed), the walker's appearance within
// kSwapFindSeconds is. Otherwise the person himself gets up.
void Raise(UInt32 refId) {
	const UInt32 ref = LookupForm(refId);
	if (!LooksLikeObject(ref) || !IsActor(ref)) {
		g_log("Rise: %08X is gone, nobody gets up", refId);
		return;
	}
	if (!IsDead(ref)) {
		g_log("Rise: %08X is alive already", refId);
		return;
	}
	if (g_settings.asCreature) {
		FindWalkers();
		const UInt32 walker = PickWalker(g_walkers, g_walkerCount, NextRandom());
		if (walker != 0 && g_console != nullptr && !g_swap.active) {
			char line[64];
			std::snprintf(line, sizeof(line), "PlaceAtMe %08X 1 0 0", walker);
			g_swap = Swap{};
			UInt32 before[kSwapKnownMost];
			NoteKnownWalkers(g_swap, before, CollectWalkerRefs(walker, before, kSwapKnownMost));
			const bool answer = ConsoleOn(ref, line);
			g_swap.active = true;
			g_swap.corpseId = refId;
			g_swap.walkerBase = walker;
			g_swap.x = FloatAt(ref + kRefPositionOffset);
			g_swap.y = FloatAt(ref + kRefPositionOffset + 4);
			g_swap.z = FloatAt(ref + kRefPositionOffset + 8);
			g_log("Rise: %08X (base %08X) - walker %08X asked for at %.0f %.0f %.0f (xOBSE answered %d, %u of that "
			      "base known before); %u waiting",
			      refId, FormIdOf(Read(ref + kRefBaseFormOffset)), walker, g_swap.x, g_swap.y, g_swap.z,
			      answer ? 1 : 0, g_swap.knownCount, g_list.count);
			return;
		}
		if (walker != 0 && g_swap.active) {
			if (Requeue(g_list, refId, FormIdOf(Read(ref + kRefBaseFormOffset)), 2.0f)) {
				g_log("Rise: a swap is still in flight, %08X waits 2 s more", refId);
				return;
			}
			g_log("Rise: a swap is still in flight and the list is full, %08X gets up as himself", refId);
		} else if (walker != 0) {
			g_log("Rise: no console interface for the walker, %08X gets up as himself", refId);
		}
	}
	RaiseAsSelf(refId);
}

// The get-up, measured: the engine's dead flag and the first step off the
// spot, in milliseconds after the Resurrect.
void StepRisenWatch() {
	const UInt32 ref = LookupForm(g_risen.refId);
	if (!LooksLikeObject(ref) || !IsActor(ref)) {
		g_log("Risen: %08X is gone, the watch ends", g_risen.refId);
		g_risenWatching = false;
		return;
	}
	const UInt32 now = GetTickCount();
	const float lookSeconds = g_settings.lookAfterSeconds < 0.0f ? 0.0f : g_settings.lookAfterSeconds;
	const UInt32 events = walkers::StepRisenWatch(g_risen, IsDead(ref), FloatAt(ref + kRefPositionOffset),
	                                              FloatAt(ref + kRefPositionOffset + 4),
	                                              FloatAt(ref + kRefPositionOffset + 8), now,
	                                              static_cast<UInt32>(lookSeconds * 1000.0f));
	if (events & kRisenAlive) {
		g_log("Risen: %08X counted alive %u ms after the Resurrect", g_risen.refId, now - g_risen.startMs);
	}
	if (events & kRisenMoved) {
		g_log("Risen: %08X first off the spot %u ms after the Resurrect", g_risen.refId, now - g_risen.startMs);
	}
	if (events & kRisenLook) {
		// xOBSE's Update3D on the reference: Set3D(0), then the model
		// loader's queue builds him anew from his base - the clone with the
		// Walker race (rise-zombie-race.txt run 2: the skin followed).
		if (g_console == nullptr) {
			g_log("Risen: %08X keeps his old look - no console interface for Update3D", g_risen.refId);
		} else {
			const bool answer = g_console->RunScriptLine2("Update3D", reinterpret_cast<void*>(ref), true);
			g_log("Risen: %08X built anew from his base (Update3D handed to xOBSE, answer %d) %u ms after the Resurrect",
			      g_risen.refId, answer ? 1 : 0, now - g_risen.startMs);
		}
	}
	if (events & kRisenDone) {
		g_log("Risen: the watch on %08X ends after %u ms (alive seen %d, moved %d)", g_risen.refId,
		      now - g_risen.startMs, g_risen.aliveSeen ? 1 : 0, g_risen.moveSeen ? 1 : 0);
		g_risenWatching = false;
	}
}

// xOBSE's RegisterEvent says no while its event manager holds no event
// infos yet - at the plugins' load it does not (the run of 2026-10-23 17:41:
// "refused the OnDeath handler"), so the frame tick asks again until it takes.
bool TryRegisterDeath() {
	++g_registerTries;
	if (!g_events->RegisterEvent("OnDeath", &OnDeath, nullptr, nullptr, nullptr)) {
		if (g_registerTries == 1) {
			g_log("xOBSE refused the OnDeath handler at load (its event manager is not up yet): asked again "
			      "every frame");
		}
		return false;
	}
	g_deathWatchOn = true;
	g_log("Death watch on (the OnDeath handler taken at try %u)", g_registerTries);
	return true;
}

// The two change places in one frame: the corpse hidden, the walker shown
// lying where it was laid, the man's items on it.
unsigned long long QpcNow();

void RevealSwap() {
	const UInt32 corpse = LookupForm(g_swap.corpseId);
	const UInt32 zombie = LookupForm(g_swap.zombieId);
	Vec3 corpsePelvis{};
	Vec3 corpseHead{};
	if (!g_settings.hideCorpseAtPose && PelvisAndHead(corpse, &corpsePelvis, &corpseHead)) {
		// Where the corpse lies as it goes: against the pose's reading, whether
		// the walker pushed it - or, kept apart, whether it sank.
		g_log("Pose: the corpse at the reveal - pelvis at %.0f %.0f %.0f, head at %.0f %.0f %.0f", corpsePelvis.x,
		      corpsePelvis.y, corpsePelvis.z, corpseHead.x, corpseHead.y, corpseHead.z);
	}
	const bool hidden = g_settings.hideCorpseAtPose || (LooksLikeObject(corpse) && ConsoleOn(corpse, "Disable"));
	const bool shown = LooksLikeObject(zombie) && ConsoleOn(zombie, "SetActorAlpha 1");
	bool moved = false;
	if (LooksLikeObject(corpse) && LooksLikeObject(zombie) && g_swap.corpseId != g_swap.zombieId) {
		char line[64];
		std::snprintf(line, sizeof(line), "RemoveAllItems %08X", g_swap.zombieId);
		moved = ConsoleOn(corpse, line);
	}
	Vec3 pelvis{};
	Vec3 head{};
	if (PelvisAndHead(zombie, &pelvis, &head)) {
		g_log("Pose: shown - the walker's pelvis at %.0f %.0f %.0f, head at %.0f %.0f %.0f", pelvis.x, pelvis.y,
		      pelvis.z, head.x, head.y, head.z);
	}
	g_log("Swap: walker %08X lies at %.0f %.0f %.0f in the place of %08X - the corpse hidden (%d), the walker "
	      "shown (%d), the items moved over (%d) [tick %llu]",
	      g_swap.zombieId, LooksLikeObject(zombie) ? FloatAt(zombie + kRefPositionOffset) : 0.0f,
	      LooksLikeObject(zombie) ? FloatAt(zombie + kRefPositionOffset + 4) : 0.0f,
	      LooksLikeObject(zombie) ? FloatAt(zombie + kRefPositionOffset + 8) : 0.0f, g_swap.corpseId,
	      hidden ? 1 : 0, shown ? 1 : 0, moved ? 1 : 0, QpcNow());
}

// The QueryPerformanceCounter now, for the swap's log lines: the harness's
// marks carry the same clock ("tick"), so a recording of the game's sound
// lines up with the swap (Wine's QPC is CLOCK_MONOTONIC in 100 ns units).
unsigned long long QpcNow() {
	LARGE_INTEGER now;
	QueryPerformanceCounter(&now);
	return static_cast<unsigned long long>(now.QuadPart);
}

// The sound lists the walker's death sound is drawn from (SoundLogic.h); 0
// when its base is no creature or the engine's GetSound is not the game's own.
constexpr UInt32 kCreatureGetSound = 0x0051CEC0;  // thiscall(creature, type), ret 4
constexpr UInt8 kCreatureGetSoundBytes[8] = {0x8B, 0x41, 0x28, 0xC1, 0xE8, 0x08, 0xA8, 0x01};

bool GetSoundIsTheGames() {
	return std::memcmp(reinterpret_cast<const void*>(kCreatureGetSound), kCreatureGetSoundBytes,
	                   sizeof(kCreatureGetSoundBytes)) == 0;
}

UInt32 DeathSoundListsOf(UInt32 actor) {
	const UInt32 base = LooksLikeObject(actor) ? Read(actor + kRefBaseFormOffset) : 0;
	if (!LooksLikeObject(base) || Read8(base + kFormTypeOffset) != kFormTypeCreature || !GetSoundIsTheGames()) {
		return 0;
	}
	const UInt32 lists = SoundListsOf(base, [](UInt32 address) { return LooksLikeObject(address & ~3u) ? Read(address) : 0u; });
	return LooksLikeObject(lists) ? lists : 0;
}

// The sound of `type` the engine would draw for the actor now - its own
// GetSound, the dice included; 0 when it draws none or it is no SOUN.
UInt32 DrawnSoundId(UInt32 actor, UInt32 type) {
	const UInt32 base = Read(actor + kRefBaseFormOffset);
	using GetSoundFn = UInt32(__fastcall*)(void* creature, void* edx, UInt32 type);
	const UInt32 sound = reinterpret_cast<GetSoundFn>(kCreatureGetSound)(reinterpret_cast<void*>(base), nullptr, type);
	return LooksLikeObject(sound) && Read8(sound + kFormTypeOffset) == kFormTypeSound ? FormIdOf(sound) : 0;
}

// The swap in flight, one frame: the walker's reference is made unseen and
// killed the moment its model exists (before its AI takes a step); its
// ragdoll settles unseen where the man lies, DropSeconds later the two
// change places - the corpse hidden, the walker shown, the items over - and
// LieSeconds after that the walker gets up from there.
void StepSwapInFlight(float dt) {
	UInt32 found = 0;
	bool has3D = false;
	if (g_swap.stage == SwapStage::Placed) {
		found = FindNewWalker(g_swap);
	} else if (g_swap.stage == SwapStage::Loading) {
		const UInt32 zombie = LookupForm(g_swap.zombieId);
		has3D = LooksLikeObject(zombie) && Read(zombie + kRefNiNodeOffset) != 0;
	}
	const UInt32 events =
		StepSwap(g_swap, dt, found, has3D, g_settings.dropSeconds < 0.0f ? 0.0f : g_settings.dropSeconds,
		         MatchCorpseFrom(g_settings.matchCorpse) == MatchCorpse::Pose && g_settings.settleSeconds > 0.0f
		             ? g_settings.settleSeconds
		             : 0.0f,
		         g_settings.lieSeconds < 0.0f ? 0.0f : g_settings.lieSeconds);
	// Noted before its kill below: NoLoot never takes a risen walker.
	NoteRisen(g_risenSet, g_swap.zombieId);
	// The corpse out of the walker's way: before the walker is dropped onto
	// it below, and each frame after until it takes.
	if (ApartDue(g_settings.keepCorpseApart, g_swap.stage, g_apartWalker == g_swap.zombieId)) {
		const ApartReport apart = KeepCorpseApart(LookupForm(g_swap.zombieId), LookupForm(g_swap.corpseId));
		if (apart.done) {
			g_apartWalker = g_swap.zombieId;
			g_log("Apart: the corpse %08X's %u ragdoll bodies moved into walker %08X's collision group %04X, part 0 - "
			      "the two cannot meet (%u walker bodies read, %.0f us)",
			      g_swap.corpseId, apart.corpseBodies, g_swap.zombieId, apart.group, apart.walkerBodies,
			      apart.microseconds);
		} else if (g_apartRefusedFor != g_swap.zombieId) {
			g_apartRefusedFor = g_swap.zombieId;
			g_log("Apart: not yet for walker %08X - %s (asked again each frame until the two change places)",
			      g_swap.zombieId, apart.refused);
		}
	}
	if (events == kSwapFound) {
		const UInt32 zombie = LookupForm(g_swap.zombieId);
		g_log("Swap: walker %08X exists on %08X (model %s) - waiting for its model before the kill [tick %llu]",
		      g_swap.zombieId, g_swap.corpseId,
		      LooksLikeObject(zombie) && Read(zombie + kRefNiNodeOffset) != 0 ? "there" : "loading", QpcNow());
	} else if (events == kSwapDropUnseen) {
		// Killed in the frame its model appears: a kill before that left a
		// "dead" walker sliding at walking speed (the trace of 2026-10-24
		// 07:10, 400 units/s in a line), one after a few frames lets it take
		// a step. Unseen first (SetActorAlpha 0, the CS wiki's "0.0 meaning
		// fully transparent"): its fall onto the spot is nobody's to see.
		const UInt32 zombie = LookupForm(g_swap.zombieId);
		const bool unseen = LooksLikeObject(zombie) && ConsoleOn(zombie, "SetActorAlpha 0");
		// Outdoors the engine places the walker away from the corpse (the
		// run of 2026-10-24 07:25: 670 units off on a slope; the hall's flat
		// floor keeps it on the spot): onto the corpse first, then the kill.
		char moveLine[64];
		std::snprintf(moveLine, sizeof(moveLine), "MoveTo %08X", g_swap.corpseId);
		const float bx = LooksLikeObject(zombie) ? FloatAt(zombie + kRefPositionOffset) : 0.0f;
		const float by = LooksLikeObject(zombie) ? FloatAt(zombie + kRefPositionOffset + 4) : 0.0f;
		const bool movedOnto = LooksLikeObject(zombie) && ConsoleOn(zombie, moveLine);
		g_log("Swap: walker %08X placed at %.0f %.0f, the corpse at %.0f %.0f - unseen (%d), moved onto it (%d)",
		      g_swap.zombieId, bx, by, g_swap.x, g_swap.y, unseen ? 1 : 0, movedOnto ? 1 : 0);
		// Its death unheard (SoundLogic.h): the sound it would make drawn now
		// for its get-up, its death list emptied for this one Kill and put
		// back - the engine's death plays a creature's sound inside the call.
		const UInt32 lists = DeathSoundListsOf(zombie);
		const UInt32 slot = g_settings.holdDeathSound ? DeathListSlot(lists) : 0;
		g_getUpSound = slot != 0 ? DrawnSoundId(zombie, kCreatureSoundDeath) : 0;
		const UInt32 heldList = slot != 0 ? Read(slot) : 0;
		if (slot != 0) {
			*reinterpret_cast<UInt32*>(slot) = 0;
		}
		const bool killed = LooksLikeObject(zombie) && ConsoleOn(zombie, "Kill");
		if (slot != 0) {
			*reinterpret_cast<UInt32*>(slot) = heldList;
		}
		g_log("Swap: walker %08X dies unheard - %s; its sound for the get-up: %08X [tick %llu]", g_swap.zombieId,
		      slot != 0                       ? "its death list held back for the kill"
		      : !g_settings.holdDeathSound    ? "the death sound left to the engine (HoldDeathSound=0)"
		                                      : "no sound lists found, so none held back",
		      g_getUpSound, QpcNow());
		g_log("Swap: walker %08X's model is there after %.1f s at %.0f %.0f %.0f - killed (%d), dead now %d, the "
		      "corpse %08X still lying",
		      g_swap.zombieId, g_swap.seconds, LooksLikeObject(zombie) ? FloatAt(zombie + kRefPositionOffset) : 0.0f,
		      LooksLikeObject(zombie) ? FloatAt(zombie + kRefPositionOffset + 4) : 0.0f,
		      LooksLikeObject(zombie) ? FloatAt(zombie + kRefPositionOffset + 8) : 0.0f, killed ? 1 : 0,
		      LooksLikeObject(zombie) && IsActor(zombie) && IsDead(zombie) ? 1 : 0, g_swap.corpseId);
	} else if (events == kSwapPose || events == kSwapPlace) {
		// Laid into the corpse's place (MatchCorpse=1) or pose (2) while still
		// unseen. With 1 the reveal waits a frame for the nodes to follow the
		// bodies. With 2 the pose settles SettleSeconds, then the settled body
		// is moved and turned onto the corpse's place and shown at once
		// (kSwapPlace).
		static bool s_logged = false;
		// The corpse's spine and head at the pose: the settled walker's later
		// placement goes there, whether the corpse still lies or not.
		static Vec3 s_pivot{};
		static Vec3 s_head{};
		static bool s_target = false;
		const MatchCorpse setting = MatchCorpseFrom(g_settings.matchCorpse);
		const MatchCorpse mode = events == kSwapPlace ? MatchCorpse::Place : setting;
		const PoseReport pose = MatchWalkerToCorpse(LookupForm(g_swap.zombieId), LookupForm(g_swap.corpseId), mode,
		                                            !s_logged, g_log, events == kSwapPlace && s_target ? &s_pivot : nullptr,
		                                            events == kSwapPlace && s_target ? &s_head : nullptr);
		s_logged = s_logged || pose.bodies > 0;
		if (events == kSwapPose) {
			s_target = pose.corpseRead;
			s_pivot = pose.corpsePivot;
			s_head = pose.corpseHead;
			if (g_settings.hideCorpseAtPose) {
				// The test switch: the corpse gone in the frame the walker is laid,
				// before the next physics step - the two cannot collide.
				const UInt32 corpse = LookupForm(g_swap.corpseId);
				const bool hidden = LooksLikeObject(corpse) && ConsoleOn(corpse, "Disable");
				g_log("Swap: the corpse %08X hidden right after the walker was laid (HideCorpseAtPose, %d)",
				      g_swap.corpseId, hidden ? 1 : 0);
			}
		}
		g_log("Swap: walker %08X %s %08X (MatchCorpse=%u) - %u bodies: %u turned as the corpse's bones, %u carried "
		      "along, %u moved and turned whole%s, %u left; %.0f us [tick %llu]",
		      g_swap.zombieId,
		      events == kSwapPlace ? "settled, now moved onto the place of"
		                           : (mode == MatchCorpse::Pose ? "laid into the pose of" : "laid into the place of"),
		      g_swap.corpseId, static_cast<UInt32>(g_settings.matchCorpse), pose.bodies, pose.copied, pose.carried,
		      pose.placed, pose.planOk ? "" : " (no pelvis to go by)", pose.left, pose.microseconds, QpcNow());
		if (events == kSwapPlace || mode != MatchCorpse::Pose) {
			Vec3 pelvis{};
			Vec3 head{};
			if (PelvisAndHead(LookupForm(g_swap.zombieId), &pelvis, &head)) {
				g_log("Pose: before the move - the walker's pelvis at %.0f %.0f %.0f, head at %.0f %.0f %.0f", pelvis.x,
				      pelvis.y, pelvis.z, head.x, head.y, head.z);
			}
		}
		if (events == kSwapPlace) {
			// The settled walker placed: the two change places in this frame,
			// before the next physics step - the corpse lying on would push it.
			RevealSwap();
		}
	} else if (events == kSwapReveal) {
		RevealSwap();
	} else if (events == kSwapGaveUp) {
		g_log("Swap: no walker %08X appeared on %08X within %.0f s - he gets up as himself", g_swap.walkerBase,
		      g_swap.corpseId, kSwapFindSeconds);
		RaiseAsSelf(g_swap.corpseId);
	} else if (events == kSwapAbandon) {
		const UInt32 zombie = LookupForm(g_swap.zombieId);
		const bool hidden = LooksLikeObject(zombie) && ConsoleOn(zombie, "Disable");
		g_log("Swap: walker %08X's model never came within %.0f s - the walker hidden (%d), %08X gets up as himself",
		      g_swap.zombieId, kSwapLoadSeconds, hidden ? 1 : 0, g_swap.corpseId);
		RaiseAsSelf(g_swap.corpseId);
	} else if (events == kSwapRaise) {
		const UInt32 zombie = LookupForm(g_swap.zombieId);
		if (!LooksLikeObject(zombie) || !IsActor(zombie)) {
			g_log("Swap: walker %08X is gone before the raise", g_swap.zombieId);
			return;
		}
		if (!IsDead(zombie)) {
			g_log("Swap: walker %08X is alive at the raise, at %.0f %.0f %.0f - left as it is, no raise", g_swap.zombieId,
			      FloatAt(zombie + kRefPositionOffset), FloatAt(zombie + kRefPositionOffset + 4),
			      FloatAt(zombie + kRefPositionOffset + 8));
			return;
		}
		const UInt32 fn = Read(Read(zombie) + kActorVtableResurrect);
		if (fn < kTextStart || fn >= kTextEnd) {
			g_log("Swap: walker %08X has no Resurrect at vtable+0x20C, left down", g_swap.zombieId);
			return;
		}
		Vec3 pelvis{};
		Vec3 head{};
		if (PelvisAndHead(zombie, &pelvis, &head)) {
			g_log("Pose: at the raise - the walker's pelvis at %.0f %.0f %.0f, head at %.0f %.0f %.0f", pelvis.x,
			      pelvis.y, pelvis.z, head.x, head.y, head.z);
		}
		const UInt32 has3D = Read(zombie + kRefNiNodeOffset) != 0 ? 1u : 0u;
		using ResurrectFn = void(__fastcall*)(void* self, void* edx, UInt32 one, UInt32 has3D, UInt32 animate);
		reinterpret_cast<ResurrectFn>(fn)(reinterpret_cast<void*>(zombie), nullptr, 1, has3D, 1);
		g_risen = RisenWatch{};
		g_risen.refId = g_swap.zombieId;
		g_risen.startMs = GetTickCount();
		g_risen.x = FloatAt(zombie + kRefPositionOffset);
		g_risen.y = FloatAt(zombie + kRefPositionOffset + 4);
		g_risen.z = FloatAt(zombie + kRefPositionOffset + 8);
		g_risenWatching = true;
		g_log("Swap: walker %08X gets up at %.0f %.0f %.0f where %08X lay", g_swap.zombieId, g_risen.x, g_risen.y,
		      g_risen.z, g_swap.corpseId);
		const UInt32 soundRoll = NextRandom() % 100;
		if (GetUpSoundDue(g_settings.soundAtGetUpPercent, g_getUpSound, soundRoll)) {
			char line[48];
			std::snprintf(line, sizeof(line), "PlaySound3D %08X", g_getUpSound);
			const bool played = ConsoleOn(zombie, line);
			g_log("Swap: walker %08X makes the sound held back at its death as it gets up (%s, answered %d; roll %u "
			      "under %u) [tick %llu]",
			      g_swap.zombieId, line, played ? 1 : 0, soundRoll, g_settings.soundAtGetUpPercent, QpcNow());
		} else if (g_getUpSound != 0) {
			g_log("Swap: walker %08X gets up silent this time (roll %u, SoundAtGetUpPercent=%u)", g_swap.zombieId,
			      soundRoll, g_settings.soundAtGetUpPercent);
		}
		g_getUpSound = 0;
	}
}

// Every main-loop pass (xOBSE's task queue): the waiting clocks run by the
// engine's frame seconds for those in the world; a loading screen's frame
// (a second or more) counts nothing.
// The walkers walk (4.4): every frame, for every walker creature with a high
// process, a run in its movement flags becomes a walk. No engine byte is
// patched - OBVR reroutes the HighProcess flag setters for its slow
// approach and refuses to when they are not the game's own, so the plugin
// writes the flags its AI left, once a frame. Each walker's ground speed is
// measured and logged every two seconds, the proof of the walk.
constexpr UInt32 kMovementFlagsOffset = 0x1FC;  // HighProcess
constexpr UInt32 kMobileProcessOffset = 0x058;  // MobileObject::process
constexpr UInt32 kProcessLevelSlot = 0x08;      // BaseProcess virtual: 0 for a high process

struct WalkerSeen {
	UInt32 refId = 0;
	float x = 0.0f;
	float y = 0.0f;
	float speed = 0.0f;
	UInt32 turned = 0;
	bool have = false;
};
WalkerSeen g_seen[16];
float g_walkLogSeconds = 0.0f;
// The walkers met outside a chapel this session, and those taken out of
// one (AvoidChapels, SpawnedInChapel).
std::unordered_set<UInt32> g_seenOutside;
std::unordered_set<UInt32> g_banished;

// A live walker made in a chapel: disabled, as the swap hides a corpse.
bool StepChapel(UInt32 actor, UInt32 refId) {
	const char* cellId = nullptr;
	UInt32 owner = 0;
	CellFacts(actor, &cellId, &owner);
	const bool inChapel = IsChapelCell(cellId, owner);
	if (!inChapel) {
		g_seenOutside.insert(refId);
		return false;
	}
	if (!SpawnedInChapel(g_settings.avoidChapels, inChapel, g_seenOutside.count(refId) != 0,
	                     g_banished.count(refId) != 0)) {
		return false;
	}
	const bool hidden = ConsoleOn(actor, "Disable");
	g_banished.insert(refId);
	g_log("Chapel: walker %08X made in %s (owned by %08X) - taken out of the world (Disable handed to xOBSE: %d)",
	      refId, cellId != nullptr ? cellId : "-", owner, hidden ? 1 : 0);
	return true;
}

UInt32 HighProcessOf(UInt32 actor) {
	const UInt32 process = Read(actor + kMobileProcessOffset);
	if (!LooksLikeObject(process) || !LooksLikeObject(Read(process))) {
		return 0;
	}
	const UInt32 levelFn = Read(Read(process) + kProcessLevelSlot);
	if (levelFn < kTextStart || levelFn >= kTextEnd) {
		return 0;
	}
	using LevelFn = UInt32(__fastcall*)(void* process, void* edx);
	return reinterpret_cast<LevelFn>(levelFn)(reinterpret_cast<void*>(process), nullptr) == 0 ? process : 0;
}

void StepWalkers(float dt) {
	if (g_walkerCount == 0) {
		return;
	}
	g_walkLogSeconds += dt;
	const bool logNow = g_walkLogSeconds >= 2.0f;
	if (logNow) {
		g_walkLogSeconds = 0.0f;
	}
	for (UInt32 w = 0; w < g_walkerCount; ++w) {
		UInt32 refs[16];
		const UInt32 count = CollectWalkerRefs(g_walkers[w], refs, 16);
		for (UInt32 i = 0; i < count; ++i) {
			const UInt32 actor = LookupForm(refs[i]);
			if (!LooksLikeObject(actor) || !IsActor(actor) || IsDead(actor)) {
				continue;
			}
			if (StepChapel(actor, refs[i])) {
				continue;
			}
			const UInt32 process = HighProcessOf(actor);
			if (process == 0) {
				continue;
			}
			UInt16* const flags = reinterpret_cast<UInt16*>(process + kMovementFlagsOffset);
			const UInt16 before = *flags;
			if (g_settings.walkOnly) {
				*flags = WalkInsteadOfRun(before);
			}
			WalkerSeen* seen = nullptr;
			for (WalkerSeen& s : g_seen) {
				if (s.refId == refs[i]) {
					seen = &s;
					break;
				}
			}
			if (seen == nullptr) {
				for (WalkerSeen& s : g_seen) {
					if (s.refId == 0) {
						s = WalkerSeen{};
						s.refId = refs[i];
						seen = &s;
						break;
					}
				}
			}
			if (seen == nullptr) {
				continue;
			}
			if (before != *flags) {
				++seen->turned;
			}
			const float x = FloatAt(actor + kRefPositionOffset);
			const float y = FloatAt(actor + kRefPositionOffset + 4);
			if (seen->have && dt > 0.0f) {
				const float moved = std::sqrt((x - seen->x) * (x - seen->x) + (y - seen->y) * (y - seen->y));
				seen->speed = seen->speed * 0.9f + (moved / dt) * 0.1f;
			}
			seen->x = x;
			seen->y = y;
			seen->have = true;
			if (logNow) {
				g_log("Walk: walker %08X flags %04X, %.0f units/s, %u runs turned to walks", refs[i], *flags,
				      seen->speed, seen->turned);
			}
		}
	}
}

// The spawn replacement (4.6) and the walkers' health (4.5), once the ESP's
// walkers are known: at the first tick (the main menu - the records are
// loaded, nothing spawned yet), and again five game seconds after every
// load, for the creatures a mod's script put into a list since.
void StepSpawns(float dt) {
	if (g_walkerCount == 0) {
		return;
	}
	if (!g_spawnsDone) {
		g_spawnsDone = true;
		ScaleWalkerHealth(g_walkers, g_walkerCount, g_settings.healthPercent, g_log);
		ScaleWalkerSpeed(g_walkers, g_walkerCount, g_settings.speedMultiplier, g_settings.speedRandomPercent, g_log);
		RenameZombies(g_walkers, g_walkerCount, FindEspIndex(), g_settings, g_log);
		if (g_replace.enabled) {
			ReplaceSpawns(FindEspIndex(), g_replace, g_iniPath, g_log);
		}
		return;
	}
	if (g_spawnsAgainSeconds < 0.0f) {
		return;
	}
	g_spawnsAgainSeconds -= dt;
	if (g_spawnsAgainSeconds <= 0.0f) {
		g_spawnsAgainSeconds = -1.0f;
		ReplaceSpawns(FindEspIndex(), g_replace, g_iniPath, g_log);
	}
}

// BlockMooResurrect: the dead walkers due a pass lose MOO's token and brain
// (RemoveItem, the console's own; a count of 1 is what MOO gives).
void StepMooStripping(float dt) {
	UInt32 due[8];
	UInt8 pass[8];
	const UInt32 count = StepMooStrip(g_mooStrip, dt, due, 8, pass);
	for (UInt32 i = 0; i < count; ++i) {
		const UInt32 ref = LookupForm(due[i]);
		if (!LooksLikeObject(ref) || !IsActor(ref)) {
			continue;
		}
		char line[48];
		std::snprintf(line, sizeof(line), "RemoveItem %08X 1", g_mooToken);
		const bool token = ConsoleOn(ref, line);
		std::snprintf(line, sizeof(line), "RemoveItem %08X 1", g_mooBrain);
		const bool brain = ConsoleOn(ref, line);
		if (pass[i] == 1) {
			// RunScriptLine2's answer is no success flag (it reads 0 for a
			// RemoveItem that took: moo-stay-down.txt, 2026-10-25 20:12).
			g_log("MOO: walker %08X dead - MOO's token and brain taken, so MOO does not raise it (RemoveItem "
			      "answered %d %d; again in %.0f s)",
			      due[i], token ? 1 : 0, brain ? 1 : 0, kMooSecondPassSeconds);
		}
	}
}

// NoLoot: the engine's SetDestroyed on each walker that died since the last
// frame - the CS wiki: on an actor it "prevents the actor from being
// activated for conversation, looting, or pick pocketing".
void StepLootless() {
	for (UInt32 i = 0; i < g_lootlessCount; ++i) {
		const UInt32 ref = LookupForm(g_lootless[i]);
		const bool done = LooksLikeObject(ref) && IsActor(ref) && ConsoleOn(ref, "SetDestroyed 1");
		g_log("Loot: walker %08X can no longer be activated (SetDestroyed handed to xOBSE: %d)", g_lootless[i],
		      done ? 1 : 0);
	}
	g_lootlessCount = 0;
}

// Fame: the points the death path earned, through the engine's own
// ModPCFame (the CS wiki: "Adds the value to the player character current
// fame value").
void StepFame() {
	if (g_famePending == 0) {
		return;
	}
	const UInt32 player = LookupForm(kPlayerRefId);
	char line[32];
	std::snprintf(line, sizeof(line), "ModPCFame %u", g_famePending);
	const bool handed = LooksLikeObject(player) && ConsoleOn(player, line);
	g_log("Fame: %u point%s for walkers cut down (ModPCFame handed to xOBSE: %d)", g_famePending,
	      g_famePending == 1 ? "" : "s", handed ? 1 : 0);
	g_famePending = 0;
}

void Tick() {
	if (!g_deathWatchOn && !TryRegisterDeath()) {
		return;
	}
	{
		const float frame = *reinterpret_cast<const float*>(kFrameSeconds);
		FindWalkers();
		FindMoo();
		StepMooStripping(frame > 0.0f && frame < 1.0f ? frame : 0.0f);
		StepSpawns(frame > 0.0f && frame < 1.0f ? frame : 0.0f);
		StepWalkers(frame > 0.0f && frame < 1.0f ? frame : 0.0f);
		StepLootless();
		StepFame();
	}
	if (g_risenWatching) {
		StepRisenWatch();
	}
	float dt = *reinterpret_cast<const float*>(kFrameSeconds);
	if (!(dt > 0.0f && dt < 1.0f)) {
		dt = 0.0f;
	}
	if (g_swap.active) {
		StepSwapInFlight(dt);
	}
	if (g_list.count == 0) {
		return;
	}
	UInt32 due[kWaitingMost];
	const UInt32 dueCount = TickWaiting(g_list, dt, &PresenceOf, due, kWaitingMost);
	for (UInt32 i = 0; i < dueCount; ++i) {
		Raise(due[i]);
	}
}

void OnSave(void*) {
	UInt8 image[kWaitingImageMost];
	const UInt32 length = WaitingImage(g_list, image, sizeof(image));
	if (!g_serialization->OpenRecord(kWaitingRecordType, kWaitingRecordVersion) ||
	    !g_serialization->WriteRecordData(image, length)) {
		g_log("Co-save: the waiting list could not be written (%u waiting)", g_list.count);
		return;
	}
	const UInt32 gone = KeepRisen(g_risenSet, [](UInt32 id) { return LooksLikeObject(LookupForm(id)); });
	UInt8 risen[kRisenImageMost];
	const UInt32 risenLength = RisenImage(g_risenSet, risen, sizeof(risen));
	const bool risenWritten = g_serialization->OpenRecord(kRisenRecordType, kRisenRecordVersion) &&
	                          g_serialization->WriteRecordData(risen, risenLength);
	UInt8 fame[kFameImageBytes];
	const UInt32 fameLength = FameImage(g_fame, fame, sizeof(fame));
	const bool fameWritten = g_serialization->OpenRecord(kFameRecordType, kFameRecordVersion) &&
	                         g_serialization->WriteRecordData(fame, fameLength);
	g_log("Co-save: %u waiting written; %u risen walkers %s (%u gone from the game dropped); %u walker kills toward "
	      "the next point of fame %s",
	      g_list.count, g_risenSet.count, risenWritten ? "written" : "could not be written", gone, g_fame.kills,
	      fameWritten ? "written" : "could not be written");
}

bool ResolveSaved(UInt32 savedId, UInt32& loadedId) {
	return g_serialization->ResolveRefID(savedId, &loadedId);
}

void OnLoad(void*) {
	g_list = WaitingList{};
	g_risenSet = RisenSet{};
	g_fame = FameCount{};
	g_famePending = 0;
	g_lootlessCount = 0;
	g_mooStrip = MooStrip{};
	g_banished.clear();
	if (g_replace.enabled) {
		g_spawnsAgainSeconds = 5.0f;
	}
	UInt32 type = 0;
	UInt32 version = 0;
	UInt32 length = 0;
	while (g_serialization->GetNextRecordInfo(&type, &version, &length)) {
		if (type == kFameRecordType) {
			UInt8 fame[kFameImageBytes];
			const UInt32 read =
				version == kFameRecordVersion && length <= sizeof(fame) ? g_serialization->ReadRecordData(fame, length) : 0;
			const bool whole = ReadFameImage(g_fame, fame, read);
			g_log("Co-save: %u walker kills toward the next point of fame read%s", g_fame.kills,
			      whole ? "" : " (version or length off: from 0)");
			continue;
		}
		if (type == kRisenRecordType) {
			if (version != kRisenRecordVersion || length > kRisenImageMost) {
				g_log("Co-save: risen walkers of version %u, %u bytes: not read", version, length);
				continue;
			}
			UInt8 risen[kRisenImageMost];
			const UInt32 read = g_serialization->ReadRecordData(risen, length);
			const bool whole = ReadRisenImage(g_risenSet, risen, read, &ResolveSaved);
			g_log("Co-save: %u risen walkers read%s", g_risenSet.count, whole ? "" : " (the record cut short)");
			continue;
		}
		if (type != kWaitingRecordType) {
			continue;
		}
		if (version != kWaitingRecordVersion || length > kWaitingImageMost) {
			g_log("Co-save: a waiting list of version %u, %u bytes: not read", version, length);
			continue;
		}
		UInt8 image[kWaitingImageMost];
		const UInt32 read = g_serialization->ReadRecordData(image, length);
		if (!ReadWaitingImage(g_list, image, read, &ResolveSaved)) {
			g_log("Co-save: the waiting list is cut short (%u of %u bytes), %u read before that", read, length,
			      g_list.count);
			continue;
		}
		g_log("Co-save: %u waiting read", g_list.count);
	}
}

void OnNewGame(void*) {
	g_list = WaitingList{};
	g_risenSet = RisenSet{};
	g_fame = FameCount{};
	g_famePending = 0;
	g_lootlessCount = 0;
	g_mooStrip = MooStrip{};
	g_banished.clear();
	if (g_replace.enabled) {
		g_spawnsAgainSeconds = 5.0f;
	}
	g_log("New game: nobody waits");
}

}  // namespace

bool InstallRise(const obse::Interface& xse, const RiseSettings& settings, const ReplaceSettings& replace,
                 const char* iniPath, LogFn log) {
	g_log = log;
	g_settings = settings;
	g_replace = replace;
	std::snprintf(g_iniPath, sizeof(g_iniPath), "%s", iniPath != nullptr ? iniPath : "");
	g_spawnsDone = false;
	g_spawnsAgainSeconds = -1.0f;
	g_lootlessCount = 0;
	g_list = WaitingList{};
	g_risenSet = RisenSet{};
	g_fame = FameCount{};
	g_famePending = 0;
	g_random = GetTickCount() | 1u;
	g_deathWatchOn = false;
	g_registerTries = 0;
	g_risenWatching = false;
	g_swap = Swap{};
	g_walkersLooked = false;
	g_walkerCount = 0;
	g_mooLooked = false;
	g_mooStrip = MooStrip{};
	if (xse.QueryInterface == nullptr) {
		log("xOBSE offers no QueryInterface: no death watch");
		return false;
	}
	g_events = static_cast<obse::EventManagerInterface*>(xse.QueryInterface(obse::kInterface_EventManager));
	g_tasks = static_cast<obse::TasksInterface*>(xse.QueryInterface(obse::kInterface_Tasks));
	g_serialization = static_cast<obse::SerializationInterface*>(xse.QueryInterface(obse::kInterface_Serialization));
	g_console = static_cast<obse::ConsoleInterface*>(xse.QueryInterface(obse::kInterface_Console));
	if (g_console != nullptr && (g_console->version < 2 || g_console->RunScriptLine2 == nullptr)) {
		log("xOBSE's console interface is version %u without RunScriptLine2: the risen keep their old look",
		    g_console->version);
		g_console = nullptr;
	}
	if (g_events == nullptr) {
		log("xOBSE has no event manager interface: no death watch");
		return false;
	}
	if (g_tasks == nullptr) {
		log("xOBSE has no task interface: no frame tick, no death watch");
		return false;
	}
	if (g_tasks->EnqueueTask(&Tick) == nullptr) {
		log("xOBSE refused the frame tick: no death watch");
		return false;
	}
	TryRegisterDeath();
	if (g_serialization != nullptr && xse.GetPluginHandle != nullptr) {
		// xOBSE calls a plugin's save and load callbacks only when it set an
		// opcode base of its own (the run of 2026-10-24 08:52: "HandleSaveGame:
		// plugin with default opcode base registered for serialization", our
		// save callback never ran). 0x2B00 is free in the registered list
		// (github.com/llde/xOBSE/wiki/Registered-Opcodes-for-OBSE-Plugins,
		// read 2026-10-24); a registration of our own is still to be asked for.
		if (xse.SetOpcodeBase != nullptr) {
			xse.SetOpcodeBase(kOpcodeBase);
		}
		const UInt32 plugin = xse.GetPluginHandle();
		g_serialization->SetSaveCallback(plugin, &OnSave);
		g_serialization->SetLoadCallback(plugin, &OnLoad);
		g_serialization->SetNewGameCallback(plugin, &OnNewGame);
		log("The waiting list goes to the co-save (plugin handle %u, opcode base %04X)", plugin, kOpcodeBase);
	} else {
		g_serialization = nullptr;
		log("No serialization interface: the waiting list lives for the session only");
	}
	return true;
}

}  // namespace walkers
