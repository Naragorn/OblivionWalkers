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
| xOBSE runs a plugin's co-save callbacks only when the plugin set an opcode base of its own (`SetOpcodeBase`; obse.log otherwise: "HandleSaveGame: plugin with default opcode base registered for serialization"); the plugin uses 0x2B00, free in github.com/llde/xOBSE/wiki/Registered-Opcodes-for-OBSE-Plugins on 2026-10-24 and 2026-11-03, requested as 0x2B00-0x2B0F in llde/xOBSE issue 263 (2026-11-03), registered by llde 2026-11-07 (the wiki list at our next release) | `rise-cosave.txt` runs 1 (no callback) and 2 (written, read, the walker up after the load) | **measured** |
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

**ESL** (Nadi, 2026-10-28: "für den nexusmods release bieten wir beides: esl
als erstes mit requirements zu oblivionesl und als esp"; MOO and the other
mods' lookups not touched - "moo usw beachten wir aber nicht"): the same
records as an ESL-flagged ESP for OblivionESL (Nexus 56578; it needs xOBSE
22.13, and Cell Lookup Robustness because the ESP has a cell, the horde's
holding cell with its markers and guard pool). `tools/esp-write.py --esl`
sets the header flag 0x200 (the TES4Edit custom build's; OblivionESL also
takes 0x80000, ESLHooks.cpp) - one byte differs from the plain ESP. No
compacting: every local ID already lies in 0x800..0xFFF (0x800..0x952,
~340 records of the 2048 allowed); the script refuses an ID past 0xFFF.
The DLL keeps the ESP's place as the base of its IDs (`RiseLogic.h`
`EspFormId`, `IsEspForm`, `FindEspBase`): OblivionESL's export
`ESL_GetFormIDBase` first (0xFE | light index << 12), else the mod index <<
24. The co-save's references resolve through xOBSE's ResolveRefID, which
OblivionESL hooks for light plugins. Not the same for a save: a save made
with the plain ESP loses its walkers and guard references under the ESL
variant and the other way round (OblivionESL's own warning). Nadi,
2026-10-28: "A" - the release says a switch keeps the game but loses the
walkers already risen and what they carry (the spawned ones come again
with the cells' resets). Run 2026-10-28 09:06 (`harness/esl-check.txt`,
PASS, Linux with the Windows mod list): the ESP found as light plugin 0
(FE000000), 64 walkers, the grab idles, the spawn lists, horde, guards,
news, LINK's 66 variables written, the test grab. Not seen: LINK's menu, a
save carried over.

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

### 4.6a Replacement by category - `[ReplaceCategories] Skeletons=1` ... `KnightsOfOrder=0`, `[ReplaceExceptions]`

What (Nadi, 2026-10-29: "nicht jede eintrag einzeln mit 1 zu replacen
sondern in LINK ingame die kategorie enablen so wie F4 Zombie walkers. Also
1 für Skeletons und alle skeletons egal wie die im CS heissen werden
ersetzt"; option A, people too, "eig will ich sofort"): one switch per kind
of creature or person, in the INI and in LINK's menu, that takes every
record of that kind - the game's and every mod's, whatever it is called -
and takes at once when switched in LINK.

How (built 2026-10-29, `CategoryLogic.h`, `ReplaceLogic.h` `ReplaceOn`,
`Replace.cpp`):
- A creature's category is the last folder of its model (TESActorBase's
  TESModel at +0xAC, its path at +0xB0 - xOBSE GameForms.h), matched against
  a table of 34 creature categories; the dog's folder splits by type (undead:
  the skinned hounds, else wolves and dogs). A person's is the first of 11
  categories one of its factions (TESActorBaseData's list inline at +0x3C)
  is an Oblivion.esm faction of: Vampires before the cults, Necromancers
  before Conjurers.
- Read in Oblivion.esm (`tools/esm-spawns.py`, 2026-10-29): the leveled
  lists spawn from 39 model folders, each with its category (one test runs
  them all); every skeleton - Skeleton, Champion, Guardian, Hero, Bregor the
  Cremator - is `Creatures\Skeleton\`; 381 people, a vampire's class one of
  some twenty, but every vampire in VampireFaction 033F53. In MOO's ESP the
  same holds: `MOO\Skeleton\` (51), `MOO\Zombie\` (102), `MOO\Goblin\`, and
  its people in the game's factions (703 vampires, 229 bandits, 320 cult
  mages in both cults). MOO itself tells its zombies by the path `\Zombie\`.
  Over Oblivion.esm, Knights and the DLCs (`tools/ini-creatures.py`): 692 of
  the 726 records take a category; 34 people stay without one (Bruma
  Militia, the Turnkeys, Fort Ontus' mages, the Vitharn ghosts, Mehrunes
  Razor's Drothmeri and laborers).
- What an entry points at (`ReplaceOn`): `[Replace] Enabled=0` keeps all;
  an exception line decides over all else; a category decides for its own,
  but never turns a creature with a script or one that only swims (as the
  lines kept them); `[ReplaceCreatures]` decides only for one without a
  category, and only those get a new line there - a categorized one's line
  counts for nothing. The shipped section is cut to the 34.
- The defaults keep what the lines did: creature categories on but the
  tame animals (deer, sheep, wild horses) and the slaughterfish, people
  off. One record changes: the Goblin Rat Farmer (the goblin's model,
  confidence 25 or less, a "harmless animal" before) becomes a walker with
  Goblins=1.
- At once: every pass remembers what each entry (its ListData) named
  before it pointed at the walkers, and points every entry as the settings
  say now - at the walkers or back at its own. A change of `Enabled` or a
  category in LINK runs a pass (`LinkEffect::Spawns`); the main menu and
  five seconds after a load run one as before. An extra entry (4.13) named
  nothing of its own and stays; `ExtraPercent` still takes at the next
  start. Spawns already standing stay until their cell resets - the lists
  decide only what spawns next.
- LINK: 45 toggles from index 68 under two headers, "ini_cat_<key>",
  version 7 (a save of 6 takes the plugin INI's values once more).

Test: `CategoryLogicTest` (the folder, the match, every vanilla folder,
the factions and their order, the table and its defaults, every flow of
`ReplaceOn`), `LinkLogicTest` (the toggles, their order, effect and the
version), `ReplaceLogicTest` (a comment that reads like a line is none).
Harness `spawn-category.txt` (Goblins on, then off through the quest
variable: walkers, then goblins) - written, not run yet.

Open: not run in the game. A mod's own folder (MOO's harpies, draugr,
mummies, ghouls) or own faction is no category: its line decides, as
before.

Cost: **medium** (as rated): the table and decision new and unit-tested,
the engine reads two offsets of xOBSE's layout, the pass reused.

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
- **Raised at home - built 2026-11-05** (`[Safeguard] RaiseAtHome`; Nadi,
  2026-11-04: "kann der sie wiederbeleben aber irgendwo anders wo sie halt
  spawnen zuhause zb", then "A wenns easy geht ansonsten B", 2026-11-05:
  "go"). B first (5c16055): raised where it lay, then `EvaluatePackage` so
  it walks back to its schedule. A on top: before the raise, an actor whose
  home is an interior with an editor ID is moved there by the console's
  `PositionCell` to its start position, then raised; an outdoor home, an
  interior without an editor ID or no start known: B.
  - The start position: TESObjectREFR's virtual at vtable+0xF4 (an actor's
    reads its +0xE8..+0xF0, 0x00628E40). The angle is vtable+0xF0 (read in
    0x0045BB30) - not +0xF8, xOBSE's order, which left the raise unfinished
    at 10:32; the raised one keeps the heading it lay with (the angle not
    used yet).
  - The start cell: Actor+0xF8, a TESObjectCELL (type 0x30) or a
    TESWorldSpace (0x35) - read from the engine's own move to the editor
    location 0x0045BB30 (0x005E0260 / 0x005E1F60 / 0x005E1F40). Measured
    wrong first: ExtraPersistentCell (0x11) held Tamriel's persistent cell
    00003615 for Dro'shanji (it follows where a persistent actor is, derived),
    ExtraStartingWorldOrCell (0x53) was absent; and the cell type had been
    taken as 0x39, it is 0x30 (xOBSE GameForms.h).
  - Harness `safeguard-home.txt` PASS 2026-11-05 11:05: Dro'shanji (0002CA3B)
    brought into Bravil's street, killed, 15 s later "its start world or cell
    (Actor+0xF8) 0002CA39 (type 30)", "moved home (PositionCell handed
    over)", "after the move in 0002CA39 at -34 54 -291", raised there with
    his packages asked anew. Seconds later his schedule had him in cell
    0002FF55 (where he stood before the test too) - the AI's doing, not the
    safeguard's.
  - Open: the engine's own move 0x0045BB30 (or 0x004DD4B0, its cell move)
    instead of the console would take worldspace homes too and the angle -
    its this-pointer (the extra list at +0x44, derived from its
    `param_1 == -0x44` test) unverified; **small** to try, a harness run.
    The raise of a body moved into an unloaded cell is the safeguard's
    waiting as before (not run for a home far away). Unit flows:
    `SafeguardLogicTest` (HomeMoveFor, every choice).

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

### 4.11c Hordes wait out of sight - `[Horde] GateHordesMost=1`, `GateNearUnits=8000`, `LoadedGrid=5` (Nadi 2026-11-11, built 2026-11-11)

Why: OBNG's actor census at Anvil's docks (2026-11-11, OBNG repo
`artifacts/shadow/obng-profile-anvil-docks/20261111-101335/OBNG.log`) found
two Walkers hordes (groups 00 and 01, 14-15 walkers, OblivionWalkers log
of that run) holding before Anvil Main Gate 10 000-11 700 units from the
player, out of sight, each in high process with the full Actor::Update
every frame - about half of the actors' 5 ms a frame there. A hold at a
gate was unlimited (4.11b): every group within CityRange came and stayed,
up to 16 groups of 9 - the crowd Nadi once met at a gate that was no siege.
Nadi (2026-11-11): hordes wait out of sight and come when their turn is,
and at most a few a gate.

What (`HordeLogic.h` `MayHold`, `HoldAtGate`, `WaitAway`,
`OutsideLoadedGrid`; `HordeLogicTest` TestGateWait):
- A group near its gate holds there only if fewer than GateHordesMost
  others hold at that gate (0: any) and the player is outdoors in Tamriel
  within GateNearUnits of it (0: always; the player not outdoors in Tamriel
  - in a city's own worldspace, inside - counts as near, as no walker of
  Tamriel is in the player's grid then).
- Otherwise it waits: its marker straight away from the player from its
  leader, at the first 512-unit step outside the grid of LoadedGrid x
  LoadedGrid exterior cells around the player's cell (the engine's
  uGridsToLoad, 5 by default - read from the INI, not from the engine).
  The spot is set once: a player who walks up to it meets the horde.
- Each step a waiting group let to hold moves up to its gate (the same
  hold spot as before), and a holding group no longer let (the player gone
  beyond GateNearUnits) goes off to wait. A noise draws a waiting group as
  any other (4.20.2).
- Only a holding group begins a siege (4.12, NoteHordeAtGate) - so a horde
  begins one only with the player near, as the siege's walkers show only
  within 8000 units.
- Log: "Horde: group NN waits out of the player's grid near <gate>" and
  "... moves up to hold at <gate>", with the marker and the player's
  distance (200 lines a session); the group summary says "waiting out of
  sight". INI, LINK (170..172, header "Hordes - waiting out of sight",
  version 31 - a save of version 30 takes the plugin INI's values once).

Derived, not measured: that a horde outside the loaded grid leaves the
full update (OBNG's reading from the distances; the engine's rule not
read) - to measure with OBNG's census once a run shows hordes waiting.
Walkers that walk out of the grid may leave the process lists and their
group (as any walker out of reach - the group is formed anew when they come
back in). Harness: `harness/horde-gate-wait.txt` via
`tools/horde-gate-wait.sh`. Run 1 (2026-11-11 11:12, Linux, headless,
`artifacts/hand-script/horde-gate-wait/20261111-111253/`): "Horde: on" with
1 and 8000; a group held at Anvil Main Gate, and a second group near it
waited ("group 02 waits out of the player's grid near Anvil Main Gate -
marker at -180139 -24623, the player 1817 units from the gate") - the cap
seen. Not seen: the holding group going off with the player away, and a
waiting group moving up - the hold began a siege at once (4.12), its
walkers joined the groups (a group led by siege walker 2600095A) and went
back to the pool when the player left, so neither group was left for those
steps. The runner now turns [Siege] Enabled off for the run (restored
after; the deployed INI had no [Siege] section - added for the run). Run 2
(11:33, `.../20261111-113336/`): no siege; a group held at the gate, but no
second group came near it (one batch went into a group of the save's), and
the player's moves by `cow` load the grid anew - the holding group ended
and the slots were taken by others, so the cap, the going off and the
moving up were not seen. The scenario now moves the player by `setpos`;
run 3 queued.
Found in both runs: four or five groups of the harness save (eight
followers each) stood some 6000 units north of Anvil Main Gate around
-191200, -21700 for minutes - "2000 to its marker" step after step, the
leaders moving a few hundred units at most, new headings every
StaleSeconds without progress. Out of sight, inside the loaded grid, far
from the gate: 4.11c does not touch them, and they are likely the bulk of
OBNG's census (75 of 98 high-process actors Walkers horde members >= 10 000
units off, 2026-11-11). Where: Oblivion.esm puts Whitmond Farm's corn
field there (CornStalk flora at z ~1430, fences, a fence gate 000975ED,
unlocked), Maeva the Buxom (00185FE7) works it 9-18 (UESP). Nadi
(2026-11-11): they stood about the farm and followed him when he came - so
not fenced in. OBNG's census of 11:56 (Windows list) had three walkers there
at 0 units/s beside Maeva. Hypothesis, not measured: the marker goes to the
leader's height (`MoveTo <leader> dx dy 0`), the field lies on a hill
(rocks 500 units south already 100-200 lower) - a marker 2000 units
downhill hangs in the air, and the travel package does not get there.
Built against it (proof switch `[Horde] MarkerOnGround=1`, INI only): the
marker's MoveTo carries the land's height under its spot as the z offset -
TES::GetTerrainHeight, thiscall 0x00440590 on the TES singleton at
0x00B333A0 (xOBSE's GetTerrainHeight, GameData.cpp/Commands_Cell.cpp; `ret 8`
read off the EXE), the leader's height where no loaded cell is
(`MarkerHeightOffset`, HordeLogicTest TestMarkerHeight). The group summary
logs the leader's z, the marker's z and the land's. A/B to settle it:
`tools/horde-stuck-farm.sh` runs `harness/horde-stuck-farm.txt` with the
switch 0 and 1 and prints each leader's way and the marker's height over
the land - run 2026-11-11 12:54 (A, off) and 12:56 (B, on),
`artifacts/hand-script/horde-stuck-farm/`: not settled by it - in both, the
farm groups fell apart within seconds of forming ("9 walkers here, 0 fit,
0 groups" step after step in B): Slow aggression (4.20.x) had each walker
"turn on" a dozen actors within 1000 units after 5-10 s - Maeva, the
player (tdetect notwithstanding) and creatures that died there to walkers
in these runs: CreatureSheep 000151DD and CreatureSheepRam 0005D558
(Oblivion.esm), MOO creatures (2300C07F/C086) - and a walker in combat is
unfit for its group. The marker's height: A's leaders on the field at z
1360-1372 with the land 1367-1391 under their first spots - no marker in the
air there; group 01 on the hill east at z 4983 with the land 3945 under its
spot (1038 below) walked 2018 units in A all the same. So the marker's
height is not shown to be the cause; the switch stays on (a marker on the
land is right either way). The pictures of headless runs are black. Gate
run 3 (12:59): the batches placed at the gate formed no group at all (the
same slow aggression near the gate), so 4.11c's going off and moving up are
still not seen. Next: the slow aggression's line names its target's base,
kind and distance, and a group member's unfitness its reason.

### 4.12 City siege - `[Siege] Enabled=1`, `WalkersAtGate=24`, `ChancePerDayPercent=10`, `WalkersMin=40`, `WalkersMax=120`, `PauseDays=3` (designed 2026-10-26, built 2026-11-04)

**Built 2026-11-04** (Nadi: "ja mach 1-4", the guards' toll 2 a game hour,
and a new wish: "was aktuell belagert wird wird auch teil von rumors also
bei allen npcs"). SiegeLogic.h (SiegeLogicTest) and Siege.cpp:

1. **The state**: per city active, gate row, walkers left, the toll's day,
   the pause; a chance a game day per city (rolled once per new whole day,
   however many passed), or a horde the player sees holding at a gate
   (Horde.cpp NoteHordeAtGate). Co-save `SIEG` version 1. `[Siege]` in the
   INI and LINK (seven entries after 4.17's, version 10); test switches
   `TestGate` (a kCityGates row, -1 off) and `TestEndSeconds`.
2. **The gate shut - dropped the same day** (Nadi, 2026-11-04: "als player
   würde ich schon gerne die stadt verlassen können um mitzuhelfen bei der
   belagerung", go A; the lock held only the player - travellers pass a
   locked gate, 2026-10-27): the gate stays open, nothing is locked. A gate
   a build of that day locked (the co-save's bit) is unlocked at the load
   and at its siege's end. What was built and seen first: both doors (CityGates.h now has the city-side door,
   tools/city-gates.py) `Lock 100`, read back with GetLocked/GetLockLevel
   into the quest variables siege_lock and siege_lock_level; a door locked
   before is left alone; unlocked at the end. A lock outlives a load the
   save has no change for (siege-lock.txt 16:02: still locked after the
   harness's reload) - so before a load or a new game the session's locks
   are given back, and after the co-save is read its sieges lock again.
3. **The siege seen**: 100 ACRE of vanilla looks with a head in the ESP
   (0x953.., persistent, initially disabled, the holding cell); while the
   player is outdoors in Tamriel within 8000 units of the besieged gate,
   WalkersAtGate of them are enabled and moved in rows from 900 units out
   (SiegeSpot), refilled as they end (a slot never used first, else a
   corpse's, raised with SetDestroyed 0 and Resurrect); the gate guards
   (4.11b) come out for them by themselves. A walker of the siege ended
   (4.17's end) counts off - noted in the death path, counted at the next
   step. Away, the guards' toll; the player leaving puts them back in the
   pool; at a load every pool walker is disabled.
4. **The Courier**: 16 new editions (besieged and lifted per city,
   0x9B7..0x9C6, NEWS version 3); a siege on is the news before all else.
5. **Rumors** (new): a GLOB per city (0x9C7..0x9CE, 1 while besieged) and
   an INFO per city in Oblivion.esm's Rumors topic (INFOGENERAL 000000D7,
   the DIAL overridden with the Walkers quest's QSTI added), condition
   GetGlobalValue == 1 (function 74, as 246 of Oblivion.esm's INFOs use it),
   not random, so it comes first while the city is besieged.

**Seen in the game** (Linux, headless): `siege-lock.txt` 16:05 and 16:20 -
the outer door locked (read back 1, level 100) and unlocked (0), given back
before the reload, the rumor global 1 then 0, the broadsheets on edition 18
then 26; `siege-walkers.txt` 16:15 - 24 out before Anvil Castle Gate, five
guards out for them, 30 ended one by one to "the siege is over", the gate
unlocked, 30 back in the pool. **Not seen:** an NPC saying the rumor (no
harness for dialogue), a day's roll beginning one, a horde beginning one,
the toll ending one, a save and load with a siege on. **Found:** the inner
door 00006B96 of Anvil Castle Gate reads locked at 100 in the harness save
before any siege (Oblivion.esm has no XLOC on it) - probably the lock test
of 2026-10-27 kept in OBVRHarness.ess; the siege leaves it as it is.
**Settled (2026-11-04, siege-door-read.txt):** the eight city-side doors
read locked only because a gate's two doors share one lock. Bravil's inner
door 00005603 read 0 unloaded and loaded with no siege (20:33); with
Bravil's siege the outer door's Lock 100 made it read locked 1, level 100,
unloaded and loaded, and the outer's Unlock made it read 0 (20:36). So the
siege locks and unlocks the outer door only, reads both back, and leaves a
gate alone when either read locked before; the whole run again PASS with
the new lines (20:39).

**Seen in the whole** (`tools/siege-harness.sh`, harness/siege-full.txt,
2026-11-04 20:27, all 13 checks): the test gate and the day's roll at 100
percent besieging all eight cities, the walkers out and counted off, a save
and load with the siege on (the lock given back, read from the co-save,
locked again, the walkers out again), leaving, four and two game days of
the guards' toll ending every siege, every lock undone, the rumors and the
Courier's besieged and lifted editions.


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

On the list of ideas again 2026-11-04 (4.20.3).

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

### 4.17 Only a lost head ends a walker - `[Zombies] DieOnlyBeheaded=1`, `RiseAgainSeconds=20`, `RiseAgainJitterSeconds=10` (built 2026-10-30)

What (Nadi, 2026-10-25: "walker nur sterben wenn geköpft. default an.
ansonsten resurrect[en] die immer"): a walker that falls gets up again, every
time, until its head is cut off. Every walker: the replaced spawns (4.6), the
extra spawns (4.13) and the risen (4.1). It is the walkers' own answer to
MOO's zombie resurrect (7a), which stays off.

**Built 2026-10-30** (Nadi: "jo"; EndLogic.h, EndLogicTest): the death watch
decides per walker (WalkerFallVerdict) - up again on the waiting list
(Requeue, the clock RiseAgainIn), or the end: the rule off, no head to lose
(ACBS No Head), the head noted, or no cutter at all (below).
The due walker is raised as itself (Raise -> RaiseAsSelf, no clone). OBVR's
'WGDM' {on} and 'WGHD' {refId, kind 1 cut / 2 crushed} come through the
grab's OBVR listener (registered at xOBSE's PostLoad); a head is noted at
once (HeadNotes, for a death in the same frame) and acted on at the next
frame tick (StepHeads: off the list for good - lootless, fame -, or killed
when still alive). **Without a cutter every kill ends a walker** (Nadi,
2026-10-30: "bei Dismemberment=0 killt jeder schlag und walker steht auch
nicht auf") - OBVR's Dismemberment=0, flat, no OBVR: no rise at all. It
replaces the power attack's kill of point 6 (2026-10-25, b): VR plays no
power attack (OBVR's strike calls the engine's AttackHandling itself, no
anim - OBVR, 2026-10-30), so xOBSE's IsPowerAttacking read 0 in
end-power-attack.txt (19:50, 19:53). NoLoot and Fame wait for the end. The headless are left out of the rise's candidates and unlinked from
WalkersAnyWalker at the start (ReplaceSettings::headlessWalkers, from the
INI). LINK: three entries after the categories (113..115), version 9.
**Seen in the game** (OBVR's dismember-walkers-job.txt, 19:40, and
end-power-attack.txt): a walker downs and gets up in place, ended by a cut
while it lies; a person cut at the killing blow or as a corpse stays down.
**Not seen yet:** the unlinked spawn list, the no-cutter end
(harness/end-no-cutter.txt). A walker lying
to rise again can be searched (NoLoot sets destroyed only at the end).

**Open - flat with dismemberment** (Nadi, 2026-10-30: "in Flat: dism=1
spieler muss zombies powerattacken um sie final zu killen und deren kopf
fliegt weg, dism=0 einfach umhauen"; flat players play without OBVR, "obvr
ist eine vr mod. das abtrennen will ich später aus obvr extrahieren"). The
four cases: VR dism=1 only a cut ends a walker (built); VR dism=0 every kill
ends it (built); flat dism=0 every kill ends it (built - no cutter speaks);
flat dism=1 a power attack's kill ends it and its head flies off (not
built). Planned: the cut extracted from OBVR into a plugin of its own, which
in flat cuts the head at a power attack's kill (the engine plays the power
attack there, xOBSE's IsPowerAttacking or OnAttack tells it) and speaks
'WGDM' and 'WGHD' as OBVR does - Walkers then needs only to listen to that
plugin's name beside "OBVR" (Grab.cpp registers the listener by sender
name). Nothing of it in Walkers before then - decided by Nadi 2026-10-30
("a"): wait for the extraction, no interim power attack rule in Walkers;
until then flat plays as dism=0.

How (as proposed 2026-10-25):

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
6. **Superseded 2026-10-30: without a cutter every kill ends a walker** (see
   Built above). The plan of 2026-10-25 (b) was: a power attack ends a walker. No OBVR, OBVR with `Dismemberment=0`, or an OBVR that
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

#### 4.17a Others' kills - `[Zombies] OthersEndPercent=100` - BUILT 2026-11-09

Found by Nadi (Windows, 2026-11-08): a walker that a guard, a bandit or a
companion kills gets up again and again - only OBVR cuts heads, and only the
player's blows reach it; the refugees' kills had their own rule (4.20.6,
'WGCT'). Options given 2026-11-09: A every kill by someone else ends it (OBVR
asked to cut the head), B the same by a chance, C only guards. Nadi: "B".
- A walker felled by a killer that is not the player, not a camp's refugee
  and not a walker (its base one of the 64 walker records) ends by
  OthersEndPercent (0-100, default 100; `OthersEnd`: the roll's last two
  digits under it), OBVR asked to cut its head ('WGCT', as a refugee's kill:
  `WalkerFall::EndedByOther`). A roll missed: it gets up again as ever.
  Without a cutter every kill ends it already (EndedNoCutter). No killer
  (a fall, a trap without owner): it gets up again.
- INI and LINK (setting 166, LINK version 28, 167 settings). EndLogicTest
  covers the verdict and the roll.
- Harness gate-guards.txt 2026-11-09, Linux headless, default INI, **PASS**:
  Anvil's gate guards out (seven, for 7 then 31 walkers), 90 walkers "ended
  (felled by someone else - OBVR asked to cut its head)" - killers the pool
  guards, Anvil's own guards and others (0004F001, 00185FE7, 0004E9A8) -
  none "gets up again". OBVR answered "head is cut off" for 21 of the 90:
  the rest not confirmed; the walkers end either way. OBVR measured it
  (its OBVR.log of the run, 2026-11-09): not its roster cap - OBVR keeps 16
  cut parts at once (Dismember.cpp kCutsMost=16); of the 24 'WGCT' it
  logged, 17 "head cut", 7 "NOT cut: sixteen parts cut already - no room"
  (its log stops at 24 lines; the rest likely the same). Fix on OBVR's side
  (more places, or the oldest corpse's recycled), waiting for Nadi's go -
  the guess of a roster cap was wrong.
  Nadi's go 2026-11-09: OBVR raised them to 128 (OBVR e9ecad0, OBVR.dll
  33557bf8). gate-guards again (OBVR's run 115135, Linux headless):
  **91 felled by others, 91 heads cut off**, none refused. The log's
  dispatch line said "felled by a refugee" for these too - fixed to "a
  refugee or someone else".

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
configured anew; the rest is read where it is used. Since 2026-10-29
`ini_replace_enabled` and the categories' toggles point the lists' entries
anew at once (4.6a). **Deviation, named:** `ini_extra_percent` takes effect
at the next start (its entries are added once a session) - the menu's
description says so.

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

### 4.19 Grabbed and overwhelmed - `[Grab] Enabled=1`, `ChancePercent=66`, `BitesToDie=3`, `BiteSeconds=4`, `BiteHealthPercent=20`, `StruggleHits=2`, `TearFreeDistance=1000`, `BiteResetSeconds=30`, test switch `TestGrabSeconds=0` (built 2026-10-28, A + B)

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
2. **Held**: a holder roots the player (one already since 2026-10-30)
   (VR: OBVR's input gate; flat: `SetRestrained` on the player, or the
   plugin's own movement hook if the turn block feels bad). The holder is
   restrained before the player, faces the player and plays the hold.
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

The player needs no animation in VR (OBVR draws the player's arms); flat in
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

Decided (Nadi, 2026-10-28: "go a + b. 1) einstellbar wieviele bisse player
aushält, default 3 2) nein 3) treffer"): the player takes `BitesToDie`
bites, the last kills; no walker at the neck; a grab only at a landed hit.

**Built 2026-10-28** (`GrabLogic.h`, `Grab.cpp`, `tools/grab-anims.py`, the
ESP's IDLE 0x94D..0x952):

- The grab: xOBSE's OnHit (the rise's handler) hands each hit on; at the
  next frame a walker's hit on the player grabs by the chance the arm on
  the walker's side (`SideOf`: the player's heading at +0x28), the other
  when that one is held, none when both are. The player's landed blows on a
  holder count toward `StruggleHits`.
- Held: every frame the holder's HighProcess movement flags (+0x1FC) lose
  every step and pace (it stands), the player's lose the run with one
  holder and every step with two (`HeldPlayerFlags`) - nothing is in the
  save, a load starts free.
- The bites: each holder on its own clock; the damage through xOBSE's
  `ModAV2 Health -N` (the damage modifier, its docs via deepwiki), a bite
  that leaves nothing through `Kill <holder>`; god mode (0x0065D820, as
  OBVR reads it) leaves a point. After the last bite every holder lets go.
- The animations (option B): `tools/grab-anims.py` reads the zombie's own
  KFs out of Oblivion - Meshes.bsa and writes WalkersGrabHold.kf
  (Walk01ArmsUp's upper body, a loop at priority 70), WalkersGrabBite.kf
  (the forward power attack's spine, neck and head with the hold's arms, a
  clamp of 1.67 s) and WalkersGrabLetGo.kf (Stagger whole) beneath
  Creatures\Zombie\IdleAnims and MOO\zombie\IdleAnims (MOO's zombie
  skeleton has the same 69 bones by name, read with pyffi). The ESP's IDLE
  records name them, follow the zombie's IdleSway (0002ACCD) in the idle
  tree and carry a condition never true (GetRandomPercent < 0), so no
  zombie picks them on its own. The plugin plays them as xOBSE's PlayIdle
  does (the anim data from the actor's virtual +0x164, then 0x00477DB0
  thiscall (idle, actor, 4, 3), "ret 10h" read with llvm-objdump), the hold
  asked again each second (a no-op while it plays).
- OBVR: the holders go to OBVR as message 'WGRB' (count, then reference
  and arm per holder) whenever one takes hold or lets go; a 'WGFR' from
  OBVR (one word, the holder) lets that one go.
- **VR shake** (Nadi, 2026-10-28: "abschütteln in vr. Ähnlich wie saints and
  sinners"). How Saints & Sinners does it: a walker grabs either arm (two at
  once), the held hand leaves physics and its movement drives the struggle,
  which ends with the walker dislodged and staggering back or the player
  bitten (Skydance on the Unreal Engine tech blog, "The Grapple"); players
  shake the held hand up and down, or push the walker off with the free hand
  (r/SaintsAndSinners kocc1j, Steam discussion 1742265917492234829). Split:
  - OBVR is the sensor: once it runs it sends 'WGVR' 0x52564757 (one word, 1
    in VR); it reads 'WGRB' and, for each held hand, sends 'WGSK'
    0x4B534757 (one word, the arm, 1 left 2 right) at each swing of that
    hand - a reversal of its movement after enough travel and speed, OBVR's
    thresholds. A shove or blow of the free hand on a holder may send
    'WGFR' (that holder), as S&S's push. Pulling the held hand toward the
    holder is OBVR's feel, not needed for the count.
  - Walkers decides (GrabLogic.h StepShake, ShakeStrokesNeeded): per arm,
    `ShakeStrokes` (6) strokes times the crowd and the tiredness of the
    presses' formula, none more than `StruggleGapSeconds` apart, shake that
    arm's holder off (LetGo Shaken, "You break free.", its stagger); each
    stroke costs `ShakeFatigue` (2). After 'WGVR' 1 the grab's line says
    "Shake it off (n)". The Block key and blows still work in VR. Time
    runs out by the bites, as flat.
  - LINK: "VR: swings to shake free" and "VR: fatigue per swing", last in
    the list (indices 66, 67) under the grab's advanced header - 68 in all,
    version 6.
- **One holder roots, no line in VR** (Nadi, 2026-10-30, at the headset: "griff
  kommt zwar aber ich kann einfach weg. und die notification zeile bringts
  auch nicht. sains & sinners machts besser mit der einblendung"; "go A. go
  B"). A: one holder already roots the player (PlayerHold has no Slowed any
  more); walking off frees no one - `TearFreeDistance` is 1000 by default,
  only a jump apart (a door, a teleport) lets go; free by blows, Block, the
  shake or the holder's death. LINK "Let-go distance" 100..3000, version 8.
  B: after 'WGVR' 1 no line is said at all (GrabText false); instead Walkers
  sends OBVR 'WGBT' 0x54424757 at each bite (four words: the arm, the bite,
  BitesToDie, 1 for the last), and OBVR shows grab and bite on the body: a
  jolt in the held hand at 'WGRB', a stronger one and a red flash at
  'WGBT', its stick movement locked while 'WGRB' counts a holder (the
  engine's movement flags do not reach OBVR's own locomotion - not
  verified, OBVR's to check). The room itself cannot be locked. The holder's
  hands on the player's arm (IK on the walker's skeleton) is the next,
  larger step, not built.
- **In reach of the arm** (Nadi, 2026-11-04, "B", OBVR building the grip IK:
  a walker grabs only when it already stands in reach of the arm, a jump only
  if small and forgivable). OBVR streams 'WGHP' 0x50484757 {arm 1/2, the
  forearm's world x, y, z} for both arms ten times a second in VR; at a
  landed hit the walker's nearer shoulder (Bip01 L/R UpperArm, world
  position) to that forearm: within `ReachUnits` (37, OBVR's measure of the
  zombie's arm) it holds as it stands, within `NudgeUnits` (10, OBVR's
  proposal, some 14 cm) more it holds and steps those units closer, beyond it
  is no grab. Flat, or no forearm known within a second: it holds where it
  hit, as before. The test grab still jumps: 30 units ahead of OBVR's forearm along the heading, facing it (in sight - the forearm lies behind the feet in VR).
  INI and LINK (version 12). The 45-unit move of ca2e81f and the 'WGHP'
  answer of e103534 replaced. Neither value tried in the headset.
- **The lunge, no damage** (Nadi, 2026-11-04: "go 1. also greifen die quasi
  normal an aber statt dmg und so halten die einen fest"; OBVR's harness: real
  hits land 69-87 units shoulder to forearm, so B grabbed almost never). A hit
  that grabs gives back the health it took (the player's health as the last
  frame left it, ModAV2 - the engine's damage itself is not stopped, the
  health bar may dip for a frame; a stagger or the hit's sound is not
  stopped either, not measured). In VR beyond `ReachUnits` the walker lunges
  the units it lacks, up to `LungeUnits` (100 since 17:30, first 60), over `LungeSeconds` (0.3) for 60 units and longer at that pace beyond,
  eased out, a MoveTo each frame toward the forearm, turned to it; beyond
  that no grab. LINK version 13 (NudgeUnits gone).
  - Built in Walkers 2026-10-28, unit flows PASS; OBVR's side handed to
    the OBVR session; the feel only in the headset.
- LINK: "Walkers grab", "Chance to grab", "Bites you take" under the header
  Grabbed (version 3, so a save of version 2 takes the INI's values again);
  since 2026-10-28 (Nadi: "können die auch in die link settings?") also
  "Presses to break free" (StrugglePresses 0..10), "Harder per walker"
  (StruggleCrowdStep 0..1), "Fatigue per press" (StruggleFatigue 0..20) and
  "Grab messages" (Messages) - version 4.
- Since 2026-10-28 (Nadi: "GPL-3.0 und ja" to all of them, AGENTS.md's rule)
  every INI setting LINK can show is in the menu: 31 more under six
  "- advanced" headers after the others (the rise's swap timings and
  corpse match, the names and MOO, the horde's radii and headings, the gate
  radius and clearing time, the safeguard and the news, the grab's timings,
  damage, strikes, distances and the crowd's cap and radius) - 66 in all,
  version 5. **INI-only, named:** `Name` (text), `[ReplaceCreatures]` and
  `[ReplaceExceptions]` (lists; their categories are in the menu, 4.6a), `StruggleControl` (a control), the test switches `TestKillSeconds`
  and `TestGrabSeconds`, and the proof and fallback switches `AsCreature`,
  `CloneBase`, `ZombieRace`, `LookAfterSeconds` (the race path behind
  AsCreature=0) and `HoldDeathSound`. `RenameToWalker` and `RenameVanilla`
  take effect at the next start (the records are named once a session).

**Deviations, named:**
- The holder's own blows go on while it holds: the grab does not take the
  hit's damage away (the design's "instead of the blow's damage"); the hold
  at priority 70 covers its swing. Taking the damage away needs a hook in
  the hit's damage path - not looked for.
- A power attack does not free a holder at once: OnHit does not say whether
  the blow was a power attack; every landed blow counts toward StruggleHits.
- A bite at low health can kill before the last bite: a bite is damage, the
  count is when death comes at the latest.
- The bites are forgotten after BiteResetSeconds free - my choice, so a
  fight hours ago does not count; Nadi's to change.
- No infection from a bite: 4.14 is not built.

**Seen in the game** (`harness/grab-hold.txt`, three runs 2026-10-28 06:58-07:04,
Linux, god mode): the grab at the first landed hit, left and right arm by
side, "2 hold, the player rooted"; the bites every 4 s, the third the last,
then both let go and the count starts anew; the slowed player on the stick
backward at a walk ("moved 51 / 81 / 117 units in the last 1.0 s (flags now
0102)") - the per-frame write holds against the player's own input - and
torn free at 160 units; the idle queue taking each of the three idles
("played: 1"). **Not seen:** the rooted player pushing the stick (derived:
the same write), a bite's damage (god mode keeps the health), the lethal
bite without god mode, the player's blows on a holder, MOO's skeleton, and
the animations' look - the holders stood beside the camera, out of the
pictures; "played" says the idle was queued, not how it looks.

**The animations in the picture** (`harness/grab-anims.txt`, 2026-10-28
07:36, with the test switch `[Grab] TestGrabSeconds` - the nearest walker
moved 80 units before the player, turned to face it, grabbing as at a hit;
a run waiting for a landed hit had the walker wander off): the engine plays
all three script-made KFs. The hold: upper body bent forward, both arms
raised and reaching, swaying; the bite: the head lunges at the camera; the
let-go: the stagger carries it back, and the hold's loop ends - it walks
off as itself. **Not right yet:** the hands do not close on the player's
arm - Walk01ArmsUp's arms reach wide past the body, from about a metre.
Nadi, 2026-10-28: "ok erstmal 1., 2. können wir machen wenn 1.
shice ist". **Option 1 built** (`tools/grab-anims.py` GRIP_TARGETS): each
upper arm turned, one turn for the whole loop, so the shoulder-to-hand line
points 50 units ahead at chest height, the hands 24 apart - read off the
skeleton and the keys offline: the left hand from (-24, 43, 101) to (-12,
45, 90), the right from (25, 26, 88) to (12, 32, 94); the sway kept (keyed
interpolators at the B-spline's own times). The test walker stands 62
units before the player. Run 2026-10-28 07:47 (grab-anims, PASS): the arms
come in before the body, the hands low and together at the player's
middle, no longer reaching wide past it. Whether it reads as holding an
arm is the headset's; option D stays the fallback.

**Option D, noted for later** (Nadi, 2026-10-28: "notiere option d"): the
walker's hands bent onto the player's real arm every frame - IK on the
walker's own skeleton, nothing VR in it. The target differs: in VR the
player's arm bones as OBVR sets them from the controller (read off the
player's skeleton, no message needed); flat the arm bone of the player's
own animation - little to see in first person, a hold on the arm in third.
Cost **large** either way: writing another actor's bones each frame is new
(OBVR writes the player's only), its cost with many walkers unmeasured.
Built only if option 1's fixed pose does not hold up in the headset.

**Flat (A + B, Nadi 2026-10-28: "go A + B"):**
- A, the lines on the screen (`Messages=1`): through the console's
  Message, "A walker grabs your left arm! Strike it or mash Block.", "Two
  walkers hold you - you cannot move!", "Bitten! (2 of 3)", "The walkers
  overwhelm you.", "You break free." (`GrabText`).
- B, wrenching free by the key (`StrugglePresses=4`, `StruggleCrowdStep=0.5`,
  `StruggleCrowdMost=3`, `StruggleCrowdRadius=300`, `StruggleGapSeconds=0.8`,
  `StruggleFatigue=5`, `StruggleControl=6` Block): presses of the control,
  none more than 0.8 s apart, and the longest holder lets go ("wrenched
  off"); each press costs 5 fatigue (`ModAV2 Fatigue`). How many (Nadi,
  2026-10-28: "wieviel ausdauer man noch hat und wie viele walker einen
  umzingeln umso schwerer wenn man es nicht in zeit schafft -> death",
  then "go" on the formula): 4 x crowd (1 + 0.5 per walker within 300
  units beyond the first, at most x3) x tiredness (1 fresh to 2 with no
  fatigue left) - one walker fresh 4, three 8, five 12, three with a
  quarter left 14 (`StrugglePressesNeeded`), looked at once a second; the
  number stands in the grab's line ("mash Block (8)"). The time is the
  bites' (no clock of its own, as proposed and agreed): the last kills. The
  presses' own fatigue makes a long struggle harder. Run 2026-10-28 08:18
  (grab-struggle, PASS): one walker near, fatigue full - "1 of 4" .. the
  fourth "wrenched off". Not seen: more walkers near, a tired player, the
  presses' fatigue (god mode kept it at 145). The control's state
  is read as xOBSE's IsControlPressed reads it (OSInputGlobals at
  OSGlobals 0x00B33398 +0x20: the control map's key +0x1B7E and mouse
  button +0x1B9B, the keys +0x18F4, the buttons +0x1B2C), not behind a
  menu. Works for VR players too, with whatever OBVR sends as Block.
Test: `GrabLogicTest` (the struggle's flows, the texts); harness
`grab-struggle.txt` 2026-10-28 08:09 PASS - six presses of the harness's E
(StruggleControl 13, Jump) counted 1..5 of 6, the sixth "wrenched off",
"You break free." on the screen. Not run: the game without OBVR, the
mouse's Block, the grab's own line in a picture.

Test: `GrabLogicTest` (every flow of side, grab, hold, bites, damage, let
go, forgetting, the message), `LinkLogicTest` (the three settings).

### Later (named by Nadi, not in the first build)

- Barricades (28 Days' door locks by Security skill, walkers breaking them).
- Extra guards around the cities.
- Guard parties that attack Oblivion gates and always lose when the player
  is not there.

### 4.20 Ideas of 2026-11-04 - not built, not decided

Proposed by Claude, Nadi: "geil notiere alle ideeen in specs". Each with
what it does in play and its cost; none is decided or started. The
recommendation of that day: first Nadi's headset test and a 0.2.0 release,
then 4.20.1, with 4.20.5 as a cheap side dish.

#### 4.20.1 Walker blood as a disguise - `[Disguise] Enabled=1`, `Seconds=180` - BUILT 2026-11-09

**Walkers down after the raise, again (2026-11-11, Nadi's Windows log):**
13 raised walkers at Whitmond Farm "not up 20 s after the raise", knocked
state 3 (11) or 6 (2), health full, no package, no fight; all of MOO's and
the vanilla looks alike (bases 3700091B..948); never on Linux. Read off
the EXE (the knocked update 0x006545E0, its table at 0x00654C58): 4 thrown,
3 down - the get-up idle looked for (IdleManager 0x00521450; none: "No
'GetUp' animation found for actor '%s'." and it stays 3), 5 the get-up
playing - back to 3 when the animation stops before its end, 6 the blend
out - to 0 when done. The zombies' get-up idles exist for both skeletons
(Oblivion.esm 00092159/0009215A Creatures\Zombie, MOO.esp
MOOIdleZombieGetUpFaceUp/Down MOO\zombie; IsFacingUp ==/!= 1, fn 107 != 0),
so "none found" is not the likely branch; a get-up stopped before its end
and tried again is (Nadi's "twitching"). Built: the walk rule (a run turned
to a walk, every frame) leaves a walker that gets up alone
(`WalkRuleApplies`; it turned some walkers' runs 4000+ times); and the
raise watch keeps the get-up's trail (`NoteKnocked`: tries into 5, falls
back 5 to 3, the walk rule's held-back rewrites while down), told in the
"not up" line. RiseLogicTest TestKnockedTrail. Not shown yet that this is
the cause - the next Windows log's "not up" lines will tell (tries and
falls back counted).

**LINK values lost, 2026-11-11 (Nadi's Windows log, found by OBVR):** two
loads read `ini_version` 32 and every setting 0 - every slider to its
least (rise off, hordes off, speed 0.2); a load of the same session's day
before had read good values; never seen in older logs. OBVR writes no quest
variables (its laser only clicks LINK's menu). Cause open; candidates: a
save holding zeros, or b021941's first sync at the first tick after a load
reading before the engine restored the quest's variables. Guard
(`LinkLooksEmpty`, `StartOf(..., empty)`, LinkLogicTest TestEmpty): this
version but every setting with a least above 0 reading 0 - the INI's values
go in ("the save had this version but no values (guard)"). Diagnosis: the
first look logs the frames since the load, the version, how many variables
were found and two raw values. Nadi (2026-11-11): the sliders could not be
moved on Windows (only to their max), so Nadi pressed LINK's Defaults - LINK
runs Data\OblivionWalkers.ini as a batch (LINK.esp
LINKOnClickGenericModsConfigMenuDefaultsLoadIniFunctionScript). Harness
`link-save-load.txt` (18:5x, Linux, LINK++): that batch, a save, two loads -
each load "first look 1 frames after the load - version 32, 174 of 174
variables there, values set", "the save's values decide", no change after:
the first tick reads the save's values, and they survive. The Windows zeros
are not reproduced; open (the Windows LINK, the save Nadi loaded). Again
2026-11-12 09:56 with 5ba1a32: PASS - the checker
(tools/link-save-load-check.py) judges only the script's two reloads; the
harness's own save loads before them with no Walkers values (version 0,
EMPTY - right there, the guard writes the INI's).

**Bug fixed 2026-11-11 (Nadi, Windows):** in a hand-to-hand fight a beaten
NPC (00023502) fell by the player's hands and the disguise came on - log
"Disguise: OBVR's gesture smeared from 00023502", "on (OBVR's gesture)".
Cause: Walkers took OBVR's 'WGBL' as it came; OBVR's side says "OBVR only
sends dead bodies; which of them count is Walkers' call" (OBVR
vr/WalkersDisguise.h), and Walkers never made that call. Now
(`SmearAccepted`, DisguiseLogicTest TestSmear) the corpse must be a walker
and dead, as for the dialog; otherwise "no disguise (walker 0, dead 0)".
That OBVR took a knocked-down, living NPC for a dead body is OBVR's to look
at (told).

The player smears himself with a fallen walker's blood (activates its
corpse, a choice in a small menu or a held activate): for a few game
minutes the walkers take him for one of theirs and leave him be - until he
strikes one, or it rains, or the time runs out. Sneaking through a horde
becomes a thing. Cost **medium**: the corpse's activation caught (an
activate hook or a script on the walker records), the walkers made
peaceful to the player for the time (their aggression or a faction
relation, measured first - the "peaceful walkers" bug of 7a shows a faction
change does it), a screen tint and a line while it lasts; unit flows for
the clock and what ends it, a harness walking the player through walkers.
Open: what counts as "strikes one" in VR (OBVR's hits), whether the guards
mind.

**Built 2026-11-09** (Nadi: "go 4.20.1"; then: the time in real minutes -
"echte", 3 game minutes being 6 real seconds at the game's TimeScale 30; a
confirm dialog - "ok mit confirm dialog, also iwas wie do you want to rub
..."; the walkers fighting stop - "ok"; any rain ends it - "jo").
`DisguiseLogic.h`, `Disguise.cpp`, `DisguiseLogicTest`; INI `[Disguise]
Enabled=1`, `Seconds=180`, `Test=0` (a test switch); LINK 167..168 (version
29, 169 settings).
- Asked: the player, sneaking, activates a dead walker (xOBSE's OnActivate:
  arg0 the activated, arg1 the activator - measured, the run below). The
  dialog is the game's own message box (xOBSE GameAPI.cpp for 1.2.416:
  ShowMessageBox 0x00579C10 with its callback 0x005003E0, the button pressed
  at 0x00B09E24, 0xFF none - as xOBSE's MessageBoxEx sets it up), "Rub it on"
  / "Leave it", the answer read each frame. Normal activation loots as ever
  (the loot window still opens: OnActivate only tells).
- On: Seconds real seconds (the frame's time); the slow aggression (4.21)
  skips the player as a target; with the slow aggression off the walker
  records' aggression is held at 0 while it lasts; no grab; the walkers in
  combat near are told StopCombat once (all of them - also those fighting
  someone else; they turn on that one again by the slow aggression); the
  ability "Walker Blood" (ESP SPEL 0xA45, a copy of SEAbOrderKnight2 with
  Resist Disease 1 %) on the player; OBVR told ('WGDS' 1/0).
- Ends: the time; the player striking a walker (OnHit); swimming (the
  process's movement flag 0x800); rain outdoors (the sky's weather,
  precipitation 3 at TESWeather +0x53); switched off. A line for each.
- VR: OBVR's gesture sends 'WGBL' (the corpse) - the disguise on without the
  dialog; OBVR's side (the gesture, the dark rim, blood on the hands) is
  OBVR's to build.
- The movement flags are read at the process's +0x1FC (16 bits), as OBVR
  reads its sneak eye. Calling BaseProcess::GetMovementFlags through its
  vtable slot 0xAF returned garbage and broke the frame (run 14:37: the
  disguise ended in the frame it began) - not used.
- Harness `disguise` (tools/disguise-harness.sh: Test=1, Seconds=40),
  2026-11-09, Linux headless, **PASS** (14:5x): left Ctrl held, the player
  activates a living walker - "OnActivate - FF000F86 and 00000014 (the player
  the second; walker 1, dead 0, movement flags 00000400)": the order and the
  sneak flag; the walker turns on the player (slow aggression, 10.1 s); the
  dialog shown 25 s after the load, answered by the console's
  SetButtonPressed 0 - "button 0", "on - 40 real seconds", "1 walkers
  fighting told StopCombat"; no "turns on" for 40 s with the walker moved to
  600 units; "off - The walker blood has dried"; moved back to 600, "turns
  on 00000014, near it 7.1 s".
- The blood shown (Nadi, 2026-11-09: "A + C machen wir doch eine schwarz
  rote vignette 3 min und bild dunkler auch"): A, flat and in VR - Oblivion.esm's
  effectDamage (000C7941, a dark red membrane: fill b9 00 00, edge 71 00 00,
  no particles) played on the player by PlayMagicShaderVisuals for Seconds,
  stopped at the end, played anew when smeared again; C, in VR - the
  vignette black and red, the picture darker and blood on the hands, OBVR's
  on 'WGDS' (Nadi's go there, 2026-11-09). Flat, no vignette and no darker
  picture: Oblivion has no screen effect for it - a full-screen HUD tile of
  our own would be medium, offered - WON'T DO (Nadi, 2026-11-09). Harness disguise again, PASS:
  "PlayMagicShaderVisuals 000C7941 40", then "StopMagicShaderVisuals" at
  the end (the console answers 0 either way, as for AddScriptPackage) - how
  it looks is not seen (headless).
- OBVR's side, 2026-11-09 (OBVR 3ec71a4, OBVR.dll 0d413a80; its harness
  walkers-disguise, Linux headless, PASS by OBVR's report): its gesture sent
  'WGBL' FF000F89, taken here - "on (OBVR's gesture) - 40 real seconds" -
  'WGDS' 1 and 0 received there, the vignette in and out. Blood on the
  hands: OBVR laid a decal on the first-person hand, not seen in its
  pictures at first; seen since OBVR e39911b (OBVR.dll a68625b3, its run
  through our Test=1 dialog, by OBVR's report).
- Not seen: the dialog in the real flow (a dead walker activated while
  sneaking - the run offers it as that would), the rain and the swim ends
  (the run stood indoors, precipitation 1), the ability's icon, the
  StopCombat ending a fight in the picture (headless), the headset, OBVR's
  gesture.

#### 4.20.2 Noise draws them - `[Noise] Enabled=1`, `Radius=1200`, `SearchSeconds=30`, `TravelSeconds=60` - BUILT 2026-11-05

A blow that lands, a spell cast and a thing thrown in VR draw the hordes
near it. Nadi, 2026-11-05: "go 5", in the INI and LINK, on by default.

How (`NoiseLogic.h`, `HordeLogic.h` HearNoise and StepHorde, `Horde.cpp`,
`Rise.cpp`):
- The noises: xOBSE's OnHit (where the hit actor stands), OnSpellCast and
  OnScrollCast (where the caster stands; the spell's blow, where it lands,
  comes as an OnHit), and OBVR's 'WNOI' (16 bytes: x, y, z floats, a kind
  word 1 = thrown; the player's worldspace). Outdoors only - the hordes are.
  Queued as they come, a noise within SearchUnits of one queued already
  dropped, heard at the horde's next look (once a second).
- Heard by every group with a fit walker (not fighting) within `Radius` in
  the same worldspace. Its marker goes on the spot, the leader is led there,
  its followers follow. On the spot (ReachUnits, 300) - or after
  `TravelSeconds` on the way - they search for `SearchSeconds`, the marker
  hopping within 400 units of the spot every 10 s; then the marker goes
  LeadUnits ahead again (a group that held a gate holds it again).
- A noise within 400 units of the spot a group searches makes the search
  last anew (a fight going on); one farther off draws it on.
- A walker that sees the player fights as ever: the engine's combat makes it
  unfit, it drops out of its group (4.11).
- LINK (version 19): `ini_noise_enabled`, `ini_noise_radius` (200-5000),
  `ini_noise_search` (5-120 s), the horde configured anew on a change.

Deviations from the design of 2026-11-05, none silent:
- Per group, not per walker: a group goes as a whole when one of its fit
  walkers hears - the horde's markers and packages are reused, no new
  package per walker. Walkers the horde does not hold (low process, inside)
  do not react.
- Spells are heard at the caster (OnSpellCast), not through
  OnMagicEffectHit: the latter also comes for effects not cast at that
  moment (derived from xOBSE's EventManager - it marks every magic effect
  applied; not measured in the game).
- The throw: Walkers takes 'WNOI'; OBVR sends it since its 8d6d2a2
  (2026-11-05, `[Hands] ThrowNoise=1`): once a throw, at its first hard stop
  or where it comes to rest - OBVR's harness throw-noise-weapon: "'WNOI' to
  Walkers taken" (OBVR's measurement, not run here).

Measured (harness/noise-draws.txt through tools/noise-harness.sh, Linux,
headless, 2026-11-05 14:20, PASS): some hundred walkers out at Tamriel
-29,5, three attacking the untouchable player. 33 noises heard (blows and
spells - the spells a fight's off to the south-west); groups drawn from
1057 and 1328 units away ("marker to ..., on the noise for ..."); group 10
at the noise and searching after 1 to 13 s, drawn on as its fight moved.
Not seen in the game: a search running out (each fight went on through the
run; NoiseLogicTest covers it), the throw, the headset. The first run
(14:14, with tdetect) had no blow land at all; the second (14:17) showed the
fighters' own group counted as hearing - since then only fit walkers hear.

#### 4.20.3 Infection - see 4.14

Back on the list as an idea of that day (Nadi, 2026-10-26: at the very end).
A bite infects; the player who dies infected rises as a walker (a game over,
or a curse to cure); a chapel or a potion cures. Cost **medium**.

#### 4.20.4 Walker kinds - `[Kinds] RunnerPercent=2`, `BloatedPercent=5`, `ArmoredPercent=5` - WON'T DO (Nadi, 2026-11-09: the design does not please him; kept here)

Rare kinds among the spawns and the risen: a runner (runs - the exception
to 4.4), a bloated one (a poison cloud where it falls), an armored one (a
helmet - only a hard blow to the head counts, or the cut). Cost **medium
each**: the records and the rules are there (the looks, the pace, 4.17),
new are each kind's effect, its look and its share in the lists; unit flows
for the shares, a harness per kind.

#### 4.20.5 Nights are worse - `[Night] Enabled=1`, `SpeedMultiplier=1.3`, `SpawnPercent=150`, `HordeLeadPercent=150`, `StartHour=20`, `EndHour=6`, `RampHours=1` - BUILT 2026-11-05

At night the walkers are quicker, the spawns give more, the hordes move on
farther; dawn calms them. Nadi, 2026-11-05: "go 3", in the INI and LINK, on
by default.

How (`NightLogic.h`, `Rise.cpp` StepNight, `Replace.cpp`, `Horde.cpp`):
- Once a second the hour (GameHour off GameDaysPassed's clock) gives the
  night's share: 0 by day, 1 at full night, rising over `RampHours` after
  `StartHour` and falling over `RampHours` before `EndHour` (across midnight
  when the start is the later hour; start and end alike: no night). In 12
  steps a ramp hour - the records and the list are written only when the
  step changes.
- The pace: the 64 walker records' Speed (4.4) at `[Zombies]
  SpeedMultiplier` times the night's factor (up to `SpeedMultiplier` 1.3).
  Measured: a live walker takes the record's new Speed within the 2 s of
  the log (below).
- The spawns: each count slot of the variant list (4.6) at `SpawnPercent`
  of the day's, rounded, at most 20 (`kNightWalkersPerSpawnMost`; the day's
  most is 10) - the defaults' 1..10 become 2..15.
- The hordes: the marker `HordeLeadPercent` of `[Horde] LeadUnits` ahead
  (a guess at "move farther", Nadi's word; the pace makes them cover more
  ground on its own too). Not seen in a run with a horde yet.
- The rumor: the ESP's GLOB `WalkersNight` (local 0x9D7) is 1 while
  `Enabled`, an INFO of the Rumors topic (0x9D8) said while it is 1 - at
  random among the vanilla rumors (DATA flags 0x02, as Oblivion.esm's 76
  rumors without a condition), not before them as the siege's are. Not
  heard in the game yet.
- Not changed: the grab's chance, the siege's rules. The siege's walkers
  share the walker records, so they are quicker at night too (derived; the
  pace has one value per record).
- LINK (version 18): on/off, the pace, the spawns, the horde's lead, the
  start and the end hour - live, the next second's step applies them.
  `RampHours` only in the INI. LINK's slider holds the pace to 0.5..2.0: an
  INI value past it is read back clamped once LINK is loaded (3 read as 2,
  14:05).

Tests: `NightLogicTest` (the hours, the ramps, across midnight or not, no
night, off, the clock unknown, the steps, the factors, the counts),
`LinkLogicTest` (the six at 128..133). Harness `night-pace.txt` through
`tools/night-harness.sh` (`SpeedMultiplier=2`, `RampHours=0`), PASS
2026-11-05 14:12: noon "step 0 of 12", 1:00 "step 12 of 12 - pace x2.00,
spawns 150 %, the horde's lead x1.50", record 26000803 from 10 to 19, the
counts "2 3 5 6 8 9 11 12 14 15 (150 % of the day's)", 12:00 back to 10 and
"1 2 ... 10"; the walker (record 2600091B) at 97, 81, 112 units/s at night,
35 to 84 (mean about 55) by day after. Open: the headset's feel, a horde at
night, the rumor heard.

#### 4.20.6 Survivors' camps - `[Survivors] Enabled=1` - step 1 (defending) BUILT 2026-11-05, step 2 (escort, things, news, camp life) BUILT 2026-11-08

Groups of refugees on the roads and in small camps, to escort to a city or
to hold against a horde; a camp overrun becomes walkers. Cost **large**:
NPC groups, their packages and places, the fight's outcome, the news.
Nadi, 2026-11-05: "go 4.20.6 (default on)". Built as the design's first
step - the camps and their defence; the escort (step 2) is not built.

How (`CampLogic.h`, `CampPlaces.h`, `CampSpots.h`, `Camps.cpp`,
`tools/camp-places.py`, `tools/esp-write.py`):
- Places: twelve, proposed by `tools/camp-places.py` off Oblivion.esm's
  roads (the ROAD record's points and links, sampled every 1000 units): at
  least 9000 units from every city gate, 2500 from every door, 2000 from
  every map marker, moved 450 units off the road to its flatter side, the
  land within 400 units varying at most 90 (LAND VHGT); each the farthest
  from those picked before. Named after the nearest map marker (no Oblivion
  gate's): near Hrota Cave, Walker Camp, Darkfathom Cave, Ceyatatar,
  Crestbridge Camp, Bruma Caverns, Pillaged Mine, Bawn, Mortal Camp, Fort
  Urasek, Mingo Cave, Dragonclaw Rock. Not picked by Nadi yet (the "still to
  settle" below) - a place is a row of `CampPlaces.h`; the ESP and
  `CampSpots.h` are written anew from it.
- The ESP: fourteen refugee bases (copies of Oblivion.esm's generic bandits
  and marauders for faces, races and sexes - six men, six women, a trading
  man and woman - without factions, packages, scripts, spells and things;
  "Refugee", generic so the rise takes them, aggression 5, confidence 70,
  dressed in lower-class clothes with a rusty weapon). Each camp in
  Tamriel's persistent cell (00023777, the ESP carries Tamriel's WRLD record
  verbatim but its OFST table): thirteen people (six men, six women, the
  trader - persistent, initially disabled), a map marker "Refugee Camp"
  (hidden until found), a campfire (DExteriorCampFire300, the flame node,
  three log piles), two tents with a bedroll each, two bedrolls by the fire,
  each on the land's height. The trader sells as Thoronir (services 0x359F)
  from one merchant chest in the holding cell (XMRC). A pool of 24 walkers
  (two camps of twelve), two markers and two travel packages in the holding
  cell.
- The roster: PeoplePerCamp (2..6) of the slots, WomenPercent of them women
  (rounded half up), and the trader (Trader). A living camp nobody of which
  is lost takes a changed setting at once; one that lost people when it
  comes back.
- Attacks: each new game day, each living camp by AttackChancePercent, at a
  random hour of that day; AttackWalkersMin..Max walkers. The camp shown
  (the player within 6000 units, outdoors, at most CampsAttackedAtOnce of
  them, the nearest first): up to twelve walkers at once out of the pool on
  a ring 1600 units out, away from the road, led at the camp by the travel
  package (the marker moved onto the camp's map marker); each walker ended
  (4.17) counts off, the next comes out; no horde takes them (busy). Not
  shown: the toll each game hour - RefugeeKillsPerHour walkers cut down (a
  part carried), one refugee lost by LossPercentPerHour (disabled).
- Ends: the last walker ended with a refugee alive holds the camp; shown
  during the attack it is defended - ModPCFame FamePoints (RewardFame),
  Gold (RewardGold) and always a line ("The refugees near ... are safe") and
  UIQuestUpdate's sound; held away, nothing. The last refugee dead (in the
  game: each step reads the people of a camp the player is at) overruns it:
  those killed in the game rise (generic, the safeguard leaves them alone -
  `Victim::campPerson`); the walkers left and, away, the ones lost stay at
  the camp, among its tents when the player comes (the pool). Each new day
  an overrun camp comes back by ReturnChancePercent: its dead raised
  (Resurrect), stripped and dressed anew (the rise moved their things onto
  their walkers), all enabled where they lay.
- Co-save "CAMP" v1: the last day rolled, per camp status, roster, lost,
  walkers left, the coming attack, the toll's day and carry.
- LINK (version 20): all fifteen (137..151), live. `TestCamp` only in the
  INI (a test switch).

Deviations from the design, none silent:
- The escort, "gold or things" and the news: built 2026-11-08 (step 2,
  below).
- Rewards come only for an attack the player was at when it was shown; a
  camp that holds while he is away gives none.
- The end rule at camps (4.17): with DieOnlyBeheaded=1 and OBVR cutting
  heads, a walker the refugees felled got up again - the run of 14:56 below,
  no attack could end. Nadi, 2026-11-05: "go A, aber der kopf fliegt einfach
  ab egal ob trifft oder nicht beim kill": a walker felled by a camp's
  refugee ends (EndLogic.h `EndedByRefugee`, its killer one of the camps'
  people), and OBVR is asked to cut its head off wherever the blow landed -
  'WGCT' to OBVR, one word, the walker's reference, sent the frame after the
  death; OBVR answers with its 'WGHD' as for a cut of its own. OBVR's side:
  its adcdbda (2026-11-05) - camp-defend twice PASS on OBVR's side, every
  'WGCT' walker "its head cut" and its 'WGHD' taken; a pool walker raised
  again gets its head back (the engine rebuilds it at the Resurrect, OBVR
  forgets the cut of an actor alive again - its dismember-resurrect PASS).
  OBVR's measurements, not run here. OBVR notes its roster stops at 64
  actors (a camp attack had 142): 'WGCT' works around it, its killing-blow
  search does not yet.
- The refugees stood where they were put (no packages) until 2026-11-08:
  since then they sleep and wander about where they stand (step 2, below).

Measured (harness/camp-defend.txt through tools/camp-harness.sh, Linux,
headless, 2026-11-05, PASS both): the twelve camps set up at the load (7
enabled, 6 disabled each - roster 11C7: three men, three women, the trader).
14:56, the INI's DieOnlyBeheaded=1 and OBVR cutting heads: TestCamp=7
attacked, the player at it, six walkers out on the ring (MoveTo answered 1
each), the refugees felled all six (their killers the refugees' IDs), each
"down - it gets up again"; a refugee killed rose as walker 26000938 - none
ended, so the attack could not end. 15:00, CAMP_ENDS=1 (DieOnlyBeheaded=0):
two refugees killed by walkers (both rising), the six walkers ended one by
one, "camp 07 near Bawn defended ... (+1 fame, 150 gold)". Found and fixed on
the way: the walkers were led in the step they came out - now the step
after. The console answers 0 for AddScriptPackage and PlaySound whether they
take or not (the horde's lines say the same of AddScriptPackage): the walkers
reached the camp and fought; the sound not heard (headless).
Not seen in the game: the toll away, an overrun, a camp coming back (the dead
raised and dressed), the trader's barter, the map markers on the map, the
props' looks and collision, the reward's gold and fame in the player's
stats, the headset. CampLogicTest covers the flows of the decisions.

15:1x, after "go A" (DieOnlyBeheaded=1, OBVR cutting): five walkers felled by
the refugees, each "felled by a refugee - OBVR asked to cut its head ('WGCT'
dispatched: 1)" and "ended at camp 07 ... N left"; one refugee dead (rising);
the sixth walker still fighting when the run ended. The head flying off not
seen (OBVR does not handle 'WGCT' yet; headless).

Open for Nadi: the escort (step 2) - built 2026-11-08, below.

**Step 2, BUILT 2026-11-08** (Nadi, 2026-11-08: "go A B C D. 1) passt, und
reward gibts/skaliert je nachdem wieviele überleben. 2) ja" - A the escort,
B things, C the news, D the refugees' camp life; 1 the failure rule proposed
- all dead, or left behind farther than 5000 units; 2 a reward of its own on
arrival). Settings (INI and LINK 160..165, LINK version 27, 166 settings):
`RewardThings=1`, `Escort=1`, `EscortFame=2`, `EscortGold=300`,
`EscortBehindUnits=5000`, `EscortStayDays=7`; test switches INI only:
`TestEscort=N`, `TestEscortBehind=1`.
- B, things: a defended camp gives one thing, an escort one for each two
  who made it (rounded up). A thing is an entry of one of Oblivion.esm's
  LL1LootWeapon100 (00032D6E), LL1LootArmor100 (00032D79) or
  LL1LootPotions2Average (00058F36), drawn at random, picked by the plugin
  for the player's level (`PickLeveled`: the top level at or below his, and
  every entry down to 8 below it; nested lists followed) and given by
  AddItem. Derived, not read off the engine: the 8 is the game's default
  iLevItemLevelDifferenceMax (UESP's leveled lists page), the player's level
  read at his TESNPC's TESActorBaseData +0x0E (xOBSE GameForms.h) - logged at
  each thing ("at the player's level N"). Not used: AddItem with the list
  itself (whether the engine resolves a list there is not documented - CS
  wiki AddItem says nothing; xOBSE's CalcLeveledItem exists but is a script
  function).
- C, the news: two Courier editions, "Refugees Saved on the Road!" (a camp
  defended or brought to a city) and "A Refugee Camp Overrun" - the camps'
  latest within RecentDays, after the sieges and the attacked cities, before
  a horde heading (`ChooseEdition`; co-save NEWS version 4). The books are
  generic: they name no camp (the editions are pre-written ESP books). A
  rumor at random among the others while the camps are on (global
  WalkersCamps).
- D, camp life: the refugees' bases carry two packages - sleep near where
  they stand at night (a copy of aaaSleepCurrent22x8, 000A3A5A: 22:00 for 8
  hours, 1000 units) and wander about it otherwise (TWander, 000177D6, at 600
  units); "EvaluatePackage" after each Enable, so "where they stand" is the
  camp after a kit moved. Whether they lie down on the camp's bedrolls and
  how it looks: not seen (headless).
- A, the escort: the refugees' greeting (GREETING, 000000C8, two lines with
  the speaker one of the 14 refugee bases by GetIsID, OR'd, and the global
  WalkersEscortOffer at 1) asks to be led to the city and adds the topic
  "Safety"; its line's result script sets WalkersEscortAsk to 1 (the
  compiled bytes of Oblivion.esm's INFO 000A343E "set HighwaymanGotMoney to
  1", its SCRO swapped). The plugin sets the offer (1 while a living,
  unattacked camp with a kit is within 3000 units and no escort runs, 2 on
  the way - their greeting then "Keep going ..."), reads the ask, and begins
  the escort of the nearest such camp to the city gate whose outside spot is
  nearest the camp (`NearestGate`). Its people get the follow package
  (FollowPlayer's copy, 200 units) by AddScriptPackage, sent again every
  5 s (it does not take in dialogue - CS wiki AddScriptPackage). Each second
  outdoors: the nearest of them farther than EscortBehindUnits from the
  player fails it - RemoveScriptPackage, their kit freed, the camp living
  again (set up anew when the player comes); all dead fails it - the camp
  empty (`Left`), its dead rise as any; one within 1500 units of the gate
  brings them - fame and gold scaled by those who made it (`ScaledReward`,
  half up), things, the line "You brought N of M refugees safe to <gate>",
  the quest sound, the news; RemoveScriptPackage, they wander by the gate.
  Their kit stays with them while the player is within 18000 units of the
  gate, then is freed (they are in the city, unseen); EscortStayDays after
  the arrival they leave the game, the camp `Left` - settled again by
  ReturnChancePercent as an overrun camp. One escort at a time. Co-save
  CAMP version 3 (the escort's camp, gate, those set out, arrival day, kit;
  version 2 read with none); a save loaded on the way keeps the kit where
  its people are.
- Deviation, said now: "stays 7 game days in the city" - they stay by the
  gate outside while the player is near, then unseen; they are not moved
  into the city's worldspace (where inside is not chosen, and a MoveTo into
  another worldspace for 4 kits is unmeasured). Cheap to change if wanted.
  **Changed 2026-11-09** (Nadi: "Eskorte endet in stadt als toggle default
  an"): `EscortIntoCity=1` (INI and LINK 169, LINK version 30, 170
  settings). Arrived, they are moved into the city through the gate - MoveTo
  its city-side door with the offset of the spot the Tamriel-side door's own
  teleport puts one on (tools/city-gates.py, CityGates.h inDx/inDy/inDz),
  side by side - and wander its streets. Their kit is out while the player
  walks the city's worldspace (the city-side door's) or stands within 18000
  units of the gate outside (`EscortKitWanted`); freed otherwise and put back
  in the city's streets when he comes again (SetUpKit); EscortStayDays on
  they leave. Off: as before, by the gate. On the way the escorted kit is now
  kept wherever the player goes - before, a door into a house freed it (the
  distances read -1 indoors).
  Harness camp-escort 2026-11-09, PASS: "7 taken into the city by Skingrad
  West Gate (MoveTo its city-side door 00007DE0)", the line "... - they go
  into the city"; 45 s after the load all 7 in worldspace 0001C31D
  (Skingrad's) with the player outside; the player taken in, all 7 there,
  325-573 units from him. Not seen: how they look and walk there, the kit
  put back on a second visit, the 7 days' end in the city.
- Unknowns, for the harness and the headset: whether the greeting wins over
  the vanilla greetings for a refugee (PNAM none - first in the topic - as
  the siege's rumors come first; not measured), whether the follow package
  holds across a load, whether the followers keep up through doors.
- Harness `camp-escort` (tools/camp-harness.sh CAMP_SCENARIO=escort, with
  ESCORT_BEHIND=1 the failure): the ask set as the yes would, the distance
  after 20 s, then arrival at the gate (or the failure).
- Harness `camp-escort` 2026-11-08 20:4x, Linux headless, **PASS**: camp 07
  near Wellspring Cave asked (offer 1), the escort begun to Imperial City
  Prison Gate, AddScriptPackage to 7; one refugee killed on the way by a
  walker near (slot 1, rising); 20 s after the ask the six living stood 197
  to 699 units from the player (set out some 1500 off) - **they follow**;
  taken to the gate: "You brought 6 of 7 refugees safe to Imperial City
  Prison Gate (+2 fame, 257 gold, some of their things)" - 2 x 6/7 = 1.7 to 2,
  300 x 6/7 = 257; the things FurGreaves and IronBoots off LL1LootArmor100 at
  the player's level 1 (read: plausible for the harness save, not checked
  against the game's own). The third drew nothing off
  LL1LootPotions2Average (its entries start at level 2) - since then the
  next list is tried. Not run: the failure (ESCORT_BEHIND=1 - Nadi stopped
  the session for Windows), the greeting and topic in dialogue, camp life
  (sleep, wander), the news books, the headset.
- Harness camp-escort with ESCORT_BEHIND=1, 2026-11-09, four runs: the
  failure PASSes each time ("left behind; 7 go back to the camp" once the
  player is 7000 off).
- **Bug - the follow was unreliable (fixed below)** (the same four runs): 20 s after
  the ask most had not come - 6 of 7 at 1170-1550 (run 1), 6 of 7 at
  1330-1835 with the resend off (EscortFollowSeconds=0, run 2: the resend is
  not the cause), 6 of 7 at 1810-2150 (run 3), 4 of 7 at 1400-1560 (run 4).
  The probe (Horde.cpp ActorPackageNow) read what they run: those near the
  player the follow package (26000A42); the others their own wander
  (26000A43) - the script package gone - or a package the engine made at
  run time (FF00xxxx); none in combat. The run of 2026-11-08 (all six near)
  had a fight at the camp. Not known yet: why the script package drops
  (AddScriptPackage keeps it "until it is done" - CS wiki; FollowPlayer's
  PKDT flags 0x00400004, must complete) and what the FF packages are. Many
  walkers were near (99 loaded, 12 hordes).
  Measured 2026-11-09 (probe each second for 20 s, TestEscort): the three
  that came ran WalkersRefugeeWander (type 5) at the ask, then the follow
  package (type 1) from the next second and closed in (1544 to 190 units in
  7 s). The four that stayed ran a package the engine made (FF00xxxx) of
  type 18 - past xOBSE's enum (Find 0 .. CombatController 12, 0x0D) - from
  before the ask to the end, two of them sharing one (slots 0 and 12, slots
  1 and 6), none in combat, not moving. Derived: two refugees talking to
  each other (an engine-made package shared by a pair, an interrupt;
  AddScriptPackage does not take during one - CS wiki). Not read off the
  engine: what type 18 is.
  **Fixed 2026-11-09** (Nadi: "a"): at the first look a second after the
  ask, then every EscortFollowSeconds, one running a package the engine made
  (its ID's top byte 0xFF) and not fighting is disabled, enabled the next
  second and given the follow (`EscortResets`; re-enabled at once when the
  escort ends in that second). Harness camp-escort twice, **PASS**: 6 of 7
  held by type 18 each run, all reset; 20 s after the ask all 7 within
  148-262 units of the player (the probe: they closed in from 1100-1700 in
  some 12 s); the failure run "left behind"; the arrival run "You brought
  7 of 7 refugees safe to Bravil (+2 fame, 300 gold, some of their things)" -
  four things, all at the player's level 1. Not seen: the blink of the reset
  (headless).

Dynamic camps (Nadi, 2026-11-05/06: "Go dynamic") - **BUILT 2026-11-06**,
props open (below). `CampCount` default 64 (INI and LINK, LINK version 21,
153 settings), `CampSpacingUnits` 10000 (about 140 m, INI only; Nadi: "ok").
- Places: `tools/camp-places.py` prints `src/CampCandidates.h` - 140
  candidates (21 by the roads, 119 off them): flat land above the water,
  within 12000 units of a road point (Tamriel's land goes on past the border
  and up the peaks; the first list without it put camps at 283136,-107008 and
  z 49808), 450 units clear of every tree, rock and other solid reference and
  each prop 300, away from gates, doors and map markers, 2500 apart. Each
  carries its props', people's and walker spots' heights.
- Measured off the file: the land the player reaches, free of trees and
  rocks, holds only 64 to 71 camps 10000 apart (200 random orders of the 140);
  with 900 units clear only some 35. So 64 at 140 m is at the edge:
  `RollCampPlaces` tries up to 8 orders and keeps the one placing most; a
  camp left without a place is out of play (logged).
- Rolled once per game (a new game, or CampCount raised), kept in the co-save
  'CAMP' version 2 (64 camps, 28 bytes each, the place added). Version 1 (the
  twelve fixed camps) is not read: those camps are rolled anew.
- ESP (since Nadi's "A", 2026-11-06, below): at every candidate its map
  marker (persistent, initially disabled, hidden until found, 0xAA7 + the
  candidate) and its props in their exterior cells (156 cells' CELL records
  copied from Oblivion.esm, nested in their blocks and sub-blocks), the
  marker their enable parent. The plugin moves nothing: it enables the
  markers of the candidates in play and disables the rest. The plain ESP
  lays out all eleven props (1540 references, their IDs past 0xFFF - only
  the props, which the plugin never looks up); the ESL eight (one log pile
  of three, one bedroll by the fire of two: 1120, the last ID 0xF93) - Nadi,
  2026-11-06: "Mach die esp großzügiger. und die esl light".
- 4 kits of 13 people (persistent, initially disabled). A kit goes to a camp
  within 14000 units of the player, the nearest first, and is freed past
  18000 (`ChooseCampKits`); 52 refugees instead of 156. A kit's dead are
  raised and dressed before it moves and taken off the rise's list
  (`UnlistRising`).
- `TestCamp=N` now also takes the player to camp N, 30 s after the load (the
  harness cannot know where it was rolled), and 15 s after the attack drops
  the player from 600 units onto the camp's first tent, then onto the ground
  toward the road.
- Harness `camp-defend` 2026-11-06 12:03 PASS: 64 places rolled, camp 07 near
  Wellspring Cave attacked, kit 0 moved there, six walkers out and led, all
  six cut down by the refugees ('WGCT' each), defended with the reward.
- Gone: the twelve fixed places (CampPlaces.h, CampSpots.h), the probe's
  code (CampProbeLogic.h, camp-probe.txt/.sh - in history at d9f4e6d).

Harness `camp-defend` 2026-11-06 13:14 PASS with the props in the ESP: 64
places rolled, "64 candidates' markers and props enabled, the other 76
disabled (0 not found)", camp 07 near Imperial Sewer attacked, the six
walkers out, led and cut down, defended with the reward. The drop onto the
first tent (38109,63453) stopped at 3685 - the land there 3584 (Oblivion.esm,
tools/camp-places.py ground_at), 101 over it, the tent's roof as at the
first probe (103) - then slid to 3663: **the tent is solid**. Not seen: the
camps' looks, the map markers on the world map (whether a disabled marker
stays off it), the ESL variant in the game.

Superseded (Nadi chose A) - the props across the map (measured 2026-11-06, the far probe, camp
07's tent to camp 04's place 77000 units off, the player dropped onto the
tent's spot and a ground spot):

| How | after | the tent's spot |
|---|---|---|
| SetPos x y z + Update3D | the player taken there (MoveTo) | ground (1016) |
| same | Update3D there again | ground (1016) |
| MoveTo the far map marker + Update3D | taken there | ground (1019) |
| same | Update3D there | ground (1016) |
| same | SaveGame, Load | ground (1009) |

The tent's top would stop a fall near 1103 (its base 1000; at camp 07 the
top read 103 over the base). MoveTo set its parent cell to the far one
(000066DB to 00006059), the word at 0x3C (read as its 3D node, offset
unverified) held its old value after Update3D. So a prop moved across the
map is **not solid** there, whatever was tried; within the loaded cells
(500 units) SetPos + Update3D was. Whether it is seen there is not known
(headless pictures are black). The kits' props are moved that way now: the
camps work (people, walkers, rewards), the tents likely stand unseen or
without collision. Options for Nadi, 2026-11-06 (A chosen):
- A: the props laid out at every candidate in the ESP (140 x 11 = 1540
  references in their exterior cells, each under one persistent enable
  parent per candidate, initially disabled), the plugin enabling a camp's
  parent - the engine's own way, nothing moved. Cost medium: the ESP's
  exterior cell groups are new (some 140 cells' CELL records copied), the
  kits keep only the people.
- B: PlaceAtMe the props at a kit's setting up and delete them on freeing -
  the new reference catching unknown (the console gives it back to no one),
  medium and unmeasured.
- C: probe further (moved while the far cells are loaded, Disable before the
  move) - small each, the outcome unknown.

Probe run 2026-11-06 09:4x (`tools/camp-probe.sh`, `harness/camp-probe.txt`,
[Survivors] TestProbe=7, src/CampProbeLogic.h): camp 07's first tent
(26000AC6) moved 500 units out by SetPos x/y, the player dropped from 600
units above onto the new and the old spot after each step. Measured (where
each fall first stopped; the tent's top reads 2327, the new spot's ground
2173, the old spot's 2243):

| After | new spot | old spot |
|---|---|---|
| nothing (before the move) | 2176 (ground) | 2327 (tent) |
| SetPos | 2177 (ground) | 2327 (tent) |
| Disable + Enable | 2173 (ground) | 2329 (tent) |
| Update3D | **2327 (tent)** | **2243 (ground)** |

- SetPos moves the reference (its position reads the new place) but not the
  collision; Disable/Enable does not either; **Update3D does** - the
  collision follows to the new spot and leaves the old.
- The tent keeps its z (2224): at the new spot, 52 units lower, it floats.
  Moved props need the new spot's ground (derived: precomputed with the
  places, as tools/camp-places.py reads the land).
- The map marker's reference reads the new place after SetPos; whether the
  world map shows it there is **not seen** (the headless pictures are black).
- The cells reloaded (player 30000 units off and back): **inconclusive** -
  the player hung at the drop height for both drops (did not fall within 4 s
  after 10 s back); not needed, Update3D answers it.

Grab shock sound (Nadi, 2026-11-06: "go B"; then "default #1, wir bieten
aber alle drei in LINK und oder ini an") - **BUILT 2026-11-06**: a stinger of
about 2 s when a walker's grab begins (Grab.cpp PlayShock, at the hold idle
and the test grab), `[Grab] ShockSound` 1..3 (0 none, default 1) in the INI
and LINK (setting 153, LINK version 22). The three WAVs are the project's own,
synthesised by `tools/grab-shock-sounds.py` (no samples; the freesound CC0
candidates were rejected), under `sounds/fx/OblivionWalkers/`, deployed and
packed to `Data\Sound\fx\OblivionWalkers\`; three SOUN records in the ESP
(0xAA4..0xAA6, copies of UIQuestUpdate 00185DD3, a 2D sound), played by
`PlaySound` on the player, none within 3 s of the last (GrabLogic.h
ShockSoundDue, GrabLogicTest). Harness `grab-hold` 2026-11-06 12:4x PASS
(ChancePercent=100 for the run): "Grab: shock sound 1 (PlaySound 26000AA4
answered 0)" right with the grab. Whether it is heard is not known (headless;
the console's answer is no success flag) - for the headset.

#### 4.20.7 Refugees hold dungeons - designed with Nadi 2026-11-11, not built

Refugees hole up in dungeons; walkers try to get in. Idea of 2026-11-05,
the vision settled in a question round on 2026-11-11 (Nadi's answers):

- **Places**: every dungeon kind - forts, mines, caves and Ayleid ruins -
  but never one a quest needs (main quest, guilds, Daedra, side quests).
- **Choice of places**: a fixed pool of checked candidates, the situation
  dynamic - as the camps (4.20.6: the places fixed, who lives there, the
  attacks and the overruns rolled at run time). A script checks every
  dungeon beforehand (its door in Tamriel, room at the arrival point, no
  quest's) into a pool of some 20-40; at run time the game rolls which of
  them is held, besieged or to be cleared, and that changes over time.
- **At once**: 3-5 places active (INI).
- **Where they live**: sometimes inside only, sometimes both (some outside
  in front of the door, the rest inside).
- **Variants**, equally likely by default, each weight in the INI:
  1. Refuge - held and quiet, trade and a bed.
  2. Besieged - held, walkers storm it, the player helps.
  3. To clear - bandits or creatures inside, the refugees wait outside;
     the player clears it, then they move in.
  4. Walker dungeon - an overrun place; cleared, it can be taken again.
  The occupants of a held place are disabled while it is held.
- **Overrun**: the dead refugees rise as walkers, the place becomes a
  walker dungeon (variant 4) - the circle closes with "take it".
- **What the player does**: defend, rest (refuge), bring refugees there
  (an escort from a camp to the place instead of a city), take a place.
- **Quest display**: a real journal quest with stages (e.g. "Fort X is
  besieged" -> "Clear Fort X" -> "Defended"), the place's map marker as
  the quest target, vanilla's quest-updated sound.
- **Dialogue**: 2-4 topics a variant - a greeting by the place's state,
  "What happened?", "How can I help?", trade/rest; conditioned on the
  place's state, silent (no voice), as the escort's lines (quest priority
  50, see 7a).
- **Finding one**: all of - rumors and dialogue (refugees, guards,
  innkeepers), finding it oneself (the quest starts on entering), a
  messenger who comes to the player (as vanilla's), the Black Horse
  Courier, and MOO's notice board.
- **Reward**: as the camps' (fame, gold, a thing - their settings).

**MOO's notice board** (read 2026-11-11 off the installed MOO 5.0.7:
`Maskar's Oblivion Overhaul Readme.pdf` chapter 8, `... for
compatibility.ini`, `... for quests.ini`): the readme names the board's
notice kinds only (creature invasions, houses for sale, wanted npcs,
stolen, lost items, missing pets, the player's bounty, the Shivering Isles
door) and no way for another mod to post one; the INIs configure only the
boards' places (`MOO.ini_noticeboard_<city>_xpos/ypos/zpos`,
`MOO.ini_noticeboard_worldspace`). So a notice of ours on MOO's board is not
offered - not checked: MOO.esp's scripts (whether a notice list could be
written from outside; reverse engineering, fragile across MOO updates).
Proposed: a notice of our own (an ESP object, read to start the quest)
placed next to MOO's board when MOO is loaded - its position read from
those INI variables - and at the city gate or the inn otherwise.

Found on the way: `... for quests.ini` lists, by map marker, the places
MOO holds back until a quest is done (campsites, Ayleid ruins, ...) - a
cross-check for our list of quest places, not a replacement for reading
Oblivion.esm. MOO also changes dungeons itself (dungeon extensions, vampire
invasions turning a region's dungeons' inhabitants into vampires) - a place
of ours must not be one MOO took over; how to read that is open.

Cost: **large** in all (the variants, an interior cell's people and attacks
through the door, the journal quest with stages, the dialogue, five ways to
find one). Reused: the camps' pool, roster, attack and end logic, the rise,
the escort, the ESP writer (quests, INFOs), the rumors, the Courier.

Open, to measure before building: whether a held place stays held across
its cell reset (the engine respawns leveled occupants after 3 days);
whether the spawn replacement (4.6) or a held place wins over its
occupants; how walkers behave in an interior cell that is not loaded (the
toll away, as the camps'); the roster cap (64; a camp attack had 142
actors) shared with the camps; the price of far actors (OBNG measured
2026-11-11 at Anvil's docks some 14-15 walkers ~150 m away in full high
process every frame, about half of the actors' 5 ms). First step, cheap:
the candidate count (the script over Oblivion.esm, quest places out).

**Candidate count, 2026-11-13** (`tools/dungeon-candidates.py`, its rules in
its header): 327 interiors have a door in Tamriel; 213 of them are
dungeons by their map marker (90 caves, 50 Ayleid ruins, 48 forts, 25
mines). Out: a quest names something of it (84 - its references, cells,
the door outside or its map marker, a unique person of a quest, a quest
item; object scripts that run a quest; MOO's held-back markers add one,
Fort Irony), fewer than 6 path grid points within 1024 units of the
arrival (29), an owned cell (5). Left with Oblivion.esm: **107** (38
caves, 24 ruins, 33 forts, 12 mines); with Oblivion.esm, UOP, the DLCs and
Knights read for their quests: the same 107; with Nadi's Windows load
order (63 plugins - first read as the Linux one: an OBNG run had linked the
Windows list in at the time): **83** - Quest of the Week (QOTW0xx, 17
places) and Better Cities' quests (BAN..., 6) take the rest, nearly all
through a quest's stage result script (SCRO) or target (QSTA). Nadi's own
Linux list (39 plugins, no QOTW, no Better Cities): 107. Cells per dungeon: 27 one,
49 two, 26 three, 5 four; the room at the arrival 6 to 56 points (median
11). Three dungeons have two doors in.
Not caught (UESP, read 2026-11-13): Vilverin ("quest-related": selling its
Ayleid statue starts Umbacano's quest - the statue carries no quest flag)
and Lost Boy Cavern ("quest-related", a lich boss) are in the pool; quests
that send people there by their AI packages are not read. So the pool of
20-40 taken from the 107 is to be checked against UESP's place pages
(their "quest-related" mark), one page each. A mod's quest in the player's
own load order (QOTW, Better Cities) the pool cannot know at build time.

**The pool in the game, built 2026-11-13** (Nadi: "passt A + B"): the 107
go into `src/DungeonCandidates.h` (`tools/dungeon-candidates.py --header`,
read with Oblivion.esm, UOP, the DLCs and Knights), each with its cells.
At the first tick `Dungeons.cpp` takes out
- A: one whose cell a loaded quest names, or holds a reference a loaded
  quest names - its script, its stages' result scripts, its targets, the
  names Replace.cpp's CollectQuestNames reads anyway (KeepQuestLists, 7a),
  now also each named reference's parent cell (+0x40);
- B: one the INI's `[Dungeons] Skip` names, by its name or a cell's form ID
  in hex (a list: INI only, not LINK).
Log: "Dungeons: X out - a loaded quest names something in it | [Dungeons]
Skip names it", "Dungeons: 107 candidates, N in the pool, ...". Flows:
tests/DungeonLogicTest.cpp. What A does not see (the build-time scan does,
for Oblivion.esm): dialogue lines' conditions and result scripts (read off
the file on demand, not in memory), object scripts that run a quest, the
unique people a quest names by their record.
Run `harness/dungeon-pool.txt` 2026-11-13 11:28 (Linux, Nadi's 39 plugins):
"Quests: 606 quests read, 12648 names", "718 cells named or holding a
named reference", "Dungeons: 107 candidates, 107 in the pool, 0 out by a
loaded quest" - as the build-time scan of that list says (107).
With the Windows list (OBNG tools/run-with-windows-mods.py, 12:02): "107
candidates, 86 in the pool, 21 out" - Culotte, Charcoal Cave and Fort
Empire missed: Quest of the Week and Better Cities name their MAP MARKERS
(QOTW048 CulotteMapMarker, AqueductQuest CharcoalCaveMapMarker,
BANGHCGoldHorseRanks FortEmpireMapMarker - in Tamriel's persistent cell,
no cell of the dungeon). Since then a candidate carries its door outside
and its map marker, and a quest naming either takes it out. Run 2 (12:04):
"1243 cells named ..., 4322 references named", "107 candidates, 83 in the
pool, 24 out" - the build-time 83. Not the same 24: Ondo is out at build
time only (an object script that runs a quest, DMDungeon06Tafel - A does
not read object scripts; [Dungeons] Skip is for it), Haunted Mine in the
game only (a mod's quest names a reference a mod placed in it - the
build-time scan reads Oblivion.esm's references only).

**The pool's size, 2026-11-13** (Nadi: the 20-40 are no backup - the
situation is dynamic either way; "user können wechseln: A oder B. A ist
default"): `[Dungeons] OnlyChosen` (LINK 174, version 33), 0 the default -
every candidate free of quests may be held; 1 - only the chosen ones.
UESP checked for every candidate (`tools/dungeon-uesp.py` ->
`tools/dungeon-uesp.tsv`, each place page's "Related Quests" and its
"quest-related" mark, 2026-11-13): 16 more are a quest's - A Venerable
Vintage's six forts (Aurus, Carmala, Dirich, Magia, Scinia, Vlastarus),
The Collector's four ruins (Culotte, Vilverin, Welke, Wendir), Black Bow
Bandits (Rockmilk, Undertow), Beldaburo (A Wound In Time), Fort Coldcorn
(The Unbeaten Path), Lost Boy Cavern (its lich), Mingo Cave (The Crimson
Cave) - their quests use dialogue, books and items, not what the scan
reads. Candidates now **91** (34 caves, 19 ruins, 26 forts, 12 mines); the
chosen **32**: at least 10 path grid points at the arrival, three cells at
most, doors 12000 units apart, the roomiest first, shares of 9 caves, 8
ruins, 9 forts, 6 mines (`--chosen-*`). OnlyChosen changes the pool at
once (StepDungeonPool, the log line "Dungeons: ... (OnlyChosen=N) ...").

#### 4.20 Designed with Nadi, 2026-11-05 (decided, not built)

Order of building, proposed: 4.20.5, 4.20.1, 4.20.2, 4.20.6. Every setting
below goes into the INI and LINK.

**4.20.6 Survivors' camps** - `[Survivors] Enabled=1`
- Places: both, as options - fixed places chosen by hand (some 12 along the
  roads, built by the ESP) on by default; random places off by default.
- What the player does: both - defend a camp against a horde, and escort
  its group to the nearest city; built in two steps, defending first.
- Reward, each settable: fame (on), a trader in the camp (on), "gold or
  things" (on - Nadi's "b on"), and without any of them a "quest completed"
  moment (a line, a sound - Nadi: "a gibt ein quest completed ereignis oder
  so").
- Attacks: a chance a camp and game day (15 percent), whether the player is
  there or not; away, the refugees lose by a chance, as the siege's toll.
- People: the share of men and women settable, 50/50 by default.
- Overrun: the dead rise as walkers (as designed in 4.20.6).

**4.20.1 Walker blood as a disguise** - `[Disguise] Enabled=1`
- Smearing: flat, activating a walker's corpse and a question; in VR a
  gesture (a hand in the corpse, then over the chest) - OBVR's gesture,
  a message to Walkers, as the shake.
- Lasts: settable, 3 game minutes by default.
- Breaks: an attack on a walker, and water (rain or swimming); running does
  not break it.
- Shown: a light red tint and a line; in VR a dark rim and blood on the
  hands (OBVR).
- The others: guards and people do not react (first step).

**4.20.2 Noise draws them** - `[Noise] Enabled=1`, `Radius=500`
- Noise: blows that land, spells, thrown things (xOBSE OnHit,
  OnMagicEffectHit, OBVR's throws); doors and traps not (no clean hook).
- Reach: settable, 500 units by default (Nadi; the proposal was 1500-3000
  by kind - 500 is some 7 m, asked back 2026-11-05).
- The walkers go to the spot and search it for 30 s, then back to their
  horde; one that sees the player attacks as ever.
- Distraction: yes - a thrown bottle draws them to where it breaks.

**4.20.5 Nights are worse** - `[Night] Enabled=1`, `SpeedMultiplier=1.3`, `SpawnPercent=150`
- At night the walkers are 1.3 times as quick and the spawns give 1.5 times
  as many; the grab's chance stays. Each settable.
- Night: 20:00 to 6:00, settable, the pace rising and falling over an hour.
- Told by a rumor ("at night they are quicker ...").

**Settled, 2026-11-05** (Nadi's answers):
- 4.20.2: `Radius=1200` by default for every noise - "die sind dumpf".
- 4.20.6: an escorted group stays in the city 7 game days, then leaves the
  game; an overrun camp comes back by a chance (a chance a game day once
  it lies empty, settable); camp people killed by walkers rise as walkers,
  the safeguard leaves them alone; at most `CampsAttackedAtOnce=1` near the
  player (settable), the others rolled in the background.
- 4.20.1: a disguised player is not grabbed; the disguise also holds before
  a siege's walkers.
- 4.20.5: at night the hordes move farther, the siege stays as it is.
- Order: 4.20.5, 4.20.1, 4.20.2, 4.20.6 - after the safeguard's raise at
  home (4.11b, "A") and Nadi's headset test.

**Still to settle when each is built:** the twelve camp places (Claude
proposes them off Oblivion.esm's roads, Nadi picks); the reward's amounts
(gold, fame points, the trader's goods); when an escort fails (the group
dead, or left behind farther than some distance); the chance a day a camp
comes back (proposed 10 percent).

### 4.21 Slow aggression - BUILT 2026-11-08, harness PASS, headset open

**Roused, then vanilla (2026-11-11, Nadi: "warum dann nicht einfach wieder
auf 100 setzen sobald der timer weg ist" - "bau").** Why: the fight sent
again every second (Keep, below) held walkers in fights they could not
reach - at Whitmond Farm a dozen targets each (Maeva, sheep, MOO creatures,
the player), the walkers out of their groups for good (4.11c's runs of
2026-11-11). The records' aggression must stay 0 - a record is shared by
every walker of its look - so it is raised per walker: `ModAV Aggression
100` on the reference (CS wiki SetActorValue: SetAV changes the base,
ModActorValue only the reference; kActorVal_Aggression 33, xOBSE). How
(AggressionLogic.h `NextRouse`, `Roused`; Rise.cpp StepSlowAggression,
CarryRouse; AggressionLogicTest TestRouse):
- A walker whose time near someone ran out is roused: its Aggression 100
  above its record's, one StartCombat on that one; from then on the
  engine's own rule picks its fights (any undead's - the player, people,
  animals). Keep is no longer acted on.
- A roused walker with nobody living near (within SlowAggressionUnits; not
  walkers, not the disguised player) for `SlowAggressionCalmSeconds` (10)
  calms: ModAV back by what it is above its record. With the rule off or
  the player disguised (4.20.1) every roused walker calms at once.
- Roused is read off the game (the walker's Aggression above its
  record's, 50 or more), so a save keeps it and nothing goes into the
  co-save; a roused walker loaded calms by the same rule.
- INI `[Zombies] SlowAggressionCalmSeconds=10`; LINK 173 (version 32,
  header "Walkers - slow aggression, calm again").
- Log: "Slow aggression: walker X's time near Y ran out", "walker X roused
  - the engine decides whom it fights (...)", "walker X calms down (...)".
Harness `harness/slow-aggression-rouse.txt` via
`tools/slow-aggression-rouse.sh`, 2026-11-11 18:55 (Linux, headless): roused
- "walker FF000FCA roused ... its time near 00000014 ran out; Aggression
above its record 0, now 100; ModAV Aggression 100 answered 1" - so ModAV
works per creature reference. Not seen then: the calming - corrected
2026-11-12: not the walker following the player, but the place (-29,5, a
deer herd's spawn points): others stayed near and one killed the walker.
Run 2026-11-12 09:58 at -29,-11 (no Oblivion.esm spawn or placed actor
within three cells; the player 3000 east): "walker FF000F9D roused (its
time near FF000F8D ran out ...)", then "calms down (nobody near for 10 s;
Aggression above its record 100, now 0; ModAV Aggression -100 answered 1)"
- the calm PASS. Three of MOO's people (2301CAEE x3, load index 0x23)
stood there and roused it first, not the player; the checker lists them,
a note now, not a failure. Farm A/B
of 18:57/19:00 with the rouse: the groups now form and walk (leaders 1000
to 2500 units in a run, against 0 and groups broken up before) - the stuck
farm was the fight resent every second. Gate run 4 (19:02): PASS, every
check - "group 05 waits out of the player's grid ... the player 8970 units
from the gate", "group 05 moves up to hold ... 1413 units". Not measured: that ModAV
Aggression works on a creature reference as the CS wiki says for actors,
and that the base GetBaseActorValue (0x005F1910) reads is the record the
plugin holds at 0.

What (Nadi, 2026-11-08): walkers are slow to anger. A walker turns on the
player or anyone else only once that one has stayed near it for 5 s. Default
on, 5 s; near is some 14 m (Nadi: "ja ok 14m"), no line of sight ("nein").

How (AggressionLogic.h, Rise.cpp StepSlowAggression):
- The walker records' aggression goes to 0 while the rule is on and back to
  each record's own (100, read at the start) when it is off. No walker starts
  a fight of its own.
- The records are kept every frame (Rise.cpp KeepWalkerRecords, together with
  the way back of NoLevelingXP, 7a).
- Every 0.5 s the plugin times every pair of a walker in high process and a
  living one within the range: the player, and every person or creature on
  the high process list that is no walker. Each new pair draws its time at
  random in the level's range (Nadi, 2026-11-08: "statt 5s machen wir ein
  random aus x,y"; then "bei weniger ist der rand 10,15s bei medium 5,10, bei
  hoch 1,5s default medium"): Low 10 to 15 s, Medium 5 to 10 s (the
  default), High 1 to 5 s, Custom the own fewest to most seconds (if the
  fewest is set above the most, the two are taken in order). A pair that
  stays near for its time in one go gets `StartCombat <that one>` on the walker,
  then again every second while it stays near (kNearKeepSeconds). Out of
  range, its clock starts anew.
- A walker that is hit fights back: that is the engine's own rule.

Settings: `[Zombies] SlowAggression=1`, `SlowAggressionLevel=Medium`
(Low/Medium/High/Custom or 0..3), `SlowAggressionSecondsMin=5`,
`SlowAggressionSecondsMax=10` (Custom only), `SlowAggressionUnits=1000`; in
LINK as the last five (version 25, header "Walkers - slow aggression", the
level a slider 0..3). The old `SlowAggressionSeconds` is not read any more.
Unit flows in AggressionLogicTest (the draw: NeededSeconds, the roll kept per
pair). The log line "turns on" names the drawn time.

Harness `harness/slow-aggression.txt`, 2026-11-08 (Linux, headless):
- 10:29 - one StartCombat only: the walker swung once ("hit on the player -
  no grab") and walked off, 429 units away at the end. Measured: with
  Aggression 0 the engine drops the fight StartCombat gave. Hence the fight
  sent again every second.
- 10:31 PASS - "Slow aggression: on - the 64 walker records' aggression 0",
  "walker FF000F85 turns on 00000014, near it 5.0 s", then it held the
  player's arm and bit three times until the player went down.
- `StartCombat` answers 0 through RunScriptLine2 although the fight starts
  (the hit and the grab follow): the answer is no success flag here.
- The test cannot show the calm phase: the walker came back to the player by
  itself while it stood 1500 units off (46 units at the next mark) - it walks
  to him without fighting. Not measured: the 5 s with no fight before.
- Levels (d1b0fe6), PASS: "for 5.0 to 10.0 s (level 1, drawn per pair)",
  "turns on 00000014, near it 6.6 s (drawn 6.5 of 5.0..10.0)", then the
  walker held the player's right arm. Only Medium run; Low, High and Custom
  only in the unit flows.
Not measured: how guards, sieges and camps feel with calm walkers (the
guards are sent by StartCombat already, 4.11b); the headset.

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
| `night-pace.txt` | the nights (4.20.5) through `tools/night-harness.sh`: noon, 1:00, noon - the records' pace x2 and back, the counts at 150 % and back, a risen walker's units/s at night and by day - PASS 2026-11-05 14:12 | the plugin, the ESP active |
| `camp-defend.txt` | the survivors' camps (4.20.6) through `tools/camp-harness.sh` (TestCamp=7, six walkers; CAMP_ENDS=1 adds DieOnlyBeheaded=0): the camps set up, camp 07 attacked, its walkers out and led, refugees dead and rising, walkers ended, defended - PASS 2026-11-05 14:56 and 15:00 | the plugin, the ESP active |
| `noise-draws.txt` | the noise (4.20.2) through `tools/noise-harness.sh`: walkers attacking the player at Tamriel -29,5 - blows and spells heard, groups 1000-1300 units off drawn, at the spot and searching - PASS 2026-11-05 14:20 | the plugin, the ESP active |
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

- **FIX BUILT 2026-11-10, Windows open - no refugee ever asked to be led** (Nadi, 2026-11-10:
  found a camp, could start the escort nowhere). Cause, read: the escort's greeting,
  its topic "Safety" and its lines sat in the LINK quest, priority 0; the game says the
  first GREETING info whose conditions hold, taking the quests by priority (CS wiki Quest
  Data Tab; Dialogue Tutorial, "NPCs won't say my stuff"), so every generic greeting went
  first and the topic was never added. The harness had set the ask global directly and
  never spoke. Fix: the escort's dialogue in a quest of its own, WalkersEscortQuest
  (0xA46), start game enabled, priority 50 (the convention's miscellaneous quests; below
  guilds 60, the main quest 85, crime 90); tools/esp-write.py reads it back. Not seen in
  the game yet. The same priority 0 holds for the sieges', the night's and the camps'
  rumors (INFOGENERAL) - not measured whether they are ever said.
- **Rise stand watch, 2026-11-10:** every raise (a walker up again, the swap's walker, the
  siege pool's, the safeguard's) is watched; one still within 16 units of its spot 20 s
  after the raise logs "Risen: ... still on its spot" with its health, fatigue, 3D,
  process, package and flags (RiseLogic.h StandWatch, RiseLogicTest).
- **FIX BUILT 2026-11-10, harness and Windows open - the game crashed in a horde follow
  package** (OBNG's reading of two Windows crash logs of 2026-11-10, 20:40 and 21:08:
  EXCEPTION_ACCESS_VIOLATION at 0x0064F67C, call trace 0x0064F650 <- 0x0063A993 <-
  0x006587CE, the current package 37000816 / 37000817, WalkersHordeFollow00/01). Cause,
  read: the plugin wrote the leader's raw reference pointer into the group's follow
  package (src/Horde.cpp AimFollow, as xOBSE's SetPackageTarget does) and never took it
  back - once the leader was gone (cleaned up, unloaded, another load) every follower ran
  a package pointing into freed memory, and 0x0064F650 read the reference's flags there.
  A null target is no way out: 0x0064F840 and 0x0063D0C0 call through the target
  without a check (REA 2026-11-10). Fix: each frame a package whose leader is no longer
  the reference the game keeps under his ID, or is dead, disabled or unloaded, is aimed
  at its group's own marker (persistent, never freed); every load and new game aims all
  sixteen there; an aim at a leader that does not hold is refused and tried again
  (HordeLogic.h FollowTargetHolds, HordeLogicTest "The follow package's target").
  `tools/horde-leader-gone.sh` 2026-11-11 07:58 PASS: groups formed, killall, every dead
  leader's package aimed at its marker, none failed, the walkers up again.
  Not covered: a leader freed within the very frame he is still alive, enabled and
  loaded - the engine frees no such reference as far as read, not measured.
  Separately read by OBNG, cause unknown: the form heap 91 % and 99 % full at the two
  crashes against 33-65 % in every older crash log.
- **OPEN 2026-11-10 (Nadi, Windows, the siege save among others) - risen walkers lie on
  the ground and twitch** ("walker manchmal am boden wiederbelebt ... zucken aber auch
  nicht aufstehen"). Not read: which raise it was (the rise again of DieOnlyBeheaded,
  RaiseAsSelf; the siege pool's console Resurrect; the safeguard), which walkers, what
  their fatigue and process were. The session's log is gone - only the last two were
  kept; since 2026-11-10 the last 20 sessions stay in OblivionWalkers-logs. Next: the
  log of the next sighting, or a harness run on the siege save killing walkers that are
  not beheaded and watching each risen one off its spot.
  **Harness 2026-11-11 07:54 PASS, not reproduced** (`tools/rise-twitch.sh`, the spawn
  replacement off for the run): killed standing, in flight (push 20), lying (push 5),
  unloaded while it got up - all four raised and walked off within 20 s; killed by an
  NPC (Oblivion.esm's 0002C0B9 as the killer) ended and stayed down. One earlier run
  (2026-11-10 23:13, push 30, killer none) logged FF00114F "still on its spot" 20 s after
  its raise - alive, health and fatigue full, high process, the horde's travel package
  (type 6), not in combat; whether it lay or stood is not known (the knocked state is
  logged since). Next: the log of Nadi's next sighting.
  **Cause, derived 2026-11-11, fix built** (Nadi: "die walker wollten wiederbelebt werden
  aber blieben dann am boden liegen und machten sounds und drehten sich"): alive - sounds,
  turning - but the get-up broken off. The CS wiki's Resurrect: "Scripting or AI packages
  may interrupt the animation". The horde handed a just-raised walker its package within
  the second (it counted as fit: alive, high process, 3D, outdoors), the slow aggression
  its StartCombat once near someone. Fix: a walker getting up - the engine's knocked state
  not 0, or raised within 6 s (RiseLogic.h GettingUp, kGetUpGraceMs) - is busy for the
  horde and skipped by the slow aggression. The stand watch now watches the full 20 s and
  logs "not up" when it never moved or is still knocked (a lying body turned by the AI
  moves too). Harness 2026-11-11 09:10 with the fix: all four raised and up after 20 s
  (moved, knocked state 0), the NPC-killed walker ended. The bug was never reproduced in
  the harness before the fix, so this run shows no regression, not that the fix cures
  it - that is Nadi's next Windows run.

- **OPEN 2026-10-28 night (Nadi, Windows, for the next day) - some walkers are peaceful**
  ("zombies waren teilweise friedlich zu mir auch zu guards"): some walkers neither attack
  the player nor guards. Not read yet: which ones (spawned, risen or replaced), their
  faction, aggression and confidence as the engine has them, and whether a load or a
  disposition change (OBVR's slap or middle finger, a yield) made them peaceful. First
  step: the plugin's log at the next sighting plus `getav aggression` / `getfactionrank`
  on one of them in the console; then a harness or unit flow for the rule that decides
  their hostility.
  **Suspect, 2026-11-04** (Nadi: Oblivion Combat Extender puts attacker and player in one
  faction when the player goes down; he removed the mod): OblivionCombatExtender.esp
  carries the factions xLDDFaction, xLDDDuelFaction, xLDDPlayerFaction and a script
  calling xLDDRemoveNPCFromFaction over GetNthFaction (strings read off the ESP) - it
  does change factions. Not measured on a walker. On 2026-11-04 the mod is still in the
  Windows Data folder and in Windows' Plugins.txt (line 55), not on Linux. A faction
  given to a reference stays in the save after the mod is gone, so walkers made peaceful
  in an old save may stay so. Next: Nadi's word on whether it still happens without the
  mod; if it does, the console check above.
  **Cause found, 2026-11-08** (Nadi, Windows run of 2026-11-07: "teilweise passiv", and
  they attacked no townsfolk). Measured:
  - The walkers' faction is right. CreatureZombie1..4, which every walker copies, are in
    UndeadFaction (0009DB1F) and NecromancerDungeon (0004B90C), aggression 100 and
    confidence 100 (read off Oblivion.esm, AIDT and SNAM).
  - NoLevelingXP sets a dead walker's Confidence and Aggression to 0 in OnDeath
    (src/Rise.cpp, kLevelingHoldLines), because Ultimate Leveling is loaded on Windows
    (log line 28).
  - With DieOnlyBeheaded=1 the same walker gets up again. Nothing sets the two values
    back. In the log of 2026-11-07 23:45, 103 walkers killed by the player rose again,
    for example FF00D860 and FF00D86B ("Confidence and Aggression to 0", then "down - it
    gets up again").
  Derived, not measured: a risen walker with Aggression 0 starts no fight, with the
  player or with townsfolk. Not measured: that the values survive the engine's
  Resurrect (`getav aggression` on one such walker settles it).
  Same flaw for Oblivion XP (kObXpHoldLines: its eight attributes, Health among them, set
  to 0). Not loaded on Windows.
  OblivionCombatExtender.esp is still in Windows' Data folder and Plugins.txt (line 55,
  2026-11-08), so the first suspect stays open beside this one.
  **Cause measured 2026-11-08** (harness/leveling-rise-again.txt, Linux, Ultimate
  Leveling loaded, SlowAggression=0): before the kill "base Aggression 100, Confidence 100
  (its record 100, 100)", after it "base Aggression 0, Confidence 0 (its record 0, 0)". The
  hold's SetAV writes the creature RECORD, which every walker of that base shares. So one
  kill made every walker of that look passive, risen or not - worse than first thought.
  **Fix, harness PASS 2026-11-08 10:27:** the plugin reads the walker records at the main
  menu, before any save, and gives each record its held values back every frame
  (Rise.cpp KeepWalkerRecords, LevelingLogic.h RecordValueBack: the last value above 0
  it saw, else the one at the start; a value changed but not to 0, like the night's speed,
  is left). Ultimate Leveling has counted the kill by then: "XP 0 before, 0 after". Log:
  "Leveling: walker record 26000801 Aggression 0 - back to 100", the same for Confidence;
  the walker got up and held and bit the player again. A first per-reference fix (SetAV
  back on the live walker, 1c945f0) never fired, because the record itself read 0; it is
  gone. Not measured: whether a save stores the zeroed record (Nadi's Windows save of
  2026-11-07 may hold zeroed walker records; the fix writes them back either way).

- **Fix BUILT 2026-11-08 - a quest's creature became a walker** (Nadi, Windows: Revenge
  Served Cold, "alle goblins durch walker ersetzt"). Measured in Oblivion.esm: SQ03 needs
  only SQ03JadeNecklace in the player's inventory (SQ03JadeNecklaceScript, OnAdd player
  sets stage 10 or 30). The necklace (CLOT, quest item flag 0x400) is carried by
  SQ03CreatureGoblin2..4, the entries of SQ03LL1GoblinBossLvl100, which SQ3BossRef places
  in Exhausted Mine. Those goblins have no script, so the Goblins category turned them
  into walkers and the necklace was gone. In Oblivion.esm 25 creatures without a script of
  their own carry an item with the quest item flag (SQ01's ogre, SQ03's goblins, SQ09's
  slaughterfish, MG07's skeleton, MS06's captain, MS16's ogre, SE03's grummite shamans, the
  unicorn). Rule now (ReplaceLogic.h CreatureFacts.questItem, Replace.cpp
  CarriesQuestItem): a creature whose inventory holds an item with the quest item flag
  stays itself, whatever its category; only a [ReplaceExceptions] line of 1 makes it a
  walker. Its INI note reads "carries a quest item". Unit flows in ReplaceLogicTest and
  CategoryLogicTest.
  Not covered: SQ06's bears carry SQ06BearFang, a scripted MISC without the quest item
  flag, so they still become walkers; a quest that counts kills of a base. A creature
  already spawned as a walker in a cell stays one until the cell resets. Not run in the
  game yet.
- **OPEN - Quest audit, measured 2026-11-08** (Nadi: "ja mach mal A erstmal alles
  messen"). `tools/quest-audit.py <Data> <Plugins.txt>` reads the load order (39
  plugins, Linux) and writes `artifacts/quest-audit/summary.md`. "Named by a quest" =
  in the quest's script, stage results, targets, conditions, or its dialogue. "Replaced" =
  every leveled-list base the two keep rules (scripted, quest item) leave - an upper
  bound: the categories are not applied. Of 4197 leveled-list bases 299 are kept.
  - Gap 1, a needed item without the flag: only SQ06 (SQ06BearFang on its six bear
    bases) has an item of its own. 35 more quests name ordinary items (torches,
    clothes, ingredients such as MinotaurHorn, DaedraHeart) that replaced bases carry;
    those come from elsewhere too - not a block, but fewer of them in the world.
  - Gap 2, a placed leveled list named by a quest: 14 quests; 10 with replaced bases
    in the list - Dark07Medicine, FGC03Thieves, MG01Destruct, MG14Plot, MS16A, MS18,
    MS49, SE03A, SQ06, TG09Arrow. Safe already (all kept): SQ03, SQ09, ND10. Plus 18
    object scripts (FanacasBossREF, MG08, MG10 summons, MOO's placers) and 79
    persistent placed lists named by nothing.
  - Gap 3, a replaced base or list named by a quest (GetDeadCount and the like): 3
    quests - MG01Destruct (its zombie bases), NPCAdventurer (conditions), SE14 (its
    attack lists). 110 more names are mod scripts (MOO, Basic Primary Needs) that read
    the lists - not quests.
  - Not measured: whether a quest really breaks when its named one is a walker. A kill
    by reference (GetDead) likely still counts - but a walker that rises again, an
    item the quest needs, or a line to speak are lost. Not seen: names built at run
    time, leveled item lists in inventories.
  - Measured in memory 2026-11-08 (`harness/quest-probe.txt`, Linux, [Debug]
    QuestProbe=1, Replace.cpp ProbeQuests): at the main menu, before any load, the
    quests' scripts (TESQuest +0x1C), their stage result scripts and their targets
    (+0x48, target +0x0C) are in memory with every name resolved - placed references
    with their base (SQ06's 18BA01 base 18AE3C LVLC, MS18's nine wave refs, SE03A's
    three knights, TG09Arrow's target 9836C base 3EE6B). A stage item holds its result
    script inline at +0x0C (the Script vtable 00A49DA4 there, in every item of seven
    quests); a Script's names at +0x40 {var, next}, a name's form at +0x08. Dialogue
    result scripts are read from the file on demand (xOBSE GameForms.h TESTopicInfo
    unk34) - not in memory; only NPCAdventurer would need them.
  - What a keep rule would keep (the audit, quest scripts, stage results, targets):
    20 lists named directly, 43 with the lists nested in them. Among them generic
    lists used across the world: LL1MythicEnemy100 (FGC03's respawn), LL1ConjurerDaeda75
    (TG09's target) directly; LL1BanditMelee100, LL1Minotaur100, LL0WraithFaded100 and
    Shivering Isles creature lists only nested.
  - **BUILT 2026-11-08 - option 1** (Nadi: "1) default an"): `[Replace] KeepQuestLists=1`
    (LINK version 26, last setting, header "Spawns - quests", redoes the spawns when
    switched). At the first pass (the main menu) Replace.cpp CollectQuestNames reads every
    quest's script, stage result scripts and targets: a leveled list named - itself or
    through a placed reference - is a quest's, and every entry in it stays its own
    (ReplaceInList, even against an exception of 1); a creature or person named is
    CreatureKind::QuestNamed, kept like a quest item carrier (an exception of 1 still makes
    it a walker). Lists nested in a quest's list are not kept (option 2 would). No extra
    walker entries (ExtraPercent) in a quest's list. Log: "Quests: ... leveled lists a quest
    needs", "Spawns: KeepQuestLists=1 - N of the lists are a quest's; E entries ... stay
    their own". Unit flows in CategoryLogicTest TestQuestNames, ReplaceLogicTest, LinkLogicTest.
    Harness `harness/quest-keep.txt`. Not covered: dialogue result scripts (NPCAdventurer),
    lists nested in a quest's list (Dark07's bandits, SE14's, the minotaur ones), spawns
    already made walkers in a save until their cell resets.
    Harness PASS 2026-11-08 (Linux): 605 quests, 20 lists a quest needs (the audit's
    20), MG01's seven zombies the only leveled-list bases kept by name; 52 entries kept
    at the main menu, 22 at the pass after the load (not explained). Not played
    through: a quest with its foes; the Windows run; the headset.
  - The nested lists checked 2026-11-08 (Nadi: "schau mal die quest lücken nach"; the
    audit plus the quests' script sources, SCTX in Oblivion.esm). Lists nested in a
    quest's list, walkers possible there: FGC03Thieves' LL1MythicEnemy100 (imps,
    minotaurs), TG09Arrow's LL1ConjurerDaeda75 (scamps), SE14's two attack lists (all
    their creatures and the bandits are nested). Dark07Medicine and MG14Plot have none
    that turn (an earlier guess named Dark07 - wrong). Read in the scripts: FGC03 enables
    FGC03EnemyRespawn at stage 100, after the quest is done; TG09 names its list's spot
    only as a target; SE14's attacks are the post-quest settlement raids (SE14Script:
    "this script will handle the post-quest activities"), counted by
    SE14AttackCountScript off the lists' template 0004682E, reset after 3 game days
    when not quelled. So none blocks a quest - derived from the scripts, not played.
    Open: whether a walker spawned from SE14's list gets the template's script (then
    its death counts). Not built further.

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

- **Built 2026-11-04, off by default: `[Rise] NoRiseInCities`** (Nadi: "bau die
  stadt regel, default off") - nobody killed in a city's streets (the
  worldspaces Oblivion.esm puts under Tamriel for the eight cities,
  IsCityWorldspace; not the houses) rises; the safeguard takes them as any
  victim of the walkers. harness/rise-no-city.txt 21:24 PASS: a bandit killed
  in Bravil's streets "stays down (in a city ...)", no rise. The earlier entry:
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
