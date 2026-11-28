/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 Data: which config file the rules read (mod option kt1_jsonconfig), and the compare of the two readers. Engine-free.

#ifndef KT1_DATA_CHECK_H
#define KT1_DATA_CHECK_H

#include <cstddef>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace KT1::Data {

// "0" (and unset): the rules read gamedata/kt1/<rule>.lua. "1": data/kt1/<rule>.json. "check": both are read at game
// start, every value is compared bit for bit, and the Lua values are used.
enum class Source { Lua, Json, Check };
Source ParseSource(std::string_view option);   // "1" or "true": Json; "check": Check; anything else: Lua
const char* SourceName(Source source);         // "0", "1", "check"

// The values a rule runs on, by name (Rule::ConfigValues).
class Values {
public:
	void Add(const std::string& key, bool value);
	void Add(const std::string& key, int value);
	void Add(const std::string& key, float value);   // kept with its bits
	void Add(const std::string& key, const std::string& value);
	void Add(const std::string& key, const char* value) { Add(key, std::string(value)); }
	void Add(const std::string& key, const float* values, std::size_t count);   // key[1], key[2], ...

	const std::vector<std::pair<std::string, std::string>>& All() const { return all; }

private:
	std::vector<std::pair<std::string, std::string>> all;
};

struct Difference { std::string key, lua, json; };

struct Comparison {
	std::size_t compared = 0;          // keys either side has
	std::vector<Difference> diffs;     // a value that differs, or "absent" on one side
};

Comparison Compare(const Values& lua, const Values& json);

} // namespace KT1::Data

#endif
