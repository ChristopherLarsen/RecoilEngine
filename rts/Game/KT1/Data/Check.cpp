/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 engine-free: also built into kt1_core_tests (KT1/Test/README.md).
// KT1 Data: the config switch and the compare (Check.h). Engine-free.

#include "Check.h"

#include <algorithm>
#include <bit>
#include <cctype>
#include <cstdint>
#include <cstdio>

namespace KT1::Data {

Source ParseSource(std::string_view option) {
	std::string v(option);
	std::transform(v.begin(), v.end(), v.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	if (v == "1" || v == "true") return Source::Json;
	return v == "check" ? Source::Check : Source::Lua;
}

const char* SourceName(Source source) {
	return source == Source::Json ? "1" : (source == Source::Check ? "check" : "0");
}

void Values::Add(const std::string& key, bool value) { all.emplace_back(key, value ? "true" : "false"); }
void Values::Add(const std::string& key, int value) { all.emplace_back(key, std::to_string(value)); }
void Values::Add(const std::string& key, const std::string& value) { all.emplace_back(key, "\"" + value + "\""); }

void Values::Add(const std::string& key, float value) {
	char text[48];
	std::snprintf(text, sizeof(text), "%.9g (0x%08x)", static_cast<double>(value),
	              static_cast<unsigned>(std::bit_cast<std::uint32_t>(value)));
	all.emplace_back(key, text);
}

void Values::Add(const std::string& key, const float* values, std::size_t count) {
	for (std::size_t i = 0; i < count; ++i) Add(key + "[" + std::to_string(i + 1) + "]", values[i]);
}

static const std::string* Find(const Values& values, const std::string& key) {
	for (const auto& entry : values.All())
		if (entry.first == key) return &entry.second;
	return nullptr;
}

Comparison Compare(const Values& lua, const Values& json) {
	Comparison c;
	for (const auto& entry : lua.All()) {
		++c.compared;
		const std::string* other = Find(json, entry.first);
		if (other == nullptr) c.diffs.push_back({entry.first, entry.second, "absent"});
		else if (*other != entry.second) c.diffs.push_back({entry.first, entry.second, *other});
	}
	for (const auto& entry : json.All()) {
		if (Find(lua, entry.first) != nullptr) continue;
		++c.compared;
		c.diffs.push_back({entry.first, "absent", entry.second});
	}
	return c;
}

} // namespace KT1::Data
