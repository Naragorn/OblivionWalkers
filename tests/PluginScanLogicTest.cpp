// Every flow of the boss guard's file reading (docs/walkers-spec.md 4.6c;
// Nadi, 2026-11-16: "ja A bauen"): the words in an editor ID, a plugin's
// masters out of its TES4 record, a file's form ID turned into the game's,
// the records of a group walked (the compressed ones skipped), and the
// decision on one list or one creature.

#include "PluginScanLogic.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf("  %s  %s\n", condition ? "ok  " : "FAIL", what);
	if (!condition) {
		++g_failures;
	}
}

void Put32(std::vector<UInt8>& b, UInt32 v) {
	for (int i = 0; i < 4; ++i) {
		b.push_back(static_cast<UInt8>(v >> (8 * i)));
	}
}
void Put16(std::vector<UInt8>& b, UInt16 v) {
	b.push_back(static_cast<UInt8>(v));
	b.push_back(static_cast<UInt8>(v >> 8));
}
void PutSub(std::vector<UInt8>& b, const char* type, const std::string& data) {
	b.insert(b.end(), type, type + 4);
	Put16(b, static_cast<UInt16>(data.size()));
	b.insert(b.end(), data.begin(), data.end());
}
// A record: type, size, flags, form ID, version control, then its data.
void PutRecord(std::vector<UInt8>& b, const char* type, UInt32 flags, UInt32 formId, const std::vector<UInt8>& data) {
	b.insert(b.end(), type, type + 4);
	Put32(b, static_cast<UInt32>(data.size()));
	Put32(b, flags);
	Put32(b, formId);
	Put32(b, 0);
	b.insert(b.end(), data.begin(), data.end());
}
std::vector<UInt8> Edid(const std::string& name) {
	std::vector<UInt8> d;
	PutSub(d, "EDID", name + std::string(1, '\0'));
	return d;
}

void TestWords() {
	std::printf("The words in an editor ID\n");
	const char* words = "Boss,Champion,Leader,Lord,Chief";
	Check(walkers::HasAnyWord("LL1GoblinBossLvl100", words) && walkers::HasAnyWord("MOOLL1ChampionNord", words) &&
	          walkers::HasAnyWord("MOOLL0LeaderOrc", words) && walkers::HasAnyWord("MOOLL0DaedricLord100", words),
	      "Boss, Champion, Leader, Lord: kept");
	Check(walkers::HasAnyWord("se09ll1atronachfleshBOSS100", words), "in any case");
	Check(!walkers::HasAnyWord("LL1Goblin100", words) && !walkers::HasAnyWord("MOOLL0GoblinBHard", words) &&
	          !walkers::HasAnyWord("MOOLL1RoadHighlands", words),
	      "a plain list, MOO's Hard and the Highlands: not");
	Check(!walkers::HasAnyWord("LL1GoblinBossLvl100", "") && !walkers::HasAnyWord("", words),
	      "no words, or no name: nothing");
	Check(walkers::HasAnyWord("LL1GoblinBossLvl100", " boss , x"), "the words trimmed");
	Check(!walkers::HasAnyWord("LL1GoblinBossLvl100", ",,"), "empty words match nothing");
}

void TestMasters() {
	std::printf("A plugin's masters\n");
	std::vector<UInt8> data;
	PutSub(data, "HEDR", std::string(12, '\0'));
	PutSub(data, "MAST", std::string("Oblivion.esm") + '\0');
	PutSub(data, "DATA", std::string(8, '\0'));
	PutSub(data, "MAST", std::string("Maskar's Oblivion Overhaul.esp") + '\0');
	walkers::PluginMasters m;
	Check(walkers::ReadMasters(data.data(), static_cast<UInt32>(data.size()), m) && m.count == 2 &&
	          std::strcmp(m.names[0], "Oblivion.esm") == 0 &&
	          std::strcmp(m.names[1], "Maskar's Oblivion Overhaul.esp") == 0,
	      "two MAST, in order, the other subrecords passed over");
	walkers::PluginMasters none;
	Check(walkers::ReadMasters(data.data(), 5, none) && none.count == 0, "cut short: what was whole");
}

void TestResolve() {
	std::printf("A file's form ID turned into the game's\n");
	const UInt8 masterIndex[2] = {0x00, 0x17};  // Oblivion.esm at 00, MOO at 17
	Check(walkers::ResolveFormId(0x00034097, masterIndex, 2, 0x20) == 0x00034097, "a master's own: its index (00)");
	Check(walkers::ResolveFormId(0x01001234, masterIndex, 2, 0x20) == 0x17001234, "the second master: 17");
	Check(walkers::ResolveFormId(0x02000ABC, masterIndex, 2, 0x20) == 0x20000ABC, "past the masters: the file's own");
	const UInt8 missing[1] = {0xFF};
	Check(walkers::ResolveFormId(0x00000ABC, missing, 1, 0x20) == 0, "a master not loaded: 0, nothing");
}

