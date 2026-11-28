/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// Core/FxEvents: the missile relay, the death blast timing and the build work rules, engine-free.

#include "../KT1Test.h"

#include <array>
#include <cmath>

#include "FxTestWorld.h"

using namespace KT1::Test::Fx;

KT1_CASE("FXREL-STRATEGIC", "a strategic missile and its interceptor: launch, positions, ends, in the relay's order") {
	Rig r;
	MissileIn m;
	m.key = 100;
	m.strategic = 1;
	m.x = 500.0f;
	m.hitStep = 90;
	m.outerRadiusUnits = 10.0f;
	m.outerSteps = 3;
	InterceptorIn i;
	i.id = 4;
	i.targetKey = 100;
	const std::array<MissileIn, 1> ms = {m};
	std::array<InterceptorIn, 1> is = {i};
	RelayStrategic(r.state.strategic, r.config.strategic, ms, is, {}, 5, r.query, r.out);
	RelayStrategic(r.state.strategic, r.config.strategic, ms, is, {}, 6, r.query, r.out);
	KT1_CHECK((r.Types() == std::vector<int>{Kind<StrategicLaunch>(), Kind<StrategicInterceptor>(),
		Kind<StrategicInterceptorPos>(), Kind<StrategicInterceptorPos>()}));
	KT1_CHECK_EQ(r.Get<StrategicLaunch>().id, 1);
	KT1_CHECK_EQ(r.Get<StrategicLaunch>().ringElmos, 80.0f);
	KT1_CHECK_EQ(r.Get<StrategicInterceptor>().missile, 1);
	r.out.Clear();
	std::array<MissileIn, 1> dead = {m};
	dead[0].end = MissileEnd::ShotDown;
	dead[0].hasInterceptPos = 1;
	dead[0].interceptX = 300.0f;
	is[0].end = MissileEnd::Hit;
	is[0].lastStep = 7;
	RelayStrategic(r.state.strategic, r.config.strategic, dead, is, {}, 7, r.query, r.out);
	RelayStrategic(r.state.strategic, r.config.strategic, dead, is, {}, 8, r.query, r.out);   // told once
	KT1_CHECK((r.Types() == std::vector<int>{Kind<StrategicInterceptorEnd>(), Kind<StrategicEnd>()}));
	KT1_CHECK_EQ(r.Get<StrategicInterceptorEnd>().result, 1);
	KT1_CHECK_EQ(r.Get<StrategicEnd>().result, 2);
	KT1_CHECK_EQ(r.Get<StrategicEnd>().x, 300.0f);
	KT1_CHECK_EQ(r.Get<StrategicEnd>().step, 7);
	KT1_CHECK_EQ(r.Get<StrategicEnd>().bands, 3);
	KT1_CHECK(r.state.strategic.tracked.empty());
}

KT1_CASE("FXREL-TACTICAL", "a tactical missile: progress, a redirection's carried profile, its end") {
	Rig r;
	MissileIn m;
	m.key = 1;
	m.x = 100.0f;
	m.launchStep = 0;
	m.hitStep = 10;
	m.hasPos = 1;
	m.gx = 20.0f;
	std::array<MissileIn, 1> ms = {m};
	const auto relay = [&](int step, std::span<const RedirectIn> turns = {}) {
		RelayStrategic(r.state.strategic, r.config.strategic, ms, {}, turns, step, r.query, r.out);
	};
	relay(2);
	KT1_CHECK((r.Types() == std::vector<int>{Kind<TacticalLaunch>(), Kind<TacticalPos>()}));
	KT1_CHECK_EQ(r.Get<TacticalPos>().progress, 0.2f);
	KT1_CHECK_EQ(r.Get<TacticalPos>().pathLength, 100.0f);
	r.out.Clear();
	ms[0].redirected = 1;
	ms[0].launchStep = 4;
	ms[0].hitStep = 14;
	ms[0].x = 900.0f;
	const std::array<RedirectIn, 1> turn = {{{1, 55, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f}}};
	relay(9, turn);
	KT1_CHECK((r.Types() == std::vector<int>{Kind<MissileRedirected>(), Kind<TacticalPos>()}));
	KT1_CHECK_EQ(r.Get<MissileRedirected>().id, 1);
	KT1_CHECK_EQ(r.Get<TacticalPos>().progress, 0.2f + (1.0f - 0.2f) * 0.5f);
	KT1_CHECK_EQ(r.Get<TacticalPos>().pathLength, 100.0f);
	r.out.Clear();
	ms[0].end = MissileEnd::Hit;
	relay(14);
	KT1_CHECK((r.Types() == std::vector<int>{Kind<TacticalEnd>(), Kind<StrategicTactical>()}));
	KT1_CHECK_EQ(r.Get<TacticalEnd>().result, 1);
	KT1_CHECK_EQ(r.Get<TacticalEnd>().x, 20.0f);
	KT1_CHECK_EQ(r.Get<StrategicTactical>().x, 900.0f);
}

