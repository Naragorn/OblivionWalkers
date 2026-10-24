// Every flow of the spawn replacement's decisions (docs/walkers-spec.md 4.6,
// 4.5): which creature becomes a walker, its INI line both ways, the
// section merge, the walkers' count spread and their health.

#include "ReplaceLogic.h"

#include <cstdio>
#include <cstring>
#include <string>

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf("  %s  %s\n", condition ? "ok  " : "FAIL", what);
	if (!condition) {
		++g_failures;
	}
}

using walkers::CreatureFacts;
using walkers::CreatureKind;

CreatureFacts Wolf() {
	CreatureFacts facts;
	facts.type = walkers::kCreatureTypeCreature;
	facts.confidence = 50;
	facts.flags = 0x350;  // swims, walks, respawn (CreatureWolf's ACBS)
	return facts;
}

void TestClassify() {
	std::printf("Which creature becomes a walker\n");
	Check(walkers::ClassifyCreature(Wolf()) == CreatureKind::Replace, "a wolf: replaced");

	CreatureFacts deer = Wolf();
	deer.confidence = 0;
	Check(walkers::ClassifyCreature(deer) == CreatureKind::HarmlessAnimal, "a deer (confidence 0): a harmless animal");
	CreatureFacts ram = Wolf();
	ram.confidence = walkers::kHarmlessConfidenceMost;
	Check(walkers::ClassifyCreature(ram) == CreatureKind::HarmlessAnimal,
	      "the sheep ram at the bound (confidence 25): still harmless");
	CreatureFacts boar = Wolf();
	boar.confidence = walkers::kHarmlessConfidenceMost + 1;
	Check(walkers::ClassifyCreature(boar) == CreatureKind::Replace, "one above the bound: replaced");

	CreatureFacts scamp = Wolf();
	scamp.type = walkers::kCreatureTypeDaedra;
	scamp.confidence = 0;
	Check(walkers::ClassifyCreature(scamp) == CreatureKind::Replace,
	      "a daedra that never fights back: no animal, replaced");
	CreatureFacts skeleton = Wolf();
	skeleton.type = walkers::kCreatureTypeUndead;
	Check(walkers::ClassifyCreature(skeleton) == CreatureKind::Replace, "the undead: replaced");

	CreatureFacts fish = Wolf();
	fish.flags = 0x318;  // swims, no walks (CreatureSlaughterfish)
	Check(walkers::ClassifyCreature(fish) == CreatureKind::SwimsOnly, "a slaughterfish (swims, never walks): kept");
	CreatureFacts wisp = Wolf();
	wisp.flags = walkers::kCreatureSwims | walkers::kCreatureFlies;
	Check(walkers::ClassifyCreature(wisp) == CreatureKind::Replace, "one that swims and flies: no swimmer only");

	CreatureFacts arena = Wolf();
	arena.scripted = true;
	Check(walkers::ClassifyCreature(arena) == CreatureKind::Scripted,
	      "a creature with a script (the arena's): kept, a quest may count on it");
	CreatureFacts scriptedDeer = deer;
	scriptedDeer.scripted = true;
	Check(walkers::ClassifyCreature(scriptedDeer) == CreatureKind::Scripted, "the script decides before the animal");

	CreatureFacts ours = Wolf();
	ours.ours = true;
	Check(walkers::ClassifyCreature(ours) == CreatureKind::Walker, "a walker of the ESP: never replaced by itself");

	CreatureFacts bandit;
	bandit.person = true;
	Check(walkers::ClassifyCreature(bandit) == CreatureKind::Person, "a person of a leveled list: a person");
	CreatureFacts scriptedBandit = bandit;
	scriptedBandit.scripted = true;
	Check(walkers::ClassifyCreature(scriptedBandit) == CreatureKind::Person, "a person with a script: still a person");
	CreatureFacts oursPerson = bandit;
	oursPerson.ours = true;
	Check(walkers::ClassifyCreature(oursPerson) == CreatureKind::Walker, "a record of the ESP: ours before a person");

	Check(walkers::DefaultOn(CreatureKind::Replace) && !walkers::DefaultOn(CreatureKind::HarmlessAnimal) &&
	          !walkers::DefaultOn(CreatureKind::SwimsOnly) && !walkers::DefaultOn(CreatureKind::Scripted) &&
	          !walkers::DefaultOn(CreatureKind::Person) && !walkers::DefaultOn(CreatureKind::Walker),
	      "only the replaced kind is on by default - never a person");
	Check(std::strcmp(walkers::KindNote(CreatureKind::Replace), "") == 0 &&
	          std::strcmp(walkers::KindNote(CreatureKind::HarmlessAnimal), " harmless animal") == 0 &&
	          std::strcmp(walkers::KindNote(CreatureKind::SwimsOnly), " swims only") == 0 &&
	          std::strcmp(walkers::KindNote(CreatureKind::Scripted), " has a script") == 0 &&
	          std::strcmp(walkers::KindNote(CreatureKind::Person), " person") == 0,
	      "the notes the INI line carries");
}

