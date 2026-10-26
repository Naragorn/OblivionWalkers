#pragma once

// The city gates the hordes head for (docs/walkers-spec.md 4.11b): the
// persistent doors in the Tamriel worldspace whose teleport leads into a
// city's own worldspace, read from Oblivion.esm on 2026-10-26 (the XTEL of
// every Tamriel REFR, its destination's worldspace by the WRLD group; the
// Oblivion worlds of the main quest and Cheydinhal's gate, and Anvil's castle
// courtyard, left out). Positions are the doors' own, in Tamriel's units;
// the names after the nearest city map marker (TNAM 3) where one lies within
// a few hundred units, the Imperial City's after the district the door leads
// into; two doors have no marker that near and keep their IDs.

#include "HordeLogic.h"
#include "Types.h"

namespace walkers {

enum class City : UInt8 { Anvil, Bravil, Bruma, Cheydinhal, Chorrol, Leyawiin, Skingrad, ImperialCity, Count };

struct CityGate {
	City city;
	UInt32 door;  // the Tamriel-side door (Oblivion.esm)
	float x;
	float y;
	const char* name;
};

inline constexpr UInt32 kTamrielWorldspace = 0x0000003C;

inline constexpr CityGate kCityGates[] = {
	{City::Anvil, 0x00006FBC, -189812.0f, -27878.0f, "Anvil Main Gate"},
	{City::Anvil, 0x00006FBA, -186109.0f, -34829.0f, "Anvil Castle Gate"},
	{City::Anvil, 0x00006FB8, -194892.0f, -31964.0f, "Anvil Dock Gate"},
	{City::Bravil, 0x00005605, 66297.0f, -31629.0f, "Bravil"},
	{City::Bruma, 0x00003614, 27971.0f, 146704.0f, "Bruma East Gate"},
	{City::Bruma, 0x001771BB, 23618.0f, 151255.0f, "Bruma North Gate"},
	{City::Cheydinhal, 0x0000799C, 114618.0f, 91855.0f, "Cheydinhal West Gate"},
	{City::Cheydinhal, 0x00007A67, 123765.0f, 95473.0f, "Cheydinhal East Gate"},
	{City::Chorrol, 0x0000092F, -64638.0f, 91894.0f, "Chorrol South Gate"},
	{City::Chorrol, 0x0006A814, -66176.0f, 100349.0f, "Chorrol North Gate"},
	{City::Leyawiin, 0x0000887E, 97553.0f, -140901.0f, "Leyawiin gate (door 0000887E)"},
	{City::Leyawiin, 0x0000887F, 96667.0f, -139113.0f, "Leyawiin North East Gate"},
	{City::Leyawiin, 0x0001C356, 84896.0f, -139423.0f, "Leyawiin West Gate"},
	{City::Skingrad, 0x00001442, -64237.0f, 4793.0f, "Skingrad gate (door 00001442)"},
	{City::Skingrad, 0x00007DE6, -72129.0f, 3127.0f, "Skingrad West Gate"},
	{City::Skingrad, 0x00007DEA, -63008.0f, 7347.0f, "Skingrad East Gate"},
	{City::ImperialCity, 0x000176EE, 18766.0f, 63424.0f, "Imperial City Talos Plaza Gate"},
	{City::ImperialCity, 0x0001C2C2, 21120.0f, 50184.0f, "Imperial City Temple Gate"},
	{City::ImperialCity, 0x0001C2BF, 33671.0f, 54819.0f, "Imperial City Arboretum Gate"},
	{City::ImperialCity, 0x0001C2C1, 33676.0f, 72037.0f, "Imperial City Market Gate"},
	{City::ImperialCity, 0x0008698B, 36148.0f, 76352.0f, "Imperial City Prison Gate"},
	{City::ImperialCity, 0x0001BC3D, 36270.0f, 50241.0f, "Imperial City Arcane University Gate"},
};
inline constexpr UInt32 kCityGateCount = sizeof(kCityGates) / sizeof(kCityGates[0]);

// The same as the horde's gates (positions only), in the table's order.
inline const HordeGate* HordeGates() {
	static HordeGate gates[kCityGateCount];
	static bool filled = false;
	if (!filled) {
		for (UInt32 i = 0; i < kCityGateCount; ++i) {
			gates[i].x = kCityGates[i].x;
			gates[i].y = kCityGates[i].y;
		}
		filled = true;
	}
	return gates;
}

}  // namespace walkers
