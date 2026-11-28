/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// Core/FxEvents/FxBudget.h and the budgeted spawn, engine-free.

#include "../KT1Test.h"

#include "FxTestWorld.h"

using namespace KT1::Test::Fx;
using KT1::Core::Rate;

KT1_CASE("FXBUD-POINTS", "effects per second follow the budget points, linear between, flat above the last") {
	const BudgetConfig c;
	KT1_CHECK_EQ(BudgetPerSecond(c, -1.0f), 0.0f);
	KT1_CHECK_EQ(BudgetPerSecond(c, 0.0f), 0.0f);
	KT1_CHECK_EQ(BudgetPerSecond(c, 0.25f), 150.0f);
	KT1_CHECK_EQ(BudgetPerSecond(c, 0.5f), 300.0f);
	KT1_CHECK_EQ(BudgetPerSecond(c, 0.75f), 750.0f);
	KT1_CHECK_EQ(BudgetPerSecond(c, 1.0f), 1200.0f);
	KT1_CHECK_EQ(BudgetPerSecond(c, 1.5f), 2100.0f);
	KT1_CHECK_EQ(BudgetPerSecond(c, 2.0f), 3000.0f);
	KT1_CHECK_EQ(BudgetPerSecond(c, 5.0f), 3000.0f);
}

KT1_RATE_CASE("FXBUD-PERSTEP", "the per step limit is the second's share of a step with the burst, at least the minimum") {
	const BudgetConfig c;
	const bool thirty = testCase.StepsPerSecond() == 30;
	KT1_CHECK_EQ(BudgetPerStep(c, 0.0f), 0);
	KT1_CHECK_EQ(BudgetPerStep(c, 0.5f), thirty ? 16 : 48);
	KT1_CHECK_EQ(BudgetPerStep(c, 1.0f), thirty ? 64 : 192);
	KT1_CHECK_EQ(BudgetPerStep(c, 2.0f), thirty ? 160 : 480);
	KT1_CHECK_EQ(BudgetPerStep(c, 0.001f), 4);
}

KT1_RATE_CASE("FXBUD-OFF", "scale 0 refuses everything, hit confirmation too") {
	Budget b;
	b.Init({}, 0.0f);
	Refusal why = Refusal::None;
	KT1_CHECK(!b.Admit(Priority::Hit, &why));
	KT1_CHECK(why == Refusal::Off);
	KT1_CHECK_EQ(b.Stats().off, 1u);
	KT1_CHECK_EQ(b.Stats().dropped[0], 1u);
	KT1_CHECK_EQ(b.Stats().requested[0], 1u);
}

KT1_RATE_CASE("FXBUD-STEP", "the step's limit holds all but hits, and a new step opens it again") {
	Budget b;
	b.Init({}, 1.0f);
	b.NewStep(1);
	const int perStep = BudgetPerStep({}, 1.0f);
	for (int i = 0; i < perStep; ++i) KT1_CHECK(b.Admit(Priority::Death));
	Refusal why = Refusal::None;
	KT1_CHECK(!b.Admit(Priority::Death, &why));
	KT1_CHECK(why == Refusal::Step);
	KT1_CHECK(b.Admit(Priority::Hit));
	b.NewStep(2);
	KT1_CHECK(b.Admit(Priority::Death));
	KT1_CHECK_EQ(b.Stats().step, 1u);
}

KT1_RATE_CASE("FXBUD-ORDER", "a filling second cuts ambient first, then state, trail, shield, death; never hits") {
	Budget b;
	b.Init({}, 1.0f);
	const int sps = testCase.StepsPerSecond();
	int step = 1, spent = 0;
	const auto fill = [&](int target) {          // spend with hits, which count but are never refused
		while (spent < target) { b.Admit(Priority::Hit); ++spent; }
	};
	const Priority cut[] = {Priority::Ambient, Priority::State, Priority::Trail, Priority::Shield, Priority::Death};
	const int at[] = {600, 720, 900, 1020, 1140};
	for (int i = 0; i < 5; ++i) {
		fill(at[i] - 1);
		b.NewStep(step++ % (sps - 1) + 1);
		KT1_CHECK(b.Admit(cut[i]));
		++spent;
		Refusal why = Refusal::None;
		KT1_CHECK(!b.Admit(cut[i], &why));
		KT1_CHECK(why == Refusal::Second);
		for (int higher = i + 1; higher < 5; ++higher) {
			KT1_CHECK(b.Admit(cut[higher]));
			++spent;
		}
	}
	fill(2000);
	KT1_CHECK(b.Admit(Priority::Hit));
}

KT1_RATE_CASE("FXBUD-SECOND", "the second turns over every second's steps and only then") {
	Budget b;
	b.Init({}, 1.0f);
	const int sps = testCase.StepsPerSecond();
	for (int i = 0; i < 1200; ++i) b.Admit(Priority::Hit);
	b.NewStep(sps - 1);
	KT1_CHECK(!b.Admit(Priority::Death));
	b.NewStep(sps);
	KT1_CHECK(b.Admit(Priority::Death));
}

KT1_RATE_CASE("FXBUD-SPAWN", "pooled and unknown effects ask nothing; low settings draw a large hit one tier smaller") {
	Rig r;
	r.Init(0.5f);
	const FxEnv env = r.Env(1);
	KT1_CHECK(!Spawn(env, {5, 5, true}, {}, {}, Priority::Hit));
	KT1_CHECK(!Spawn(env, EffectRef{}, {}, {}, Priority::Hit));
	KT1_CHECK_EQ(r.state.budget.Stats().requested[0], 0u);
	KT1_CHECK(Spawn(env, {5, 4, false}, {1.0f, 2.0f, 3.0f}, {0.0f, 1.0f, 0.0f}, Priority::Hit, 7.0f));
	KT1_CHECK(Spawn(env, {5, 4, false}, {}, {}, Priority::Death));
	KT1_CHECK_EQ(r.out.spawns.size(), size_t(2));
	KT1_CHECK_EQ(r.out.spawns[0].effect, 4);
	KT1_CHECK_EQ(r.out.spawns[0].length, 7.0f);
	KT1_CHECK_EQ(r.out.spawns[0].pos.z, 3.0f);
	KT1_CHECK_EQ(r.out.spawns[1].effect, 5);
	KT1_CHECK_EQ(r.state.spawnedTotal, 2u);
	r.state.budget.SetScale(1.0f);
	KT1_CHECK(Spawn(env, {5, 4, false}, {}, {}, Priority::Hit));
	KT1_CHECK_EQ(r.out.spawns[2].effect, 5);
}
