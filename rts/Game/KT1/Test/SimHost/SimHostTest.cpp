/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// Core/SimHost and Core/SimProbe (LUA-FREE-WORKPACKAGES F05), engine-free.

#include "../KT1Test.h"

#include "Game/KT1/Core/SimHost/CheckTally.h"
#include "Game/KT1/Core/SimHost/DisplayQueue.h"
#include "Game/KT1/Core/SimHost/Ordered.h"
#include "Game/KT1/Core/SimHost/Phase.h"
#include "Game/KT1/Core/SimHost/SimEventLog.h"
#include "Game/KT1/Core/SimHost/StateHash.h"
#include "Game/KT1/Core/SimHost/UnitColumns.h"
#include "Game/KT1/Core/SimProbe/SimProbeCore.h"

#include <functional>
#include <initializer_list>
#include <string>
#include <utility>
#include <vector>

using namespace KT1::Core::SimHost;
using KT1::Test::AllocationCount;

namespace {
	Event Ev(EventType t, int id, int arg = -1, float value = 0.0f) {
		Event e;
		e.type = t;
		e.id = id;
		e.arg = arg;
		e.value = value;
		return e;
	}

	std::string Tag(EventType t, int id) { return std::to_string(static_cast<int>(t)) + ":" + std::to_string(id) + " "; }

	std::string Raised(const EventBatch& b) {
		std::string s;
		b.ForEachRaised([&](const Event& e) { s += Tag(e.type, e.id); });
		return s;
	}

	std::string ById(const EventBatch& b, EventType t) {
		std::vector<uint32_t> scratch;
		std::string s;
		b.ForEachById(t, scratch, [&](const Event& e) { s += std::to_string(e.id) + "/" + std::to_string(e.arg) + " "; });
		return s;
	}
}

KT1_CASE("SIMHOST-ORDER", "a batch: one type by ascending id, across types as the engine raised them") {
	EventLog log;
	const int r = log.AddReader(ALL_EVENTS);
	log.Push(Ev(EventType::UnitDamaged, 9, 1));
	log.Push(Ev(EventType::UnitDestroyed, 4));
	log.Push(Ev(EventType::UnitDamaged, 2, 2));
	log.Push(Ev(EventType::UnitCreated, 7));
	log.Push(Ev(EventType::UnitDamaged, 9, 3));
	log.Push(Ev(EventType::UnitDamaged, 2, 4));
	const EventBatch b = log.Take(r);
	KT1_CHECK_EQ(ById(b, EventType::UnitDamaged), std::string("2/2 2/4 9/1 9/3 "));   // equal ids as raised
	const std::string want = Tag(EventType::UnitDamaged, 9) + Tag(EventType::UnitDestroyed, 4) + Tag(EventType::UnitDamaged, 2)
		+ Tag(EventType::UnitCreated, 7) + Tag(EventType::UnitDamaged, 9) + Tag(EventType::UnitDamaged, 2);
	KT1_CHECK_EQ(Raised(b), want);
	KT1_CHECK(b.Size(EventType::UnitDamaged) == 4 && b.At(EventType::UnitDamaged, 0).id == 9);
	KT1_CHECK(log.Take(r).Empty());
}

KT1_CASE("SIMHOST-READERS", "each reader gets its types once; nothing unasked is kept") {
	EventLog log;
	log.Push(Ev(EventType::UnitCreated, 1));
	KT1_CHECK(log.Held(EventType::UnitCreated) == 0);   // no reader, nothing recorded
	const int a = log.AddReader(MaskOf(EventType::UnitCreated));
	const int c = log.AddReader(MaskOf(EventType::UnitCreated) | MaskOf(EventType::UnitIdle));
	log.Push(Ev(EventType::UnitCreated, 1));
	log.Push(Ev(EventType::UnitFinished, 1));
	log.Push(Ev(EventType::UnitIdle, 5));
	KT1_CHECK(log.Held(EventType::UnitFinished) == 0);
	KT1_CHECK_EQ(Raised(log.Take(a)), Tag(EventType::UnitCreated, 1));
	log.Compact();
	KT1_CHECK(log.Held(EventType::UnitCreated) == 1);   // c has not taken it
	log.Push(Ev(EventType::UnitCreated, 2));
	KT1_CHECK_EQ(Raised(log.Take(c)), Tag(EventType::UnitCreated, 1) + Tag(EventType::UnitIdle, 5) + Tag(EventType::UnitCreated, 2));
	KT1_CHECK_EQ(Raised(log.Take(a)), Tag(EventType::UnitCreated, 2));   // its cursor survives compaction
	log.Compact();
	KT1_CHECK(log.Held(EventType::UnitCreated) == 0 && log.Held(EventType::UnitIdle) == 0);
	const int late = log.AddReader(ALL_EVENTS);
	log.Push(Ev(EventType::UnitIdle, 8));
	KT1_CHECK_EQ(Raised(log.Take(late)), Tag(EventType::UnitIdle, 8));
}

