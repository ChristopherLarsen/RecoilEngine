/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 Data, the engine side (DataEngine.h).
//
// Reload: "/kt1 reload <system>" reads an unsynced system's file again at once; in a dev archive each unsynced system's
// file is also looked at every POLL_SECONDS and read when its text changed. A synced system keeps its values until the
// next game start. The command is an unsynced action and the look is an unsynced event client, both registered from here:
// no stock file has a hook for either.

#include "DataEngine.h"

#include "Reload.h"
#include "Game/IGameCommands.h"
#include "Game/KT1/KT1Registry.h"
#include "Game/UnsyncedActionExecutor.h"
#include "Game/UnsyncedGameCommands.h"
#include "Lua/LuaParser.h"
#include "System/EventHandler.h"
#include "System/FileSystem/FileHandler.h"
#include "System/FileSystem/VFSModes.h"
#include "System/Log/ILog.h"
#include "System/Misc/SpringTime.h"

namespace KT1::Data {

namespace {

Watch& Watched() {
	static Watch watch;
	return watch;
}

// Reads the rule's file now; true when the new values apply.
bool Apply(Rule& rule, const std::string& text) {
	const Report report = rule.ReadConfig(text, GameStrictness());
	Log(rule.Key(), report);
	if (!report.accepted) {
		LOG_L(L_WARNING, "[KT1] %s: %s not applied; the running values stay", rule.Info().name, PathFor(rule.Key()).c_str());
		return false;
	}
	rule.ConfigReloaded();
	LOG("[KT1] %s: %s reloaded", rule.Info().name, PathFor(rule.Key()).c_str());
	return true;
}

void Command(const std::string& args) {
	const std::string system = ReloadTarget(args);
	if (system.empty()) {
		LOG("[KT1] usage: /kt1 reload <system>");
		return;
	}
	Rule* rule = Registry::Get().Find(system.c_str());
	const bool enabled = rule != nullptr && rule->SwitchMode() != Rule::Mode::Off;
	switch (Judge(rule != nullptr, enabled, rule != nullptr && rule->Info().synced)) {
		case Verdict::Unknown: LOG("[KT1] reload: no system '%s'", system.c_str()); return;
		case Verdict::Off: LOG("[KT1] reload: %s is off in this game", rule->Info().name); return;
		case Verdict::NextStart: LOG("[KT1] reload: %s is synced; its file is read at the next game start", rule->Info().name); return;
		case Verdict::Live: break;
	}
	std::string text;
	if (!ReadText(rule->Key(), text)) {
		LOG_L(L_WARNING, "[KT1] reload: no %s in the game archive", PathFor(rule->Key()).c_str());
		return;
	}
	Watched().Seen(rule->Key(), text);
	Apply(*rule, text);
}

class ReloadAction : public IUnsyncedActionExecutor {
public:
	ReloadAction() : IUnsyncedActionExecutor("kt1", "KT1 engine systems: /kt1 reload <system> reads its config file again") {}

	bool Execute(const UnsyncedAction& action) const override {
		Command(action.GetArgs());
		return true;
	}
};

// Looks at the unsynced systems' files once per poll period (dev archive only).
class FilePoll : public CEventClient {
public:
	FilePoll() : CEventClient("[KT1DataPoll]", 199992, false) {
		autoLinkEvents = true;
		RegisterLinkedEvents(this);
		eventHandler.AddClient(this);
	}

