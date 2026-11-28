/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// Engine-free tests of Core/SimHost. Run: test/run.sh. One "[CASE] <id> PASS|FAIL <text>" line per case.

#include <cstdio>
#include <cstdlib>
#include <functional>
#include <initializer_list>
#include <new>
#include <string>
#include <utility>
#include <vector>

#include "../DisplayQueue.h"
#include "../Ordered.h"
#include "../Phase.h"
#include "../SimEventLog.h"
#include "../StateHash.h"

using namespace KT1::SimHost;

static size_t allocations = 0;
void* operator new(size_t n) { ++allocations; void* p = std::malloc(n ? n : 1); if (!p) std::abort(); return p; }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p, size_t) noexcept { std::free(p); }

static int failed = 0;
static void Case(const std::string& id, bool ok, const std::string& text) {
	std::printf("[CASE] %s %s %s\n", id.c_str(), ok ? "PASS" : "FAIL", text.c_str());
	if (!ok) ++failed;
}

static Event Ev(EventType t, int id, int arg = -1) {
	Event e;
	e.type = t;
	e.id = id;
	e.arg = arg;
	return e;
}

static std::string Raised(const EventBatch& b) {
	std::string s;
	b.ForEachRaised([&](const Event& e) { s += std::to_string((int)e.type) + ":" + std::to_string(e.id) + " "; });
	return s;
}

static std::string ById(const EventBatch& b, EventType t) {
	std::vector<uint32_t> scratch;
	std::string s;
	b.ForEachById(t, scratch, [&](const Event& e) { s += std::to_string(e.id) + "/" + std::to_string(e.arg) + " "; });
	return s;
}

static std::string Tag(EventType t, int id) { return std::to_string((int)t) + ":" + std::to_string(id) + " "; }

static void TestOrder() {
	EventLog log;
	const int r = log.AddReader(ALL_EVENTS);
	log.Push(Ev(EventType::UnitDamaged, 9, 1));
	log.Push(Ev(EventType::UnitDestroyed, 4));
	log.Push(Ev(EventType::UnitDamaged, 2, 2));
	log.Push(Ev(EventType::UnitCreated, 7));
	log.Push(Ev(EventType::UnitDamaged, 9, 3));
	log.Push(Ev(EventType::UnitDamaged, 2, 4));
	const EventBatch b = log.Take(r);
	Case("SH-ORD-1", ById(b, EventType::UnitDamaged) == "2/2 2/4 9/1 9/3 ", "one type: ascending id, equal ids as raised");
	const std::string want = Tag(EventType::UnitDamaged, 9) + Tag(EventType::UnitDestroyed, 4) + Tag(EventType::UnitDamaged, 2)
		+ Tag(EventType::UnitCreated, 7) + Tag(EventType::UnitDamaged, 9) + Tag(EventType::UnitDamaged, 2);
	Case("SH-ORD-2", Raised(b) == want, "across types: the order the engine raised them");
	Case("SH-ORD-3", b.Size(EventType::UnitDamaged) == 4 && b.At(EventType::UnitDamaged, 0).id == 9, "At() is in raise order");
	Case("SH-ORD-4", log.Take(r).Empty(), "a taken event is not taken again");
}

static void TestReaders() {
	EventLog log;
	log.Push(Ev(EventType::UnitCreated, 1));
	Case("SH-RD-1", log.Held(EventType::UnitCreated) == 0, "with no reader nothing is recorded");

	const int a = log.AddReader(MaskOf(EventType::UnitCreated));
	const int c = log.AddReader(MaskOf(EventType::UnitCreated) | MaskOf(EventType::UnitIdle));
	log.Push(Ev(EventType::UnitCreated, 1));
	log.Push(Ev(EventType::UnitFinished, 1));
	log.Push(Ev(EventType::UnitIdle, 5));
	Case("SH-RD-2", log.Held(EventType::UnitFinished) == 0, "a type nobody asked for is not recorded");
	Case("SH-RD-3", Raised(log.Take(a)) == Tag(EventType::UnitCreated, 1), "a reader gets only its mask");

	log.Compact();
	Case("SH-RD-4", log.Held(EventType::UnitCreated) == 1, "compact keeps what a reader has not taken");
	log.Push(Ev(EventType::UnitCreated, 2));
	Case("SH-RD-5", Raised(log.Take(c)) == Tag(EventType::UnitCreated, 1) + Tag(EventType::UnitIdle, 5) + Tag(EventType::UnitCreated, 2),
	     "the late reader gets everything once, in order");
	Case("SH-RD-6", Raised(log.Take(a)) == Tag(EventType::UnitCreated, 2), "the early reader's cursor survives compaction");
	log.Compact();
	Case("SH-RD-7", log.Held(EventType::UnitCreated) == 0 && log.Held(EventType::UnitIdle) == 0, "all taken: all dropped");

	const int late = log.AddReader(ALL_EVENTS);
	log.Push(Ev(EventType::UnitIdle, 8));
	Case("SH-RD-8", Raised(log.Take(late)) == Tag(EventType::UnitIdle, 8), "a new reader starts at now");
}

