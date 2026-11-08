#pragma once

// The walkers' news in the game (docs/walkers-spec.md 4.9): the broadsheets
// of the inns, taverns and the Courier's office (CourierRefs.h) carry the
// edition NewsLogic.h chooses - their reference's base set to that edition's
// book of the ESP while their cell is loaded. The gate attacks go into the
// co-save through Rise.cpp.

#include "NewsLogic.h"
#include "Rise.h"

namespace walkers {

// The settings and the ESP's load index, once a session (the editions are
// the ESP's books). An index above 0xFF: no news.
void ConfigureNews(const NewsSettings& settings, UInt32 espBase, LogFn log);

// A siege of `city` begun or lifted (Siege.cpp), now.
void NoteSiegeInNews(UInt32 city, bool begun);

// Walkers at `city`'s gate and its guards out (Horde.cpp), now.
void NoteGateAttack(UInt32 city);

// Every frame; the broadsheets are looked at once a second. `headingCity`:
// the city the hordes near the player head for, -1 none (Horde.h) - a
// sighting stays the news for HeadingHoldDays.
void StepNewsInGame(float dt, int headingCity, LogFn log);

// The game's clock in days (GameDaysPassed's days plus GameHour), -1 when
// its globals are not read.
float GameDaysNow();

// The attacks kept across saves (Rise.cpp's co-save callbacks).
// 4.20.6: a refugee camp saved (defended, or brought to a city) or overrun.
void NoteCampInNews(bool saved);
NewsState& CurrentNews();

}  // namespace walkers
