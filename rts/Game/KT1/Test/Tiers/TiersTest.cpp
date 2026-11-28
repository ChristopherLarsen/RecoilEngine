/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 rule Tiers, engine-free: PRF-002 v3 R7 cases I-M, case D and every promotion path. The period cases are data:
// game-build tools/test/core_cases/tiers/period.json.

#include "../KT1CaseLoader.h"
#include "../KT1Test.h"
#include "Game/KT1/Rules/Tiers/KT1TiersCore.h"

#include <algorithm>
#include <functional>
#include <set>
#include <string_view>
#include <vector>

using namespace KT1::TiersCore;

namespace {
	// the unit id and tier period of the released cases
	constexpr int U = 4;
	constexpr int P = 6;

	// One unit through frames 0..last. events(f, facts, live) edits the state the unit shows during frame f (live) and at
	// its end (facts). Returns the frames on which the unit is updated (not asleep), as the engine hooks see it.
	std::vector<int> Run(int id, int period, int last, std::function<void(int, Facts&, Live&)> events) {
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

	bool Has(const std::vector<int>& v, int f) { return std::find(v.begin(), v.end(), f) != v.end(); }

	bool AllFrom(const std::vector<int>& v, int from, int to) {
		for (int f = from; f <= to; ++f)
			if (!Has(v, f)) return false;
		return true;
	}
}

KT1_JSON_CASES("tiers") {
	const std::string_view check = data["check"];
	KT1_CHECK_MSG(check == "period", "unknown check");
	KT1_CHECK_EQ(Period(static_cast<float>(double(data["seconds"])), static_cast<int>(int64_t(data["steps_per_second"]))),
		int64_t(data["frames"]));
}

KT1_CASE("TIERS-PRF-002-I", "no events: updated on 2, 8 and 14 only") {
	const auto v = Run(U, P, 17, [](int, Facts&, Live&) {});
	KT1_CHECK((v == std::vector<int>{2, 8, 14}));
}

KT1_CASE("TIERS-PRF-002-J", "an order at tick 5: updated every tick from it (the queue is live)") {
	const auto v = Run(U, P, 17, [](int f, Facts& e, Live& l) { if (f >= 5) { l.queueEmpty = false; e.idle = false; } });
	KT1_CHECK_MSG(AllFrom(v, 6, 17), "every tick from 6");
	KT1_CHECK_MSG(Has(v, 5), "the order wakes in its own frame");
}

KT1_CASE("TIERS-PRF-002-K", "an enemy within range at tick 9: updated every tick from 10") {
	const auto v = Run(U, P, 17, [](int f, Facts& e, Live&) { if (f >= 9) e.far = false; });
	KT1_CHECK_MSG(AllFrom(v, 10, 17), "every tick from 10");
	KT1_CHECK_MSG(!Has(v, 9), "noticed at the end of 9, acts on 10");
}

KT1_CASE("TIERS-PRF-002-L", "damage on tick 3: awake on 3 and 4") {
	const auto v = Run(U, P, 17, [](int f, Facts& e, Live& l) {
		if (f >= 3) l.lastAttackFrame = 3;
		if (f == 3) e.changed = true;   // health fell
	});
	KT1_CHECK(Has(v, 3));
	KT1_CHECK(Has(v, 4));
}

// near action: awake here every tick from 6 (R1 half rate is the move type's)
KT1_CASE("TIERS-PRF-002-M", "an enemy within the action distance from tick 5: awake every tick from 6") {
	const auto v = Run(U, P, 17, [](int f, Facts& e, Live&) { if (f >= 5) e.far = false; });
	KT1_CHECK(AllFrom(v, 6, 17));
}

KT1_CASE("TIERS-PRF-002-D", "the same inputs update the same frames") {
	const auto events = [](int f, Facts& e, Live&) { if (f % 11 == 3) e.far = false; };
	KT1_CHECK(Run(5, 15, 200, events) == Run(5, 15, 200, events));
}

// each path from tick 7, a sleeping tick for id 4 at period 6
KT1_CASE("TIERS-PROMOTE", "every promotion path wakes the unit in its own frame and the next") {
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
	for (const Path& p: paths) {
		const auto v = Run(U, P, 12, [&p](int f, Facts& e, Live& l) { if (f == 7) { p.live(l); e.idle = false; } });
		KT1_CHECK_MSG(Has(v, 7) && Has(v, 8), p.name);
	}
}

// Lua SetUnitPosition, team change
KT1_CASE("TIERS-ENDFRAME", "a change seen at the end of a frame wakes the unit on the next one") {
	const auto v = Run(U, P, 12, [](int f, Facts& e, Live&) { if (f == 7) e.changed = true; });
	KT1_CHECK(Has(v, 8));
	KT1_CHECK(!Has(v, 7));
}

KT1_CASE("TIERS-STAGGER", "ids spread over the period") {
	std::set<int> firstVisits;
	for (int id = 0; id < 15; ++id)
		firstVisits.insert(Run(id, 15, 14, [](int, Facts&, Live&) {}).at(0));
	KT1_CHECK_EQ(firstVisits.size(), 15u);
}

KT1_CASE("TIERS-GRID", "the ally-team grid finds enemies by cell, clamps at the map edge and clears") {
	Grid g;
	g.Init(4096, 4096, 512);
	g.Add(100, 100, 0);
	g.Add(3000, 3000, 1);
	KT1_CHECK_MSG(!g.EnemyWithin(100, 100, 300, 1ull << 1), "enemy far");
	KT1_CHECK_MSG(g.EnemyWithin(2600, 2600, 450, 1ull << 1), "enemy near (cell touch)");
	KT1_CHECK_MSG(!g.EnemyWithin(3000, 3000, 50, 1ull << 0), "own team is not an enemy");
	KT1_CHECK_MSG(g.EnemyWithin(-50, -50, 10, 1ull << 0), "off-map clamps");
	g.Clear();
	KT1_CHECK_MSG(!g.EnemyWithin(3000, 3000, 900, ~0ull), "clear");
}

// the fastest unit (19 MU/s = 152 elmos/s) against the 80-elmo margin
KT1_RATE_CASE("TIERS-GRID-MARGIN", "the grid margin covers one step of the fastest unit") {
	KT1_CHECK(152.0f / testCase.StepsPerSecond() < 80.0f);
}