void TestKeys() {
	std::printf("The INI line of a creature, both ways\n");
	char key[160];
	walkers::FormatReplaceKey(key, sizeof(key), "Timber Wolf", "Oblivion.esm", 0x0001E5F6, CreatureKind::Replace);
	Check(std::strcmp(key, "Timber Wolf [Oblivion.esm 01E5F6]") == 0, "a replaced creature: its name and its ID");
	walkers::FormatReplaceKey(key, sizeof(key), "Deer", "Oblivion.esm", 0x00031550, CreatureKind::HarmlessAnimal);
	Check(std::strcmp(key, "Deer [Oblivion.esm 031550] harmless animal") == 0, "a kept one: with its note");
	walkers::FormatReplaceKey(key, sizeof(key), "Bandit", "Oblivion.esm", 0x00069AD1, CreatureKind::Person);
	Check(std::strcmp(key, "Bandit [Oblivion.esm 069AD1] person") == 0, "a person: with its note");
	walkers::FormatReplaceKey(key, sizeof(key), "A=B;[C]", "Mod.esp", 0x123, CreatureKind::Replace);
	Check(std::strcmp(key, "A B  C  [Mod.esp 000123]") == 0, "the INI's own signs in a name become spaces");
	walkers::FormatReplaceKey(key, sizeof(key), "", "Mod.esp", 0x123, CreatureKind::Replace);
	Check(std::strcmp(key, "Creature [Mod.esp 000123]") == 0, "a nameless one: Creature");
	walkers::FormatReplaceKey(key, sizeof(key), nullptr, "Mod.esp", 0x123, CreatureKind::Replace);
	Check(std::strcmp(key, "Creature [Mod.esp 000123]") == 0, "no name at all: Creature");
	walkers::FormatReplaceKey(key, sizeof(key), "Wolf", "Mod.esp", 0xFF123456, CreatureKind::Replace);
	Check(std::strcmp(key, "Wolf [Mod.esp 123456]") == 0, "only the local ID, never the load index");
	char small[12];
	walkers::FormatReplaceKey(small, sizeof(small), "Timber Wolf", "Oblivion.esm", 0x1E5F6, CreatureKind::Replace);
	Check(std::strlen(small) == sizeof(small) - 1, "a short buffer: cut, still ended");

	char mod[64];
	UInt32 local = 0;
	Check(walkers::ParseReplaceKey("Timber Wolf [Oblivion.esm 01E5F6]", mod, sizeof(mod), &local) &&
	          std::strcmp(mod, "Oblivion.esm") == 0 && local == 0x01E5F6,
	      "read back: the mod and the local ID");
	Check(walkers::ParseReplaceKey("Deer [Oblivion.esm 031550] harmless animal", mod, sizeof(mod), &local) &&
	          local == 0x031550,
	      "the note behind the bracket is no part of it");
	Check(walkers::ParseReplaceKey("Wolf [Maskar's Oblivion Overhaul.esp 00ABCD]", mod, sizeof(mod), &local) &&
	          std::strcmp(mod, "Maskar's Oblivion Overhaul.esp") == 0 && local == 0xABCD,
	      "a mod name with spaces: up to the last space");
	Check(walkers::ParseReplaceKey("A [B] [Mod.esp 000001]", mod, sizeof(mod), &local) &&
	          std::strcmp(mod, "Mod.esp") == 0 && local == 1,
	      "the last bracket counts");
	Check(!walkers::ParseReplaceKey("Wolf", mod, sizeof(mod), &local), "no bracket: no creature");
	Check(!walkers::ParseReplaceKey("Wolf [Oblivion.esm]", mod, sizeof(mod), &local), "no ID: no creature");
	Check(!walkers::ParseReplaceKey("Wolf [Oblivion.esm 0G1234]", mod, sizeof(mod), &local), "no hex: no creature");
	Check(!walkers::ParseReplaceKey("Wolf [Oblivion.esm 1234567]", mod, sizeof(mod), &local),
	      "more than six digits: no creature");
	Check(!walkers::ParseReplaceKey("Wolf [Oblivion.esm 01E5F6", mod, sizeof(mod), &local), "unclosed: no creature");
	Check(!walkers::ParseReplaceKey("Wolf [ 01E5F6]", mod, sizeof(mod), &local), "no mod name: no creature");
	char tiny[4];
	Check(!walkers::ParseReplaceKey("Wolf [Oblivion.esm 01E5F6]", tiny, sizeof(tiny), &local),
	      "a mod name too long for the buffer: refused, not cut");
}

