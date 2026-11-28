/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// Engine-free tests of Core/ShotFollower (Lua-free plan D05): each rule against the Lua it replaces (LuaUI/fx/shots.lua,
// LuaRules/Gadgets/gfx_shots.lua), at 30 and 10 steps per second where a period is involved.

#include "../KT1Test.h"

#include "Game/KT1/Core/Common/Rate.h"
#include "Game/KT1/Core/ShotFollower/ShotFollowerCore.h"

#include <array>
#include <cmath>

using namespace KT1::Core;
using namespace KT1::Core::ShotFollower;
using KT1::Test::AllocationCount;

#define CHECK(c, msg) KT1_CHECK_MSG(c, msg)

namespace {
bool Near(float a, float b, float eps = 1e-5f) { return std::fabs(a - b) <= eps; }
}

// ---- setting, caps, mode --------------------------------------------------------------------------------------------
KT1_CASE("SHOTF-SETTING", "setting and caps row as shots.lua ParseSetting and Caps") {
	CHECK(ParseSetting(false, 0) == Setting::Short, "unset: the default (Medium's)");
	CHECK(ParseSetting(true, -1) == Setting::None && ParseSetting(true, 0) == Setting::None, "0 or less: none");
	CHECK(ParseSetting(true, 1) == Setting::Short && ParseSetting(true, 1.5f) == Setting::Short, "under 2: short");
	CHECK(ParseSetting(true, 2) == Setting::Full && ParseSetting(true, 7) == Setting::Full, "2 and up: full");
	const ShotFollowerConfig c;
	CHECK(CapsFor(c, 0) == nullptr && CapsFor(c, -1) == nullptr, "effects off: no caps");
	CHECK(CapsFor(c, 0.25f) == &c.caps[0], "under every row: the first");
	CHECK(CapsFor(c, 0.5f) == &c.caps[0] && CapsFor(c, 0.9f) == &c.caps[0], "0.5 row");
	CHECK(CapsFor(c, 1) == &c.caps[1] && CapsFor(c, 1.5f) == &c.caps[1], "1 row");
	CHECK(CapsFor(c, 2) == &c.caps[2] && CapsFor(c, 9) == &c.caps[2], "2 row");
	CHECK(c.caps[1].ribbons == 200 && c.caps[1].points == 12 && c.caps[1].puffsPerSecond == 600, "defaults are S.CAPS");
}

KT1_CASE("SHOTF-MODE", "mode as shots.lua S.Mode") {
	const ShotFollowerConfig c;
	const Mode off = MakeMode(c, Setting::Full, false, nullptr);
	CHECK(!off.ribbons && !off.keep && !off.puffs && !off.pulse && !off.flak, "effects off: nothing");
	const Mode none = MakeMode(c, Setting::None, false, &c.caps[2]);
	CHECK(!none.ribbons && !none.keep && !none.puffs && none.pulse && none.flak, "setting none: bodies only, pulse and flak by row");
	const Mode shortM = MakeMode(c, Setting::Short, false, &c.caps[1]);
	CHECK(shortM.ribbons && shortM.keep && shortM.puffs && shortM.missilesOnly && Near(shortM.lifeMaxS, 0.5f), "short: missiles, 0.5 s");
	CHECK(shortM.pulse && !shortM.flak, "row 1: pulse, no flak");
	const Mode full = MakeMode(c, Setting::Full, false, &c.caps[0]);
	CHECK(full.ribbons && !full.missilesOnly && full.lifeMaxS < 0 && !full.pulse, "full at row 0.5: every class, no pulse");
	const Mode guard = MakeMode(c, Setting::Full, true, &c.caps[1]);
	CHECK(!guard.ribbons && !guard.puffs && guard.keep && Near(guard.lifeMul, 0.5f), "guard: no new ribbons or puffs, keep at half life");
}

