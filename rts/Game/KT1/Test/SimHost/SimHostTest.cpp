/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// Core/SimHost and Core/SimProbe (LUA-FREE-WORKPACKAGES F05), engine-free.

#include "../KT1Test.h"

#include "Game/KT1/Core/Common/DueWheel.h"
#include "Game/KT1/Core/SimHost/CheckPairs.h"
#include "Game/KT1/Core/SimHost/CheckTally.h"
#include "Game/KT1/Core/SimHost/DisplayQueue.h"
#include "Game/KT1/Core/SimHost/Ordered.h"
#include "Game/KT1/Core/SimHost/Phase.h"
#include "Game/KT1/Core/SimHost/SimEventLog.h"
#include "Game/KT1/Core/SimHost/StartGate.h"
#include "Game/KT1/Core/SimHost/StateHash.h"
#include "Game/KT1/Core/SimHost/UnitColumns.h"
#include "Game/KT1/Core/SimHost/Validators.h"
#include "Game/KT1/Core/SimHost/WriteGate.h"
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

KT1_CASE("SIMHOST-FAKES", "a fake and a real unit in one step: a normal reader sees the real one, an opted-in reader both, in order") {
	SplitEventLog log;
	const int normal = log.AddReader(ALL_EVENTS, false), intel = log.AddReader(ALL_EVENTS, true);
	for (const EventType t : {EventType::UnitCreated, EventType::UnitDamaged, EventType::UnitDestroyed}) {
		log.Push(Ev(t, 5), true);
		log.Push(Ev(t, 8), false);
	}
	const std::string real = Tag(EventType::UnitCreated, 8) + Tag(EventType::UnitDamaged, 8) + Tag(EventType::UnitDestroyed, 8);
	const std::string both = Tag(EventType::UnitCreated, 5) + Tag(EventType::UnitCreated, 8) + Tag(EventType::UnitDamaged, 5) +
		Tag(EventType::UnitDamaged, 8) + Tag(EventType::UnitDestroyed, 5) + Tag(EventType::UnitDestroyed, 8);
	KT1_CHECK_EQ(Raised(log.Take(normal)), real);
	KT1_CHECK_EQ(Raised(log.Take(intel)), both);
	log.Compact();
	KT1_CHECK_EQ(log.Held(EventType::UnitCreated), static_cast<size_t>(0));
	log.Push(Ev(EventType::UnitIdle, 5), true);
	KT1_CHECK(log.Take(normal).Empty());
	KT1_CHECK_EQ(Raised(log.Take(intel)), Tag(EventType::UnitIdle, 5));
}

KT1_CASE("SIMHOST-FAKES-NONE", "without a reader that sees fakes, a fake unit's events are not kept at all") {
	SplitEventLog log;
	log.AddReader(ALL_EVENTS, false);
	log.Push(Ev(EventType::UnitCreated, 5), true);
	KT1_CHECK_EQ(log.Held(EventType::UnitCreated), static_cast<size_t>(0));
	log.Clear();
	KT1_CHECK_EQ(log.Wanted(), static_cast<EventMask>(0));
}

namespace {
	// A rule as the registry sees it: its validator, registered at config load, reads per-def data that Start fills.
	struct FakeRule {
		struct RuleFacts { bool synced; };
		FakeRule(bool synced, bool enabled = true): facts{synced}, on(enabled) {}
		bool Enabled() const { return on; }
		const RuleFacts& Info() const { return facts; }
		void Start() { ++starts; perDef.assign(4, 7); }
		bool Ask(int def) const { return def < static_cast<int>(perDef.size()) && perDef[def] == 7; }
		RuleFacts facts;
		bool on;
		int starts = 0;
		std::vector<int> perDef;
	};
}

