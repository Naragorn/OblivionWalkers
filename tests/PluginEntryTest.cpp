// Every flow of Query and Load, with the side effects replaced by fakes.

#include "PluginEntry.h"

#include <cstdarg>
#include <cstdio>
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

struct Fake {
	int resets = 0;
	bool iniExists = true;
	walkers::Settings iniSettings;
	int installs = 0;
	bool installSucceeds = true;
	walkers::RiseSettings installed;
	std::vector<std::string> lines;

	bool Logged(const char* text) const {
		for (const auto& line : lines) {
			if (line.find(text) != std::string::npos) {
				return true;
			}
		}
		return false;
	}
};
Fake* g_fake = nullptr;

void FakeReset() { ++g_fake->resets; }
void FakeLog(const char* format, ...) {
	char buffer[512];
	va_list args;
	va_start(args, format);
	std::vsnprintf(buffer, sizeof(buffer), format, args);
	va_end(args);
	g_fake->lines.emplace_back(buffer);
}
bool FakeReadSettings(walkers::Settings* settings) {
	if (!g_fake->iniExists) {
		return false;
	}
	*settings = g_fake->iniSettings;
	return true;
}
bool FakeInstallRise(const walkers::obse::Interface&, const walkers::RiseSettings& rise) {
	++g_fake->installs;
	g_fake->installed = rise;
	return g_fake->installSucceeds;
}

const walkers::Host kHost{&FakeReset, &FakeLog, &FakeReadSettings, &FakeInstallRise};

walkers::obse::Interface Game() {
	walkers::obse::Interface xse{};
	xse.obseVersion = 22;
	xse.oblivionVersion = walkers::kOblivionVersion_1_2_416;
	return xse;
}

void TestQuery() {
	std::printf("Query\n");
	Fake fake;
	g_fake = &fake;
	walkers::obse::PluginInfo info{};
	Check(walkers::Query(Game(), &info, kHost), "says yes");
	Check(info.infoVersion == 3 && std::string(info.name) == "OblivionWalkers" && info.version == 1,
	      "names the plugin with info version 3");
	Check(fake.resets == 1 && fake.Logged("OblivionWalkers 1 - xOBSE 22"), "starts the log with the versions");
}

