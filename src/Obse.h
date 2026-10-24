#pragma once

// Minimal replicas of xOBSE's plugin interfaces (obse/PluginAPI.h, read in
// llde/xOBSE on 2026-10-23): only the layouts, binary compatible, nothing of
// the game object tree. The plugin does not include the xOBSE headers for the
// same reason OBVR and Oblivion Reloaded do not: they drag in everything and
// tie the build to MSVC.

#include "Types.h"

namespace walkers::obse {

// OBSEInterface, as xOBSE hands it to OBSEPlugin_Query and OBSEPlugin_Load.
struct Interface {
	UInt32 obseVersion;
	UInt32 oblivionVersion;
	UInt32 editorVersion;
	UInt32 isEditor;
	bool (*RegisterCommand)(void* info);
	void (*SetOpcodeBase)(UInt32 opcode);
	void* (*QueryInterface)(UInt32 id);
	UInt32 (*GetPluginHandle)();  // from OBSE 15
	bool (*RegisterTypedCommand)(void* info, UInt32 returnType);  // from OBSE 18
	const char* (*GetOblivionDirectory)();
	bool (*GetPluginLoaded)(const char* pluginName);  // from OBSE 21
	UInt32 (*GetPluginVersion)(const char* pluginName);
};

// PluginInfo; xOBSE refuses an infoVersion below 2, 3 is the current one.
struct PluginInfo {
	enum { kInfoVersion = 3 };
	UInt32 infoVersion;
	const char* name;
	UInt32 version;
};

// QueryInterface ids (PluginAPI.h's enum, in its order).
enum : UInt32 {
	kInterface_Console = 0,
	kInterface_Serialization = 1,
	kInterface_StringVar = 2,
	kInterface_IO = 3,
	kInterface_Messaging = 4,
	kInterface_ArrayVar = 5,
	kInterface_CommandTable = 6,
	kInterface_Script = 7,
	kInterface_Tasks = 8,
	kInterface_Input = 9,
	kInterface_EventManager = 10,
	kInterface_Tasks2 = 11,
};

// OBSESerializationInterface (version 1): the co-save beside the .ess, one
// typed and versioned record after another, per plugin.
struct SerializationInterface {
	using EventCallback = void (*)(void* reserved);
	UInt32 version;
	void (*SetSaveCallback)(UInt32 plugin, EventCallback callback);
	void (*SetLoadCallback)(UInt32 plugin, EventCallback callback);
	void (*SetNewGameCallback)(UInt32 plugin, EventCallback callback);
	bool (*WriteRecord)(UInt32 type, UInt32 version, const void* buf, UInt32 length);
	bool (*OpenRecord)(UInt32 type, UInt32 version);
	bool (*WriteRecordData)(const void* buf, UInt32 length);
	bool (*GetNextRecordInfo)(UInt32* type, UInt32* version, UInt32* length);
	UInt32 (*ReadRecordData)(void* buf, UInt32 length);
	bool (*ResolveRefID)(UInt32 refID, UInt32* outRefID);
	void (*SetPreloadCallback)(UInt32 plugin, EventCallback callback);
	bool (*PeekNextRecordInfo)(UInt32* type, UInt32* version, UInt32* length);
};

// OBSEConsoleInterface (version 2 for RunScriptLine2): a console line run
// by xOBSE, with a reference as its "this" - the plugin's way to xOBSE's own
// commands, Update3D among them.
struct ConsoleInterface {
	UInt32 version;
	bool (*RunScriptLine)(const char* text);
	bool (*RunScriptLine2)(const char* text, void* thisObj, bool silent);
};

// OBSETasksInterface: a task enqueued here runs once every main-loop pass
// (xOBSE's TaskManager::Run keeps it until RemoveTask) - the plugin's frame
// tick, with no byte of the engine patched.
struct TasksInterface {
	void* (*EnqueueTask)(void (*task)());
	void (*RemoveTask)(void* task);
	bool (*IsTaskPresent)(void* task);
};

// OBSEEventManagerInterface: native handlers for the game events xOBSE
// raises ("OnDeath": arg0 the fallen reference, arg1 the killer or null,
// thisObj the fallen again). The three pointers at registration are filters;
// null takes every source.
struct EventManagerInterface {
	using EventFunc = void (*)(void* arg0, void* arg1, void* thisObj);
	bool (*DispatchEvent)(const char* eventName, const char* senderName, UInt32 argsArrayId);
	bool (*RegisterEvent)(const char* eventName, EventFunc func, void* arg0, void* arg1, void* refr);
	bool (*UnregisterEvent)(const char* eventName, EventFunc func, void* arg0, void* arg1, void* refr);
	bool (*IsEventRegistered)(const char* eventName, EventFunc func, void* arg0, void* arg1, void* refr);
	void* (*GetGameHandleAddress)();
	bool (*RegisterEventNative)(void* info);
};

}  // namespace walkers::obse
