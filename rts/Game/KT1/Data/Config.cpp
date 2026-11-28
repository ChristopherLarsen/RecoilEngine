/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 engine-free: also built into kt1_core_tests (KT1/Test/README.md).
// KT1 Data: parsing a config file (Config.h). Engine-free.

#include "Config.h"

#include <cctype>
#include <cstdlib>

#include "simdjson.h"

namespace KT1::Data {

std::string PathFor(std::string_view system) { return "data/kt1/" + std::string(system) + ".json"; }

// The engine builds every C++ file, simdjson too, with -fsingle-precision-constant: the reader's table of powers of ten
// holds floats, exact only up to 1e10. A number whose last digit lies further from the point than that is not read to the
// nearest double, so it is refused.
static constexpr int EXACT_POWER = 10;

// The end of the number that starts at text[i]; `power` is the decimal power of its last digit (0.25 -> -2, 3e4 -> 4).
static std::size_t Number(std::string_view text, std::size_t i, int& power) {
	const auto digit = [&text](std::size_t k) { return k < text.size() && std::isdigit(static_cast<unsigned char>(text[k])) != 0; };
	if (text[i] == '-') ++i;
	while (digit(i)) ++i;
	int fraction = 0;
	if (i < text.size() && text[i] == '.')
		for (++i; digit(i); ++i) ++fraction;
	long exponent = 0;
	if (i < text.size() && (text[i] == 'e' || text[i] == 'E')) {
		const std::size_t from = ++i;
		if (i < text.size() && (text[i] == '+' || text[i] == '-')) ++i;
		while (digit(i)) ++i;
		exponent = std::strtol(std::string(text.substr(from, i - from)).c_str(), nullptr, 10);
	}
	power = static_cast<int>(exponent) - fraction;
	return i;
}

// `text` has already parsed, so a quote opens or closes a string and a digit outside one starts a number.
static void CheckNumbers(std::string_view text, Report& report) {
	for (std::size_t i = 0; i < text.size();) {
		const char c = text[i];
		if (c == '"') {
			for (++i; i < text.size() && text[i] != '"'; ++i)
				if (text[i] == '\\') ++i;
			++i;
		} else if (c == '-' || std::isdigit(static_cast<unsigned char>(c)) != 0) {
			int power = 0;
			const std::size_t end = Number(text, i, power);
			if (power > EXACT_POWER || power < -EXACT_POWER)
				report.Error("", std::string(text.substr(i, end - i)) + " is not read exactly: give at most 10 decimal places");
			i = end;
		} else {
			++i;
		}
	}
}

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

	CheckNumbers(text, report);
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
