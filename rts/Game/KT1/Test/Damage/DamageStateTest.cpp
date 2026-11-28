/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// Core/Damage/DamageState.h (step S21), engine-free: cmb_damage.lua and cmb_impact_patterns.lua as the Lua has them.
// Fixture values are invented (DESIGN), not the pack's.

#include "../KT1Test.h"

#include "Game/KT1/Core/Damage/DamageState.h"

#include <array>
#include <cmath>
#include <map>
#include <vector>

using namespace KT1::Core::Damage;

namespace {

constexpr int32_t RIFLE = 0, BOMB = 1, FIRE = 2, TWO = 3, TORP = 4, PLAIN = 5;   // weapon defs
constexpr int32_t TANK = 0, WALL = 1, ROCK = 2;                                  // unit defs: profiled, unprofiled, not damageable

struct FakeUnit { int32_t team = 0; float hp = 100, maxHp = 100; float pos[3] = {}; bool dead = false; };

struct FakeWorld : World {
	std::map<int32_t, FakeUnit> units;
	std::vector<int32_t> ally0 = {0, 1};          // teams 0 and 1 allied
	bool truce = false;
	struct Add { int32_t unit; float amount; int32_t attacker; int32_t wd; };
	std::vector<Add> adds;
	std::vector<DotTick> ticks;
	int hitFx = 0;
	std::map<int32_t, std::array<float, 3>> proPos;
	int32_t nextId = 100;
	std::vector<int32_t> spawned, deleted, burst;
	DamageState* state = nullptr;
	const DamageDefs* defs = nullptr;

