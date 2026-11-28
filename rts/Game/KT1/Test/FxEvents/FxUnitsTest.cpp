/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// Core/FxEvents/FxLogic.h: a unit's death, damage states, build sets, hidden wrecks and the step, engine-free.

#include "../KT1Test.h"

#include <cmath>

#include "FxTestWorld.h"

using namespace KT1::Test::Fx;
using KT1::Core::Rate;

namespace {

UnitNow Land(int32_t defId) {
	UnitNow u;
	u.pos = {100.0f, 12.0f, 100.0f};
	u.radius = 32.0f;
	u.height = 40.0f;
	u.health = 100.0f;
	u.maxHealth = 100.0f;
	u.box[0] = u.box[1] = u.box[2] = 16.0f;
	u.defId = defId;
	u.team = 2;
	u.allyTeam = 1;
	u.heading = 77;
	return u;
}

// unit def 1: a giant structure with build slot 2; unit def 2: a mobile unit
void Units(Rig& r) {
	r.defs.units.resize(4);
	r.defs.units[1] = {DeathKind::Structure, 2, true, true, 4.0f};
	r.defs.features.assign(2, SynFeatureDef{});
	r.defs.features[1].unitWreck = true;
	r.defs.giantCore = Ref(30);
	r.defs.giantScorchWeapon = 5;
}

void DeathData(Rig& r) {
	DeathConfig& c = r.config.death;
	c.bound_xz_factor = 0.5f;
	c.selection.push_back({"land", 0.0f, -1.0f, "S", "X", 100.0f});
	c.sequence.push_back({"mobile", 2, 4});
	c.sequence.push_back({"structure", 1, 1});
	c.timed_threshold = 2.0f;
	c.timed_wait_ticks[0] = 1.0f;
	c.timed_wait_ticks[1] = 3.0f;
	c.timed_count_per_legacy[1] = 1.0f;
	c.timed_offset_box_fraction = 0.5f;
	c.timed_pause_ticks = 2;
	c.damage_counts.push_back({0.0f, 2, 1.0f});
	c.damage_states.push_back({"mobile", "1", 0.75f, "D1"});
	c.damage_states.push_back({"mobile", "2", 0.5f, "D2+D3"});
	ResolveDeathPlan(c, r.defs.death);
	for (size_t i = 0; i < r.defs.death.sets.size(); ++i) r.defs.deathSets.push_back(Ref(100 + static_cast<int>(i)));
}

int32_t SetEffect(const Rig& r, const char* name) {
	for (size_t i = 0; i < r.defs.death.sets.size(); ++i)
		if (r.defs.death.sets[i] == name) return 100 + static_cast<int32_t>(i);
	return -2;
}

} // namespace

KT1_RATE_CASE("FXEV-DEATH-ORDER", "a giant's death raises Death, FogDeath, its blast and ring, then CritDeath") {
	Rig r;
	r.AllRows();
	Units(r);
	r.config.death.enabled = false;
	r.Init();
	OnUnitDestroyed(r.Env(10), {7, Land(1)});
	KT1_CHECK((r.Types() == std::vector<int>{Kind<Death>(), Kind<FogDeath>(), Kind<Blast>(), Kind<Scorch>(), Kind<Scorch>(),
		Kind<Scorch>(), Kind<Scorch>(), Kind<Scorch>(), Kind<CritDeath>()}));
	KT1_CHECK_EQ(r.out.spawns.size(), size_t(1));
	KT1_CHECK_EQ(r.out.spawns[0].effect, 30);
	KT1_CHECK(r.out.spawns[0].priority == Priority::Death);
	KT1_CHECK_EQ(r.out.spawns[0].pos.y, 22.0f);
	KT1_CHECK(r.Get<Blast>().cls == BlastClass::ExperimentalDeath);
	KT1_CHECK_EQ(r.Get<Blast>().weaponDef, -1);
	KT1_CHECK_EQ(r.Get<Scorch>(0).pos.x, 100.0f);
	KT1_CHECK_EQ(r.Get<Scorch>(1).pos.x, 170.0f);
	KT1_CHECK_EQ(r.Get<Scorch>(2).pos.z, 170.0f);
	KT1_CHECK_EQ(r.Get<Scorch>(3).pos.x, 30.0f);
	KT1_CHECK_EQ(r.Get<Scorch>(4).pos.z, 30.0f);
	KT1_CHECK(r.Get<Scorch>(4).source == ScorchSource::Death);
	KT1_CHECK_EQ(r.Get<CritDeath>().heading, 77);
	KT1_CHECK(r.Get<CritDeath>().medium == DeathMedium::Land);
	KT1_CHECK_EQ(r.Get<Death>().finished, 1);
}

KT1_RATE_CASE("FXEV-DEATH-SITE", "an unfinished site raises only Death, marked unfinished") {
	Rig r;
	r.AllRows();
	Units(r);
	r.config.death.enabled = false;
	r.Init();
	UnitNow site = Land(1);
	site.build = 0.4f;
	site.beingBuilt = true;
	OnUnitDestroyed(r.Env(10), {7, site});
	KT1_CHECK((r.Types() == std::vector<int>{Kind<Death>()}));
	KT1_CHECK_EQ(r.Get<Death>().finished, 0);
	KT1_CHECK(r.out.spawns.empty());
}

