/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 Damage core (step S21): cmb_damage.lua and cmb_impact_patterns.lua as the Lua has them, on DamageState.h's interface.
// No engine include. Requirements: game-build docs/engine/rules/combat-requirements.md; the chain order and the open
// corrections: docs/engine/rules/damage.md. Float maths in the Lua's order; no FMA contraction.

#pragma GCC optimize("fp-contract=off")

#include "Game/KT1/Core/Damage/DamageState.h"

#include <algorithm>
#include <cmath>

namespace KT1::Core::Damage {
namespace {

constexpr float ROUND = 0.5f;          // DESIGN: the Lua's math.floor(x + 0.5)
constexpr int32_t MIN_PULSES = 2;      // REQ CMB-006 R6: damage over time needs at least two pulses (the first is the hit)

// a pulse's step: step + floor(seconds * steps a second + 0.5), as the Lua's frame + math.floor(t * FPS + 0.5)
int32_t After(int32_t step, float seconds, int32_t stepsPerSecond) {
	return step + static_cast<int32_t>(std::floor(seconds * static_cast<float>(stepsPerSecond) + ROUND));
}

// the record of a projectile id in a list kept in ascending id; nullptr when absent
template <typename T>
T* Find(std::vector<T>& list, int32_t projectile) {
	auto it = std::lower_bound(list.begin(), list.end(), projectile, [](const T& r, int32_t p) { return r.projectile < p; });
	return (it != list.end() && it->projectile == projectile) ? &*it : nullptr;
}

template <typename T>
T& Insert(std::vector<T>& list, int32_t projectile) {
	auto it = std::lower_bound(list.begin(), list.end(), projectile, [](const T& r, int32_t p) { return r.projectile < p; });
	if (it != list.end() && it->projectile == projectile) return *it;
	it = list.insert(it, T{});
	it->projectile = projectile;
	return *it;
}

template <typename T>
void Erase(std::vector<T>& list, int32_t projectile) {
	auto it = std::lower_bound(list.begin(), list.end(), projectile, [](const T& r, int32_t p) { return r.projectile < p; });
	if (it != list.end() && it->projectile == projectile) list.erase(it);
}

DenseIndex::Slot SlotFor(DamageState& state, int32_t unit) {
	if (!state.hurt.InRange(unit)) return NONE;
	if (!state.hurt.Has(unit)) return state.hurt.Add(unit);
	return state.hurt.SlotOf(unit);
}

HitResult Zero(const Hit& hit, uint8_t rule) { return {0, 0, hit.paralyzer, rule}; }

} // namespace

DamageParams MakeParams(const DamageConfig& c, int32_t stepsPerSecond) {
	DamageParams p;
	p.deathThreshold = c.death_threshold;
	p.splashQueryPad = c.splash_query_pad;
	p.elmosPerMu = c.elmos_per_mu;
	p.stepsPerSecond = std::max<int32_t>(1, stepsPerSecond);
	p.twoStageFirst = c.two_stage_first_fraction;
	p.twoStageSecond = c.two_stage_second_fraction;
	p.twoStageDelayS = c.two_stage_second_delay_s;
	p.weaveHoldS = c.weave_hold_s;
	p.weaveStartMu = c.weave_start_amplitude_mu;
	p.weaveStepS = c.weave_step_s;
	p.weaveEndMu = c.weave_end_amplitude_mu;
	p.weaveEndFrequency = c.weave_end_frequency;
	p.weaveFrequencyPerAmplitude = c.weave_frequency_per_amplitude;
	p.weaveMinFrequency = c.weave_min_frequency;
	p.weaveStepsMu.clear();
	for (const WeaveStep& s : c.weave_steps) p.weaveStepsMu.push_back(s.amplitude_mu);
	return p;
}

// ---- armour (REQ CMB-007 v2) ----

void ArmourTable::Build(const DamageConfig& c) {
	classes.clear(); types.clear(); mults.clear();
	for (const ArmourClass& a : c.armour) {
		classes.push_back(a.name);
		for (const ArmourMult& m : a.mults)
			if (std::find(types.begin(), types.end(), m.type) == types.end()) types.push_back(m.type);
	}
	mults.assign(classes.size() * types.size(), 1.0f);                 // R3: a type not listed counts 1
	for (size_t i = 0; i < c.armour.size(); ++i)
		for (const ArmourMult& m : c.armour[i].mults) {
			const size_t t = std::find(types.begin(), types.end(), m.type) - types.begin();
			mults[i * types.size() + t] = m.mult;
		}
	auto at = [](const std::vector<std::string>& v, const std::string& s) {
		const auto it = std::find(v.begin(), v.end(), s);
		return it == v.end() ? NONE : static_cast<int32_t>(it - v.begin());
	};
	fallbackClass = at(classes, c.default_armour_class);
	fallbackType = at(types, c.default_damage_type);
}

int32_t ArmourTable::ClassIndex(std::string_view name) const {
	for (size_t i = 0; i < classes.size(); ++i) if (classes[i] == name) return static_cast<int32_t>(i);
	return fallbackClass;
}

int32_t ArmourTable::TypeIndex(std::string_view type) const {
	if (type.empty()) return fallbackType;
	for (size_t i = 0; i < types.size(); ++i) if (types[i] == type) return static_cast<int32_t>(i);
	return NONE;
}

float ArmourTable::Mult(int32_t classIndex, int32_t typeIndex) const {
	if (classIndex < 0 || typeIndex < 0 || static_cast<size_t>(classIndex) >= classes.size() || static_cast<size_t>(typeIndex) >= types.size())
		return 1.0f;                                                   // R2, R3: unlisted counts 1
	return mults[static_cast<size_t>(classIndex) * types.size() + static_cast<size_t>(typeIndex)];
}

// ---- state ----

void DamageState::Init(int32_t maxUnits) {
	hurt.Init(maxUnits);
	lastHit.Init(hurt);
	lastKill.Init(hurt);
	shots.clear(); dots.clear(); stages.clear(); parents.clear(); children.clear();
	inDot = inSplash = inStage = false;
	splits = 0;
}

// ---- UnitPreDamaged, cmb_damage (layer -10) ----

HitResult DamageRules(DamageState& state, const DamageDefs& defs, const DamageParams& params, const Hit& hit, int32_t step, World& world) {
	const UnitRules* ud = defs.Unit(hit.unitDef);
	if (ud == nullptr || !ud->damageable) return Zero(hit, RULE_UNDAMAGEABLE);   // CMB-008 R1 (an unknown def: the Lua's nil)
	if (hit.attacker != NONE && world.Truce(world.UnitTeam(hit.attacker), hit.unitTeam)) return Zero(hit, RULE_TRUCE);
	const WeaponRules* wr = defs.Weapon(hit.weaponDef);
	const WeaponRules* w = (wr != nullptr && wr->rules) ? wr : nullptr;
	if (w != nullptr && w->splashRadiusMu > 0 && !state.inSplash && !state.inDot) return Zero(hit, RULE_SPLASH_DROP);   // the area hit is the Explosion's
	float damage = hit.damage;
	uint8_t rules = RULE_NONE;
	if (w != nullptr) {
		if (hit.attacker != NONE && hit.attacker != hit.unit && world.ValidUnit(hit.attacker) && !w->friendlyFire
			&& world.Allied(world.UnitTeam(hit.attacker), hit.unitTeam))
			return Zero(hit, RULE_FRIENDLY);                                                  // CMB-006 R2
		if (w->initial > 0 && hit.projectile >= 0 && !state.inDot && !state.inSplash) {      // CMB-006 v3 R5
			ShotMemory& m = Insert(state.shots, hit.projectile);
			if (w->radius > 0) {                                                                // an area hit: every unit once
				if (std::find(m.hit.begin(), m.hit.end(), hit.unit) == m.hit.end()) {
					m.hit.push_back(hit.unit);
					damage = damage + w->initial;
					rules |= RULE_INITIAL;
				}
			} else if (!m.first) {
				m.first = true;
				damage = damage + w->initial;
				rules |= RULE_INITIAL;
			}
		}
		if (w->dotTime > 0 && w->dotPulses >= MIN_PULSES && !state.inDot && !w->patternDot && hit.projectile >= 0) {
			ShotMemory* seen = Find(state.shots, hit.projectile);
			if (!(w->radius > 0 && seen != nullptr && seen->dot)) {
				Insert(state.shots, hit.projectile).dot = true;                                // an area shot pulses once (a point shot each hit, as the Lua)
				const float gap = w->dotTime / static_cast<float>(w->dotPulses - 1);          // CMB-006 R6
				float pos[3] = {};
				world.Position(hit.unit, pos);
				for (int32_t i = 1; i <= w->dotPulses - 1; ++i) {
					DotPulse p;
					p.atStep = After(step, static_cast<float>(i) * gap, params.stepsPerSecond);
					p.unit = hit.unit; p.weaponDef = hit.weaponDef; p.attacker = hit.attacker; p.area = w->radius > 0;
					p.pos[0] = pos[0]; p.pos[1] = pos[1]; p.pos[2] = pos[2];
					p.amount = w->damage;
					state.dots.push_back(p);
				}
			}
		}
	}
	if (damage <= 0) return Zero(hit, static_cast<uint8_t>(rules | RULE_NO_DAMAGE));          // CMB-006 R4
	if (ud->profiled || w != nullptr) {
		float hp = 0, maxHp = 0;
		if (world.Health(hit.unit, hp, maxHp) && hp - damage < params.deathThreshold) {       // CMB-008 R2
			const DenseIndex::Slot s = SlotFor(state, hit.unit);
			if (s != NONE) state.lastKill[s] = KillRecord{true, damage, hp, maxHp};
			return {std::max(damage, hp), 0, hit.paralyzer, static_cast<uint8_t>(rules | RULE_KILL)};
		}
	}
	return {damage, 0, hit.paralyzer, rules};                                                 // CMB-006 R7: no push
}

// ---- UnitPreDamaged, cmb_impact_patterns (layer -9), on the chain's answer so far ----

HitResult PatternRules(DamageState& state, const DamageDefs& defs, const DamageParams& params, const Hit& hit, const HitResult& in) {
	HitResult r = in;
	r.impulse = 0;
	const SplitChild* c = hit.projectile >= 0 ? Find(state.children, hit.projectile) : nullptr;
	const WeaponRules* cw = c != nullptr ? defs.Weapon(c->weaponDef) : nullptr;
	const WeaponRules* w = defs.Weapon(hit.weaponDef);
	if (cw != nullptr && cw->splitCount > 0) {                                                    // CMB-022 R5: a child carries 1 / split_count
		r.damage = in.damage / static_cast<float>(cw->splitCount);
		r.rules |= RULE_PATTERN;
	} else if (w != nullptr && w->pattern == Pattern::TwoStage && !state.inStage && params.twoStageFirst >= 0) {
		r.damage = in.damage * params.twoStageFirst;                                             // R4: the engine's hit is the first stage
		r.rules |= RULE_PATTERN;
	}
	const DenseIndex::Slot s = SlotFor(state, hit.unit);
	if (s != NONE) state.lastHit[s] = r;
	return r;
}

// ---- Explosion ----

int32_t SplashExplosion(DamageState& state, const DamageDefs& defs, const DamageParams& params, int32_t weaponDef, const float (&pos)[3],
	int32_t attacker, World& world) {
	const WeaponRules* w = defs.Weapon(weaponDef);
	if (w == nullptr || !w->rules || !(w->splashRadiusMu > 0)) return 0;
	const float full = w->damage + w->initial;
	std::vector<int32_t> units;
	world.UnitsInCylinder(pos[0], pos[2], w->splashRadiusMu * params.elmosPerMu + params.splashQueryPad, units);
	int32_t hit = 0;
	for (const int32_t v : units) {
		float p[3] = {};
		if (!world.Position(v, p)) continue;
		const float dx = p[0] - pos[0], dz = p[2] - pos[2];
		const float d = std::sqrt(dx * dx + dz * dz) / params.elmosPerMu;
		if (d <= w->splashRadiusMu && v != attacker) {                                         // the edge counts as inside
			state.inSplash = true;
			world.AddDamage(v, full, attacker, weaponDef);
			state.inSplash = false;
			hit += 1;
		}
	}
	return hit;
}

bool PatternImpact(DamageState& state, const DamageDefs& defs, const DamageParams& params, int32_t weaponDef, const float (&pos)[3],
	int32_t attacker, int32_t step) {
	const WeaponRules* w = defs.Weapon(weaponDef);
	if (w == nullptr || (w->pattern != Pattern::Incendiary && w->pattern != Pattern::TwoStage)) return false;
	auto schedule = [&](int32_t at, float amount) {
		PatternPulse p;
		p.atStep = at; p.weaponDef = weaponDef; p.attacker = attacker;
		p.pos[0] = pos[0]; p.pos[1] = pos[1]; p.pos[2] = pos[2];
		p.radius = w->radius; p.amount = amount;
		state.stages.push_back(p);
	};
	if (w->pattern == Pattern::Incendiary && w->dotPulses >= MIN_PULSES && w->dotTime > 0) {            // R3
		const float gap = w->dotTime / static_cast<float>(w->dotPulses - 1);                    // CMB-006 R6 spacing
		for (int32_t i = 1; i <= w->dotPulses - 1; ++i)
			schedule(After(step, static_cast<float>(i) * gap, params.stepsPerSecond), w->patternDamage);
	} else if (w->pattern == Pattern::TwoStage && params.twoStageSecond >= 0 && params.twoStageDelayS >= 0) {   // R4
		schedule(After(step, params.twoStageDelayS, params.stepsPerSecond), w->patternDamage * params.twoStageSecond);
	}
	return true;
}

// ---- projectiles ----

void ProjectileCreated(DamageState& state, const DamageDefs& defs, int32_t projectile, int32_t owner, int32_t weaponDef) {
	const WeaponRules* w = defs.Weapon(weaponDef);
	if (w == nullptr || !w->split || Find(state.children, projectile) != nullptr) return;
	SplitParent& p = Insert(state.parents, projectile);
	p.weaponDef = weaponDef;
	p.owner = owner;
}

void ProjectileDestroyed(DamageState& state, int32_t projectile) {
	Erase(state.shots, projectile);
	Erase(state.parents, projectile);
	Erase(state.children, projectile);
}

// ---- after the move: cmb_damage's GameFrame, then cmb_impact_patterns' (splits, then pulses) ----

void RunDotPulses(DamageState& state, const DamageDefs& defs, int32_t step, World& world) {
	if (state.dots.empty()) return;
	std::vector<DotPulse> keep;
	std::vector<int32_t> units;
	const std::vector<DotPulse> due = state.dots;                     // AddDamage re-enters the chain; inDot keeps it from adding pulses
	for (const DotPulse& p : due) {
		if (step < p.atStep) { keep.push_back(p); continue; }
		const WeaponRules* w = defs.Weapon(p.weaponDef);
		state.inDot = true;
		if (p.area) {
			units.clear();
			if (w != nullptr) world.UnitsInSphere(p.pos, w->radius, units);
			for (const int32_t v : units)
				if (v != p.attacker || (w != nullptr && w->selfDamage))                  // CMB-006 R3: never the firing unit
					world.AddDamage(v, p.amount, p.attacker, p.weaponDef);
		} else if (world.ValidUnit(p.unit) && !world.DeadUnit(p.unit)) {            // a single target: stops if gone
			world.AddDamage(p.unit, p.amount, p.attacker, p.weaponDef);
		}
		state.inDot = false;
		DotTick t;
		t.unit = p.area ? NONE : p.unit;
		t.pos[0] = p.pos[0]; t.pos[1] = p.pos[1]; t.pos[2] = p.pos[2];
		t.radius = (p.area && w != nullptr) ? w->radius : 0;
		world.Show(t);                                                               // look only
	}
	state.dots = std::move(keep);
}

void RunSplits(DamageState& state, const DamageDefs& defs, const DamageParams& params, int32_t step, World& world) {
	if (state.parents.empty() && state.children.empty()) return;
	std::vector<int32_t> ids;
	for (const SplitParent& p : state.parents) ids.push_back(p.projectile);         // ascending, before any spawn
	for (const int32_t pro : ids) {
		SplitParent* pp = Find(state.parents, pro);
		if (pp == nullptr) continue;
		const SplitParent p = *pp;
		float pos[3] = {};
		if (!world.ProjectilePosition(pro, pos)) { Erase(state.parents, pro); continue; }
		if (!(pos[1] < 0)) continue;
		Erase(state.parents, pro);                                                   // R5: under the surface: it splits
		const WeaponRules* w = defs.Weapon(p.weaponDef);
		float vel[3] = {};
		world.ProjectileVelocity(pro, vel);
		const ProjectileAim aim = world.Aim(pro);
		const int32_t team = (p.owner != NONE && world.ValidUnit(p.owner)) ? world.UnitTeam(p.owner) : NONE;
		for (int32_t k = 0; w != nullptr && k < w->splitCount; ++k) {
			const int32_t cid = world.SpawnChild(p.weaponDef, pos, vel, p.owner, team);
			if (cid == NONE) continue;
			Erase(state.parents, cid);                                                // ProjectileCreated ran inside the spawn: a child never splits
			SplitChild& c = Insert(state.children, cid);
			c.weaponDef = p.weaponDef;
			c.bornStep = step;
			if (aim.present) world.SetAim(cid, aim);
		}
		world.DeleteProjectile(pro);
		state.splits += 1;
	}
	ids.clear();
	for (const SplitChild& c : state.children) ids.push_back(c.projectile);
	const float sps = static_cast<float>(params.stepsPerSecond);
	for (const int32_t cid : ids) {
		const SplitChild* cp = Find(state.children, cid);
		if (cp == nullptr) continue;
		const SplitChild c = *cp;
		const WeaponRules* w = defs.Weapon(c.weaponDef);
		if (w == nullptr) continue;
		const float age = static_cast<float>(step - c.bornStep) / sps;
		float amp = 0, freq = 0;
		SplitWeave(params, *w, age, amp, freq);
		const int32_t every = std::max<int32_t>(1, static_cast<int32_t>(std::floor(sps / std::max(freq, params.weaveMinFrequency) + ROUND)));
		world.SetWeave(cid, amp * params.elmosPerMu, every);
		if (w->selfDestructS >= 0 && age >= w->selfDestructS) {
			Erase(state.children, cid);
			world.BurstProjectile(cid);                                               // it bursts
		}
	}
}

void RunPatternPulses(DamageState& state, const DamageDefs& defs, int32_t step, World& world) {
	if (state.stages.empty()) return;
	std::vector<PatternPulse> keep;
	std::vector<int32_t> units;
	const std::vector<PatternPulse> due = state.stages;
	for (const PatternPulse& p : due) {
		if (step < p.atStep) { keep.push_back(p); continue; }
		state.inStage = true;
		units.clear();
		world.UnitsInCylinder(p.pos[0], p.pos[2], p.radius, units);
		for (const int32_t v : units) world.AddDamage(v, p.amount, p.attacker, p.weaponDef);
		state.inStage = false;
		const WeaponRules* w = defs.Weapon(p.weaponDef);
		if (w != nullptr && w->pattern == Pattern::Incendiary) {
			DotTick t;
			t.unit = NONE;
			t.pos[0] = p.pos[0]; t.pos[1] = p.pos[1]; t.pos[2] = p.pos[2];
			t.radius = p.radius;
			world.Show(t);
		}
		if (w != nullptr && w->pattern == Pattern::TwoStage && w->hasHitEffect) world.HitEffect(p.weaponDef, p.pos);   // the second stage shows
	}
	state.stages = std::move(keep);
}

// cmb_drop.lua D.SplitWeave
void SplitWeave(const DamageParams& params, const WeaponRules& weapon, float age, float& amplitudeMu, float& frequency) {
	if (!weapon.splitDecaying) { amplitudeMu = weapon.childWeaveMu; frequency = weapon.childWeaveFrequency; return; }
	if (age < params.weaveHoldS) {
		amplitudeMu = params.weaveStartMu;
		frequency = params.weaveFrequencyPerAmplitude * params.weaveStartMu;
		return;
	}
	if (params.weaveStepS > 0) {
		const int32_t i = static_cast<int32_t>(std::floor((age - params.weaveHoldS) / params.weaveStepS));   // the Lua's index, from 0
		if (i >= 0 && static_cast<size_t>(i) < params.weaveStepsMu.size()) {
			amplitudeMu = params.weaveStepsMu[i];
			frequency = params.weaveFrequencyPerAmplitude * amplitudeMu;
			return;
		}
	}
	amplitudeMu = params.weaveEndMu;
	frequency = params.weaveEndFrequency;
}

} // namespace KT1::Core::Damage