void TestSection() {
	std::printf("The [ReplaceCreatures] section\n");
	// GetPrivateProfileSection's shape: key=value lines, each ended, the last
	// one twice.
	const char section[] = "Wolf [Oblivion.esm 06E912]=1\0Deer [oblivion.esm 031550] harmless animal=0\0"
	                       "Imp [Oblivion.esm 01E649]= 1 \0Rat [Oblivion.esm 025314]=yes\0"
	                       "nothing to read\0Ogre [Oblivion.esm 00C20D]=\0\0";
	Check(walkers::FindReplaceSwitch(section, "Oblivion.esm", 0x06E912) == 1, "a line with 1: on");
	Check(walkers::FindReplaceSwitch(section, "Oblivion.esm", 0x031550) == 0,
	      "a line with 0: off - the mod's name without case");
	Check(walkers::FindReplaceSwitch(section, "Oblivion.esm", 0x01E649) == 1, "spaces around the value");
	Check(walkers::FindReplaceSwitch(section, "Oblivion.esm", 0x025314) == 0, "a value that is no number: off");
	Check(walkers::FindReplaceSwitch(section, "Oblivion.esm", 0x00C20D) == 0, "an empty value: off");
	Check(walkers::FindReplaceSwitch(section, "Oblivion.esm", 0x000B7F) == -1, "no line: not in the INI yet");
	Check(walkers::FindReplaceSwitch(section, "Knights.esp", 0x06E912) == -1, "the same ID of another mod: not it");
	Check(walkers::FindReplaceSwitch("\0", "Oblivion.esm", 1) == -1, "an empty section: nothing");

	char buffer[64] = {};
	UInt32 used = 0;
	Check(walkers::AppendSectionLine(buffer, sizeof(buffer), &used, "Wolf [Oblivion.esm 06E912]", 1) && used == 29,
	      "a line appended: key=value and its end");
	Check(walkers::AppendSectionLine(buffer, sizeof(buffer), &used, "Deer [Oblivion.esm 031550]", 0) && used == 58,
	      "a second one behind it");
	Check(buffer[used] == '\0' && std::strcmp(buffer, "Wolf [Oblivion.esm 06E912]=1") == 0 &&
	          std::strcmp(buffer + 29, "Deer [Oblivion.esm 031550]=0") == 0,
	      "the shape WritePrivateProfileSection takes: lines, then the closing end");
	Check(walkers::FindReplaceSwitch(buffer, "Oblivion.esm", 0x031550) == 0, "and the finder reads it");
	Check(!walkers::AppendSectionLine(buffer, sizeof(buffer), &used, "Too long for what is left", 1) && used == 58 &&
	          buffer[58] == '\0',
	      "no room: refused, the section as it was");
}