// ---- per shot -------------------------------------------------------------------------------------------------------
KT1_CASE("SHOTF-BAND", "bands, body fade and depth alpha as shots.lua Band, BodyFade, DepthAlpha") {
	const ShotFollowerConfig c;
	CHECK(BandOf(c, 0) == BAND_NEAR && BandOf(c, 1499 * 1499.0f) == BAND_NEAR, "near");
	CHECK(BandOf(c, 1500 * 1500.0f) == BAND_MID && BandOf(c, 3499 * 3499.0f) == BAND_MID, "mid from 1500");
	CHECK(BandOf(c, 3500 * 3500.0f) == BAND_FAR && BandOf(c, 5999 * 5999.0f) == BAND_FAR, "far from 3500");
	CHECK(BandOf(c, 6000 * 6000.0f) == BAND_BEYOND, "beyond from 6000");
	CHECK(BodyFade(c, 4800 * 4800.0f) == 1, "full up to 4800 (the last 20 %)");
	CHECK(Near(BodyFade(c, 5400 * 5400.0f), 0.5f, 1e-3f), "half way through the fade");
	CHECK(BodyFade(c, 6000 * 6000.0f) == 0, "gone at far");
	CHECK(DepthAlpha(c, 5) == 1 && Near(DepthAlpha(c, -40), 0.5f) && Near(DepthAlpha(c, -200), 0.3f), "depth alpha: half at 40, floor 0.3");
	CHECK(Visible(false, false, false) && Visible(true, false, true) && !Visible(true, false, false) && Visible(true, true, false),
		"enemy only in sight; spectator sees all");
	CHECK(Collapsed(true, false) && Collapsed(false, true) && !Collapsed(false, false), "collapsed by overview or icon");
}

KT1_CASE("SHOTF-ELIGIBLE", "eligibility as shots.lua S.Eligible") {
	const ShotFollowerConfig c;
	const Mode full = MakeMode(c, Setting::Full, false, &c.caps[1]);
	const Mode top = MakeMode(c, Setting::Full, false, &c.caps[2]);
	const Mode shortM = MakeMode(c, Setting::Short, false, &c.caps[1]);
	const Mode guard = MakeMode(c, Setting::Full, true, &c.caps[1]);
	RibbonTraits shell, missile, flak, essential, layered;
	missile.missile = true;
	flak.flak = true;
	essential.essential = true;
	layered.layered = true;
	CHECK(Eligible(shell, BAND_NEAR, full, false) && Eligible(shell, BAND_MID, full, false), "near and mid at full");
	CHECK(!Eligible(shell, BAND_FAR, full, false) && Eligible(layered, BAND_FAR, full, false), "far: only a layered look");
	CHECK(!Eligible(flak, BAND_NEAR, full, false) && Eligible(flak, BAND_NEAR, top, false), "flak only at the top row");
	RibbonTraits notHigh = flak;
	notHigh.highOnly = 0;
	CHECK(Eligible(notHigh, BAND_NEAR, full, false), "a recipe may say flak is not high-only");
	CHECK(!Eligible(shell, BAND_NEAR, shortM, false) && Eligible(missile, BAND_NEAR, shortM, false), "short: missiles only");
	CHECK(Eligible(essential, BAND_NEAR, shortM, false), "an essential look at short too");
	CHECK(!Eligible(shell, BAND_NEAR, guard, false) && Eligible(shell, BAND_NEAR, guard, true), "guard: only ribbons already started");
	CHECK(Eligible(essential, BAND_NEAR, guard, false), "guard keeps an essential look");
}

// ---- cap plan -------------------------------------------------------------------------------------------------------
KT1_CASE("SHOTF-PLAN", "cap order: priority, then nearer, then lower id; no allocation") {
	CapsRow caps{1, 3, 12, 600};
	std::array<PlanShot, 6> s{};
	std::array<int32_t, 6> order{};
	// id, priority, d2, want
	const int32_t ids[6] = {50, 10, 20, 30, 40, 60};
	const int32_t pr[6] = {3, 1, 3, 3, 7, 1};
	const float d2[6] = {100, 900, 100, 50, 1, 5};
	const bool want[6] = {true, true, true, true, true, false};
	for (int i = 0; i < 6; ++i) { s[i].id = ids[i]; s[i].priority = pr[i]; s[i].d2 = d2[i]; s[i].want = want[i]; }
	const long long a0 = AllocationCount();
	const PlanResult r = Plan(s.data(), 6, &caps, order.data());
	CHECK(AllocationCount() == a0, "no allocation in the plan");
	CHECK(r.drawn == 3 && r.capped == 2, "3 drawn of 5 wanting");
	CHECK(s[1].draw, "priority 1 first even far");
	CHECK(s[3].draw, "then priority 3 nearest (d2 50)");
	CHECK(s[2].draw && !s[0].draw, "equal priority and distance: the lower id (20 before 50)");
	CHECK(!s[4].draw && !s[5].draw, "priority 7 capped; not wanting never draws");
	caps.ribbons = 10;
	const PlanResult all = Plan(s.data(), 6, &caps, order.data());
	CHECK(all.drawn == 5 && all.capped == 0, "under the cap every wanting shot draws");
	const PlanResult none = Plan(s.data(), 6, nullptr, order.data());
	CHECK(none.drawn == 0 && none.capped == 5 && !s[1].draw, "effects off: none draw, all capped");
}