	bool ValidUnit(int32_t u) override { return units.count(u) != 0; }
	bool DeadUnit(int32_t u) override { return units.count(u) && units[u].dead; }
	int32_t UnitTeam(int32_t u) override { return units.count(u) ? units[u].team : NONE; }
	bool Allied(int32_t a, int32_t b) override {
		auto in = [&](int32_t t) { for (int32_t x : ally0) if (x == t) return true; return false; };
		return a == b || (in(a) && in(b));
	}
	bool Truce(int32_t, int32_t) override { return truce; }
	bool Health(int32_t u, float& h, float& m) override { if (!units.count(u)) return false; h = units[u].hp; m = units[u].maxHp; return true; }
	bool Position(int32_t u, float (&p)[3]) override {
		if (!units.count(u)) return false;
		for (int i = 0; i < 3; ++i) p[i] = units[u].pos[i];
		return true;
	}
	void UnitsInSphere(const float (&p)[3], float r, std::vector<int32_t>& out) override {
		for (auto& [id, u] : units) {
			const float dx = u.pos[0] - p[0], dy = u.pos[1] - p[1], dz = u.pos[2] - p[2];
			if (dx * dx + dy * dy + dz * dz <= r * r) out.push_back(id);
		}
	}
	void UnitsInCylinder(float x, float z, float r, std::vector<int32_t>& out) override {
		for (auto& [id, u] : units) { const float dx = u.pos[0] - x, dz = u.pos[2] - z; if (dx * dx + dz * dz <= r * r) out.push_back(id); }
	}
	void AddDamage(int32_t u, float amount, int32_t attacker, int32_t wd) override { adds.push_back({u, amount, attacker, wd}); }
	void ClearBlocking(int32_t) override {}
	void Show(const DotTick& t) override { ticks.push_back(t); }
	void HitEffect(int32_t, const float (&)[3]) override { hitFx += 1; }
	bool ProjectilePosition(int32_t p, float (&out)[3]) override {
		auto it = proPos.find(p);
		if (it == proPos.end()) return false;
		for (int i = 0; i < 3; ++i) out[i] = it->second[i];
		return true;
	}
	void ProjectileVelocity(int32_t, float (&v)[3]) override { v[0] = 1; v[1] = -1; v[2] = 0; }
	ProjectileAim Aim(int32_t) override { ProjectileAim a; a.present = true; a.id = 7; return a; }
	int32_t SpawnChild(int32_t wd, const float (&)[3], const float (&)[3], int32_t owner, int32_t) override {
		const int32_t id = nextId++;
		spawned.push_back(id);
		if (state && defs) ProjectileCreated(*state, *defs, id, owner, wd);   // the engine calls ProjectileCreated inside the spawn
		return id;
	}
	void SetAim(int32_t, const ProjectileAim&) override {}
	bool SetWeave(int32_t, float, int32_t) override { return true; }
	void DeleteProjectile(int32_t p) override { deleted.push_back(p); }
	void BurstProjectile(int32_t p) override { burst.push_back(p); }
};

DamageDefs Defs() {
	DamageDefs d;
	d.weapons.resize(6);
	WeaponRules& rifle = d.weapons[RIFLE];
	rifle.rules = true; rifle.damage = 10; rifle.initial = 5;
	WeaponRules& bomb = d.weapons[BOMB];
	bomb.rules = true; bomb.damage = 40; bomb.initial = 10; bomb.aoe = 48; bomb.radius = 24; bomb.splashRadiusMu = 6;
	WeaponRules& fire = d.weapons[FIRE];
	fire.rules = true; fire.damage = 8; fire.patternDamage = 8; fire.pattern = Pattern::Incendiary; fire.patternDot = true;
	fire.dotTime = 2; fire.dotPulses = 5; fire.radius = 16;
	WeaponRules& two = d.weapons[TWO];
	two.pattern = Pattern::TwoStage; two.patternDamage = 100; two.radius = 20; two.hasHitEffect = true;
	WeaponRules& torp = d.weapons[TORP];
	torp.split = true; torp.splitCount = 3; torp.selfDestructS = 1;
	WeaponRules& plain = d.weapons[PLAIN];
	plain.rules = true; plain.damage = 6; plain.dotTime = 1; plain.dotPulses = 3;
	d.units.resize(3);
	d.units[TANK].profiled = true;
	d.units[ROCK].damageable = false;
	return d;
}

DamageParams Params(int sps) {
	DamageConfig c;
	c.death_threshold = 2;
	c.two_stage_first_fraction = 0.25f; c.two_stage_second_fraction = 0.75f; c.two_stage_second_delay_s = 1.5f;
	return MakeParams(c, sps);
}

Hit H(int32_t unit, int32_t def, int32_t team, float dmg, int32_t wd, int32_t pro, int32_t attacker) {
	Hit h;
	h.unit = unit; h.unitDef = def; h.unitTeam = team; h.damage = dmg; h.weaponDef = wd; h.projectile = pro; h.attacker = attacker;
	return h;
}

// the chain as the handler runs it for these two layers: cmb_damage, then cmb_impact_patterns on its answer
HitResult Chain(DamageState& s, const DamageDefs& d, const DamageParams& p, const Hit& h, int32_t step, World& w) {
	return PatternRules(s, d, p, h, DamageRules(s, d, p, h, step, w));
}

} // namespace

KT1_CASE("DMG-CMB-008-R1", "a unit that is not damageable takes nothing") {
	DamageState s; s.Init(64); const DamageDefs d = Defs(); FakeWorld w;
	w.units[1] = {};
	const HitResult r = Chain(s, d, Params(30), H(1, ROCK, 0, 50, RIFLE, 9, NONE), 0, w);
	KT1_CHECK_EQ(r.damage, 0.0f);
	KT1_CHECK(r.rules & RULE_UNDAMAGEABLE);
}

KT1_CASE("DMG-TRUCE", "during the opening truce a hit with an attacker does nothing") {
	DamageState s; s.Init(64); const DamageDefs d = Defs(); FakeWorld w;
	w.units[1] = {}; w.units[2] = {}; w.units[2].team = 3; w.truce = true;
	KT1_CHECK_EQ(Chain(s, d, Params(30), H(1, TANK, 0, 50, RIFLE, 9, 2), 0, w).damage, 0.0f);
	KT1_CHECK_EQ(Chain(s, d, Params(30), H(1, TANK, 0, 50, RIFLE, 9, NONE), 0, w).damage, 50.0f + 5.0f);   // no attacker: no truce test
}