static void TestReentry() {
	EventLog log;
	const int r = log.AddReader(ALL_EVENTS);
	for (int i = 0; i < 4; ++i) log.Push(Ev(EventType::UnitDestroyed, i));
	const EventBatch b = log.Take(r);
	int seen = 0, sum = 0;
	b.ForEachRaised([&](const Event& e) {
		++seen;
		for (int k = 0; k < 100; ++k) log.Push(Ev(EventType::UnitDestroyed, 100 + e.id));   // forces the list to grow
		sum += e.id;
	});
	Case("SH-RE-1", seen == 4 && sum == 6, "events raised while reading do not enter or break the batch");
	Case("SH-RE-2", log.Take(r).Size(EventType::UnitDestroyed) == 400, "they are in the next batch");
}

// The same sim seconds at either rate: every event is delivered once and in order, whatever the step length.
static void TestRates() {
	for (const int rate : {30, 10}) {
		EventLog log;
		const int r = log.AddReader(ALL_EVENTS);
		std::vector<uint32_t> scratch;
		int pushed = 0, got = 0, lastValue = -1;
		bool inOrder = true;
		for (int step = 0; step < 10 * rate; ++step) {   // 10 s of sim time, 300 events per second
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
		Case("SH-RT-1-" + std::to_string(rate), got == pushed && pushed == 3000 && inOrder,
		     "all events once, in order at " + std::to_string(rate) + " steps per second");
	}
}

static void TestNoAlloc() {
	EventLog log;
	DisplayQueue q;
	const int r = log.AddReader(ALL_EVENTS);
	std::vector<uint32_t> scratch;
	long sink = 0;
	auto step = [&]() {
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
	const size_t before = allocations;
	for (int i = 0; i < 1000; ++i) step();
	const size_t used = allocations - before;
	Case("SH-NA-1", used == 0 && sink != 0, "1000 warm steps: no allocation");
}

static void TestOrdered() {
	Ordered<std::function<bool(std::string&)>> chain;
	auto v = [](const char* tag, bool allow) {
		return [tag, allow](std::string& seen) { seen += tag; return allow; };
	};
	chain.Add(10, "zeta", v("z", true));
	chain.Add(10, "alpha", v("a", true));
	chain.Add(-5, "late", v("l", true));
	chain.Add(20, "tail", v("t", true));
	std::string seen;
	const bool all = AllAllow(chain, seen);
	Case("SH-CH-1", all && seen == "lazt", "order, then name: not the order of registration");
	chain.Add(10, "mid", v("m", false));
	seen.clear();
	const bool denied = !AllAllow(chain, seen);
	Case("SH-CH-2", denied && seen == "lam", "the first no ends the chain");
	Ordered<std::function<bool(std::string&)>> none;
	seen.clear();
	const bool empty = AllAllow(none, seen);
	Case("SH-CH-3", empty && seen.empty(), "an empty chain allows");
}

static void TestPhase() {
	constexpr IdRange all = IdRange::All();
	const IdRange half{10, 20};
	Case("SH-PH-1", all.Contains(0) && all.Contains(1000000), "the full range holds every id");
	Case("SH-PH-2", half.Contains(10) && half.Contains(19) && !half.Contains(20) && !half.Contains(9), "a range is half open");
	Case("SH-PH-3", std::string(PhaseName(Phase::FrameStart)) == "FrameStart" && std::string(PhaseName(Phase::FrameEnd)) == "FrameEnd", "phase names");
}

static void TestHash() {
	auto h = [](std::initializer_list<std::pair<int, float>> rows) {
		StateHash s;
		for (const auto& r : rows) { s.Add(r.first); s.Add(r.second); }
		return s.Get();
	};
	Case("SH-HS-1", h({{1, 2.0f}, {3, 4.0f}}) == h({{1, 2.0f}, {3, 4.0f}}), "the same state hashes the same");
	Case("SH-HS-2", h({{1, 2.0f}, {3, 4.0f}}) != h({{3, 4.0f}, {1, 2.0f}}), "order counts");
	Case("SH-HS-3", h({{1, 0.0f}}) != h({{1, -0.0f}}), "bits count: -0 differs from 0");
	Case("SH-HS-4", h({{1, 2.0f}}) != h({{2, 2.0f}}), "the id counts");
}

static void TestDisplay() {
	DisplayQueue q;
	auto emit = [&](int id) { DisplayEvent d; d.kind = DisplayKind::Sound; d.id = id; q.Emit(d); };
	std::string seen;
	auto replay = [&]() { seen.clear(); q.Replay([&](const DisplayEvent& d) { seen += std::to_string(d.id) + " "; }); };
	emit(1);
	replay();
	Case("SH-DQ-1", seen.empty(), "nothing is replayed before the step is published");
	emit(2);
	q.Publish();
	emit(3);
	replay();
	Case("SH-DQ-2", seen == "1 2 ", "one step, in emit order; the next step's events wait");
	q.Publish();
	emit(4);
	q.Publish();
	replay();
	Case("SH-DQ-3", seen == "3 4 ", "two steps before a replay: both, in order");
	replay();
	Case("SH-DQ-4", seen.empty(), "replayed once");
	DisplayEvent d;
	d.SetText(std::string(500, 'x').c_str());
	Case("SH-DQ-5", std::string(d.text).size() == DISPLAY_TEXT - 1, "long text is cut");
}

int main() {
	TestOrder();
	TestReaders();
	TestReentry();
	TestRates();
	TestNoAlloc();
	TestOrdered();
	TestPhase();
	TestHash();
	TestDisplay();
	std::printf("simhost_test: %d failed\n", failed);
	return failed == 0 ? 0 : 1;
}
