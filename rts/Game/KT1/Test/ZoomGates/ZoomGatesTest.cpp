/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// Core/ZoomGates/ZoomGatesCore.h, engine-free (LUA-FREE-WORKPACKAGES D07). The values are the game's own
// (game-build LuaUI/lib/zoomlevel.lua, game/data/kt1/zoomgates.json), repeated here so the logic is checked on real rows.

#include "../KT1Test.h"

#include "Game/KT1/Core/ZoomGates/ZoomGatesCore.h"

using namespace KT1::Core::ZoomGates;

namespace {
Levels GameLevels() {
	Levels lv;
	const float b[kBounds] = {450, 820, 930, 1040, 2000, 4000, 8000};
	for (int i = 0; i < kBounds; ++i) lv.bounds[i] = b[i];
	lv.stepIndex = 3;      // 1040, the icon switch
	lv.hyst = 0.05f;
	return lv;
}
Gate LevelGate(int from, int to) { Gate g; g.unit = Unit::Level; g.from = float(from); g.to = float(to); return g; }
Gate ElmosGate(float from, float to, float lo, float hi, float cap = kNoEnd, float hyst = 0) {
	Gate g; g.unit = Unit::Elmos; g.from = from; g.to = to; g.cap = cap; g.hyst = hyst;
	g.detail[0] = lo; g.detail[1] = 1; g.detail[2] = hi;
	return g;
}
} // namespace

KT1_CASE("ZOOMGATES-LEVEL-RAW", "a distance maps to its level L0..L7; a split point belongs to the farther level") {
	const Levels lv = GameLevels();
	KT1_CHECK_EQ(RawLevel(lv, 0), 0);
	KT1_CHECK_EQ(RawLevel(lv, 449), 0);
	KT1_CHECK_EQ(RawLevel(lv, 450), 1);
	KT1_CHECK_EQ(RawLevel(lv, 1039), 3);
	KT1_CHECK_EQ(RawLevel(lv, 1040), 4);
	KT1_CHECK_EQ(RawLevel(lv, 7999), 6);
	KT1_CHECK_EQ(RawLevel(lv, 90000), 7);
	KT1_CHECK_EQ(Level(lv, 100, kNoLevel, true), 7);       // the whole map in view is always the top level
}

KT1_CASE("ZOOMGATES-LEVEL-BAND", "a one-level move waits for the 5% band; the icon switch and a jump do not") {
	const Levels lv = GameLevels();
	KT1_CHECK_EQ(Level(lv, 460, 0, false), 0);             // 450 * 1.05 = 472.5 not reached
	KT1_CHECK_EQ(Level(lv, 473, 0, false), 1);
	KT1_CHECK_EQ(Level(lv, 430, 1, false), 1);             // 450 * 0.95 = 427.5 not reached
	KT1_CHECK_EQ(Level(lv, 427, 1, false), 0);
	KT1_CHECK_EQ(Level(lv, 1040, 3, false), 4);            // the icon switch: one step, no band
	KT1_CHECK_EQ(Level(lv, 1039, 4, false), 3);
	KT1_CHECK_EQ(Level(lv, 825, 0, false), 2);             // two levels at once: no band
	KT1_CHECK_EQ(Level(lv, 2050, 4, false), 4);            // 2000 * 1.05 = 2100 not reached
	KT1_CHECK_EQ(Level(lv, 2101, 4, false), 5);
}

KT1_CASE("ZOOMGATES-LEVEL-GATE", "a level row is drawn at its levels only: shadows at L0-L1, the cubemap at L0-L3") {
	const Gate shadow = LevelGate(0, 1), cube = LevelGate(0, 3), far = LevelGate(5, 7);
	KT1_CHECK(DrawnAtLevel(shadow, 0) && DrawnAtLevel(shadow, 1) && !DrawnAtLevel(shadow, 2));
	KT1_CHECK(DrawnAtLevel(cube, 3) && !DrawnAtLevel(cube, 4));
	KT1_CHECK(!DrawnAtLevel(far, 4) && DrawnAtLevel(far, 5) && DrawnAtLevel(far, 7));
	Gate open = LevelGate(2, 0); open.to = kNoEnd;
	KT1_CHECK(!DrawnAtLevel(open, 1) && DrawnAtLevel(open, 7));
}

KT1_CASE("ZOOMGATES-ELMOS-DETAIL", "the detail option scales a distance row; the cap limits the scaled end") {
	const Gate bars = ElmosGate(0, 1584, 0.819f, 1.072f);
	KT1_CHECK(DrawnAtDistance(bars, 1583, 1, true) && !DrawnAtDistance(bars, 1584, 1, true));
	KT1_CHECK(DrawnAtDistance(bars, 1297, 0, true) && !DrawnAtDistance(bars, 1298, 0, true));   // 1584 * 0.819 = 1297.3
	KT1_CHECK(DrawnAtDistance(bars, 1697, 2, true) && !DrawnAtDistance(bars, 1699, 2, true));   // 1584 * 1.072 = 1698.0
	const Gate large = ElmosGate(0, 3680, 0.794f, 1.052f, 1500);
	KT1_CHECK_EQ(End(large, 1), 1500.0f);
	KT1_CHECK_EQ(End(large, 2), 1500.0f);
	const Gate smallLow = ElmosGate(0, 1760, 0.819f, 1.072f, 1500);
	KT1_CHECK(End(smallLow, 0) < 1500.0f && End(smallLow, 0) > 1441.0f);                       // 1441.4: under the cap
	const Gate icons = ElmosGate(1040, kNoEnd, 0.923f, 1.154f);
	KT1_CHECK(!DrawnAtDistance(icons, 1039, 1, false) && DrawnAtDistance(icons, 1040, 1, false));
	KT1_CHECK(DrawnAtDistance(icons, 961, 0, false) && !DrawnAtDistance(icons, 1199, 2, false)); // 959.9 and 1200.2
	KT1_CHECK_EQ(End(icons, 1), kNoEnd);
	KT1_CHECK_EQ(Factor(bars, -4), 0.819f);                // an option value out of range takes the nearest
	KT1_CHECK_EQ(Factor(bars, 9), 1.072f);
}

