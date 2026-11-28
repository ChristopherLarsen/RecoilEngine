/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// kt1_core_tests: runs every Core test case in id order.
//   kt1_core_tests [--cases <dir>] [--only <id prefix>] [--from <n>] [--list]
// Output: "[RUN] <n> <id>" before a case, "[CHECKS] <count>" and "[CASE] <id> PASS|FAIL <text>" after it,
// "[DONE] pass=N fail=N" at the end.
// The driver uses [RUN] and [DONE] to fail a case that crashes or hangs and to go on from the next one (--from).
// Exit 0 all passed, 1 a case failed or the arguments are wrong.

#include "KT1CaseLoader.h"
#include "KT1Test.h"
#include "Game/KT1/Core/Common/Rate.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <string>
#include <vector>

#ifdef _WIN32
	#ifndef NOMINMAX
		#define NOMINMAX
	#endif
	#include <windows.h>
#endif

using namespace KT1::Test;
using KT1::Core::Rate;

namespace {
	struct Options {
		std::string casesDir;
		std::string only;
		size_t from = 0;
		bool list = false;
	};

	bool Parse(int argc, char** argv, Options& options) {
		for (int i = 1; i < argc; ++i) {
			const std::string arg = argv[i];
			const bool hasValue = i + 1 < argc;

			if (arg == "--list")
				options.list = true;
			else if (arg == "--cases" && hasValue)
				options.casesDir = argv[++i];
			else if (arg == "--only" && hasValue)
				options.only = argv[++i];
			else if (arg == "--from" && hasValue)
				options.from = std::strtoul(argv[++i], nullptr, 10);
			else
				return false;
		}
		return true;
	}

	std::string OneLine(std::string s) {
		std::replace(s.begin(), s.end(), '\n', ' ');
		std::replace(s.begin(), s.end(), '\r', ' ');
		return s;
	}

	// The sim rate is set for a rate pass only and unset after every pass, so a conversion nobody set a rate for asserts.
	void RunPass(const CaseDef& def, Case& t) {
		if (t.StepsPerSecond() > 0)
			Rate::SetStepsPerSecond(t.StepsPerSecond());

		try {
			def.body(t);
		} catch (const std::exception& error) {
			t.Fail(std::string("exception: ") + error.what());
		} catch (...) {
			t.Fail("unknown exception");
		}
		Rate::Clear();
	}

	// one pass, or one per step rate; returns the first failing pass's report, empty when all passed
	std::string Run(const CaseDef& def, int& checks) {
		const std::vector<int> rates = def.eachRate ? std::vector<int>(STEP_RATES.begin(), STEP_RATES.end()) : std::vector<int>{0};

		for (const int rate: rates) {
			Case t(rate);
			RunPass(def, t);
			checks += t.Checks();
			if (t.Failed())
				return t.Report();
		}
		return {};
	}

	// registered and data cases in id order; a repeated id fails
	std::vector<CaseDef> Collect(const Options& options) {
		std::vector<CaseDef> cases = Registry();
		if (!options.casesDir.empty())
			LoadJsonCases(options.casesDir, cases);

		std::stable_sort(cases.begin(), cases.end(), [](const CaseDef& a, const CaseDef& b) { return a.id < b.id; });

		for (size_t i = 1; i < cases.size(); ++i) {
			if (cases[i].id != cases[i - 1].id)
				continue;

			cases[i].body = [](Case& t) { t.Fail("duplicate case id"); };
			cases[i].eachRate = false;
		}

		std::erase_if(cases, [&](const CaseDef& def) { return !def.id.starts_with(options.only); });
		return cases;
	}

	// a crash must end the process with an exit code, not wait on a dialog
	void NoCrashDialogs() {
	#ifdef _WIN32
		SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
	#endif
	}
}

int main(int argc, char** argv) {
	NoCrashDialogs();

	Options options;
	if (!Parse(argc, argv, options)) {
		std::fprintf(stderr, "usage: kt1_core_tests [--cases <dir>] [--only <id prefix>] [--from <n>] [--list]\n");
		return 1;
	}

	const std::vector<CaseDef> cases = Collect(options);
	int passed = 0;
	int failed = 0;

	for (size_t n = options.from; n < cases.size(); ++n) {
		const CaseDef& def = cases[n];
		if (options.list) {
			std::printf("%s %s\n", def.id.c_str(), OneLine(def.text).c_str());
			continue;
		}

		std::printf("[RUN] %zu %s\n", n, def.id.c_str());
		std::fflush(stdout);

		int checks = 0;
		const std::string failure = Run(def, checks);
		std::printf("[CHECKS] %d\n", checks);
		if (failure.empty()) {
			++passed;
			std::printf("[CASE] %s PASS %s\n", def.id.c_str(), OneLine(def.text).c_str());
		} else {
			++failed;
			std::printf("[CASE] %s FAIL %s: %s\n", def.id.c_str(), OneLine(def.text).c_str(), OneLine(failure).c_str());
		}
		std::fflush(stdout);
	}

	if (!options.list)
		std::printf("[DONE] pass=%d fail=%d\n", passed, failed);

	return (failed > 0) ? 1 : 0;
}
