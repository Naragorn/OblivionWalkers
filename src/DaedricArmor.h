#pragma once

// Written by tools/daedric-armor.py off Oblivion.esm - do not edit; run it again.
// The pieces of Daedric armor (docs/walkers-spec.md 4.22 idea 7, the Daedric
// disguise): every ARMO whose model lies under Armor\Daedric\ - the plain,
// the enchanted, the Dremora's and the bound pieces - with its body slots
// (BMDT: 0x01 head, 0x04 upper body, 0x08 lower body, 0x10 hands, 0x20
// feet). Shields and the Daedric bracers are left out.

#include "Types.h"

namespace walkers {

struct DaedricPiece {
	UInt32 formId;  // Oblivion.esm's
	UInt8 slots;
};

inline constexpr DaedricPiece kDaedricPieces[] = {
	{0x0000C57D, 0x01},  // MQ14DremoraHelmet
	{0x0000C57F, 0x20},  // MQ14DremoraBoots
	{0x0000C580, 0x04},  // MQ14DremoraCuirass
	{0x0000C581, 0x08},  // MQ14DremoraGreaves
	{0x0000C582, 0x10},  // MQ14DaedricGauntlets
	{0x0002626A, 0x20},  // BoundBoots
	{0x0002626B, 0x04},  // BoundCuirass
	{0x0002626C, 0x10},  // BoundGauntlets
	{0x00026270, 0x08},  // BoundGreaves
	{0x00026272, 0x01},  // BoundHelmet
	{0x0002C212, 0x01},  // EnchDaedricHelmetDetectLife
	{0x0002C214, 0x10},  // EnchDaedricGauntletsDetectLife
	{0x0002C215, 0x20},  // EnchDaedricBootsFeather
	{0x0002C217, 0x10},  // EnchDaedricGauntletsFortStrength
	{0x0002C218, 0x04},  // EnchDaedricCuirassFortEndurance
	{0x0002C21A, 0x08},  // EnchDaedricGreavesFortFatigue
	{0x0002C21B, 0x04},  // EnchDaedricCuirassFortHealth
	{0x0002C21D, 0x10},  // EnchDaedricGauntletsFortArmorer
	{0x0002C21E, 0x20},  // EnchDaedricBootsFortAthletics
	{0x0002C221, 0x10},  // EnchDaedricGauntletsFortHandtoHand
	{0x0002C223, 0x04},  // EnchDaedricCuirassFortHeavyArmor
	{0x0002C224, 0x10},  // EnchDaedricGauntletsLight
	{0x0002C229, 0x08},  // EnchDaedricGreavesResistFire
	{0x0002C233, 0x10},  // EnchDaedricGauntletsResistFrost
	{0x0002C235, 0x01},  // EnchDaedricHelmetResistMagicka
	{0x0002C239, 0x08},  // EnchDaedricGreavesResistParalysis
	{0x0002C23B, 0x04},  // EnchDaedricCuirassResistPoison
	{0x0002C23F, 0x20},  // EnchDaedricBootsResistShock
	{0x0002C247, 0x08},  // EnchDaedricGreavesSpellAbsorption
	{0x0002C24E, 0x01},  // EnchDaedricHelmetWaterbreathing
	{0x00036357, 0x01},  // DaedricHelmet
	{0x00036358, 0x10},  // DaedricGauntlets
	{0x00036359, 0x20},  // DaedricBoots
	{0x0003635A, 0x08},  // DaedricGreaves
	{0x0003635B, 0x04},  // DaedricCuirass
	{0x0003E9AE, 0x08},  // Dremora2KynvalGreaves
	{0x0003E9AF, 0x20},  // Dremora3KynreeveBoots
	{0x0003E9B0, 0x04},  // Dremora3KynreeveCuirass
	{0x0003E9B1, 0x08},  // Dremora3KynreeveGreaves
	{0x0003E9B3, 0x20},  // Dremora4KynmarcherBoots
	{0x0003E9B4, 0x04},  // Dremora4KynmarcherCuirass
	{0x0003E9B5, 0x08},  // Dremora4KynmarcherGreaves
	{0x0003E9B6, 0x20},  // Dremora5MarkynazBoots
	{0x0003E9B7, 0x04},  // Dremora5MarkynazCuirass
	{0x0003E9B8, 0x08},  // Dremora5MarkynazGreaves
	{0x0003E9B9, 0x20},  // Dremora0ChurlBoots
	{0x0003E9BA, 0x20},  // Dremora1CaitiffBoots
	{0x0003E9BB, 0x20},  // Dremora2KynvalBoots
	{0x0003E9BC, 0x08},  // Dremora0ChurlGreaves
	{0x0003E9BD, 0x04},  // Dremora0ChurlCuirass
	{0x0003E9BE, 0x04},  // Dremora1CaitiffCuirass
	{0x0003E9BF, 0x08},  // Dremora1CaitiffGreaves
	{0x0003E9C1, 0x04},  // Dremora2KynvalCuirass
	{0x00045DF3, 0x20},  // SEEnchDaedricBootsResistShock
	{0x00045E15, 0x20},  // SEEnchDaedricBootsFeather
	{0x00045E18, 0x10},  // SEEnchDaedricGauntletsSpellAbsorption
	{0x000733F4, 0x01},  // ArenaDaedricHelmet
	{0x0008B882, 0x01},  // Dremora1CaitiffHelmet
	{0x0008B888, 0x01},  // Dremora2KynvalHelmet
	{0x0008B88A, 0x01},  // Dremora3KynreeveHelmet
	{0x0008B88C, 0x01},  // Dremora4KynmarcherHelmet
	{0x0008B88D, 0x01},  // Dremora5MarkynazHelmet
};
inline constexpr UInt32 kDaedricPieceCount = sizeof(kDaedricPieces) / sizeof(kDaedricPieces[0]);

}  // namespace walkers
