#include "Grab.h"

#include "Engine.h"

#include <cmath>
#include <cstdio>

namespace walkers {
namespace {

using namespace engine;

// Engine facts of Oblivion 1.2.416 the grab uses (docs/walkers-spec.md 4.19):
constexpr UInt32 kPlayerRefId = 0x14;
constexpr UInt32 kRefRotZOffset = 0x28;       // TESObjectREFR::rotZ, radians (xOBSE GameObjects.h)
constexpr UInt32 kRefPositionOffset = 0x2C;   // posX, posY, posZ
constexpr UInt32 kRefNiNodeOffset = 0x3C;     // TESObjectREFR::niNode: in the world when set
constexpr UInt32 kMovementFlagsOffset = 0x1FC; // HighProcess, as the walk of 4.4 writes it
// The idle queue as xOBSE's PlayIdle reaches it (Cmd_PlayIdle_Execute, read
// through deepwiki on llde/xOBSE 2026-10-28): the actor's anim data from its
// virtual at +0x164 (OBVR's MeleeHits.cpp the same), then 0x00477DB0 on it,
// thiscall (idle, actor, the idle's section, 3) - its "ret 10h" read with
// llvm-objdump 2026-10-28; a section of 0 or 5 is taken as 3 there. PlayIdle
// queues only when the anim data's +0xD0 (unkC8[2]) is 0 or its +0x24
// (niNodes24[0]) is not that idle already: the same here, so asking for
// the playing idle again is nothing.
constexpr UInt32 kActorVtableAnimData = 0x164;
constexpr UInt32 kQueueIdle = 0x00477DB0;
constexpr UInt32 kAnimDataIdleOffset = 0x24;
constexpr UInt32 kAnimDataIdleActiveOffset = 0xD0;
constexpr UInt8 kIdleSection = 0x04;  // the ESP's ANAM, Special Idle (tools/esp-write.py)
// The player's health as OBVR reads it (PlayerTeleport.cpp): GetActorValue_F
// at vtable +0x288, GetBaseActorValue 0x005F1910 (thiscall), actor value 8;
// god mode's getter 0x0065D820 (cdecl, bool).
constexpr UInt32 kActorVtableActorValueF = 0x288;
constexpr UInt32 kGetBaseActorValue = 0x005F1910;
constexpr UInt32 kActorValueHealth = 8;
constexpr UInt32 kActorValueFatigue = 10;  // as OBVR's PlayerStagger reads it (GetActorValue_F 0xA)
constexpr UInt32 kGodModeGet = 0x0065D820;
// The control's state as xOBSE's IsControlPressed reads it (deepwiki on
// llde/xOBSE, GameOSDepend.h, 2026-10-28): OSInputGlobals' keyboard state
// at +0x18F4 (DIK-indexed, 0x80 down), the mouse's buttons at +0x1B2C, the
// control map's key at +0x1B7E and mouse button at +0x1B9B per control
// (0xFF none). OSInputGlobals from OSGlobals (pointer at 0x00B33398) +0x20,
// as OBVR reads it (GameAddresses.h kControlKeyName). IsMenuMode 0x00578F60
// (OBVR GameAddresses.h, xOBSE GameAPI.cpp): no struggle behind a menu.
constexpr UInt32 kOsGlobalsPointer = 0x00B33398;
constexpr UInt32 kOsGlobalsInputOffset = 0x20;
constexpr UInt32 kInputKeysOffset = 0x18F4;
constexpr UInt32 kInputMouseButtonsOffset = 0x1B2C;
constexpr UInt32 kInputKeyMapOffset = 0x1B7E;
constexpr UInt32 kInputMouseMapOffset = 0x1B9B;
constexpr UInt32 kControlCount = 29;
constexpr UInt8 kNoKey = 0xFF;
constexpr UInt32 kIsMenuMode = 0x00578F60;
constexpr UInt32 kTextStart = 0x00401000;
constexpr UInt32 kTextEnd = 0x00A27C39;
// The ESP's grab idles (tools/esp-write.py LOCAL_GRAB_IDLE_FIRST): hold,
// bite, let go beneath Creatures\Zombie, then the same beneath MOO\zombie.
constexpr UInt32 kGrabIdleLocalFirst = 0x94D;
enum GrabAnim : UInt32 { kHold = 0, kBite = 1, kLetGo = 2, kAnims = 3 };
// The bite's length (WalkersGrabBite.kf, 1.67 s, tools/grab-anims.py): the
// hold is asked for again after it.
constexpr float kBiteAnimSeconds = 1.67f;
constexpr const char* kObvr = "OBVR";

const GrabSettings* g_settings = nullptr;
GrabServices g_services{};
obse::MessagingInterface* g_messaging = nullptr;
UInt32 g_pluginHandle = 0;
UInt32 g_espIndex = 0x100;
GrabState g_state;

// The hits xOBSE's OnHit handed on, carried out at the next frame: nothing
// of an actor is touched inside the engine's hit path (the rise's rule for
// its death path).
struct PendingHit {
	UInt32 target;
	UInt32 attacker;
};
PendingHit g_hits[16];
UInt32 g_hitCount = 0;
UInt32 g_freeRequests[4];
UInt32 g_freeRequestCount = 0;

// Per holder: the bite still playing, and the clock that asks for the hold
// again (a no-op while it plays).
struct HolderAnim {
	UInt32 refId = 0;
	float biteLeft = 0.0f;
	float holdClock = 0.0f;
};
HolderAnim g_anims[kGrabHolders];

// The proof of the hold: how far the player got in the last second.
float g_moveClock = 0.0f;
float g_moveX = 0.0f;
float g_moveY = 0.0f;
PlayerHold g_lastHold = PlayerHold::Free;
UInt32 g_refusalLines = 12;
float g_testClock = 0.0f;
Struggle g_struggle;
// The presses needed now (StrugglePressesNeeded), looked at once a second
// while held - the walkers near are counted off the process lists.
UInt32 g_needed = 0;
float g_neededClock = 0.0f;
UInt32 g_walkersNear = 0;
UInt32 g_moveLines = 40;

bool InText(UInt32 fn) { return fn >= kTextStart && fn < kTextEnd; }

UInt32 Player() { return LookupForm(kPlayerRefId); }

UInt32 IdleFor(UInt32 actor, GrabAnim anim) {
	if (g_espIndex > 0xFF) {
		return 0;
	}
	const UInt32 refId = FormIdOf(actor);
	const UInt32 skeleton = g_services.onMooSkeleton != nullptr && g_services.onMooSkeleton(refId) ? 1u : 0u;
	const UInt32 form = LookupForm((g_espIndex << 24) | (kGrabIdleLocalFirst + skeleton * kAnims + anim));
	return LooksLikeObject(form) ? form : 0;
}

// The actor plays the idle (PlayIdle's way, see above); false when it
// could not be asked.
bool PlayIdle(UInt32 actor, GrabAnim anim) {
	const UInt32 idle = IdleFor(actor, anim);
	if (idle == 0 || !LooksLikeObject(actor)) {
		return false;
	}
	const UInt32 fn = Read(Read(actor) + kActorVtableAnimData);
	if (!InText(fn)) {
		return false;
	}
	using AnimDataFn = UInt32(__fastcall*)(void* self, void* edx);
	const UInt32 animData = reinterpret_cast<AnimDataFn>(fn)(reinterpret_cast<void*>(actor), nullptr);
	if (!LooksLikeObject(animData)) {
		return false;
	}
	if (Read(animData + kAnimDataIdleActiveOffset) != 0 && Read(animData + kAnimDataIdleOffset) == idle) {
		return true;
	}
	using QueueFn = void(__fastcall*)(void* self, void* edx, UInt32 idle, UInt32 actor, UInt32 section, UInt32 three);
	reinterpret_cast<QueueFn>(kQueueIdle)(reinterpret_cast<void*>(animData), nullptr, idle, actor, kIdleSection, 3);
	return true;
}

float ValueOf(UInt32 actor, UInt32 av) {
	const UInt32 fn = Read(Read(actor) + kActorVtableActorValueF);
	if (!InText(fn)) {
		return -1.0f;
	}
	using GetFn = float(__fastcall*)(void* self, void* edx, UInt32 av);
	return reinterpret_cast<GetFn>(fn)(reinterpret_cast<void*>(actor), nullptr, av);
}

float BaseValueOf(UInt32 actor, UInt32 av) {
	using GetBaseFn = SInt32(__fastcall*)(void* self, void* edx, UInt32 av);
	return static_cast<float>(reinterpret_cast<GetBaseFn>(kGetBaseActorValue)(reinterpret_cast<void*>(actor), nullptr, av));
}

float HealthOf(UInt32 actor) { return ValueOf(actor, kActorValueHealth); }
float BaseHealthOf(UInt32 actor) { return BaseValueOf(actor, kActorValueHealth); }

bool GodMode() {
	using GetFn = bool(__cdecl*)();
	return reinterpret_cast<GetFn>(kGodModeGet)();
}

const char* HoldName(PlayerHold hold) {
	return hold == PlayerHold::Rooted ? "rooted" : (hold == PlayerHold::Slowed ? "slowed to a walk" : "free");
}

// A line on the screen (Messages), as the console's Message prints it.
void Say(GrabSay say, Arm arm, UInt32 bite) {
	char text[128];
	if (!GrabText(say, arm, bite, g_needed, *g_settings, text, sizeof(text))) {
		return;
	}
	char line[160];
	std::snprintf(line, sizeof(line), "Message \"%s\"", text);
	g_services.console(Player(), line);
}

// Whether the struggle's control is down now, on the keyboard or the mouse.
bool ControlDown(UInt32 control) {
	const UInt32 globals = Read(kOsGlobalsPointer);
	const UInt32 input = LooksLikeObject(globals) ? Read(globals + kOsGlobalsInputOffset) : 0;
	if (!LooksLikeObject(input) || control >= kControlCount) {
		return false;
	}
	const UInt8 key = Read8(input + kInputKeyMapOffset + control);
	const UInt8 button = Read8(input + kInputMouseMapOffset + control);
	return (key != kNoKey && (Read8(input + kInputKeysOffset + key) & 0x80) != 0) ||
	       (button < 8 && (Read8(input + kInputMouseButtonsOffset + button) & 0x80) != 0);
}

bool MenuMode() {
	using MenuFn = bool(__cdecl*)();
	return reinterpret_cast<MenuFn>(kIsMenuMode)();
}

// The presses needed now: the walkers near, the player's fatigue.
void UpdateNeeded(UInt32 player) {
	g_walkersNear = g_services.walkersNear != nullptr ? g_services.walkersNear(g_settings->struggleCrowdRadius) : 1;
	const float fatigue = ValueOf(player, kActorValueFatigue);
	const float base = BaseValueOf(player, kActorValueFatigue);
	g_needed = StrugglePressesNeeded(*g_settings, g_walkersNear, fatigue, base);
}

void SendState() {
	if (g_messaging == nullptr) {
		return;
	}
	UInt32 words[kGrabMessageWords];
	const UInt32 bytes = GrabMessage(g_state, words);
	g_messaging->Dispatch(g_pluginHandle, kGrabStateMessage, words, bytes, kObvr);
}

HolderAnim* AnimOf(UInt32 refId) {
	for (HolderAnim& a : g_anims) {
		if (a.refId == refId) {
			return &a;
		}
	}
	for (HolderAnim& a : g_anims) {
		if (a.refId == 0) {
			a = HolderAnim{};
			a.refId = refId;
			return &a;
		}
	}
	return nullptr;
}

void ForgetAnim(UInt32 refId) {
	for (HolderAnim& a : g_anims) {
		if (a.refId == refId) {
			a = HolderAnim{};
		}
	}
}

// A holder let go: it reels back (the let-go idle ends the hold's loop).
void LetGoOf(UInt32 refId, LetGo why) {
	ForgetAnim(refId);
	const UInt32 actor = LookupForm(refId);
	const bool played = why != LetGo::Gone && why != LetGo::Dead && LooksLikeObject(actor) && PlayIdle(actor, kLetGo);
	g_services.log("Grab: walker %08X lets go - %s (the let-go played: %d); %u still hold, the player %s", refId,
	               LetGoName(why), played ? 1 : 0, g_state.count, HoldName(HoldOf(g_state)));
	Say(SayFor(why), Arm::None, 0);
}

void HandleHit(const PendingHit& hit, UInt32 player) {
	if (hit.target == player && LooksLikeObject(hit.attacker) && g_services.isWalker(hit.attacker) &&
	    !g_services.dead(hit.attacker)) {
		const float dx = FloatAt(hit.attacker + kRefPositionOffset) - FloatAt(player + kRefPositionOffset);
		const float dy = FloatAt(hit.attacker + kRefPositionOffset + 4) - FloatAt(player + kRefPositionOffset + 4);
		const Arm side = SideOf(FloatAt(player + kRefRotZOffset), dx, dy);
		const UInt32 refId = FormIdOf(hit.attacker);
		Arm arm = Arm::None;
		const UInt32 roll = g_services.roll() % 100;
		const GrabRefusal r = TryGrab(g_state, *g_settings, refId, side, roll, &arm);
		if (r != GrabRefusal::None) {
			if (g_refusalLines > 0 && r != GrabRefusal::AlreadyHolding) {
				--g_refusalLines;
				g_services.log("Grab: walker %08X's hit on the player - no grab (%s; rolled %u of %u)", refId,
				               RefusalName(r), roll, g_settings->chancePercent);
			}
			return;
		}
		const bool played = PlayIdle(hit.attacker, kHold);
		UpdateNeeded(player);
		Say(g_state.count >= 2 ? GrabSay::Rooted : GrabSay::Grabbed, arm, 0);
		g_services.log("Grab: walker %08X holds the player's %s arm (it stood on the %s, rolled %u of %u; the hold "
		               "played: %d); %u hold, the player %s; bites so far %u of %u",
		               refId, ArmName(arm), ArmName(side), roll, g_settings->chancePercent, played ? 1 : 0,
		               g_state.count, HoldName(HoldOf(g_state)), g_state.bites, g_settings->bitesToDie);
		SendState();
		return;
	}
	if (hit.attacker == player && LooksLikeObject(hit.target)) {
		const UInt32 refId = FormIdOf(hit.target);
		if (!Holds(g_state, refId)) {
			return;
		}
		if (OnStruck(g_state, *g_settings, refId)) {
			LetGoOf(refId, LetGo::Struck);
			SendState();
			return;
		}
		g_services.log("Grab: the player strikes holder %08X - not yet free (StruggleHits %u)", refId,
		               g_settings->struggleHits);
	}
}

void Bite(const GrabEvent& ev, UInt32 player) {
	const UInt32 actor = LookupForm(ev.refId);
	if (LooksLikeObject(actor)) {
		PlayIdle(actor, kBite);
	}
	if (HolderAnim* a = AnimOf(ev.refId)) {
		a->biteLeft = kBiteAnimSeconds;
	}
	Say(ev.lethal ? GrabSay::LastBite : GrabSay::Bitten, ev.arm, ev.biteNumber);
	const float now = HealthOf(player);
	const float base = BaseHealthOf(player);
	const bool god = GodMode();
	bool kills = false;
	const float take = BiteDamage(*g_settings, base, now, ev.lethal, god, &kills);
	char line[64];
	bool done = false;
	if (kills) {
		std::snprintf(line, sizeof(line), "Kill %08X", ev.refId);
		done = g_services.console(player, line);
	} else if (take >= 1.0f) {
		std::snprintf(line, sizeof(line), "ModAV2 Health -%d", static_cast<int>(take));
		done = g_services.console(player, line);
	}
	g_services.log("Grab: walker %08X bites the player's %s arm - bite %u of %u%s; health %.1f of %.0f, %s%s (\"%s\" "
	               "handed to xOBSE: %d)",
	               ev.refId, ArmName(ev.arm), ev.biteNumber, g_settings->bitesToDie, ev.lethal ? ", the last" : "",
	               now, base, kills ? "it kills" : "taken ", kills ? "" : (take >= 1.0f ? "some" : "nothing"),
	               kills || take >= 1.0f ? line : "-", done ? 1 : 0);
	if (!kills && take >= 1.0f) {
		g_services.log("Grab: the bite took %d, the player's health now %.1f%s", static_cast<int>(take),
		               HealthOf(player), god ? " (god mode)" : "");
	}
}

// The test switch: the nearest walker moved before the player (the player's
// heading, 62 units on: the grip's hands about 45 ahead of it meet the body), turned to face the player, and grabbing as at a hit.
void TestGrab(UInt32 player) {
	const UInt32 walker = g_services.nearestWalker != nullptr ? g_services.nearestWalker(4000.0f) : 0;
	if (walker == 0) {
		g_services.log("Grab: test - no living walker within 4000 units");
		return;
	}
	const float heading = FloatAt(player + kRefRotZOffset);
	char line[96];
	std::snprintf(line, sizeof(line), "MoveTo player %.0f %.0f 0", 62.0f * std::sin(heading), 62.0f * std::cos(heading));
	const bool moved = g_services.console(walker, line);
	std::snprintf(line, sizeof(line), "SetAngle z %.1f", heading * 57.29578f + 180.0f);
	const bool turned = g_services.console(walker, line);
	const UInt32 refId = FormIdOf(walker);
	Arm arm = Arm::None;
	const GrabRefusal r = TryGrab(g_state, *g_settings, refId, Arm::Right, 0, &arm);
	const bool played = r == GrabRefusal::None && PlayIdle(walker, kHold);
	g_services.log("Grab: test - walker %08X moved before the player (%d) and turned to face it (%d): %s the %s arm "
	               "(the hold played: %d)",
	               refId, moved ? 1 : 0, turned ? 1 : 0, RefusalName(r), ArmName(arm), played ? 1 : 0);
	if (r == GrabRefusal::None) {
		SendState();
	}
}

void ReleaseAll(LetGo why) {
	while (g_state.count > 0) {
		const UInt32 refId = g_state.holders[0].refId;
		ReleaseHolder(g_state, refId);
		LetGoOf(refId, why);
	}
	SendState();
}

void OnObvrMessage(obse::MessagingInterface::Message* message) {
	if (message == nullptr || message->type != kGrabFreeMessage) {
		return;
	}
	UInt32 refId = 0;
	if (FreeRequestOf(message->data, message->dataLen, refId) && g_freeRequestCount < 4) {
		g_freeRequests[g_freeRequestCount++] = refId;
	}
}

void HoldFlags(UInt32 player, float dt) {
	const PlayerHold hold = HoldOf(g_state);
	for (UInt32 i = 0; i < g_state.count; ++i) {
		const UInt32 actor = LookupForm(g_state.holders[i].refId);
		const UInt32 process = LooksLikeObject(actor) ? g_services.highProcess(actor) : 0;
		if (process != 0) {
			UInt16* const flags = reinterpret_cast<UInt16*>(process + kMovementFlagsOffset);
			*flags = HolderFlags(*flags);
		}
	}
	const UInt32 process = g_services.highProcess(player);
	if (process != 0 && hold != PlayerHold::Free) {
		UInt16* const flags = reinterpret_cast<UInt16*>(process + kMovementFlagsOffset);
		*flags = HeldPlayerFlags(*flags, hold);
	}
	// The proof in the log: the player's ground covered per second while held.
	const float x = FloatAt(player + kRefPositionOffset);
	const float y = FloatAt(player + kRefPositionOffset + 4);
	if (hold != g_lastHold) {
		g_lastHold = hold;
		g_moveClock = 0.0f;
		g_moveX = x;
		g_moveY = y;
		return;
	}
	if (hold == PlayerHold::Free) {
		return;
	}
	g_moveClock += dt;
	if (g_moveClock >= 1.0f) {
		const float moved = std::sqrt((x - g_moveX) * (x - g_moveX) + (y - g_moveY) * (y - g_moveY));
		if (g_moveLines > 0) {
			--g_moveLines;
			g_services.log("Grab: the player %s moved %.0f units in the last %.1f s (flags now %04X)", HoldName(hold),
			               moved, g_moveClock,
			               process != 0 ? *reinterpret_cast<UInt16*>(process + kMovementFlagsOffset) : 0);
		}
		g_moveClock = 0.0f;
		g_moveX = x;
		g_moveY = y;
	}
}

}  // namespace

void ConfigureGrab(const GrabSettings* settings, const GrabServices& services, obse::MessagingInterface* messaging,
                   UInt32 pluginHandle) {
	g_settings = settings;
	g_services = services;
	g_messaging = messaging;
	g_pluginHandle = pluginHandle;
	ResetGrabInGame();
	bool listening = false;
	if (g_messaging != nullptr && g_messaging->RegisterListener != nullptr) {
		listening = g_messaging->RegisterListener(pluginHandle, kObvr, &OnObvrMessage);
	}
	g_services.log("Grab: [Grab] Enabled=%d ChancePercent=%u BitesToDie=%u BiteSeconds=%.1f BiteHealthPercent=%u "
	               "StruggleHits=%u TearFreeDistance=%.0f BiteResetSeconds=%.0f TestGrabSeconds=%.0f StrugglePresses=%u "
	               "StruggleCrowdStep=%.2f StruggleCrowdMost=%.1f StruggleCrowdRadius=%.0f StruggleGapSeconds=%.1f "
	               "StruggleFatigue=%.0f Messages=%d; OBVR's free requests %s",
	               settings->enabled ? 1 : 0, settings->chancePercent, settings->bitesToDie, settings->biteSeconds,
	               settings->biteHealthPercent, settings->struggleHits, settings->tearFreeDistance,
	               settings->biteResetSeconds, settings->testGrabSeconds, settings->strugglePresses,
	               settings->struggleCrowdStep, settings->struggleCrowdMost, settings->struggleCrowdRadius,
	               settings->struggleGapSeconds, settings->struggleFatigue, settings->messages ? 1 : 0,
	               listening ? "listened to" : "not listened to");
}

void ConfigureGrabIdles(UInt32 espIndex) {
	g_espIndex = espIndex;
	UInt32 found = 0;
	for (UInt32 i = 0; i < 2 * kAnims && espIndex <= 0xFF; ++i) {
		const UInt32 form = LookupForm((espIndex << 24) | (kGrabIdleLocalFirst + i));
		found += LooksLikeObject(form) ? 1u : 0u;
	}
	g_services.log("Grab: %u of %u grab idles found in the ESP", found, 2 * kAnims);
}

void GrabOnHit(UInt32 target, UInt32 attacker) {
	if (g_settings == nullptr || !g_settings->enabled || g_hitCount >= 16) {
		return;
	}
	g_hits[g_hitCount++] = PendingHit{target, attacker};
}

void StepGrabInGame(float dt) {
	if (g_settings == nullptr) {
		return;
	}
	const UInt32 player = Player();
	if (!LooksLikeObject(player)) {
		g_hitCount = 0;
		return;
	}
	for (UInt32 i = 0; i < g_hitCount; ++i) {
		HandleHit(g_hits[i], player);
	}
	g_hitCount = 0;
	for (UInt32 i = 0; i < g_freeRequestCount; ++i) {
		if (ReleaseHolder(g_state, g_freeRequests[i])) {
			LetGoOf(g_freeRequests[i], LetGo::Asked);
			SendState();
		}
	}
	g_freeRequestCount = 0;
	if (!g_settings->enabled && g_state.count > 0) {
		ReleaseAll(LetGo::SwitchedOff);
	}
	// Flat: mashing the Block key wrenches the longest holder off.
	bool pressed = false;
	const bool down = g_state.count > 0 && !MenuMode() && ControlDown(g_settings->struggleControl);
	g_neededClock += dt;
	if (g_state.count > 0 && (g_needed == 0 || g_neededClock >= 1.0f)) {
		g_neededClock = 0.0f;
		UpdateNeeded(player);
	}
	if (StepStruggle(g_struggle, *g_settings, g_needed, down, dt, g_state.count > 0, &pressed)) {
		const UInt32 refId = g_state.holders[0].refId;
		ReleaseHolder(g_state, refId);
		LetGoOf(refId, LetGo::Struggled);
		SendState();
	}
	if (pressed && g_settings->struggleFatigue >= 1.0f) {
		char line[48];
		std::snprintf(line, sizeof(line), "ModAV2 Fatigue -%d", static_cast<int>(g_settings->struggleFatigue));
		g_services.console(player, line);
		g_services.log("Grab: the player struggles (control %u pressed, %u of %u - %u walkers near, fatigue %.0f of %.0f)%s",
		               g_settings->struggleControl, g_struggle.bar, g_needed, g_walkersNear,
		               ValueOf(player, kActorValueFatigue), BaseValueOf(player, kActorValueFatigue),
		               g_struggle.bar == 0 ? " - free of one" : "");
	}
	if (TestGrabDue(*g_settings, g_testClock, dt, g_state.count > 0)) {
		TestGrab(player);
	}
	HolderView views[kGrabHolders];
	const float px = FloatAt(player + kRefPositionOffset);
	const float py = FloatAt(player + kRefPositionOffset + 4);
	for (UInt32 i = 0; i < g_state.count; ++i) {
		const UInt32 actor = LookupForm(g_state.holders[i].refId);
		const bool found = LooksLikeObject(actor) && Read(actor + kRefNiNodeOffset) != 0;
		views[i].found = found;
		views[i].dead = found && g_services.dead(actor);
		views[i].distance = found ? std::sqrt((FloatAt(actor + kRefPositionOffset) - px) *
		                                          (FloatAt(actor + kRefPositionOffset) - px) +
		                                      (FloatAt(actor + kRefPositionOffset + 4) - py) *
		                                          (FloatAt(actor + kRefPositionOffset + 4) - py))
		                          : 0.0f;
	}
	GrabEvent events[8];
	const UInt32 n = StepGrab(g_state, *g_settings, dt, g_state.count > 0 ? views : nullptr, events, 8);
	bool changed = false;
	bool lethal = false;
	for (UInt32 i = 0; i < n; ++i) {
		if (events[i].kind == GrabEvent::Release) {
			LetGoOf(events[i].refId, events[i].why);
			changed = true;
		} else {
			Bite(events[i], player);
			lethal = lethal || events[i].lethal;
		}
	}
	if (lethal) {
		ReleaseAll(LetGo::PlayerDown);
		ResetGrab(g_state);
		changed = false;
	}
	if (changed) {
		SendState();
	}
	for (UInt32 i = 0; i < g_state.count; ++i) {
		HolderAnim* a = AnimOf(g_state.holders[i].refId);
		if (a == nullptr) {
			continue;
		}
		a->biteLeft -= dt;
		a->holdClock += dt;
		if (a->biteLeft <= 0.0f && a->holdClock >= 1.0f) {
			a->holdClock = 0.0f;
			PlayIdle(LookupForm(a->refId), kHold);
		}
	}
	HoldFlags(player, dt);
}

void ResetGrabInGame() {
	ResetGrab(g_state);
	g_hitCount = 0;
	g_freeRequestCount = 0;
	for (HolderAnim& a : g_anims) {
		a = HolderAnim{};
	}
	g_lastHold = PlayerHold::Free;
	g_testClock = 0.0f;
	g_struggle = Struggle{};
	g_needed = 0;
	g_neededClock = 0.0f;
}

}  // namespace walkers