// InsertSectionLines over `text`, as a string; "" when it refused.
std::string Inserted(const std::string& text, const char* lines, UInt32 capacity = 4096) {
	std::string out(capacity, '\0');
	const UInt32 used = walkers::InsertSectionLines(text.data(), static_cast<UInt32>(text.size()), "ReplaceCreatures",
	                                                lines, &out[0], capacity);
	out.resize(used);
	return out;
}

void TestInsert() {
	std::printf("New lines into the INI file, the rest of it as it was\n");
	const char two[] = "Bat [Mod.esp 000001]=1\0Rat [Mod.esp 000002]=0\0";

	const std::string middle =
	    "; [ReplaceCreatures] lists them\r\n[Replace]\r\nEnabled=1\r\n\r\n[ReplaceCreatures]\r\n; =0 keeps one\r\n"
	    "Wolf [Oblivion.esm 06E912]=1\r\n\r\n; the next\r\n[Zombies]\r\nWalk=1\r\n";
	Check(Inserted(middle, two) ==
	          "; [ReplaceCreatures] lists them\r\n[Replace]\r\nEnabled=1\r\n\r\n[ReplaceCreatures]\r\n; =0 keeps one\r\n"
	          "Wolf [Oblivion.esm 06E912]=1\r\nBat [Mod.esp 000001]=1\r\nRat [Mod.esp 000002]=0\r\n\r\n; the next\r\n"
	          "[Zombies]\r\nWalk=1\r\n",
	      "a section between others: behind its last line; the blank line, the comments and the next section kept");
	Check(Inserted("[ReplaceCreatures]\r\nWolf [Oblivion.esm 06E912]=1\r\n", two) ==
	          "[ReplaceCreatures]\r\nWolf [Oblivion.esm 06E912]=1\r\nBat [Mod.esp 000001]=1\r\nRat [Mod.esp 000002]=0\r\n",
	      "the section at the file's end");
	Check(Inserted("[ReplaceCreatures]\r\nWolf [Oblivion.esm 06E912]=1", two) ==
	          "[ReplaceCreatures]\r\nWolf [Oblivion.esm 06E912]=1\r\nBat [Mod.esp 000001]=1\r\nRat [Mod.esp 000002]=0\r\n",
	      "the last line without its end: ended first");
	Check(Inserted("[ReplaceCreatures]\nWolf [Oblivion.esm 06E912]=1\n\n[Zombies]\n", two) ==
	          "[ReplaceCreatures]\nWolf [Oblivion.esm 06E912]=1\nBat [Mod.esp 000001]=1\nRat [Mod.esp 000002]=0\n\n"
	          "[Zombies]\n",
	      "a file of plain line feeds: line feeds");
	Check(Inserted("[ReplaceCreatures]\r\n[Zombies]\r\n", two) ==
	          "[ReplaceCreatures]\r\nBat [Mod.esp 000001]=1\r\nRat [Mod.esp 000002]=0\r\n[Zombies]\r\n",
	      "an empty section: right under its header");
	Check(Inserted("[ReplaceCreatures]\r\n; 1 replaces\r\n\r\n; the next\r\n[Zombies]\r\n", two) ==
	          "[ReplaceCreatures]\r\n; 1 replaces\r\nBat [Mod.esp 000001]=1\r\nRat [Mod.esp 000002]=0\r\n\r\n"
	          "; the next\r\n[Zombies]\r\n",
	      "a section of comments only: behind those right under the header, not the next section's");
	Check(Inserted("[replacecreatures]  \r\n[Zombies]\r\n", two) ==
	          "[replacecreatures]  \r\nBat [Mod.esp 000001]=1\r\nRat [Mod.esp 000002]=0\r\n[Zombies]\r\n",
	      "the header in other case, spaces behind it: still the section, as the profile API reads it");
	Check(Inserted("[Zombies]\r\nWalk=1\r\n", two) ==
	          "[Zombies]\r\nWalk=1\r\n[ReplaceCreatures]\r\nBat [Mod.esp 000001]=1\r\nRat [Mod.esp 000002]=0\r\n",
	      "no such section: added at the end");
	Check(Inserted("", two) == "[ReplaceCreatures]\r\nBat [Mod.esp 000001]=1\r\nRat [Mod.esp 000002]=0\r\n",
	      "an empty file: the section alone");
	Check(Inserted("[ReplaceCreatures]\r\nWolf=1\r\n", "") == "[ReplaceCreatures]\r\nWolf=1\r\n",
	      "no new lines: the file as it was");
	Check(Inserted(middle, two, 40).empty(), "no room for all of it: refused, nothing half written");
}

