#include "Rise.h"

#include "Engine.h"
#include "Grab.h"
#include "Horde.h"
#include "LevelingLogic.h"
#include "LinkLogic.h"
#include "MooLogic.h"
#include "News.h"
#include "SoundLogic.h"
#include "Pose.h"
#include "QuestVars.h"
#include "SkillLogic.h"
#include "Replace.h"

#include <windows.h>
#include <intrin.h>

#include <cmath>
#include <cstdio>
#include <cstring>
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
// A cell's flags (bit 0: interior) and an exterior's worldspace - as Horde.cpp
// reads them (0x004C9CF0).
constexpr UInt32 kCellFlagsOffset = 0x24;
constexpr UInt32 kCellWorldspaceOffset = 0x50;
// The game's uGridsToLoad:General setting: its value at the Setting's head,
// the name "uGridsToLoad:General" (0x00A3D7C4) right after it - read off
// Oblivion.exe's bytes 2026-10-27; 5 there, the INI's value once loaded.
constexpr UInt32 kGridsToLoadSetting = 0x00B06A2C;
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
// A creature's No Head, the record's ACBS bit (UESP Tes4Mod:Mod_File_Format/
// CREA) in the same flags the record's essential and no-low-level bits are.
constexpr UInt32 kActorBaseNoHead = 0x8000;
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
UInt32 g_walkers[kWalkerCount] = {};
LookFamily g_walkerFamilies[kWalkerCount] = {};
bool g_walkerHeadless[kWalkerCount] = {};
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
// 4.17 (EndLogic.h): whether OBVR cuts heads ('WGDM'), the heads it reported
// ('WGHD', noted at once for a death in the same frame), and those still to
// act on at the next frame tick.
bool g_headCutter = false;
HeadNotes g_heads;
UInt32 g_headsDue[8] = {};
UInt32 g_headsDueCount = 0;
UInt32 g_powerChecks = 0;
// BlockMooResurrect: MOO's token and brain at MOO's load index (0 without
// MOO), looked up once a session, and the dead walkers still to strip.
bool g_mooLooked = false;
// NoLevelingXP: Maskar's Ultimate Leveling loaded (LevelingLogic.h), looked
// up with MOO; how many walker deaths were held back, for the log.
bool g_levelingLoaded = false;
UInt32 g_levelingHeld = 0;
UInt32 g_levelingIndex = 0x100;
// LINK (LinkLogic.h): loaded, the variables brought in line with the
// settings since the game's load, and the second's clock.
bool g_linkLoaded = false;
bool g_linkSynced = false;
float g_linkClock = 0.0f;
// The kills by the player whose experience the log checks a frame later:
// ULVL.XP read before Ultimate Leveling's handler of the death, and after.
struct LevelingCheck {
	UInt32 refId;
	bool walker;
	double before;      // ULVL.XP
	double obXpBefore;  // ObXPMain.currentXP
};
bool g_obXpLoaded = false;
UInt32 g_obXpIndex = 0x100;
LevelingCheck g_levelingChecks[4] = {};
UInt32 g_levelingCheckCount = 0;
UInt32 g_levelingChecksLeft = 12;
float g_levelingCheckWait = 0.0f;
// MOO's ESP in the load order: its archives with the meshes of the walkers'
// MOO looks are loaded (WalkerLooks.h LookUsable).
bool g_mooLoaded = false;
UInt32 g_mooToken = 0;
UInt32 g_mooBrain = 0;
MooStrip g_mooStrip;
// Fame (4.8): the player's walker kills toward the next point, and the
// points earned in the death path, handed to the engine at the next tick.
FameCount g_fame;
UInt32 g_famePending = 0;
constexpr UInt32 kPlayerRefId = 0x14;
// The safeguard (4.11b): whoever the walkers killed, raised once it is safe;
// looked at once a second.
SafeguardList g_safeguard;
float g_safeguardClock = 0.0f;
// The deaths without a killer the safeguard asks about at the next tick.
UInt32 g_unclaimed[16] = {};
UInt32 g_unclaimedCount = 0;
// Every 10 s, how the ones still down stand (a few dozen lines a session).
float g_safeguardStatusClock = 0.0f;
UInt32 g_safeguardStatusLeft = 60;

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

// Whether `ref` is a walker: a creature of the ESP's walker bases.
bool IsWalkerRef(UInt32 ref) {
	if (!LooksLikeObject(ref) || Read(ref) != kVtblCreature) {
		return false;
	}
	const UInt32 baseId = FormIdOf(Read(ref + kRefBaseFormOffset));
	for (UInt32 w = 0; w < g_walkerCount; ++w) {
		if (g_walkers[w] == baseId) {
			return true;
		}
	}
	return false;
}

bool ConsoleOn(UInt32 ref, const char* line);
UInt32 FindEspBase();

// Whether the walker `refId` wears one of MOO's looks - MOO's zombie
// skeleton, whose idles lie beneath MOO\zombie (the grab, 4.19).
bool OnMooSkeleton(UInt32 refId) {
	const UInt32 ref = LookupForm(refId);
	if (!LooksLikeObject(ref)) {
		return false;
	}
	const UInt32 baseId = FormIdOf(Read(ref + kRefBaseFormOffset));
	for (UInt32 w = 0; w < g_walkerCount; ++w) {
		if (g_walkers[w] == baseId) {
			return g_walkerFamilies[w] != LookFamily::Vanilla;
		}
	}
	return false;
}

// NoSkillGain (SkillLogic.h): the frame's last hit, from xOBSE's OnHit, and
// the player's skill use behind it.
UInt32 g_frame = 0;
struct LastHit {
	UInt32 frame = 0xFFFFFFFF;
	UInt32 target = 0;
	UInt32 attacker = 0;
};
LastHit g_lastHit;
UInt32 g_skillLinesLeft = 24;
UInt32 g_otherSkillLinesLeft = 6;
UInt32 g_hitLinesLeft = 12;
UInt32 g_originalSkillUse = 0;
bool g_hitWatchOn = false;

// xOBSE's OnHit: the hit actor and who hit it, noted for the skill use of the
// same frame.
void __cdecl OnHit(void* arg0, void* arg1, void* thisObj) {
	g_lastHit.frame = g_frame;
	g_lastHit.target = reinterpret_cast<UInt32>(arg0);
	g_lastHit.attacker = reinterpret_cast<UInt32>(arg1);
	GrabOnHit(g_lastHit.target, g_lastHit.attacker);
	if (g_hitLinesLeft > 0) {
		--g_hitLinesLeft;
		g_log("Hit: %08X hit by %08X (frame %u; this %08X)", FormIdOf(g_lastHit.target), FormIdOf(g_lastHit.attacker),
		      g_frame, FormIdOf(reinterpret_cast<UInt32>(thisObj)));
	}
}

// The caller's EDI as the hook was entered (SkillUseStub) - at the armor
// site the attacker (SkillLogic.h SiteOf).
UInt32 g_callerEdi = 0;
UInt32 g_callerEbp = 0;  // at the block site the attacker