KT1_CASE("ZOOMGATES-ELMOS-BAND", "a distance row with a band does not flip while the camera rests on its end") {
	const Gate g = ElmosGate(0, 1000, 1, 1, kNoEnd, 0.02f);
	KT1_CHECK(DrawnAtDistance(g, 1010, 1, true));          // drawn: stays to 1020
	KT1_CHECK(!DrawnAtDistance(g, 1021, 1, true));
	KT1_CHECK(!DrawnAtDistance(g, 990, 1, false));         // not drawn: back below 980
	KT1_CHECK(DrawnAtDistance(g, 979, 1, false));
	Gate d = g; d.unit = Unit::Depth;
	KT1_CHECK(DrawnAtDepth(d, 999, 1) && !DrawnAtDepth(d, 1000, 1));   // a depth row has no memory: the exact end
}

KT1_CASE("ZOOMGATES-EVALUATE", "the mask has one bit per row and changes only where a row's answer changes") {
	Table t;
	t.gates[t.count++] = LevelGate(0, 1);                                  // 0 shadow pass
	t.gates[t.count++] = LevelGate(0, 3);                                  // 1 cubemap
	t.gates[t.count++] = ElmosGate(1040, kNoEnd, 0.923f, 1.154f);          // 2 icons
	t.gates[t.count++] = ElmosGate(0, 1760, 0.819f, 1.072f, 1500);         // 3 small models
	t.gates[t.count++] = ElmosGate(0, 1584, 0.819f, 1.072f);               // 4 bars
	Gate fx; fx.unit = Unit::Depth; fx.to = 1200; t.gates[t.count++] = fx; // 5 effects: not a camera gate
	Gate props; props.unit = Unit::Scale; t.gates[t.count++] = props;      // 6 props: factor only
	const uint64_t always = (1u << 5) | (1u << 6);
	const uint64_t l0 = Evaluate(t, 0, 300, 1, 0, true);
	KT1_CHECK_EQ(l0, uint64_t(0b0011011) | always);
	const uint64_t l2 = Evaluate(t, 2, 900, 1, l0, false);
	KT1_CHECK_EQ(l0 ^ l2, uint64_t(1));                                    // only the shadow pass changed
	const uint64_t l4 = Evaluate(t, 4, 1100, 1, l2, false);
	KT1_CHECK_EQ(l2 ^ l4, uint64_t(0b110));                                // cubemap off, icons on; models and bars stay
	const uint64_t a = Evaluate(t, 4, 1520, 1, l4, false);
	KT1_CHECK_EQ(l4 ^ a, uint64_t(0b1000));                                // models gone at the 1500 cap, bars still drawn
	const uint64_t b = Evaluate(t, 4, 1600, 1, a, false);
	KT1_CHECK_EQ(a ^ b, uint64_t(0b10000));                                // bars gone at 1584
	KT1_CHECK_EQ(Evaluate(t, 4, 1600, 1, b, false), b);                    // nothing moved: nothing to apply
	KT1_CHECK_EQ(Evaluate(t, 4, 1600, 2, b, false) ^ b, uint64_t(0b10000)); // highest option: bars back (cut 1698)
}

KT1_CASE("ZOOMGATES-SWEEP-FACTOR", "the option factor is the ratio of sweep bracket midpoints to the middle option's") {
	const float small[kDetails][2] = {{160, 180}, {200, 215}, {215, 230}};
	KT1_CHECK_EQ(SweepFactor(small, 1), 1.0f);
	KT1_CHECK(SweepFactor(small, 0) > 0.8192f && SweepFactor(small, 0) < 0.8194f);   // 170 / 207.5
	KT1_CHECK(SweepFactor(small, 2) > 1.0722f && SweepFactor(small, 2) < 1.0724f);   // 222.5 / 207.5
	const float icons[kDetails][2] = {{0, 120}, {120, 140}, {140, 160}};
	KT1_CHECK(SweepFactor(icons, 0) > 0.9230f && SweepFactor(icons, 0) < 0.9232f);   // 120 / 130: gone at the first zoom
	const float plants[kDetails][2] = {{540, 600}, {600, -1}, {600, 800}};
	KT1_CHECK_EQ(SweepFactor(plants, 0), 0.95f);                                     // 570 / 600
	KT1_CHECK_EQ(SweepFactor(plants, 2), 1.0f);                                      // middle open: farther or equal = equal
}