void TestFactsNote() {
	std::printf("The facts behind a creature's log line\n");
	char note[96];
	walkers::FormatFacts(note, sizeof(note), Wolf());
	Check(std::strcmp(note, "type 0, confidence 50, flags 00000350") == 0, "a creature: its type, confidence, flags");
	CreatureFacts bandit;
	bandit.person = true;
	bandit.type = 84;  // what +0x104 of a TESNPC held in the run of 20:36 - no type there
	bandit.confidence = 80;
	bandit.flags = 0x00100291;
	walkers::FormatFacts(note, sizeof(note), bandit);
	Check(std::strcmp(note, "confidence 80, flags 00100291") == 0, "a person: no creature type to name");
}

void TestCounts() {
	std::printf("How many walkers one spawn gives\n");
	Check(walkers::SpreadCount(1, 3, 0, 3) == 1 && walkers::SpreadCount(1, 3, 1, 3) == 2 &&
	          walkers::SpreadCount(1, 3, 2, 3) == 3,
	      "1 to 3 over three slots: 1, 2, 3");
	Check(walkers::SpreadCount(1, 1, 0, 3) == 1 && walkers::SpreadCount(1, 1, 2, 3) == 1, "1 to 1: always one");
	Check(walkers::SpreadCount(2, 5, 0, 3) == 2 && walkers::SpreadCount(2, 5, 1, 3) == 4 &&
	          walkers::SpreadCount(2, 5, 2, 3) == 5,
	      "2 to 5: the ends and the rounded middle");
	Check(walkers::SpreadCount(3, 1, 0, 3) == 3 && walkers::SpreadCount(3, 1, 2, 3) == 3,
	      "a maximum below the minimum: the minimum");
	Check(walkers::SpreadCount(0, 0, 1, 3) == 1, "zero: at least one, a spawn point never yields none by this");
	Check(walkers::SpreadCount(1, 50, 2, 3) == walkers::kWalkersPerSpawnMost,
	      "a huge maximum: capped at the most");
	Check(walkers::SpreadCount(1, 3, 5, 3) == 3, "a slot past the last: the maximum");
	Check(walkers::SpreadCount(1, 3, 0, 1) == 1 && walkers::SpreadCount(2, 3, 0, 0) == 2,
	      "one slot or none: the minimum");

	// The ESP's ten slots: 1 to 10 gives every count once, so each is as
	// likely as the next.
	bool every = true;
	for (UInt32 slot = 0; slot < walkers::kVariantSlots; ++slot) {
		every = every && walkers::SpreadCount(1, 10, slot, walkers::kVariantSlots) == slot + 1;
	}
	Check(every, "1 to 10 over the ten slots: 1, 2, ... 10");
	UInt32 times[4] = {};
	for (UInt32 slot = 0; slot < walkers::kVariantSlots; ++slot) {
		++times[walkers::SpreadCount(1, 3, slot, walkers::kVariantSlots)];
	}
	Check(times[1] == 3 && times[2] == 4 && times[3] == 3, "1 to 3 over the ten slots: 3 ones, 4 twos, 3 threes");
	Check(walkers::SpreadCount(10, 10, 0, walkers::kVariantSlots) == 10 &&
	          walkers::SpreadCount(10, 10, 9, walkers::kVariantSlots) == 10,
	      "10 to 10: always ten");
}