// ---- lives and strides ----------------------------------------------------------------------------------------------
KT1_RATE_CASE("SHOTF-LIFE", "ribbon life, extra life, stride and heavy lightening") {
	const int sps = testCase.StepsPerSecond();
	const ShotFollowerConfig c;
	const CapsRow& caps = c.caps[1];
	CHECK(PointsFor(caps, BAND_NEAR) == 12 && PointsFor(caps, BAND_MID) == 6 && PointsFor(c.caps[0], BAND_MID) == 3, "mid: half the points");
	// shots.lua Stride(life x 30, points): 0.5 s, 12 points = ceil(15 / 12) = 2 at 30; ceil(5 / 12) = 1 at 10
	CHECK(StrideSteps(0.5f, 12) == (sps == 30 ? 2 : 1), "stride from life and points");
	CHECK(StrideSteps(2.0f, 6) == (sps == 30 ? 10 : 4), "a 2 s ribbon of 6 points");
	CHECK(StrideSteps(0, 12) == 1 && StrideSteps(1, 0) == sps, "at least 1; no points counts as 1");
	Mode m;
	m.lifeMul = 0.5f;
	m.lifeMaxS = 0.3f;
	CHECK(Near(LifeUnderMode(0.4f, m), 0.2f) && Near(LifeUnderMode(2, m), 0.3f), "life x mul, at most the max");
	CHECK(Near(RibbonLife(c, 1, 0.7f, 50, 400), 0.7f), "a set flight wins");
	CHECK(Near(RibbonLife(c, 1, 0, 100, 400), 0.25f), "length over speed");
	CHECK(Near(RibbonLife(c, 0.05f, 0, 100, 400), 0.15f), "at most 3 x the life");
	CHECK(Near(RibbonLife(c, 1, 0, 1, 400), 2.0f / 30), "at least 2/30 s");
	CHECK(Near(RibbonLife(c, 1, 0, 100, 0.5f), 1), "too slow: its own life");
	CHECK(Near(ExtraLife(c, 1, 0, 0, 400), 1) && Near(ExtraLife(c, 1, 0, 100, 400), 0.25f) && Near(ExtraLife(c, 1, 0.4f, 100, 400), 0.4f),
		"extra: own life, length, flight");
	CHECK(Near(ExtraLife(c, 9, 0, 100, 4000), 2.0f / 30), "extra length life at least 2/30 s");
	const Heavy h40 = HeavyFor(c, 40), h41 = HeavyFor(c, 41), h121 = HeavyFor(c, 121);
	CHECK(!h40.noExtras && h40.lifeK == 1 && h41.noExtras && h41.lifeK == 1 && h121.noExtras && Near(h121.lifeK, 0.5f), "heavy at 41 and 121");
}

// ---- cut-offs -------------------------------------------------------------------------------------------------------
KT1_CASE("SHOTF-CUTOFF", "view depth with the detail factor; part, core and stream gates") {
	// camera at the origin looking straight down: a point 1200 below is at depth 1200
	CHECK(Near(ViewDepth(10, -1200, 5, 0, 0, 0, 0, -1, 0, 1), 1200), "depth along the view");
	CHECK(Near(ViewDepth(10, -1200, 5, 0, 0, 0, 0, -1, 0, 0.8f), 1500), "lowest detail: deeper by 1 / 0.8");
	CHECK(Near(ViewDepth(10, -1200, 5, 0, 0, 0, 0, -1, 0, 0), 1200), "no factor: plain depth");
	CHECK(PartIn(1280, true, 1280, BAND_FAR, true, false) && !PartIn(1280, true, 1281, BAND_NEAR, false, true), "own cut-off, inclusive");
	CHECK(PartIn(-1, true, 1e6f, BAND_BEYOND, true, false), "cut-off -1: never cut");
	CHECK(PartIn(0, false, 9999, BAND_FAR, false, true), "no cut-off, layered: drawn");
	CHECK(!PartIn(0, false, 0, BAND_FAR, true, false) && PartIn(0, false, 0, BAND_MID, true, false) && PartIn(0, false, 0, BAND_FAR, false, false),
		"no cut-off: tiers keep the core only at far");
	CHECK(CoreOut(1280, true, 1281) && !CoreOut(1280, true, 1280) && !CoreOut(-1, true, 1e6f) && !CoreOut(10, false, 1e6f), "core out");
	CHECK(StreamIn(true, 1200, 1200, BAND_BEYOND, false, false) && !StreamIn(true, 1200, 1201, BAND_NEAR, false, false), "layer cut-off");
	CHECK(StreamIn(true, -1, 1e6f, BAND_BEYOND, false, false), "layer cut-off -1: never cut");
	CHECK(StreamIn(false, 0, 0, BAND_MID, false, false) && !StreamIn(false, 0, 0, BAND_FAR, false, false), "band rule: near and mid");
	CHECK(StreamIn(false, 0, 0, BAND_FAR, true, true) && !StreamIn(false, 0, 0, BAND_FAR, true, false), "a tiered body stream to far");
	CHECK(ExtraStreamIn(true, 900, 900, BAND_BEYOND) && !ExtraStreamIn(false, 0, 0, BAND_FAR) && ExtraStreamIn(false, 0, 0, BAND_MID), "extras");
}