KT1_CASE("SIMHOST-START", "a validator asked at GameStart sees per-def data; setup starts synced rules only, each side once") {
	FakeRule synced(true), draw(false), off(true, false);
	std::vector<FakeRule*> rules{&synced, &draw, &off};
	StartGate gate;
	KT1_CHECK(gate.SetupIfNeeded(rules));          // game load, before LuaRules
	KT1_CHECK(synced.Ask(2));                       // GameStart creates units: the validator is asked before any step
	KT1_CHECK_EQ(draw.starts, 0);                   // setup starts no unsynced rule
	KT1_CHECK(!gate.SetupIfNeeded(rules));         // each sim step: nothing left to start
	gate.Unsynced(rules);                           // each drawn frame
	gate.Unsynced(rules);
	KT1_CHECK_EQ(synced.starts, 1);
	KT1_CHECK_EQ(draw.starts, 1);
	KT1_CHECK_EQ(off.starts, 0);
}

KT1_CASE("SIMHOST-START-NOSETUP", "a sim step without setup starts synced rules once; a drawn frame never does") {
	FakeRule synced(true), draw(false);
	std::vector<FakeRule*> rules{&synced, &draw};
	StartGate gate;
	gate.SetupIfNeeded(rules);
	gate.Reset();                                   // the next game, one that never ran setup
	synced.starts = 0;
	gate.Unsynced(rules);
	KT1_CHECK_EQ(synced.starts, 0);
	KT1_CHECK(gate.SetupIfNeeded(rules));           // its first step: the registry logs this
	KT1_CHECK(!gate.SetupIfNeeded(rules));
	gate.Unsynced(rules);
	KT1_CHECK_EQ(synced.starts, 1);
	KT1_CHECK_EQ(draw.starts, 1);
}

KT1_CASE("SIMHOST-START-HEADLESS", "a step with no draw starts the unsynced rules once") {
	FakeRule synced(true), draw(false);
	std::vector<FakeRule*> rules{&synced, &draw};
	StartGate gate;
	gate.SetupIfNeeded(rules);                      // game load
	for (int step = 0; step < 3; ++step) {          // the registry's step: synced fallback, then unsynced
		KT1_CHECK(!gate.SetupIfNeeded(rules));
		gate.Unsynced(rules);
	}
	KT1_CHECK_EQ(synced.starts, 1);
	KT1_CHECK_EQ(draw.starts, 1);
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
		q.Publish(0);
		q.Replay([&](const DisplayEvent& d) { sink += d.id; });
	};
	for (int i = 0; i < 4; ++i) step();
	const long long before = AllocationCount();
	for (int i = 0; i < 1000; ++i) step();
	KT1_CHECK_EQ(AllocationCount() - before, 0LL);
	KT1_CHECK(sink != 0);
}

namespace {
	using Fn = std::function<bool(std::string&)>;
	void AddValidator(Chain<Fn>& chain, int order, const char* name, const char* tag, bool allow, bool check = false) {
		chain.Add(order, name, {name, check, [tag, allow](std::string& seen) { seen += tag; return allow; }});
	}
}

KT1_CASE("SIMHOST-CHAIN", "validators run by (order, name); the first live no ends the chain") {
	Chain<Fn> chain;
	AddValidator(chain, 10, "zeta", "z", true);
	AddValidator(chain, 10, "alpha", "a", true);
	AddValidator(chain, -5, "late", "l", true);
	AddValidator(chain, 20, "tail", "t", true);
	std::string seen;
	WriteGate gate;
	int checked = 0;
	const auto count = [&](const std::string&, bool) { ++checked; };
	KT1_CHECK(AskVeto(chain, gate, count, seen));
	KT1_CHECK_EQ(seen, std::string("lazt"));
	AddValidator(chain, 10, "mid", "m", false);
	seen.clear();
	KT1_CHECK(!AskVeto(chain, gate, count, seen));
	KT1_CHECK_EQ(seen, std::string("lam"));
	KT1_CHECK_EQ(checked, 0);
	const Chain<Fn> none;
	seen.clear();
	KT1_CHECK(AskVeto(none, gate, count, seen) && seen.empty());
}

