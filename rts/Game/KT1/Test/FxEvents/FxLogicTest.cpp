/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// Core/FxEvents/FxLogic.h: explosions, hits, projectiles and the relays, engine-free.

#include "../KT1Test.h"

#include "FxTestWorld.h"

using namespace KT1::Test::Fx;
using KT1::Core::Rate;

namespace {

// weapon 1 has every explosion part at once
void FullWeapon(Rig& r) {
	r.defs.weapons.resize(8);
	SynWeapon& w = r.defs.weapons[1];
	w.hit = Ref(10);
	w.decal = true;
	w.decalSize = 30.0f;
	w.decalLife = 9.0f;
	w.gated.on = true;
	w.gated.leaves = true;
	w.gated.gate = 2.0f;                 // always
	w.gated.size = 1.0f;
	w.gated.sizePlus = 1.0f;
	w.gated.life = 5.0f;
	w.gated.lifePlus = 10.0f;
	w.gated.radius = 40.0f;
	w.gated.puff = Ref(11);
	w.depthTier = 3;
	w.blastClass = BlastClass::Artillery;
	w.blastFamily = BlastFamily::Missile;
	w.faction = Faction::Grey;
	w.blastCore = Ref(12);
	w.scorchWatched = true;
	w.script = ScriptTree::ClsA;
}

} // namespace

KT1_RATE_CASE("FXEV-EXPLOSION-ORDER", "an explosion's spawns and events come in the Lua gadgets' order") {
	Rig r;
	r.AllRows();
	FullWeapon(r);
	r.Init();
	OnExplosion(r.Env(7), {1, 3, {100.0f, 20.0f, 200.0f}});
	KT1_CHECK_EQ(r.out.spawns.size(), size_t(3));
	KT1_CHECK_EQ(r.out.spawns[0].effect, 10);
	KT1_CHECK_EQ(r.out.spawns[1].effect, 11);
	KT1_CHECK_EQ(r.out.spawns[1].pos.y, 10.0f);          // the puff lies on the ground
	KT1_CHECK_EQ(r.out.spawns[2].effect, 12);
	KT1_CHECK((r.Types() == std::vector<int>{Kind<Scorch>(), Kind<Scorch>(), Kind<DepthHit>(), Kind<Blast>(), Kind<Scorch>(),
		Kind<ScriptBlast>(), Kind<Impact>()}));
	const Scorch decal = r.Get<Scorch>(0), gated = r.Get<Scorch>(1), mark = r.Get<Scorch>(2);
	KT1_CHECK_EQ(decal.size, 30.0f);
	KT1_CHECK_EQ(decal.lifeS, 9.0f);
	KT1_CHECK_EQ(decal.pos.y, 0.0f);
	KT1_CHECK(decal.source == ScorchSource::Parity);
	const float extra = 68.0f / 101.0f;                  // (100 * 7 + 200 * 13) % 101 = 3300 % 101 = 68
	KT1_CHECK_EQ(gated.size, (1.0f + 1.0f * extra) * 40.0f * 2.0f);
	KT1_CHECK_EQ(gated.lifeS, 5.0f + 10.0f * extra);
	KT1_CHECK(mark.source == ScorchSource::Explosion);
	KT1_CHECK_EQ(mark.pos.y, 20.0f);
	KT1_CHECK_EQ(mark.size, 0.0f);
	KT1_CHECK_EQ(r.Get<DepthHit>().tier, 3);
	KT1_CHECK_EQ(r.Get<DepthHit>().seen, 5u);
	KT1_CHECK(r.Get<Blast>().cls == BlastClass::Artillery && r.Get<Blast>().faction == Faction::Grey);
	KT1_CHECK_EQ(r.Get<Blast>().weaponDef, 1);
	KT1_CHECK(r.Get<ScriptBlast>().tree == ScriptTree::ClsA);
	KT1_CHECK_EQ(r.Get<Impact>().owner, 3);
}

KT1_RATE_CASE("FXEV-MAP", "without a row a sim event raises nothing, and the spawns stay") {
	Rig r;
	FullWeapon(r);
	r.Init();
	OnExplosion(r.Env(1), {1, -1, {100.0f, 20.0f, 200.0f}});
	OnExplosion(r.Env(1), {-1, -1, {}});
	KT1_CHECK(r.out.events.empty());
	KT1_CHECK_EQ(r.out.spawns.size(), size_t(3));
	r.Row("Explosion", "DepthHit");
	r.config.events.back().weapon = "some_weapon";       // Core cannot read a filtered row
	OnExplosion(r.Env(1), {1, -1, {100.0f, 20.0f, 200.0f}});
	KT1_CHECK(r.out.events.empty());
}