KT1_CASE("SIMHOST-REENTRY", "an event raised while a batch is read waits for the next batch") {
	EventLog log;
	const int r = log.AddReader(ALL_EVENTS);
	for (int i = 0; i < 4; ++i) log.Push(Ev(EventType::UnitDestroyed, i));
	const EventBatch b = log.Take(r);
	int seen = 0, sum = 0;
	b.ForEachRaised([&](const Event& e) {
		++seen;
		for (int k = 0; k < 100; ++k) log.Push(Ev(EventType::UnitDestroyed, 100 + e.id));   // the list grows under us
		sum += e.id;
	});
	KT1_CHECK(seen == 4 && sum == 6);
	KT1_CHECK(log.Take(r).Size(EventType::UnitDestroyed) == 400);
}

// Ten seconds of sim time at either rate.
KT1_RATE_CASE("SIMHOST-RATE", "every event once and in order, whatever the step length") {
	const int rate = testCase.StepsPerSecond();
	EventLog log;
	const int r = log.AddReader(ALL_EVENTS);
	std::vector<uint32_t> scratch;
	int pushed = 0, got = 0, lastValue = -1;
	bool inOrder = true;
	for (int step = 0; step < 10 * rate; ++step) {
		for (int k = 0; k < 300 / rate; ++k) {
			log.Push(Ev((k & 1) ? EventType::UnitIdle : EventType::UnitDamaged, (k * 7) % 11, pushed));
			++pushed;
		}
		const EventBatch b = log.Take(r);
		b.ForEachRaised([&](const Event& e) { inOrder = inOrder && (e.arg == lastValue + 1); lastValue = e.arg; ++got; });
		int prev = -1;
		b.ForEachById(EventType::UnitDamaged, scratch, [&](const Event& e) { inOrder = inOrder && e.id >= prev; prev = e.id; });
		log.Compact();
	}
	KT1_CHECK(pushed == 3000);
	KT1_CHECK_EQ(got, pushed);
	KT1_CHECK(inOrder);
}

KT1_CASE("SIMHOST-NOALLOC", "warm event lists and display queue allocate nothing") {
	EventLog log;
	DisplayQueue q;
	const int r = log.AddReader(ALL_EVENTS);
	std::vector<uint32_t> scratch;
	long sink = 0;
	const auto step = [&]() {
		for (int k = 0; k < 64; ++k) {
			log.Push(Ev(EventType::UnitDamaged, 64 - k));
			DisplayEvent d;
			d.id = k;
			q.Emit(d);
		}
		const EventBatch b = log.Take(r);
		b.ForEachRaised([&](const Event& e) { sink += e.id; });
		b.ForEachById(EventType::UnitDamaged, scratch, [&](const Event& e) { sink -= e.id; });
		log.Compact();
		q.Publish();
		q.Replay([&](const DisplayEvent& d) { sink += d.id; });
	};
	for (int i = 0; i < 4; ++i) step();
	const long long before = AllocationCount();
	for (int i = 0; i < 1000; ++i) step();
	KT1_CHECK_EQ(AllocationCount() - before, 0LL);
	KT1_CHECK(sink != 0);
}

KT1_CASE("SIMHOST-CHAIN", "validators run by (order, name); the first no ends the chain") {
	Ordered<std::function<bool(std::string&)>> chain;
	const auto v = [](const char* tag, bool allow) {
		return [tag, allow](std::string& seen) { seen += tag; return allow; };
	};
	chain.Add(10, "zeta", v("z", true));
	chain.Add(10, "alpha", v("a", true));
	chain.Add(-5, "late", v("l", true));
	chain.Add(20, "tail", v("t", true));
	std::string seen;
	KT1_CHECK(AllAllow(chain, seen));
	KT1_CHECK_EQ(seen, std::string("lazt"));
	chain.Add(10, "mid", v("m", false));
	seen.clear();
	KT1_CHECK(!AllAllow(chain, seen));
	KT1_CHECK_EQ(seen, std::string("lam"));
	const Ordered<std::function<bool(std::string&)>> none;
	seen.clear();
	KT1_CHECK(AllAllow(none, seen) && seen.empty());
}

KT1_CASE("SIMHOST-PHASE", "phase names and half-open id ranges") {
	constexpr IdRange all = IdRange::All();
	const IdRange half{10, 20};
	KT1_CHECK(all.Contains(0) && all.Contains(1000000));
	KT1_CHECK(half.Contains(10) && half.Contains(19) && !half.Contains(20) && !half.Contains(9));
	KT1_CHECK_EQ(std::string(PhaseName(Phase::FrameStart)), std::string("FrameStart"));
	KT1_CHECK_EQ(std::string(PhaseName(Phase::FrameEnd)), std::string("FrameEnd"));
}