void TestHealth() {
	std::printf("The walkers' health\n");
	Check(walkers::ScaledHealth(200, 100) == 200, "100 percent: the record's own");
	Check(walkers::ScaledHealth(200, 25) == 50, "25 percent: a quarter");
	Check(walkers::ScaledHealth(3, 10) == 1, "a fraction: never below one");
	Check(walkers::ScaledHealth(200, 0) == 2, "0 percent: read as the least, 1");
	Check(walkers::ScaledHealth(200, 500) == 200, "above 100: read as 100");
	Check(walkers::ScaledHealth(0, 50) == 0, "a record without health stays so");
}

void TestSpeed() {
	std::printf("The walkers' pace, one per record\n");
	// The ESP's walkers read Speed 12 (CreatureZombie1..4's DATA).
	Check(walkers::WalkerSpeed(12, 1.0f, 0, 0, 4) == 12 && walkers::WalkerSpeed(12, 1.0f, 0, 3, 4) == 12,
	      "no spread, multiplier 1: the record's own for all four");
	Check(walkers::WalkerSpeed(12, 1.0f, 30, 0, 4) == 8 && walkers::WalkerSpeed(12, 1.0f, 30, 1, 4) == 11 &&
	          walkers::WalkerSpeed(12, 1.0f, 30, 2, 4) == 13 && walkers::WalkerSpeed(12, 1.0f, 30, 3, 4) == 16,
	      "30 percent over four: 8, 11, 13, 16 - the slowest 30 percent under, the fastest 30 over");
	Check(walkers::WalkerSpeed(12, 2.0f, 0, 1, 4) == 24, "multiplier 2: twice the record's");
	Check(walkers::WalkerSpeed(12, 0.5f, 30, 0, 4) == 4, "both: half, then the slowest 30 percent under");
	Check(walkers::WalkerSpeed(12, 1.0f, 30, 0, 1) == 12, "one record alone: no spread to make");
	Check(walkers::WalkerSpeed(12, 1.0f, 500, 0, 4) == 1, "above 100 percent read as 100: the slowest stands at 1");
	Check(walkers::WalkerSpeed(200, 2.0f, 0, 0, 4) == 255, "past what the record holds: 255");
	Check(walkers::WalkerSpeed(12, 0.0f, 0, 0, 4) == 12 && walkers::WalkerSpeed(12, -1.0f, 0, 0, 4) == 12,
	      "a multiplier of 0 or below - a word in the INI reads 0: the record's own");
	Check(walkers::WalkerSpeed(12, 1.0f, 30, 7, 4) == 16, "a slot past the last: the fastest");
	Check(walkers::WalkerSpeed(0, 1.0f, 30, 3, 4) == 0, "a record of Speed 0 stays so");
}

void TestSettings() {
	std::printf("The defaults\n");
	walkers::ReplaceSettings settings;
	Check(settings.enabled && settings.minCount == 1 && settings.maxCount == 3,
	      "[Replace] Enabled=1, MinCount=1, MaxCount=3");
	Check(walkers::kVariantSlots == 10 && walkers::kWalkersPerSpawnMost == 10,
	      "ten count slots per walker in the ESP's list, ten walkers the most");
}

}  // namespace

int main() {
	TestClassify();
	TestKeys();
	TestSection();
	TestInsert();
	TestFactsNote();
	TestCounts();
	TestHealth();
	TestSpeed();
	TestSettings();
	if (g_failures != 0) {
		std::printf("%d failed\n", g_failures);
		return 1;
	}
	std::printf("all passed\n");
	return 0;
}
