#pragma once

// Written by tools/oblivion-gates.py off Oblivion.esm - do not edit; run it again.
// The Oblivion gates standing in Tamriel (docs/walkers-spec.md 4.22, step 3):
// 103 doors - 90 random (OblivionGateRandomScript), 6 of the cities
// (MQ11OblivionGateScript), 7 the quests' own. Their Oblivion.esm IDs (load index 0).

#include "Types.h"

namespace walkers {

enum class GateKind : UInt8 { Random, City, Quest };

struct OblivionGateRef {
	UInt32 door;    // the door reference
	UInt32 parent;  // its enable parent, 0 none
	float x, y, z;
	GateKind kind;
	bool initiallyDisabled;
};

inline constexpr OblivionGateRef kOblivionGates[] = {
	{0x0001E8A3, 0x00091229, -144799.0f, -16499.0f, 11381.0f, GateKind::Quest, false},  // MS48OblivionGate
	{0x00026EBA, 0x000613C3, 30495.0f, 142151.0f, 25440.0f, GateKind::Quest, false},  // MQ10OblivionArchGate01
	{0x00026EBE, 0x0018AE1B, 71021.0f, -26094.0f, 1443.0f, GateKind::City, false},  // MQ11BravilOblivionGate
	{0x00026EBF, 0x0018D452, -202755.0f, -22498.0f, 2039.0f, GateKind::City, false},  // MQ11AnvilOblivionGate
	{0x00026F12, 0x000613C4, 107696.0f, 96064.0f, 7840.0f, GateKind::City, false},  // MS13OblivionGate
	{0x00026F13, 0x0018AD7C, 104617.0f, -137433.0f, 23.0f, GateKind::City, false},  // MQ11LeyawinOblivionGate
	{0x000270F7, 0x00188770, -62943.0f, 84015.0f, 14558.0f, GateKind::City, true},  // MQ11ChorrolOblivionGate
	{0x000270F8, 0x00064583, -50863.0f, 2395.0f, 7920.0f, GateKind::City, false},  // MQ11SkingradOblivionGate
	{0x0002756E, 0x0002756D, 37608.0f, 155650.0f, 24804.0f, GateKind::Quest, true},  // MQ13GreatGate
	{0x0002756F, 0x0002756D, 37983.0f, 156036.0f, 24565.0f, GateKind::Quest, true},  // MQ13OblivionGate3
	{0x00027571, 0x0002756D, 36075.0f, 155624.0f, 24473.0f, GateKind::Quest, true},  // MQ13OblivionGate2
	{0x00027574, 0x0002756D, 37433.0f, 154205.0f, 24497.0f, GateKind::Quest, true},  // MQ13OblivionGate1
	{0x00051262, 0x00051281, -18051.0f, -22607.0f, 2292.0f, GateKind::Random, false},  // OblivionGatetoOblivion
	{0x0005160D, 0x0005160E, 22506.0f, -14470.0f, 4276.0f, GateKind::Random, false},  // OblivionGatetoOblivion
	{0x000517B5, 0x0005214A, -36001.0f, -9943.0f, 4924.0f, GateKind::Random, false},  // OblivionGatetoOblivion
	{0x000521A8, 0x00053442, -3285.0f, -15646.0f, 1629.0f, GateKind::Random, false},  // OblivionGatetoOblivion
	{0x0005403E, 0x00097C88, -186552.0f, 14954.0f, 4675.0f, GateKind::Quest, true},  // MS94OblivionGate
	{0x00055D87, 0x00055D88, -39688.0f, 1589.0f, 6148.0f, GateKind::Random, false},  // OblivionGatetoOblivion
	{0x00057038, 0x000572E6, -54347.0f, 14582.0f, 5846.0f, GateKind::Random, false},  // OblivionGatetoOblivion
	{0x000572EE, 0x0005737D, -70313.0f, 28188.0f, 7060.0f, GateKind::Random, false},  // OblivionGatetoOblivion
	{0x00057388, 0x00057F44, -84132.0f, 17762.0f, 5821.0f, GateKind::Random, false},  // OblivionGatetoOblivion
	{0x0005A376, 0x0005A3ED, -90053.0f, 4507.0f, 7258.0f, GateKind::Random, false},  // OblivionGatetoOblivion
	{0x0005A68E, 0x0005A6AC, -87321.0f, -5445.0f, 6729.0f, GateKind::Random, false},  // OblivionGatetoOblivion
	{0x000608A8, 0x000608B7, 80355.0f, 27102.0f, 2120.0f, GateKind::Random, false},  // OblivionGatetoOblivion
	{0x000608BD, 0x000608CF, 103538.0f, 27295.0f, 3946.0f, GateKind::Random, false},  // OblivionGatetoOblivion
	{0x000608D5, 0x000608E5, 125357.0f, 14400.0f, 4282.0f, GateKind::Random, false},  // OblivionGatetoOblivion
	{0x000608EB, 0x000608FB, 125138.0f, -6033.0f, 5841.0f, GateKind::Random, false},  // OblivionGatetoOblivion
	{0x000613FA, 0x00061409, -142912.0f, -37261.0f, 5376.0f, GateKind::Random, false},  // OblivionGatetoOblivion
	{0x000614CD, 0x000614CC, 150076.0f, -88088.0f, 1640.0f, GateKind::Random, false},  // OblivionGatetoOblivion
	{0x000614CE, 0x000614CF, 107362.0f, -74720.0f, 910.0f, GateKind::Random, false},  // OblivionGatetoOblivion
	{0x000615E1, 0x0006161E, -137469.0f, 2520.0f, 6949.0f, GateKind::Random, true},  // OblivionGatetoOblivion
	{0x000615E2, 0x0006162D, -120999.0f, 26771.0f, 10227.0f, GateKind::Random, false},  // OblivionGatetoOblivion
	{0x000615E3, 0x00061655, -146164.0f, 35165.0f, 13943.0f, GateKind::Random, false},  // OblivionGatetoOblivion
	{0x000615E4, 0x00061675, -125678.0f, 49946.0f, 19347.0f, GateKind::Random, false},  // OblivionGatetoOblivion
	{0x000615E8, 0x00062621, -64205.0f, 133134.0f, 28962.0f, GateKind::Random, false},  // OblivionGatetoOblivion
	{0x000615EA, 0x00062622, -88895.0f, 75323.0f, 13670.0f, GateKind::Random, false},  // OblivionGatetoOblivion
	{0x000615EB, 0x00062623, -96342.0f, 46058.0f, 9878.0f, GateKind::Random, false},  // OblivionGatetoOblivion
	{0x000615EC, 0x00062624, -96440.0f, 34349.0f, 14051.0f, GateKind::Random, false},  // OblivionGatetoOblivion
	{0x00061699, 0x000616A6, 122687.0f, -57728.0f, 712.0f, GateKind::Random, false},  // OblivionGatetoOblivion
	{0x000616AB, 0x000616BB, 136999.0f, -42675.0f, 7511.0f, GateKind::Random, false},  // OblivionGatetoOblivion
	{0x000616BF, 0x000616CB, 171190.0f, -9684.0f, 2540.0f, GateKind::Random, false},  // OblivionGatetoOblivion
	{0x000616D1, 0x000616E1, 145072.0f, 23849.0f, 9756.0f, GateKind::Random, false},  // OblivionGatetoOblivion
	{0x000616E6, 0x000616F8, 181620.0f, -40454.0f, 3917.0f, GateKind::Random, true},  // OblivionGatetoOblivion
	{0x000616FF, 0x0006170D, 203310.0f, -10069.0f, 7752.0f, GateKind::Random, false},  // OblivionGatetoOblivion
	{0x000623B6, 0x000623B7, 64618.0f, -5149.0f, 2910.0f, GateKind::Random, false},  // OblivionGatetoOblivion
	{0x000623B8, 0x000623B9, 60005.0f, 9334.0f, 1089.0f, GateKind::Random, false},  // OblivionGatetoOblivion
	{0x000623BA, 0x000623BB, 48126.0f, 41295.0f, 903.0f, GateKind::Random, false},  // OblivionGatetoOblivion
	{0x000623BC, 0x000623BD, 55484.0f, 93190.0f, 955.0f, GateKind::Random, false},  // OblivionGatetoOblivion
	{0x000623BE, 0x000623BF, 69197.0f, 85610.0f, 5735.0f, GateKind::Random, false},  // OblivionGatetoOblivion
	{0x000623C0, 0x000623C1, 79334.0f, 62532.0f, 4266.0f, GateKind::Random, false},  // OblivionGatetoOblivion
	{0x000623C2, 0x000623C3, 83172.0f, 2815.0f, 958.0f, GateKind::Random, false},  // OblivionGatetoOblivion
	{0x0006253C, 0x0006253B, -80494.0f, 116829.0f, 19002.0f, GateKind::Random, false},  // OblivionGatetoOblivion
	{0x0006253D, 0x0006253E, -79923.0f, 100300.0f, 16159.0f, GateKind::Random, false},  // OblivionGatetoOblivion
	{0x0006253F, 0x00062540, -95777.0f, 104231.0f, 19513.0f, GateKind::Random, false},  // OblivionGatetoOblivion
	{0x00062541, 0x00062542, -112410.0f, 67080.0f, 23380.0f, GateKind::Random, false},  // OblivionGatetoOblivion
	{0x000643DC, 0x000643DD, -30065.0f, 146013.0f, 32908.0f, GateKind::Random, false},  // OblivionGatetoOblivion
	{0x000643F2, 0x000643F1, 96513.0f, 129143.0f, 27016.0f, GateKind::Random, false},  // OblivionGatetoOblivion
	{0x0006440E, 0x0006440F, 5707.0f, 146405.0f, 28231.0f, GateKind::Random, false},  // OblivionGatetoOblivion
	{0x00064416, 0x00064417, -59882.0f, 153499.0f, 40117.0f, GateKind::Random, false},  // OblivionGatetoOblivion
	{0x000645A3, 0x000645A4, 161490.0f, 30674.0f, 5436.0f, GateKind::Random, false},  // OblivionGatetoOblivion
	{0x000645CE, 0x000645CF, 112352.0f, 149102.0f, 21432.0f, GateKind::Random, false},  // OblivionGatetoOblivion
	{0x000645D6, 0x00064531, 140284.0f, 104319.0f, 18553.0f, GateKind::Random, false},  // OblivionGatetoOblivion
	{0x000836D3, 0x00084B07, -38814.0f, 84319.0f, 8665.0f, GateKind::Random, false},  // OblivionGatetoOblivion
	{0x00188742, 0x0018877A, -55386.0f, 63701.0f, 10663.0f, GateKind::Random, false},  // OblivionGatetoOblivion
	{0x00188824, 0x00188825, -100435.0f, 14645.0f, 6688.0f, GateKind::Random, false},  // OblivionGatetoOblivion
	{0x0018884F, 0x00188850, 5611.0f, -6123.0f, 5686.0f, GateKind::Random, false},  // OblivionGatetoOblivion
	{0x0018A833, 0x0018A834, 125007.0f, -145395.0f, 3130.0f, GateKind::Random, false},  // OblivionGatetoOblivion
	{0x0018ADFF, 0x0018AE03, 30226.0f, 1693.0f, 4019.0f, GateKind::Random, false},  // OblivionGatetoOblivion
	{0x0018AE1C, 0x0018AE22, 79905.0f, 149114.0f, 34027.0f, GateKind::Random, false},  // OblivionGatetoOblivion
	{0x0018B0C3, 0x0018BB6E, 42966.0f, -18391.0f, 1663.0f, GateKind::Random, false},  // OblivionGatetoOblivion
	{0x0018B0C5, 0x0018BB6F, -6590.0f, 18151.0f, 3491.0f, GateKind::Random, false},  // OblivionGatetoOblivion
	{0x0018B0C6, 0x0018D8C9, -36115.0f, 38694.0f, 6172.0f, GateKind::Random, false},  // OblivionGatetoOblivion
	{0x0018B243, 0x0018BB06, -177901.0f, -46890.0f, 4138.0f, GateKind::Random, false},  // OblivionGatetoOblivion
	{0x0018B244, 0x0018BB4B, -157883.0f, -51137.0f, 1023.0f, GateKind::Random, false},  // OblivionGatetoOblivion
	{0x0018B245, 0x0018D0EA, -165790.0f, -22401.0f, 4946.0f, GateKind::Random, false},  // OblivionGatetoOblivion
	{0x0018B246, 0x0018BB4A, -153186.0f, 2400.0f, 7426.0f, GateKind::Random, false},  // OblivionGatetoOblivion
	{0x0018B247, 0x00061690, -158668.0f, 17176.0f, 8889.0f, GateKind::Random, false},  // OblivionGatetoOblivion
	{0x0018B248, 0x0018D10E, -194577.0f, 26229.0f, 5925.0f, GateKind::Random, false},  // OblivionGatetoOblivion
	{0x0018B249, 0x0018D494, -202215.0f, 1288.0f, 7382.0f, GateKind::Random, false},  // OblivionGatetoOblivion
	{0x0018B24A, 0x0018D4B6, -177934.0f, -18267.0f, 5435.0f, GateKind::Random, false},  // OblivionGatetoOblivion
	{0x0018B73C, 0x0018B73D, 109217.0f, -165929.0f, 2777.0f, GateKind::Random, false},  // OblivionGatetoOblivion
	{0x0018B73E, 0x0018B73F, 113091.0f, -153307.0f, 3306.0f, GateKind::Random, false},  // OblivionGatetoOblivion
	{0x0018B740, 0x0018B741, 78572.0f, -100260.0f, 1662.0f, GateKind::Random, false},  // OblivionGatetoOblivion
	{0x0018B742, 0x0018B743, 95891.0f, -132406.0f, 133.0f, GateKind::Random, false},  // OblivionGatetoOblivion
	{0x0018BB6B, 0x0018BB6A, 18267.0f, 15214.0f, 1142.0f, GateKind::Random, false},  // OblivionGatetoOblivion
	{0x0018BB6D, 0x0018BB6C, 38268.0f, 130053.0f, 16898.0f, GateKind::Random, false},  // OblivionGatetoOblivion
	{0x0018BB70, 0x0018BB71, 72951.0f, 113880.0f, 16257.0f, GateKind::Random, false},  // OblivionGatetoOblivion
	{0x0018BB72, 0x0018BB73, -25536.0f, 72153.0f, 6680.0f, GateKind::Random, false},  // OblivionGatetoOblivion
	{0x0018BD37, 0x0018BD3A, -14858.0f, -6144.0f, 4722.0f, GateKind::Random, false},  // OblivionGatetoOblivion
	{0x0018D172, 0x0018D184, 111040.0f, 71200.0f, 5596.0f, GateKind::Random, false},  // OblivionGatetoOblivion
	{0x0018D18D, 0x0018D19E, 120386.0f, 60708.0f, 1275.0f, GateKind::Random, false},  // OblivionGatetoOblivion
	{0x0018D19F, 0x0018D1AE, 127412.0f, 76161.0f, 1353.0f, GateKind::Random, false},  // OblivionGatetoOblivion
	{0x0018D1B4, 0x0018D1C2, 121663.0f, 108619.0f, 7765.0f, GateKind::Random, false},  // OblivionGatetoOblivion
	{0x0018D1C6, 0x0018D1D5, 87902.0f, 47462.0f, 2282.0f, GateKind::Random, false},  // OblivionGatetoOblivion
	{0x0018D320, 0x0018D31F, 118889.0f, -100803.0f, 4712.0f, GateKind::Random, false},  // OblivionGatetoOblivion
	{0x0018D322, 0x0018D321, 105280.0f, -112374.0f, 3814.0f, GateKind::Random, false},  // OblivionGatetoOblivion
	{0x0018D324, 0x0018D323, 138824.0f, -133084.0f, 3411.0f, GateKind::Random, false},  // OblivionGatetoOblivion
	{0x0018D326, 0x0018D325, 132281.0f, -169259.0f, 2312.0f, GateKind::Random, false},  // OblivionGatetoOblivion
	{0x0018D328, 0x0018D327, 121965.0f, -174993.0f, 373.0f, GateKind::Random, false},  // OblivionGatetoOblivion
	{0x0018D329, 0x0018D31E, 141294.0f, -101356.0f, 3079.0f, GateKind::Random, false},  // OblivionGatetoOblivion
	{0x0018D8C4, 0x0018D8C3, 88181.0f, 104473.0f, 9851.0f, GateKind::Random, false},  // OblivionGatetoOblivion
	{0x0018D8C6, 0x0018D8C5, -4726.0f, 103229.0f, 7555.0f, GateKind::Random, false},  // OblivionGatetoOblivion
	{0x0018D8C7, 0x0018D8C8, 23548.0f, 107514.0f, 6402.0f, GateKind::Random, false},  // OblivionGatetoOblivion
};
inline constexpr UInt32 kOblivionGateCount = sizeof(kOblivionGates) / sizeof(kOblivionGates[0]);

}  // namespace walkers