void TestGroup() {
	std::printf("The records of a group\n");
	std::vector<UInt8> g;
	PutRecord(g, "LVLC", 0, 0x00001001, Edid("LL1GoblinBossLvl100"));
	PutRecord(g, "LVLC", 0x00040000, 0x00001002, std::vector<UInt8>(12, 0));  // compressed
	PutRecord(g, "LVLC", 0, 0x00001003, Edid("LL1Goblin100"));
	struct Seen {
		UInt32 ids[8];
		std::string names[8];
		UInt32 n = 0;
	} seen;
	UInt32 compressed = 0;
	walkers::WalkGroupRecords(g.data(), static_cast<UInt32>(g.size()), compressed,
	                          [&seen](const char* type, UInt32 formId, const char* edid) {
		                          (void)type;
		                          seen.ids[seen.n] = formId;
		                          seen.names[seen.n++] = edid;
	                          });
	Check(seen.n == 2 && seen.ids[0] == 0x1001 && seen.names[0] == "LL1GoblinBossLvl100" && seen.ids[1] == 0x1003 &&
	          seen.names[1] == "LL1Goblin100",
	      "two plain records with their names");
	Check(compressed == 1, "the compressed one counted, passed over");
	std::vector<UInt8> cut(g.begin(), g.begin() + 30);
	seen.n = 0;
	walkers::WalkGroupRecords(cut.data(), static_cast<UInt32>(cut.size()), compressed,
	                          [&seen](const char*, UInt32, const char*) { ++seen.n; });
	Check(seen.n == 0, "a record cut short: nothing read past the end");
}

void TestDecision() {
	std::printf("What is kept\n");
	walkers::BossGuard guard;
	Check(guard.keepBosses && std::strcmp(guard.listWords, "Boss,Champion,Leader,Lord,Chief") == 0 &&
	          std::strcmp(guard.actorWords, "Boss") == 0,
	      "defaults: on; lists by Boss, Champion, Leader, Lord, Chief; creatures and people by Boss");
	Check(walkers::KeepList(guard, "LL1GoblinBossLvl100", -1) && !walkers::KeepList(guard, "LL1Goblin100", -1),
	      "a boss list kept, a plain one not");
	Check(!walkers::KeepList(guard, "LL1GoblinBossLvl100", 0) && walkers::KeepList(guard, "LL1Goblin100", 1),
	      "[ReplaceKeepLists] decides over the name: 0 never kept, 1 always");
	Check(walkers::KeepActor(guard, "SECreatureAtronachFlesh5Boss") && !walkers::KeepActor(guard, "CreatureMinotaurLord"),
	      "a boss creature kept; a Minotaur Lord (a common high-level foe) not");
	guard.keepBosses = false;
	Check(!walkers::KeepList(guard, "LL1GoblinBossLvl100", -1) && !walkers::KeepActor(guard, "BanditBossMale1"),
	      "KeepBosses=0: nothing kept by its name");
	Check(walkers::KeepList(guard, "LL1Goblin100", 1), "but a list the INI names still kept");
}

void TestHardHigh() {
	std::printf("MOO's Hard and the High lists\n");
	walkers::BossGuard guard;
	Check(!guard.keepHard && !guard.keepHigh, "both off by default (Nadi, 2026-11-16)");
	Check(!walkers::KeepList(guard, "MOOLL0GoblinBHard", -1) && !walkers::KeepList(guard, "MOOLL1RoadHighlands", -1),
	      "off: replaced like any list");
	guard.keepHard = true;
	Check(walkers::KeepList(guard, "MOOLL0GoblinBHard", -1) && !walkers::KeepList(guard, "MOOLL1RoadHighlands", -1),
	      "KeepHardLists=1: the Hard ones kept, the High ones not");
	guard.keepHigh = true;
	Check(walkers::KeepList(guard, "MOOLL2UndeadZombie0HighElf", -1), "KeepHighLists=1: the High ones too");
	guard.keepBosses = false;
	Check(!walkers::KeepList(guard, "LL1GoblinBossLvl100", -1) && walkers::KeepList(guard, "MOOLL0GoblinBHard", -1),
	      "each switch on its own: KeepBosses=0 lets the bosses go, the Hard ones stay");
	const UInt8 bits = walkers::ListNameBits(guard, "MOOLL0HighBossHard");
	Check(bits == (walkers::kListNameBoss | walkers::kListNameHard | walkers::kListNameHigh),
	      "a name may say all three");
	Check(!walkers::KeepListByBits(guard, bits, 0), "[ReplaceKeepLists]=0 over all of them");
}

}  // namespace

int main() {
	TestWords();
	TestMasters();
	TestResolve();
	TestGroup();
	TestDecision();
	TestHardHigh();
	if (g_failures != 0) {
		std::printf("%d failed\n", g_failures);
		return 1;
	}
	std::printf("all passed\n");
	return 0;
}
