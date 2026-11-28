/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 Data: parsing a config file (Config.h). Engine-free.

#include "Config.h"

#include "simdjson.h"

namespace KT1::Data {

std::string PathFor(std::string_view system) { return "data/kt1/" + std::string(system) + ".json"; }

Report Parse(std::string_view text, Strictness strictness, const std::function<void(Schema&)>& describe) {
	Report report;
	report.strictness = strictness;

	simdjson::dom::parser parser;
	const simdjson::padded_string padded(text.data(), text.size());
	simdjson::dom::element root;
	const simdjson::error_code err = parser.parse(padded).get(root);
	if (err != simdjson::SUCCESS) {
		report.Error("", std::string("not valid JSON: ") + simdjson::error_message(err));
		return report;
	}
	simdjson::dom::object object;
	if (root.get(object) != simdjson::SUCCESS) {
		report.Error("", "the file must hold one object: { ... }");
		return report;
	}

	Schema schema(object, "", report);
	describe(schema);
	schema.Finish();
	report.accepted = strictness == Strictness::Release || !report.HasErrors();
	return report;
}

std::string Line(std::string_view system, const Issue& issue) {
	std::string line = PathFor(system) + ": ";
	if (!issue.where.empty()) line += issue.where + ": ";
	return line + issue.text;
}

} // namespace KT1::Data
