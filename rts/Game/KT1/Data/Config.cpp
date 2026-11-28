/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 engine-free: also built into kt1_core_tests (KT1/Test/README.md).
// KT1 Data: parsing a config file (Config.h). Engine-free.

#include "Config.h"

#include "Json.h"

namespace KT1::Data {

std::string PathFor(std::string_view system) { return "data/kt1/" + std::string(system) + ".json"; }

Report Parse(std::string_view text, Strictness strictness, const std::function<void(Schema&)>& describe) {
	Report report;
	report.strictness = strictness;

	Node root;
	std::string error;
	if (!ReadObject(text, root, error)) {
		report.Error("", error);
		return report;
	}

	Schema schema(root, "", report);
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