KT1_CASE("SIMHOST-CHAIN-CHECK", "a check-mode validator never refuses; its answer goes to the comparison") {
	Chain<Fn> chain;
	AddValidator(chain, 1, "probe", "p", false, true);
	AddValidator(chain, 2, "live", "v", true);
	AddValidator(chain, 3, "other", "o", true, true);
	std::string seen, answers;
	WriteGate gate;
	KT1_CHECK(AskVeto(chain, gate, [&](const std::string& system, bool yes) { answers += system + (yes ? "=yes " : "=no "); }, seen));
	KT1_CHECK_EQ(seen, std::string("pvo"));
	KT1_CHECK_EQ(answers, std::string("probe=no other=yes "));
}

namespace {
	struct PairRun {
		CheckTally tally;
		CheckPairs pairs{tally};
		int described = 0;
		void Give(bool native, int id, float answer, const float* input = nullptr, float second = -1.0f) {
			CheckValue v;
			v.v = {answer, second};
			v.count = (second >= 0.0f) ? 2 : 1;
			if (input != nullptr) { v.hasInput = true; v.input = *input; }
			const auto say = [this](const CheckKey&, const CheckValue*, const CheckValue&) { ++described; };
			if (native) pairs.Native(CheckKey{"S", "w", {id, 0, 0}}, v, say);
			else pairs.Lua(CheckKey{"S", "w", {id, 0, 0}}, v, say);
		}
		void Close() { pairs.CloseStep([this](const CheckKey&, const CheckValue*, const CheckValue&) { ++described; }); }
		std::string Total() {
			std::string out;
			tally.Totals([&](const std::string& s) { out += s; });
			return out;
		}
	};
	std::string Counts(int n, int diffs, int unmatched) {
		return "[KT1CHECK] S total n=" + std::to_string(n) + " diffs=" + std::to_string(diffs) + " unmatched=" +
			std::to_string(unmatched) + " writes_skipped=0 cumulative";
	}
}

KT1_CASE("SIMHOST-PAIRS-ALLOW", "a call's native answer is matched inside its scope; never asked is unmatched, not a difference") {
	PairRun r;
	size_t mark = r.pairs.Mark();
	r.Give(true, 7, 1.0f);
	r.Give(false, 7, 1.0f);       // the replaced gadget agrees
	r.pairs.CloseScope(mark);
	mark = r.pairs.Mark();
	r.Give(true, 7, 0.0f);        // another gadget refused first: never asked
	r.pairs.CloseScope(mark);
	mark = r.pairs.Mark();
	r.Give(true, 7, 1.0f);
	r.Give(false, 7, 0.0f);       // disagrees
	r.pairs.CloseScope(mark);
	r.Close();
	KT1_CHECK_EQ(r.Total(), Counts(2, 1, 1));
	KT1_CHECK_EQ(r.described, 1);
	KT1_CHECK(r.pairs.Empty());
}

KT1_CASE("SIMHOST-PAIRS-LUAONLY", "a Lua answer with no native one for its key is a difference, at the step's end") {
	PairRun r;
	const size_t mark = r.pairs.Mark();
	r.Give(true, 7, 1.0f);
	r.Give(false, 8, 1.0f);
	r.pairs.CloseScope(mark);
	KT1_CHECK_EQ(r.Total(), Counts(0, 0, 1));
	r.Close();
	KT1_CHECK_EQ(r.Total(), Counts(1, 1, 1));
}

KT1_CASE("SIMHOST-PAIRS-ORDER", "a value matches in either order; a nested call takes its own answer and leaves the outer one") {
	PairRun r;
	r.Give(false, 3, 2.5f);       // Lua first (a value, no scope)
	r.Give(true, 3, 2.5f);
	const size_t outer = r.pairs.Mark();
	r.Give(true, 9, 1.0f);
	const size_t inner = r.pairs.Mark();
	r.Give(true, 9, 0.0f);        // the same key, asked again inside the outer call
	r.Give(false, 9, 0.0f);
	r.pairs.CloseScope(inner);
	r.Give(false, 9, 1.0f);
	r.pairs.CloseScope(outer);
	r.Close();
	KT1_CHECK_EQ(r.Total(), Counts(3, 0, 0));
}

