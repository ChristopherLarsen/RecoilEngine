/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 engine-free Core tests (LUA-FREE-WORKPACKAGES F03): case registration and checks. Standard library only.
// Run: python -I tools/engine/core_tests.py (game-build). No engine build, no list to edit: the driver compiles
// KT1/Core/**/*.cpp, KT1/Test/*.cpp, every KT1/Test/**/*Test.cpp and the .cpp files marked engine-free into one binary.
//
//   Test/<System>/<Name>Test.cpp:
//     KT1_CASE("SYS-ONE", "what must hold") { KT1_CHECK(a < b); KT1_CHECK_EQ(Sum(v), 6); }
//     KT1_RATE_CASE("SYS-TWO", "a period in seconds") { Run(testCase.StepsPerSecond()); }   // runs at 30 and at 10
//   Cases as JSON files: KT1CaseLoader.h.
//
// One case prints one line, "[CASE] <id> PASS|FAIL <text>". Ids are unique across all systems. A case that fails,
// throws or crashes fails alone. Cases run in id order and must not depend on each other.

#ifndef KT1_TEST_H
#define KT1_TEST_H

#include <array>
#include <functional>
#include <sstream>
#include <string>
#include <vector>

namespace KT1::Test {
	// the sim step rates every rate case runs at (SIM-10HZ-PLAN)
	inline constexpr std::array<int, 2> STEP_RATES = {30, 10};

	// calls of operator new in this binary so far (the runner replaces it): equal before and after a steady run means no
	// allocation. A test file must not define operator new itself.
	long long AllocationCount();

	// the running case, named testCase inside a case body; pass it on to helpers that check
	class Case {
	public:
		explicit Case(int stepsPerSecond = 0): stepsPerSecond(stepsPerSecond) {}

		void Fail(const char* file, int line, const std::string& what);
		void Fail(const std::string& what);
		bool Failed() const { return failures > 0; }
		// one check of the macros below, passed or not
		void CountCheck() { ++checks; }
		int Checks() const { return checks; }
		// the rate of this pass of a rate case, 0 in a plain case
		int StepsPerSecond() const { return stepsPerSecond; }
		// the first failure and how many followed it
		std::string Report() const;

	private:
		std::string first;
		int failures = 0;
		int checks = 0;
		int stepsPerSecond;
	};

	struct CaseDef {
		std::string id;
		std::string text;
		std::function<void(Case&)> body;
		bool eachRate = false;
	};

	// every case registered by a static Registrar
	std::vector<CaseDef>& Registry();

	struct Registrar {
		Registrar(const char* id, const char* text, void (*body)(Case&), bool eachRate) {
			Registry().push_back({id, text, body, eachRate});
		}
	};

	template<class A, class B>
	std::string Differ(const char* expression, const A& actual, const B& expected) {
		std::ostringstream out;
		out << expression << ": got " << actual << ", expected " << expected;
		return out.str();
	}
}

#define KT1_TEST_JOIN_(a, b) a##b
#define KT1_TEST_JOIN(a, b) KT1_TEST_JOIN_(a, b)

#define KT1_TEST_CASE_(fn, id, text, eachRate) \
	static void fn(KT1::Test::Case& testCase); \
	static const KT1::Test::Registrar KT1_TEST_JOIN(fn, Registrar)(id, text, &fn, eachRate); \
	static void fn([[maybe_unused]] KT1::Test::Case& testCase)

#define KT1_CASE(id, text) KT1_TEST_CASE_(KT1_TEST_JOIN(kt1Case, __COUNTER__), id, text, false)
#define KT1_RATE_CASE(id, text) KT1_TEST_CASE_(KT1_TEST_JOIN(kt1Case, __COUNTER__), id, text, true)

// A failed check is recorded and the case goes on. Wrap a condition or value that holds a bare comma in parentheses.
#define KT1_CHECK_MSG(condition, message) do { \
		testCase.CountCheck(); \
		if (!(condition)) testCase.Fail(__FILE__, __LINE__, (message)); \
	} while (0)
#define KT1_CHECK(condition) KT1_CHECK_MSG(condition, #condition)
#define KT1_CHECK_EQ(actual, expected) do { \
		const auto& kt1Actual = (actual); const auto& kt1Expected = (expected); \
		testCase.CountCheck(); \
		if (!(kt1Actual == kt1Expected)) testCase.Fail(__FILE__, __LINE__, KT1::Test::Differ(#actual, kt1Actual, kt1Expected)); \
	} while (0)

#endif
