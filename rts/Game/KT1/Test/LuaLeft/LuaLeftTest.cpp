/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 system LuaLeft, engine-free: self time, windows, top rows, allocations, counts and the two [LUALEFT] lines.

#include "../KT1Test.h"
#include "Game/KT1/Core/LuaLeft/LuaLeftCore.h"

#include <chrono>
#include <cstdio>
#include <cstring>
#include <memory>

using namespace KT1::LuaLeft;

namespace {
	constexpr int64_t MS = 1000000;

	int RowOf(Meter& m, const char* state, Side side, const char* callIn) {
		uint32_t h = 0;
		for (const char* c = callIn; *c; ++c) h = h * 33u + uint32_t(*c);
		return m.Row(m.State(state, side), h, callIn);
	}

	// GameFrame 0..10 ms holds RecvFromSynced 1..3 and UnitDestroyed 4..8 (which holds a collection 5..6)
	Report Nested(Meter& m) {
		m.StartWindow(0, 0);
		const int gf = RowOf(m, "LuaRules", SYNCED, "GameFrame");
		const int recv = RowOf(m, "LuaRules", UNSYNCED, "RecvFromSynced");
		const int dead = RowOf(m, "LuaRules", SYNCED, "UnitDestroyed");
		m.Enter(gf, SYNCED, 0, 1);
		m.Enter(recv, UNSYNCED, 1 * MS, 1); m.Leave(3 * MS);
		m.Enter(dead, SYNCED, 4 * MS, 1);
		m.EnterGc(SYNCED, 5 * MS); m.Leave(6 * MS);
		m.Leave(8 * MS);
		m.Leave(10 * MS);
		return m.Close(1, 1, 1);
	}

	// one window of `period` steps: a 4 ms GameFrame a step and two 3 ms DrawScreen a step
	Report FullWindow(Meter& m, int period, bool& earlyDue) {
		m.StartWindow(100, 500);
		const int gf = RowOf(m, "LuaRules", SYNCED, "GameFrame");
		const int draw = RowOf(m, "LuaUI", UNSYNCED, "DrawScreen");
		int64_t t = 0;
		earlyDue = false;
		for (int s = 101; s <= 100 + period; ++s) {
			m.Enter(gf, SYNCED, t, s); m.Leave(t += 4 * MS);
			for (int d = 0; d < 2; ++d) { m.Enter(draw, UNSYNCED, t, s); m.Leave(t += 3 * MS); }
			earlyDue = earlyDue || (s < 100 + period && m.Due(s, period));
		}
		return m.Close(100 + period, 500 + 2 * period, period);
	}

	// ten seconds of steps at a rate
	int PeriodAt(int rate) { return static_cast<int>(Config{}.reportPeriodS * rate); }
}

KT1_CASE("LL-NEST", "a nested call or collection is taken out of its caller: nothing is counted twice") {
	auto m = std::make_unique<Meter>();
	const Report r = Nested(*m);
	KT1_CHECK(m->Idle());
	KT1_CHECK_EQ(r.selfNs[SYNCED], 7 * MS);
	KT1_CHECK_EQ(r.selfNs[UNSYNCED], 2 * MS);
	KT1_CHECK_EQ(r.gcNs[SYNCED], 1 * MS);
	KT1_CHECK_EQ(r.selfNs[SYNCED] + r.selfNs[UNSYNCED] + r.gcNs[SYNCED] + r.gcNs[UNSYNCED], 10 * MS);
	KT1_CHECK_EQ(r.calls[SYNCED], 2u);
	KT1_CHECK_EQ(r.calls[UNSYNCED], 1u);
	KT1_CHECK_EQ(r.topCount, 3);
	KT1_CHECK(std::strcmp(r.top[0].callIn, "GameFrame") == 0 && r.top[0].selfNs == 4 * MS);
	KT1_CHECK(std::strcmp(r.top[1].callIn, "UnitDestroyed") == 0);
	KT1_CHECK(std::strcmp(r.top[2].callIn, "RecvFromSynced") == 0);
}

