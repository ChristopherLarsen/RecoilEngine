/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 Intel facts, engine-free (LUA-FREE S06): CMD-010 v6 R7 and R8 with fixture SF-010b, INT-005 v7 R1 and R3b with SF-INT-5
// and CMD-010-I, the seconds-to-steps conversion at 30 and 10 steps a second, and the shot record's lag.

#pragma GCC optimize("fp-contract=off")

#include "../KT1Test.h"
#include "Game/KT1/Core/Intel/IntelFactsCore.h"

#include <set>
#include <vector>

using namespace KT1::IntelFactsCore;

namespace {
	// SF-010b: vision cell 3, vision 21 (radius units with MU 1, so map units equal radius units)
	constexpr float CELL = 3.0f;

	Geometry Geo() {
		Geometry g;
		g.fe = CELL; g.ce = 2 * CELL; g.fnx = 100; g.fnz = 100; g.cnx = 50; g.cnz = 50; g.MU = 1;
		return g;
	}

	// SF-INT-5 / SF-010b: redraw_fraction 0.25, redraw_interval 0.8 s
	Rules Sf(int sps, bool restampOnCell) {
		Rules r;
		r.geo = Geo();
		r.cell = CELL;
		r.redrawFraction = 0.25f;
		r.restampOnCell = restampOnCell;
		Seconds s;
		s.redrawInterval = 0.8f;
		r.steps = Steps::From(s, sps, true, 12);
		return r;
	}

	float CellX(float x) { Row o; Cells(Geo(), x, 4.4f, o); return o.fcx; }
}

KT1_CASE("INTF-CMD-010-R7", "a position's cell is floor(x / cell); a point on an edge is in the higher cell") {
	KT1_CHECK_EQ(CellX(4.4f), 1.0f);
	KT1_CHECK_EQ(CellX(3.0f), 1.0f);    // case E: on the edge
	KT1_CHECK_EQ(CellX(5.9f), 1.0f);    // case E: same cell
	KT1_CHECK_EQ(CellX(2.9f), 0.0f);    // case F: a hundredth lower is the lower cell
	KT1_CHECK_EQ(CellX(24.0f), 8.0f);   // case D's wall on an edge
	KT1_CHECK_EQ(CellX(23.5f), 7.0f);
}

KT1_CASE("INTF-CMD-010-R8", "the radius counts whole cells: 21 and 22 over 3 are both 7 cells") {
	KT1_CHECK_EQ(Reach(21.0f, 1.0f, CELL), 21.0f);
	KT1_CHECK_EQ(Reach(22.0f, 1.0f, CELL), 21.0f);   // case G
	KT1_CHECK_EQ(Reach(20.9f, 1.0f, CELL), 18.0f);
	KT1_CHECK_EQ(Reach(20.0f, 8.0f, 16.0f), 160.0f);  // radius units times MU
}

KT1_CASE("INTF-KEYS", "fine and coarse keys, buckets and the off-map key -1") {
	Row o;
	Cells(Geo(), 7.6f, 4.4f, o);
	KT1_CHECK_EQ(o.fk, 1.0f * 100 + 2);
	KT1_CHECK_EQ(o.ck, 0.0f * 50 + 1);
	KT1_CHECK_EQ(o.fb, 0.0f);
	Cells(Geo(), -0.5f, 4.4f, o);
	KT1_CHECK_EQ(o.fk, -1.0f);
	Cells(Geo(), 300.0f, 4.4f, o);        // fine cell 100 is past the map's 100 cells
	KT1_CHECK_EQ(o.fk, -1.0f);
}

KT1_RATE_CASE("INTF-STEPS", "seconds become steps once: facts grid, redraw interval and lags at 30 and at 10") {
	const int sps = testCase.StepsPerSecond();
	Seconds s;
	s.redrawInterval = 0.6f; s.factsGridTicks = 4; s.refTick = 0.1f; s.moveLagEnter = 0; s.moveLagLeave = 0;
	const Steps st = Steps::From(s, sps, true, sps == 30 ? 12 : 4);
	KT1_CHECK_EQ(st.FG, sps == 30 ? 12 : 4);
	KT1_CHECK_EQ(st.ri, sps == 30 ? 18 : 6);
	KT1_CHECK_EQ(st.reF, 1);
	const Steps un = Steps::From(s, sps, false, 0);
	KT1_CHECK_EQ(un.FG, 1);
	KT1_CHECK_EQ(un.reF, sps == 30 ? 18 : 6);   // redraw_period 0.6 s
	Seconds l = s; l.moveLagEnter = 0.5f; l.moveLagLeave = 0.2f;
	const Steps lg = Steps::From(l, sps, false, 0);
	KT1_CHECK_EQ(lg.lagE, sps == 30 ? 15 : 5);
	KT1_CHECK_EQ(lg.lagL, lg.lagE);              // leaving is never quicker than entering
}

