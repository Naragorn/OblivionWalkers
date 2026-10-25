# Oblivion Walkers - specification

Status: design, 2026-10-23. Nothing built. Decided by Nadi on 2026-10-23: the
features in section 4, the dead rise *as themselves* and only after a
configurable delay (4.1), the mod in its own folder now and its own repository
later.

## 1. What it is

A zombie apocalypse for Oblivion in the spirit of Zombie Walkers (Fallout 4,
v2.6) and 28 Days and a Bit 4 (Oblivion, 2010-2014), done where those two
could not go: in a native xOBSE plugin. Spawns become walkers, the fallen get
up, cities get besieged, and every piece has a switch.

It lives beside OBVR and does not need it. Two features need what only OBVR
knows: headshots only (4.10) the hit's place on the body, and the end by a
lost head (4.17) OBVR's cut. Without OBVR, 4.10 is off and 4.17 takes a
power attack's kill instead.

Why a plugin and not a script mod: both predecessors ran on scripts, and the
comment threads say what that cost. Zombie Walkers 2.6.3's resurrection
script was the mod's CTD source ("crashing right when a body was waking up",
Buffout logs pointing at the mod; the author dropped the feature in 3.0 as
"script intensive" and crash-prone). 28 Days' infected-NPC script left corpses
"running against walls" and flying off, with crashes at door breaches and in
interiors. A plugin acts in the engine's own frame, at the exact point a
thing is decided, and keeps its own bookkeeping outside the scripts' timing.

## 2. Engine facts this rests on

Read in Oblivion.exe 1.2.0.416 (Steam, SHA-256 74db8031...) with REA/Ghidra
and in xOBSE's sources on 2026-10-23, plus the harness runs named.