// ---- cadence --------------------------------------------------------------------------------------------------------
KT1_RATE_CASE("SHOTF-CADENCE", "stream periods from rates, mid doubling, staggered by id") {
	const int sps = testCase.StepsPerSecond();
	CHECK(EveryFromRate(10) == (sps == 30 ? 3 : 1), "10 a second: every 3 steps at 30, every step at 10");
	CHECK(EveryFromRate(2) == sps / 2 && EveryFromRate(0) == 1 && EveryFromRate(1000) == 1, "2 a second; at least every step");
	CHECK(StreamEverySteps(3, BAND_MID, false) == 6 && StreamEverySteps(3, BAND_MID, true) == 3 && StreamEverySteps(3, BAND_NEAR, false) == 3,
		"mid band: every second puff unless per-layer");
	CHECK(StreamEverySteps(0, BAND_NEAR, false) == 1, "a period under 1 is every step");
	int due[3] = {0, 0, 0};
	for (int step = 0; step < 30; ++step)
		for (int id = 0; id < 3; ++id)
			due[id] += DueAt(step, id, 3) ? 1 : 0;
	CHECK(due[0] == 10 && due[1] == 10 && due[2] == 10, "each id due every third step");
	CHECK(DueAt(0, 0, 3) && !DueAt(0, 1, 3) && DueAt(2, 1, 3), "staggered: step + id");
}

// ---- puff budget ----------------------------------------------------------------------------------------------------
KT1_RATE_CASE("SHOTF-PUFFBUDGET", "rolling one-second budget, the same per second at every rate") {
	const int sps = testCase.StepsPerSecond();
	const ShotFollowerConfig c;
	PuffBudget b;
	b.Init(c, 5);
	CHECK(!b.Admit(), "nothing before the first step");
	b.NewStep(0);
	int got = 0;
	for (int i = 0; i < 9; ++i) got += b.Admit() ? 1 : 0;
	CHECK(got == 5 && b.Total() == 5, "5 in the window");
	b.NewStep(sps - 1);
	CHECK(!b.Admit(), "still full at the window's last step");
	b.NewStep(sps);
	CHECK(b.Total() == 0 && b.Admit(), "the first step left the window");
	b.NewStep(sps + 3 * sps);                  // a long jump forgets everything
	CHECK(b.Total() == 0, "a jump clears the window");
	// steady: 1 admit per step for 3 s with a limit of half the rate: exactly sps / 2 admitted per window
	PuffBudget s;
	s.Init(c, sps / 2);
	const long long a0 = AllocationCount();
	int admitted = 0;
	for (int step = 0; step < 3 * sps; ++step) {
		s.NewStep(step);
		admitted += s.Admit() ? 1 : 0;
	}
	CHECK(AllocationCount() == a0, "no allocation");
	CHECK(admitted == 3 * (sps / 2), "half the rate admitted each second for 3 s");
}

