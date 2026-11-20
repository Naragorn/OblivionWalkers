// Every flow of setting one INI key (docs/walkers-spec.md 4.24a): the key
// there and replaced, the key missing from its section, the section missing,
// the file's own line ends, the comments, blank lines and other sections
// untouched byte for byte, the case aside, a buffer too small.

#include "IniEditLogic.h"

#include <cstdio>
#include <string>

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf("  %s  %s\n", condition ? "ok  " : "FAIL", what);
	if (!condition) {
		++g_failures;
	}
}

std::string Set(const std::string& text, const char* section, const char* key, const char* value) {
	std::string out(text.size() + 256, '\0');
	const UInt32 used = walkers::SetIniKey(text.data(), static_cast<UInt32>(text.size()), section, key, value, out.data(),
	                                       static_cast<UInt32>(out.size()));
	if (used == 0) {
		return "<refused>";
	}
	out.resize(used);
	return out;
}

void TestReplace() {
	std::printf("The key is there\n");
	const std::string file = "[Rise]\nEnabled=1\n\n[Walkers]\n; Who comes.\nMode=1\nWalkOnly=1\n\n[Daedra]\nMode=0\n";
	Check(Set(file, "Walkers", "Mode", "2") == "[Rise]\nEnabled=1\n\n[Walkers]\n; Who comes.\nMode=2\nWalkOnly=1\n\n[Daedra]\nMode=0\n",
	      "the value replaced, every other byte as it was");
	Check(Set(file, "Daedra", "Mode", "1") == "[Rise]\nEnabled=1\n\n[Walkers]\n; Who comes.\nMode=1\nWalkOnly=1\n\n[Daedra]\nMode=1\n",
	      "the same key name in another section: that section's");
	Check(Set(file, "walkers", "mode", "0") == "[Rise]\nEnabled=1\n\n[Walkers]\n; Who comes.\nMode=0\nWalkOnly=1\n\n[Daedra]\nMode=0\n",
	      "section and key matched without regard to case");
	Check(Set("[A]\nMode = 1  \n", "A", "Mode", "2") == "[A]\nMode = 2  \n",
	      "blanks around the '=' and after the value kept");
	Check(Set("[A]\r\nMode=1\r\nOther=3\r\n", "A", "Mode", "0") == "[A]\r\nMode=0\r\nOther=3\r\n", "carriage returns kept");
	Check(Set("[A]\nMode=1", "A", "Mode", "2") == "[A]\nMode=2", "the last line without a line end");
	Check(Set("[A]\nMode=1\nMode=5\n", "A", "Mode", "2") == "[A]\nMode=2\nMode=5\n",
	      "twice in a section: the first, as the profile API reads it");
	Check(Set("[A]\n; Mode=1\nMode=4\n", "A", "Mode", "2") == "[A]\n; Mode=1\nMode=2\n", "a comment that looks like the key is skipped");
	Check(Set("[A]\nMode=1\n", "A", "Mode", "10") == "[A]\nMode=10\n" && Set("[A]\nMode=10\n", "A", "Mode", "1") == "[A]\nMode=1\n",
	      "a longer and a shorter value");
	Check(Set("[A]\nMode=1\n", "A", "Mode", "1") == "[A]\nMode=1\n", "the same value: the file as it was");
	Check(Set("[A]\nModeX=1\nMode=4\n", "A", "Mode", "2") == "[A]\nModeX=1\nMode=2\n", "a key that only starts the same is not it");
}

void TestInsert() {
	std::printf("The key is missing\n");
	Check(Set("[A]\nX=1\n\n[B]\nY=2\n", "A", "Mode", "2") == "[A]\nX=1\nMode=2\n\n[B]\nY=2\n",
	      "behind the section's last key line");
	Check(Set("[A]\nX=1\n; note\nZ=3\n\n[B]\n", "A", "Mode", "2") == "[A]\nX=1\n; note\nZ=3\nMode=2\n\n[B]\n",
	      "behind the last key, comments between kept");
	Check(Set("[A]\n; only a comment\n\n[B]\nY=2\n", "A", "Mode", "2") == "[A]\n; only a comment\nMode=2\n\n[B]\nY=2\n",
	      "no key in the section: behind the comments under its header");
	Check(Set("[A]\n\n[B]\n", "A", "Mode", "2") == "[A]\nMode=2\n\n[B]\n", "an empty section: under its header");
	Check(Set("[A]\r\nX=1\r\n[B]\r\n", "A", "Mode", "2") == "[A]\r\nX=1\r\nMode=2\r\n[B]\r\n", "with the file's carriage returns");
	Check(Set("[A]\nX=1", "A", "Mode", "2") == "[A]\nX=1\nMode=2\n", "the last line without a line end: one added first");
	Check(Set("[A]\nX=1\n[B]\nMode=9\n", "A", "Mode", "2") == "[A]\nX=1\nMode=2\n[B]\nMode=9\n",
	      "the key is in another section only: not taken from there");
}

void TestSection() {
	std::printf("The section is missing\n");
	Check(Set("[A]\nX=1\n", "Onboarding", "Show", "0") == "[A]\nX=1\n[Onboarding]\nShow=0\n", "added at the file's end");
	Check(Set("[A]\r\nX=1\r\n", "Onboarding", "Show", "0") == "[A]\r\nX=1\r\n[Onboarding]\r\nShow=0\r\n",
	      "with the file's carriage returns");
	Check(Set("[A]\nX=1", "Onboarding", "Show", "0") == "[A]\nX=1\n[Onboarding]\nShow=0\n", "no line end at the end: one added");
	Check(Set("", "Onboarding", "Show", "0") == "[Onboarding]\r\nShow=0\r\n", "an empty file: the section and the key, a carriage return and line feed");
	Check(Set("Mode=1\n[A]\n", "Mode", "Mode", "2") == "Mode=1\n[A]\n[Mode]\nMode=2\n",
	      "a key before any section belongs to no section");
}

void TestSeveral() {
	std::printf("Three keys one after the other, as the onboarding writes them\n");
	std::string file = "[Walkers]\r\n; who\r\nMode=1\r\n\r\n[Daedra]\r\nMode=0\r\nSharePercent=50\r\n";
	file = Set(file, "Walkers", "Mode", "2");
	file = Set(file, "Daedra", "Mode", "1");
	file = Set(file, "Onboarding", "Show", "0");
	Check(file == "[Walkers]\r\n; who\r\nMode=2\r\n\r\n[Daedra]\r\nMode=1\r\nSharePercent=50\r\n[Onboarding]\r\nShow=0\r\n",
	      "the file with the two switches changed and Show=0 added, the rest byte for byte");
}

void TestRefuse() {
	std::printf("No room\n");
	char out[8];
	const std::string text = "[A]\nMode=1\n";
	Check(walkers::SetIniKey(text.data(), static_cast<UInt32>(text.size()), "A", "Mode", "2", out, sizeof(out)) == 0,
	      "a buffer too small for a replaced value: nothing");
	Check(walkers::SetIniKey(text.data(), static_cast<UInt32>(text.size()), "B", "Mode", "2", out, sizeof(out)) == 0,
	      "a buffer too small for a new section: nothing");
	Check(walkers::SetIniKey(text.data(), static_cast<UInt32>(text.size()), "A", "Other", "2", out, sizeof(out)) == 0,
	      "a buffer too small for a new key: nothing");
}

}  // namespace

int main() {
	TestReplace();
	TestInsert();
	TestSection();
	TestSeveral();
	TestRefuse();
	if (g_failures != 0) {
		std::printf("%d failed\n", g_failures);
		return 1;
	}
	std::printf("all passed\n");
	return 0;
}
