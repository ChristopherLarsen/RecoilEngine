// Engine-free tests for KT1 Core/Common (LUA-FREE-WORKPACKAGES F02). Run: ./run.sh
// Each suite runs once at 30 and once at 10 steps per second. "No allocation in steady state" is checked by counting
// calls to the global operator new between a warm-up and the end of a steady run (AllocCount).

#include "../Column.h"
#include "../DenseIndex.h"
#include "../DetMath.h"
#include "../DueWheel.h"
#include "../EventBuffer.h"
#include "../FixedPool.h"
#include "../Rate.h"
#include "../SortedIdList.h"

#include <cstdio>
#include <cstdlib>
#include <new>
#include <set>
#include <vector>

using namespace KT1::Core;

// ---- allocation counter ---------------------------------------------------------------------------------------------
static long long allocs = 0;
void* operator new(std::size_t n) {
	++allocs;
	if (void* p = std::malloc(n ? n : 1))
		return p;
	throw std::bad_alloc();
}
void* operator new[](std::size_t n) { return operator new(n); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
void operator delete[](void* p, std::size_t) noexcept { std::free(p); }
static long long AllocCount() { return allocs; }

// ---- checks ---------------------------------------------------------------------------------------------------------
static int fails = 0;
static int checks = 0;
static int sps = 30;
#define CHECK(c, msg) do { ++checks; if (!(c)) { ++fails; std::printf("FAIL [%d sps] %s (line %d)\n", sps, msg, __LINE__); } } while (0)

// a small deterministic generator for the mixed sequences (not the engine's RNG)
struct Lcg {
	uint32_t s;
	uint32_t Next() { s = s * 1664525u + 1013904223u; return s >> 8; }
	int32_t Below(int32_t n) { return static_cast<int32_t>(Next() % static_cast<uint32_t>(n)); }
};

// ---- Rate -----------------------------------------------------------------------------------------------------------
static void TestRate() {
	CHECK(Rate::StepsPerSecond() == sps, "rate set");
	CHECK(Rate::Every(0.5) == sps / 2, "0.5 s period is half the rate");
	CHECK(Rate::Every(1.0) == sps, "1 s period is the rate");
	CHECK(Rate::Every(0.0) == 1, "0 s period is every step");
	CHECK(Rate::Every(0.01) == 1, "a period never under 1");
	CHECK(Rate::Steps(0.0) == 0, "0 s delay is this step");
	CHECK(Rate::Steps(2.0) == 2 * sps, "2 s delay");
	CHECK(Rate::Steps(-1.0) == 0, "a delay never under 0");
	CHECK(Rate::Seconds(Rate::Every(3.0)) == 3.0, "steps back to seconds");
	// rounding is half away from zero: 0.25 s is 7.5 steps at 30 (8) and 2.5 at 10 (3)
	CHECK(Rate::Every(0.25) == (sps == 30 ? 8 : 3), "0.25 s rounds half away from zero");
	CHECK(Rate::Every(0.1) == sps / 10, "0.1 s");
}

// ---- DenseIndex + Column --------------------------------------------------------------------------------------------
static void TestDenseIndex() {
	DenseIndex index;
	index.Init(64);
	Column<int> hp;
	hp.Init(index, -1);

	// add 5, 3, 9, 1: slots in add order
	const int ids[] = {5, 3, 9, 1};
	for (int id: ids) {
		const auto s = index.Add(id);
		hp[s] = id * 10;
	}
	CHECK(index.Size() == 4, "size after add");
	CHECK(index.SlotOf(5) == 0 && index.SlotOf(1) == 3, "slots in add order");
	CHECK(index.Add(3) == 1, "re-add keeps the slot");
	CHECK(index.Add(64) == DenseIndex::NONE && index.Add(-1) == DenseIndex::NONE, "out of range refused");

	// remove 3 (slot 1): the last (id 1, slot 3) moves into slot 1, the column follows
	const auto m = index.Remove(3);
	hp.Apply(m);
	CHECK(m.from == 3 && m.to == 1, "swap-remove move");
	CHECK(index.SlotOf(1) == 1 && hp[1] == 10, "moved id keeps its value");
	CHECK(hp[3] == -1, "freed slot is blank");
	CHECK(!index.Has(3) && index.SlotOf(3) == DenseIndex::NONE, "removed id gone");
	CHECK(index.Remove(3).from == DenseIndex::NONE, "remove twice is a no-op");

	// removing the last slot moves nothing
	const auto mLast = index.Remove(9);   // 9 holds the last slot (2)
	hp.Apply(mLast);
	CHECK(mLast.from == mLast.to && index.Size() == 2, "remove last");

	// ascending iteration
	index.Add(2);
	hp[index.SlotOf(2)] = 20;
	{
		const auto asc = index.AscendingIds();
		const std::vector<int> got(asc.begin(), asc.end());
		CHECK((got == std::vector<int>{1, 2, 5}), "ascending ids");
	}

	// a mixed sequence against a std::set model; every value must still sit with its id
	std::set<int> model(index.SlotIds().begin(), index.SlotIds().end());
	Lcg rng{static_cast<uint32_t>(sps)};
	bool valuesOk = true, orderOk = true;
	for (int i = 0; i < 4000; ++i) {
		const int id = rng.Below(64);
		if (rng.Below(3) != 0) {
			if (!index.Has(id)) { hp[index.Add(id)] = id * 10; model.insert(id); }
		} else {
			hp.Apply(index.Remove(id));
			model.erase(id);
		}
		if (i % 97 == 0) {
			const auto asc = index.AscendingIds();
			orderOk = orderOk && std::vector<int>(asc.begin(), asc.end()) == std::vector<int>(model.begin(), model.end());
		}
	}
	for (int s = 0; s < index.Size(); ++s)
		valuesOk = valuesOk && hp[s] == index.IdOf(s) * 10;
	for (int s = index.Size(); s < index.Capacity(); ++s)
		valuesOk = valuesOk && hp[s] == -1;
	CHECK(static_cast<size_t>(index.Size()) == model.size(), "mixed: size");
	CHECK(orderOk, "mixed: ascending order");
	CHECK(valuesOk, "mixed: values follow ids, unused slots blank");

	// steady state: no allocation
	const long long before = AllocCount();
	long long sum = 0;
	for (int i = 0; i < 20000; ++i) {
		const int id = rng.Below(64);
		if (rng.Below(2) == 0) { if (!index.Has(id)) hp[index.Add(id)] = id * 10; }
		else hp.Apply(index.Remove(id));
		if (i % 50 == 0)
			for (const int a: index.AscendingIds()) sum += a;
		for (const int v: hp.Live(index)) sum += v;
	}
	CHECK(AllocCount() == before, "DenseIndex/Column: no allocation in steady state");
	CHECK(sum > 0, "steady run did work");
}

// ---- SortedIdList ---------------------------------------------------------------------------------------------------
static void TestSortedIdList() {
	SortedIdList list;
	list.Init(32);
	for (int id: {7, 2, 9, 2, 4})
		list.Insert(id);
	CHECK((std::vector<int>(list.begin(), list.end()) == std::vector<int>{2, 4, 7, 9}), "sorted, no duplicates");
	CHECK(!list.Insert(4) && list.Erase(4) && !list.Erase(4), "insert/erase report");
	CHECK(list.Contains(9) && !list.Contains(4), "contains");

	Lcg rng{static_cast<uint32_t>(sps) * 7u};
	const long long before = AllocCount();
	for (int i = 0; i < 20000; ++i) {
		const int id = rng.Below(32);
		if (rng.Below(2) == 0) list.Insert(id); else list.Erase(id);
	}
	bool ascending = true;
	for (int i = 1; i < list.Size(); ++i)
		ascending = ascending && list.Ids()[i - 1] < list.Ids()[i];
	CHECK(ascending, "mixed: ascending");
	CHECK(AllocCount() == before, "SortedIdList: no allocation in steady state");
}

// ---- EventBuffer ----------------------------------------------------------------------------------------------------
struct Hit { int32_t target; int32_t attacker; double damage; };

static void TestEventBuffer() {
	EventBuffer<Hit> hits;
	hits.Init(16);
	hits.Push({3, 1, 10.0});
	hits.Emplace(Hit{1, 2, 20.0});
	hits.Push({2, 3, 30.0});
	CHECK(hits.Pending().size() == 3 && hits.Ready().empty(), "pending before flip");
	CHECK(hits.Pending()[0].target == 3 && hits.Pending()[1].target == 1 && hits.Pending()[2].target == 2, "push order");
	hits.Flip();
	CHECK(hits.Pending().empty() && hits.Ready().size() == 3, "flip moves to ready");
	CHECK(hits.Ready()[0].target == 3 && hits.Ready()[2].damage == 30.0, "ready keeps push order");
	hits.Push({9, 9, 1.0});
	hits.Flip();
	CHECK(hits.Ready().size() == 1 && hits.Ready()[0].target == 9, "old ready dropped");
	CHECK(hits.HighWater() == 3, "high water");

	// steady state: one step of events every step for 10 s of game time
	const long long before = AllocCount();
	const int steps = Rate::Steps(10.0);
	bool order = true;
	for (int step = 0; step < steps; ++step) {
		const int n = step % 16;
		for (int i = 0; i < n; ++i)
			hits.Push({i, step, 1.0});
		hits.Flip();
		for (int i = 0; i < static_cast<int>(hits.Ready().size()); ++i)
			order = order && hits.Ready()[i].target == i && hits.Ready()[i].attacker == step;
	}
	CHECK(order, "per-step order over 10 s");
	CHECK(AllocCount() == before, "EventBuffer: no allocation in steady state");
}

// ---- DueWheel -------------------------------------------------------------------------------------------------------
static std::vector<int> Ids(std::span<const int32_t> s) { return {s.begin(), s.end()}; }

// wrap: an 8-step ring; dues before, at and after the ring's end, and far past it
static void TestDueWheelWrap() {
	{
		DueWheel w;
		w.Init(8, 8, 8);
		w.Schedule(40, 7);
		w.Schedule(41, 8);    // ring end at the first Take's view (steps 0..7 in the ring, 8 far)
		w.Schedule(42, 9);
		w.Schedule(43, 15);
		w.Schedule(44, 16);
		w.Schedule(45, 30);   // far: enters the ring at step 22
		w.Schedule(12, 3);
		w.Schedule(10, 3);    // same step, out of order
		w.Schedule(11, 3);
		w.Schedule(10, 3);    // duplicate
		std::vector<std::vector<int>> got(32);
		for (int step = 0; step < 32; ++step)
			got[step] = Ids(w.Take());
		CHECK(w.Now() == 31, "now after 32 takes");
		CHECK((got[3] == std::vector<int>{10, 11, 12}), "bucket sorted and unique");
		CHECK((got[7] == std::vector<int>{40}), "due 7");
		CHECK((got[8] == std::vector<int>{41}), "due 8 (first wrap)");
		CHECK((got[9] == std::vector<int>{42}), "due 9");
		CHECK((got[15] == std::vector<int>{43}) && (got[16] == std::vector<int>{44}), "due 15, 16 (second wrap)");
		CHECK((got[30] == std::vector<int>{45}), "far due 30");
		int other = 0;
		for (int step = 0; step < 32; ++step)
			if (step != 3 && step != 7 && step != 8 && step != 9 && step != 15 && step != 16 && step != 30)
				other += static_cast<int>(got[step].size());
		CHECK(other == 0, "nothing on other steps");
		CHECK(w.Waiting() == 0, "all taken");
	}
}

// past dues go to the next step; cancel in the ring and far
static void TestDueWheelCancel() {
	{
		DueWheel w;
		w.Init(4, 4, 4, 100);
		w.Schedule(1, 50);
		w.Schedule(2, 103);
		w.Schedule(3, 200);
		CHECK(w.Cancel(2, 103) && !w.Cancel(2, 103), "cancel in ring");
		CHECK(w.Cancel(3, 200) && !w.Cancel(3, 200), "cancel far");
		CHECK((Ids(w.Take()) == std::vector<int>{1}), "a past due is due on the next step");
		CHECK(w.Take().empty() && w.Take().empty() && w.Take().empty(), "cancelled entries never come");
	}
}

// periodic visits: unit u every Rate::Every(period_s), first visit at step u % period; checked against arithmetic
static void TestDueWheelPeriodic() {
	{
		const int units = 300;
		const double periods_s[] = {0.5, 1.0, 2.0};   // three classes of unit
		DueWheel w;
		w.Init(Rate::Every(1.0), units, units);   // a 1 s ring: the 2 s class goes through the overflow
		std::vector<int> period(units);
		for (int u = 0; u < units; ++u) {
			period[u] = Rate::Every(periods_s[u % 3]);
			w.Schedule(u, u % period[u]);
		}
		const int steps = Rate::Steps(20.0);
		bool exact = true, sorted = true;
		long long visits = 0;
		const long long before = AllocCount();
		for (int step = 0; step < steps; ++step) {
			const auto due = w.Take();
			int last = -1, n = 0;
			for (const int u: due) {
				sorted = sorted && u > last;
				last = u;
				exact = exact && (step % period[u]) == (u % period[u]);
				w.Schedule(u, step + period[u]);
				++n;
			}
			int want = 0;
			for (int u = 0; u < units; ++u)
				want += (step % period[u]) == (u % period[u]);
			exact = exact && n == want;
			visits += n;
		}
		CHECK(exact, "periodic: each unit exactly on its steps, over 20 s");
		CHECK(sorted, "periodic: ascending ids each step");
		CHECK(visits > 0, "periodic: visits");
		CHECK(AllocCount() == before, "DueWheel: no allocation in steady state");
	}
}

// ---- DetMath --------------------------------------------------------------------------------------------------------
static void TestDetMath() {
	using namespace DetMath;
	CHECK(Mod(-1.0, 3.0) == 2.0 && Mod(5.5, 2.0) == 1.5 && Mod(7.0, -3.0) == -2.0, "Lua 5.1 mod");
	CHECK(RoundHalfUp(2.5) == 3.0 && RoundHalfUp(-0.5) == 0.0 && RoundHalfUp(-1.5) == -1.0, "floor(x + 0.5)");
	CHECK(Floor(-0.25) == -1.0, "floor");
	CHECK(Lerp(1.0, 3.0, 0.25) == 1.5 && Lerp(2.0, 2.0, 0.7) == 2.0, "lerp");
	CHECK(Clamp(5.0, 0.0, 1.0) == 1.0 && Clamp(-5.0, 0.0, 1.0) == 0.0 && Clamp(0.5, 0.0, 1.0) == 0.5, "clamp");
	CHECK(Dist2(3.0, 4.0) == 5.0 && Dist3(2.0, 3.0, 6.0) == 7.0, "dist");
	CHECK(Dot2(1.0, 2.0, 3.0, 4.0) == 11.0 && Dot3(1.0, 2.0, 3.0, 4.0, 5.0, 6.0) == 32.0, "dot");
	// order is pinned: front to back gives 0 here, back to front would give 1
	const double v[] = {1e16, 1.0, -1e16};
	CHECK(Sum(v) == 0.0, "sum front to back");
	// a value the Lua would compute through a + (b - a) * t, not a * (1 - t) + b * t
	const double a = 0.1, b = 0.7, t = 0.3;
	CHECK(Lerp(a, b, t) == a + (b - a) * t, "lerp is the Lua form");
	// a rate-dependent use: per-step decay over 1 s ends at the same value at 30 and 10 within one step's rounding
	double hp = 100.0;
	const double perStep = 10.0 * Rate::StepSeconds();   // 10 per second
	for (int i = 0; i < Rate::Steps(1.0); ++i)
		hp = Clamp(hp - perStep, 0.0, 100.0);
	CHECK(std::fabs(hp - 90.0) < 1e-9, "rate-agnostic decay");
}

// ---- FixedPool ------------------------------------------------------------------------------------------------------
struct Shot { int32_t owner = -1; double x = 0.0; };

static void TestFixedPool() {
	FixedPool<Shot> pool;
	pool.Init(3);
	const auto a = pool.Acquire(), b = pool.Acquire(), c = pool.Acquire();
	CHECK(a == 0 && b == 1 && c == 2, "fresh pool hands out 0, 1, 2");
	CHECK(pool.Acquire() == FixedPool<Shot>::NONE, "full");
	pool[b].owner = 7;
	CHECK(pool.Release(b) && !pool.Release(b), "release once");
	const auto d = pool.Acquire();
	CHECK(d == 1 && pool[d].owner == -1, "last released reused first, blank");
	std::vector<int> live;
	pool.ForEachLive([&](int h, Shot&) { live.push_back(h); });
	CHECK((live == std::vector<int>{0, 1, 2}), "live ascending");

	FixedPool<Shot> big;
	big.Init(256);
	Lcg rng{static_cast<uint32_t>(sps) * 13u};
	std::vector<int> held;
	held.reserve(256);
	const long long before = AllocCount();
	for (int step = 0; step < Rate::Steps(30.0); ++step) {
		for (int i = 0; i < 4; ++i) {
			const auto h = big.Acquire();
			if (h != FixedPool<Shot>::NONE) { big[h].owner = step; held.push_back(h); }
		}
		while (!held.empty() && rng.Below(2) == 0) {
			const size_t k = static_cast<size_t>(rng.Below(static_cast<int>(held.size())));
			big.Release(held[k]);
			held[k] = held.back();
			held.pop_back();
		}
	}
	CHECK(big.Size() == static_cast<int>(held.size()), "pool count");
	CHECK(AllocCount() == before, "FixedPool: no allocation in steady state");
}

int main() {
	{
		const long long before = AllocCount();
		std::vector<int> probe(4);
		probe[0] = 1;
		CHECK(AllocCount() >= before + 1, "the allocation counter sees heap use");
	}
	CHECK(!Rate::IsSet(), "no default rate: the host must set it");
	for (const int rate: {30, 10}) {
		sps = rate;
		Rate::SetStepsPerSecond(rate);
		TestRate();
		TestDenseIndex();
		TestSortedIdList();
		TestEventBuffer();
		TestDueWheelWrap();
		TestDueWheelCancel();
		TestDueWheelPeriodic();
		TestDetMath();
		TestFixedPool();
	}
	std::printf("KT1 Core/Common: %d checks, %d failed (30 and 10 steps per second)\n", checks, fails);
	return fails == 0 ? 0 : 1;
}