KT1_RATE_CASE("FXEV-WRECK-HIDE", "a unit wreck near a land death is hidden until the main blast") {
	Rig r;
	Units(r);
	r.config.death.enabled = false;
	r.Init();
	const int sps = testCase.StepsPerSecond();
	OnUnitDestroyed(r.Env(10), {7, Land(2)});            // mobile: the start delay alone, 0.2 to 0.3 s
	CriteriaRng rng(7);
	const float mainAt = MainBlastAfterS(r.config.criteria, 0.0f, DeathMedium::Land, false, rng);
	const int show = 10 + static_cast<int>(std::ceil(mainAt * static_cast<float>(sps))) + Rate::Steps(0.067f);
	OnFeatureCreated(r.Env(11), 3, 0, {100.0f, 0.0f, 100.0f});      // no unit wreck
	OnFeatureCreated(r.Env(11), 4, 1, {900.0f, 0.0f, 100.0f});      // too far
	OnFeatureCreated(r.Env(11), 5, 1, {110.0f, 0.0f, 100.0f});
	OnFeatureCreated(r.Env(11), 6, 1, {110.0f, 0.0f, 100.0f});      // the death is used up
	KT1_CHECK((r.out.hideFeatures == std::vector<int32_t>{5}));
	for (int step = 11; step < show; ++step) StepFx(r.Env(step));
	KT1_CHECK(r.out.showFeatures.empty());
	StepFx(r.Env(show));
	KT1_CHECK((r.out.showFeatures == std::vector<int32_t>{5}));
	OnUnitDestroyed(r.Env(500), {8, Land(2)});
	StepFx(r.Env(500 + Rate::Steps(2.0f) + 1));          // no wreck came: the wait is dropped
	KT1_CHECK(r.state.hideWaits.empty());
}

KT1_RATE_CASE("FXEV-DEATH-SETS", "a death's sets wait their drawn ticks and spawn in the order made") {
	Rig r;
	Units(r);
	DeathData(r);
	r.Init();
	const int tick = Rate::Every(0.1f);
	KT1_CHECK_EQ(tick, testCase.StepsPerSecond() == 30 ? 3 : 1);
	OnUnitDestroyed(r.Env(10), {7, Land(2)});
	KT1_CHECK_EQ(r.world.ranges.size(), size_t(1));      // one draw: the wait, 2 to 4 ticks
	KT1_CHECK_EQ(r.world.draws, 0);
	KT1_CHECK(r.out.spawns.empty());
	for (int step = 11; step < 10 + 2 * tick; ++step) StepFx(r.Env(step));
	KT1_CHECK(r.out.spawns.empty());
	StepFx(r.Env(10 + 2 * tick));
	KT1_CHECK_EQ(r.out.spawns.size(), size_t(2));
	KT1_CHECK_EQ(r.out.spawns[0].effect, SetEffect(r, "S"));
	KT1_CHECK_EQ(r.out.spawns[1].effect, SetEffect(r, "X"));
	KT1_CHECK_EQ(r.out.spawns[0].pos.y, 12.0f + 2.0f * 8.0f * 0.5f);
	KT1_CHECK((r.Types() == std::vector<int>{Kind<DeathSet>(), Kind<DeathSet>()}));
	KT1_CHECK_EQ(r.Get<DeathSet>().unit, 7);
	KT1_CHECK(r.state.pending.empty());
}

KT1_RATE_CASE("FXEV-DEATH-TIMED", "a wide structure's timed hits draw three places and one wait each, then the sets") {
	Rig r;
	Units(r);
	r.defs.units[1].giant = false;
	DeathData(r);
	r.Init();
	const int tick = Rate::Every(0.1f);
	UnitNow u = Land(1);
	u.box[0] = u.box[2] = 24.0f;                         // 3 map units: 3 hits
	OnUnitDestroyed(r.Env(10), {7, u});
	KT1_CHECK_EQ(r.world.draws, 9);
	KT1_CHECK_EQ(r.world.ranges.size(), size_t(4));
	KT1_CHECK_EQ(r.state.pending.size(), size_t(5));
	const int first = 10 + tick;
	KT1_CHECK_EQ(r.state.pending[0].at, first);
	KT1_CHECK_EQ(r.state.pending[1].at, first + tick);
	KT1_CHECK_EQ(r.state.pending[3].at, first + 3 * tick + 2 * tick);
	KT1_CHECK_EQ(r.state.pending[0].set, r.defs.death.timedSet);
}

