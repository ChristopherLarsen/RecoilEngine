/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 Data, the engine side: reads data/kt1/<system>.json from the game archive, logs what the loader found, and reloads
// unsynced systems while a game runs (game-build docs/engine/rules/data.md). The only file pair of Data/ that includes
// engine headers.
//
// Dev or release: a game archive that holds data/kt1/dev_data.json is a dev archive (release packages strip dev_ files).
// Every client plays the same archive, so every client reads a synced config the same way.

#ifndef KT1_DATA_ENGINE_H
#define KT1_DATA_ENGINE_H

#include <string>

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

// At game start: hands the rule its file (or "{}" when there is none), logs the result, and remembers the text for reload.
void LoadRule(Rule& rule, const char* modeName);

// Once, at the first frame: the "/kt1 reload <system>" command and, in a dev archive, the look at the files.
void StartReload();

} // namespace KT1::Data

#endif