KT1_CASE("DMG-CMB-006-R2", "an allied attacker's hit is refused unless the weapon has friendly fire; the firer itself is not refused") {
	DamageState s; s.Init(64); DamageDefs d = Defs(); FakeWorld w;
	w.units[1] = {}; w.units[2] = {}; w.units[2].team = 1;
	KT1_CHECK(Chain(s, d, Params(30), H(1, TANK, 0, 50, RIFLE, 9, 2), 0, w).rules & RULE_FRIENDLY);
	KT1_CHECK(!(Chain(s, d, Params(30), H(2, TANK, 1, 50, RIFLE, 10, 2), 0, w).rules & RULE_FRIENDLY));   // attacker == unit
	d.weapons[RIFLE].friendlyFire = true;
	KT1_CHECK(Chain(s, d, Params(30), H(1, TANK, 0, 50, RIFLE, 11, 2), 0, w).damage > 0);
}

KT1_CASE("DMG-CMB-006-R5", "a point shot adds its initial damage on the first hit only; an area shot once per unit") {
	DamageState s; s.Init(64); const DamageDefs d = Defs(); FakeWorld w;
	w.units[1] = {}; w.units[2] = {};
	w.units[1].hp = w.units[2].hp = 1000;
	KT1_CHECK_EQ(Chain(s, d, Params(30), H(1, TANK, 0, 10, RIFLE, 9, NONE), 0, w).damage, 15.0f);
	KT1_CHECK_EQ(Chain(s, d, Params(30), H(1, TANK, 0, 10, RIFLE, 9, NONE), 0, w).damage, 10.0f);
	ProjectileDestroyed(s, 9);                                                  // forgotten with its projectile
	KT1_CHECK_EQ(Chain(s, d, Params(30), H(1, TANK, 0, 10, RIFLE, 9, NONE), 0, w).damage, 15.0f);
	s.inSplash = true;                                                          // the bomb's own area hit: damage + initial already
	KT1_CHECK_EQ(Chain(s, d, Params(30), H(1, TANK, 0, 50, BOMB, 20, NONE), 0, w).damage, 50.0f);
	KT1_CHECK_EQ(Chain(s, d, Params(30), H(2, TANK, 0, 50, BOMB, 20, NONE), 0, w).damage, 50.0f);
	s.inSplash = false;
}

KT1_CASE("DMG-SPLASH-DROP", "a window bomb's engine hit is dropped; its own area hit gives the full hit to units within the radius, edge inside, not the attacker") {
	DamageState s; s.Init(64); const DamageDefs d = Defs(); FakeWorld w;
	const DamageParams p = Params(30);
	KT1_CHECK(Chain(s, d, p, H(1, TANK, 0, 40, BOMB, 20, NONE), 0, w).rules & RULE_SPLASH_DROP);
	w.units[1] = {}; w.units[1].pos[0] = 48;                                    // 6 map units: on the edge
	w.units[2] = {}; w.units[2].pos[0] = 48.5f;                                 // just outside
	w.units[3] = {}; w.units[3].pos[2] = -8;                                    // inside, the attacker
	w.units[4] = {};                                                            // at the centre
	const float c[3] = {0, 0, 0};
	KT1_CHECK_EQ(SplashExplosion(s, d, p, BOMB, c, 3, w), 2);
	KT1_CHECK_EQ(static_cast<int>(w.adds.size()), 2);
	KT1_CHECK_EQ(w.adds[0].amount, 50.0f);
	KT1_CHECK(!s.inSplash);
}

KT1_RATE_CASE("DMG-CMB-006-R6", "damage over time: pulses - 1 more hits spaced over the time; a single target stops when gone") {
	const int sps = testCase.StepsPerSecond();
	DamageState s; s.Init(64); const DamageDefs d = Defs(); FakeWorld w;
	const DamageParams p = Params(sps);
	w.units[1] = {}; w.units[1].hp = 1000;
	Chain(s, d, p, H(1, TANK, 0, 6, PLAIN, 9, NONE), 0, w);
	KT1_CHECK_EQ(static_cast<int>(s.dots.size()), 2);
	KT1_CHECK_EQ(s.dots[0].atStep, static_cast<int32_t>(std::floor(0.5f * sps + 0.5f)));   // gap 1 s / 2
	KT1_CHECK_EQ(s.dots[1].atStep, sps);
	RunDotPulses(s, d, sps / 2, w);
	KT1_CHECK_EQ(static_cast<int>(w.adds.size()), 1);
	w.units[1].dead = true;
	RunDotPulses(s, d, sps, w);
	KT1_CHECK_EQ(static_cast<int>(w.adds.size()), 1);                           // gone: no more damage
	KT1_CHECK_EQ(static_cast<int>(w.ticks.size()), 2);                          // the look still ticks
	KT1_CHECK(s.dots.empty());
}