KT1_RATE_CASE("LL-WINDOW", "a report is due after ten seconds of steps; sim steps and drawn frames are counted") {
	const int period = PeriodAt(testCase.StepsPerSecond());
	auto m = std::make_unique<Meter>();
	KT1_CHECK(m->NeedsStart(100) && !m->Due(100 + period, period));
	bool early = true;
	const Report r = FullWindow(*m, period, early);
	KT1_CHECK(!early);
	KT1_CHECK_EQ(r.simSteps, period);
	KT1_CHECK_EQ(r.drawFrames, 2 * period);
	KT1_CHECK(!r.partial);
	const Report empty = m->Close(100 + period + 5, 500 + 2 * period + 5, period);
	KT1_CHECK(empty.partial && empty.selfNs[SYNCED] == 0 && empty.topCount == 0);
	KT1_CHECK(m->NeedsStart(1) && !m->NeedsStart(100 + period + 6));   // a step counter gone back starts anew
	m->Stop();
	KT1_CHECK(m->NeedsStart(100 + period + 6));
}

KT1_RATE_CASE("LL-LINES", "the summary and top lines carry synced ms per step and unsynced ms per drawn frame") {
	auto m = std::make_unique<Meter>();
	bool early = false;
	const Report r = FullWindow(*m, PeriodAt(testCase.StepsPerSecond()), early);
	char line[2048];
	FormatSummary(r, line, sizeof(line));
	KT1_CHECK_MSG(std::strstr(line, "[LUALEFT] synced_ms=4.000 unsynced_ms=3.000 gc_ms=0.000 allocs=0 gadgets=0 widgets=0 ") != nullptr, line);
	KT1_CHECK_MSG(std::strstr(line, "calls_synced=1 calls_unsynced=1 ") != nullptr, line);
	KT1_CHECK_MSG(std::strstr(line, "window=full lost_calls=0") != nullptr, line);
	FormatTop(r, line, sizeof(line));
	KT1_CHECK_MSG(std::strstr(line, "U:LuaUI:DrawScreen=3.000(1.0)") != nullptr, line);
	KT1_CHECK_MSG(std::strstr(line, "S:LuaRules:GameFrame=4.000(1.0)") != nullptr, line);
}

KT1_CASE("LL-FORMAT", "a short buffer truncates safely and a window without frames divides by nothing") {
	auto m = std::make_unique<Meter>();
	const Report r = Nested(*m);
	char small[40];
	FormatTop(r, small, sizeof(small));
	KT1_CHECK_EQ(std::strlen(small), sizeof(small) - 1);
	char line[1024];
	FormatSummary(Report{}, line, sizeof(line));
	KT1_CHECK_MSG(std::strstr(line, "synced_ms=0.000 unsynced_ms=0.000") != nullptr, line);
}

KT1_CASE("LL-TOP", "the ten dearest rows of 25, dearest first; the others still count in the total") {
	auto m = std::make_unique<Meter>();
	m->StartWindow(0, 0);
	int64_t t = 0;
	char name[16];
	for (int i = 1; i <= 25; ++i) {
		const int cost = (i * 7) % 25 + 1;                          // 1..25 ms, arriving out of order
		std::snprintf(name, sizeof(name), "C%02d", cost);
		const int row = RowOf(*m, "LuaUI", UNSYNCED, name);
		m->Enter(row, UNSYNCED, t, 1); m->Leave(t += cost * MS);
	}
	const Report r = m->Close(1, 1, 1);
	KT1_CHECK_EQ(r.topCount, MAX_TOP);
	for (int k = 0; k < r.topCount; ++k) KT1_CHECK_EQ(r.top[k].selfNs, (25 - k) * MS);
	KT1_CHECK_EQ(r.selfNs[UNSYNCED], 325 * MS);
}

KT1_CASE("LL-ROWS", "rows are per state and call-in; a full table keeps the total and reports the lost call") {
	auto m = std::make_unique<Meter>();
	const int s = m->State("LuaRules", SYNCED), u = m->State("LuaRules", UNSYNCED);
	KT1_CHECK(s != u && m->State("LuaRules", SYNCED) == s);
	const int a = m->Row(s, 7, "GameFrame"), b = m->Row(u, 7, "GameFrame"), c = m->Row(s, 7, "GameStart");
	KT1_CHECK(a != b && a != c && b != c && m->Row(s, 7, "GameFrame") == a);
	m->StartWindow(0, 0);
	char name[16];
	int last = 0;
	for (int i = 0; i < MAX_ROWS + 5; ++i) {
		std::snprintf(name, sizeof(name), "N%d", i);
		last = m->Row(s, uint32_t(i), name);
	}
	KT1_CHECK_EQ(last, -1);
	KT1_CHECK_EQ(m->Row(s, 7, "GameFrame"), a);
	m->Enter(last, SYNCED, 0, 1); m->Leave(2 * MS);
	const Report r = m->Close(1, 1, 1);
	KT1_CHECK_EQ(r.selfNs[SYNCED], 2 * MS);
	KT1_CHECK_EQ(r.lostCalls, 1u);
}

