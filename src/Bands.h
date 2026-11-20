#pragma once

// The war bands out of the open Oblivion gates in the game
// (docs/walkers-spec.md 4.22 steps 2 and 3, BandLogic.h): the 103 gates'
// flags read once a second, the far bands stepped by the game's clock, a
// band that comes near placed at the ESP's band marker (PlaceAtMe of the
// kind's list), its members found on the process lists and handed to the
// horde (Horde.h), which leads them to the city as it leads the walkers.

#include "BandLogic.h"
#include "Horde.h"
#include "KindLogic.h"

namespace walkers {

// The ESP's base (RiseLogic.h EspFormId), once a session.
void ConfigureBands(UInt32 espBase, LogFn log);

// Once a frame; the gates and the bands are looked at once a second.
void StepBandsInGame(float dt, const BandSettings& bands, const InvaderSettings& invaders, ConsoleFn console,
                     DeadFn dead, LogFn log);

// The open gates at the last look; -1 before the first.
int OpenGateCount();

// Whether the spawn replacement should be on now (the walkers' switch and
// [Daedra] EverywhereUntilGatesOpen, BandLogic.h ReplacementOn); with the
// gates not read yet, as if none were open. `invaders`: InvadersNow.
bool BandsWantReplacement(const BandSettings& bands, const InvaderSettings& invaders);

// Whether the Daedra come through the spawn replacement now
// (BandLogic.h DaedraEverywhere), the gates read as above.
bool DaedraEverywhereNow(const BandSettings& bands, const InvaderSettings& invaders);

// The band members placed this session, alive or not, for the horde's
// candidates (Horde.h); appended after `count` entries already in `out`.
UInt32 AppendBandMembers(HordeCandidate* out, UInt32 count, UInt32 most, DeadFn dead);

// Whether a reference is a band member of this session.
bool IsBandMember(UInt32 refId);

// A load or a new game: the far bands and the members are forgotten.
void ResetBands();

// The points a city's daily siege chance is up by (BandLogic.h): the bands
// that reached it lately.
UInt32 SiegePressureOf(UInt32 city);

// The co-save record 'BAND' (BandLogic.h BandImage): written on save, read
// on load after ResetBands; `resolve` xOBSE's ResolveRefID.
UInt32 BandsImage(UInt8* out, UInt32 capacity);
bool ReadBandsImage(const UInt8* in, UInt32 length, UInt32 version, bool (*resolve)(UInt32, UInt32&));
UInt32 FarBandCount();

// "Close the gate" (4.22a step 6): its co-save record 'GATQ'
// (GateQuestLogic.h), and the gate it names (-1 none).
UInt32 GateQuestImageNow(UInt8* out, UInt32 capacity);
void ReadGateQuestNow(const UInt8* in, UInt32 length);
int GateQuestGate();
UInt32 BandMemberCount();

}  // namespace walkers
