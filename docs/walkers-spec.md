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

**Twenty looks** (Nadi, 2026-10-27: "gibts noch andere zombie modelle?
brauchen mer diversität an zombies" - "go A"): every zombie of Oblivion.esm
is one skeleton (Creatures\Zombie\Skeleton.NIF) with body parts (NIFZ):
hair, head, arms, hands, legs and torso halves, each as 01 or 02, and a neck
plug for the headless. Its CREA records wear twenty different part sets
(read 2026-10-27); the walkers were four of them (CreatureZombie1..4). Since
2026-10-27 the ESP has the other sixteen too, at 0x90E..0x91D
(`tools/esp-write.py` PART_DONORS, `RiseLogic.h` kWalkerLocalIds): each is
CreatureZombie1, 2 or 3 - the one missing the same arm, whose animations
(KFFZ), bound (MODB, MODT) and arm flags fit - with the donor's parts, and
No Head (ACBS 0x8000, UESP Tes4Mod:Mod_File_Format/CREA) where no head is
among them; five are headless. The stats stay one; the spawns' variant list
holds each walker ten times (200 entries), the risen draw one of the twenty,
the speed spreads over twenty paces. 0x801..0x804 stay where they were, so a
save's walkers keep their base. Option C, the s6o6t zombie overhaul on
Nexus (55729), Nadi turned down (2026-10-27: "C ist schlecht").

**MOO's looks** (option B, Nadi 2026-10-27: "go"; one ESP, the race
zombies and MOO's default set, the risen by race): Maskar's Oblivion
Overhaul has its own zombie skeleton (MOO\zombie\skeleton.nif) with 46 part
sets (read off its ESP 2026-10-27): its default 8, Argonian 10, Orc 10, High
Elf 8, Dark Elf 8 (normal, headless and dread), 2 burning. The ESP has 44 of
them at 0x91E..0x949 (`src/WalkerLooks.h`, `tools/esp-write.py` MOO_DONORS;
the burning two not chosen), built as the sixteen above but with the donor's
skeleton, bound, textures, parts and animations (MODL, MODB, MODT, NIFZ,
KFFZ). They name mesh paths only, no MOO form, so the ESP has no MOO master
(deviation from the first plan of a second ESP with MOO as master, put to
Nadi and chosen 2026-10-27: no load order to change, no missing-master
start without MOO). Their meshes come with MOO's archives, so the plugin
uses them only with MOO's ESP in the load order (`FindMoo`):
- **The risen by race** (`RiseCandidates`): a fallen Argonian (00023FE9),
  Orc (000191C0), High Elf (00019204) or Dark Elf (000191C1) rises as one
  of his race's looks; everyone else as one of the vanilla twenty or MOO's
  default eight. Without MOO, or none of a race's looks found: the vanilla
  ones. The rise's log line names the race, the walker and how many looks
  it was drawn from.
- **The spawns** draw from all 64, each walker of a spawn on its own
  (since 2026-10-27, Nadi: "wenn 10 walker auf einmal spawnen dann ist es
  immer der selbe typ ... es sollten 10 unterschiedliche sein"): the variant
  list holds ten count slots, each the any-walker list WalkersAnyWalker
  (0x94A) at a count 1..10; that list holds the vanilla twenty first, then
  MOO's 44, a walker with a head twice, a headless one once (103 entries),
  and carries LVLF bit 2, "calculate for each item in count", so the engine
  draws it anew for each walker of the count (CalcItemForContainer
  0x0046CDE0, read with REA; `spawn-mixed.txt` 15:37: four placements of
  4, 4, 6, 3 walkers, all different looks - before, 14:41, one look each).
  Without MOO the plugin cuts the any-walker list before its first MOO entry
  (`TrimAnyWalkerForMoo`, one link), so it is the 35 vanilla entries.
- **Pace** spreads over the families (`SpeedSlot`: record i takes slot
  i x 37 mod 64), so no race is all fast or all slow.
- **Headless half as often** (Nadi, 2026-10-27: "ja"): 25 of the 64 have no
  head (5 vanilla, 20 of MOO's "Zombie2" and "SummonZombie2" sets), which
  alike would have made two spawns in five headless. A headless walker
  stands once in the any-walker list, one with a head twice
  (`AnyWalkerTimes`), and among the risen's candidates the same
  (`RiseCandidates`). With MOO 25 of 103 entries are headless (about one
  walker in four), without it 5 of 35 (one in seven). 4.17's rule (no
  headless with DieOnlyBeheaded) reads the same flag.
- **Does MOO replace the vanilla zombies?** No (read 2026-10-27): MOO's ESP
  overrides none of Oblivion.esm's zombie records, and its archives hold no
  file under meshes\creatures\zombie or textures\creatures\zombie - all
  168 zombie meshes and 38 textures are under its own moo\zombie folders.
  It adds zombies of its own (on its skeleton) to the leveled lists; those
  the spawn replacement turns into walkers like any other (the
  `Spawns: Zombie [Maskar's Oblivion Overhaul.esp ...] - walkers` lines). A
  loose Data\meshes\creatures\zombie\skeleton.nif of 2026-10-23 in both
  installs is from a mod not traced here; it is the skeleton, not a look.
Not built: MOO's burning zombies, draugr, ghouls and mummies (other
skeletons). Cost medium (as rated); test `WalkerLooksTest` (the table, the
races, who rises as what with and without MOO, which entries the list
keeps), `SpeedSlot` in `ReplaceLogicTest`, harness `rise-moo-races.txt`.

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
pelvis-to-head heading; 2 (then the default) lays each body on a bone the corpse has
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
- Default back to 1 (Nadi, 2026-10-25: "ja mach beides" - back to 1 and read
  the filter): the place alone, 4 to 18 units and 4 to 46 degrees off in its
  runs, against 2's 26 units and sitting on the corpse. Neither live INI
  names MatchCorpse, so the DLL's default holds in both installs.
- The filter read (REA, 2026-10-25; ApartLogic.h has every step) and
  measured: both ragdolls are bipeds (layer 8), parts 1 to 16, not linked,
  one group each (harness 15:27: walker 0xCF, corpse 0xCD) - the rule
  0x008A7F70 lets two groups of bipeds collide, every pair. That run
  (MatchCorpse=1, the corpse lying on): pelvis 4 units off at the raise,
  but turned 77 degrees - the frame between the place and the reveal is
  enough for the overlapping bodies to push.
- KeepCorpseApart built (47e13a7, `[Rise] KeepCorpseApart`, off by
  default): from the frame the walker's model exists - before its drop onto
  the corpse - the corpse's bodies take the walker's group and part 0. By
  the rule, one group of bipeds goes to the bone table by part, and no bone
  row has bit 0, so walker and corpse never meet; with anyone else the
  corpse is decided as before (its layer kept). Written as the engine's own
  setters do (the word, then the wrapper's vtable +0x80 = 0x008B0060,
  updateCollisionFilterOnEntity), inside the Havok lock, and only when the
  rule's code is the game's own and the rule on the live tables says no
  pair meets. Asked again each frame until it takes.
- KeepCorpseApart runs (harness 2026-10-25, one walker each, numbers from
  the log: the corpse's spine and head at the pose against the walker's at
  the raise, artifacts/hand-script/rise-swap-look/<run>/OblivionWalkers.log).
  Every run: "16 ragdoll bodies moved into walker's collision group, part
  0" in under 90 us, the filter log then reads the corpse as 00CF0008
  beside the walker's 00CF0208 and so on - the move took. The pictures
  show no body through the floor.

  | Run | MatchCorpse | Pelvis off | Head off | Turned | The corpse, pose to reveal |
  |---|---|---|---|---|---|
  | 15:27 (apart off) | 1 | 4 | 56 | 77 degrees | - |
  | 18:04 | 1 | 7 | 2 | 9 degrees | did not move |
  | 18:08 | 1 | 16 | 46 | 81 degrees | did not move |
  | 18:06 | 2 | 4 | 10 | 18 degrees | 9 units |
  | 18:09 | 2 | 4 | 12 | 17 degrees | 6 units |

  So (measured): the two no longer push each other - the corpse lying on
  stays put, and with 2 the walker ends on the corpse's spot in both runs,
  where without it it sat on the corpse (13:48). With 1 the walker still
  turned some 80 degrees in one run of two while it lay seen, with nothing
  to push it - so 1 has a second cause the filter does not touch. Suspect
  (not proven): ragdoll bodies on nodes outside the bone list
  (Pose.cpp kBones) are not moved by the place, and their constraints yank
  the moved body round; with 2 the settle lets them follow before the small
  final move. The cheapest step that settles it: log every node of the
  walker's skeleton that carries a body. Open as well: the corpse moved 6
  to 9 units while the posed walker settled (seen, in its 1.5 s) - its own
  settling, or its bodies no longer meeting each other; not read.
- Defaults MatchCorpse=2 and KeepCorpseApart=1 (Nadi, 2026-10-25: "go").
  Harness 18:29 and 18:30 with the defaults: 8 and 13 units off, 14 and 8
  degrees; the corpse moved 15 and 7 units while the walker settled.
- The body census (18:29, every node of both skeletons asked for its body):
  the walker carries 17 bodies, the bone list named 16 - the one it missed
  is "Bip01 NonAccum" (filter 00CF0208: a biped, part 2, the hips' body on
  skeletons whose pelvis carries none); the corpse carries 18, missing
  "Bip01 NonAccum" and "SideWeapon" (00CF4008, switched off). So the place
  and the pose moved every body but the one at the hips, which stayed
  where the walker fell and pulled the rest round; and the corpse's two
  were left in its own group, so the walker still met them.
- Fixed (582b6cf, Pose.cpp BodiesOf): the swap and KeepCorpseApart take
  every body on the skeleton - the tree walked down from the reference's
  root, each node asked for its body - instead of the named list (kept for
  the census). Harness 18:34, 18:35, 18:37 (MatchCorpse=1) and 18:39,
  18:40 (MatchCorpse=2, the defaults), all PASS:

  | Run | MatchCorpse | Pelvis off | Head off | Turned | The pose's drift while settling | The corpse moved |
  |---|---|---|---|---|---|---|
  | 18:34 | 1 | 0 | 1 | 1 degree | - | 0 |
  | 18:35 | 1 | 0 | 0 | 0 degrees | - | 0 |
  | 18:37 | 1 | 1 | 1 | 2 degrees | - | 0 |
  | 18:39 | 2 | 0 | 1 | 1 degree | 2 degrees | 0 |
  | 18:40 | 2 | 0 | 1 | 1 degree | 5 degrees | 0 |

  Measured: the walker now lies where the corpse lay, to a unit and a
  degree, in both modes; the copied pose holds while it settles (2 and 5
  degrees, where it turned 46 to 83 before); the corpse lying on no longer
  moves. The pictures of 18:40 (s10, s12) show the walker in the corpse's
  place and much its pose. Derived: the two causes were the corpse's
  collision and the body at "Bip01 NonAccum" left behind - the "joint
  limits" and "bone axes" suspects above are dropped. Since the pose now
  holds, the settle (SettleSeconds 1.5) may no longer be needed - with 0
  the walker would show a frame after its pose; not run.
- SettleSeconds=0 by default (Nadi, 2026-10-25: "go"): the walker is laid
  into the corpse's pose and shown the next frame, no second placement.
  Harness 18:47 PASS: shown with its pelvis 1 unit off the corpse's (the
  pelvis node no longer lags - its body at NonAccum moves with the rest),
  1 unit and 3 degrees off at the raise, the corpse not moved; the pictures
  s09 (the corpse) and s11 (the walker) show the same body in the same
  pose. The second run (18:49) FAILed before the world - the harness's
  script did not start within 240 s, as at 13:49; nothing of the plugin
  ran. Harness 19:05 and 19:07 PASS: shown 0 and 1 unit off the corpse's
  pelvis, 2 units and 5 and 1 degrees off at the raise, the corpse not
  moved. Three runs: holds.