KT1_CASE("DMG-CMB-008-R2", "a hit that leaves less than the threshold kills: at least the health is dealt and the kill is recorded") {
	DamageState s; s.Init(64); const DamageDefs d = Defs(); FakeWorld w;
	w.units[1] = {}; w.units[1].hp = 11;
	const HitResult r = Chain(s, d, Params(30), H(1, TANK, 0, 4, RIFLE, 9, NONE), 0, w);   // 4 + 5 initial: 11 - 9 = 2, not below
	KT1_CHECK(!(r.rules & RULE_KILL));
	w.units[1].hp = 10.5f;
	const HitResult k = Chain(s, d, Params(30), H(1, TANK, 0, 9, RIFLE, 10, NONE), 0, w);   // 14: kills
	KT1_CHECK(k.rules & RULE_KILL);
	KT1_CHECK_EQ(k.damage, 14.0f);
	const KillRecord& kr = s.lastKill[s.hurt.SlotOf(1)];
	KT1_CHECK(kr.set);
	KT1_CHECK_EQ(kr.before, 10.5f);
	w.units[2] = {}; w.units[2].hp = 1;
	KT1_CHECK(!(Chain(s, d, Params(30), H(2, WALL, 0, 3, NONE, -1, NONE), 0, w).rules & RULE_KILL));   // unprofiled, no rules weapon
}

KT1_CASE("DMG-CMB-022-R4", "two stage: the engine hit is the first fraction; the second fraction comes later in the same radius with its look") {
	DamageState s; s.Init(64); const DamageDefs d = Defs(); FakeWorld w;
	const DamageParams p = Params(30);
	w.units[1] = {}; w.units[1].hp = 1000;
	KT1_CHECK_EQ(Chain(s, d, p, H(1, WALL, 0, 100, TWO, 30, NONE), 0, w).damage, 25.0f);
	const float at[3] = {0, 0, 0};
	KT1_CHECK(PatternImpact(s, d, p, TWO, at, NONE, 10));
	KT1_CHECK_EQ(static_cast<int>(s.stages.size()), 1);
	KT1_CHECK_EQ(s.stages[0].atStep, 10 + 45);
	KT1_CHECK_EQ(s.stages[0].amount, 75.0f);
	RunPatternPulses(s, d, 55, w);
	KT1_CHECK_EQ(static_cast<int>(w.adds.size()), 1);
	KT1_CHECK_EQ(w.hitFx, 1);
	s.inStage = true;
	KT1_CHECK_EQ(Chain(s, d, p, H(1, WALL, 0, 75, TWO, -1, NONE), 0, w).damage, 75.0f);   // the second stage is whole
}

KT1_RATE_CASE("DMG-CMB-022-R3", "incendiary: the pattern's pulses at the impact point, to whoever is in the radius then; cmb_damage adds none") {
	const int sps = testCase.StepsPerSecond();
	DamageState s; s.Init(64); const DamageDefs d = Defs(); FakeWorld w;
	const DamageParams p = Params(sps);
	w.units[1] = {}; w.units[1].hp = 1000;
	Chain(s, d, p, H(1, TANK, 0, 8, FIRE, 40, NONE), 0, w);
	KT1_CHECK(s.dots.empty());
	const float at[3] = {0, 0, 0};
	KT1_CHECK(PatternImpact(s, d, p, FIRE, at, NONE, 0));
	KT1_CHECK_EQ(static_cast<int>(s.stages.size()), 4);
	KT1_CHECK_EQ(s.stages[3].atStep, 2 * sps);
	w.units[1].pos[0] = 30;                                                     // left the radius
	w.units[2] = {};                                                            // entered
	RunPatternPulses(s, d, 2 * sps, w);
	KT1_CHECK_EQ(static_cast<int>(w.adds.size()), 4);
	for (const auto& a : w.adds) KT1_CHECK_EQ(a.unit, 2);
	KT1_CHECK_EQ(static_cast<int>(w.ticks.size()), 4);
}

