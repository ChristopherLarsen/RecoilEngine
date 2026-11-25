// Engine-free tests for KT1 rule Tiers (PRF-002 v3 R7 cases I-M and every promotion path). Run: ./run.sh
#include "../KT1TiersCore.h"

#include <cstdio>
#include <functional>
#include <set>
#include <vector>

using namespace KT1::TiersCore;

static int fails = 0;
#define CHECK(c, msg) do { if (!(c)) { ++fails; std::printf("FAIL %s (line %d)\n", msg, __LINE__); } } while (0)

// One unit through frames 0..last. events(f, facts, live) edits the state the unit shows during frame f (live) and at its
// end (facts). Returns the frames on which the unit is updated (not asleep), as the engine hooks see it.
static std::vector<int> Run(int id, int period, int last, std::function<void(int, Facts&, Live&)> events) {
	std::vector<int> updated;
	Facts facts; facts.idle = true; facts.far = true;
	bool byte = Decide(facts, 0, id, period);   // the state before tick 0 (case I counts from an idle start)
	for (int f = 0; f <= last; ++f) {
		Live live;
		Facts end = facts; end.changed = false;
		events(f, end, live);
		const bool asleep = byte && !Wake(live, f);
		if (!asleep) updated.push_back(f);
		// an update that ran with work makes the unit not idle at the end of the frame (R2)
		facts = end;
		byte = Decide(facts, f + 1, id, period);
	}
	return updated;
}

static bool Has(const std::vector<int>& v, int f) { for (int x : v) if (x == f) return true; return false; }
static bool AllFrom(const std::vector<int>& v, int from, int to) { for (int f = from; f <= to; ++f) if (!Has(v, f)) return false; return true; }

int main() {
	const int U = 4, P = 6;

	// periods
	CHECK(Period(0.5f, 30) == 15, "period 30 Hz");
	CHECK(Period(0.5f, 10) == 5, "period 10 Hz");
	CHECK(Period(0.01f, 10) == 2, "period floor 2");

	// PRF-002-I: no events, updated on 2, 8, 14 only
	{
		auto v = Run(U, P, 17, [](int, Facts&, Live&) {});
		CHECK((v == std::vector<int>{2, 8, 14}), "case I");
	}
	// PRF-002-J: order at tick 5 -> every tick from 6 (here the queue is live, so from 5)
	{
		auto v = Run(U, P, 17, [](int f, Facts& e, Live& l) { if (f >= 5) { l.queueEmpty = false; e.idle = false; } });
		CHECK(AllFrom(v, 6, 17), "case J every tick from 6");
		CHECK(Has(v, 5), "order wakes in its own frame");
	}
	// PRF-002-K: enemy within range at tick 9 -> updated every tick from 10
	{
		auto v = Run(U, P, 17, [](int f, Facts& e, Live&) { if (f >= 9) e.far = false; });
		CHECK(AllFrom(v, 10, 17), "case K");
		CHECK(!Has(v, 9), "case K: noticed at the end of 9, acts on 10");
	}
	// PRF-002-L: damage on tick 3 -> every tick from 4
	{
		auto v = Run(U, P, 17, [](int f, Facts& e, Live& l) {
			if (f >= 3) l.lastAttackFrame = 3;
			if (f == 3) e.changed = true;   // health fell
		});
		CHECK(Has(v, 3) && Has(v, 4), "case L wake at 3 and 4");
	}
	// PRF-002-M: enemy within action distance from 5 -> near action: awake here every tick from 6 (R1 half rate is the move type's)
	{
		auto v = Run(U, P, 17, [](int f, Facts& e, Live&) { if (f >= 5) e.far = false; });
		CHECK(AllFrom(v, 6, 17), "case M near");
	}
	// promotion paths, each from tick 7 (a sleeping tick for id 4, P 6)
	struct Path { const char* name; std::function<void(Live&)> live; };
	const Path paths[] = {
		{"transported", [](Live& l) { l.transported = true; }},
		{"carrying", [](Live& l) { l.carrying = true; }},
		{"being built", [](Live& l) { l.beingBuilt = true; }},
		{"nano (repair/reclaim/capture)", [](Live& l) { l.lastNanoAdd = 7; }},
		{"stunned", [](Live& l) { l.stunned = true; }},
		{"target set", [](Live& l) { l.hasTarget = true; }},
		{"moved by Lua", [](Live& l) { l.moving = true; }},
		{"order", [](Live& l) { l.queueEmpty = false; }},
		{"damage", [](Live& l) { l.lastAttackFrame = 7; }},
	};
	for (const Path& p : paths) {
		auto v = Run(U, P, 12, [&p](int f, Facts& e, Live& l) { if (f == 7) { p.live(l); e.idle = false; } });
		CHECK(Has(v, 7) && Has(v, 8), p.name);
	}
	// end-of-frame change (Lua SetUnitPosition, team change): awake next frame
	{
		auto v = Run(U, P, 12, [](int f, Facts& e, Live&) { if (f == 7) e.changed = true; });
		CHECK(Has(v, 8) && !Has(v, 7), "end-of-frame change wakes next frame");
	}
	// determinism (case D): same inputs, same frames
	{
		auto ev = [](int f, Facts& e, Live&) { if (f % 11 == 3) e.far = false; };
		CHECK(Run(5, 15, 200, ev) == Run(5, 15, 200, ev), "case D");
	}
	// stagger: ids spread over the period
	{
		std::set<int> firstVisit;
		for (int id = 0; id < 15; ++id) firstVisit.insert(Run(id, 15, 14, [](int, Facts&, Live&) {}).at(0));
		CHECK(firstVisit.size() == 15, "stagger by id");
	}
	// grid
	{
		Grid g; g.Init(4096, 4096, 512);
		g.Add(100, 100, 0); g.Add(3000, 3000, 1);
		CHECK(!g.EnemyWithin(100, 100, 300, 1ull << 1), "enemy far");
		CHECK(g.EnemyWithin(2600, 2600, 450, 1ull << 1), "enemy near (cell touch)");
		CHECK(!g.EnemyWithin(3000, 3000, 50, 1ull << 0), "own team is not an enemy");
		CHECK(g.EnemyWithin(-50, -50, 10, 1ull << 0), "off-map clamps");
		g.Clear();
		CHECK(!g.EnemyWithin(3000, 3000, 900, ~0ull), "clear");
		// margin: the fastest unit (19 MU/s = 152 elmos/s) moves under the 80-elmo margin per frame at 30 and 10 Hz
		CHECK(152.0f / 30 < 80.0f && 152.0f / 10 < 80.0f, "margin covers one frame");
	}
	std::printf(fails ? "%d FAILED\n" : "all passed\n", fails);
	return fails ? 1 : 0;
}