// PlayerCharacter's skill use, in its vtable slot: held back when the call
// site names a walker as the attacker (the armor of its hit), or when the
// frame's hit was between the player and a walker and the skill is a blow's.
void __fastcall SkillUseHook(void* self, void* edx, UInt32 skill, UInt32 use, float times) {
	const UInt32 caller = reinterpret_cast<UInt32>(_ReturnAddress());
	const SkillSite site = SiteOf(caller);
	const UInt32 attacker = site == SkillSite::BlockOfHit ? g_callerEbp : g_callerEdi;
	SkillHit hit{};
	hit.thisFrame = g_lastHit.frame == g_frame;
	if (hit.thisFrame) {
		hit.targetWalker = IsWalkerRef(g_lastHit.target);
		hit.attackerWalker = IsWalkerRef(g_lastHit.attacker);
		hit.targetPlayer = FormIdOf(g_lastHit.target) == 0x14;
		hit.attackerPlayer = FormIdOf(g_lastHit.attacker) == 0x14;
	}
	const bool siteWalker = (site == SkillSite::ArmorOfHit || site == SkillSite::BlockOfHit) && LooksLikeObject(attacker) &&
	                        IsWalkerRef(attacker);
	const bool hold = HoldsSiteUse(g_settings.noSkillGain, site, siteWalker) || HoldsSkillUse(g_settings.noSkillGain, skill, hit);
	if (!IsBlowSkill(skill) && g_otherSkillLinesLeft > 0) {
		--g_otherSkillLinesLeft;
		g_log("Skills: the player's skill %02X use %u (not a blow's) - trained", skill, use);
	}
	if (IsBlowSkill(skill) && g_skillLinesLeft > 0) {
		--g_skillLinesLeft;
		g_log("Skills: the player's skill %02X use %u x%.2f (from %08X, %s%s%08X) - %s; the frame's hit %s: %08X by %08X",
		      skill, use, times, caller,
		      site == SkillSite::ArmorOfHit       ? "the armor of a hit"
		      : site == SkillSite::BlockOfHit     ? "a block"
		      : site == SkillSite::WeaponOfAttack ? "the weapon of an attack"
		                                          : "elsewhere",
		      site == SkillSite::ArmorOfHit || site == SkillSite::BlockOfHit ? ", the attacker " : ", EDI ",
		      site == SkillSite::ArmorOfHit || site == SkillSite::BlockOfHit ? FormIdOf(attacker) : attacker,
		      hold ? "HELD BACK" : "trained", hit.thisFrame ? "this frame" : "none", FormIdOf(g_lastHit.target),
		      FormIdOf(g_lastHit.attacker));
	}
	if (hold) {
		return;
	}
	using SkillUseFn = void(__fastcall*)(void* self, void* edx, UInt32 skill, UInt32 use, float times);
	reinterpret_cast<SkillUseFn>(g_originalSkillUse)(self, edx, skill, use, times);
}

// Entered from the vtable slot: notes the caller's EDI, then on to the hook
// with the stack as the caller left it (ECX the player, the three arguments,
// the return address).
__declspec(naked) void SkillUseStub() {
	__asm {
		mov g_callerEdi, edi
		mov g_callerEbp, ebp
		jmp SkillUseHook
	}
}

// The player's landed-hit count (SkillLogic.h kHitCountSlot): a landed hit
// on a walker - the target in ESI at 0x005FF54B - is not counted, so the
// attack update (or OBVR after its strike) trains no weapon skill for it.
UInt32 g_callerEsi = 0;
UInt32 g_originalHitCount = 0;
UInt32 g_hitCountLinesLeft = 12;

void __fastcall HitCountHook(void* self, void* edx, UInt32 count) {
	const UInt32 caller = reinterpret_cast<UInt32>(_ReturnAddress());
	const UInt32 target = g_callerEsi;
	const bool targetWalker = caller == kHitCountReturn && LooksLikeObject(target) && IsWalkerRef(target);
	const bool hold = HoldsHitCount(g_settings.noSkillGain, caller, targetWalker);
	if (caller == kHitCountReturn && g_hitCountLinesLeft > 0) {
		--g_hitCountLinesLeft;
		g_log("Skills: the player's landed hit on %08X counted to %u - %s", FormIdOf(target), count,
		      hold ? "NOT COUNTED (a walker)" : "counted");
	}
	if (hold) {
		return;
	}
	using HitCountFn = void(__fastcall*)(void* self, void* edx, UInt32 count);
	reinterpret_cast<HitCountFn>(g_originalHitCount)(self, edx, count);
}

__declspec(naked) void HitCountStub() {
	__asm {
		mov g_callerEsi, esi
		jmp HitCountHook
	}
}

// OBVR's skill exceptions (SkillLogic.h): the walker bases with NoSkillGain
// on, none with it off; sent once the walkers are known after a load, and
// again when LINK changes NoSkillGain.
obse::MessagingInterface* g_messaging = nullptr;
UInt32 g_pluginHandle = 0;
bool g_exceptionsSent = false;
bool g_exceptionsSetting = false;

void SendSkillExceptions() {
	if (g_messaging == nullptr || g_walkerCount == 0) {
		return;
	}
	static UInt32 bases[kWalkerCount];
	const UInt32 n = SkillExceptions(g_settings.noSkillGain, g_walkers, g_walkerCount, bases, kWalkerCount);
	const bool sent = g_messaging->Dispatch(g_pluginHandle, kSkillExceptionsMessage, n > 0 ? bases : nullptr, n * 4,
	                                        kSkillExceptionsReceiver);
	g_exceptionsSent = true;
	g_exceptionsSetting = g_settings.noSkillGain;
	g_log("Skills: %u walker bases sent to %s as skill exceptions (NoSkillGain=%d; dispatched: %d)", n,
	      kSkillExceptionsReceiver, g_settings.noSkillGain ? 1 : 0, sent ? 1 : 0);
}

// The slot is swapped once a session, if it still holds the engine's own
// function (another plugin's hook there: left alone, logged).
void InstallSkillHook() {
	auto* const slot = reinterpret_cast<UInt32*>(kPlayerSkillUseSlot);
	if (*slot != kPlayerSkillUse) {
		g_log("Skills: the player's skill use slot holds %08X, not the engine's %08X - left alone, NoSkillGain off",
		      *slot, kPlayerSkillUse);
		return;
	}
	DWORD old = 0;
	if (!VirtualProtect(slot, 4, PAGE_EXECUTE_READWRITE, &old)) {
		g_log("Skills: the vtable slot could not be written - NoSkillGain off");
		return;
	}
	g_originalSkillUse = *slot;
	*slot = reinterpret_cast<UInt32>(&SkillUseStub);
	VirtualProtect(slot, 4, old, &old);
	g_log("Skills: the player's skill use (vtable slot %08X) watched - NoSkillGain=%d", kPlayerSkillUseSlot,
	      g_settings.noSkillGain ? 1 : 0);
	auto* const count = reinterpret_cast<UInt32*>(kHitCountSlot);
	if (*count < 0x00401000 || *count >= 0x00A00000) {
		g_log("Skills: the player's hit count slot holds %08X, not in the game's code - left alone", *count);
		return;
	}
	if (!VirtualProtect(count, 4, PAGE_EXECUTE_READWRITE, &old)) {
		g_log("Skills: the hit count slot could not be written - landed hits on walkers still count");
		return;
	}
	g_originalHitCount = *count;
	*count = reinterpret_cast<UInt32>(&HitCountStub);
	VirtualProtect(count, 4, old, &old);
	g_log("Skills: the player's landed-hit count (vtable slot %08X, %08X) watched", kHitCountSlot, g_originalHitCount);
}