	void Update() override {
		if (!clock.Due(spring_gettime().toSecsf())) return;
		for (const auto& rule : Registry::Get().All()) {
			if (rule->Info().synced || rule->SwitchMode() == Rule::Mode::Off) continue;
			std::string text;
			if (ReadText(rule->Key(), text) && Watched().Changed(rule->Key(), text)) Apply(*rule, text);
		}
	}

private:
	PollClock clock;
};

std::string LuaPathFor(const std::string& system) { return "gamedata/kt1/" + system + ".lua"; }

bool HasLua(const std::string& system) { return CFileHandler::FileExists(LuaPathFor(system), SPRING_VFS_MOD_BASE); }

// Hands the rule its .lua table; false, with `error`, when the file is missing or fails (the rule reads its defaults).
bool ReadLua(Rule& rule, std::string& error) {
	// setup {true}: the parser needs its own Lua state
	LuaParser parser(LuaPathFor(rule.Key()), SPRING_VFS_MOD_BASE, SPRING_VFS_ZIP, {rule.Info().synced}, {true});
	const bool ok = parser.Execute();
	rule.LoadConfig(parser.GetRoot());   // an invalid root reads every field as its default
	if (!ok) error = parser.GetErrorLog();
	return ok;
}

void LoadJson(Rule& rule, const char* modeName) {
	std::string text;
	const bool found = ReadText(rule.Key(), text);
	Watched().Seen(rule.Key(), found ? text : std::string());   // a file that appears later is a change too
	// "{}" when there is no file: the rule still derives its values from the defaults
	const Report report = rule.ReadConfig(found ? text : std::string("{}"), GameStrictness());
	Log(rule.Key(), report);
	const char* note = !found ? " (none: defaults)" : (report.accepted ? "" : " (rejected: defaults)");
	LOG("[KT1] %s v%d %s, config %s%s mode=%s", rule.Info().name, rule.Info().version,
	    rule.Info().synced ? "synced" : "unsynced", PathFor(rule.Key()).c_str(), note, modeName);
}

void Differs(ConfigCheck& check, const std::string& what, const std::string& lua, const std::string& json) {
	++check.diffs;
	LOG_L(L_WARNING, "[KT1CHECK-DIFF] JsonConfig %s lua=%s json=%s", what.c_str(), lua.c_str(), json.c_str());
}

} // namespace

Strictness GameStrictness() {
	return CFileHandler::FileExists(PathFor("dev_data"), SPRING_VFS_MOD) ? Strictness::Dev : Strictness::Release;
}

bool ReadText(const std::string& system, std::string& text) {
	CFileHandler file(PathFor(system), SPRING_VFS_MOD);
	if (!file.FileExists()) return false;
	text.clear();
	return file.LoadStringData(text);
}

void Log(const std::string& system, const Report& report) {
	for (const Issue& issue : report.issues) {
		if (issue.error) LOG_L(L_ERROR, "[KT1] %s", Line(system, issue).c_str());
		else LOG_L(L_WARNING, "[KT1] %s", Line(system, issue).c_str());
	}
}

void LoadRule(Rule& rule, Source source, const char* modeName) {
	std::string text;
	// a rule written after the .lua configs has only a .json file
	if (source == Source::Json || (!HasLua(rule.Key()) && ReadText(rule.Key(), text))) return LoadJson(rule, modeName);
	std::string error;
	const bool ok = ReadLua(rule, error);
	LOG("[KT1] %s v%d %s, config %s%s%s mode=%s", rule.Info().name, rule.Info().version,
	    rule.Info().synced ? "synced" : "unsynced", LuaPathFor(rule.Key()).c_str(), ok ? "" : " (none: defaults): ",
	    error.c_str(), modeName);
}

void CheckRule(Rule& rule, ConfigCheck& check) {
	const std::string& key = rule.Key();
	std::string text;
	const bool hasJson = ReadText(key, text);
	if (!HasLua(key)) {
		if (hasJson) LOG("[KT1] JsonConfig: %s has no .lua config; nothing to compare", rule.Info().name);
		return;
	}
	if (!hasJson) return Differs(check, key + ".file", LuaPathFor(key), "absent");

	const Report report = rule.ReadConfig(text, GameStrictness());
	Log(key, report);
	Values json;
	rule.ConfigValues(json);
	if (!report.accepted || report.HasErrors()) Differs(check, key + ".file", "read", "has errors");

	std::string error;
	if (!ReadLua(rule, error)) Differs(check, key + ".file", "failed: " + error, "read");
	Values lua;
	rule.ConfigValues(lua);

	const Comparison c = Compare(lua, json);
	++check.configs;
	check.compared += static_cast<unsigned>(c.compared);
	for (const Difference& d : c.diffs) Differs(check, key + "." + d.key, d.lua, d.json);
}

void LogCheck(const ConfigCheck& check) {
	LOG("[KT1CHECK] JsonConfig f=0 n=%u diffs=%u", check.compared, check.diffs);
	LOG("[KT1] JsonConfig: compared %u configs", check.configs);
}

void StartReload() {
	// the engine's command table owns and destroys the action; the poll lives as long as the process
	if (unsyncedGameCommands != nullptr && unsyncedGameCommands->GetActionExecutor("kt1") == nullptr)
		unsyncedGameCommands->AddActionExecutor(new ReloadAction());
	static FilePoll* poll = nullptr;
	if (poll == nullptr && GameStrictness() == Strictness::Dev) poll = new FilePoll();
}

} // namespace KT1::Data
