/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 Data: the one place a number in a data file becomes a C++ value. Engine-free. Every loader of JSON numbers uses
// ReadNumber and nothing else (docs/engine/rules/data.md).
//
// The JSON parser's own number reader is never used: the engine builds it with -fsingle-precision-constant, which turns
// its table of powers of ten into floats. ReadNumber hands the file's own text to the C library, as Lua's reader does
// (lua_str2number is strtod, lua_Number is float), so a value moved from a .lua file keeps its bits.

#ifndef KT1_DATA_NUMBER_H
#define KT1_DATA_NUMBER_H

#include <string_view>

namespace KT1::Data {

enum class NumberError {
	Ok,
	Syntax,     // not a JSON number: -?(0|[1-9]digits)(.digits)?((e|E)(+|-)?digits)?
	NotWhole,   // an int was asked for and the text has a fraction or an exponent
	TooLarge,   // outside the type
};

// `token` is the number's text and nothing else. The target is left alone unless the result is Ok.
NumberError ReadNumber(std::string_view token, double& value);   // strtod
NumberError ReadNumber(std::string_view token, float& value);    // strtod, then cast once to float
NumberError ReadNumber(std::string_view token, int& value);      // strtoll

// True when the text has neither a fraction nor an exponent.
bool IsWholeNumber(std::string_view token);

} // namespace KT1::Data

#endif