KT1_CASE("FXREL-CHAIN", "a death's delay and chain are rolled from the unit id, in the Lua's order") {
	const CriteriaTimingConfig c;
	CriteriaRng a(7), b(7), first(0);
	KT1_CHECK_EQ(first.state, 12345u);
	for (int i = 0; i < 100; ++i) {
		const float x = a.Next();
		KT1_CHECK(x >= 0.0f && x < 1.0f);
		KT1_CHECK_EQ(x, b.Next());
	}
	CriteriaRng rng(7), expect(7);
	const DeathChain chain = PlanDeathChain(c, 4.0f, DeathMedium::Land, true, 0.0f, rng);
	KT1_CHECK_EQ(chain.delayS, 0.2f + 0.1f * expect.Next());
	const int n = std::max(1, std::min(8, static_cast<int>(std::floor(0.75f * 4.0f * (1.0f + expect.Next())))));
	KT1_CHECK_EQ(chain.count, n);
	KT1_CHECK_EQ(chain.mainS, 1.6f);
	KT1_CHECK_EQ(chain.timesS[0], 1.6f * (0.3f + 0.55f * 0.0f) + 0.05f * expect.Next());
	CriteriaRng air(7);
	KT1_CHECK_EQ(PlanDeathChain(c, 4.0f, DeathMedium::Air, true, 0.0f, air).delayS, 0.0f);
	CriteriaRng small(7);
	KT1_CHECK_EQ(PlanDeathChain(c, 0.5f, DeathMedium::Land, true, 0.0f, small).count, 0);
	CriteriaRng anim(7);
	const DeathChain played = PlanDeathChain(c, 4.0f, DeathMedium::Land, true, 1.0f, anim);
	KT1_CHECK(played.count >= 5 && played.count <= 10);
	KT1_CHECK_EQ(played.mainS, played.timesS[played.count - 1] + 0.3f);
}

KT1_CASE("FXREL-MEDIUM", "a death is in the air high up, else on water over a sea floor, else on land") {
	const CriteriaTimingConfig c;
	KT1_CHECK(MediumOfDeath(c, 40.0f, 10.0f) == DeathMedium::Air);
	KT1_CHECK(MediumOfDeath(c, 34.0f, 10.0f) == DeathMedium::Land);
	KT1_CHECK(MediumOfDeath(c, 0.0f, -50.0f) == DeathMedium::Water);
	KT1_CHECK(MediumOfDeath(c, 25.0f, -50.0f) == DeathMedium::Air);
}

KT1_CASE("FXREL-BUILDWORK", "a builder's target, reach and nano piece") {
	const BuildTarget build = TargetOf({5, 0, -1, 0}, 1000);
	KT1_CHECK(build.working && build.kind == BuildWork::Build && build.target == 5);
	KT1_CHECK(TargetOf({5, 40, 5, 1}, 1000).kind == BuildWork::Repair);
	const BuildTarget wreck = TargetOf({-1, 90, 1007, 1}, 1000);
	KT1_CHECK(wreck.working && wreck.feature && wreck.target == 7 && wreck.kind == BuildWork::Reclaim);
	KT1_CHECK(!TargetOf({-1, 90, 7, 4}, 1000).working);      // an area order
	KT1_CHECK(TargetOf({-1, 130, 7, 1}, 1000).kind == BuildWork::Capture);
	KT1_CHECK(!TargetOf({-1, 10, 7, 1}, 1000).working);
	KT1_CHECK(!TargetOf({-1, 0, -1, 0}, 1000).working);
	const BuildWorkConfig c;
	KT1_CHECK(InReach(c, 154.0f, 100.0f, 30.0f));
	KT1_CHECK(!InReach(c, 154.5f, 100.0f, 30.0f));
	const std::array<std::string_view, 5> pieces = {"base", "flare", "NanoB", "nanoa", "beam"};
	KT1_CHECK_EQ(NanoPiece(c, pieces), 3);
	KT1_CHECK_EQ(NanoPiece(c, std::span<const std::string_view>(pieces).first(1)), -1);
}