KT1_CASE("SIMHOST-PAIRS-INPUT", "answers about different inputs are unmatched; the shorter answer decides what is compared") {
	PairRun r;
	const float engine = 40.0f, changed = 20.0f;
	r.Give(true, 1, 10.0f, &engine);
	r.Give(false, 1, 5.0f, &changed);
	r.Give(true, 2, 1.0f, nullptr, 0.0f);
	r.Give(false, 2, 1.0f);
	r.Give(true, 3, 1.0f, &engine, 3.0f);
	r.Give(false, 3, 1.0f, &engine, 4.0f);
	r.Close();
	KT1_CHECK_EQ(r.Total(), Counts(2, 1, 1));
}

KT1_CASE("SIMHOST-PHASE", "phase names and half-open id ranges") {
	constexpr IdRange all = IdRange::All();
	const IdRange half{10, 20};
	KT1_CHECK(all.Contains(0) && all.Contains(1000000));
	KT1_CHECK(half.Contains(10) && half.Contains(19) && !half.Contains(20) && !half.Contains(9));
	KT1_CHECK_EQ(std::string(PhaseName(Phase::FrameStart)), std::string("FrameStart"));
	KT1_CHECK_EQ(std::string(PhaseName(Phase::AfterUnits)), std::string("AfterUnits"));
	KT1_CHECK_EQ(PHASES, static_cast<size_t>(3));
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
	const auto replay = [&]() {
		seen.clear();
		q.Replay([&](const DisplayEvent& d) { seen += std::to_string(d.id) + "@" + std::to_string(d.step) + " "; });
	};
	emit(1);   // a system emits in step 7
	KT1_CHECK_EQ(q.Pending(), static_cast<size_t>(0));
	replay();
	KT1_CHECK(seen.empty());   // the step is not published: nothing to see
	emit(2);
	q.Publish(7);
	emit(3);
	KT1_CHECK_EQ(q.Pending(), static_cast<size_t>(2));   // published, and seen by nobody until Replay
	replay();
	KT1_CHECK_EQ(seen, std::string("1@7 2@7 "));
	replay();
	KT1_CHECK(seen.empty());   // once
	q.Publish(8);
	emit(4);
	q.Publish(9);
	replay();
	KT1_CHECK_EQ(seen, std::string("3@8 4@9 "));   // two steps without a drawn frame: step order
	DisplayEvent d;
	d.SetText(std::string(500, 'x').c_str());
	KT1_CHECK(std::string(d.text).size() == DISPLAY_TEXT - 1);
}

KT1_CASE("SIMHOST-DISPLAY-CAP", "with no replay the published events stay bounded: the oldest go, counted") {
	DisplayQueue q;
	const int total = static_cast<int>(DISPLAY_PUBLISHED_MAX) + 10;
	for (int i = 0; i < total; ++i) {
		DisplayEvent d;
		d.id = i;
		q.Emit(d);
		q.Publish(i);
	}
	KT1_CHECK_EQ(q.Pending(), DISPLAY_PUBLISHED_MAX);
	KT1_CHECK_EQ(q.Dropped(), static_cast<uint64_t>(10));
	int first = -1;
	q.Replay([&](const DisplayEvent& d) { if (first < 0) first = d.id; });
	KT1_CHECK_EQ(first, 10);
}

KT1_CASE("SIMHOST-GATE", "check mode is compute-only: its writes are skipped and counted, a validator's too") {
	CheckTally tally;
	WriteGate gate;
	int written = 0;
	const auto write = [&]() { if (gate.Allow(tally)) ++written; };
	write();   // no system running
	{
		const WriteGate::Scope probe(gate, "S", true);
		write();   // the order
		{
			const WriteGate::Scope live(gate, "Live", false);
			write();   // a nested live system writes
		}
		write();   // the display event
	}
	write();
	KT1_CHECK_EQ(written, 3);
	Chain<std::function<bool()>> chain;
	chain.Add(0, "S", {"S", true, [&]() { write(); return false; }});
	KT1_CHECK(AskVeto(chain, gate, [](const std::string&, bool) {}));
	KT1_CHECK_EQ(written, 3);
	std::string out;
	tally.Totals([&](const std::string& s) { out += s; });
	KT1_CHECK_EQ(out, std::string("[KT1CHECK] S total n=0 diffs=0 unmatched=0 writes_skipped=3 cumulative"));
}