KT1_RATE_CASE("FXEV-DAMAGE", "a damaged unit's states spawn by the scan, repeat, and stop when it heals") {
	Rig r;
	Units(r);
	DeathData(r);
	r.Init();
	const int scan = Rate::Every(0.5f), repeat = Rate::Every(2.0f);
	UnitNow u = Land(2);
	u.health = 40.0f;
	r.world.units[7] = u;
	OnUnitDamaged(r.Env(1), {7, -1, -1, 1.0f});
	StepFx(r.Env(scan + 1));
	KT1_CHECK(r.out.spawns.empty());
	StepFx(r.Env(scan));
	// two states, two emitters each: D1, D1, then D2+D3 twice
	KT1_CHECK_EQ(r.out.spawns.size(), size_t(6));
	KT1_CHECK_EQ(r.out.spawns[0].effect, SetEffect(r, "D1"));
	KT1_CHECK_EQ(r.out.spawns[2].effect, SetEffect(r, "D2"));
	KT1_CHECK_EQ(r.out.spawns[3].effect, SetEffect(r, "D3"));
	KT1_CHECK(r.out.spawns[0].priority == Priority::State);
	KT1_CHECK_EQ(r.world.ranges.size(), size_t(4));
	KT1_CHECK_EQ(r.world.draws, 18);
	StepFx(r.Env(scan * 2));
	KT1_CHECK_EQ(r.out.spawns.size(), size_t(6));
	StepFx(r.Env(scan + repeat));
	KT1_CHECK_EQ(r.out.spawns.size(), size_t(12));
	r.world.units[7].health = 90.0f;
	StepFx(r.Env(scan + repeat * 2));
	KT1_CHECK_EQ(r.state.damaged.Size(), 0);
}

KT1_RATE_CASE("FXEV-BUILD", "build sets are made in unit id order at the slot's period, with draws for refused ones") {
	Rig r;
	Units(r);
	r.config.build.pool_shrink_progress = 0.5f;
	r.defs.buildSlots[2] = {{Ref(40), Ref(41)}, 6, false};
	r.Init();
	UnitNow u = Land(1);
	u.build = 0.25f;
	r.world.units[9] = u;
	r.world.units[3] = u;
	OnUnitCreated(r.Env(1), 9, 1, 0.0f);
	OnUnitCreated(r.Env(1), 3, 1, 0.0f);
	OnUnitCreated(r.Env(1), 4, 2, 0.0f);                 // no slot
	OnUnitCreated(r.Env(1), 5, 1, 1.0f);                 // already built
	KT1_CHECK_EQ(r.state.building.Size(), 2);
	StepFx(r.Env(7));
	KT1_CHECK(r.out.spawns.empty());
	StepFx(r.Env(12));
	KT1_CHECK_EQ(r.out.spawns.size(), size_t(4));
	KT1_CHECK_EQ(r.world.draws, 12);
	KT1_CHECK_EQ(r.Get<BuildSet>(0).unit, 3);
	KT1_CHECK_EQ(r.Get<BuildSet>(2).unit, 9);
	KT1_CHECK_EQ(r.Get<BuildSet>(1).effect, 41);
	KT1_CHECK_EQ(r.out.spawns[0].pos.y, 12.0f + 0.5f * 40.0f * 0.25f);
	r.state.budget.SetScale(0.0f);
	StepFx(r.Env(18));
	KT1_CHECK_EQ(r.world.draws, 24);
	KT1_CHECK_EQ(r.Count<BuildSet>(), 4);
	r.defs.buildSlots[2].pool = true;
	r.world.units[3].build = r.world.units[9].build = 0.9f;
	StepFx(r.Env(24));
	KT1_CHECK_EQ(r.world.draws, 24);                     // the pool has shrunk: nothing drawn
	OnUnitFinished(r.Env(25), 9);
	OnUnitDestroyed(r.Env(25), {3, u});
	KT1_CHECK_EQ(r.state.building.Size(), 0);
}

KT1_RATE_CASE("FXEV-STEP-ORDER", "a step: the Scale event, then build sets, due death sets and damage states") {
	Rig r;
	Units(r);
	DeathData(r);
	r.defs.buildSlots[2] = {{Ref(40)}, 1, false};
	r.Init();
	const int step = Rate::Every(0.5f) * 2;              // a whole second: the scan and the Scale event
	UnitNow site = Land(1);
	site.build = 0.5f;
	r.world.units[3] = site;
	UnitNow hurt = Land(2);
	hurt.health = 60.0f;
	r.world.units[7] = hurt;
	OnUnitCreated(r.Env(1), 3, 1, 0.0f);
	OnUnitDamaged(r.Env(1), {7, -1, -1, 1.0f});
	OnUnitDestroyed(r.Env(1), {8, Land(2)});
	StartStep(r.Env(step));
	StepFx(r.Env(step));
	KT1_CHECK((r.Types() == std::vector<int>{Kind<Scale>(), Kind<BuildSet>(), Kind<DeathSet>(), Kind<DeathSet>(),
		Kind<DeathSet>(), Kind<DeathSet>()}));
	KT1_CHECK_EQ(r.out.spawns[0].effect, 40);
	KT1_CHECK_EQ(r.out.spawns[1].effect, SetEffect(r, "S"));
	KT1_CHECK_EQ(r.out.spawns[3].effect, SetEffect(r, "D1"));
}