KT1_CASE("LL-DEEP", "calls nested past the stack unwind, and the outermost call's time counts once") {
	auto m = std::make_unique<Meter>();
	m->StartWindow(0, 0);
	const int row = RowOf(*m, "LuaRules", SYNCED, "GameFrame");
	const int n = MAX_DEPTH + 3;
	for (int i = 0; i < n; ++i) m->Enter(row, SYNCED, i * MS, 1);
	for (int i = 0; i < n; ++i) m->Leave((n + i) * MS);
	KT1_CHECK(m->Idle());
	const Report r = m->Close(1, 1, 1);
	KT1_CHECK_EQ(r.selfNs[SYNCED], (2 * n - 1) * MS);
	KT1_CHECK_EQ(r.lostCalls, 3u);
}

KT1_CASE("LL-LOST", "a call without a row or past the stack is lost once") {
	auto m = std::make_unique<Meter>();
	m->StartWindow(0, 0);
	for (int i = 0; i < MAX_DEPTH; ++i) m->Enter(-1, SYNCED, 0, 1);
	m->Enter(-1, SYNCED, 0, 1);                   // no row and too deep
	m->EnterGc(SYNCED, 0);                        // a collection too deep
	for (int i = 0; i < MAX_DEPTH + 2; ++i) m->Leave(0);
	KT1_CHECK(m->Idle());
	KT1_CHECK_EQ(m->Close(1, 1, 1).lostCalls + 0, uint64_t(MAX_DEPTH + 2));
}

KT1_CASE("LL-ALLOCS", "allocations are counter growth per side; new and gone contexts are handled") {
	auto m = std::make_unique<Meter>();
	int a = 0, b = 0, c = 0;
	m->SampleAllocs(&a, SYNCED, 1000);
	m->SampleAllocs(&b, UNSYNCED, 5000);
	m->StartWindow(0, 0);                         // priming: what came before the window is dropped
	m->SampleAllocs(&a, SYNCED, 1300);
	m->SampleAllocs(&b, UNSYNCED, 5040);
	m->SampleAllocs(&c, UNSYNCED, 7);             // created in the window
	const Report r = m->Close(1, 1, 1);
	KT1_CHECK_EQ(r.allocs[SYNCED], 300u);
	KT1_CHECK_EQ(r.allocs[UNSYNCED], 47u);
	m->SampleAllocs(&a, SYNCED, 20);              // a new context at the old address
	const Report r2 = m->Close(2, 2, 1);
	KT1_CHECK_EQ(r2.allocs[SYNCED], 20u);
	KT1_CHECK_EQ(r2.allocs[UNSYNCED], 0u);
	for (int i = 0; i < 3; ++i) { m->SampleAllocs(&a, SYNCED, 20); m->Close(3 + i, 3 + i, 1); }
	static int many[MAX_ALLOCS];
	for (int i = 0; i < MAX_ALLOCS - 1; ++i) m->SampleAllocs(&many[i], UNSYNCED, 1);
	const Report r3 = m->Close(9, 9, 1);
	KT1_CHECK_EQ(r3.allocs[UNSYNCED], uint64_t(MAX_ALLOCS - 1));
}

