#include "PluginEntry.h"

namespace walkers {

namespace {

Settings g_settings;

}  // namespace

bool Query(const obse::Interface& xse, obse::PluginInfo* info, const Host& host) {
	info->infoVersion = obse::PluginInfo::kInfoVersion;
	info->name = kPluginName;
	info->version = kPluginVersion;
	host.resetLog();
	host.log("%s %u - xOBSE %u, Oblivion %08X, editor=%u", kPluginName, kPluginVersion, xse.obseVersion,
	         xse.oblivionVersion, xse.isEditor);
	return true;
}

bool Load(const obse::Interface& xse, const Host& host) {
	if (xse.isEditor != 0) {
		host.log("Editor: nothing to do");
		return true;
	}
	if (xse.oblivionVersion != kOblivionVersion_1_2_416) {
		host.log("Oblivion %08X is not 1.2.416 (%08X): the engine addresses are for 1.2.416 only, nothing done",
		         xse.oblivionVersion, kOblivionVersion_1_2_416);
		return true;
	}
	if (xse.obseVersion < kObseVersionLeast) {
		host.log("xOBSE %u is older than %u: no event manager, no tasks, nothing done", xse.obseVersion,
		         kObseVersionLeast);
		return true;
	}
	g_settings = Settings{};
	if (!host.readSettings(&g_settings)) {
		host.log("No OblivionWalkers.ini next to the DLL - defaults: Rise Enabled=1 AfterSeconds=20 "
		         "JitterSeconds=10 ChancePercent=100 OnlyGeneric=1");
	}
	const RiseSettings& rise = g_settings.rise;
	const ReplaceSettings& replace = g_settings.replace;
	if (!host.installRise(xse, g_settings)) {
		host.log("Rise: not installed, the fallen stay down");
		return true;
	}
	if (!rise.enabled) {
		host.log("Rise: off (Enabled=0), the fallen stay down");
	} else {
		const char* how = "";
		if (rise.asCreature) {
			how = " - as a walker, the ESP's zombie creature at his spot";
		} else if (rise.cloneBase) {
			how = rise.zombieRace ? ", on a clone of his base record with the Walker race"
			                      : ", on a clone of his base record";
		}
		host.log("Rise: watching deaths - a fallen %s%s gets up %.0f to %.0f s later, %u%% of the time%s",
		         rise.onlyGeneric ? "generic person" : "person",
		         rise.onlyHumans ? " of a human race" : (rise.onlyPlayableRaces ? " of a playable race" : ""),
		         rise.afterSeconds, rise.afterSeconds + rise.jitterSeconds, rise.chancePercent, how);
	}
	if (replace.enabled) {
		host.log("Spawns: the creatures of the leveled lists become %u to %u walkers, as [ReplaceCreatures] says",
		         SpreadCount(replace.minCount, replace.maxCount, 0, kVariantSlots),
		         SpreadCount(replace.minCount, replace.maxCount, kVariantSlots - 1, kVariantSlots));
	} else {
		host.log("Spawns: off (Enabled=0), the lists as they are");
	}
	host.log("Walkers: %u%% health, %s, %s", ScaledHealth(100, rise.healthPercent),
	         rise.walkOnly ? "walk only" : "they run", rise.noLoot ? "no loot" : "lootable");
	return true;
}

const Settings& LoadedSettings() {
	return g_settings;
}

void ResetForTest() {
	g_settings = Settings{};
}

}  // namespace walkers
