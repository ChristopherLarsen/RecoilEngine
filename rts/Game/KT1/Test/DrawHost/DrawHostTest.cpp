/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 draw host (LUA-FREE-WORKPACKAGES F06), engine-free: IdBits, SelectionTracker, InstanceStore (the range bookkeeping
// of InstanceBatch), PhaseList and Host/Snapshot<T>. Run: python -I tools/engine/core_tests.py --tree rc-<yours> --only DRAWHOST-

#include "../KT1Test.h"

#include "Game/KT1/Core/DrawHost/IdBits.h"
#include "Game/KT1/Core/DrawHost/InstanceStore.h"
#include "Game/KT1/Core/DrawHost/PhaseList.h"
#include "Game/KT1/Core/DrawHost/SelectionTracker.h"
#include "Game/KT1/Host/Snapshot.h"

#include <cstring>
#include <string>
#include <vector>

using namespace KT1;
using KT1::Test::AllocationCount;

namespace {

struct Inst { float x, y; };

bool Same(const std::vector<SlotRange>& a, const std::vector<SlotRange>& b) {
	if (a.size() != b.size()) return false;
	for (size_t i = 0; i < a.size(); i++) {
		if (a[i].first != b[i].first || a[i].count != b[i].count) return false;
	}
	return true;
}

struct Lcg {
	uint32_t s;
	int Below(int n) { s = s * 1664525u + 1013904223u; return static_cast<int>((s >> 8) % static_cast<uint32_t>(n)); }
};

} // namespace

KT1_CASE("DRAWHOST-IDBITS", "ids come out ascending; ids out of range are ignored") {
	IdBits b;
	b.Init(200);
	for (int id : {130, 3, 63, 64, 199, 3, -1, 200}) b.Set(id);
	std::vector<int> out;
	b.AppendTo(out);
	KT1_CHECK((out == std::vector<int>{3, 63, 64, 130, 199}));
	KT1_CHECK_EQ(b.Count(), 5);
	KT1_CHECK(b.Test(64) && !b.Test(65) && !b.Test(200) && !b.Test(-1));
	b.Clear();
	KT1_CHECK(b.Count() == 0 && !b.Test(3));
}

KT1_CASE("DRAWHOST-SELECTION", "the change counter moves only when the selected set differs") {
	SelectionTracker s;
	s.Init(100);
	KT1_CHECK(!s.Update(std::vector<int>{}) && s.Changes() == 0);
	KT1_CHECK(s.Update(std::vector<int>{7, 2, 50}) && s.Changes() == 1);
	KT1_CHECK((s.Ids() == std::vector<int>{2, 7, 50}));
	KT1_CHECK(!s.Update(std::vector<int>{50, 7, 2}) && s.Changes() == 1);   // same set, other order
	KT1_CHECK(s.Update(std::vector<int>{7, 2, 51}) && s.Changes() == 2);    // same size, other set
	KT1_CHECK(s.Contains(51) && !s.Contains(50));
	KT1_CHECK(s.Update(std::vector<int>{}) && s.Changes() == 3 && s.Ids().empty());
}

KT1_CASE("DRAWHOST-STORE-CHANGES", "each buffer region gets only what changed, joined over small gaps") {
	InstanceStore<Inst> s;
	s.Init(64, 3);
	std::vector<SlotRange> r;
	for (int id = 0; id < 10; id++) s.Set(id, {float(id), 0.0f});
	for (int region = 0; region < 3; region++) {
		s.TakeStale(region, 0, r);
		KT1_CHECK(Same(r, {{0, 10}}));
	}
	for (int id = 0; id < 10; id++) s.Set(id, {float(id), 0.0f});       // nothing changed
	s.TakeStale(0, 0, r);
	KT1_CHECK(r.empty());
	s.Set(4, {40.0f, 0.0f});
	s.Set(8, {80.0f, 0.0f});
	s.TakeStale(0, 0, r);
	KT1_CHECK(Same(r, {{4, 1}, {8, 1}}));
	s.TakeStale(0, 0, r);
	KT1_CHECK(r.empty());                                                // region 0 holds them now
	s.TakeStale(1, 3, r);
	KT1_CHECK(Same(r, {{4, 5}}));                                        // joined over a gap of 3
	s.TakeStale(2, 2, r);
	KT1_CHECK(Same(r, {{4, 1}, {8, 1}}));
	s.Set(64, {1.0f, 1.0f});
	s.Set(-1, {1.0f, 1.0f});
	KT1_CHECK_EQ(s.Size(), 10);
}

KT1_CASE("DRAWHOST-STORE-REMOVE", "removal keeps the instances packed and uploads the moved one") {
	InstanceStore<Inst> s;
	s.Init(16, 1);
	std::vector<SlotRange> r;
	for (int id = 0; id < 5; id++) s.Set(id, {float(id), 0.0f});
	s.TakeStale(0, 0, r);
	s.Remove(1);                                                         // the last (id 4) moves into slot 1
	KT1_CHECK(s.Size() == 4 && s.IdAt(1) == 4 && s.Data()[1].x == 4.0f && !s.Has(1));
	s.TakeStale(0, 0, r);
	KT1_CHECK(Same(r, {{1, 1}}));
	s.Remove(3);                                                         // the last slot: nothing to upload
	s.TakeStale(0, 0, r);
	KT1_CHECK(r.empty() && s.Size() == 3);
	s.RemoveUnless([](int id) { return id == 4; });
	KT1_CHECK(s.Size() == 1 && s.IdAt(0) == 4 && s.Data()[0].x == 4.0f);
	s.TakeStale(0, 0, r);
	KT1_CHECK(Same(r, {{0, 1}}));
	s.MarkAll();
	s.TakeStale(0, 0, r);
	KT1_CHECK(Same(r, {{0, 1}}));
}

// a ring of region copies, written only through TakeStale, must equal the store whenever its turn comes; also after
// frames in which nothing was drawn (a system switched off and on again)
KT1_CASE("DRAWHOST-STORE-REGIONS", "5000 random frames: a region fed by changed ranges equals the store") {
	constexpr int N = 128, REGIONS = 3;
	InstanceStore<Inst> s;
	s.Init(N, REGIONS);
	std::vector<std::vector<Inst>> gpu(REGIONS, std::vector<Inst>(N, Inst{-1.0f, -1.0f}));
	std::vector<SlotRange> r;
	Lcg rnd{12345};
	int mismatches = 0, drawn = 0;
	for (int frame = 0; frame < 5000; frame++) {
		for (int op = 0, n = rnd.Below(12); op < n; op++) {
			const int id = rnd.Below(N);
			if (rnd.Below(4) == 0) s.Remove(id);
			else s.Set(id, {float(rnd.Below(5)), float(id)});
		}
		if ((frame / 100) % 3 == 2) continue;   // not drawn
		const int region = drawn++ % REGIONS;
		s.TakeStale(region, rnd.Below(4), r);
		for (const SlotRange& range : r) {
			for (int i = range.first; i < range.first + range.count; i++) gpu[region][i] = s.Data()[i];
		}
		mismatches += std::memcmp(gpu[region].data(), s.Data(), s.Size() * sizeof(Inst)) != 0;
	}
	KT1_CHECK_EQ(mismatches, 0);
}

KT1_CASE("DRAWHOST-PHASES", "systems run by order then name; switched-off ones not at all") {
	int a = 0, b = 0, c = 0;
	PhaseList<int> l;
	KT1_CHECK(l.Add(&b, "beta", 0, PhaseBit(DrawPhase::World) | PhaseBit(DrawPhase::Screen)));
	KT1_CHECK(l.Add(&c, "first", -1, PhaseBit(DrawPhase::World)));
	KT1_CHECK(l.Add(&a, "alpha", 0, PhaseBit(DrawPhase::World)));
	KT1_CHECK(!l.Add(&a, "again", 0, 0) && !l.Add(&b, "alpha", 0, 0));
	std::string seen;
	const auto walk = [&](DrawPhase p) { seen.clear(); l.ForPhase(p, [&](const auto& e) { seen += e.name + " "; }); };
	walk(DrawPhase::World);
	KT1_CHECK_EQ(seen, std::string("first alpha beta "));
	walk(DrawPhase::Screen);
	KT1_CHECK_EQ(seen, std::string("beta "));
	KT1_CHECK(l.Wants(DrawPhase::Screen) && !l.Wants(DrawPhase::MiniMap));
	KT1_CHECK(l.SetEnabled("beta", false) && !l.Wants(DrawPhase::Screen) && l.AnyEnabled());
	KT1_CHECK(!l.SetEnabled("nobody", true));
	KT1_CHECK_EQ(std::string(PhaseName(DrawPhase::PreDraw)), std::string("PreDraw"));
	KT1_CHECK_EQ(std::string(PhaseName(DrawPhase::FrameEnd)), std::string("FrameEnd"));
}

// drawn frames at 60 Hz over a sim at the pass's rate: the pair read is always the last two committed steps
KT1_RATE_CASE("DRAWHOST-SNAPSHOT-RATE", "two steps and their numbers, with the step published in slices") {
	constexpr int DRAWN_PER_SECOND = 60;
	Snapshot<float> s;
	s.Init(8);
	const int framesPerStep = DRAWN_PER_SECOND / testCase.StepsPerSecond();
	int committed = 0, bad = 0;
	for (int frame = 0; frame < 10 * DRAWN_PER_SECOND; frame++) {
		const int slice = frame % framesPerStep;
		const int step = frame / framesPerStep + 1;
		if (slice == 0) s.Begin(step);
		if (slice == 0) s.Put(1, 7, float(step));                       // ids 1 and 2 on different drawn frames
		if (slice == framesPerStep - 1) { s.Put(2, 8, float(step) * 2.0f); s.Commit(); committed = step; }
		if (committed < 2) continue;
		const float* c = s.Current(1);
		const float* p = s.Previous(1);
		bad += !(s.Step() == committed && s.PrevStep() == committed - 1 && c && p && *c == float(committed) && *p == float(committed - 1));
		bad += !(s.Current(2) && *s.Current(2) == float(committed) * 2.0f);
	}
	KT1_CHECK_EQ(bad, 0);
}

KT1_CASE("DRAWHOST-SNAPSHOT-IDS", "new, dead and reused ids") {
	Snapshot<int> s;
	s.Init(4);
	KT1_CHECK(!s.Ready() && s.Current(0) == nullptr && s.Previous(0) == nullptr);
	s.Put(0, 1, 5);                                                      // outside a step: ignored
	s.Begin(1); s.Put(0, 1, 10); s.Put(1, 2, 11); s.Put(9, 1, 1); s.Put(-1, 1, 1); s.Commit();
	KT1_CHECK(s.Ready() && !s.HasPrevious() && *s.Current(0) == 10 && s.Previous(0) == nullptr);
	KT1_CHECK((s.Ids() == std::vector<int>{0, 1}));
	s.Begin(2); s.Put(0, 3, 20); s.Put(2, 4, 22); s.Put(2, 4, 23);        // id 0 reused, id 1 died, id 2 born
	KT1_CHECK(s.Step() == 1 && *s.Current(0) == 10);                      // nothing shows before Commit
	s.Commit();
	KT1_CHECK(s.HasPrevious() && *s.Current(0) == 20 && s.Previous(0) == nullptr);
	KT1_CHECK(s.Current(1) == nullptr && s.Previous(1) == nullptr && *s.Current(2) == 23);
	KT1_CHECK((s.Ids() == std::vector<int>{0, 2}));
	s.Begin(3); s.Put(2, 4, 30); s.Commit();
	KT1_CHECK(s.Current(0) == nullptr && *s.Previous(2) == 23);
}

KT1_CASE("DRAWHOST-SNAPSHOT-BOARD", "a published snapshot is found by its value type") {
	struct Other { int v; };
	Snapshot<int> a;
	Snapshot<Other> b;
	SnapshotBoard board;
	KT1_CHECK(board.Find<int>() == nullptr);
	board.Publish(a);
	board.Publish(b);
	KT1_CHECK(board.Find<int>() == &a && board.Find<Other>() == &b);
	board.Clear();
	KT1_CHECK(board.Find<int>() == nullptr);
}

KT1_CASE("DRAWHOST-NOALLOC", "no allocation per drawn frame after Init") {
	SelectionTracker sel;
	InstanceStore<Inst> store;
	Snapshot<Inst> snap;
	IdBits visible;
	constexpr int N = 4096;
	sel.Init(N); store.Init(N, 3); snap.Init(N); visible.Init(N);
	std::vector<SlotRange> ranges;
	ranges.reserve(N);
	std::vector<int> chosen, list;
	chosen.reserve(N);
	list.reserve(N);
	const long long before = AllocationCount();
	for (int frame = 0; frame < 100; frame++) {
		chosen.clear();
		list.clear();
		visible.Clear();
		for (int id = frame % 7; id < N; id += 3) { chosen.push_back(id); visible.Set(id); }
		visible.AppendTo(list);
		sel.Update(chosen);
		store.RemoveUnless([&](int id) { return visible.Test(id); });
		for (const int id : list) store.Set(id, {float(frame), float(id)});
		store.TakeStale(frame % 3, 4, ranges);
		snap.Begin(frame);
		for (const int id : list) snap.Put(id, 1, {0.0f, 0.0f});
		snap.Commit();
	}
	KT1_CHECK_EQ(AllocationCount(), before);
}
