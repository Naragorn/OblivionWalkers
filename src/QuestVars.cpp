#include "QuestVars.h"

#include "Engine.h"
#include "QuestVarLogic.h"

namespace walkers {
namespace {

using namespace engine;

// xOBSE's GameForms.h and Script.h for 1.2.416: TESQuest's script (its
// TESScriptableForm at +0x18, the script at +0x04 in it) and event list;
// Script's variable list (the first entry held in the script itself); a
// VariableInfo's index and name; the event list's variables and a
// variable's id and value.
constexpr UInt32 kQuestScriptOffset = 0x18 + 0x04;
constexpr UInt32 kQuestEventListOffset = 0x58;
constexpr UInt32 kScriptVarListOffset = 0x48;
constexpr UInt32 kVarInfoIndexOffset = 0x00;
constexpr UInt32 kVarInfoNameOffset = 0x18;
constexpr UInt32 kEventListVarsOffset = 0x0C;
constexpr UInt32 kVarIdOffset = 0x00;
constexpr UInt32 kVarDataOffset = 0x08;
constexpr UInt32 kVarsMost = 4096;

}  // namespace

double* QuestVariable(UInt32 questFormId, const char* name) {
	const UInt32 quest = LookupForm(questFormId);
	if (!LooksLikeObject(quest)) {
		return nullptr;
	}
	const UInt32 script = Read(quest + kQuestScriptOffset);
	const UInt32 list = Read(quest + kQuestEventListOffset);
	if (!LooksLikeObject(script) || !LooksLikeObject(list)) {
		return nullptr;
	}
	static VarName names[kVarsMost];
	UInt32 count = 0;
	for (UInt32 entry = script + kScriptVarListOffset; LooksLikeObject(entry) && count < kVarsMost;
	     entry = Read(entry + 4)) {
		const UInt32 info = Read(entry);
		if (!LooksLikeObject(info)) {
			continue;
		}
		const UInt32 text = Read(info + kVarInfoNameOffset);
		names[count++] =
			VarName{Read(info + kVarInfoIndexOffset), LooksLikeObject(text) ? reinterpret_cast<const char*>(text) : nullptr};
	}
	UInt32 idx = 0;
	if (!FindVariableIndex(names, count, name, idx)) {
		return nullptr;
	}
	UInt32 seen = 0;
	for (UInt32 entry = Read(list + kEventListVarsOffset); LooksLikeObject(entry) && seen < kVarsMost;
	     entry = Read(entry + 4), ++seen) {
		const UInt32 var = Read(entry);
		if (LooksLikeObject(var) && Read(var + kVarIdOffset) == idx) {
			return reinterpret_cast<double*>(var + kVarDataOffset);
		}
	}
	return nullptr;
}

}  // namespace walkers