namespace {
	// The host's state, as Host/HostReset.cpp clears it, with the per-system state a rule rebuilds at game setup.
	struct MiniHost {
		EventLog events;
		DisplayQueue display;
		CheckTally tally;
		CheckPairs pairs{tally};
		Chain<std::function<bool()>> chain;
		KT1::Core::SimProbe::State probe;
		KT1::Core::DueWheel wheel;

		// One game: setup, then `steps` steps. Returns everything a step shows.
		std::string Run(int steps) {
			std::string out;
			const int reader = events.AddReader(ALL_EVENTS);
			probe.Init(64);
			wheel.Init(8, 4, 4);
			chain.Add(0, "S", {"S", false, []() { return true; }});
			for (int step = 0; step < steps; ++step) {
				Event e;
				e.type = EventType::UnitCreated;
				e.id = step + 1;
				events.Push(e);
				const EventBatch batch = events.Take(reader);
				batch.ForEachRaised([&](const Event& r) {
					probe.Apply(r);
					out += std::to_string(r.id) + "#" + std::to_string(r.seq) + " ";
				});
				wheel.Schedule(step + 1, step + 1);
				for (const int32_t id : wheel.Take()) out += "due" + std::to_string(id) + " ";
				pairs.Native(CheckKey{"S", "w", {step, 0, 0}}, CheckValue{}, [](auto&&...) {});
				DisplayEvent d;
				d.id = step;
				display.Emit(d);
				pairs.CloseStep([](auto&&...) {});
				tally.CloseStep(step, [&](const std::string& s) { out += s; });
				tally.Totals([&](const std::string& s) { out += s; });
				events.Compact();
				display.Publish(step);
				out += "units=" + std::to_string(probe.units.index.Size()) + " shown=" + std::to_string(display.Pending()) +
					" links=" + std::to_string(chain.Size()) + ";";
			}
			events.Push(Event{});   // left over at shutdown
			display.Emit(DisplayEvent{});
			pairs.Lua(CheckKey{"S", "w", {99, 0, 0}}, CheckValue{}, [](auto&&...) {});
			return out;
		}

		void Shutdown() {
			display.Clear();
			pairs.Clear();
			tally.Clear();
			chain.Clear();
			events.Clear();
		}
	};
}

KT1_CASE("SIMHOST-RESTART", "after a shutdown the host's buffers and counters are empty and a second game starts as a first") {
	MiniHost fresh;
	const std::string first = fresh.Run(1), five = MiniHost().Run(5);
	MiniHost host;
	host.Run(5);
	host.Shutdown();
	KT1_CHECK_EQ(host.events.Wanted(), static_cast<EventMask>(0));
	KT1_CHECK_EQ(host.events.Held(EventType::UnitCreated), static_cast<size_t>(0));
	KT1_CHECK_EQ(host.display.Pending(), static_cast<size_t>(0));
	KT1_CHECK_EQ(host.display.Dropped(), static_cast<uint64_t>(0));
	KT1_CHECK(host.tally.Empty() && host.pairs.Empty() && host.chain.Empty());
	KT1_CHECK_EQ(host.Run(1), first);
	host.Shutdown();
	KT1_CHECK_EQ(host.Run(5), five);
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
	KT1_CHECK_EQ(lines, std::string("[KT1CHECK] Abc total n=60 diffs=30 unmatched=0 writes_skipped=0 cumulative;"
		"[KT1CHECK] Zed total n=5 diffs=0 unmatched=0 writes_skipped=0 cumulative;"));
	KT1_CHECK(CheckTally::SameBits(1.5f, 1.5f) && !CheckTally::SameBits(0.0f, -0.0f));
}