- Harness 19:54 (rise-swap-look, the defaults, OblivionFrameFixes newly in
  the plugin folder): shown 1 unit off the corpse, but 25 units and 26
  degrees off at the raise - the freshly laid pose moved while it lay
  seen for LieSeconds (3 s in the harness, 1 by default). So with
  SettleSeconds=0, one run of four slid visibly; the settle (1.5 s, unseen)
  absorbed that in both runs that had it. Derived: not FrameFixes (it
  changes how scripts find their variables; the swap reads none). Open:
  whether 0 stays the default - Nadi's call.
- SettleSeconds=1.5 by default again (Nadi, 2026-10-25: "ich will aber
  keine Lücke in der keine Leiche da ist und auch kein Walker der
  aufsteht"): the corpse lies seen while the walker settles unseen, and the
  two change places in one frame (RevealSwap: the corpse's Disable and the
  walker's SetActorAlpha 1 in the same call) - never a moment with neither.
  The harness's pictures are a second apart, so that one frame is read in
  the code, not seen. Harness 20:24 PASS: 1 unit and 1 degree off at the
  raise, the corpse not moved.
- **Open bug, fix built 2026-10-26, not yet heard** - "die walker machen
  bereits einen sound wenn sie unsichtbar spawenne das stört. der sound kann
  kommen aber erst wenn sie daliegen und aufstehen" (Nadi, Windows,
  2026-10-25 evening). Read with REA (SoundLogic.h has the addresses): the
  engine's death 0x006005F0 plays a creature's Death sound inside the call,
  drawn by TESCreature::GetSound 0x0051CEC0 from the walker's sound lists
  (CreatureZombie's, through its sound base); the swap kills the walker
  unseen through the console's Kill, so the groan came from an invisible
  body (derived - the recording that would prove it is still to be made).
  Built (bd94d82, then `[Rise] SoundAtGetUpPercent=60` on Nadi's word,
  621e75a): for that one Kill the death list's head is emptied and put
  back, so no sound is drawn; the sound it would have drawn is drawn just
  before and, at 60 in 100, played with the console's PlaySound3D on the
  walker when it gets up. `[Rise] HoldDeathSound=0` is the test switch
  that leaves the death sound to the engine (d82ff3d).
  Harness 2026-10-26 07:21, 07:36, 07:37 (held) and 07:41 (HoldDeathSound=0),
  all PASS: the log reads the walker's sound lists and the drawn sound
  000A655C each time, "its death list held back for the kill" and the
  PlaySound3D at the raise - so the data path is the read one (measured).
  Not heard: the game's audio recording (parec on its stream, aligned by the
  QPC ticks now in the swap's lines) stayed at the noise floor in every run,
  the bandit's death included - WirePlumber restores the stream
  "TES4: Oblivion" muted (`~/.local/state/wireplumber/stream-properties`,
  "mute":true), so a muted stream is all a recording gets. Whether the
  groan is gone at the unseen kill and comes at the raise is for an ear in
  the game (Nadi, Windows) - or a recording with the stream unmuted and
  routed to a null sink, which changes WirePlumber's saved state.
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

Read with REA 2026-10-27 (Nadi: "go A", then "auch klären ob VR Schläge
überhaupt skill üben"): the player's skill use is PlayerCharacter's virtual
at vtable+0x39C (0x00668C30: experience 0x00668B30, the raise 0x00668070),
read at 19 call sites. Those of a fight:
- **Armor of the hit** - in AttackHandling 0x005FEBF0 at 0x005FFC90: the
  hit actor (ESI, the target since 0x005FF005) uses Light (0x1B) or Heavy
  Armor (0x12); the attacker is EDI.
- **Block** - 0x005F5B70 (0x0F, "%.20s blocks %.0f%% of %.20s's blow!").
- **The weapon skill** - not in AttackHandling but in the attack update
  0x005FCAB0 at 0x005FD83C: `UseSkill(weapon skill via +0x21C, 0)`, EBX
  times. OBVR's hand-tracked strikes call AttackHandling directly
  (OBVR GameAddresses.h kAttackHandling), not the attack update - so,
  derived, **a VR swing trains no weapon skill at all**, only the armor of
  the one hit. `blade-body.txt` 16:50 fits: a swing went into the beggar and
  no Blade use came (the hook installed and logging blow skills) - one run;
  `skill-hits.txt` (an NPC's blows on the player) is to show the hook seeing
  uses at all.
- **Acrobatics** 0x005EFAE0, **magic** 0x00671620 and others.
xOBSE's OnHit comes through the engine's MarkEventList (its kMarkEvent
hook), which AttackHandling calls on the target's script events - likely
only for actors with a script (derived from xOBSE's EventManager via
deepwiki; no OnHit line came in four runs). So the hook reads the context
from the registers of the call site instead where it can (EDI at 0x005FFC90).

Built and run 2026-10-27 (`SkillLogic.h`, the naked `SkillUseStub` in the
player's vtable slot 0x00A73DA8 notes the caller's EDI): `skill-hits.txt`
20:47 - a bandit's blows on the armored player: "skill 12 use 0 (from
005FFCA4, the armor of a hit, the attacker FF000EDF) - trained", twelve
times; 20:51 - a walker's: "the attacker FF000EEA - HELD BACK", every time.
So the walker's hits on the player train no Heavy Armor, anyone else's do
(measured). OnHit came for the player as the hit actor (the "Hit:" lines,
with the same attacker as EDI). OBVR's hand-tracked strike calls
AttackHandling directly and no skill use (read in OBVR src/game/MeleeHits.cpp:
`AttackHandlingFn(kAttackHandling)(player, ..., actor)`, no call of the
skill slot) - so a VR strike trains no weapon skill at all (derived from
that and the engine's reading above; blade-body.txt 16:50, a swing in the
beggar, no Blade use).

Since 2026-10-27 evening (Nadi: "OBVR bekommt: VR-Schläge sollen Waffenskill
trainieren mit einer Möglichkeit, Records zu übergeben, die dies nicht tun
... du baust für die flat Leute ohne OBVR diese Option in Walkers"):
- **Block**: in 0x005F5B70 (thiscall, ECX kept in EBP the attacker, the
  blocker in ESI - its debug text "%.20s blocks %.0f%% of %.20s's blow!"
  names them so) the Block use returns to 0x005F5C0B; the stub notes EBP
  too, a walker attacker holds it back. Built, not yet run (the harness's
  left trigger did not block without hands, 21:44).
- **The weapon skill of a landed blow**: AttackHandling counts a landed hit
  on the attacker (0x005FF53D reads the count via +0x214, 0x005FF54B sets
  it +1 via +0x210, the target in ESI); the attack update trains the weapon
  skill that many times and resets the count; OBVR's strikes, since OBVR's
  `StrikesTrainSkills`, do the same after their AttackHandling. So the
  player's vtable slot 0x210 (0x00A73C1C, the engine's 0x0066B0D0) holds a
  stub that notes ESI: the call returning to 0x005FF54D with a walker there
  is not passed on - the hit is not counted, no weapon skill for it, flat
  and VR alike. Every other call (the resets) passes through. Seen in
  OBVR's runs of its `StrikesTrainSkills` (d776f25, OBVR-wt-holster
  artifacts/hand-script/skill-train/20261027-215211 and
  skill-train-walker/20261027-215327, this plugin's log read there): the
  beggar - "landed hit on FF000EE5 counted to 1 - counted", then "skill 0E
  use 0 ... - trained", four strikes; the walker 0x801 - "landed hit on
  FF000EE0 ... NOT COUNTED (a walker)", no Blade use. The flat attack (the
  attack update reading the same count) is not run: the harness's right
  trigger made no attack without hands (21:44, "0 attack actions
  begun"); it rests on the same counting site (read). The VR swings of
  blade-body and skill-walker no longer report "a swing went into someone"
  since OBVR's StrikeAtContact (they strike at the touch; OBVR's session).
- **OBVR's exceptions**: Walkers sends OBVR (xOBSE messaging, receiver
  "OBVR", type 0x58454B53) its 64 walker bases with NoSkillGain on, an empty
  list with it off, again on a LINK change. With OBVR d776f25 (its
  listener): "64 walker bases sent ... dispatched: 1", OBVR "Skill
  exceptions: 64 base(s) from OblivionWalkers".
- Corrected: OnHit does come for an unscripted target - the beggar's
  lines in skill-train read "the frame's hit this frame: FF000EE5 by
  00000014"; the earlier runs without OnHit lines landed no hit at all.


### 4.3b No Ultimate Leveling experience from walkers - `[Zombies] NoLevelingXP=1`

What (Nadi, 2026-10-27: "da ich ultimate leveling by maskar nutze: füge
eine option hinzu, dass für diese mod zombie kills keine erfahrung
bringen"): with Maskar's Ultimate Leveling's experience system, a walker's
death gives no experience.

How (built 2026-10-27, `LevelingLogic.h`), read in Ultimate Leveling.esp's
script sources: its OnDeath handler (ULVLOnDeathFunctionScript, set at
every game load while ini_leveling_system is 2) computes basexp * levelmod
* toughnessmod * share, the toughness modifier taking (3 * Confidence +
Aggression) / 400 of the dead at that moment, and gives experience only
"if ULVL.XPmod > 0". The plugin's own OnDeath handler sets a dead walker's
Confidence and Aggression to 0 (the console's SetAV), so the experience is
0 - none given, no "dies" message. **Deviation from this spec's own rule**
(4.1 step 1: nothing of the actor touched in the frame of the death, after
Zombie Walkers' crash), named: the value has to be there when Ultimate
Leveling's handler of the same event reads it. MOO's and Ultimate
Leveling's handlers touch the dead in that event too (a token given, one
removed). It rests on the plugin's handler running first: xOBSE calls an
event's handlers in the order they were set (deepwiki on llde/xOBSE,
EventManager's CallbackList), and the plugin sets its own at the main menu,
before Ultimate Leveling's at the game load - seen since. Harness
`leveling-no-xp.txt` 2026-10-27 16:23 PASS (Ultimate Leveling 1.9.5.2 from
Nadi's Windows install, activated in the Linux load order for the run
only): the walker killed by the player "Ultimate Leveling's XP 0 before, 0
after", the bandit "0 before, 38 after" - ULVL.XP read off its quest a
second after each kill (`QuestVars.cpp`).

Oblivion XP Update 4.5 too (Nadi, 2026-10-27: "auch für oblivion xp update
brauchen wir was"), the same switch: read in its ESP, OBXPfnOnDeath (set by
ObXPMainScript at the game load) gives a kill by the player baseXP times
bonusXP, bonusXP = the dead's base Health + Agility + Endurance +
Intelligence + Personality + Speed + Strength + Willpower over the
player's, times multXPKillBonus (1); the plugin sets the dead walker's eight
to 0. It still shows "Gained 0.00 XP. Killed Walker" - it has no check for
nothing gained, and its message buffer is a script array the plugin does
not touch. With multXPKillBonus 0 in its INI the bonus is 1 and walkers
give XP again (not a default). `leveling-no-xp.txt` 16:35 PASS with Oblivion
XP alone (its ESP, its Ini\Oblivion XP Settings.ini and strings, not its
menus, for the run only): the walker "currentXP 0.00 before, 0.00 after",
the bandit "0.00 before, 656.71 after".

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

### 4.9 Black Horse Courier - `[News] Courier=1`, `RecentDays=3`, `HeadingHoldDays=0.5`

What: the broadsheets in the inns and taverns, and in the Courier's own
office in the Imperial City, carry the latest from the outbreak: the outbreak
in general, a horde heading for a city, walkers at a city's gates, or several
cities at once - each with what the Legion asks of the citizens.

Built 2026-10-26 (Nadi's choices: the vanilla places, "attacked" = walkers at
the gate and its guards out):

- The ESP ships 18 pre-written editions, BOOK records 0x8FC..0x90D
  (`tools/courier-editions.py`, written by `tools/esp-write.py` as copies of
  Broadsheet01VampireNest's record with their own text, so the same
  `Clutter\Books\Broadsheet01.NIF`): the outbreak 0, a horde heading for
  each city 1-8, each city's gates attacked 9-16, several cities 17, the
  cities in `CityGates.h`'s order. What the Legion asks matches what the
  walkers do in the game (they walk, the chapels are safe, what they carried
  is spoiled, the guard comes out of the gates).
- The places are Oblivion.esm's own broadsheets in 15 inns and taverns and
  the Courier's office and its upstairs, 47 references
  (`tools/courier-refs.py` writes `src/CourierRefs.h`); no cell of the ESP is
  edited, the broadsheets in people's houses keep the old news.
- Once a second (`src/News.cpp`), each of those references the engine has,
  whose base is still its Oblivion.esm broadsheet or one of the editions, is
  set to the edition `NewsLogic.h` chooses; a reference another mod gave a
  different base is left alone. Whether a save keeps the base set is not
  measured; either way the plugin sets it again within a second of a load.
- The choice (`ChooseEdition`): two or more cities attacked within the last
  `RecentDays` game days - several; one - that city's attack; else the city
  the biggest group of walkers near the player heads for (`HordeLogic.h`
  `HordeHeadingGate`) - a horde heading there; else the outbreak in general.
- A sighting is held (`NoteHeading`, `HeadingHoldDays` 0.5, built
  2026-10-27): the city a horde was last seen heading for stays the news
  for half a game day after the last sighting; a newer sighting toward
  another city replaces it, none seen keeps it. Found in Nadi's Windows run
  of 2026-10-26 22:28 (artifacts/win-run-2026-10-26): the sheets went
  between the outbreak and "a horde heading for the Imperial City" 7 times
  in a few game minutes, whenever the groups near the player were out of
  reach for a moment (a load, a door). The 0.5 is a proposal, not measured
  against play; `HeadingHoldDays=0` is the old behaviour.
- "Attacked" is what the plugin knows (`GuardLogic.h` `CityAttacked`): one of
  the city's pool guards out at a gate with walkers near it (4.11b). The last
  attack's game time per city goes into the co-save, record `NEWS` version 2
  with the last sighting's city and game day after them (version 1, the
  attacks alone, is still read).
- The game time is GameDaysPassed's whole days plus GameHour / 24 (GLOBs
  0x39 and 0x38, `NewsLogic.h` `GameTimeDays`): GameDaysPassed counts whole
  days only - the harness read 1.000 while GameHour ran from 12.927 to
  13.503 (2026-10-26 20:03 and 20:10), so it alone would let an attack stay
  news up to a day too long or too short.

Deviations from the first plan, and why:

- **Not on entering the inn, but once a second while loaded.** Entering a
  cell has no callback the plugin holds; the one-second look costs 47 form
  lookups and is the same to the reader.
- **Where a horde heads is known only near the player.** The horde's groups
  are the walkers on the process lists (4.11); a horde far away is not
  simulated, so "a horde heading for" reports the groups the player could
  meet.
- **No siege yet.** Once real sieges exist (4.12, not built), a city under
  siege belongs in the Courier too - its own editions or the attack edition
  counting a siege as an attack (Nadi, 2026-10-26: "das müssen wir aber noch
  ändern dann wenn wir echte belagerungen haben").

Test: `tests/NewsLogicTest.cpp` (every choice, the RecentDays edge, the
game clock, the co-save image), `CityAttacked` in `GuardLogicTest.cpp`,
`HordeHeadingGate` in `HordeLogicTest.cpp`; harness `news-courier.txt` -
PASS 2026-10-26 20:10: the outbreak in the Count's Arms, a horde heading
for Anvil, then walkers at Anvil's gates; in the Courier's office the
broadsheet opens on "Walkers at the Gates of Anvil!" with the Courier's
header (artifacts/hand-script/news-courier/20261026-201034, shot 08). Of the
47 references 6 are persistent (the engine has them from the start, set
even in the main menu), the other 41 come with their cell.

Cost: **small** (writing the editions was most of it).

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

Built 2026-10-26 (Nadi: "go horde"; dd5761f, `[Horde] Enabled=0` until the
runs show it holds; HordeLogic.h decides, Horde.cpp carries it out):
- The ESP (tools/esp-write.py) carries 16 groups' records: WalkersHordeTravelNN
  (Travel to WalkersHordeMarkerNN, must reach it, 256 near - a copy of
  SE32HlovalReturnStart), WalkersHordeFollowNN (Follow, must complete, 300
  behind - FollowPlayer without its defensive combat, so a follower still
  attacks), and the 16 markers, persistent XMarkers in the interior holding
  cell WalkersHordeMarkers.
- Once a second the walkers on the process lists are read: fit is alive, a
  high process, its model there, outdoors (the cell's flags +0x24 bit 0,
  0x004C9CF0), not fighting (vtable +0x334) and not in a swap; their
  package now (the process's vtable +0x184) - the reads OBVR's leading by
  the hand uses (OBVR src/game/Lead.cpp).
- A fit walker in no group joins the nearest leader within JoinRadius
  (1500) with room (GroupMost 8), highest rank first; else it leads a group
  of its own: its marker LeadUnits (2000) ahead along a random heading
  (MoveTo the marker with an offset from the leader), its travel package
  given a frame later (AddScriptPackage). Followers get the group's follow
  package, its target set to the leader (TESPackage +0x28, as xOBSE's
  SetPackageTarget). Reached (within 300) the marker moves on, at most
  TurnDegrees (60) off the last heading; no nearer for StaleSeconds (30), a
  new heading anywhere. A lost leader is replaced by the follower of the
  highest rank; leaders within half JoinRadius merge into the higher rank's
  group. Packages lost (after a fight) are given again.
- Two choices against the text above: "sees another" is taken as "within
  JoinRadius" (no line of sight is read); dungeon walkers are left alone by
  leaving every interior alone.
- First run, harness 2026-10-26 08:33 (horde-walk.txt, Tamriel -29,5, 79
  walkers loaded there by the spawn replacement): "Horde: on", 14 groups
  formed, 55 follow orders; the placed walker nearest the player walked off
  at about 27 units a second (124, 339, 600, 871, 1158 units away at the
  ten-second marks). Open: two leaders were given their travel package again
  every second (30 and 16 times in the log's first 200 lines) - they do not
  read as running it; the summary lines added after (leader's way to its
  marker, its package, the followers' distance) are for the next run.
- Harness 09:24 (the summaries, every 5 s for 95 s; six groups summed up):
  the leaders ran their travel package in 18 of 19 summaries and walked
  1360 to 3010 units, their way to the marker down from 2000 to 455-1507;
  every follower ran its follow package, on average 350 to 520 units from
  its leader. The two leaders given their package every second ran packages
  made at runtime (FF000F41, FF000F42 - the engine's or a script's) that
  took ours back each time. Fixed (3887371): a walker on a runtime package
  is left alone until it runs its own or ours again, and a package given
  again is held back 5 s per walker. Harness 09:28: given again at most 7
  times per group in the run (was 29), followers on their package 100 %,
  leaders 14 to 18 of 19, the same distances.
- On by default since then (Nadi's "go horde"; the spec's `Enabled=1`).
  Not judged yet: the feel in the headset, and the cost - the step reads
  some 80 walkers once a second and gives packages through the console.
- **Fixed 2026-10-26 - the game froze** (`gate-guards.txt` 11:12 and 11:17,
  near Anvil with some 60 walkers and the gate guards fighting them; both
  times the main thread stopped right after a step that aimed group
  follow packages, OBVR's watchdog: "no frame has ended for 10 seconds").
  The summaries showed leaders running a follow package - their own
  group's (group 03's leader on 11000819, group 03's own: following
  itself once aimed) or another group's (leaders following each other
  round). A follower promoted to leader (its leader in a fight) still ran
  the follow package when the plugin aimed that package at it. Now
  (`HordeLogic.h` SettleFollows): a group's follow package is aimed at its
  leader by an order of its own (Aim), only once the leader runs no follow
  package, before any follower is given it; until then the group's follows
  wait. `HordeLogicTest` "A churning horde" (600 steps, 60 walkers dropping
  out and back, packages taking three times in four): nobody follows
  itself or round a ring - red before the fix. After it: 11:29, 11:33,
  11:36 ran to the end (three of three; before, two of two froze). That
  the self-follow was the freeze's cause is derived from the timing and
  the summaries, not read in the engine.

### 4.11b Hordes at the gates - `[Horde] TowardCities=1`, `[Guards] Enabled=1`, `HealthFactor=5`, `ZombiesPerGuard=5`, `[Safeguard] Enabled=1`

What (Nadi, 2026-10-26): the hordes drift at random but lean toward the
cities; outside a city's gate they stop and mill about, and guards come out
to fight them - "die zombies müssen aber immer verlieren sonst brechen vll
quests usw." The guards are copies of the city guards, not essential and
without boosted stats of their own: each has `HealthFactor` (5) times a
walker's health (as `[Zombies] HealthPercent` makes it), and there is one
for every `ZombiesPerGuard` (5) walkers at the gate; whenever one dies a new
one comes out of the gate behind the line. As a safeguard the plugin keeps
everyone the walkers killed (who does not rise as a walker) and raises them
again once no walker is near (on by default).

How:
- The gates: the Tamriel-side load doors into the city worldspaces, read
  from Oblivion.esm (persistent DOOR refs whose teleport leads into
  AnvilWorld, BravilWorld, ...; 21 doors, the Imperial City's district
  gates among them). A group in Tamriel picks the nearest gate within
  `CityRange` when it forms; each new marker leans `CityPull` (0.5) of the
  way toward it; within reach the marker goes to a point `GateHoldUnits`
  (800) in front of the door on the side the group comes from, and stays.
  The walkers never pass the door (the marker is never inside).
- The guards (built 2026-10-26, `src/GuardLogic.h`, `src/Horde.cpp`): one
  ESP copy of a guard per city (NPC_ 0x837..0x83E: its factions, gear,
  face and level-scaled stats as the city's own; its AI packages and
  script dropped, essential cleared - so it stands and fights where it is
  put) and a pool of 21 persistent, initially disabled references of each
  in the holding cell (ACHR 0x83F..0x8FB, in banks of six per city: slots
  0-5 at 0x83F + city x 6 as in the six-pool of the morning, 6-11 at
  0x86F + city x 6 as in the pool of twelve, 12-17 at 0x89F + city x 6,
  18-20 at 0x8CF + city x 6 - `GuardLogic.h` PoolLocalId / PoolSlotOf; a
  save made with a smaller pool keeps its guards; twelve since Nadi's
  "größerer Pool, 12 pro Stadt", 21 since "lass mal 21 wachen machen",
  2026-10-26). 21 guards at one city take 101 walkers near its gates. A gate with walkers within
  `Radius` (4000) of its door gets ceil(walkers / `ZombiesPerGuard`)
  guards, at most the city's 21: a waiting one enabled and moved
  (MoveTo the door with an offset) onto the spot the city's way out puts
  the player (the city-side door's XTEL, `tools/city-gates.py`), in rows
  of six side by side across it, each next row 70 units farther out -
  behind the walkers, who hold `GateHoldUnits` farther out. The frame after: `Resurrect 0` when it lay dead,
  `ResetHealth`, its health read back into the log. Each second a guard
  out, standing and not fighting is sent at the nearest walker
  (`StartCombat`). Health: the guard record's TESHealthForm (+0x84) set
  to `HealthFactor` x the walker record's health before each sending
  (5 x 12 = 60 with `HealthPercent` 15). One who dies is replaced by the
  next waiting one; with none waiting, the dead are raised and sent out
  again - the pool is all there is, so the supply never ends and no
  reference is ever made at runtime. **Deviation from "spawnt eine neue",
  named:** the "new" guard is one of the 21 again - a dead one raised
  vanishes from where it fell. PlaceAtMe would make a truly new one, but
  PlaceAtMe's actors stay in the save for good (the CS wiki's PlaceAtMe
  page), and a supply that never ends would grow the save without bound.
  `ClearSeconds` (30) without a walker near the gate: the guards there are
  disabled (they vanish where they stand). After a load the pool is read
  off the world (an enabled guard is out at its city's nearest gate).
  A dead gate guard never rises as a walker and is not the safeguard's.
- The safeguard (built 2026-10-26, `src/SafeguardLogic.h`, `Rise.cpp`):
  the death watch reads the killer; an actor a walker killed - not a
  walker, not the player, not a gate guard, not one the rise takes (its
  verdict Waits: the generic people, whom no quest counts on) - goes on a
  list of up to 128 kept in the co-save ('SAFE'); once dead
  `AfterSeconds` (15), its body loaded (3D) and no living walker within
  `Radius` (3000) of it in the same interior or worldspace, the engine's
  Resurrect raises it there with the get-up animation (its own AI then
  takes it home). A death without any killer handed over (11:17: a named
  woman died among the walkers with killer 00000000) is asked about at the
  next frame: a living walker within 1000 units then counts as the killer. "Irgendwo sicher" is taken as "where it fell, once it is
  safe there". A body whose cell is unloaded waits for the player to come
  back: a raise without 3D is not read yet. Decided by Nadi, 2026-10-26:
  creatures are raised too (as built), and the safeguard stays on by
  default - the INI says that off is more realistic but may break quests.
  The fallen pool guards get no loot block (Nadi, 2026-10-26): what the
  player takes from one stays taken when it is raised and sent out again
  (derived from Resurrect keeping the inventory as it is; not run).

Cost: towards cities and the hold **small**; the guards **medium**; the
safeguard **small**. Tests: `GuardLogicTest` (every flow of the pool: how
many, out, replaced, raised, taken in, after a load, the spots, the gate
table), `SafeguardLogicTest` (who is noted, when raised, the co-save).
Harness: `gate-guards.txt`, `gate-guards-replace.txt`. Read in the game
(2026-10-26, Tamriel -47,-7 by Anvil): two guards out for seven walkers,
six for thirty (two a second); their health read back 60 of 60 (2 of 2
with `HealthFactor=0.2` by hand) - the record's +0x84 holds for these
auto-calc guards; they kill walkers (killer 11000840 in the death lines),
go back in 30 s after the last walker. The safeguard raised a mod's horses
the walkers killed (`Radius=300` by hand), noted a named woman.
Fallen guards (`gate-guards-dead.txt`, 11:51, `[Guards] TestKillSeconds=4`
by hand - a test switch, off by default, since a console kill of a pool
guard does not take through the harness's console lines): 42 guards
killed in 100 s; each death read "Death: gate guard ... fell" and nothing
else - no rise, no waiting list, no swap, no MOO strip, no loot block, no
safeguard line for a pool guard (the death path returns before them); the
three waiting ones sent in their place, then the fallen raised ("raised;
health 60 of 60") and sent out again, 36 times. The safeguard with its
default Radius 3000 raised a White Horse (0004E9A8) 66 s after the walkers
killed it, once the last walker near it was gone. The named woman (Maeva
the Buxom, 00185FE7, Whitmond) the walkers killed in 4 of 7 runs stayed
down to the end of each: a walker stood 51 to 95 units off her body the
whole 90 s (11:54, the status lines) - the rule waits for that, and so it
stays (Nadi chose to wait, 7a). The raise of an unloaded body (Nadi,
2026-10-27: "go A", "a 1 go"; `rise-after-away.txt`): a waiting body - the
rise's and the safeguard's - is present only in the cells the game runs
around the player (an interior: the player's own cell; outside: the same
worldspace within uGridsToLoad / 2 cells, the setting read at 0x00B06A2C),
since a left interior keeps its 3D in the cell buffer (run 1: the swap came
in the left hall, a walker in T-pose beside the corpse). A reference the
game freed with its cell is kept as missing, its clock standing, for three
game days (`MissingLogic.h`; run 2 had dropped it, and the same reference
lay there dead again at the return); co-save WAIT and SAFE version 2 carry
the day it went missing. Run 3 PASS: missing, present again, risen 20 s
after the return. **Open:** the three days are from memory (about the
game's cell reset), not read; the player leaving within a swap's two
seconds; the feel and the cost in the headset.

### 4.12 City siege - `[Siege] Enabled=1`, `WalkersAtGate=24`, `ChancePerDayPercent=10`, `WalkersMin=40`, `WalkersMax=120`, `PauseDays=3` (designed 2026-10-26, not built)

What (Nadi, 2026-10-26): a siege is a horde of walkers before one of a
city's gates, outside the walls only ("belagerung ist nur vor der stadt am
tor. erstmal"). It happens whether the player is there or not ("1. ja").
While it lasts the gate is shut ("tor zu bis vorbei"); the walkers do not
come into the city - those that follow the player in may (no city rule,
Nadi: "keine stadt regel"; with the gate shut none do). The first plan's
walkers in the streets are dropped ("walker in städten will ich nicht").

Built towards it: 4.11b - hordes near the player lean toward the cities and
hold at a gate, the gate guards come out, fight and go back in; 4.9 - the
Courier reports a gate attacked. What is missing, as designed:

1. **The siege as a city's state** (pure logic, `SiegeLogic.h`, co-save):
   when it begins, how many walkers stand before which gate, how many the
   guards cut down as the game hours pass, when it ends (none left). It
   runs on game time, so it goes on while the player is anywhere else.
2. **The siege seen**: when the player comes near the besieged gate, the
   plugin brings out walkers before it - a pool of walker references in the
   ESP like the gate guards' (PlaceAtMe's actors stay in the save for
   good), enabled and moved to the gate, the guards (4.11b) coming out to
   them. A walker killed there counts off the siege.
3. **The gate shut**: the besieged gate's two doors (the Tamriel side and
   the city side) locked with the engine's Lock at level 100, "needs a key"
   (the CS wiki's Lock page: 100 = Needs a key; the player cannot pick it),
   unlocked at the end; what the plugin locked kept in the co-save, a door
   that was locked before left as it was.
4. **The Courier**: siege editions, the same pipeline as 4.9 (Nadi's note
   under 4.9).

Decided (Nadi, 2026-10-27: "1. passt 2. einstellbar von 12-100 default 24,
für mein windows 100. 3. passt"; the build not started - "4.12 noch nich
starten"):

- **How a siege begins**: both - a chance a game day per city while the
  player is elsewhere, and a horde the player saw hold at a gate (4.11b);
  a pause per city after one. Set (Nadi, 2026-10-27: "1. passt"), all in
  the INI: `[Siege] ChancePerDayPercent=10` a city and game day,
  `WalkersMin=40`, `WalkersMax=120` in a siege (of them `WalkersAtGate` at
  once), `PauseDays=3` a city after one. Start values, not measured: how
  often that comes in an evening's play shows only in play.
- **Walkers before the gate at once**: `[Siege] WalkersAtGate=24`, clamped
  to 12..100, from a larger count, the next coming as one falls; Nadi's
  Windows live INI gets 100 when the key exists. Derived from the
  measurement of 2026-10-26 (`perf-walkers-in-view.txt`, some 0.18 ms of CPU
  a walker in view and scene pass, two passes a frame in VR, Linux CPU): 24
  in view some 8.6 ms a frame, 100 some 36 ms - the rate at 100 falls far
  under half (said to Nadi; the walkers out of view cost less, not
  measured). Measured since by the performance session
  (`perf-walkers-crowd.txt`, Linux, the Beyond at 75 Hz, placed walkers
  standing, 2026-10-27, one run): 25 walkers 13.7 ms frames, 50 24.2 ms,
  100 51 ms - at 100 about a fifth of the headset's rate. The pool in the ESP holds at least 100 (240 disabled cost
  nothing seen, `perf-siege-pool.txt`).
- **Which gates**: only the besieged one is shut; the player leaves by
  another, or by fast travel.
- Unverified: whether the game's people who travel between cities (merchants,
  quest people) wait at a locked gate or go round; a mod removed during a
  siege leaves the gate locked (the plugin's Enabled=0 should unlock it).

Measured first: what a pool of disabled walker references costs
(`perf-siege-pool.txt`, `tools/esp-write.py --test-walker-pool 30`,
2026-10-26 20:47/20:51, one run each): **nothing seen** - 240 disabled
walkers in the holding cell, frames 13.36 against 13.35-13.39 ms, the scene
pass 0.87-0.90 against 0.90-0.93 ms, and none of them on the process lists.
On a cell change and a load (`perf-siege-pool-load.txt`, 2026-10-27 09:30
deployed ESP and 09:33 with `--test-walker-pool 30`, one run each, Linux,
the Beyond at 75 Hz): the longest frame of each coc 142.6 / 150.5 ms
against 151.1 / 161.2 ms with the 240 references, the save's load 1420.7
against 1439.9 ms - some 10 to 20 ms more, within what one run each can
tell apart (derived: not a cost to design around). The cost of a siege is
the walkers in view, not the pool.

The game's travellers at a shut gate (`siege-gate-travel.txt` and its
control `-open`, 2026-10-27 09:24 and 09:27, one run each): Anvil's patrol
guard AnvilGuardCastlePatrolDay01 (0000BFD8), his travel package taking him
from the city through the Anvil Castle Gate to AnvilCastleGateMarker in
Tamriel, the player at the marker (the city not loaded, the guard in low
process): he came out into Tamriel between 90 and 120 s with the gate open
**and** with both doors (00006B96, 00006FBA) locked at 100. Not yet
settled: whether the lock took - the console's answer says nothing of that,
and the rerun's read-back failed (10:25 and 10:28: `00006FBA.activate
player` moved the player through neither the open nor the locked door; he
stood on the bridge in both, shot 10, so it tells nothing). The plugin will
lock the doors itself (point 3), and reading a door's lock is part of that -
the cheapest step that settles it. MOO has a locks module of its own (`Maskar's Oblivion
Overhaul for locks.ini`), not read. If the lock held, a low-process
traveller passes a shut gate - the siege then shuts the gate for the player,
not for the game's people.

Cost: **medium to large** - the state and its tests small to medium (the
co-save and the clock are 4.9's), the pool and bringing it out medium (the
guards' pool is the pattern), the lock small, the editions small; the risk
is the cost of the walkers in view and how the game's travellers take a
locked gate.

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
5. **No headless walkers while it is on** (Nadi, 2026-10-27: "die kopflosen
   müssen dann nicht verwendet werden wenn wir später death only by
   decapitation gebaut haben und dieser toggle an ist"): a walker without a
   head could never be ended. With `DieOnlyBeheaded=1` the spawns' variant
   list and the rise leave out the headless walkers (ACBS No Head, 0x8000:
   25 of the 64 - vanilla 0x90E, 0x913, 0x914, 0x915, 0x91A, and 20 of
   MOO's looks - read off the ESP of 2026-10-27); the plugin reads the flag
   off the records at the start, so no list is kept by hand. The variant list's headless entries get count 0
   or are unlinked (to be measured which the engine takes), the rise draws
   from those with a head. Walkers already headless
   in a save (made with the toggle off) stay as they are: proposed, they end
   as before 4.17 (killed once). Cost small, built with 4.17.
6. **Without a head cutter a power attack ends a walker** (decided by Nadi
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
7. **The others.**
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

### 4.18 LINK - the settings in the game's menu

What (Nadi, 2026-10-27: "Link support ... dann muss ich keine inis anpassen
sondern kanns ingame tun"; the split "ok passt für mich so"): Maskar's LINK
(Nexus 50144, 1.4) shows 27 of the plugin's settings in its in-game menu
(the 26 agreed and NoLevelingXP, 4.3b); the fine values stay in
`Data\OBSE\Plugins\OblivionWalkers.ini`. LINK++ (Nexus 53352) reads mods
made for LINK the same way (its description); not tried.

How (built 2026-10-27, `LinkLogic.h`, `QuestVars.cpp`, LINK's readme 3.2/3.3
read): the ESP holds the quest Walkers (0x94B, start game enabled) with the
script WalkersLinkScript (0x94C) that declares the settings' variables
(`ini_rise_enabled` ... `ini_courier`, all floats) and `ini_version`; the
variable list is read off `LinkLogic.h` by `tools/esp-write.py`, one table
for both. At every start the plugin writes `Data\OblivionWalkers.cfg` (the
menu: four headers, toggles and sliders) and `Data\OblivionWalkers.ini`
(set lines at the plugin INI's values - LINK's defaults button). At the
first look after a load (`StartOf`): without LINK the plugin INI's values
go into the variables, every load (the INI decides); with LINK and the
save's `ini_version` not 1 the same once, then the save's values decide.
Once a second with LINK the plugin reads the variables and applies a change
at once: health and pace written on the walker records anew, the spawn
counts into the variant list, the horde, the gate guards and the news
configured anew; the rest is read where it is used. **Deviation, named:**
`ini_replace_enabled` and `ini_extra_percent` take effect at the next start
(the leveled lists are changed once a session; undoing an injection is not
built) - the menu's description says so.

Test: `LinkLogicTest` (the table, the values both ways and their ranges,
the effects, the load's decision, both files), `QuestVarLogicTest` (a
variable by name). Harness `link-settings.txt` (LINK in the Linux install,
three variables set by the console): 2026-10-27 16:22 PASS (LINK 1.4 from
Nadi's Windows install, for the run only) - "LINK: loaded, the save had
none of this version - the plugin INI's values written into the quest's 27
of 27 variables", then within a second of the console's sets "ini_health_
percent now 40" with the 64 walkers' health written anew (32 of 80),
"ini_max_count now 4" with the counts "1 1 2 2 2 3 3 3 4 4", "ini_courier
now 0". Seen too: the first write comes before the save's quest data is
read and is done again after it (two such lines) - harmless. Not run: the
menu itself (Nadi's, Windows).

### 4.19 Grabbed and overwhelmed - `[Grab]` (feasibility study 2026-10-28, not built)

What (Nadi, 2026-10-28: "wenn von zuvielen zombies umzingelt, greifen oder
halten die einen fest und iwann ist man tot... also so wie Saints and
Sinners VR"): a walker that reaches the player grabs an arm and holds it;
held, the player is slowed, then rooted; not freed in time, the walker
bites; held by too many, the player dies.

**The reference, read 2026-10-28:**

- Skydance's tech blog (unrealengine.com, 2020-04-24, P. Akemann): walkers
  grab the player "by either arm (including two Walkers at once)"; player
  and walkers are then "locked into a common animation state", the player's
  hand is taken out of physics and its motion drives a 2D blendspace "to
  simulate the struggle to escape"; it ends with the walker dislodged and
  staggering back, or the player bitten. The player can at the same time
  grab a walker's head with the other hand, which holds it still for a stab.
  The walkers themselves are animated (mocap and hand-made), with an active
  ragdoll on the upper body only while no grab or stab connects them.
- Game Developer, "Designing around panic" (2020-04-08): a walker AI decides
  "who grabs you on which arm"; an earlier build let a third grab the neck,
  dropped as "too much for the player"; shaking a walker off was made simple
  and clearly explained.
- Players (r/PSVR "How do I shake off zombies", r/SaintsAndSinners "What
  actually kills you", Steam discussion 1742265917492234829): free yourself
  by a wide shake or pull of the held arm ("range of movement is more
  important than speed"), by pushing the walker off or braining it with the
  other hand; a walker hurts you over time while you fight it off.

**What already exists to build on** (read in this repo and OBVR):

| Piece | Where | Status |
| --- | --- | --- |
| A walker's landed hit on the player passes AttackHandling 0x005FEBF0 with the walker as attacker (EDI at the armor site) | 4.3, `SkillUseStub` | **measured** ("HELD BACK" for walker hits, 2026-10-27) - the natural trigger of a grab |
| Messages between Walkers and OBVR | 4.3 SKEX, xOBSE MessagingInterface | built one way (Walkers to OBVR); the way back is not |
| `SetRestrained 1`: the actor does not move but keeps thinking; on the player it blocks turning left/right, not up/down | CS wiki SetRestrained | **read**, not tried on a walker in combat |
| `PlayIdle idle forceIdle` (xOBSE): an IDLE record's KF on any actor, queued through ActorAnimData | CS wiki PlayIdle, deepwiki llde/xOBSE `Cmd_PlayIdle_Execute` | **read**; a KF can only come through an IDLE record |
| Custom KF: an animation-only export, the sequence named `SpecialIdle_<name>`, bone priority set high | CS wiki "Blender/Creating a Character Animation", "Exporting Character Animations from Blender" | **read** (old tutorials; whether today's Blender NifTools addon exports Oblivion KF cleanly: **not verified**) |
| Stagger, shove, hand position and speed, hand-to-body contact, the input gate of the player's movement | OBVR (CombatRing, Shove, MeleeHits, 0x0040DC61) | built in OBVR |
| The bite's disease | 4.14 | designed |

**A design, for Nadi to decide:**

1. **Grab** (`GrabLogic.h`, pure): a walker's landed hit on the player
   becomes, by `[Grab] ChancePercent`, a grab of the arm on its side
   instead of the blow's damage. Two arms, so at most two holders (S&S's
   choice); a third and further walkers around a held player count as
   "pressing in".
2. **Held**: one holder slows the player (`SlowPercent`), two root him
   (VR: OBVR's input gate; flat: `SetRestrained` on the player, or the
   plugin's own movement hook if the turn block feels bad). The holder is
   restrained before the player, faces him, and plays the hold.
3. **Bite**: a holder not shaken off within `BiteSeconds` bites - damage
   and the infection chance of 4.14 - and holds on.
4. **Overwhelmed**: two holders and `OverwhelmCount` (3?) walkers in
   reach for `OverwhelmSeconds` - the player dies (the engine's own death;
   god mode and essential respected).
5. **Free**: VR - OBVR pulls the held hand toward the holder (the spring
   it already has for weapon weight) and frees it at a wide shake or pull of
   that hand, at a shove or a head blow from the other hand; the walker
   staggers back (OBVR's shove). Flat - attack or block presses fill a
   struggle bar, each costing fatigue; a power attack frees one holder.
   Both: killing the holder frees.

**The animations - the real question:**

| Option | What it gives | Cost and why |
| --- | --- | --- |
| A. None new: the holder restrained, its attack held back, its own idle | the mechanics, no look of a grab | **cheap** - two console-level calls; looks like a walker standing at the player |
| B. Two or three KF made by a script: hold (arms forward, clutching, leaning in), bite (head lunge), maybe pull | a believable grab from a few keyframes per arm and head bone; IDLE records from `tools/esp-write.py`, `PlayIdle` from the plugin | **medium** - pyffi is installed on Linux and writes NIF/KF; new are the KF writer and the poses; risk: whether the engine plays a script-made KF without fixing in NifSkope (NonAccum, priorities, text keys), the look of hand-set keys |
| C. Hand-made in Blender | the best look, like S&S's | **large** - an animator's work; the addon's Oblivion KF export not verified |
| D. Walker's arms bent toward the player's real arm each frame (IK on the walker's skeleton) | in VR the hands sit on the arm wherever it is | **large** - OBVR moves the player's bones, never another actor's; new per-frame bone writing on NPCs, its cost with many walkers unmeasured |

The player needs no animation in VR (OBVR draws his own arms); flat in
first person a camera shake and the HUD bar would do, a struggle idle for
third person is optional.

**Cost, whole:**

| Part | Cost | Why |
| --- | --- | --- |
| `GrabLogic.h` (arms, holders, timers, bite, overwhelm, struggle) | small | pure logic, flows unit-testable |
| Grab at the walker's landed hit | small | the AttackHandling site is hooked already (4.3) |
| Holding walker and player (restrain, face, keep its attack back) | medium | engine unknown: whether a restrained walker in combat keeps swinging and turning; 4.16's attack tempo may help |
| Animations | B medium / C large / D large | see above |
| OBVR: pulled hand, shake to free, messages back to Walkers | medium | builds on OBVR's spring, shove and contacts; feel only in the headset |
| Flat struggle, bite, death | small | fatigue, damage, Kill, 4.14's disease |
| Harness: walkers around a held player, freed and overwhelmed | medium | OBVR's harness, headless; the feel needs Nadi |

Together **large**, mostly for the animations and the VR feel.
Recommended: A first, as a spike, then B - A answers the engine's unknowns
for little and is a playable grab already; B gives the look without an
animator; C or D only if B looks wrong in the headset. The cheapest step
that settles the open unknown: a harness run in which a walker reaches the
held player with `SetRestrained 1` on both (does it keep attacking or
turning?), and a scan of the vanilla idle KFs for a pose with the arms
forward that B could start from.

Open, Nadi's to decide: instant death when overwhelmed, or bites until
dead; the third walker at the neck (S&S dropped it); whether a grab should
also come from the walkers' own attack start rather than a landed hit.

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
| `moo-stay-down.txt` | with MOO set to raise every dead zombie, a dead walker stays down (7a, BlockMooResurrect) - PASS 2026-10-25 20:10 (control, BlockMooResurrect=0: up again within 30 s) and 20:12 (=1: dead to the end) | the plugin, the ESP and MOO active |
| `moo-stay-down-moo-look.txt` | as `moo-stay-down`, with a walker in MOO's default look (0x91E, MOO\zombie\skeleton.nif): MOO raises it too, the plugin keeps it down | 2026-10-27 07:59 PASS with BlockMooResurrect=0 (up again at t60, at the player); 08:02 PASS with 1 ("MOO's token and brain taken", dead to the end) |
| `chapel-walker.txt` | a walker placed in a chapel is taken out of the world (4.7) - PASS 2026-10-24 21:21, 21:26 (made-there rule) |
| `chapel-follow.txt` | a walker met outside and moved into a chapel stays (4.7) - PASS 2026-10-24 21:27 | the plugin, the ESP active | the plugin, the ESP active |
| `spawn-extra.txt` | ExtraPercent=50: walkers beside a list's own people (4.13) - PASS 2026-10-24 21:15 | the plugin, the ESP active, ExtraPercent by hand |
| `spawn-replace-person.txt` | five people set to 1 by hand become walkers in a list of people (4.6) - PASS 2026-10-24 20:49 | the plugin, the ESP active, the INI lines set by hand |
| `spawn-replace-outdoors.txt` | the engine's own spawn points at Tamriel -29,5 yield walkers, the deer and the people stay (4.6) - PASS 2026-10-24 11:17 | the plugin, the ESP active |
| `gate-guards.txt` | seven walkers near Anvil's main gate: two guards out with 60 health, sent at the walkers, back in after the last; the walkers' victims raised by the safeguard (4.11b) - 11:12 and 11:17 froze (the horde's self-follow, fixed), 11:29 PASS (`[Safeguard] Radius=300` by hand: horses raised), 11:33 PASS (`HealthFactor=0.2`) | the plugin, the ESP with the guard pool |
| `gate-guards-21.txt` | 105 walkers: the pool of 21 out, two a second, all four banks, 60 of 60 each; 93 walkers killed by them, 6 fell - 12:15 PASS (71.5 % reprojected, headless, some 190 walkers loaded) | the plugin, the ESP with the pool of 21 |
| `gate-guards-twelve.txt` | sixty walkers: the pool of twelve out, two a second, in two rows, 60 of 60 each, back in together - 12:08 PASS (53 % reprojected, headless, some 110 walkers loaded) | the plugin, the ESP with the pool of twelve |
| `gate-guards-dead.txt` | fifteen walkers, every guard killed 4 s after it comes out (`TestKillSeconds=4` by hand): the fallen left to the pool alone (no rise, no safeguard), the waiting sent, then the fallen raised and sent out again - 11:51 PASS, 42 killed, 36 raised | the plugin, the ESP with the guard pool |
| `gate-guards-replace.txt` | thirty walkers, guards of 2 health (`HealthFactor=0.2` by hand): the pool's six out, the fallen replaced - 11:36 PASS for six out and back in; no replacement was needed (one guard fell to another's swing) | the plugin, the ESP with the guard pool |
| `rise-moo-races.txt` | five generic people of five races killed (Argonian, Orc, High Elf, Dark Elf, Nord): each rises as his race's MOO look, the Nord as a vanilla or MOO default one (OblivionWalkers.log); one of each MOO family placed for the pictures - 2026-10-27 07:32 PASS headless: the Argonian rose as 0x927, the Orc 0x935, the High Elf 0x941, the Dark Elf 0x943 (one of 16, 16, 12, 12 candidates), the Nord as vanilla 0x91C (of 47); every swap laid the MOO walker into the corpse's pose (13 and 16 ragdoll bodies) and got up; "64 walker creatures found", "515 entries over 64 walkers (25 of them headless, half as often)"; shots 06 and 10 show MOO's looks, no missing mesh | the plugin, the ESP with the 64 walkers, MOO loaded |
| `walker-variety.txt` | the twenty walkers placed in the prison, ten and ten, their AI off: the parts in the pictures, "20 walker creatures found" and "200 entries over 20 walkers" in OblivionWalkers.log - 2026-10-27 07:13 PASS headless, both log lines there, twenty mixed walkers in shots 08 and 12; 2026-10-27 08:05 PASS again with the final build (64 walkers, "515 entries over 64 walkers (25 of them headless, half as often)"), the twenty in shot 11 | the plugin, the ESP with the twenty walkers |
| `news-courier.txt` | the Count's Arms broadsheet in Anvil carries the outbreak; seven walkers at Anvil's main gate bring its guards out; the Courier's office broadsheet opens on "Walkers at the Gates of Anvil!" (4.9) - 19:59 and 20:03 PASS on the log, the book not opened (the console's activate opens none); 20:07 PASS, the name in the HUD; 20:10 PASS, opened with the hand's A, the text in the picture; 2026-10-27 06:48 PASS with HeadingHoldDays 0.5 (outbreak, heading for Anvil, then the attack; the hold itself is not reached here, the attack outranks it) | the plugin, the ESP with the editions |
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

- **FIXED 2026-10-27 - Ten walkers at once are one look** (Nadi, Windows, 2026-10-27: "wenn 10
  walker auf einmal spawnen dann ist es immer der selbe typ aus unserem
  pool, aber es sollten 10 unterschiedliche sein"). Seen in his log too:
  MemorialCave's dead FF001548..FF001550 all base 2500093B. Cause, read in
  the ESP and the plugin: WalkersZombieVariant holds each walker with a
  count (1..10, `SpreadCount`), so one draw picks one walker and the engine
  makes count copies of it. Measured (`spawn-mixed.txt`, 2026-10-27 14:41
  control, 14:44 with `esp-write.py --test-nested-variant`, Linux): four
  placements of the list gave one base each (3, 8, 3, 2) - and with the
  count moved onto an entry pointing at a second list of every walker once,
  still one base each (2, 9, 9, 3): the engine draws a nested list once and
  copies it count times - with the list's flags 0. Read with REA after it
  (Nadi: "go A", the engine side): creatures from a leveled list go through
  TESLeveledList's CalcItemForContainer (0x0046CDE0; the summon effect
  0x006A5C60 casts to TESLevCreature and calls it), which draws a list with
  LVLF bit 2 once for each of the count. The probe with that flag on the
  nested list (15:37): 4, 4, 6, 3 walkers, every one a different look. Built
  so (the any-walker list, 4.1 "The spawns"); no engine hook needed.

- **Crash on loading a save inside a dungeon** (Nadi, Windows, 2026-10-27
  13:54, CrashLogger.2026-10-27-12-54-52.log): access violation in
  NiGeometry::Render+0xA (0x00722A1A, caller 0x00719BAE), ecx a
  Lighting30ShaderProperty where a geometry is expected; on the stack two
  walkers FF0013A9 and FF0013AA (base 25000802, cell "Temp Pathgrid"), the
  horde's follow package 25000818, a SaveGameFile. Read in that session's
  OblivionWalkers.log.prev: the plugin never named either walker (no death,
  swap, horde follow, MOO strip or loot block); 818 went to FF0012EA/EC/EE
  only - derived: the two carried it from an earlier save. The performance
  session reads the engine side. Not read: whether the plugin's frame tick
  runs during a load and hands console lines (the MOO strip's second
  RemoveItem, the horde's MoveTo) to references being torn down.

- **Walkers can still be made in a city** (2026-10-26): the rise has no
  city rule - a person it takes who dies in a city gets up there as a
  walker (derived from the code; no run). **Decided: no city rule** (Nadi,
  2026-10-26: "keine stadt regel") - not a bug, as meant; a walker that
  follows the player in may stay.

- **Dense dungeons cost frames** (Nadi's Windows headset run, 2026-10-26
  12:48-12:55): in Fort Urasek's Company Billets (FortUrasek02, 000CA58F)
  83 walkers stood on the process lists, 42 in high process; after the
  load into it 18 walkers were in view and the frames ran 22-23 ms for
  13 s (SteamVR half rate - 931 of the session's 974 half-rate frames,
  from the performance session's reading of OBVR.log), up to the crash
  (left aside, Nadi). The cell has some twelve creature spawn points, each
  1 to MaxCount (10) walkers. Linux harness, headless
  (`perf-fort-urasek.txt`): MaxCount=10 - 47 walkers, 3.7 % reprojected;
  MaxCount=3 - 19 walkers, 1.4 % - one run each; those runs measure the
  walkers' AI, not their drawing (11 and 0 walkers came near the player,
  no capture ran). Whether the Windows frames are CPU or GPU is not
  measured; the CPU side wants walkers placed around the player with
  OBVR's performance capture on, the Pimax's GPU side a Windows run.
  The CPU side, measured 14:15 (`perf-walkers-in-view.txt`, Linux, 75 Hz,
  dual pass): eighteen walkers standing in view cost 3.3 ms of CPU a scene
  pass (0.89 -> 4.17 ms mean), some 0.18 ms a walker and pass; frames over
  20 ms went from 0.2 % to 21.7 %. Derived: on the same CPU at 90 Hz
  (11.1 ms) that alone explains Nadi's half rate with 18 in view - the
  number of walkers in view is the cost, so the number made per spawn
  (MaxCount) is the mod's lever. Actor shadows on top (15:02 and 15:06,
  iActorShadowCountInt 0 against 10, the walkers standing): 3.73 against
  5.15 ms a scene pass, frames over 20 ms 20.8 % against 75.4 % - ten
  shadow-casting walkers cost 2.8 ms a stereo frame, and Nadi's Windows INI
  has iActorShadowCountInt=10, iActorShadowCountExt=10 (Linux 5 and 3).
  Built in OBVR on Nadi's call (f53cf6d, `[Render] ShareActorShadows`,
  default off until the headset): the second eye keeps the first eye's
  shadow maps - with ten casters scene_pass 3.85 against 3.35 ms, frames
  over 20 ms 6.7 % against 1.1 % (`perf-walkers-shadows-shared.txt`, the
  eyes' dumps alike in `-eyes.txt`). Options for Nadi: (A) keep
  MaxCount 10 (his default since 2026-10-24); (B) a lower default, say 4;
  (C) a cap per cell. **Decided A** (Nadi, 2026-10-26: "maxcount bleibt
  10").

- **Walk-only reaches only 16 walkers of each kind** (found 2026-10-26
  reading the code for the pool of 21): `Rise.cpp` StepWalkers collects at
  most 16 references per walker base each frame (`refs[16]`), so with more
  than 64 walkers loaded (four bases) the rest keep running when their AI
  asks for it - `[Zombies] Walk=1` does not hold for them; AvoidChapels
  missed them the same way. **FIXED 2026-10-26** (Nadi: "ja"): one walk of
  the process lists a frame for every walker kind (`Rise.cpp`
  CollectAllWalkers, `RiseLogic.h` KeepWalkers - sorted, each once, up to
  256), used by StepWalkers and the horde; a summary line every 2 s.
  `gate-guards-21.txt` 12:23: "111 walkers on the process lists, 111 of
  them in high process kept walking", up to 2056 runs turned to walks in
  2 s (the AI asks again every frame). The cost of the walk itself is not
  measured apart (the run reprojected 69 %, headless, 111 walkers and 21
  guards).

- **A victim stays down while walkers camp at the body** (2026-10-26,
  `gate-guards.txt` 11:54): the safeguard noted Maeva the Buxom, killed by
  a walker at Whitmond, and a walker stood 51 to 95 units off her body for
  the 90 s to the end of the run - the rule (no walker within Radius) keeps
  her down as long as they stay. Options for Nadi: (A) keep waiting, as
  now; (B) after a limit (say 120 s) raise her at the nearest city gate's
  spot, where the guards are - "irgendwo sicher" - her AI walks her home;
  (C) after the limit raise her where she lies, among them. **Decided A**
  (Nadi, 2026-10-26: "A"): the safeguard waits until no walker is within
  Radius, however long - not a bug, the rule as meant.
  Seen again in Nadi's Windows run of 2026-10-26 22:28: a MOO Black Guar
  (FF000EEB, base MOO 09976A) killed three walkers and was killed; a walker
  stood 61 to 368 units off it for 446 s, then the Guar left the list
  without a line - alive again or gone from the game, which is unknown.
  Since 2026-10-27 the step hands every one leaving the list unraised to
  the log ("Safeguard: X off the list after N s dead - alive again by other
  means / gone from the game"), `SafeguardLogicTest`.

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
  **FIXED 2026-10-25** (Nadi: "go" for taking them off, MooLogic.h,
  `[Zombies] BlockMooResurrect=1`): read in MOO's ESP - its OnDeath handler
  (SetEventHandler "OnDeath" MOOOnDeathByCreatureFunctionScript) gives a
  dead "\Zombie\Skeleton.nif" the token MOOTokenZombie (CLOT, local
  042C18), whose script raises it 10.8 to 54 s later (GetTimeDead past
  0.003 * TimeScale * Rand(1, 5) hours) if it carries MOOZombieBrain (INGR,
  local 0F8B6D) or at ini_zombie_resurrect percent. The plugin finds MOO by
  name, checks the two forms' types (Clothing 0x16, Ingredient 0x19) and
  removes both from a dead walker at the next tick and 2 s on (the
  console's RemoveItem). `moo-stay-down.txt` 2026-10-25, MOO set to raise
  every zombie without a brain: with BlockMooResurrect=0 the walker stood
  again between 15 and 30 s after the kill and walked to the player
  (20:10); with 1 it lay dead to the end, 70 s (20:12). MOO's INI in both
  installs is back to MOO's own ini_zombie_resurrect 70 (Nadi, 2026-10-25:
  "2. ja mach"), so MOO's own zombies get up again as MOO means them to.

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
  Status 2026-10-25 evening (looked at again on Nadi's "go"): that run was
  already made - the narrowing above (2026-10-24 08:46, seven outdoor runs,
  `rise-creature-outdoors-nomods.txt` among them): with and without
  Ragdolls, MOO and both Enhanced Grabbing plugins only the bandit the
  console placed goes disabled; the area's people and Nadi's own kills
  rose. Left open: who disables a console-placed corpse outdoors - the
  engine or one of the OBSE DLL plugins, which the ESP-off runs kept
  (EngineBugFixes, AveSithisEngineFixes, BA_EngineFixes, GPEngineFixes,
  Modern Engine Fixes, FriendlyCreatures and others; none of their INIs
  names corpses or dead bodies). Whether it reaches actors other mods place
  with PlaceAtMe (MOO's spawns are such) is not known. The decisive step: a
  probe on the engine's reference disable that logs, for a waiting corpse,
  the return address - inside Oblivion.exe or inside which DLL - small to
  medium (the plugin hooks no engine code yet). Not read yet: the disable
  routine's address - xOBSE's GameObjects.cpp names 0x004FBB30 (deepwiki),
  but the bytes there read as a list insert (REA), so that is unverified.

## 8. Phases

1. **Engine proofs** (this week): `spawn-count`, `rise-baseline` (both PASS
   2026-10-23); the plugin skeleton with INI, log, co-save and the rise on a
   clone (`rise-walker`, `rise-walker-clone`, PASS 2026-10-23); the ESP with
   the Walker race, the four Walker creatures and their list
   (`tools/esp-write.py`); the creature rise (`rise-creature`, PASS 2026-10-23
   20:44) - done.
2. **Core**: 4.6 replacement, 4.4 walk, 4.5 health, 4.2 name, 4.15 loot, 4.1
   rise. Harness green for each.
3. **World**: 4.7 chapels, 4.8 fame, 4.13 extra spawns, 5 scenarios with
   OblivionCrisis, 4.9 Courier (built 2026-10-26).
4. **Feel**: 4.11 horde, 4.12 siege (outside the walls, Nadi 2026-10-26),
   4.10 headshots with OBVR, 4.16 tempo, 4.3 no skill gain.
5. Later list.
6. **Last** (Nadi, 2026-10-26): 4.14 infection.

4.17 waits for OBVR's decapitation (Nadi, 2026-10-26: "4.17 erst wenn obvr
decapitation kann") - not even the power-attack rule before it.

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