KT1_CASE("SIMHOST-HASH", "the state hash sees order, ids and every bit") {
	const auto h = [](std::initializer_list<std::pair<int, float>> rows) {
		StateHash s;
		for (const auto& r : rows) { s.Add(r.first); s.Add(r.second); }
		return s.Get();
	};
	KT1_CHECK(h({{1, 2.0f}, {3, 4.0f}}) == h({{1, 2.0f}, {3, 4.0f}}));
	KT1_CHECK(h({{1, 2.0f}, {3, 4.0f}}) != h({{3, 4.0f}, {1, 2.0f}}));
	KT1_CHECK(h({{1, 0.0f}}) != h({{1, -0.0f}}));
	KT1_CHECK(h({{1, 2.0f}}) != h({{2, 2.0f}}));
}

KT1_CASE("SIMHOST-DISPLAY", "display events: after publish, in emit order, once") {
	DisplayQueue q;
	const auto emit = [&](int id) { DisplayEvent d; d.kind = DisplayKind::Sound; d.id = id; q.Emit(d); };
	std::string seen;
	const auto replay = [&]() { seen.clear(); q.Replay([&](const DisplayEvent& d) { seen += std::to_string(d.id) + " "; }); };
	emit(1);
	replay();
	KT1_CHECK(seen.empty());   // the step is not published
	emit(2);
	q.Publish();
	emit(3);
	replay();
	KT1_CHECK_EQ(seen, std::string("1 2 "));
	q.Publish();
	emit(4);
	q.Publish();
	replay();
	KT1_CHECK_EQ(seen, std::string("3 4 "));   // two steps before a replay: both
	replay();
	KT1_CHECK(seen.empty());
	DisplayEvent d;
	d.SetText(std::string(500, 'x').c_str());
	KT1_CHECK(std::string(d.text).size() == DISPLAY_TEXT - 1);
}

KT1_CASE("SIMPROBE-STATE", "units are tracked from creation to death, and a reused id starts blank") {
	KT1::Core::SimProbe::State s;
	s.Init(32);
	s.Apply(Ev(EventType::UnitDamaged, 3, -1, 5.0f));   // never created: ignored
	s.Apply(Ev(EventType::UnitCreated, 7));
	s.Apply(Ev(EventType::UnitCreated, 3));
	s.Apply(Ev(EventType::UnitDamaged, 7, -1, 2.5f));
	s.Apply(Ev(EventType::UnitDamaged, 3, -1, 1.0f));
	s.Apply(Ev(EventType::ProjectileCreated, 7));   // not a unit
	KT1_CHECK_EQ(s.units.index.Size(), 2);
	KT1_CHECK(s.damage[s.units.index.SlotOf(7)] == 2.5f && s.events[s.units.index.SlotOf(7)] == 2);
	s.Apply(Ev(EventType::UnitDestroyed, 7));
	KT1_CHECK(!s.units.index.Has(7) && s.damage[s.units.index.SlotOf(3)] == 1.0f);
	s.Apply(Ev(EventType::UnitCreated, 7));
	KT1_CHECK(s.damage[s.units.index.SlotOf(7)] == 0.0f && s.events[s.units.index.SlotOf(7)] == 1);
}

KT1_CASE("SIMHOST-COLUMNS", "one removal moves every owned column") {
	UnitColumns units;
	KT1::Core::Column<float> a;
	KT1::Core::Column<int> b;
	units.Init(16);
	units.Own(a, -1.0f);
	units.Own(b);
	for (const int id : {5, 2, 9}) {
		const auto slot = units.AddUnit(id);
		a[slot] = static_cast<float>(id);
		b[slot] = 10 * id;
	}
	units.RemoveUnit(5);   // the first slot: the last one moves into it
	units.RemoveUnit(77);  // not there: nothing moves
	KT1_CHECK_EQ(units.index.Size(), 2);
	for (const int id : {2, 9})
		KT1_CHECK(a[units.index.SlotOf(id)] == static_cast<float>(id) && b[units.index.SlotOf(id)] == 10 * id);
	const auto reused = units.AddUnit(5);
	KT1_CHECK(a[reused] == -1.0f && b[reused] == 0);
}

KT1_CASE("SIMHOST-CHECK", "check lines: a step line only on differences, totals by system name, 20 descriptions") {
	CheckTally t;
	std::string lines;
	const auto add = [&](const std::string& s) { lines += s + ";"; };
	t.Add("Zed", 5, 0);
	t.CloseStep(1, add);
	KT1_CHECK(lines.empty());
	int described = 0;
	for (int i = 0; i < 30; ++i) described += t.Add("Abc", 2, 1) ? 1 : 0;
	KT1_CHECK_EQ(described, CHECK_DETAILS);
	t.CloseStep(2, add);
	KT1_CHECK_EQ(lines, std::string("[KT1CHECK] Abc f=2 n=60 diffs=30;"));
	lines.clear();
	t.CloseStep(3, add);
	KT1_CHECK(lines.empty());
	t.Totals(add);
	KT1_CHECK_EQ(lines, std::string("[KT1CHECK] Abc total n=60 diffs=30;[KT1CHECK] Zed total n=5 diffs=0;"));
	KT1_CHECK(CheckTally::SameBits(1.5f, 1.5f) && !CheckTally::SameBits(0.0f, -0.0f));
}