KT1_RATE_CASE("FXEV-GATE", "a gated mark needs land, no air burst, and its hash or distance gate") {
	Rig r;
	r.AllRows();
	r.defs.weapons.resize(8);
	GatedScorch& g = r.defs.weapons[2].gated;
	g.on = true;
	g.gate = -1.0f;                                      // the hash never opens it
	g.radius = 40.0f;
	g.puff = Ref(11);
	r.Init();
	OnExplosion(r.Env(1), {2, -1, {100.0f, 20.0f, 100.0f}});   // the first
	OnExplosion(r.Env(2), {2, -1, {110.0f, 20.0f, 100.0f}});   // near the last: shut
	KT1_CHECK_EQ(r.out.spawns.size(), size_t(1));
	OnExplosion(r.Env(3), {2, -1, {141.0f, 20.0f, 100.0f}});   // far from the last
	KT1_CHECK_EQ(r.out.spawns.size(), size_t(2));
	OnExplosion(r.Env(4), {2, -1, {500.0f, 66.0f, 100.0f}});   // radius + 16 above the ground: air
	KT1_CHECK_EQ(r.out.spawns.size(), size_t(2));
	r.world.ground = 0.0f;
	OnExplosion(r.Env(5), {2, -1, {900.0f, 1.0f, 100.0f}});    // not land
	KT1_CHECK_EQ(r.out.spawns.size(), size_t(2));
}

KT1_RATE_CASE("FXEV-BEAM", "a beam's blast comes once per cell in its gap; its hit sets once per shooter in theirs") {
	Rig r;
	r.AllRows();
	r.defs.weapons.resize(8);
	SynWeapon& w = r.defs.weapons[1];
	w.blastClass = BlastClass::SmallArms;
	w.beamBlast = true;
	w.blastCore = Ref(12);
	SynWeapon& c = r.defs.weapons[2];
	c.hit = Ref(10);
	c.contactSteps = 3;
	r.Init();
	const int gap = Rate::Every(0.333f);
	KT1_CHECK_EQ(gap, testCase.StepsPerSecond() == 30 ? 10 : 3);
	const Vec3 at = {100.0f, 0.0f, 100.0f};
	OnExplosion(r.Env(5), {1, -1, at});
	OnExplosion(r.Env(5 + gap - 1), {1, -1, at});
	KT1_CHECK_EQ(r.Count<Blast>(), 1);
	OnExplosion(r.Env(5 + gap - 1), {1, -1, {400.0f, 0.0f, 100.0f}});   // another cell
	OnExplosion(r.Env(5 + gap), {1, -1, at});
	KT1_CHECK_EQ(r.Count<Blast>(), 3);
	r.out.Clear();
	OnExplosion(r.Env(100), {2, 4, at});
	OnExplosion(r.Env(102), {2, 4, at});
	OnExplosion(r.Env(102), {2, 5, at});
	OnExplosion(r.Env(103), {2, 4, at});
	OnExplosion(r.Env(103), {2, -1, at});                // no shooter: never held back
	OnExplosion(r.Env(103), {2, -1, at});
	KT1_CHECK_EQ(r.out.spawns.size(), size_t(5));
}

KT1_RATE_CASE("FXEV-STEP-LIMITS", "scorch, script and blast requests stop at their step limits and open next step") {
	Rig r;
	r.AllRows();
	r.config.relay.scorch_a_step = 2;
	r.config.relay.script_blasts_a_step = 1;
	r.config.relay.blasts_a_step = 3;
	r.defs.weapons.resize(8);
	SynWeapon& w = r.defs.weapons[1];
	w.scorchWatched = true;
	w.script = ScriptTree::NukDq;
	w.blastClass = BlastClass::TankShell;
	r.Init();
	for (int i = 0; i < 5; ++i) OnExplosion(r.Env(9), {1, -1, {}});
	KT1_CHECK_EQ(r.Count<Scorch>(), 2);
	KT1_CHECK_EQ(r.Count<ScriptBlast>(), 1);
	KT1_CHECK_EQ(r.Count<Blast>(), 3);
	KT1_CHECK(!ForwardBlast(r.Env(9), BlastClass::TankShell, BlastFamily::Cannon, Faction::Human, {}, -1));
	KT1_CHECK(ForwardBlast(r.Env(10), BlastClass::TankShell, BlastFamily::Cannon, Faction::Human, {}, -1));
	OnExplosion(r.Env(10), {1, -1, {}});
	KT1_CHECK_EQ(r.Count<Scorch>(), 3);
}