| Fact | Where | Status |
|---|---|---|
| Oblivion has one leveled-actor record, LVLC, for NPCs and creatures alike; each entry carries level, form and **count** | CS wiki Leveled Creature; `tools/esm-spawns.py` over Oblivion.esm: 682 lists, 8005 spawn points | read |
| The vanilla lists with a count above 1 are the deer and horse herds (LL0Deer100: doe x3, x2; LL0Horse100: x3, x2) | `artifacts/esm/summary.md` | read |
| A world spawn point with an entry of count above 1 spawns that many actors | `harness/spawn-count.txt`, 2026-10-23 15:29: 30 actors from the loaded grid's 16 leveled points, 18 deer where at most 16 points could yield deer, no directly placed actor there | **measured, honoured** - caveat: the harness save may carry spawns of an earlier visit to these cells, which the roster cannot tell apart; the clean proof is the plugin's own injection into LL0ZombieVariant at Hame |
| `TESLeveledList::CalcElement` = 0x0046CC70: chance none at +0xC, flags at +0xD (1 calc all levels, 2 calc each in count), RNG 0x0047DF80 modulo 100, recursion into nested lists; only two callers, both in the container variant 0x0046CDE0 | REA | read |
| The path "world reference with an LVLC base becomes an actor on cell load" uses another entry; not found | REA | open, not needed for the plan |
| `ResurrectActor` (console `resurrect`): table entry 0x00B0DEA0, opcode 0x108C, execute 0x00510150 = DynamicCast to Actor, then Actor's virtual at **vtable+0x20C** with (1, has3D, animate) | REA, bytes read by hand | read |
| The engine's resurrect on a corpse brings the same reference back alive: a placed bandit (00069AD1) killed by `killall` reads "1 loaded, 1 dead ... (dead 1)" and ten seconds after `<ref>.resurrect 1` "1 loaded, 0 dead" again; a reference-prefixed console line reaches the game through xOBSE's console interface (its answer is no success flag) | `harness/rise-baseline.txt`, 2026-10-23 16:48, PASS | **measured** |
| A native plugin registers for OnDeath/OnHit through `OBSEEventManagerInterface::RegisterEvent` | deepwiki xOBSE (PluginAPI.h) | read |
| Leveled-list edits by OBSE live for the session, not in the save | xOBSE docs | read |
| `CloneForm`: `CreateFormInstance` + `CopyFrom` + the created-base list; clones get mod index FF and are saved | xOBSE GameForms.cpp | read |
| TESNPC: race at +0xE4 (TESRaceForm, the pointer at +0xE8), hair +0x1C8, eyes +0x1D0, FaceGen on the NPC record; TESActorBaseData flags at base+0x24+4 (essential 0x2, respawn 0x8, no low-level processing 0x200) | xOBSE GameForms.h | read |
| `TESObjectREFR::baseForm` at +0x1C; xOBSE's SetBaseForm refuses actors | xOBSE | read, the reason open |
| PlaceAtMe actors stay in the save; DeleteReference refuses actors; dead actors of a reset cell go when it resets | CS wiki PlaceAtMe | read |
| Movement: `RunSpeed = WalkSpeed * (fMoveRunMult + fMoveRunAthleticsMult * athletics/100)`, creatures from `fMoveCreatureWalkMin/Max` | CS wiki Movement Game Settings | read |
| Oblivion gives no XP for kills; skills rise by use | the game | read |
| The actor process manager's four lists (high to low) through 0x00673A50(0x00B3BD00, level) | OBVR GameAddresses.h | read, in use |
| The plugin's Resurrect(1, has3D, 1) plays the get-up: the ragdolled bandit bent double with his hands at the floor under a second after the call, upright and walking 1.5 s later (pictures); the engine's dead flag flips at the call itself (12 ms), so the flag says nothing of the animation | `harness/rise-getup.txt` run 2, 2026-10-23 18:22 | **measured** |
| Only people rise: a Character reference (vtable 0x00A6FC9C) with an NPC_ base; creatures never. Of those, `OnlyPlayableRaces=1` (default) keeps Dremora, Golden Saints and the like down through TESRace's playable byte (+0x70, the race pointer at TESNPC+0xE8); the bandit's race 00000D43 reads playable | xOBSE GameForms.h; `rise-getup.txt` run 2 | read, in use |
| `<ref>.RemoveAllItems <the same ref>` - a container emptied into itself - loops the engine forever (the main thread at Oblivion.exe+58DF71, OBVR's watchdog); a placed actor's reference ID is not the next free one (the weapon dropped at a death takes IDs first) | `rise-creature-proof.txt` runs 1 and 2 | measured; the plugin never guesses a reference and never moves a container into itself |
| Resurrect(1, has3D=1, 1) keeps the actor's existing 3D: a race swapped on the base before the raise shows no new skin (`rise-zombie-race.txt` run 1); xOBSE's Update3D (Set3D(0), then the model loader's queue) on the risen reference builds him anew from the clone - a Redguard with Imperial skin four seconds later, his own face and gear (run 2, 19:54) | the runs' pictures | **measured**: the swap is visible after a rebuild; the plugin issues it after the get-up |
| xOBSE runs a plugin's co-save callbacks only when the plugin set an opcode base of its own (`SetOpcodeBase`; obse.log otherwise: "HandleSaveGame: plugin with default opcode base registered for serialization"); the plugin uses 0x2B00, free in github.com/llde/xOBSE/wiki/Registered-Opcodes-for-OBSE-Plugins on 2026-10-24, not yet registered there | `rise-cosave.txt` runs 1 (no callback) and 2 (written, read, the walker up after the load) | **measured** |
| Walk-only by the HighProcess movement flags (+0x1FC, run 0x200 -> walk 0x100) written once a frame for every live walker, no hook (OBVR reroutes the setters for its slow approach and refuses foreign entries): with `[Zombies] Walk=1` the walker stayed at 31-65 units/s with 618 runs turned to walks; with Walk=0 the game's zombie ran in a burst at 196 units/s | `walker-speed.txt`, 2026-10-24 10:26 and 10:29 | **measured** |
| Every leveled creature list in memory is reachable through the engine's form table, NiTPointerMap<TESForm> at 0x00B0613C itself (LookupFormByID 0x0046B250 starts `mov ecx, 0x00B0613C`; bucket count +4, buckets +8, an entry {next, key, form}); a TESLevCreature's TESLeveledList sits at +0x24, its first ListEntry {data, next} inline at +0x28, ListData {level, pad, form, count}; an entry's form pointer rewritten to another list makes the engine resolve that list in its place | xOBSE GameTypes.cpp, GameForms.h, NiTypes.h; EXE bytes 2026-10-24; `spawn-replace.txt` and `spawn-replace-outdoors.txt` | read, **measured**: 122297 forms, 2623 lists with MOO and Ascension loaded, 4055 entries in 832 lists pointed at the walkers at the main menu, 36 more in 23 lists five seconds after the save loaded (a mod's script adds them at load) |
| TESCreature in 1.2: flags +0x28 (swims 0x10, flies 0x20, walks 0x40), AISettings +0x6C (aggression, then confidence +0x6D), health +0x84, the name string +0xA4, the script +0xC8, the type +0x104 (0 creature, 1 daedra, 2 undead) | xOBSE GameForms.h | read; the confidence offset **measured**: the deer (file aggression 100, confidence 0) read as harmless at runtime |
| A placed leveled list honours a nested entry's count: `player.placeatme 00084B2C 3` (LL0Goblin100) with every goblin replaced by WalkersZombieVariant (each walker at counts 1, 2, 3) gave 7 walkers in one run and 4 in the other, no goblin; at the engine's own spawn points (Tamriel -29,5, cells loaded fresh) 31 walkers stood where spawn-count read 6 wolves and 4 bear cubs, the deer and two MOO NPCs as before | `spawn-replace.txt` 2026-10-24 11:14 and 11:15, `spawn-replace-outdoors.txt` 11:17 | **measured** - this also settles spawn-count's caveat: those cells spawn fresh from the harness save |
| `SetDestroyed 1` on a dead actor keeps it from being activated: the console's `<corpse>.activate player` opened no container on a destroyed walker; on one not destroyed it opened the container menu | CS wiki SetDestroyed (cs.uesp.net/wiki/SetDestroyed: on an actor it "prevents the actor from being activated for conversation, looting, or pick pocketing"); `spawn-replace.txt` 11:14 (NoLoot=1) and 11:15 (NoLoot=0) | read, **measured** through the console's activate; the player's own activate key not run |
| The runtime keeps no editor IDs for races (xOBSE's GetEditorID: cells and quests only), so the plugin finds the ESP's race by load index: DataHandler (0x00B33A98) holds the loaded mods by runtime index at +0x8D4 (ModEntry::Data*, the file name at +0x1C) and their count at +0x8D0; the race is local ID 0x800 under that index | deepwiki xOBSE GameData.h | read, built (Rise.cpp FindZombieRace), to be run |
| The ESP file: record header type/size/flags/formID/vcs, GRUP with the size including its header, TES4 data HEDR(1.0f, records, next ID) CNAM SNAM MAST DATA; a new record's mod index is the count of masters | DLCShiveringIsles.esp and Oblivion.esm bytes, UESP TES4 page | read, `tools/esp-write.py` writes and re-reads it |
| The engine's frame seconds (0x00B33E9C) summed to 10.0 s in about 12 s of wall clock during a harness run with picture captures: AfterSeconds is engine time, a little slower than the wall | `rise-getup.txt` runs 1 and 2 | measured, cause not pinned (captures or a capped delta) |
| The console's resurrect, byte by byte: `cmp [animate],1; sete al; cmp [actor+0x3C],0; setne cl; push eax; push ecx; push 1; call [vtbl+0x20C]` - Actor::Resurrect(1, has3D, animate), xOBSE names it `Resurrect(UInt32, UInt8, bool)` with the comment that animate=1 takes the bhkProxyController and 0 recreates the processes | REA 0x005101A8, xOBSE GameObjects.h | read, the plugin calls it the same way |
| xOBSE's task queue (`kInterface_Tasks` = 8): a task enqueued with EnqueueTask runs once every main-loop pass until RemoveTask - the plugin's frame tick, no engine byte patched | deepwiki xOBSE TaskManager::Run | read, in use |
| xOBSE's event manager (`kInterface_EventManager` = 10): `RegisterEvent("OnDeath", func, 0, 0, 0)`; the handler gets (arg0 the fallen reference, arg1 the killer or null, thisObj the fallen), cdecl, inside the death path on the main thread; nothing clears native handlers on a game load. **At the plugins' load RegisterEvent answers false**: EventManager::Init() runs in OBSE_Initialize after InstallPlugins, so the handler must be registered later - the plugin asks again from its frame tick until it is taken (run 1 of `rise-walker.txt`, 2026-10-23 17:41) | deepwiki xOBSE EventManager.cpp, obse.cpp; `rise-walker.txt` run 2, 2026-10-23 17:54 | read, in use; **measured**: the handler taken at the second frame tick, a console `killall` raises OnDeath with arg1 = 0 (no killer) and thisObj = 0, the DLL's Resurrect after 23.5 s stood the bandit up (the roster alive again, 91 units from the player) |
| TESNPC/TESCreature: TESActorBaseData at +0x24, its flags at +0x28 (female 1, essential 2, respawn 8, no low-level processing 0x200); 719 of Oblivion.esm's 2482 NPC_ records carry 0x200, the bandits among them (00069AD1: 00100290), most named people not | xOBSE GameForms.h; `tools/esm-spawns.py` module over the ACBS subrecord | read |
| `LookupFormByID` = 0x0046B250 (cdecl, the form ID), `CreateFormInstance` = 0x00447DF0 (the type byte), `TESForm::CopyFrom` is virtual **45** (vtable+0xB4, BaseFormComponent's four virtuals counted; a first reading as 33 was wrong and cost run 1 of `rise-walker-clone.txt`), `AddFormToDataHandler` 0x0044D950 on 0x00B33A98, `AddFormToCreatedBaseObjectsList` 0x00459800 on 0x00B33B00: xOBSE's CloneForm in that order; a clone's ID gets the FF index from the engine | deepwiki xOBSE GameAPI.cpp, GameForms.cpp | read, for the clone (4.1 step 3) |
| The clone path's conventions, byte by byte: CreateFormInstance 0x00447DF0 takes the type off the stack behind its SEH frame (cdecl); AddFormToDataHandler 0x0044D950 and AddFormToCreatedBaseObjectsList 0x00459800 take their object in ecx and the form on the stack, `ret 4` (thiscall) - as xOBSE calls them | REA; `rise-walker-clone.txt`, 2026-10-23 18:04 and 18:07 | read, **measured**: the clone made (CreateFormInstance hands out the FF ID itself: FF000BB1, FF000BB4), CopyFrom (0x00521ED0 for a TESNPC) copied the flags, the reference's base swapped on the corpse, the engine's Resurrect stood him up on the clone - the roster alive with the clone's base |
| xOBSE's SetBaseForm refuses actors only through its own guard `!thisObj->IsActor()` in the command - no engine reason is given | deepwiki xOBSE Commands | read; the swap on a corpse followed by the resurrect **works** (`rise-walker-clone.txt`, 2026-10-23) |

## 3. Architecture

Three parts:

1. **`OblivionWalkers.esp`** - records only, no scripts, written by
   `tools/esp-write.py` from Oblivion.esm's own records (no Construction Set):
   today the Walker race for the risen (4.1, a copy of the Imperial race, local
   ID 0x800, the look to follow); later the walker creatures (own copies of the
   zombie records with their own animation folder, 4.4), factions, the Black
   Horse Courier editions (4.8), the scenario globals.
2. **`OblivionWalkers.dll`** (xOBSE plugin) - everything that happens at
   runtime: list injection, the death watch, raising, speed, health, loot,
   fame, hordes, sieges, scenarios, the INI.
3. **`OblivionWalkers.ini`** - every switch, read at start and on change
   (hot reload every few seconds, as OBVR's ReloadGate does).

Save discipline: nothing the plugin does at start goes into the save. List
injection is redone every session (the engine keeps lists in memory only).
What *must* persist (who is waiting to rise, the scenario's state, the city
populations) goes into the OBSE co-save through the serialization interface,
with RefIDs resolved on load.

Harness: OBVR's hand script harness runs the scenarios in `harness/`; since
2026-10-23 every mark writes a `HandScript: actors` line (everyone loaded,
by base form, alive and dead) and `{actor}` names the nearest actor in a
console line. The Walkers plugin writes its own `Walkers:` log lines the
scenarios `expect`.

## 4. Features

Costs: cheap / small / medium / large / huge, with the reason. Each feature
names its setting, what it does, how, what it costs, and how it is tested.

### 4.1 The dead rise - `[Rise] Enabled=1`, `AfterSeconds=20`, `JitterSeconds=10`, `ChancePercent=100`, `OnlyInfected=0`, `OnlyGeneric=1`, `OnlyPlayableRaces=1`, `OnlyHumans=0`, `CloneBase=0` (proof switch, to become 1), `ZombieRace=1`, `LookAfterSeconds=3`

**Decided 2026-10-23 evening (Nadi): the fallen rises as the game's own zombie
creature, not as himself with a zombie race.** At the rise the corpse is swapped
for a Walker creature (a copy of CreatureZombie1..4, the records 4.6 uses too)
at its spot: the zombie placed and dropped dead there, the corpse's inventory
moved over, the corpse disabled (out of the world, a record left in the save
until the cell resets - the engine deletes no actor), then the zombie raised
with the engine's Resurrect(1, has3D, animate=1) so it gets up where the man
fell. Gained: the zombie's look, shamble, groans, claws and hostility for free,
no factions, combat style or walk to rebuild on a person, one set of Walker
records for 4.1 and 4.6. Lost: his face, clothes and name. The race path below
stays measured and built behind `CloneBase` as the fallback.

What (the race path, built and measured, kept as the fallback): a fallen
person of a playable race (men, mer, beast folk; `OnlyPlayableRaces=0` takes
Dremora and the Isles' Saints and Seducers too; `OnlyHumans=1` narrows to the
four human races, Nadi's extra option of 2026-10-23, off by default; creatures
never) gets up as a zombie *as himself*:
same face, same body, same inventory, grey skin, "Zombie" for a name, hostile
to the living, slow. Not a spawn beside the corpse (decided 2026-10-23). After
the delay, not at the death.

How, in order:
1. OnDeath (native callback): if the actor is an NPC, not essential, matches
   the filters (`OnlyGeneric`: the base carries the no-low-level-processing
   flag 0x200, which leveled-list NPCs must; `OnlyInfected`: the Zombie Rot
   disease is on him, 4.14), roll `ChancePercent`, then put the reference on
   the waiting list with a time: `AfterSeconds` plus a random share of
   `JitterSeconds`. The death itself runs untouched: ragdoll, loot drop, quest
   counters. Nothing is touched in the frame of the death - that was Zombie
   Walkers' crash.
2. A per-frame tick counts real seconds for every waiting reference whose
   cell is loaded (3D present). Leaving the cell freezes the clock; it runs on
   at the next visit, so the getting-up is always in view and never off-screen.
3. Rise: clone the base (the engine's own clone path as xOBSE's CloneForm
   does it), on the clone set the race to the ESP's zombie race, the name
   (4.2), the factions (zombie faction, friendly to zombies), aggression 100,
   the combat style (no spells, no bows), the speed (4.4), the health (4.5);
   point the reference's base (+0x1C) at the clone; call Actor's virtual at
   vtable+0x20C with (1, has3D, animate=1) - the engine plays the get-up on the
   old model (it keeps the 3D it has); `LookAfterSeconds` (3) later the plugin
   has xOBSE's Update3D build him anew from the clone, and the race shows
   (measured 2026-10-23, `rise-zombie-race.txt`). Built: the clone, the race
   from the ESP, the rebuild; open: the zombie look itself (textures), name,
   factions, aggression, combat style, speed and health on the clone.
4. "Already a zombie": the base's race is the zombie race. No co-save needed
   for that; the waiting list with its clocks goes to the co-save.

Why the clone and not the ESP's own base records: a raised bandit must keep
his face; FaceGen lives on the NPC record, so the record that carries his
face must be the one the reference points at. Cost of the clone: one created
base record per risen zombie, in the save. Cap it: `[Rise] MostAlive=60`;
above the cap nobody new rises until one is cut down. Whether a clone can be
destroyed once its reference is gone (the engine's `Destroy`/`SetDeleted` on
created forms) is open; until measured, the cap is the control.

Essential NPCs only fall unconscious: no OnDeath, no rise. Quest NPCs are
kept out by `OnlyGeneric=1` (default); `0` raises everyone and is the
player's own risk.

Cost: **medium to large.** Resurrect is found; the clone path is readable in
xOBSE; the base swap on a corpse followed by the resurrect is measured and works
(2026-10-23, `rise-walker-clone.txt`: clone, swap, get-up, the roster alive
on the clone). The race swap's 3D rebuild (does the fallen NPC's head go on
the zombie race's body?) is the one unknown left, and needs the ESP's race. Harness:
`rise-baseline.txt` (the console's own resurrect: PASS 2026-10-23, the
corpse stood up alive as the same reference), then `rise-walker.txt` (the
plugin's rise, with the roster showing the clone base and the zombie race).

The swap's look (built 2026-10-25, Nadi 2026-10-24: "die zombies fallen an
deren stelle aus dem himmel auf den boden und stehen dann auf. besser wäre:
zombie ersetzt direkt die leiche und steht dann auf"): the walker is made
unseen (SetActorAlpha 0) in the frame its model appears, moved onto the
corpse and killed; its ragdoll settles unseen while the corpse still lies.
DropSeconds later the two change places in one frame - the corpse disabled,
the walker shown (SetActorAlpha 1), the man's items on it - and after
`[Rise] LieSeconds` (1) it gets up. `rise-swap-look.txt` PASS 2026-10-25
09:24. The walker lies in its own ragdoll pose, not the corpse's, up to a
few dozen units off (58 in the run).

MatchCorpse (built 2026-10-25, Nadi: "bau A und B, default in ini B"): one
frame before the reveal the unseen walker's ragdoll bodies are laid into the
corpse's place (src/Pose.cpp, PoseLogic.h): 1 moves the whole walker so its
pelvis lies on the corpse's and turns it about the vertical to the corpse's
pelvis-to-head heading; 2 (default) lays each body on a bone the corpse has
too onto that bone's world transform, the rest as in 1. The Havok calls are
OBVR's (the wrapper's SetTranslationAndRotation, vtable +0xA0, inside the
Havok lock; velocities zeroed, the body woken) - the engine's own push
0x0089EAE0 hands it the node's world transform, read with REA 2026-10-25.
Harness 2026-10-25 09:49 (2) and 09:51 (1), both PASS: 13 and 16 bodies
found (one walker model has no left arm), 214 and 108 microseconds. The
pictures: with 2 the walker lies where the corpse lay and much like it
(s11), a second later in another pose (s12) - its joints pull the copied
pose, or the copy is off; with 1 its head seemed to point a quarter turn
off the corpse's. Neither settled: the log now gives the pelvis and head of
both before the pose, when shown and at the raise, for the next run. The
headset is to judge it.

The numbers (harness 2026-10-25 10:10 and 10:11, the log's pelvis and head):
- MatchCorpse=1: the turn read 55 degrees; when shown the walker's head lay
  exactly on the corpse's (245 -1423), and at the raise its pelvis lay 8
  units off the corpse's and it pointed 4 degrees off the corpse's heading -
  the placement holds.
- MatchCorpse=2: when shown the head lay exactly on the corpse's too, but by
  the raise (3 s on) the walker had turned 55 degrees off the corpse's
  heading, its head 32 units away - the copied pose does not hold. The likely
  cause (derived, not proven): each body took the human bone's world place,
  but the walker's bones are of other lengths, so the bodies stand off their
  own joints and the joints pull them together, turning the body. The fix in
  reach: lay the bones by the corpse's rotations along the walker's own bone
  lengths (forward kinematics from the pelvis - what retargeting does),
  small to medium. Not built.
- MatchCorpse=2 rebuilt by retargeting (76cd42c: the corpse's bone turns
  along the walker's own bone lengths, down from the pelvis), harness
  2026-10-25 10:19 and 10:21: when shown the head lay on the corpse's again
  (1 unit off), but by the raise the walker had turned 83 and 46 degrees off
  the corpse's heading, its pelvis 26 and 12 units off. So the bone lengths
  were not the cause, or not the only one. Next suspect (not proven): the
  zombie skeleton's bone axes differ from the human's, so a human bone's
  world turn put onto a zombie bone twists it against its joints. The
  cheapest step that settles it: log, for a few bones, the direction to the
  child bone in the bone's own axes on both skeletons - equal directions
  mean equal axes. If they differ, retarget by bone directions (axis-free)
  instead of turns.
- Run 2026-10-25 12:50 (e56ed2d, MatchCorpse=2): the axes agree. The child
  bone lies along the bone's own x on both skeletons (Spine to Spine1, Spine2
  to Neck, upper arm to forearm, thigh to calf: 1.00 0.00 0.00 on both); only
  pelvis to spine differs by some 20 degrees (the walker's 0.97 0.01 0.25,
  the corpse's 0.99 -0.01 -0.11). The roll about x is not covered by this
  test. The pose drifted again: 69 degrees off the corpse's heading by the
  raise, the pelvis 10 units off - the body turned about its pelvis. So the
  axes are not the cause; left as suspects (not proven): the zombie's joint
  limits and its limbs meeting the floor at other lengths. Default stays 1.
- "B dann A" built (fe9efd8, Nadi 2026-10-25): MatchCorpse=2 lays the pose,
  leaves it unseen `SettleSeconds` (1.5), then moves and turns the settled
  body onto the corpse's place, then shows it; the placement now goes by the
  lowest spine bone (it carries a body; the pelvis lags). Harness 2026-10-25
  12:59 (2) and 13:00 (1), one run each: with 2, 9 degrees and 9 units off
  the corpse at the raise - holds; with 1, 46 degrees and 18 units off - did
  not hold this time, where the run of 10:11 (pelvis-based) held at 4. So
  the drift varies from run to run in every mode. New suspect, from how the
  swap works (derived, not proven): the walker is moved onto the corpse and
  laid into its place while the corpse still lies there - both ragdolls
  collide, and the corpse's bodies push the walker's away as much as they
  overlap. That would explain all three modes drifting by different amounts.
  The cheapest step that settles it: one run with the corpse disabled at the
  pose instead of at the reveal (a test switch; it shows a gap in the
  picture) - if the drift goes, the fix is to take the corpse's collision out
  while it stays seen.
- HideCorpseAtPose runs (644f551, harness 2026-10-25 13:29 and 13:31): with
  1, 4 units and 9 degrees off at the raise; with 2, the pose alone still
  turned some 80 degrees while it settled - with no corpse to push it, so
  the pose's own drift is not the collision - and the placement after it
  ended 1 unit off the corpse's spine and head. So: the pose drifts by
  itself (its joints), which the settle-then-place absorbs; the corpse's
  collision spoils the placement by up to tens of units and degrees (the 46
  of 13:00, the 9 of 12:59). Both measured, one run each.
- fc46f62 (MatchCorpse=2 by default, the settled walker placed and shown in
  one frame), harness 2026-10-25 13:48: worse - while the pose settled on the
  corpse the two pushed each other: the corpse's head moved 50 units and rose
  20, the walker ended sitting on it (its head 80 units above the floor);
  placed and shown, it lay 26 units off and 35 high at the raise. The second
  run FAILed before the world (the script did not start in 240 s; SteamVR's
  compositor had aborted at 13:49:03 on a watchdog, the Performance
  session's report). So the corpse must not collide with the walker while it
  settles - the settle needs the corpse out of the walker's way, not just at
  the reveal. Unbuilt fix: keep the walker's bodies from colliding with the
  corpse's (Havok's collision filter, rb+0x30, the rule 0x008A7F70 - OBVR's
  GrabPhysics.h; whether two ragdolls can be kept apart by it is not read).
- Both: the pelvis node and the reference's position stay where the walker
  fell for a frame or more after the bodies moved (the pelvis carries no
  body on these skeletons); by the raise they have followed. Whether the
  mesh is seen stretched in those frames is not known.

### 4.2 Walkers by name - `[Zombies] RenameToWalker=1`, `Name=Walker`

What: every zombie, spawned or risen, is called "Walker" (or the name set).

How: the ESP's walker creatures carry the name; raised NPCs get it on their
clone (SetName on the clone's TESFullName). Vanilla zombies that other mods
place keep theirs unless `RenameVanilla=1`, which sets the name on the
vanilla zombie records at start (in memory, every session).

Built 2026-10-24 evening: at the first tick the plugin gives the four
walker records Name (RenameToWalker=0: "Zombie") through the engine's own
BSStringT_Set (0x004028D0, xOBSE GameTypes.cpp), and with RenameVanilla=1
every other creature's "Zombie" standing as a word (`RenamedZombie`):
Zombie, Headless Zombie, Dread Zombie - and quest ones too, "Zombie
Guardian" becomes "Walker Guardian". RenameVanilla defaults to 0. In memory
only, every session. `rename.txt` PASS 2026-10-24 21:31: the walker records
read "Walker", 153 records renamed with MOO loaded, each read back.

Cost: **cheap.** Records and one string write.

### 4.3 No skill gain from zombies - later, `[Zombies] NoSkillGain=1`

Oblivion has no kill XP; skills rise by use, so "no XP from zombie kills"
becomes: a blow on a zombie trains nothing (Blade, Blunt, Hand to Hand,
Marksman, Destruction and the armor skills through hits taken from one).
Nadi, 2026-10-23: keep it, build it later, as a toggle that is **on by
default**. On by default means a player who fights mostly walkers (the
OblivionCrisis and Horde scenarios) trains no combat skill through them;
that is the hard mode Nadi wants, and `0` gives the training back.

How: a hook on the engine's skill-use increment with the hit's target at
hand (the actor-value "skill use" path; entry point to be found with REA),
vetoed when the target or the attacker is a walker. Cost: **medium** - the
entry point is the unknown, the veto a few lines, the test a harness scenario
that reads the player's skill-use counters before and after blows on a
walker and on a bandit. Not in the first build (section 8, phase 4).

### 4.4 Walkers walk - `[Zombies] Walk=1`, `SpeedMultiplier=1.0`, `SpeedRandomPercent=30`

What: zombies never run. They shamble, each at its own pace.

How (built 2026-10-24, 013709b): every frame, a run in a live walker's
HighProcess movement flags becomes a walk - no hook, so OBVR's slow approach
keeps its own (section 2). `Walk=0` leaves the vanilla zombies' bursts. The
first plan - an animation folder whose run is the walk - was not needed.

The pace (built 2026-10-24 evening): `SpeedMultiplier` and
`SpeedRandomPercent` set the Speed of the four walker records in memory at
the start (TESAttributes at +0x88 of TESActorBase, Speed at +0x90; xOBSE
GameForms.h), spread evenly from the percent under to the percent over the
multiplied pace (`WalkerSpeed`: 8, 11, 13, 16 at the defaults, the zombies'
own 12). Four paces, not one per walker: per reference the engine offers
SetActorValue, which the CS wiki says writes the base record anyway, and
ModActorValue, whose script modifier stays in the save for good. A creature
walks at fMoveCreatureWalkMin + (Max - Min) * Speed / 100 (5 and 300; the CS
wiki's Movement Game Settings) - Speed 12 is about 40 units a second.
Measured 2026-10-24 21:00 (`walker-speed.txt`, SpeedMultiplier=2,
SpeedRandomPercent=0): the records at 24, the risen walker at 69 to 106
units/s where the run of 10:26 read 31 to 65, at the player after about 10 s
where it took 24. The spread itself (four different paces) is in the log
line per record; not run with walkers of all four records side by side.
The feel of the paces (and whether the walk animation slides at the fast
end) is the headset's.

### 4.5 Zombie health - `[Zombies] HealthPercent=100`

What: 1 to 100 percent of the record's health. "Softie": 25 with more of them
(4.6).

How (built 2026-10-24): the plugin scales the health of the four walker
records at start (in memory, from the values they had; TESHealthForm +0x84)
- the risen and the spawned stand on those records. Not per reference, so
a save never carries a scaled value. The harness read the records' 80 at
100 percent; a run at another percent is not done.

Cost: **cheap.**

### 4.6 Spawn replacement - `[Replace] Bandits=1`, `Marauders=1`, `Necromancers=0`, `Conjurers=0`, `Vampires=0`, `Goblins=0`, `Animals=0`, `Daedra=0`, `Undead=0`, `ChancePercent=100`, `MinCount=1`, `MaxCount=3`

What: spawn points of the chosen kinds spawn walkers instead, one to three
of them.

How: list injection at start, every session. The plugin walks the LVLC
records by editor-ID family (`LL*Bandit*`, `LL*Marauder*`, `LL*Necromancer*`,
`LL*Conjurer*`, `LL*Vampire*`, `LL*Goblin*`, `LL*Wilderness*`/`LL*Beast*`
for animals, `LL*Daedra*`/`LL*Dremora*`, `LL*Undead*`/`LL*Tomb*` - the
families from `tools/esm-spawns.py`) and, per `ChancePercent`, replaces the
entries with walker entries of count `MinCount`..`MaxCount` (one entry per
count value, so the engine's own random pick gives the spread). The engine
then spawns them itself at its own spawn points: no new references of the
plugin's, no save bloat, the cell reset's respawn as always. Mods that add
their own lists are covered by the same family match; a `Lists=` line in the
INI names more.

Rests on the count measurement (`spawn-count.txt`). If a world spawn point
ignores the count, the fallback is the plugin spawning the extra ones beside
the first at cell load (references of its own, with a cap and cleanup).

Built 2026-10-24 differently from the family match above, on Nadi's word:
for now only creatures, no people, no harmless animals. The plugin walks
every leveled creature list in memory (the form table) at the main menu and
again five game seconds after each load; each creature an entry names gets
a line in the INI's `[ReplaceCreatures]` - `Name [Mod ID]=1` - written by the
plugin on first sight, 0 by default for harmless animals (a creature-type
animal with confidence 25 or less: deer, sheep, wild horses, cats, rabbits),
creatures that only swim (slaughterfish) and creatures with a script (a
quest or the arena may count on them), with the reason after the bracket.
An entry whose creature is on points at WalkersZombieVariant for the
session; the variant list holds every walker three times, the counts
written from `MinCount`/`MaxCount` (1, 2, 3 by default). `ChancePercent` and
the per-family switches are not built - the per-creature lines replace them.
With MOO and Ascension loaded: 1108 creatures, 938 of them walkers.
`spawn-replace.txt` and `spawn-replace-outdoors.txt` PASS (section 2).

Changed 2026-10-24 afternoon, on Nadi's word: every creature and person the
lists of Oblivion and the expansions can spawn is listed in the shipped
INI, 1 or 0; harmless animals and all people start at 0. People are now
list entries the plugin replaces too (an NPC_ entry - Oblivion's leveled
creature lists hold people, LL1BanditBossLvl100 holds nothing else), with
the note "person". `tools/ini-creatures.py` writes the section from
Oblivion.esm (Shivering Isles inside it), Knights.esp and the official
DLC*.esp, by the same rules as the plugin: 726 lines (249 on; 400 people, 63
scripted, 8 harmless animals, 6 swimmers off). Checked against the section
the plugin wrote in the run of 11:14: all 323 lines of those files it wrote
are identical in the tool's output; three more of the tool's are entries MOO
replaced in memory. A mod's creatures and people are appended by the plugin
at the first start with the mod, by the same rules (MOO's and Ascension's:
785 in that run). The variant list holds every walker ten times now, so
`MaxCount` goes to 10 with each count of 1..10 as likely; the default is
1 to 10 since the evening (Nadi), and HealthPercent 15. Scripted and swimming creatures stay at 0 - Nadi's word named
harmless animals and people, these two are mine, asked - Nadi kept them at 0
(2026-10-24 evening).

The INI kept as it is (built 2026-10-24 evening): the plugin puts its new
lines into the file itself, behind the section's last key line
(`InsertSectionLines`), written beside the INI and moved over it.
WritePrivateProfileSection had rewritten the whole file under Wine: blank
lines dropped, CRLF written, "; =0" joined to ";=0". Measured in the run of
20:47: the shipped INI a byte-for-byte prefix of the file after the plugin
appended 3329 + 15 lines of MOO and Ascension, its line feeds and blank
lines as they were. A person's log line names no creature type now: the
offset read for it is no field of a TESNPC (it read 84 and 192).

`MaxCount=10` measured (20:47): the variant list's counts read 1..10 for
each walker, the goblin list placed three times gave 15 walkers.

Open: quests that count kills of a creature without a script on it (by
GetDeadCount on its base) would never finish while it is a walker - none
known, none searched for; the Oblivion planes' daedra become walkers too,
which their INI lines switch off. A person turned on is a walker entry in a
list that otherwise spawns people: `spawn-replace-person.txt` PASS 20:49 -
the five Bandit Hedge Wizards set to 1, LL1BanditHedgeWizard placed three
times gave one walker and two other bandits (a Bandit Bowman, a Bandit:
in this load order the list holds more than the hedge wizards, at 0), no
hedge wizard.

Cost: **small** with the count working (the injection is bookkeeping over
records already in memory); **medium** on the fallback.

### 4.7 Zombies avoid chapels - `[Zombies] AvoidChapels=1`

What: no walker spawns or rises in a chapel cell, and hordes do not path
into one. 28 Days' safe zones.

How: cells whose editor ID carries `Chapel` or whose owner faction is a
chapel's are a no-go: the injection skips lists that only spawn there (none
in vanilla), the rise skips corpses there, the siege (4.12) never picks a
chapel door, and the horde's wander targets avoid them.

Built 2026-10-24 evening - the rise: `IsChapelCell` takes a cell the Nine
Divines own (NineDivines, 000223BC) or one whose editor ID says "chapel";
the death watch reads both off the corpse's cell (parentCell +0x40, its
ExtraEditorID and ExtraOwnership; a cell keeps its editor ID in the game, as
xOBSE's GetEditorID reads it). Oblivion.esm read for it: 29 cells say
chapel - the nine city chapels with their halls and undercrofts (the
Nine's), Kvatch's three (nobody's), Weynon Priory's (the Blades'), and
Cadlew Chapel (a crypt - no chapel, Nadi 2026-10-24 evening: "Cadlew Chapel
ist eine Ausnahme"; its dead rise, walkers walk there), Hackdirt's Chapel of
the Brethren, Newt Cave Chapels of Light and a dummy cell, which count as
chapels. No
leveled creature is placed in any of them (the ACRE records read the
same day: rats and the like, placed directly), so the injection has
nothing to skip in vanilla - a mod that puts a leveled list in a chapel
gets walkers there, not handled. `chapel.txt` PASS 2026-10-24 21:13: the
bandit killed in AnvilChapelofDibella, "cell AnvilChapelofDibella owned by
000223BC) stays down (in a chapel, AvoidChapels=1)", no rise. Siege and
horde do not exist yet.

None of ours spawns in a chapel (Nadi, 2026-10-24 evening, after a first
build that took every walker out: "nur keine von unser mod spawnen oder
aufstehen lassen"): the frame tick's walk pass checks every live walker's
cell; one it first meets in a chapel was made there (a mod's list there, a
script's placement) and is disabled, once (`SpawnedInChapel`); one it met
outside before walked in after the player and stays. Met is per session -
one that walked in, saved and loaded inside, counts as made there.
`chapel-walker.txt` PASS 21:26: a walker placed in the Chapel of Dibella,
"made in AnvilChapelofDibella ... taken out of the world", none in the
roster. `chapel-follow.txt` PASS 21:27: a walker met in the hall, then
moved into the chapel onto the priest's marker, stays (11000801 x1 in the
chapel's roster, no Chapel line).

Cost: **cheap.**

### 4.8 Fame for kills - `[Zombies] FamePerKills=0`

What: one point of fame per `FamePerKills` walkers cut down (28 Days gave
one per 50). `0` turns it off - the default (Nadi, 2026-10-24 evening: no
fame unless set).

How (built 2026-10-24 evening): OnDeath of a walker whose killer is the
player (0x14) counts (`CountWalkerKill`); at the threshold the next frame
tick hands `ModPCFame 1` to xOBSE's console on the player. The count toward
the next point is kept in the co-save (record `FAME`). `fame.txt` PASS
2026-10-24 21:01 with FamePerKills=1: the walker killed by the player
earned the point ("ModPCFame handed to xOBSE: 1"), the one killed by
`killall` (no killer) counted nothing. That the fame value itself rose I
could not verify: xOBSE's console answer is no success flag and the harness
reads no console output - `player.GetPCFame` in the console settles it.

Cost: **cheap.**

### 4.9 Black Horse Courier - `[News] Courier=1`

What: an edition in every inn and tavern with the latest from the outbreak:
which cities are infested, where the horde is heading, what the Legion asks.

How: the ESP ships pre-written editions (one per city state and horde state,
written once); the plugin swaps the placed edition reference's base for the
edition that fits the scenario's state when the player enters the inn.
Dynamic text in a book needs OBSE string work the engine fights; the
pre-written set is what 28 Days did and it reads well.

Cost: **small** (writing the editions is most of it).

### 4.10 Headshots only - `[Zombies] HeadshotsOnly=0`

What: a walker only dies from a blow to the head; body hits stagger and
slow it.

How: OBVR's melee contact knows the hit's place on the body. The two plugins
talk through OBSE's plugin messaging: OBVR sends the hit's bone name with
the target; Walkers vetoes or lets the damage through (OnHealthDamage, which
fires before the damage is applied, can nullify it). Without OBVR the
setting does nothing and says so in the log. Vanilla arrows and spells know
no body part, so this is a VR feature.

Cost: **medium**, across two repositories (the message in OBVR, the veto in
Walkers, the stagger on a body hit).

### 4.11 Horde behaviour - `[Horde] Enabled=1`, `WanderRadius=4000`, `FollowRank=1`

What: walkers out of combat do not stand around; they drift in a direction,
group up behind one of their own, and the group drifts on. Zombie Walkers'
patrol-and-follow.

How: each idle walker gets a wander target the plugin moves ahead of it
(the AI's travel package to a marker the plugin owns, re-placed when
reached or stale); a walker that sees another with a higher random rank
follows it instead. Walkers that belong to a dungeon's spawn points are
exempt, so dungeons stay cleared by their own count.

Cost: **medium to large.** The packages are vanilla AI; the plugin's part is
the target bookkeeping and the staleness watch (a walker stuck on a rock).
The feel needs the headset.

### 4.12 City siege - `[Siege] Enabled=1`, `ZombiesPerCity=20`, `FromDoors=1`

What: an infested city has walkers in its streets; more come out of the
doors while you are there until the city's population is spent. Guards fight
them. Zombie Walkers' settlement attacks, 28 Days' hordes.

How: the scenario (section 5) keeps a population per city in the co-save.
In a city cell the plugin spawns walkers from the cell's door references at
the rate the population allows (the engine's own placement function, the
plugin's references), the cap `MostAliveInCell`. Dead siege walkers are cut
at the cell reset. Siege visuals (28 Days' rubble) are not planned.

Cost: **medium.** The spawn is a known path; the population model is small;
the test is the harness count line across a city cell.

### 4.13 Extra spawns - `[Replace] ExtraPercent=0`

What: more walkers in the wilderness and dungeons than the replaced spawns
give, by a percentage.

How: a second injection: the plugin adds walker entries to the wilderness
and lair lists beside the originals, weighted by the percentage, so some
of those spawn points become walkers without taking anything away.

Built 2026-10-24 evening: in the same pass as the replacement, every list
with an entry of its own left (a creature or a person not switched to
walkers) gets entries pointing at WalkersZombieVariant beside each of its
levels - `ExtraEntries`: size * p / (100 - p) of them, the fraction one more
for that share of lists by the list's own fixed draw, up to 90 percent and
64 a level. The engine picks alike among the entries of a window of levels
(CalcElement), so a share per level holds for any window. The entries come
off the engine's own heap (FormHeap_Allocate 0x00401F00, as xOBSE's
TESLeveledList::CreateData takes them), each behind the last entry of its
level, so the list stays sorted the way the pick walks it; a list gets them
once a session. `spawn-extra.txt` PASS 2026-10-24 21:15 with
ExtraPercent=50: 4675 entries in 540 lists at the main menu, 321 in 39
more after the load; LL1BanditHedgeWizard placed six times gave six of its
own people (MOO's bandits) and 33 walkers. Open: a save that carries a
list changed by a script (AddToLeveledList) brings that list back as saved;
not run.

Cost: **small** (was cheap; the heap and the order were the work).

### 4.14 Infection - `[Infection] Enabled=1`, `BiteChancePercent=25`, `PlayerInfection=1`

What: a walker's bite gives Zombie Rot, a disease: fatigue and health drain
over days, cured by Cure Disease like any other. An infected NPC who dies
rises even with `OnlyInfected=1`. The player dies of it if untreated (the
game's own death).

How: the ESP's disease (Oblivion's own mechanism, as 28 Days' "Touch of the
Worm King"); the walkers' attack carries it with the chance set. No
script: the engine applies diseases on hit itself.

Cost: **cheap** for the disease; the rise filter is a flag read.

### 4.15 No loot - `[Zombies] NoLoot=1`

What (Nadi, 2026-10-24): activating a dead walker opens nothing. Not the
risen (Nadi, 2026-10-24 afternoon: "sonst bricht das ja alles" - their gear
and quest items went over to the walker): they stay lootable whatever
NoLoot says.

How (built 2026-10-24): the death watch notes a walker's death; the next
frame tick sets the corpse destroyed (`SetDestroyed 1`, section 2) - a flag
the save keeps, so a corpse stays closed when `NoLoot` is set to 0 later.
`NoLoot=0` leaves them lootable. `spawn-replace.txt` PASS 11:14, its control
with NoLoot=0 opened the container (11:15).

The risen kept apart (built 2026-10-24 afternoon): the swap notes its
walker's reference the frame it finds it - before the kill that drops it,
which runs the death watch too and had set it destroyed until then - in a
set of up to 256 (the oldest goes), kept in the co-save (record `RISN`;
those gone from the game dropped at each save). `BlocksLoot` takes a dead
walker only when it is not in the set. A risen walker set destroyed by the
build before stays so in that save. `rise-loot.txt` PASS 2026-10-24 20:36:
the bandit up as walker FF000EEC, killed again, the activate opened
"(Container)", no "can no longer be activated" in the plugin's log.

Cost: **cheap.**

### 4.16 Tempo - see 4.4; `[Zombies] AttackSpeedMultiplier=0.8`

The attack speed goes through the same sequence-offset mechanism OBVR's
EnemyAttackSpeed uses (OBVR attack-tempo lesson: the weapon speed is
additive in the sequence offset).

Not built, not planned for now (Nadi, 2026-10-24 evening: "wont
implement", maybe later; README). Blocked (2026-10-24 evening, read in OBVR's
src/game/AttackTempo.cpp): slowing a sequence moves its offset back, and a
key in the overlap fires twice (a blow lands twice), so OBVR re-points the
one key-firing call 0x00477AB1 at a wrapper, and it checks the bytes first
and leaves a call that is not the game's own alone. A second copy of that
in this plugin would meet OBVR's patch (or OBVR meet ours) and one of the
two would stand down; with OBVR the walkers fighting the player already run
at its EnemyAttackSpeed (default 75 %). The ways:
1. OBVR applies a factor per actor that this plugin hands it (OBSE plugin
   messaging: the walker records), multiplied with its own - medium, two
   repositories, without OBVR no tempo.
2. One shared tempo: OBVR's AttackTempo moves into this plugin or a small
   plugin of its own both use - medium to large.
3. The walkers' own combat style in the ESP (TESCreature +0x118): attack
   chance and hold times, no patch - small, but it is the frequency, not
   the tempo of a blow.
Recommended: 1 for the tempo, 3 at once for the frequency if wanted.
Nadi decides.

Cost: **medium** (was given as small before the patch conflict was read).

### 4.17 Only a lost head ends a walker - `[Zombies] DieOnlyBeheaded=1`, `RiseAgainSeconds=20`, `RiseAgainJitterSeconds=10`

What (Nadi, 2026-10-25: "walker nur sterben wenn geköpft. default an.
ansonsten resurrect[en] die immer"): a walker that falls gets up again, every
time, until its head is cut off. Every walker: the replaced spawns (4.6), the
extra spawns (4.13) and the risen (4.1). It is the walkers' own answer to
MOO's zombie resurrect (7a), which stays off.

How (Proposed, nothing built):

1. **Down, then up.** The death watch (built) notes a walker's death and puts
   it on 4.1's waiting list (co-save) with `RiseAgainSeconds` plus a random
   share of `RiseAgainJitterSeconds`. When its time comes the plugin raises
   the same reference with the engine's Resurrect(1, has3D, animate=1) - the
   call 4.1 uses, measured on a zombie creature (section 7, question 7). No
   swap and no clone: it is a walker already. Its health comes back with the
   resurrect (4.5's share stays on the record).
2. **The head.** OBVR cuts heads off (OBVR `docs/dismemberment-spec.md`,
   designed 2026-10-25, not built): on the blow that kills, with a blade or
   an axe fast enough on the neck, and on a corpse's neck at any time. A
   killing blow to the head with a blunt weapon ends a walker too, as a
   crushed head with nothing cut (decided by Nadi 2026-10-25, a). OBVR tells
   other plugins of both through OBSE's plugin messaging (the actor's
   reference and the part; the same channel 4.10 planned). A walker whose
   head is off or crushed leaves the waiting list for good and is noted dead
   in the co-save; a cut head stays off through OBVR's own co-save. Arrows
   and spells only drop a walker: no part is known for them.
3. **The way to end one**, follows from 1 and 2: down a walker with anything,
   then take its head off before it gets up - or take it off, or crush it,
   with the blow that downs it.
4. **Order.** OBVR cuts once it sees the death, so its message comes after
   the death watch, and the rise is seconds later: the cut lands in time. A
   cut for a walker not on the list (already ended, or never a walker) is
   ignored.
5. **Without a head cutter a power attack ends a walker** (decided by Nadi
   2026-10-25, b). No OBVR, OBVR with `Dismemberment=0`, or an OBVR that
   sends no cuts: a walker whose killing blow was a power attack (any melee
   power attack, fists included) stays down; every other death rises again.
   OBVR tells at its start (and after an INI reload) whether it cuts; with a
   cutter the head rule of 2 applies instead, never both. How the plugin
   knows the blow was a power attack (Proposed): xOBSE has `IsPowerAttacking`
   and an `OnAttack` event at the start of every melee attack (deepwiki
   llde/xOBSE, EventManager); the plugin notes the attacker's power attack
   at OnAttack, clears it at OnRelease, and reads the killer's note at
   OnDeath. Unverified: what `IsPowerAttacking` reads, whether the killer's
   attack is still on at the death, and whether OBVR's power strike by a
   fast swing counts (it hands the engine a power attack; the harness will
   tell).
6. **The others.**
   - 4.15 NoLoot: the corpse is set destroyed only at the end for good (the
     cut, the crushed head, or without a cutter the power attack's kill),
     not at each fall; whether a destroyed reference can be raised is
     not read, so the plugin never tries it.
   - 4.8 Fame: counts the ends, not the falls.
   - 4.10 HeadshotsOnly: a different rule (body hits do no harm); both can be
     on - then only head hits drop a walker and only a cut ends it.
   - Unloaded cells: a walker due while its cell is unloaded gets up when the
     cell loads, as 4.1's waiting list does.

Decided by Nadi 2026-10-25: (a) a killing blunt blow to the head ends a
walker; (b) without a cutter a power attack's kill ends it; (c) no cap on
the rises - a walker gets up as often as it falls.

Cost: **small** here - the death watch, the waiting list, the resurrect and
the co-save are built; new are the message receiver, the power attack note
(two events, one flag per attacker), the choice between the two rules and
the NoLoot and Fame ordering. The cut itself is OBVR's, **medium to large**
and not built; the messages in OBVR are cheap. Until the cut exists the
power attack rule of 5 is the one in play, in VR too.

Tests: pure - which rule is in play (a cutter or not); a walker's death
goes on the list, a person's does not (4.1 decides those); with a cutter a
cut or a crushed head takes it off, a late or foreign message is ignored, a
power attack does not; without one a power attack's kill takes it off,
a normal blow, an arrow or a spell does not; no cap - the tenth fall rises
like the first. Harness with a stand-in for OBVR's message (a debug console
command on the plugin's opcode base 0x2B00 that hands the plugin a cut for
a reference): a walker killed with `kill` gets up after the delay, again
and again; given the cut, it stays down and cannot be looted. Without the
stand-in: the player kills a walker with a normal attack (it rises) and
with a power attack (it stays down) - flat, and with OBVR's fast swing.
Then with OBVR's cut: the corpse cut of OBVR's spec.

### Later (named by Nadi, not in the first build)

- Barricades (28 Days' door locks by Security skill, walkers breaking them).
- Extra guards around the cities.
- Guard parties that attack Oblivion gates and always lose when the player
  is not there.

## 5. Scenarios - `[Scenario] Preset=`

All of 28 Days' plus one. A preset sets the switches above and the
population model; `Custom` leaves the INI as written.

| Preset | What |
|---|---|
| `Off` | the plugin sleeps |
| `Hive` | one random city starts infested; the horde spreads from it to the next city every few days |
| `Horde` | one roaming horde moves city to city, leaving "sleeper" walkers behind |
| `PatientZero` | one infected NPC somewhere; the infection spreads by bites until it is noticed |
| `DawnOfTheDead` | every city's population at its maximum from the start |
| `Arcade` | DawnOfTheDead with double populations and extra spawns at 100 % |
| `OblivionCrisis` | **new**: walkers spawn only around the Oblivion gates (the lists `LL1DaedricBeastOblivionGate100` family, 107 spawn points, and the gate cells) and move out from there into the world; cities are hit only by hordes that walked there from a gate. Closing a gate stops its source. |

The scenario's state (populations, the horde's city, the gates' sources) is
the co-save's; a preset change restarts it.

## 6. Harness scenarios

Run on OBVR's harness from the OBVR folder:

```
python3 tools/hand-script-run.py ../OblivionWalkers/harness/<name>.txt --headless \
    --artifact-root ../OblivionWalkers/artifacts/hand-script
```

| Scenario | Measures | Needs |
|---|---|---|
| `spawn-count.txt` | a world spawn point with a count-3 entry spawns three (4.6) - PASS 2026-10-23 15:29, with the caveat in section 2 | the roster line (OBVR, 2026-10-23) |
| `rise-baseline.txt` | the engine's resurrect on a corpse through the console, and `{actor}` (4.1) - PASS 2026-10-23 16:48 | the roster line |
| `rise-walker.txt` | the plugin's rise after N seconds through xOBSE's OnDeath, the frame tick and the engine's Resurrect - PASS 2026-10-23 17:54 (run 1 FAIL: the handler must be registered after the plugins' load) | the plugin |
| `rise-getup.txt` | the get-up animation after the plugin's Resurrect, in pictures, and the engine's dead flag timed - PASS 2026-10-23 18:22 (run 1 looked at a wall) | the plugin |
| `rise-races-playable.txt` | the race filter at its defaults: a Redguard and a Khajiit bandit get up, a Dremora churl stays down - PASS 2026-10-23 18:35 | the plugin |
| `rise-races-humans.txt` | `OnlyHumans=1`: the Redguard gets up, the Khajiit and the Dremora stay down - PASS 2026-10-23 18:44 | the plugin |
| `rise-creature.txt` | the plugin's own creature rise with the defaults (`AsCreature=1`): the walker asked for on the corpse, found, the corpse hidden, the walker dropped, DropSeconds later the items moved and the walker raised - PASS 2026-10-23 20:44 (run 1: xOBSE's answer to PlaceAtMe read as a refusal, the man rose and the walker killed him again) | the plugin, the ESP active |
| `rise-creature-proof.txt` | the corpse swapped for CreatureZombie1 by console lines alone: placed on the corpse, the corpse disabled, the zombie dropped, the items moved, `resurrect 1` - the zombie gets up at the spot (7.7) - PASS 2026-10-23 20:35 (run 1 a guessed reference, run 2 a self-transfer that froze the engine) | nothing of the plugin ([Rise] Enabled=0 for the run) |
| `rise-zombie-race.txt` | the clone's race set to the ESP's Walker race before the raise: a Redguard up with Imperial skin, his own face (4.1 step 3, 7.2's last question) | the plugin, the ESP active - run 1 19:50: the race on the clone, the old skin (Resurrect keeps the 3D); run 2 19:54 PASS: Update3D after the raise, pale Imperial skin in the picture; run 3 19:59 PASS: the plugin's own rebuild 3 s after the raise (`LookAfterSeconds`), pale skin in both pictures |
| `rise-walker-clone.txt` | the same on a clone of the fallen one's base record (`[Rise] CloneBase=1`): the base swap on a corpse (7.2) - PASS 2026-10-23 18:07 (run 1 FAIL: CopyFrom is virtual 45, not 33; run 2 the swap worked, the runner's verdict fell to the scenario's own reject line) | the plugin |
| `walker-speed.txt` | a walker's ground speed and that it never runs (4.4) - PASS 2026-10-24 10:26 (Walk=1) and 10:29 (Walk=0); SpeedMultiplier=2 PASS 21:00, 69 to 106 units/s | the plugin |
| `spawn-replace.txt` | a placed goblin list yields only walkers, more than one per spawn, and a dead walker opens no container (4.6, 4.15) - PASS 2026-10-24 11:14; the control with NoLoot=0 opened it (11:15, FAIL on the reject as meant); with the ten-slot ESP (MinCount 1, MaxCount 3) PASS 20:38: 8 walkers, no goblin, eight set destroyed; MaxCount 10 PASS 20:47: 15 walkers | the plugin, the ESP active |
| `rise-loot.txt` | a walker risen from a corpse opens with the fallen's items after its own death, NoLoot=1 (4.15) - PASS 2026-10-24 20:36 | the plugin, the ESP active |
| `fame.txt` | a walker killed by the player earns fame, one killed without a killer does not (4.8) - PASS 2026-10-24 21:01 | the plugin, the ESP active, FamePerKills=1 by hand |
| `chapel.txt` | a bandit killed in Anvil's Chapel of Dibella stays down (4.7) - PASS 2026-10-24 21:13 | the plugin |
| `rise-swap-look.txt` | the walker takes the corpse's place: it settles unseen, the two change places, it lies, it gets up (4.1) - PASS 2026-10-25 09:24, pictures a second apart | the plugin, the ESP active, timings by hand |
| `rename.txt` | the walkers named Name, the game's zombies renamed with RenameVanilla=1 (4.2) - PASS 2026-10-24 21:31 | the plugin, the ESP active, RenameVanilla by hand |
| `chapel-walker.txt` | a walker placed in a chapel is taken out of the world (4.7) - PASS 2026-10-24 21:21, 21:26 (made-there rule) |
| `chapel-follow.txt` | a walker met outside and moved into a chapel stays (4.7) - PASS 2026-10-24 21:27 | the plugin, the ESP active | the plugin, the ESP active |
| `spawn-extra.txt` | ExtraPercent=50: walkers beside a list's own people (4.13) - PASS 2026-10-24 21:15 | the plugin, the ESP active, ExtraPercent by hand |
| `spawn-replace-person.txt` | five people set to 1 by hand become walkers in a list of people (4.6) - PASS 2026-10-24 20:49 | the plugin, the ESP active, the INI lines set by hand |
| `spawn-replace-outdoors.txt` | the engine's own spawn points at Tamriel -29,5 yield walkers, the deer and the people stay (4.6) - PASS 2026-10-24 11:17 | the plugin, the ESP active |
| `siege-city.txt` | walkers come out of a city's doors up to the population (4.12) | the plugin |

Each plugin scenario `expect`s the plugin's own `Walkers:` log lines and the
roster line's counts.

## 7. Open questions, in the order they block

1. Count at a world spawn point (`spawn-count.txt`): **answered 2026-10-23**, honoured
   (section 2, with its caveat). 4.6 keeps its count shape; the plugin's injection
   into LL0ZombieVariant at Hame is the clean proof.
2. The base swap on a corpse followed by resurrect: **the engine takes it**
   (`rise-walker-clone.txt`, 2026-10-23), and the race swap on the clone
   **shows** once the 3D is built anew (`rise-zombie-race.txt` run 2, Update3D
   after the raise: Imperial skin on the Redguard, his own face and gear).
   Answered; left: the zombie textures themselves, and whether a rebuild on
   the corpse before the raise keeps the ragdoll pose (then the get-up would
   already show the new skin).
3. A creature record with its own animation folder (4.4). Blocks the walk;
   the fallback is an engine hook.
4. Whether created base forms can be destroyed when their reference is gone.
   Decides whether the cap in 4.1 is temporary.
5. OBVR's hit-place message (4.10) - a change in OBVR too.
   The cut message of 4.17 is the same channel, and needs OBVR's
   dismemberment (OBVR docs/dismemberment-spec.md, not built).
6. The skill-use entry point (4.3) - when its phase comes.
7. Does a zombie creature get up from its ragdoll with Resurrect(1, has3D, 1)
   as a person does? **Yes** (`rise-creature-proof.txt` run 3, 2026-10-23 20:35:
   placed on the corpse, the corpse disabled, the zombie dropped, the items
   moved, raised - low on its hands a second later, upright at 2.5 s, at the
   player at 7 s). Two things learned on the way: the new reference's ID is
   not predictable (the dropped weapon takes the next ones), and RemoveAllItems
   from a container into itself freezes the engine (run 2) - the corpse is
   disabled before anything addresses the nearest actor. The plugin takes this
   path.

## 7a. Open bugs

- **RESOLVED 2026-10-25 09:24 (1883c49) - "Zombies ersetzen nicht die leiche
  am boden sondern schweben fallend auf den boden ganz runter und stehen dann
  erst auf"** (Nadi, Windows, 2026-10-24 evening; relayed by the OBVR
  session with Nadi's OBVR log D:/Modding/OBVR/artifacts/scratch/
  OBVR-win-20261024-2229.log): the swap showed the walker standing on the
  corpse and killed it there, so its fall was seen, the corpse already gone.
  Now it drops unseen and the two change places once it lies (section 4.1,
  the swap's look; `rise-swap-look.txt` PASS), and since 2026-10-25 it is
  laid into the corpse's place or pose first (MatchCorpse).
- **"Zombies respawnen unendlich"** (the same session) - the walkers getting
  up again and again: MOO's zombie resurrect, the entry below.

- **Walkers get up again and again with MOO** (Nadi, Windows, 2026-10-25):
  not the plugin - its death watch keeps a dead walker down ("stays down (not
  a person)"). Maskar's Oblivion Overhaul, read in its ESP's script sources:
  MOOInitNpcTokenForCreatureFunctionScript gives every creature whose model
  path holds `\Zombie\` a brain (MOOZombieBrain, MOO's 0F8B6D) at
  `ini_zombie_resurrect` percent (70), and MOOTokenZombieScript (token
  MOOTokenZombie, 042C18) raises a dead zombie that still carries it
  (`me.Resurrect 1`). The walkers are copies of CreatureZombie1..4 with their
  models, and NoLoot keeps the brain out of reach. For now (Nadi): MOO's
  `set MOO.ini_zombie_resurrect to 0` in both installs (done 2026-10-25),
  noted in the README. The fix at the plugin is a head script of the
  walkers' own, later (Nadi) - or taking MOO's brain and token off a dead
  walker (small). Not seen in a log: the Windows log was not read (both NTFS
  volumes were dirty after the crash of 2026-10-24 21:40).

- **RESOLVED 2026-10-24 07:12** - the walker a few metres from the corpse, the man up again, old walkers raised: three causes, all measured (`rise-creature.txt` runs 3 to 5): the finder's radius missed the placed walker and matched old ones (now identity: the references before the PlaceAtMe against those after); a kill handed to the walker before its model existed left a "dead" body sliding at 400 units/s (the kill now waits for the model and lands in its first frame); and the two seconds of walking before the old finder gave up were the "few metres". The walker now drops within 40 units of the spot. The original entry, for the record:
- **The walker appears a few metres from the corpse in Nadi's game, not on it**
  (2026-10-23 21:23, his log): "walker 10000801 asked for at 3268 9112 40" and
  "no walker appeared within 2 s - he gets up as himself" - the walker existed
  (the player killed it later) but stood outside the finder's 150 units; the
  second rise found walker FF000F33 within the radius, killed it at once and
  raised it at 3983 8745 97, 550 units from the corpse - so the engine moves a
  placed creature to a spot of its own in that exterior (the hall's flat floor
  kept it on the corpse). Seen by Nadi as: zombie a few metres off, the man up
  again, later a second zombie and the first one up again. Fix planned
  (cheap): find the new walker by identity (the walker references before the
  PlaceAtMe against those after, no radius), `PlaceAtMe <base> 1 0 0` with an
  explicit distance of 0, and `MoveTo` the dead walker onto the corpse once
  its model stands, before the corpse is hidden; plus OblivionWalkers.log.prev
  so a player's log survives the next start.

- **Narrowed 2026-10-24 08:46** (seven outdoor runs): the disabled corpse is
  always and only the bandit the harness placed with the console's
  `player.placeatme` - in every outdoor run, with and without Ragdolls, MOO and
  both Enhanced Grabbing plugins, at the player's feet and 400 units off; an
  area bandit of the very same base (00069AD1) rose fine, and all 17 people of
  the area that died in those runs became walkers. In the hall the placed one
  rose. So a console-placed actor dying outdoors gets disabled by the engine or
  a remaining plugin some 20 s later - a harness artifact, not what a player's
  kills meet (Nadi's own kills rose). Who exactly I could not verify; the plugin
  leaves it alone.
- **Corpses disabled by something else before their rise** (2026-10-24,
  `rise-creature-outdoors.txt` runs 2 and 4, outdoors at Tamriel -29,5): a
  waiting corpse goes "absent (no 3D)" with its form flags 00000108 ->
  0000090A (0x800, disabled) some 20 s after the death, just before it is
  due; it never rises. The plugin disables a corpse only inside a swap, and
  none ran for these. Who does it I could not verify; the suspects are the
  mods installed 2026-10-23 21:20 (Ragdolls for Oblivion, Maskar's Oblivion
  Overhaul and others). Cheapest step that settles it: one run with those
  mods off. A design choice follows either way - rise from a disabled
  corpse too (place the walker at its last position) or leave it down.

## 8. Phases

1. **Engine proofs** (this week): `spawn-count`, `rise-baseline` (both PASS
   2026-10-23); the plugin skeleton with INI, log, co-save and the rise on a
   clone (`rise-walker`, `rise-walker-clone`, PASS 2026-10-23); the ESP with
   the Walker race, the four Walker creatures and their list
   (`tools/esp-write.py`); the creature rise (`rise-creature`, PASS 2026-10-23
   20:44) - done.
2. **Core**: 4.6 replacement, 4.4 walk, 4.5 health, 4.2 name, 4.15 loot, 4.1
   rise. Harness green for each.
3. **World**: 4.14 infection, 4.7 chapels, 4.8 fame, 4.13 extra spawns, 5
   scenarios with OblivionCrisis, 4.9 Courier.
4. **Feel**: 4.11 horde, 4.12 siege, 4.10 headshots with OBVR, 4.16 tempo,
   4.3 no skill gain.
5. Later list.

## 9. Sources

- Zombie Walkers: Nexus 20485 (description and 1700 of the 2480 comments,
  read 2026-10-23), the author's blog (features, 2016 horde and spawner
  posts, 2017 resurrection and replacement posts).
- 28 Days and a Bit 4: Nexus 33037 (description and all 1373 comments, read
  2026-10-23).
- CS wiki: Leveled Creature, PlaceAtMe, Resurrect, Movement Game Settings,
  Event Handler Functions. UESP: Zombie, Respawning.
- xOBSE (llde/xOBSE) through deepwiki and GameForms.h.
- Oblivion.exe through REA/Ghidra, 2026-10-23: CalcElement 0x0046CC70,
  CalcItemForContainer 0x0046CDE0, ResurrectActor 0x00B0DEA0 / 0x00510150 /
  vtable+0x20C.
- `tools/esm-spawns.py` over Oblivion.esm: `artifacts/esm/`.