KT1_RATE_CASE("INTF-INT-005-R1", "CMD-010-I: moved 3.2 (under 5.25) the disc stays at 4.4 until 0.8 s after the stamp") {
	const int sps = testCase.StepsPerSecond();
	const Rules r = Sf(sps, false);
	Stamped s;
	float ox, oz;
	Stamp(r, s, 4.4f, 4.4f, 21.0f, 0.0f, CELL, ox, oz);
	KT1_CHECK_EQ(ox, 4.4f);
	const int ri = sps * 8 / 10;
	for (int f = 1; f < ri; ++f) {
		Stamp(r, s, 7.6f, 4.4f, 21.0f, static_cast<float>(f), CELL, ox, oz);
		KT1_CHECK_MSG(ox == 4.4f, "restamped before the interval");
	}
	Stamp(r, s, 7.6f, 4.4f, 21.0f, static_cast<float>(ri), CELL, ox, oz);
	KT1_CHECK_EQ(ox, 7.6f);
	KT1_CHECK_EQ(s.stamps, 2);
}

KT1_RATE_CASE("INTF-INT-005-R1-DIST", "moving past a quarter of the radius restamps at once; a radius change restamps") {
	const Rules r = Sf(testCase.StepsPerSecond(), false);
	Stamped s;
	float ox, oz;
	Stamp(r, s, 4.4f, 4.4f, 21.0f, 0.0f, CELL, ox, oz);
	Stamp(r, s, 4.4f + 5.5f, 4.4f, 21.0f, 1.0f, CELL, ox, oz);
	KT1_CHECK_EQ(ox, 4.4f + 5.5f);
	Stamp(r, s, 4.4f + 5.5f, 4.4f, 24.0f, 2.0f, CELL, ox, oz);
	KT1_CHECK_EQ(s.stamps, 3);
	Stamp(r, s, 4.4f + 5.5f, 4.4f, 24.0f, 3.0f, CELL, ox, oz);
	KT1_CHECK_EQ(s.fresh, 1);                 // still at its stamp
	KT1_CHECK_EQ(s.stamps, 3);
}

KT1_RATE_CASE("INTF-RESTAMP-CELL", "DESIGN restamp_on_cell: crossing a cell edge restamps (CMD-010 v6 R6 says it does not)") {
	const Rules on = Sf(testCase.StepsPerSecond(), true);
	Stamped s;
	float ox, oz;
	Stamp(on, s, 4.4f, 4.4f, 21.0f, 0.0f, CELL, ox, oz);
	Stamp(on, s, 6.1f, 4.4f, 21.0f, 1.0f, CELL, ox, oz);   // cell 1 to cell 2, moved 1.7
	KT1_CHECK_EQ(ox, 6.1f);
	const Rules off = Sf(testCase.StepsPerSecond(), false);
	Stamped t;
	Stamp(off, t, 4.4f, 4.4f, 21.0f, 0.0f, CELL, ox, oz);
	Stamp(off, t, 6.1f, 4.4f, 21.0f, 1.0f, CELL, ox, oz);
	KT1_CHECK_EQ(ox, 4.4f);
}

KT1_RATE_CASE("INTF-INT-005-R3b", "each unit is read once per facts grid, at the phase of its id; replayed, the same steps") {
	const int sps = testCase.StepsPerSecond();
	Seconds s;
	const Steps st = Steps::From(s, sps, true, sps == 30 ? 12 : 4);
	const float FG = static_cast<float>(st.FG);
	const std::vector<int> ids = {3, 17, 26, 40};
	std::vector<std::vector<int>> runs(2);
	for (auto& run : runs) {
		for (int f = 0; f < 3 * st.FG; ++f)
			for (int u : ids)
				if (ReadNow(false, u, FG, Mod(static_cast<float>(f), FG), false, true)) run.push_back(f * 1000 + u);
	}
	KT1_CHECK(runs[0] == runs[1]);                                 // case H
	KT1_CHECK_EQ(runs[0].size(), 3 * ids.size());                 // once per grid
	std::set<int> phases;
	for (int u : ids) phases.insert(u % st.FG);
	KT1_CHECK(phases.size() > 1);                                 // case G: different phases
	KT1_CHECK(!ReadNow(false, 3, FG, Mod(4.0f, FG), false, true));   // not its phase
	KT1_CHECK(ReadNow(false, 3, FG, Mod(4.0f, FG), true, true));    // a fresh unit is read at once
}

KT1_CASE("INTF-SHOT-LAG", "the place detection uses is the position lagE steps ago, snapped to its cell centre") {
	Steps st; st.reF = 1; st.lagE = 3; st.lagL = 5;
	Shots s;
	float ox, oz;
	for (int f = 0; f <= 10; ++f) Shot(st, s, 1.0f + 3.0f * f, 1.0f, CELL, static_cast<float>(f), ox, oz);
	KT1_CHECK_EQ(ox, Snap(1.0f + 3.0f * 7, CELL));   // step 10 - 3
	KT1_CHECK_EQ(s.lcx, Snap(1.0f + 3.0f * 5, CELL));   // step 10 - 5
	KT1_CHECK(s.hf.size() <= 7);                   // the old records are dropped
}