KT1_RATE_CASE("FXEV-UNIT-HIT", "a unit at the explosion takes the unit set, and the confirmation follows at its gap") {
	Rig r;
	r.defs.weapons.resize(8);
	SynWeapon& w = r.defs.weapons[1];
	w.hit = Ref(10);
	w.unitHitTier = 1;
	for (int t = 0; t < 4; ++t) w.confirm[t] = Ref(20 + t);
	r.Init();
	r.world.near = true;
	OnExplosion(r.Env(1), {1, -1, {}});
	KT1_CHECK(r.out.spawns.empty());
	UnitNow target;
	target.pos = {100.0f, 0.0f, 100.0f};
	target.mid = {100.0f, 8.0f, 100.0f};
	target.radius = 40.0f;
	UnitNow shooter;
	shooter.pos = {200.0f, 0.0f, 100.0f};
	r.world.units[3] = target;
	r.world.units[4] = shooter;
	const int gap = Rate::Steps(0.2f);
	KT1_CHECK_EQ(gap, testCase.StepsPerSecond() == 30 ? 6 : 2);
	OnUnitDamaged(r.Env(50), {3, 4, 1, 5.0f});
	OnUnitDamaged(r.Env(50 + gap - 1), {3, 4, 1, 5.0f});
	KT1_CHECK_EQ(r.out.spawns.size(), size_t(1));
	KT1_CHECK_EQ(r.out.spawns[0].effect, 21);            // the weapon's unit tier, not the radius'
	KT1_CHECK_EQ(r.out.spawns[0].pos.x, 100.0f + 1.0f * 40.0f * 0.8f);
	KT1_CHECK_EQ(r.out.spawns[0].pos.y, 8.0f);
	KT1_CHECK_EQ(r.out.spawns[0].dir.x, 1.0f);
	KT1_CHECK_EQ(r.out.spawns[0].dir.y, 0.3f);
	r.defs.weapons[1].unitHitTier = -1;
	OnUnitDamaged(r.Env(50 + gap), {3, -1, 1, 5.0f});
	KT1_CHECK_EQ(r.out.spawns[1].effect, 22);            // radius 40: tier L
	KT1_CHECK_EQ(r.out.spawns[1].dir.x, 0.0f);
	OnUnitDamaged(r.Env(200), {3, -1, 1, 0.0f});         // no damage
	OnUnitDamaged(r.Env(200), {3, -1, 2, 5.0f});         // no confirmation
	KT1_CHECK_EQ(r.out.spawns.size(), size_t(2));
}

KT1_RATE_CASE("FXEV-SHOTS", "a projectile's start, depth launch and end, and the counter-shot") {
	Rig r;
	r.AllRows();
	r.defs.weapons.resize(8);
	r.defs.weapons[1].depthLaunch = true;
	r.Init();
	const ProjectileIn p = {42, 1, 3, {1.0f, 2.0f, 3.0f}, {4.0f, 5.0f, 6.0f}};
	OnProjectileCreated(r.Env(1), p);
	OnProjectileDestroyed(r.Env(2), p);
	OnProjectileCreated(r.Env(2), {43, -1, 3, {}, {}});
	KT1_CHECK((r.Types() == std::vector<int>{Kind<ShotStart>(), Kind<DepthLaunch>(), Kind<ShotEnd>()}));
	KT1_CHECK_EQ(r.Get<ShotStart>().vel.z, 6.0f);
	KT1_CHECK_EQ(r.Get<DepthLaunch>().projectile, 42);
	r.out.Clear();
	const Vec3 from = {7.0f, 8.0f, 9.0f};
	KT1_CHECK(Intercept(r.Env(3), {1.0f, 1.0f, 1.0f}, &from));
	KT1_CHECK(Intercept(r.Env(3), {2.0f, 2.0f, 2.0f}, nullptr));
	KT1_CHECK_EQ(r.Get<DepthIntercept>(0).hasFrom, 1);
	KT1_CHECK_EQ(r.Get<DepthIntercept>(1).hasFrom, 0);
	KT1_CHECK_EQ(r.state.intercepts, 2u);
}

KT1_RATE_CASE("FXEV-RELAY", "the ability relays fill what they must and share the step's limit") {
	Rig r;
	r.config.relay.ability_events_a_step = 4;
	r.Init();
	UnitNow site;
	site.pos = {50.0f, 1.0f, 60.0f};
	site.radius = 20.0f;
	site.height = 30.0f;
	site.defId = 6;
	site.allyTeam = 1;
	r.world.units[9] = site;
	Teleport t;
	t.kind = TeleportKind::Jump;
	t.unit = 9;
	t.value = 64.0f;
	KT1_CHECK(RelayTeleport(r.Env(1), t));
	KT1_CHECK(RelayCrash(r.Env(1), 9));
	KT1_CHECK(RelayRemnant(r.Env(1), 9, {}));
	Sacrifice s;
	s.site = 9;
	KT1_CHECK(RelaySacrifice(r.Env(1), s));
	KT1_CHECK(!RelayCrash(r.Env(1), 9));
	KT1_CHECK(RelayCrash(r.Env(2), 9));
	KT1_CHECK_EQ(r.Get<Teleport>().from.y, 10.0f);
	KT1_CHECK_EQ(r.Get<Teleport>().allyTeam, 1);
	KT1_CHECK_EQ(r.Get<Teleport>().seenTo, 5u);
	KT1_CHECK_EQ(r.Get<Crash>().defId, 6);
	KT1_CHECK_EQ(r.Get<Sacrifice>().siteRadius, 20.0f);
	KT1_CHECK_EQ(r.Get<Sacrifice>().sitePos.z, 60.0f);
}

KT1_RATE_CASE("FXEV-SCALE", "the effect option is sent once a second") {
	Rig r;
	r.Init(0.5f);
	const int sps = testCase.StepsPerSecond();
	for (int step = 1; step <= sps * 3; ++step) StartStep(r.Env(step));
	KT1_CHECK_EQ(r.Count<Scale>(), 3);
	KT1_CHECK_EQ(r.Get<Scale>().scale, 0.5f);
}