KT1_CASE("LL-COUNTS", "gadget and widget counts are read once a window and summed over states") {
	auto m = std::make_unique<Meter>();
	const int s = m->State("LuaRules", SYNCED), u = m->State("LuaRules", UNSYNCED);
	const int g = m->State("LuaGaia", SYNCED), ui = m->State("LuaUI", UNSYNCED);
	m->StartWindow(0, 0);
	KT1_CHECK(m->CountsWanted(s) && !m->CountsWanted(-1));
	m->SetCounts(s, 100, 0); m->SetCounts(u, 60, 0); m->SetCounts(g, 2, 0); m->SetCounts(ui, 0, 190);
	KT1_CHECK(!m->CountsWanted(s));
	const Report r = m->Close(1, 1, 1);
	KT1_CHECK_EQ(r.gadgetsSynced, 102);
	KT1_CHECK_EQ(r.gadgetsUnsynced, 60);
	KT1_CHECK_EQ(r.widgets, 190);
	KT1_CHECK(m->CountsWanted(s));
	m->SetCounts(s, 100, 0);
	const Report r2 = m->Close(2, 2, 1);
	KT1_CHECK_EQ(r2.gadgetsSynced, 102);          // quiet for one window: still there
	m->SetCounts(s, 100, 0);
	const Report r3 = m->Close(3, 3, 1);
	KT1_CHECK_EQ(r3.gadgetsSynced, 100);          // quiet for two: closed
	KT1_CHECK_EQ(r3.widgets, 0);
	KT1_CHECK(!IsPerUnitCallIn("DrawUnit"));
}

KT1_CASE("LL-UNIT-NAMES", "per-unit call-ins are told from per-step and batched ones") {
	KT1_CHECK(IsPerUnitCallIn("UnitCreated") && IsPerUnitCallIn("AllowCommand") && IsPerUnitCallIn("UnitScript"));
	KT1_CHECK(!IsPerUnitCallIn("GameFrame") && !IsPerUnitCallIn("Update") && !IsPerUnitCallIn("Unit"));
	KT1_CHECK(!IsPerUnitCallIn("SupComFightBatch"));
}

KT1_RATE_CASE("LL-UNIT-STEPS", "unit calls are counted, with the steps that had any (LF-IDLE)") {
	const int rate = testCase.StepsPerSecond();
	auto m = std::make_unique<Meter>();
	m->StartWindow(0, 0);
	const int idle = RowOf(*m, "LuaRules", SYNCED, "UnitIdle"), gf = RowOf(*m, "LuaRules", SYNCED, "GameFrame");
	for (int s = 1; s <= 10 * rate; ++s) {
		m->Enter(gf, SYNCED, s * MS, s); m->Leave(s * MS);
		if (s % rate == 0) for (int i = 0; i < 3; ++i) { m->Enter(idle, SYNCED, s * MS, s); m->Leave(s * MS); }
	}
	const Report r = m->Close(10 * rate, 10 * rate, 10 * rate);
	KT1_CHECK_EQ(r.unitCalls, 30u);
	KT1_CHECK_EQ(r.unitCallSteps, 10);
}

KT1_CASE("LL-HEAP", "the meter allocates nothing, reports included") {
	auto m = std::make_unique<Meter>();
	m->StartWindow(0, 0);
	char line[2048];
	const long long before = KT1::Test::AllocationCount();
	for (int s = 1; s <= 600; ++s) {
		const int row = RowOf(*m, "LuaRules", SYNCED, "GameFrame");
		m->Enter(row, SYNCED, s * MS, s); m->Leave(s * MS + 1);
		m->SampleAllocs(m.get(), SYNCED, uint64_t(s));
		if (m->Due(s, 300)) { const Report r = m->Close(s, s, 300); FormatSummary(r, line, sizeof(line)); FormatTop(r, line, sizeof(line)); }
	}
	KT1_CHECK_EQ(KT1::Test::AllocationCount(), before);
}

KT1_CASE("LL-COST", "one metered call, its two clock reads included, costs under 2 us (printed as [BENCH])") {
	auto m = std::make_unique<Meter>();
	m->StartWindow(0, 0);
	const char* names[] = {"GameFrame", "UnitDamaged", "AllowCommand", "RecvFromSynced", "ProjectileCreated", "DrawScreen", "Update", "DrawUnit"};
	constexpr int CALLS = 400000;
	const auto now = [] { return int64_t(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count()); };
	const int64_t from = now();
	for (int i = 0; i < CALLS; ++i) {
		const int row = RowOf(*m, "LuaRules", SYNCED, names[i % 8]);   // state and row lookup, as the adapter does
		m->Enter(row, SYNCED, now(), 1);
		m->Leave(now());
	}
	const double each = static_cast<double>(now() - from) / CALLS;
	std::printf("[BENCH] LuaLeft %.0f ns per metered call\n", each);
	KT1_CHECK(each < 2000);
}