void TestLoad() {
	std::printf("Load\n");
	{
		Fake fake;
		g_fake = &fake;
		walkers::ResetForTest();
		walkers::obse::Interface editor = Game();
		editor.isEditor = 1;
		Check(walkers::Load(editor, kHost) && fake.installs == 0 && fake.Logged("Editor"), "in the editor: nothing");
	}
	{
		Fake fake;
		g_fake = &fake;
		walkers::ResetForTest();
		walkers::obse::Interface other = Game();
		other.oblivionVersion = 0x010201A1;
		Check(walkers::Load(other, kHost) && fake.installs == 0 && fake.Logged("is not 1.2.416"),
		      "another Oblivion: nothing, and the log says so");
	}
	{
		Fake fake;
		g_fake = &fake;
		walkers::ResetForTest();
		walkers::obse::Interface old = Game();
		old.obseVersion = 20;
		Check(walkers::Load(old, kHost) && fake.installs == 0 && fake.Logged("older than 21"),
		      "an xOBSE before 21: nothing, and the log says so");
	}
	{
		Fake fake;
		g_fake = &fake;
		fake.iniExists = false;
		walkers::ResetForTest();
		Check(walkers::Load(Game(), kHost) && fake.installs == 1, "no INI: the defaults, the rise installed");
		Check(fake.Logged("No OblivionWalkers.ini") && fake.installed.enabled && fake.installed.afterSeconds == 20.0f &&
		          fake.installed.jitterSeconds == 10.0f && fake.installed.chancePercent == 100 &&
		          fake.installed.onlyGeneric,
		      "with the defaults named in the log");
		Check(fake.Logged("Rise: watching deaths - a fallen generic person of a playable race gets up 20 to 30 s "
		                  "later, 100% of the time - as a walker, the ESP's zombie creature at his spot") &&
		          fake.installed.onlyPlayableRaces && fake.installed.asCreature && fake.installed.dropSeconds == 1.5f,
		      "and the rise's line, the creature path by default");
	}
	{
		Fake fake;
		g_fake = &fake;
		fake.iniSettings.rise.afterSeconds = 5.0f;
		fake.iniSettings.rise.jitterSeconds = 0.0f;
		fake.iniSettings.rise.chancePercent = 40;
		fake.iniSettings.rise.onlyGeneric = false;
		fake.iniSettings.rise.onlyPlayableRaces = false;
		fake.iniSettings.rise.asCreature = false;
		walkers::ResetForTest();
		Check(walkers::Load(Game(), kHost) && fake.installs == 1 && fake.installed.afterSeconds == 5.0f &&
		          fake.installed.chancePercent == 40 && !fake.installed.onlyGeneric &&
		          !fake.installed.onlyPlayableRaces,
		      "an INI: its values reach the installer");
		Check(fake.Logged("a fallen person gets up 5 to 5 s later, 40% of the time") &&
		          !fake.Logged("on a clone") && !fake.Logged("as a walker") && !fake.installed.asCreature &&
		          walkers::LoadedSettings().rise.chancePercent == 40,
		      "and the log, and LoadedSettings; AsCreature=0 reaches the installer");
	}
	{
		Fake fake;
		g_fake = &fake;
		fake.iniSettings.rise.onlyHumans = true;
		walkers::ResetForTest();
		Check(walkers::Load(Game(), kHost) && fake.installs == 1 && fake.installed.onlyHumans &&
		          fake.Logged("a fallen generic person of a human race gets up 20 to 30 s later"),
		      "OnlyHumans=1: reaches the installer, named in the log over the playable races");
	}
	{
		Fake fake;
		g_fake = &fake;
		fake.iniSettings.rise.cloneBase = true;
		fake.iniSettings.rise.asCreature = false;
		walkers::ResetForTest();
		Check(walkers::Load(Game(), kHost) && fake.installs == 1 && fake.installed.cloneBase &&
		          fake.installed.zombieRace &&
		          fake.Logged("of a playable race gets up 20 to 30 s later, 100% of the time, on a clone of his "
		                      "base record with the Walker race"),
		      "CloneBase=1 with the default ZombieRace=1: reaches the installer, both named in the log");
	}
	{
		Fake fake;
		g_fake = &fake;
		fake.iniSettings.rise.cloneBase = true;
		fake.iniSettings.rise.zombieRace = false;
		fake.iniSettings.rise.asCreature = false;
		walkers::ResetForTest();
		Check(walkers::Load(Game(), kHost) && fake.installs == 1 && !fake.installed.zombieRace &&
		          fake.Logged("on a clone of his base record") && !fake.Logged("Walker race"),
		      "ZombieRace=0: the clone without the race, the log says only the clone");
	}
	{
		Fake fake;
		g_fake = &fake;
		fake.iniSettings.rise.enabled = false;
		walkers::ResetForTest();
		Check(walkers::Load(Game(), kHost) && fake.installs == 0 && fake.Logged("Rise: off (Enabled=0)"),
		      "Enabled=0: not installed, and the log says so");
	}
	{
		Fake fake;
		g_fake = &fake;
		fake.installSucceeds = false;
		walkers::ResetForTest();
		Check(walkers::Load(Game(), kHost) && fake.installs == 1 && fake.Logged("Rise: not installed"),
		      "the installer refusing: Load still says yes, the log says what is missing");
	}
}

}  // namespace

int main() {
	TestQuery();
	TestLoad();
	if (g_failures != 0) {
		std::printf("%d failed\n", g_failures);
		return 1;
	}
	std::printf("all passed\n");
	return 0;
}