// 4.17 without a head cutter: whether the killer's blow was a power attack -
// xOBSE's own IsPowerAttacking (Commands_AI.cpp: the actor's anim data plays
// a group of AttackPower..AttackRightPower; the player's first-person data
// in first person), asked on the killer into the ESP's quest variable
// power_kill. Whether the attack still plays at the death is not measured.
// The player counts: IsActor knows only the Character and Creature vtables
// (end-power-attack.txt, 2026-10-30 19:47: no question was ever asked).
bool PowerKill(UInt32 killer) {
	if (!LooksLikeObject(killer) || (!IsActor(killer) && FormIdOf(killer) != kPlayerRefId)) {
		if (g_powerChecks++ < 16) {
			g_log("End: killer %08X is no actor - no power attack", LooksLikeObject(killer) ? FormIdOf(killer) : 0);
		}
		return false;
	}
	double* power = QuestVariable(EspFormId(FindEspBase(), kLinkQuestLocalId), "power_kill");
	if (power == nullptr) {
		if (g_powerChecks++ == 0) {
			g_log("End: the ESP's quest has no power_kill (an older ESP) - no power attack ends a walker");
		}
		return false;
	}
	// -1 before the line: still -1 after it, the line did not run (xOBSE's
	// answer is no success flag - the harness read 0 for lines that ran).
	*power = -1.0;
	ConsoleOn(killer, "set Walkers.power_kill to IsPowerAttacking");
	const double read = *power;
	const bool on = read > 0.0;
	if (g_powerChecks++ < 16) {
		g_log("End: killer %08X power attacking: %d%s", FormIdOf(killer), on ? 1 : 0,
		      read < 0.0 ? " - the line did not run" : "");
	}
	return on;
}

