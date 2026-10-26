#pragma once

// Written by tools/courier-refs.py from Oblivion.esm - do not edit by hand.
// The Black Horse Courier broadsheets in the inns, taverns and the Courier's
// office that carry the walkers' news (docs/walkers-spec.md 4.9): each
// reference, its cell and the broadsheet it holds in Oblivion.esm.

#include "Types.h"

namespace walkers {

struct CourierRef {
	UInt32 ref;
	UInt32 cell;
	UInt32 sheet;  // the vanilla broadsheet (BOOK)
};

inline constexpr CourierRef kCourierRefs[] = {
	{0x00029D22, 0x00000D77, 0x000274EE},  // ChorrolTheOakandCrosierTavern, Broadsheet01Assassination
	{0x00029D23, 0x00000D77, 0x000274EE},  // ChorrolTheOakandCrosierTavern, Broadsheet01Assassination
	{0x0006BD48, 0x00000D77, 0x0006BD47},  // ChorrolTheOakandCrosierTavern, Broadsheet01Doomstones
	{0x0003FEC4, 0x0000C1F3, 0x000274EE},  // ICWaterfrontTheBloatedFloat, Broadsheet01Assassination
	{0x00082E12, 0x0002AEEA, 0x0007BEA0},  // ICTalosPlazaTheTiberSeptimHotel, Broadsheet01NightMotherRituals
	{0x000738E4, 0x0002C15E, 0x0006D6ED},  // ICMarketDistrictBlackHorseCourier, Broadsheet01AdamusPhillidaSlain
	{0x000738E5, 0x0002C15E, 0x000732B7},  // ICMarketDistrictBlackHorseCourier, Broadsheet01BaenlinDead
	{0x000738E6, 0x0002C15E, 0x0007BEA0},  // ICMarketDistrictBlackHorseCourier, Broadsheet01NightMotherRituals
	{0x000738E7, 0x0002C15E, 0x0006D6EE},  // ICMarketDistrictBlackHorseCourier, Broadsheet01GrayFoxManOrMyth
	{0x000738E8, 0x0002C15E, 0x00098682},  // ICMarketDistrictBlackHorseCourier, Broadsheet01BlackwoodCompany
	{0x000738E9, 0x0002C15E, 0x0006BD47},  // ICMarketDistrictBlackHorseCourier, Broadsheet01Doomstones
	{0x000738EC, 0x0002C15E, 0x000274EE},  // ICMarketDistrictBlackHorseCourier, Broadsheet01Assassination
	{0x000738ED, 0x0002C15E, 0x00098682},  // ICMarketDistrictBlackHorseCourier, Broadsheet01BlackwoodCompany
	{0x000738EE, 0x0002C15E, 0x0007BEA0},  // ICMarketDistrictBlackHorseCourier, Broadsheet01NightMotherRituals
	{0x000738EF, 0x0002C15E, 0x0006D6EE},  // ICMarketDistrictBlackHorseCourier, Broadsheet01GrayFoxManOrMyth
	{0x000738F0, 0x0002C15E, 0x0006D6EF},  // ICMarketDistrictBlackHorseCourier, Broadsheet01GrayFoxUnmasked
	{0x000738F1, 0x0002C15E, 0x00098682},  // ICMarketDistrictBlackHorseCourier, Broadsheet01BlackwoodCompany
	{0x000738F2, 0x0002C15E, 0x00098682},  // ICMarketDistrictBlackHorseCourier, Broadsheet01BlackwoodCompany
	{0x000738F3, 0x0002C15E, 0x00098682},  // ICMarketDistrictBlackHorseCourier, Broadsheet01BlackwoodCompany
	{0x000738F4, 0x0002C15E, 0x00098682},  // ICMarketDistrictBlackHorseCourier, Broadsheet01BlackwoodCompany
	{0x000738F5, 0x0002C15E, 0x0006BD47},  // ICMarketDistrictBlackHorseCourier, Broadsheet01Doomstones
	{0x000738F6, 0x0002C15E, 0x0006BD47},  // ICMarketDistrictBlackHorseCourier, Broadsheet01Doomstones
	{0x000738F7, 0x0002C15E, 0x0006BD47},  // ICMarketDistrictBlackHorseCourier, Broadsheet01Doomstones
	{0x0007C6C0, 0x0002C15E, 0x000274EE},  // ICMarketDistrictBlackHorseCourier, Broadsheet01Assassination
	{0x0007C6C1, 0x0002C15E, 0x0007BEA0},  // ICMarketDistrictBlackHorseCourier, Broadsheet01NightMotherRituals
	{0x000309E7, 0x0002C168, 0x000274EE},  // ICMarketDistrictTheFeedBag, Broadsheet01Assassination
	{0x000A22CA, 0x0002C1BE, 0x000274EE},  // ICElvenGardensLutherBroadsBoardingHouse, Broadsheet01Assassination
	{0x0000C4F8, 0x0002C7CC, 0x00098689},  // LeyawiinFiveClawsLodge, Broadsheet01NakedDinner
	{0x0000C4FA, 0x0002D245, 0x00098683},  // BravilLonelySuitorLodge, Broadsheet01RainofBurningDogs
	{0x0000C501, 0x0002FF55, 0x00098683},  // BravilSilverHomeonthewater, Broadsheet01RainofBurningDogs
	{0x0000C502, 0x0003053B, 0x00098689},  // LeyawiinThreeSistersInn, Broadsheet01NakedDinner
	{0x000910DB, 0x000308C9, 0x000274EE},  // AnvilTheFocsle, Broadsheet01Assassination
	{0x000564A5, 0x000308D1, 0x0006BD47},  // AnvilTheCountsArms, Broadsheet01Doomstones
	{0x000AE3A5, 0x000308D2, 0x0006D6EE},  // AnvilTheFlowingBowl, Broadsheet01GrayFoxManOrMyth
	{0x000540EE, 0x00030FCD, 0x000274EE},  // TheDrunkenDragonInn, Broadsheet01Assassination
	{0x00082E4D, 0x00030FCD, 0x0007BEA0},  // TheDrunkenDragonInn, Broadsheet01NightMotherRituals
	{0x000900D0, 0x00031347, 0x000274EE},  // GottshawInnTavern, Broadsheet01Assassination
	{0x000311C3, 0x00049032, 0x000274EE},  // ICMarketDistrictTheMerchantsInnUpstairs, Broadsheet01Assassination
	{0x000738F8, 0x0004B950, 0x0006D6EF},  // ICMarketDistrictBlackHorseCourierUpstairs, Broadsheet01GrayFoxUnmasked
	{0x000738F9, 0x0004B950, 0x0007BEA0},  // ICMarketDistrictBlackHorseCourierUpstairs, Broadsheet01NightMotherRituals
	{0x000738FA, 0x0004B950, 0x000274EE},  // ICMarketDistrictBlackHorseCourierUpstairs, Broadsheet01Assassination
	{0x000738FB, 0x0004B950, 0x000274EE},  // ICMarketDistrictBlackHorseCourierUpstairs, Broadsheet01Assassination
	{0x000738FC, 0x0004B950, 0x000274EE},  // ICMarketDistrictBlackHorseCourierUpstairs, Broadsheet01Assassination
	{0x000A68B0, 0x0004B950, 0x000274EE},  // ICMarketDistrictBlackHorseCourierUpstairs, Broadsheet01Assassination
	{0x000A68B1, 0x0004B950, 0x00066CD3},  // ICMarketDistrictBlackHorseCourierUpstairs, Broadsheet01CheydinhalHeirSaved
	{0x000A68B2, 0x0004B950, 0x000732B7},  // ICMarketDistrictBlackHorseCourierUpstairs, Broadsheet01BaenlinDead
	{0x000A68B3, 0x0004B950, 0x0006D6EF},  // ICMarketDistrictBlackHorseCourierUpstairs, Broadsheet01GrayFoxUnmasked
};
inline constexpr UInt32 kCourierRefCount = sizeof(kCourierRefs) / sizeof(kCourierRefs[0]);

}  // namespace walkers
