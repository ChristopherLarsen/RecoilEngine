/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 Data: hot reload decisions. Engine-free; the engine reads the files and applies the values.
//
// An unsynced system takes a changed file at once: on "/kt1 reload <system>", and in a dev game by itself when the
// file's text changes. A synced system reads its file at game start only: a change in a running game would desync it.
// Times here are wall-clock seconds, so nothing depends on the sim rate or the frame rate.

#ifndef KT1_DATA_RELOAD_H
#define KT1_DATA_RELOAD_H

#include <map>
#include <string>
#include <string_view>

namespace KT1::Data {

// DESIGN: seconds between two looks at the config files in a dev game; half of the one second a change may take to show.
constexpr double POLL_SECONDS = 0.5;

class PollClock {
public:
	// True when POLL_SECONDS have passed since the last true (and on the first call).
	bool Due(double nowSeconds);

private:
	double next = 0.0;
	bool started = false;
};

// Remembers each watched file's text, so each new content is read once. Comparing the text (a few KB a file) sees two
// saves within one second, which a file time in whole seconds would not.
class Watch {
public:
	void Seen(const std::string& system, std::string_view text);      // the text that was loaded
	bool Changed(const std::string& system, std::string_view text);   // true once for each new text of a seen file
	void Clear() { texts.clear(); }

private:
	std::map<std::string, std::string> texts;
};

enum class Verdict {
	Live,        // read it now
	NextStart,   // synced: the running game keeps its values
	Off,         // the system is switched off in this game
	Unknown,     // no such system
};
Verdict Judge(bool known, bool enabled, bool synced);

// The words after "/kt1": "reload <system>". Returns the system in lower case, or "" for anything else.
std::string ReloadTarget(std::string_view args);

} // namespace KT1::Data

#endif
