/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 engine-free: also built into kt1_core_tests (KT1/Test/README.md).
// KT1 Data: reading one number (Number.h). Engine-free. Keep this file free of floating literals: the engine builds with
// -fsingle-precision-constant.

#include "Number.h"

#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <string>

namespace KT1::Data {

static bool IsJsonNumber(std::string_view t) {
	std::size_t i = 0;
	const auto at = [&t](std::size_t k) { return k < t.size() ? t[k] : '\0'; };
	const auto digits = [&at, &i] {
		const std::size_t from = i;
		while (at(i) >= '0' && at(i) <= '9') ++i;
		return i > from;
	};
	if (at(i) == '-') ++i;
	if (at(i) == '0') ++i;
	else if (!digits()) return false;
	if (at(i) == '.') {
		++i;
		if (!digits()) return false;
	}
	if (at(i) == 'e' || at(i) == 'E') {
		++i;
		if (at(i) == '+' || at(i) == '-') ++i;
		if (!digits()) return false;
	}
	return i == t.size();
}

bool IsWholeNumber(std::string_view token) { return token.find_first_of(".eE") == std::string_view::npos; }

NumberError ReadNumber(std::string_view token, double& value) {
	if (!IsJsonNumber(token)) return NumberError::Syntax;
	const std::string text(token);   // strtod wants an end mark
	const double v = std::strtod(text.c_str(), nullptr);
	if (!std::isfinite(v)) return NumberError::TooLarge;
	value = v;
	return NumberError::Ok;
}

NumberError ReadNumber(std::string_view token, float& value) {
	double exact{};
	const NumberError err = ReadNumber(token, exact);
	if (err != NumberError::Ok) return err;
	const float v = static_cast<float>(exact);
	if (!std::isfinite(v)) return NumberError::TooLarge;
	value = v;
	return NumberError::Ok;
}

NumberError ReadNumber(std::string_view token, int& value) {
	if (!IsJsonNumber(token)) return NumberError::Syntax;
	if (!IsWholeNumber(token)) return NumberError::NotWhole;
	const std::string text(token);
	errno = 0;
	const long long v = std::strtoll(text.c_str(), nullptr, 10);   // DESIGN: decimal text
	if (errno == ERANGE || v < std::numeric_limits<int>::min() || v > std::numeric_limits<int>::max()) return NumberError::TooLarge;
	value = static_cast<int>(v);
	return NumberError::Ok;
}

} // namespace KT1::Data
