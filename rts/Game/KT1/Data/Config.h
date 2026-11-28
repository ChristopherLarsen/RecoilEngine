/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 Data: a system's JSON config (game archive data/kt1/<system>.json) read into its typed struct. Engine-free; the
// engine reads the file text and calls this.

#ifndef KT1_DATA_CONFIG_H
#define KT1_DATA_CONFIG_H

#include <functional>
#include <string>
#include <string_view>
#include <utility>

#include "Report.h"
#include "Schema.h"

namespace KT1::Data {

// "orders" -> "data/kt1/orders.json", the path inside the game archive.
std::string PathFor(std::string_view system);

// Reads `text` as one JSON object and lets `describe` name its keys. Sets report.accepted; the caller applies the values
// only then.
Report Parse(std::string_view text, Strictness strictness, const std::function<void(Schema&)>& describe);

// Reads `text` into a fresh T (its defaults), and replaces `target` with it when the file is accepted. A rejected file
// leaves `target` as it was: the defaults at game start, the running values on a reload.
template <class T> Report ParseInto(std::string_view text, Strictness strictness, T& target) {
	T fresh{};
	Report report = Parse(text, strictness, [&fresh](Schema& s) { fresh.Describe(s); });
	if (report.accepted) target = std::move(fresh);
	return report;
}

// One log line per issue: "data/kt1/orders.json: heavy.extras: expected a whole number, got a string".
std::string Line(std::string_view system, const Issue& issue);

} // namespace KT1::Data

#endif