// ---- path puffs -----------------------------------------------------------------------------------------------------
KT1_RATE_CASE("SHOTF-PATHPUFFS", "path puffs at their rate per second, carried between steps") {
	const int sps = testCase.StepsPerSecond();
	const ShotFollowerConfig c;
	PathState st;
	int total = 0;
	for (int step = 0; step < sps; ++step) {
		const PathPuffs p = PathPuffsFor(c, st, step, 25, 1, 0, 1);
		total += p.count;
		CHECK(p.stepsBack == 1 && p.lifeK == 1, "every step: one step of travel, life as is");
	}
	CHECK(total == 25 || total == 24, "25 a second over one second (the carry may hold the last one)");
	// read every second step: the same rate per second, spread over two steps of travel
	PathState st2;
	int total2 = 0;
	for (int step = 0; step < 2 * sps; step += 2) {
		const PathPuffs p = PathPuffsFor(c, st2, step, 25, 1, 0, 2);
		total2 += p.count;
		if (step > 0) CHECK(p.stepsBack == 2, "two steps of travel");
	}
	const int reads = sps, spanSteps = 1 + 2 * (reads - 1);        // the first read counts one step, each later one two
	const int expect2 = 25 * spanSteps / sps;
	CHECK(total2 >= expect2 - 1 && total2 <= expect2, "25 a second over the span read, every second step");
	// a fast shot: speed 3 x the pack's lays 3 x as many, living a third as long
	PathState st3;
	const float packSpeed = 100;
	const PathPuffs f = PathPuffsFor(c, st3, 0, 30, 3 * packSpeed / sps, packSpeed, 1);
	CHECK(Near(f.lifeK, 1.0f / 3, 1e-4f) && f.count == 90 / sps, "faster than the pack: more, shorter puffs");
	// the cap: a huge rate lays at most pathPuffMax, spaced over all it gave
	PathState st4;
	const PathPuffs h = PathPuffsFor(c, st4, 0, 3000, 1, 0, 1);
	CHECK(h.count == c.pathPuffMax && h.slots == 3000 / sps, "capped count, full spacing");
	// a long gap counts as the Lua's 4 frames at most
	PathState st5;
	PathPuffsFor(c, st5, 0, 30, 1, 0, 1);
	const PathPuffs g = PathPuffsFor(c, st5, 100, 30, 1, 0, 1);
	CHECK(g.stepsBack == (sps == 30 ? 4 : 1), "gap clamped to the Lua's 4 frames (one step at 10)");
}

// ---- end point ------------------------------------------------------------------------------------------------------
KT1_CASE("SHOTF-ENDPOINT", "one step on, cut at the ground, lifted") {
	auto flat = [](float, float) { return 10.0f; };
	const Vec3 air = EndPoint(Vec3{0, 50, 0}, Vec3{4, -2, 0}, 2, flat);
	CHECK(Near(air.x, 4) && Near(air.y, 48), "in the air: one step on");
	const Vec3 hit = EndPoint(Vec3{0, 14, 0}, Vec3{8, -8, 0}, 0.5f, flat);
	CHECK(Near(hit.x, 4) && Near(hit.y, 10.5f), "into flat ground: cut half way (4 above, 4 below), lifted 0.5");
	const Vec3 lift = EndPoint(Vec3{0, 11, 0}, Vec3{1, 0, 0}, 2, flat);
	CHECK(Near(lift.y, 12), "level just above the ground: lifted to 2 above");
	auto slope = [](float x, float) { return x; };     // ground rising with x
	const Vec3 s = EndPoint(Vec3{0, 6, 0}, Vec3{10, -2, 0}, 0, slope);
	CHECK(Near(s.x, 5) && Near(s.y, 5), "into a slope: linear cut on height above ground");
}

// ---- seeded jitter --------------------------------------------------------------------------------------------------
KT1_CASE("SHOTF-LCG", "Park-Miller minimal standard, Lua floor modulo for the seed") {
	Lcg a(1);
	CHECK(Near(a.Next(), 16807.0f / 2147483647.0f, 1e-9f), "first value from seed 1");
	CHECK(Near(a.Next(), 282475249.0f / 2147483647.0f, 1e-7f), "second value from seed 1");
	Lcg z(0), m(2147483647);
	CHECK(Near(z.Next(), m.Next()), "seed 0 and the modulus: the same (both map to 2^31 - 2)");
	Lcg n(-5), p(2147483642);
	CHECK(Near(n.Next(), p.Next()), "a negative seed wraps as Lua's floor modulo");
	Lcg r1(7919 * 3 + 40), r2(7919 * 3 + 40);
	bool same = true;
	for (int i = 0; i < 50; ++i) {
		const float x = r1.Next();
		same = same && x == r2.Next() && x > 0 && x < 1;
	}
	CHECK(same, "the same seed gives the same sequence in (0, 1)");
}
