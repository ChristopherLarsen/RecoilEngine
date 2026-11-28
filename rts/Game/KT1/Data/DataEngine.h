/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 Data, the engine side: reads a rule's config from the game archive (gamedata/kt1/<system>.lua or
// data/kt1/<system>.json, by mod option kt1_jsonconfig: Check.h), logs what the loader found, and reloads unsynced
// systems that read JSON while a game runs (game-build docs/engine/rules/data.md). The only file pair of Data/ that
// includes engine headers.
//
// Dev or release: a game archive that holds data/kt1/dev_data.json is a dev archive (release packages strip dev_ files).
// Every client plays the same archive, so every client reads a synced config the same way.

#ifndef KT1_DATA_ENGINE_H
#define KT1_DATA_ENGINE_H

#include <string>

#include "Check.h"
#include "Config.h"

namespace KT1 {
	class Rule;
}

namespace KT1::Data {

Strictness GameStrictness();

// The text of the system's config file; false when the game archive has none.
bool ReadText(const std::string& system, std::string& text);

// One log line per issue.
void Log(const std::string& system, const Report& report);

// A typed struct with its defaults, overlaid by data/kt1/<system>.json when the file is there and accepted.
template <class T> T Load(const std::string& system) {
	T value{};
	std::string text;
	if (ReadText(system, text)) Log(system, ParseInto(text, GameStrictness(), value));
	return value;
}

// At game start: hands the rule its config and logs the result. Json: its .json text, remembered for reload. Lua and
// Check: its .lua table (empty when there is none). A rule that has only the other file is given that one.
void LoadRule(Rule& rule, Source source, const char* modeName);

// Check: reads both of the rule's files, compares every value (Rule::ConfigValues), logs each difference and leaves the
// rule holding the Lua values. A rule with only one of the files is not compared (the count of configs says how many were).
struct ConfigCheck { unsigned compared = 0, diffs = 0, configs = 0; };
void CheckRule(Rule& rule, ConfigCheck& check);
// "[KT1CHECK] JsonConfig f=0 n=<values compared> diffs=<k>", then how many configs that covered.
void LogCheck(const ConfigCheck& check);

// Once, at the first frame of a game that reads JSON: the "/kt1 reload <system>" command and, in a dev archive, the look
// at the files.
void StartReload();

} // namespace KT1::Data

#endif
