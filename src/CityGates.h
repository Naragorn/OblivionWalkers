#pragma once

// The city gates the hordes head for (docs/walkers-spec.md 4.11b): the
// persistent doors in the Tamriel worldspace whose teleport leads into a
// city's own worldspace, read from Oblivion.esm on 2026-10-26 (the XTEL of
// every Tamriel REFR, its destination's worldspace by the WRLD group; the
// Oblivion worlds of the main quest and Cheydinhal's gate, and Anvil's castle
// courtyard, left out). Positions are the doors' own, in Tamriel's units;
// the names after the nearest city map marker (TNAM 3) where one lies within
// a few hundred units, the Imperial City's after the district the door leads
// into; two doors have no marker that near and keep their IDs. The z and the
// spots outside are tools/city-gates.py's (2026-10-26), which prints these
// rows anew.

#include "HordeLogic.h"
#include "Types.h"

namespace walkers {

enum class City : UInt8 { Anvil, Bravil, Bruma, Cheydinhal, Chorrol, Leyawiin, Skingrad, ImperialCity, Count };

struct CityGate {
	City city;
	UInt32 door;  // the Tamriel-side door (Oblivion.esm)
	float x;      // the door's position
	float y;
	float z;
	float outX;   // where its way back out of the city puts one: in front of
	float outY;   // the gate, on the ground (the city-side door's XTEL) -
	float outZ;   // where the gate guards come out
	const char* name;
};

inline constexpr UInt32 kTamrielWorldspace = 0x0000003C;

inline constexpr CityGate kCityGates[] = {
	{City::Anvil, 0x00006FBC, -189811.7f, -27877.7f, 582.2f, -189826.8f, -27820.5f, 397.1f, "Anvil Main Gate"},
	{City::Anvil, 0x00006FBA, -186108.6f, -34829.1f, 522.2f, -186111.4f, -34878.2f, 319.6f, "Anvil Castle Gate"},
	{City::Anvil, 0x00006FB8, -194891.9f, -31963.8f, 526.0f, -194865.6f, -32052.1f, 325.4f, "Anvil Dock Gate"},
	{City::Bravil, 0x00005605, 66297.1f, -31629.2f, 883.9f, 66301.9f, -31526.4f, 633.6f, "Bravil"},
	{City::Bruma, 0x00003614, 27971.1f, 146703.8f, 25864.7f, 28038.5f, 146711.7f, 25625.6f, "Bruma East Gate"},
	{City::Bruma, 0x001771BB, 23618.3f, 151255.0f, 26897.0f, 23595.7f, 151292.1f, 26664.0f, "Bruma North Gate"},
	{City::Cheydinhal, 0x0000799C, 114617.8f, 91855.4f, 7349.7f, 114441.5f, 91858.6f, 7098.6f, "Cheydinhal West Gate"},
	{City::Cheydinhal, 0x00007A67, 123765.0f, 95473.4f, 7422.5f, 123884.8f, 95542.1f, 7179.7f, "Cheydinhal East Gate"},
	{City::Chorrol, 0x0000092F, -64638.5f, 91894.3f, 14414.4f, -64633.2f, 91818.7f, 14193.0f, "Chorrol South Gate"},
	{City::Chorrol, 0x0006A814, -66176.1f, 100349.4f, 14742.8f, -66168.1f, 100434.2f, 14532.8f, "Chorrol North Gate"},
	{City::Leyawiin, 0x0000887E, 97552.5f, -140901.0f, 232.0f, 97661.0f, -140874.8f, 40.0f, "Leyawiin gate (door 0000887E)"},
	{City::Leyawiin, 0x0000887F, 96666.9f, -139112.6f, 272.0f, 96689.3f, -138966.7f, 80.0f, "Leyawiin North East Gate"},
	{City::Leyawiin, 0x0001C356, 84895.7f, -139423.2f, 461.6f, 84806.2f, -139389.1f, 278.1f, "Leyawiin West Gate"},
	{City::Skingrad, 0x00001442, -64237.1f, 4792.8f, 6152.2f, -64149.8f, 4798.4f, 5856.0f, "Skingrad gate (door 00001442)"},
	{City::Skingrad, 0x00007DE6, -72129.3f, 3127.1f, 6084.8f, -72208.1f, 3109.4f, 5842.0f, "Skingrad West Gate"},
	{City::Skingrad, 0x00007DEA, -63008.3f, 7347.1f, 6848.9f, -62879.9f, 7364.7f, 6679.0f, "Skingrad East Gate"},
	{City::ImperialCity, 0x000176EE, 18765.6f, 63424.1f, 3552.2f, 18409.7f, 63409.3f, 3332.9f, "Imperial City Talos Plaza Gate"},
	{City::ImperialCity, 0x0001C2C2, 21120.1f, 50183.9f, 624.3f, 21091.3f, 50135.2f, 382.6f, "Imperial City Temple Gate"},
	{City::ImperialCity, 0x0001C2BF, 33671.4f, 54818.6f, 3550.7f, 33707.8f, 54751.7f, 3340.7f, "Imperial City Arboretum Gate"},
	{City::ImperialCity, 0x0001C2C1, 33676.3f, 72036.6f, 3551.2f, 33746.7f, 72165.6f, 3346.3f, "Imperial City Market Gate"},
	{City::ImperialCity, 0x0008698B, 36147.8f, 76351.8f, 3389.6f, 36125.2f, 76288.7f, 3172.0f, "Imperial City Prison Gate"},
	{City::ImperialCity, 0x0001BC3D, 36270.2f, 50240.6f, 3391.8f, 36242.9f, 50292.2f, 3174.0f, "Imperial City Arcane University Gate"},
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