KT1_RATE_CASE("DMG-CMB-022-R5", "a torpedo under the surface becomes n children, each 1 / n of a hit; they burst on time") {
	const int sps = testCase.StepsPerSecond();
	DamageState s; s.Init(64); const DamageDefs d = Defs(); FakeWorld w;
	w.state = &s; w.defs = &d;
	const DamageParams p = Params(sps);
	ProjectileCreated(s, d, 50, 1, TORP);
	w.proPos[50][0] = 0; w.proPos[50][1] = 5; w.proPos[50][2] = 0;
	RunSplits(s, d, p, 0, w);
	KT1_CHECK(w.spawned.empty());                                               // above the water
	w.proPos[50][1] = -1;
	RunSplits(s, d, p, 3, w);
	KT1_CHECK_EQ(static_cast<int>(w.spawned.size()), 3);
	KT1_CHECK(s.parents.empty());                                               // a child never splits
	KT1_CHECK_EQ(static_cast<int>(s.children.size()), 3);
	KT1_CHECK_EQ(s.splits, 1);
	w.units[1] = {}; w.units[1].hp = 1000;
	KT1_CHECK_EQ(Chain(s, d, p, H(1, WALL, 0, 30, TORP, w.spawned[0], NONE), 0, w).damage, 10.0f);
	RunSplits(s, d, p, 3 + sps - 1, w);
	KT1_CHECK(w.burst.empty());
	RunSplits(s, d, p, 3 + sps, w);
	KT1_CHECK_EQ(static_cast<int>(w.burst.size()), 3);
}

KT1_CASE("DMG-WEAVE", "the decaying weave: start, then the steps, then the end values") {
	DamageConfig c;
	c.weave_hold_s = 1; c.weave_start_amplitude_mu = 4; c.weave_step_s = 0.5f; c.weave_end_amplitude_mu = 0.5f; c.weave_end_frequency = 2;
	c.weave_steps = {{3}, {2}};
	const DamageParams p = MakeParams(c, 30);
	WeaponRules wr; wr.splitDecaying = true;
	float a = 0, f = 0;
	SplitWeave(p, wr, 0.5f, a, f); KT1_CHECK_EQ(a, 4.0f);
	SplitWeave(p, wr, 1.2f, a, f); KT1_CHECK_EQ(a, 3.0f);
	SplitWeave(p, wr, 1.6f, a, f); KT1_CHECK_EQ(a, 2.0f);
	SplitWeave(p, wr, 2.1f, a, f); KT1_CHECK_EQ(a, 0.5f); KT1_CHECK_EQ(f, 2.0f);
	wr.splitDecaying = false; wr.childWeaveMu = 1.5f;
	SplitWeave(p, wr, 9, a, f); KT1_CHECK_EQ(a, 1.5f);
}

KT1_CASE("DMG-CMB-007-R2", "armour: a listed class and type multiply; an unlisted type or class counts 1") {
	DamageConfig c;
	c.default_armour_class = "light";
	c.armour = {{"light", {{"kinetic", 1.5f}, {"fire", 0.5f}}}, {"heavy", {{"kinetic", 0.25f}}}};
	ArmourTable t; t.Build(c);
	KT1_CHECK_EQ(t.Mult(t.ClassIndex("heavy"), t.TypeIndex("kinetic")), 0.25f);
	KT1_CHECK_EQ(t.Mult(t.ClassIndex("heavy"), t.TypeIndex("fire")), 1.0f);
	KT1_CHECK_EQ(t.Mult(t.ClassIndex("nobody"), t.TypeIndex("")), 1.5f);       // the default class and type
	KT1_CHECK_EQ(t.TypeIndex("plasma"), NONE);
}
