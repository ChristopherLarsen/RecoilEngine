/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 engine-free replay cases (LUA-FREE H02). In a *Test.cpp:
//
//   KT1_TRACE_CASE(Orders, "Orders.kt1t") { return std::make_unique<OrdersReplay>(); }
//
// The body makes the system's replay (Core/Trace/TraceReplay.h). With `--traces <dir>` (core_tests.py --traces) the case
// runs once for every file of that name under <dir> (the recorder writes <dir>/<engine>/<scene>/<system>.kt1t), as case
// TRACE-<system>-<engine>-<scene>; no file: one failing case. Without --traces no replay case exists.
// Each prints "[TRACE] <id> bytes= steps= inputs= compared= differences= ms=" and its first differences; it passes on a
// complete trace with at least one compared row and no difference. A column the system leaves out (Compares) is listed:
// "[TRACE] <id> skipped column <name>".

#ifndef KT1_TRACE_CASE_H
#define KT1_TRACE_CASE_H

#include "KT1Test.h"

#include "Game/KT1/Core/Trace/TraceReplay.h"

#include <memory>
#include <string>
#include <vector>

namespace KT1::Test {
	using MakeReplayFn = std::unique_ptr<Core::Trace::ReplaySystem> (*)();

	struct TraceRegistrar {
		TraceRegistrar(const char* system, const char* file, MakeReplayFn make);
	};

	// appends the replay cases of the traces under dir
	void AddTraceCases(const std::string& dir, std::vector<CaseDef>& cases);
}

#define KT1_TRACE_CASE_(fn, system, file) \
	static std::unique_ptr<KT1::Core::Trace::ReplaySystem> fn(); \
	static const KT1::Test::TraceRegistrar KT1_TEST_JOIN(fn, Registrar)(#system, file, &fn); \
	static std::unique_ptr<KT1::Core::Trace::ReplaySystem> fn()

#define KT1_TRACE_CASE(system, file) KT1_TRACE_CASE_(KT1_TEST_JOIN(kt1Trace, __COUNTER__), system, file)

#endif