// xOBSE's OnDeath, inside the engine's death path: only noted, nothing of
// the actor touched in this frame (Zombie Walkers' crash was there) - with
// one exception, NoLevelingXP: Ultimate Leveling reads the dead walker's
// Confidence and Aggression in its own handler of this same event, so they
// are set here (MOO's and Ultimate Leveling's handlers touch the dead in
// this event too: MOO gives a token, Ultimate Leveling removes one).
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
		fallen.headOff = HeadNoted(g_heads, fallen.refId);
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
	if (IsGateGuard(fallen.refId)) {
		g_log("Death: gate guard %08X fell (killer %08X) - its pool sends the next and raises it when none is left",
		      fallen.refId, FormIdOf(reinterpret_cast<UInt32>(arg1)));
		return;
	}
	bool walker = false;
	bool headless = false;
	for (UInt32 w = 0; type == kFormTypeCreature && w < g_walkerCount; ++w) {
		if (g_walkers[w] == fallen.baseId) {
			walker = true;
			headless = g_walkerHeadless[w];
		}
	}
	// The swap's own unseen kill of the walker it placed (4.1): the swap
	// raises it, not 4.17 (OBVR's dismember-walker.txt, 2026-10-30 19:14:
	// "down - it gets up again" for the swap's walker).
	const bool swapKill = g_swap.active && fallen.refId == g_swap.zombieId;
	WalkerFall fall = WalkerFall::EndedRuleOff;
	if (walker && !swapKill) {
		FallFacts facts;
		facts.headless = headless;
		facts.headNoted = HeadNoted(g_heads, fallen.refId);
		facts.cutterOn = g_headCutter;
		facts.powerKill = g_settings.end.dieOnlyBeheaded && !g_headCutter && !headless && !facts.headNoted &&
		                  PowerKill(reinterpret_cast<UInt32>(arg1));
		fall = WalkerFallVerdict(g_settings.end.dieOnlyBeheaded, facts);
	}
	// A walker that gets up again keeps its corpse searchable and counts for
	// no fame yet: both at its end (spec 4.17 point 7).
	const bool walkerUp = walker && !swapKill && !Ends(fall);
	if (!walkerUp && BlocksLoot(g_settings.noLoot, walker, IsRisen(g_risenSet, fallen.refId)) && g_lootlessCount < 32) {
		g_lootless[g_lootlessCount++] = fallen.refId;
	}
	if ((g_levelingLoaded || g_obXpLoaded) && FormIdOf(reinterpret_cast<UInt32>(arg1)) == kPlayerRefId &&
	    g_levelingChecksLeft > 0 && g_levelingCheckCount < 4) {
		const double* xp = QuestVariable((g_levelingIndex << 24) | kLevelingQuestLocalId, kLevelingXpVariable);
		const double* obXp = QuestVariable((g_obXpIndex << 24) | kObXpQuestLocalId, kObXpXpVariable);
		g_levelingChecks[g_levelingCheckCount++] =
			LevelingCheck{fallen.refId, walker, xp != nullptr ? *xp : -1.0, obXp != nullptr ? *obXp : -1.0};
	}
	const bool holdUlvl = HoldsLevelingXp(g_settings.noLevelingXp, g_levelingLoaded, walker);
	const bool holdObXp = HoldsLevelingXp(g_settings.noLevelingXp, g_obXpLoaded, walker);
	if (holdUlvl || holdObXp) {
		const UInt32 dead = LookupForm(fallen.refId);
		bool handed = LooksLikeObject(dead);
		for (UInt32 i = 0; handed && holdUlvl && i < kLevelingHoldLineCount; ++i) {
			handed = ConsoleOn(dead, kLevelingHoldLines[i]);
		}
		for (UInt32 i = 0; handed && holdObXp && i < kObXpLineCount; ++i) {
			handed = ConsoleOn(dead, kObXpHoldLines[i]);
		}
		if (++g_levelingHeld <= 8) {
			g_log("Leveling: walker %08X dead -%s%s before the leveling mods count it (handed to xOBSE: %d)",
			      fallen.refId, holdUlvl ? " Confidence and Aggression to 0" : "",
			      holdObXp ? " its eight attributes to 0" : "", handed ? 1 : 0);
		}
	}
	if (StripsMoo(g_settings.blockMooResurrect, g_mooToken != 0, walker) && !NoteMooStrip(g_mooStrip, fallen.refId)) {
		g_log("MOO: walker %08X dead, but %u wait to be stripped already - MOO may raise this one", fallen.refId,
		      g_mooStrip.count);
	}
	if (!walkerUp && FameKill(walker, FormIdOf(reinterpret_cast<UInt32>(arg1)) == kPlayerRefId) &&
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
	if (walker && !swapKill && g_settings.end.dieOnlyBeheaded) {
		const float seconds = RiseAgainIn(g_settings.end, unit);
		const bool queued = walkerUp && Requeue(g_list, fallen.refId, fallen.baseId, seconds);
		g_log("End: walker %08X (base %08X, killer %08X) %s%s; %u waiting", fallen.refId, fallen.baseId,
		      FormIdOf(reinterpret_cast<UInt32>(arg1)), WalkerFallText(fall),
		      !walkerUp ? "" : (queued ? ", in a few seconds" : " - but the list is full or it waits already: it stays down"),
		      g_list.count);
	}
	Victim victim;
	victim.killedByWalker = IsWalkerRef(reinterpret_cast<UInt32>(arg1));
	victim.killerUnknown = !LooksLikeObject(reinterpret_cast<UInt32>(arg1));
	victim.walker = walker;
	victim.player = fallen.refId == kPlayerRefId;
	victim.rises = verdict == DeathVerdict::Waits || walkerUp;
	if (SafeguardTakes(g_settings.safeguard, victim)) {
		const bool noted = NoteVictim(g_safeguard, fallen.refId);
		g_log("Safeguard: %08X killed by walker %08X - %s (%u on the list)", fallen.refId,
		      FormIdOf(reinterpret_cast<UInt32>(arg1)),
		      noted ? "gets up again once it is safe" : "the list is full, it stays dead", g_safeguard.count);
	} else if (SafeguardAsks(g_settings.safeguard, victim) && g_unclaimedCount < 16) {
		g_unclaimed[g_unclaimedCount++] = fallen.refId;
	}
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

// OBVR's 'WGDM' and 'WGHD', as they come (inside OBVR's frame): the cutter
// switched, a head noted at once and queued for the next frame tick.
void OnObvrHeads(UInt32 type, const void* data, UInt32 length) {
	bool on = false;
	UInt32 refId = 0;
	HeadKind kind = HeadKind::Cut;
	if (type == kDismemberMessage && DismemberOf(data, length, on)) {
		if (on != g_headCutter) {
			g_log("End: OBVR %s heads - %s", on ? "cuts" : "does not cut",
			      on ? "only a lost head ends a walker" : "a power attack's kill ends a walker");
		}
		g_headCutter = on;
	} else if (type == kHeadMessage && HeadOf(data, length, refId, kind)) {
		NoteHead(g_heads, refId);
		if (g_headsDueCount < 8) {
			g_headsDue[g_headsDueCount++] = refId;
		}
		g_log("End: OBVR says %08X's head is %s", refId, kind == HeadKind::Cut ? "cut off" : "crushed");
	}
}

Place PlaceOf(UInt32 ref) {
	Place place;
	const UInt32 cell = LooksLikeObject(ref) ? Read(ref + kRefParentCellOffset) : 0;
	if (!LooksLikeObject(cell)) {
		return place;
	}
	place.known = true;
	place.interior = (Read8(cell + kCellFlagsOffset) & 1) != 0;
	place.cell = cell;
	place.world = place.interior ? 0 : Read(cell + kCellWorldspaceOffset);
	place.x = FloatAt(ref + kRefPositionOffset);
	place.y = FloatAt(ref + kRefPositionOffset + 4);
	return place;
}

// uGridsToLoad as the game runs it; a value outside 1..15 (the setting not
// where it was read) is logged once and taken as the default 5.
UInt32 GridsToLoad() {
	const UInt32 grids = Read(kGridsToLoadSetting);
	static bool s_logged = false;
	if (!s_logged) {
		s_logged = true;
		g_log("Presence: uGridsToLoad reads %u%s", grids,
		      grids >= 1 && grids <= 15 ? "" : " - not a grid size, taken as 5");
	}
	return grids >= 1 && grids <= 15 ? grids : 5;
}

// In the cells the game runs around the player (RiseLogic.h InPlayersCells).
bool InPlayersReach(UInt32 ref) {
	return InPlayersCells(PlaceOf(ref), PlaceOf(LookupForm(kPlayerRefId)), GridsToLoad());
}

Presence PresenceOfNow(UInt32 refId) {
	const UInt32 ref = LookupForm(refId);
	if (!LooksLikeObject(ref)) {
		return Presence::Missing;
	}
	if (!IsActor(ref)) {
		return Presence::Gone;
	}
	return PresenceAt(true, Read(ref + kRefNiNodeOffset) != 0, PlaceOf(ref), PlaceOf(LookupForm(kPlayerRefId)),
	                  GridsToLoad());
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
		      now == Presence::Present  ? "present"
		      : now == Presence::Absent ? "absent (no 3D, or not where the player is)"
		      : now == Presence::Missing ? "missing (not held by the game - kept, its clock standing)"
		                                 : "gone",
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

// OblivionWalkers.esp's place at runtime as the base of its forms' IDs
// (RiseLogic.h EspFormId): ESL-flagged and loaded by OblivionESL, the base
// its export ESL_GetFormIDBase gives (0xFE | light index << 12; its
// ESLInterface.h, 2026-10-28); else its index in the data handler's
// mods-by-ID table, << 24; kNoEsp when it is not loaded.
UInt32 FindEspBase() {
	static UInt32 eslLinesLeft = 4;
	const HMODULE esl = GetModuleHandleA("OblivionESL");
	if (esl != nullptr) {
		using BaseFn = UInt32(__cdecl*)(const char* modName);
		using CountFn = UInt32(__cdecl*)();
		const auto base = reinterpret_cast<BaseFn>(GetProcAddress(esl, "ESL_GetFormIDBase"));
		const auto count = reinterpret_cast<CountFn>(GetProcAddress(esl, "ESL_GetLoadedCount"));
		const UInt32 light = base != nullptr ? base(kPluginEsp) : kNoEsp;
		if (eslLinesLeft > 0) {
			--eslLinesLeft;
			g_log("ESP: OblivionESL is loaded (ESL_GetFormIDBase %s, %u light plugins): %s %s", base ? "found" : "MISSING",
			      count != nullptr ? count() : 0u, kPluginEsp,
			      light != kNoEsp ? "is a light plugin" : "is no light plugin of it");
		}
		if (light != kNoEsp) {
			return light;
		}
	}
	const UInt32 handler = Read(kDataHandlerPointer);
	if (!LooksLikeObject(handler)) {
		g_log("ESP: no data handler at %08X", kDataHandlerPointer);
		return kNoEsp;
	}
	const UInt32 count = Read(handler + kDataHandlerModCountOffset);
	for (UInt32 i = 0; i < count && i < 0xFF; ++i) {
		const char* name = ModNameAt(i);
		if (name != nullptr && SameModName(name, kPluginEsp)) {
			return EspBaseOfIndex(i);
		}
	}
	g_log("ESP: %s is not among the %u loaded mods, nor a light plugin of OblivionESL", kPluginEsp, count);
	return kNoEsp;
}

// The ESP's Walker race at its local ID 0x800. 0 when the ESP is not loaded
// or the record there is no race; the log says which.
UInt32 FindZombieRace() {
	const UInt32 base = FindEspBase();
	if (base == kNoEsp) {
		g_log("Race: no ESP, the clone keeps his race");
		return 0;
	}
	const UInt32 formId = ZombieRaceFormId(base);
	const UInt32 race = LookupForm(formId);
	if (!LooksLikeObject(race) || Read8(race + kFormTypeOffset) != kFormTypeRace) {
		g_log("Race: %s is at %08X, but %08X is no race (%s): the clone keeps his race", kPluginEsp, base,
		      formId, LooksLikeObject(race) ? "another type" : "no form");
		return 0;
	}
	g_log("Race: %s is at %08X, the Walker race %08X found", kPluginEsp, base, formId);
	return race;
}

// The ESP's Walker creatures (WalkerLooks.h kWalkerLooks), looked up once per
// session; g_walkerCount stays 0 without the ESP.
void FindWalkers() {
	if (g_walkersLooked) {
		return;
	}
	g_walkersLooked = true;
	g_walkerCount = 0;
	const UInt32 base = FindEspBase();
	if (base == kNoEsp) {
		g_log("Walkers: no ESP, the fallen get up as themselves");
		return;
	}
	for (const WalkerLook& look : kWalkerLooks) {
		const UInt32 formId = EspFormId(base, look.localId);
		const UInt32 form = LookupForm(formId);
		if (LooksLikeObject(form) && Read8(form + kFormTypeOffset) == kFormTypeCreature) {
			g_walkerFamilies[g_walkerCount] = look.family;
			g_walkerHeadless[g_walkerCount] = (Read(form + kActorBaseFlagsOffset) & kActorBaseNoHead) != 0;
			g_walkers[g_walkerCount++] = formId;
		} else {
			g_log("Walkers: %08X is no creature (%s)", formId, LooksLikeObject(form) ? "another type" : "no form");
		}
	}
	g_log("Walkers: %s is at %08X (%s), %u walker creatures found", kPluginEsp, base,
	      (base >> 24) == 0xFE ? "a light plugin, OblivionESL" : "a regular plugin", g_walkerCount);
	ConfigureHorde(g_settings.horde, base, g_log);
	ConfigureGuards(g_settings.guards, g_log);
	ConfigureNews(g_settings.news, base, g_log);
	ConfigureGrabIdles(base);
}

// Maskar's Oblivion Overhaul's zombie token and brain, once a session: the
// forms at MOO's load index, each of the type its ESP gives it - or none.
void FindMoo() {
	if (g_mooLooked) {
		return;
	}
	g_mooLooked = true;
	g_mooLoaded = false;
	g_levelingLoaded = false;
	g_levelingIndex = 0x100;
	g_linkLoaded = false;
	g_obXpLoaded = false;
	g_obXpIndex = 0x100;
	g_mooToken = 0;
	g_mooBrain = 0;
	const UInt32 handler = Read(kDataHandlerPointer);
	const UInt32 count = LooksLikeObject(handler) ? Read(handler + kDataHandlerModCountOffset) : 0;
	UInt32 index = 0x100;
	for (UInt32 i = 0; i < count && i < 0xFF; ++i) {
		const char* name = ModNameAt(i);
		if (name != nullptr && SameModName(name, kMooEsp) && index > 0xFF) {
			index = i;
		}
		if (name != nullptr && SameModName(name, kObXpEsp)) {
			g_obXpLoaded = true;
			g_obXpIndex = i;
		}
		if (name != nullptr && SameModName(name, kLinkEsp)) {
			g_linkLoaded = true;
		}
		if (name != nullptr && SameModName(name, kLevelingEsp)) {
			g_levelingLoaded = true;
			g_levelingIndex = i;
		}
	}
	g_log("Leveling: %s %s; %s %s (NoLevelingXP=%d)", kLevelingEsp,
	      g_levelingLoaded ? "is loaded - a dead walker's Confidence and Aggression go to 0, so it gives no experience"
	                       : "is not loaded",
	      kObXpEsp,
	      g_obXpLoaded ? "is loaded - a dead walker's eight attributes go to 0, so it gives 0 XP" : "is not loaded",
	      g_settings.noLevelingXp ? 1 : 0);
	if (index > 0xFF) {
		g_log("MOO: %s is not loaded - nothing to keep off the walkers, the walkers' MOO looks unused", kMooEsp);
		return;
	}
	g_mooLoaded = true;
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

// Every walker of every one of the ESP's bases on the process lists, in one
// walk of them (RiseLogic.h KeepWalkers).
UInt32 CollectAllWalkers(UInt32* out, UInt32 most) {
	auto* const manager = reinterpret_cast<void*>(kActorProcessManager);
	using ListFn = void*(__fastcall*)(void* self, void* edx, UInt32 level);
	struct Node {
		void* data;
		Node* next;
	};
	static UInt32 ids[kKeepWalkersMost];
	static UInt32 bases[kKeepWalkersMost];
	UInt32 n = 0;
	for (UInt32 level = 0; level < 4 && n < kKeepWalkersMost; ++level) {
		auto* node = static_cast<Node*>(reinterpret_cast<ListFn>(kActorListByLevel)(manager, nullptr, level));
		for (UInt32 visited = 0; node != nullptr && visited < 1024 && n < kKeepWalkersMost; ++visited) {
			if (!LooksLikeObject(reinterpret_cast<UInt32>(node))) {
				break;
			}
			const UInt32 actor = reinterpret_cast<UInt32>(node->data);
			node = node->next;
			if (!LooksLikeObject(actor) || Read(actor) != kVtblCreature) {
				continue;
			}
			ids[n] = Read(actor + kFormIdOffset);
			bases[n++] = FormIdOf(Read(actor + kRefBaseFormOffset));
		}
	}
	return KeepWalkers(ids, bases, n, g_walkers, g_walkerCount, out, most);
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
	if (g_settings.cloneBase && !IsWalkerRef(ref)) {
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
	if (IsWalkerRef(ref)) {
		RaiseAsSelf(refId);
		return;
	}
	if (g_settings.asCreature) {
		FindWalkers();
		FindMoo();
		UInt32 raceId = 0;
		const UInt32 base = Read(ref + kRefBaseFormOffset);
		if (LooksLikeObject(base) && Read8(base + kFormTypeOffset) == kFormTypeNpc) {
			const UInt32 race = Read(base + kNpcRacePointerOffset);
			raceId = LooksLikeObject(race) ? Read(race + kFormIdOffset) : 0;
		}
		static UInt32 candidates[2 * kWalkerCount];
		const UInt32 candidateCount =
			RiseCandidates(g_walkers, g_walkerFamilies, g_walkerHeadless, g_walkerCount, raceId, g_mooLoaded, candidates,
			               SpawnsHeadless(g_settings.end.dieOnlyBeheaded));
		const UInt32 walker = PickWalker(candidates, candidateCount, NextRandom());
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
			g_log("Rise: %08X (base %08X, race %08X) - walker %08X, one of %u looks for him (MOO %d), asked for at "
			      "%.0f %.0f %.0f (xOBSE answered %d, %u of that base known before); %u waiting",
			      refId, FormIdOf(Read(ref + kRefBaseFormOffset)), raceId, walker, candidateCount, g_mooLoaded ? 1 : 0,
			      g_swap.x, g_swap.y, g_swap.z, answer ? 1 : 0, g_swap.knownCount, g_list.count);
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
	g_hitWatchOn = g_events->RegisterEvent("OnHit", &OnHit, nullptr, nullptr, nullptr);
	g_log("Hit watch %s (the OnHit handler, for NoSkillGain)", g_hitWatchOn ? "on" : "REFUSED");
	InstallSkillHook();
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
// All walkers' runs turned to walks since the last summary, and the
// summaries still to log this session.
UInt32 g_walkTurned = 0;
UInt32 g_walkSummariesLeft = 300;
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
	static UInt32 refs[kHordeCandidatesMost];
	const UInt32 count = CollectAllWalkers(refs, kHordeCandidatesMost);
	UInt32 high = 0;
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
		++high;
		UInt16* const flags = reinterpret_cast<UInt16*>(process + kMovementFlagsOffset);
		const UInt16 before = *flags;
		if (g_settings.walkOnly) {
			*flags = WalkInsteadOfRun(before);
		}
		g_walkTurned += before != *flags ? 1u : 0u;
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
	if (logNow && g_walkSummariesLeft > 0) {
		--g_walkSummariesLeft;
		g_log("Walk: %u walkers on the process lists, %u of them in high process kept walking; %u runs turned to "
		      "walks in the last 2 s, all walkers",
		      count, high, g_walkTurned);
		g_walkTurned = 0;
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
		RenameZombies(g_walkers, g_walkerCount, FindEspBase(), g_settings, g_log);
		if (g_replace.enabled) {
			ReplaceSpawns(FindEspBase(), g_replace, g_iniPath, g_mooLoaded, g_log);
		}
		return;
	}
	if (g_spawnsAgainSeconds < 0.0f) {
		return;
	}
	g_spawnsAgainSeconds -= dt;
	if (g_spawnsAgainSeconds <= 0.0f) {
		g_spawnsAgainSeconds = -1.0f;
		ReplaceSpawns(FindEspBase(), g_replace, g_iniPath, g_mooLoaded, g_log);
	}
}

// BlockMooResurrect: the dead walkers due a pass lose MOO's token and brain
// (RemoveItem, the console's own; a count of 1 is what MOO gives).
// LINK's two files, Data\OblivionWalkers.cfg and .ini, written at the start
// from the plugin INI's values (LinkCfgText, LinkIniText) - beside the
// ESP, where LINK looks for them. The plugin INI is Data\OBSE\Plugins\.
void WriteLinkFiles() {
	char data[260];
	std::snprintf(data, sizeof(data), "%s", g_iniPath);
	for (int up = 0; up < 3; ++up) {
		char* slash = std::strrchr(data, '\\');
		if (slash == nullptr) {
			g_log("LINK: no Data folder above %s - its files not written", g_iniPath);
			return;
		}
		*slash = '\0';
	}
	static char text[1 << 16];
	const char* names[2] = {"OblivionWalkers.cfg", "OblivionWalkers.ini"};
	for (int f = 0; f < 2; ++f) {
		const UInt32 length = f == 0 ? LinkCfgText(text, sizeof(text)) : LinkIniText(g_settings, g_replace, text, sizeof(text));
		char path[300];
		std::snprintf(path, sizeof(path), "%s\\%s", data, names[f]);
		FILE* file = length > 0 ? std::fopen(path, "wb") : nullptr;
		const bool written = file != nullptr && std::fwrite(text, 1, length, file) == length;
		if (file != nullptr) {
			std::fclose(file);
		}
		g_log("LINK: %s %s (%u bytes)", path, written ? "written" : "NOT written", length);
	}
}

// What a change in LINK's menu needs beyond the value (LinkEffect).
void ApplyLinkEffect(LinkEffect effect) {
	switch (effect) {
		case LinkEffect::WalkerRecords:
			ScaleWalkerHealth(g_walkers, g_walkerCount, g_settings.healthPercent, g_log);
			ScaleWalkerSpeed(g_walkers, g_walkerCount, g_settings.speedMultiplier, g_settings.speedRandomPercent, g_log);
			break;
		case LinkEffect::SpawnCounts:
			RewriteVariantCounts(FindEspBase(), g_replace, g_log);
			break;
		case LinkEffect::Horde:
			ConfigureHorde(g_settings.horde, FindEspBase(), g_log);
			ConfigureGuards(g_settings.guards, g_log);
			ConfigureNews(g_settings.news, FindEspBase(), g_log);
			break;
		case LinkEffect::Spawns:
			ReplaceSpawns(FindEspBase(), g_replace, g_iniPath, g_mooLoaded, g_log);
			break;
		default:
			break;
	}
}

// Once a second in a game: at the first look after a load, StartOf decides
// whether the plugin INI's values go into the quest's variables or LINK's
// (the save's) into the settings; after that, with LINK, what the menu
// changed is applied.
void StepLink(float dt) {
	g_linkClock += dt;
	if (g_linkClock < 1.0f || g_walkerCount == 0) {
		return;
	}
	g_linkClock = 0.0f;
	if (!g_exceptionsSent || g_exceptionsSetting != g_settings.noSkillGain) {
		SendSkillExceptions();
	}
	const UInt32 quest = EspFormId(FindEspBase(), kLinkQuestLocalId);
	double* version = QuestVariable(quest, kLinkVersionVariable);
	if (version == nullptr) {
		return;
	}
	if (!g_linkSynced) {
		g_linkSynced = true;
		if (StartOf(g_linkLoaded, *version) == LinkStart::WriteIni) {
			UInt32 written = 0;
			for (UInt32 i = 0; i < kLinkSettingCount; ++i) {
				double* var = QuestVariable(quest, kLinkSettings[i].var);
				if (var != nullptr) {
					*var = LinkValue(g_settings, g_replace, i);
					++written;
				}
			}
			const double was = *version;
			*version = kLinkVersion;
			g_log("LINK: %s - the plugin INI's values written into the quest's %u of %u variables (version %.0f was %.0f)",
			      g_linkLoaded ? "loaded, the save had none of this version" : "not loaded, the plugin INI decides",
			      written, kLinkSettingCount, kLinkVersion, was);
			return;
		}
		g_log("LINK: loaded - the save's values decide");
	}
	if (!g_linkLoaded) {
		return;
	}
	bool effects[kLinkEffectCount] = {};
	for (UInt32 i = 0; i < kLinkSettingCount; ++i) {
		const double* var = QuestVariable(quest, kLinkSettings[i].var);
		if (var == nullptr || !SetLinkValue(g_settings, g_replace, i, *var)) {
			continue;
		}
		const LinkEffect effect = kLinkSettings[i].effect;
		effects[static_cast<UInt32>(effect)] = true;
		g_log("LINK: %s now %.*f%s", kLinkSettings[i].var, static_cast<int>(kLinkSettings[i].decimals),
		      LinkValue(g_settings, g_replace, i), effect == LinkEffect::NextStart ? " - takes effect at the next start" : "");
	}
	for (UInt32 e = 0; e < kLinkEffectCount; ++e) {
		if (effects[e]) {
			ApplyLinkEffect(static_cast<LinkEffect>(e));
		}
	}
}

// The kills by the player a second ago (Ultimate Leveling gives the
// experience through SetStage ULVL 10, whose result script may run later than
// the handler): ULVL.XP now against before - a walker's should be unchanged
// with NoLevelingXP, anyone else's grown.
void StepLevelingChecks(float dt) {
	if (g_levelingCheckCount == 0) {
		g_levelingCheckWait = 0.0f;
		return;
	}
	g_levelingCheckWait += dt;
	if (g_levelingCheckWait < 1.0f) {
		return;
	}
	g_levelingCheckWait = 0.0f;
	const double* xp = QuestVariable((g_levelingIndex << 24) | kLevelingQuestLocalId, kLevelingXpVariable);
	const double* obXp = QuestVariable((g_obXpIndex << 24) | kObXpQuestLocalId, kObXpXpVariable);
	for (UInt32 i = 0; i < g_levelingCheckCount && g_levelingChecksLeft > 0; ++i, --g_levelingChecksLeft) {
		const LevelingCheck& check = g_levelingChecks[i];
		if (g_levelingLoaded) {
			g_log("Leveling: %08X (%s) killed by the player - Ultimate Leveling's XP %.0f before, %.0f after%s",
			      check.refId, check.walker ? "a walker" : "not a walker", check.before, xp != nullptr ? *xp : -1.0,
			      xp == nullptr ? " (ULVL.XP not found)" : "");
		}
		if (g_obXpLoaded) {
			g_log("Leveling: %08X (%s) killed by the player - Oblivion XP's currentXP %.2f before, %.2f after%s",
			      check.refId, check.walker ? "a walker" : "not a walker", check.obXpBefore,
			      obXp != nullptr ? *obXp : -1.0, obXp == nullptr ? " (ObXPMain.currentXP not found)" : "");
		}
	}
	g_levelingCheckCount = 0;
}

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

// The horde's candidates (Horde.h): every walker reference of the ESP's
// bases on the process lists, alive or not, the one in a swap marked busy.
UInt32 HordeCandidates(HordeCandidate* out, UInt32 most) {
	UInt32 count = 0;
	static UInt32 ids[kHordeCandidatesMost];
	const UInt32 found = CollectAllWalkers(ids, most < kHordeCandidatesMost ? most : kHordeCandidatesMost);
	for (UInt32 i = 0; i < found; ++i) {
		const UInt32 ref = LookupForm(ids[i]);
		if (!LooksLikeObject(ref) || !IsActor(ref)) {
			continue;
		}
		HordeCandidate& c = out[count++];
		c.ref = ref;
		c.alive = !IsDead(ref);
		c.busy = g_swap.active && ids[i] == g_swap.zombieId;
	}
	return count;
}

// The grab's test switch (GrabLogic.h testGrabSeconds): the living walker
// nearest to the player within `radius`, in the same interior or
// worldspace; 0 when there is none.
UInt32 NearestWalkerToPlayer(float radius) {
	static HordeCandidate walkers[kHordeCandidatesMost];
	const UInt32 count = HordeCandidates(walkers, kHordeCandidatesMost);
	const UInt32 player = LookupForm(0x14);
	UInt32 best = 0;
	float bestDistance = radius;
	for (UInt32 i = 0; i < count && LooksLikeObject(player); ++i) {
		if (!walkers[i].alive || walkers[i].busy) {
			continue;
		}
		const float d = NearestWalkerDistance(player, &walkers[i], 1);
		if (d >= 0.0f && d < bestDistance) {
			bestDistance = d;
			best = walkers[i].ref;
		}
	}
	return best;
}

// The grab's struggle (GrabLogic.h StrugglePressesNeeded): the living
// walkers within `radius` of the player, in the same interior or worldspace.
UInt32 WalkersNearPlayer(float radius) {
	static HordeCandidate walkers[kHordeCandidatesMost];
	const UInt32 count = HordeCandidates(walkers, kHordeCandidatesMost);
	const UInt32 player = LookupForm(0x14);
	UInt32 within = 0;  // not "near": a Windows macro
	for (UInt32 i = 0; i < count && LooksLikeObject(player); ++i) {
		if (!walkers[i].alive) {
			continue;
		}
		const float d = NearestWalkerDistance(player, &walkers[i], 1);
		within += d >= 0.0f && d <= radius ? 1u : 0u;
	}
	return within;
}

// 4.17: the heads OBVR reported since the last frame tick - a walker lying to
// rise again leaves the list for good (its corpse now lootless, its end
// counted for fame - the player cut it), a living one is killed (its death
// finds the head noted).
void StepHeads() {
	for (UInt32 i = 0; i < g_headsDueCount; ++i) {
		const UInt32 refId = g_headsDue[i];
		const UInt32 ref = LookupForm(refId);
		const bool walker = LooksLikeObject(ref) && IsWalkerRef(ref);
		const bool waiting = FindWaiting(g_list, refId) >= 0;
		const HeadAction action = HeadActionFor(walker, waiting, walker && IsDead(ref));
		if (action == HeadAction::EndWaiting) {
			Unlist(g_list, refId);
			if (walker && BlocksLoot(g_settings.noLoot, true, IsRisen(g_risenSet, refId)) && g_lootlessCount < 32) {
				g_lootless[g_lootlessCount++] = refId;
			}
			if (walker && g_settings.famePerKills > 0 && CountWalkerKill(g_fame, g_settings.famePerKills)) {
				++g_famePending;
			}
			g_log("End: %s %08X ended - its head is off, it gets up no more; %u waiting", walker ? "walker" : "the dead",
			      refId, g_list.count);
		} else if (action == HeadAction::Kill) {
			const bool killed = ConsoleOn(ref, "Kill");
			g_log("End: walker %08X lost its head alive - killed (handed to xOBSE: %d)", refId, killed ? 1 : 0);
		} else if (action == HeadAction::Ignore) {
			g_log("End: %08X is no walker and waits for no rise - noted, its death keeps it down", refId);
		}
	}
	g_headsDueCount = 0;
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

// The safeguard: once a second, the victims dead AfterSeconds, loaded, with
// no living walker within Radius get up where they fell - the engine's own
// raise, as RaiseAsSelf calls it, with the get-up animation.
void StepSafeguardInGame(float dt) {
	if (g_unclaimedCount > 0) {
		static HordeCandidate around[kHordeCandidatesMost];  // not `near`: a Windows macro
		const UInt32 aroundCount = HordeCandidates(around, kHordeCandidatesMost);
		for (UInt32 i = 0; i < g_unclaimedCount; ++i) {
			const UInt32 ref = LookupForm(g_unclaimed[i]);
			const bool walkersThere = LooksLikeObject(ref) && IsActor(ref) &&
			                          WalkerWithin(ref, kSafeguardNearUnits, around, aroundCount);
			const bool noted = walkersThere && NoteVictim(g_safeguard, g_unclaimed[i]);
			g_log("Safeguard: %08X died without a killer - %s (%u on the list)", g_unclaimed[i],
			      !walkersThere ? "no walker within 1000 units, not the walkers' doing"
			                    : (noted ? "a walker near: gets up again once it is safe" : "the list is full, it stays dead"),
			      g_safeguard.count);
		}
		g_unclaimedCount = 0;
	}
	g_safeguardClock += dt;
	if (g_safeguardClock < 1.0f || g_safeguard.count == 0) {
		return;
	}
	const float elapsed = g_safeguardClock;
	g_safeguardClock = 0.0f;
	static HordeCandidate walkers[kHordeCandidatesMost];
	const UInt32 walkerCount = HordeCandidates(walkers, kHordeCandidatesMost);
	const float radius = g_settings.safeguard.radius;
	g_safeguardStatusClock += elapsed;
	if (g_safeguardStatusClock >= 10.0f && g_safeguardStatusLeft > 0) {
		g_safeguardStatusClock = 0.0f;
		for (UInt32 i = 0; i < g_safeguard.count && i < 8 && g_safeguardStatusLeft > 0; ++i, --g_safeguardStatusLeft) {
			const SafeguardEntry& entry = g_safeguard.entries[i];
			const UInt32 ref = LookupForm(entry.refId);
			const bool loaded = LooksLikeObject(ref) && Read(ref + kRefNiNodeOffset) != 0 && InPlayersReach(ref);
			const float nearest = LooksLikeObject(ref) ? NearestWalkerDistance(ref, walkers, walkerCount) : -1.0f;
			g_log("Safeguard: %08X still down - dead %.0f s, %s, the nearest living walker %.0f units off (-1 none; "
			      "up once %.0f s and beyond %.0f)",
			      entry.refId, entry.seconds, loaded ? "loaded" : "not loaded", nearest, g_settings.safeguard.afterSeconds,
			      radius);
		}
	}
	UInt32 due[8];
	const UInt32 count = StepSafeguard(
		g_safeguard, g_settings.safeguard, elapsed,
		[](UInt32 id) {
			const UInt32 ref = LookupForm(id);
			if (!LooksLikeObject(ref)) {
				return VictimNow::Missing;
			}
			if (!IsActor(ref)) {
				return VictimNow::Gone;
			}
			return IsDead(ref) ? VictimNow::Dead : VictimNow::Alive;
		},
		[&](UInt32 id) {
			// Loaded and where the player is: an unloaded body - or one whose cell
			// the game only keeps in its buffer - waits for the player to come
			// back; a raise without 3D is not read yet (spec 4.11b).
			const UInt32 ref = LookupForm(id);
			return Read(ref + kRefNiNodeOffset) != 0 && InPlayersReach(ref) &&
			       !WalkerWithin(ref, radius, walkers, walkerCount);
		},
		[](UInt32 id, VictimNow now, float seconds) {
			g_log("Safeguard: %08X off the list after %.0f s dead - %s", id, seconds,
			      now == VictimNow::Alive     ? "alive again by other means (a script, a spell, another mod)"
			      : now == VictimNow::Missing ? "not held by the game for three game days (its cell reset, presumably)"
			                                  : "gone from the game (no actor any more)");
		},
		due, 8, GameDaysNow());
	for (UInt32 i = 0; i < count; ++i) {
		const UInt32 ref = LookupForm(due[i]);
		const UInt32 fn = Read(Read(ref) + kActorVtableResurrect);
		if (fn < kTextStart || fn >= kTextEnd) {
			g_log("Safeguard: %08X has no Resurrect at vtable+0x20C (%08X), left down", due[i], fn);
			continue;
		}
		const UInt32 has3D = Read(ref + kRefNiNodeOffset) != 0 ? 1u : 0u;
		using ResurrectFn = void(__fastcall*)(void* self, void* edx, UInt32 one, UInt32 has3D, UInt32 animate);
		reinterpret_cast<ResurrectFn>(fn)(reinterpret_cast<void*>(ref), nullptr, 1, has3D, 1);
		g_log("Safeguard: %08X (base %08X) gets up again at %.0f %.0f %.0f, no walker within %.0f (3D %u, dead to the "
		      "engine now: %d); %u still down",
		      due[i], FormIdOf(Read(ref + kRefBaseFormOffset)), FloatAt(ref + kRefPositionOffset),
		      FloatAt(ref + kRefPositionOffset + 4), FloatAt(ref + kRefPositionOffset + 8), radius, has3D,
		      IsDead(ref) ? 1 : 0, g_safeguard.count);
	}
}

void Tick() {
	++g_frame;
	if (!g_deathWatchOn && !TryRegisterDeath()) {
		return;
	}
	{
		const float frame = *reinterpret_cast<const float*>(kFrameSeconds);
		FindWalkers();
		FindMoo();
		StepLevelingChecks(frame > 0.0f && frame < 1.0f ? frame : 0.0f);
		StepLink(frame > 0.0f && frame < 1.0f ? frame : 0.0f);
		StepMooStripping(frame > 0.0f && frame < 1.0f ? frame : 0.0f);
		StepSpawns(frame > 0.0f && frame < 1.0f ? frame : 0.0f);
		StepWalkers(frame > 0.0f && frame < 1.0f ? frame : 0.0f);
		StepGrabInGame(frame > 0.0f && frame < 1.0f ? frame : 0.0f);
		StepHordeInGame(frame > 0.0f && frame < 1.0f ? frame : 0.0f, &HordeCandidates, &ConsoleOn, &IsDead, g_log);
		StepSafeguardInGame(frame > 0.0f && frame < 1.0f ? frame : 0.0f);
		StepNewsInGame(frame > 0.0f && frame < 1.0f ? frame : 0.0f, HordeHeadingCity(), g_log);
		StepHeads();
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
	const UInt32 dueCount = TickWaiting(g_list, dt, &PresenceOf, due, kWaitingMost, GameDaysNow());
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
	static UInt8 safe[kSafeguardImageMost];
	const UInt32 safeLength = SafeguardImage(g_safeguard, safe, sizeof(safe));
	const bool safeWritten = g_serialization->OpenRecord(kSafeguardRecordType, kSafeguardRecordVersion) &&
	                         g_serialization->WriteRecordData(safe, safeLength);
	g_log("Co-save: %u killed by walkers still to get up %s", g_safeguard.count,
	      safeWritten ? "written" : "could not be written");
	UInt8 news[kNewsImageBytes];
	const UInt32 newsLength = NewsImage(CurrentNews(), news, sizeof(news));
	const bool newsWritten = g_serialization->OpenRecord(kNewsRecordType, kNewsRecordVersion) &&
	                         g_serialization->WriteRecordData(news, newsLength);
	if (!newsWritten) {
		g_log("Co-save: the cities' attacks for the news could not be written");
	}
	g_log("Co-save: %u waiting written; %u risen walkers %s (%u gone from the game dropped); %u walker kills toward "
	      "the next point of fame %s",
	      g_list.count, g_risenSet.count, risenWritten ? "written" : "could not be written", gone, g_fame.kills,
	      fameWritten ? "written" : "could not be written");
}

bool ResolveSaved(UInt32 savedId, UInt32& loadedId) {
	return g_serialization->ResolveRefID(savedId, &loadedId);
}

void OnLoad(void*) {
	g_linkSynced = false;
	g_list = WaitingList{};
	g_risenSet = RisenSet{};
	g_fame = FameCount{};
	g_famePending = 0;
	g_lootlessCount = 0;
	g_mooStrip = MooStrip{};
	g_safeguard = SafeguardList{};
	g_safeguardClock = 0.0f;
	g_unclaimedCount = 0;
	ResetHorde();
	ResetGrabInGame();
	CurrentNews() = NewsState{};
	g_banished.clear();
	if (g_replace.enabled) {
		g_spawnsAgainSeconds = 5.0f;
	}
	UInt32 type = 0;
	UInt32 version = 0;
	UInt32 length = 0;
	while (g_serialization->GetNextRecordInfo(&type, &version, &length)) {
		if (type == kSafeguardRecordType) {
			if ((version != 1 && version != kSafeguardRecordVersion) || length > kSafeguardImageMost) {
				g_log("Co-save: a safeguard list of version %u, %u bytes: not read", version, length);
				continue;
			}
			static UInt8 safe[kSafeguardImageMost];
			const UInt32 read = g_serialization->ReadRecordData(safe, length);
			const bool whole = ReadSafeguardImage(g_safeguard, safe, read, &ResolveSaved, version);
			g_log("Co-save: %u killed by walkers still to get up read%s", g_safeguard.count,
			      whole ? "" : " (the record cut short)");
			continue;
		}
		if (type == kNewsRecordType) {
			UInt8 news[kNewsImageBytes];
			const UInt32 read =
				(version == kNewsRecordVersion || version == 1) && length <= sizeof(news)
					? g_serialization->ReadRecordData(news, length)
					: 0;
			const bool whole = ReadNewsImage(CurrentNews(), news, read);
			UInt32 attacked = 0;
			for (float days : CurrentNews().lastAttackDays) {
				attacked += days >= 0.0f ? 1u : 0u;
			}
			g_log("Co-save: %u cities' last attack for the news read, a horde last seen heading for city %d at game "
			      "day %.3f (version %u)%s",
			      attacked, CurrentNews().headingCity, CurrentNews().headingDays, version,
			      whole ? "" : " (version or length off: none)");
			continue;
		}
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
		if ((version != 1 && version != kWaitingRecordVersion) || length > kWaitingImageMost) {
			g_log("Co-save: a waiting list of version %u, %u bytes: not read", version, length);
			continue;
		}
		UInt8 image[kWaitingImageMost];
		const UInt32 read = g_serialization->ReadRecordData(image, length);
		if (!ReadWaitingImage(g_list, image, read, &ResolveSaved, version)) {
			g_log("Co-save: the waiting list is cut short (%u of %u bytes), %u read before that", read, length,
			      g_list.count);
			continue;
		}
		g_log("Co-save: %u waiting read", g_list.count);
	}
}

void OnNewGame(void*) {
	g_linkSynced = false;
	g_list = WaitingList{};
	g_risenSet = RisenSet{};
	g_fame = FameCount{};
	g_famePending = 0;
	g_lootlessCount = 0;
	g_mooStrip = MooStrip{};
	g_safeguard = SafeguardList{};
	g_safeguardClock = 0.0f;
	g_unclaimedCount = 0;
	ResetHorde();
	ResetGrabInGame();
	CurrentNews() = NewsState{};
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
	g_linkSynced = false;
	WriteLinkFiles();
	if (xse.QueryInterface == nullptr) {
		log("xOBSE offers no QueryInterface: no death watch");
		return false;
	}
	g_events = static_cast<obse::EventManagerInterface*>(xse.QueryInterface(obse::kInterface_EventManager));
	g_tasks = static_cast<obse::TasksInterface*>(xse.QueryInterface(obse::kInterface_Tasks));
	g_serialization = static_cast<obse::SerializationInterface*>(xse.QueryInterface(obse::kInterface_Serialization));
	g_console = static_cast<obse::ConsoleInterface*>(xse.QueryInterface(obse::kInterface_Console));
	g_messaging = static_cast<obse::MessagingInterface*>(xse.QueryInterface(obse::kInterface_Messaging));
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
		g_pluginHandle = plugin;
		g_serialization->SetSaveCallback(plugin, &OnSave);
		g_serialization->SetLoadCallback(plugin, &OnLoad);
		g_serialization->SetNewGameCallback(plugin, &OnNewGame);
		log("The waiting list goes to the co-save (plugin handle %u, opcode base %04X)", plugin, kOpcodeBase);
	} else {
		g_serialization = nullptr;
		log("No serialization interface: the waiting list lives for the session only");
	}
	GrabServices grab{};
	grab.console = &ConsoleOn;
	grab.dead = &IsDead;
	grab.isWalker = &IsWalkerRef;
	grab.onMooSkeleton = &OnMooSkeleton;
	grab.highProcess = &HighProcessOf;
	grab.roll = &NextRandom;
	grab.nearestWalker = &NearestWalkerToPlayer;
	grab.walkersNear = &WalkersNearPlayer;
	grab.otherObvrMessage = &OnObvrHeads;
	grab.log = log;
	ConfigureGrab(&g_settings.grab, grab, g_messaging, xse.GetPluginHandle != nullptr ? xse.GetPluginHandle() : 0);
	return true;
}

}  // namespace walkers
