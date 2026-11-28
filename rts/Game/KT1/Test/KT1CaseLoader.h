/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 engine-free Core tests: cases as data. A system's JSON files are game-build tools/test/core_cases/<system>/*.json:
//     {"cases": [{"id": "SYS-ONE", "text": "what must hold", ...the case's inputs and expected values}]}
// and one handler in Test/ runs each entry of that <system> folder:
//     KT1_JSON_CASES("orders") { KT1_CHECK_EQ(Next(data["queue"]), int64_t(data["expected"])); }
// data is the entry; a missing or mistyped field throws, which fails that case. A folder without a handler, a handler
// without a case and an unreadable file each print a failing case, so no data is skipped without a word.

#ifndef KT1_CASE_LOADER_H
#define KT1_CASE_LOADER_H

#include "KT1Test.h"

#include "simdjson.h"

#include <string>
#include <vector>

namespace KT1::Test {
	using JsonHandler = void (*)(Case&, simdjson::dom::element);

	struct JsonRegistrar {
		JsonRegistrar(const char* system, JsonHandler handler);
	};

	// appends the cases of every <dir>/<system>/*.json to cases
	void LoadJsonCases(const std::string& dir, std::vector<CaseDef>& cases);
}

#define KT1_JSON_CASES_(fn, system) \
	static void fn(KT1::Test::Case& testCase, simdjson::dom::element data); \
	static const KT1::Test::JsonRegistrar KT1_TEST_JOIN(fn, Registrar)(system, &fn); \
	static void fn([[maybe_unused]] KT1::Test::Case& testCase, [[maybe_unused]] simdjson::dom::element data)

#define KT1_JSON_CASES(system) KT1_JSON_CASES_(KT1_TEST_JOIN(kt1Json, __COUNTER__), system)

#endif
