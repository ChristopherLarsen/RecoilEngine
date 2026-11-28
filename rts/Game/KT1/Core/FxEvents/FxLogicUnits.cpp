/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 EFFECTS core: the synced effect logic, part 2: units, features and the step (the caller side of fx_death_sets,
// fx_build_sets and the synced halves of gfx_criteria_fx, gfx_fog_death_puff, fx_blasts' giant death and fx_core's hit
// confirmation). Order, float arithmetic and synced random draws are the Lua's. No engine include.
//
// Gadget order of one death (by layer): fx_core (-5), gfx_fog_death_puff (0), fx_build_sets and fx_death_sets (5),
// fx_blasts (10), gfx_criteria_fx (11). Of one GameFrame: "FX build sets", "FX death sets" (due sets, then damage
// states), "Criteria effects", "Wreck physics".

#include "Game/KT1/Core/FxEvents/FxLogic.h"

#include <algorithm>
#include <cmath>
#include <string_view>
#include <vector>

#include "Game/KT1/Core/Common/Rate.h"

namespace KT1::Core::Fx {

bool Mapped(const FxEventsConfig& config, std::string_view on, std::string_view display);   // FxLogic.cpp

namespace {

constexpr float HALF = 0.5f;                         // DESIGN: half, as the Lua's (math.random() - 0.5) spreads
constexpr float SPAN = 2;                            // DESIGN: a draw spread over -1 .. 1, as the Lua's math.random() * 2 - 1

float F(int32_t v) { return static_cast<float>(v); }
float Random(const FxEnv& env) { return env.world.random(env.world.ctx); }

const SynUnitDef* UnitDef(const FxEnv& env, int32_t defId) {
	if (defId < 0 || defId >= static_cast<int32_t>(env.defs.units.size())) return nullptr;
	return &env.defs.units[defId];
}

DeathKind KindOf(const FxEnv& env, int32_t defId) {
	const SynUnitDef* d = UnitDef(env, defId);
	return d != nullptr ? d->deathKind : DeathKind::Mobile;
}

void ForgetBuilding(FxState& s, int32_t unit) { s.slot.Apply(s.building.Remove(unit)); }

void ForgetDamaged(FxState& s, int32_t unit) {
	const DenseIndex::Move move = s.damaged.Remove(unit);
	for (Column<int32_t>& next : s.stateNext) next.Apply(move);
}

// ---------------------------------------------------------------- a hit

// fx_core: a short flash on the unit that was hit, at its surface towards the attacker
void ConfirmHit(const FxEnv& env, const UnitHitIn& in) {
	if (in.weaponDef < 0 || in.weaponDef >= static_cast<int32_t>(env.defs.weapons.size()) || !(in.damage > 0)) return;
	const SynWeapon& w = env.defs.weapons[in.weaponDef];
	const bool confirms = std::any_of(std::begin(w.confirm), std::end(w.confirm),
		[](const EffectRef& r) { return r.effect != NO_EFFECT || r.pooled; });
	if (!confirms || in.unit < 0 || in.unit >= static_cast<int32_t>(env.state.lastConfirm.size())) return;
	int32_t& last = env.state.lastConfirm[in.unit];
	if (last >= 0 && last + Rate::Steps(env.config.core.confirm_gap_s) > env.step) return;
	last = env.step;
	UnitNow hit;
	if (!env.world.unit(env.world.ctx, in.unit, hit)) return;
	const float r = hit.radius;
	float dx = 0, dz = 0;
	UnitNow attacker;
	if (in.attacker >= 0 && env.world.unit(env.world.ctx, in.attacker, attacker)) {
		const float ax = attacker.pos.x - hit.mid.x;
		const float az = attacker.pos.z - hit.mid.z;
		const float l = std::sqrt(ax * ax + az * az);
		if (l > 1) {
			dx = ax / l;
			dz = az / l;
		}
	}
	int32_t tier = w.unitHitTier;
	if (tier < 0) {
		const float* limit = env.config.core.confirm_radius;
		tier = static_cast<int32_t>(Tier::Xl);
		for (int32_t t = static_cast<int32_t>(Tier::L); t >= 0; --t)
			if (r <= limit[t]) tier = t;
	}
	tier = std::min(tier, static_cast<int32_t>(Tier::Xl));
	const float share = env.config.core.confirm_surface;
	const Vec3 pos = {hit.mid.x + dx * r * share, hit.mid.y, hit.mid.z + dz * r * share};
	Spawn(env, w.confirm[tier], pos, {dx, env.config.core.confirm_dir_y, dz}, Priority::Hit);
}

// fx_death_sets R11: a hit that leaves a unit at or below the highest threshold starts tracking it
void TrackDamage(const FxEnv& env, int32_t unit) {
	if (!env.config.death.enabled || env.state.damaged.Has(unit)) return;
	UnitNow now;
	if (!env.world.unit(env.world.ctx, unit, now) || !(now.maxHealth > 0)) return;
	if (now.health / now.maxHealth <= env.defs.death.topFraction) env.state.damaged.Add(unit);
}

// ---------------------------------------------------------------- a death

void RaiseDeath(const FxEnv& env, const UnitGoneIn& in) {
	const UnitNow& now = in.now;
	if (!Mapped(env.config, "UnitDestroyed", "Death")) return;
	Death e;
	e.unit = in.unit;
	e.defId = now.defId;
	e.team = now.team;
	e.allyTeam = now.allyTeam;
	e.pos = now.pos;
	e.radius = now.radius;
	e.height = now.height;
	e.finished = now.beingBuilt ? 0 : 1;
	e.seen = env.world.seen(env.world.ctx, now.pos);
	env.out.Raise(e);
}

void RaiseFogDeath(const FxEnv& env, const UnitNow& now) {
	if (now.build < 1 || !Mapped(env.config, "UnitDestroyed", "FogDeath")) return;   // an unfinished site is removed
	FogDeath e;
	e.pos = now.pos;
	e.radius = now.radius;
	e.height = now.height;
	e.allyTeam = now.allyTeam;
	e.seen = env.world.seen(env.world.ctx, now.pos);
	env.out.Raise(e);
}

void Later(const FxEnv& env, int32_t at, int32_t set, const UnitGoneIn& in, const Vec3& pos) {
	env.state.pending.push_back({at, set, in.unit, in.now.defId, pos});
}

// fx_death_sets: the wait (one draw), a structure's timed hits (four draws each), then the base and extra sets
void QueueDeathSets(const FxEnv& env, const UnitGoneIn& in) {
	const DeathConfig& c = env.config.death;
	if (!c.enabled) return;
	const Vec3& p = in.now.pos;
	const float mu = c.map_unit;
	const float sx = in.now.box[0] / mu, sy = in.now.box[1] / mu, sz = in.now.box[2] / mu;
	const DeathLayer layer = LayerOf(c, p.y, env.world.groundHeight(env.world.ctx, p.x, p.z));
	const float legacy = std::max(sx, sz);
	const DeathParts parts = PartsOfDeath(c, env.defs.death, KindOf(env, in.now.defId), layer, sx, sz, legacy,
		env.world.randomRange, env.world.ctx);
	const int32_t tick = Rate::Every(c.tick_s);
	int32_t at = env.step + parts.waitTicks * tick;
	if (parts.timed) {
		const int32_t lo = static_cast<int32_t>(c.timed_wait_ticks[0]);
		const int32_t hi = static_cast<int32_t>(c.timed_wait_ticks[1]);
		const int32_t n = std::max(1, static_cast<int32_t>(std::floor(legacy * c.timed_count_per_legacy[1] + HALF)));
		const float off = c.timed_offset_box_fraction * HALF;
		for (int32_t i = 0; i < n; ++i) {
			const float x = p.x + (Random(env) * SPAN - 1) * off * sx * mu;
			const float y = p.y + Random(env) * sy * mu;
			const float z = p.z + (Random(env) * SPAN - 1) * off * sz * mu;
			Later(env, at, env.defs.death.timedSet, in, {x, y, z});
			at += env.world.randomRange(env.world.ctx, lo, hi) * tick;
		}
		at += c.timed_pause_ticks * tick;
	}
	const Vec3 middle = {p.x, p.y + sy * mu * HALF, p.z};
	if (parts.base >= 0) Later(env, at, parts.base, in, middle);
	if (parts.extra >= 0) Later(env, at, parts.extra, in, middle);
}

void RingMark(const FxEnv& env, const Vec3& pos) {
	Scorch e;
	e.weaponDef = env.defs.giantScorchWeapon;
	e.pos = pos;
	e.source = ScorchSource::Death;
	env.out.Raise(e);
}

// fx_blasts: a finished experimental's death: the largest core, its blast and a ring of marks
void GiantDeath(const FxEnv& env, const UnitGoneIn& in) {
	const SynUnitDef* d = UnitDef(env, in.now.defId);
	if (d == nullptr || !d->giant || in.now.beingBuilt) return;
	const Vec3& p = in.now.pos;
	const Vec3 high = {p.x, p.y + env.config.blast.giant_lift, p.z};
	Spawn(env, env.defs.giantCore, high, {}, Priority::Death);
	if (Mapped(env.config, "UnitDestroyed", "Blast"))
		ForwardBlast(env, BlastClass::ExperimentalDeath, BlastFamily::Cannon, Faction::Human, high, -1);
	if (env.defs.giantScorchWeapon < 0 || !Mapped(env.config, "UnitDestroyed", "Scorch")) return;
	const float r = env.config.blast.giant_ring;
	RingMark(env, p);
	RingMark(env, {p.x + r, p.y, p.z});
	RingMark(env, {p.x, p.y, p.z + r});
	RingMark(env, {p.x - r, p.y, p.z});
	RingMark(env, {p.x, p.y, p.z - r});
}

// gfx_criteria_fx: the death for the criteria effects; a land death's wreck waits for the main blast
void CriteriaDeath(const FxEnv& env, const UnitGoneIn& in) {
	if (!env.config.core.criteria_owns_hits || in.now.beingBuilt) return;   // crit_fx_off: the gadget is off
	const CriteriaTimingConfig& c = env.config.criteria;
	const Vec3& p = in.now.pos;
	const DeathMedium medium = MediumOfDeath(c, p.y, env.world.groundHeight(env.world.ctx, p.x, p.z));
	if (Mapped(env.config, "UnitDestroyed", "CritDeath")) {
		CritDeath e;
		e.defId = in.now.defId;
		e.unit = in.unit;
		e.team = std::max(0, in.now.team);
		e.pos = p;
		e.medium = medium;
		e.heading = in.now.heading;
		e.seen = env.world.seen(env.world.ctx, p);
		env.out.Raise(e);
	}
	const SynUnitDef* d = UnitDef(env, in.now.defId);
	if (d == nullptr || medium != DeathMedium::Land) return;
	CriteriaRng rng(in.unit);
	const float mainAt = MainBlastAfterS(c, d->footprintRadius, medium, d->structure, rng);
	HideWait w;
	w.x = p.x;
	w.z = p.z;
	w.reach = std::max(c.hide_reach_min, d->footprintRadius * c.map_unit * c.hide_reach_per_radius);
	w.show = env.step + static_cast<int32_t>(std::ceil(mainAt * F(Rate::StepsPerSecond()))) + Rate::Steps(c.hide_after_s);
	w.ends = env.step + Rate::Steps(c.hide_wait_s);
	env.state.hideWaits.push_back(w);
}

// ---------------------------------------------------------------- the step

float PoolScale(float start, float progress) {
	if (progress <= start) return 1;
	const float k = (progress - start) / (1 - start);
	return 1 - k * k;
}

// one builder slot's sets at a unit under construction; three draws per set, refused by the budget or not
void BuildAt(const FxEnv& env, int32_t unit, int32_t slot) {
	const BuildSlot& s = env.defs.buildSlots[slot];
	if (env.step % std::max(1, s.period) != 0) return;
	UnitNow now;
	if (!env.world.unit(env.world.ctx, unit, now) || !(now.build < 1)) return;
	const float h = now.height;
	for (const EffectRef& effect : s.effects) {
		if (s.pool && PoolScale(env.config.build.pool_shrink_progress, now.build) < 1) continue;
		const float x = now.pos.x + (Random(env) - HALF) * h;
		const float y = now.pos.y + Random(env) * h * now.build;
		const float z = now.pos.z + (Random(env) - HALF) * h;
		if (!Spawn(env, effect, {x, y, z}, {}, Priority::Death)) continue;
		BuildSet e;
		e.unit = unit;
		e.defId = now.defId;
		e.slot = slot;
		e.effect = effect.effect;
		e.pos = {x, y, z};
		env.out.Raise(e);
	}
}

void BuildSets(const FxEnv& env) {
	if (!env.config.build.enabled) return;
	for (const int32_t unit : env.state.building.AscendingIds())   // no add or remove inside
		BuildAt(env, unit, env.state.slot[env.state.building.SlotOf(unit)]);
}

// a death or damage-state set at a place; the DeathSet event says what was spawned
void SpawnSet(const FxEnv& env, int32_t set, int32_t unit, int32_t defId, const Vec3& pos, Priority priority) {
	if (set < 0 || set >= static_cast<int32_t>(env.defs.deathSets.size())) return;
	const EffectRef& effect = env.defs.deathSets[set];
	if (!Spawn(env, effect, pos, {}, priority)) return;
	DeathSet e;
	e.unit = unit;
	e.defId = defId;
	e.effect = effect.effect;
	e.pos = pos;
	e.priority = priority;
	env.out.Raise(e);
}

// due entries spawn in the order made; the rest close up
void DueSets(const FxEnv& env) {
	std::vector<PendingSet>& pending = env.state.pending;
	size_t kept = 0;
	for (size_t i = 0; i < pending.size(); ++i) {
		const PendingSet p = pending[i];
		if (env.step >= p.at) SpawnSet(env, p.set, p.unit, p.defId, p.pos, Priority::Death);
		else pending[kept++] = p;
	}
	pending.resize(kept);
}

// one due state of a damaged unit: per emitter one alternative, each of its sets at a random place in the box
void StateEmitters(const FxEnv& env, int32_t unit, const UnitNow& now, const DamageState& state, int32_t emitters) {
	const float mu = env.config.death.map_unit;
	const float sx = now.box[0] / mu, sy = now.box[1] / mu, sz = now.box[2] / mu;
	const int32_t choices = static_cast<int32_t>(state.alternatives.size());
	for (int32_t i = 0; i < emitters && choices > 0; ++i) {
		const int32_t pick = env.world.randomRange(env.world.ctx, 1, choices) - 1;
		for (const int32_t set : state.alternatives[pick]) {
			const float x = now.pos.x + (Random(env) - HALF) * sx * mu;
			const float y = now.pos.y + Random(env) * sy * mu;
			const float z = now.pos.z + (Random(env) - HALF) * sz * mu;
			SpawnSet(env, set, unit, now.defId, {x, y, z}, Priority::State);
		}
	}
}

// false: the unit is no longer tracked
bool DamagedUnit(const FxEnv& env, int32_t unit) {
	UnitNow now;
	if (!env.world.unit(env.world.ctx, unit, now)) return false;
	const float share = now.maxHealth > 0 ? now.health / now.maxHealth : 1;
	if (share > env.defs.death.topFraction) return false;   // healed above every threshold
	if (now.build < 1) return true;
	const int32_t cls = DamageClassOf(KindOf(env, now.defId));
	const uint32_t active = ActiveStates(env.defs.death, cls, share);
	if (active == 0) return false;
	const float mu = env.config.death.map_unit;
	const int32_t emitters = DamageEmitters(env.config.death,
		(now.box[0] / mu) * (now.box[1] / mu) * (now.box[2] / mu));
	const std::vector<DamageState>& states = env.defs.death.states[cls];
	const int32_t slot = env.state.damaged.SlotOf(unit);
	for (int32_t s = 0; s < DEATH_STATES && s < static_cast<int32_t>(states.size()); ++s) {
		int32_t& next = env.state.stateNext[s][slot];
		if ((active & (1u << s)) == 0 || env.step < next) continue;
		next = env.step + Rate::Every(env.config.death.state_repeat_s);
		StateEmitters(env, unit, now, states[s], emitters);
	}
	return true;
}

void DamageStates(const FxEnv& env) {
	if (!env.config.death.enabled || env.step % Rate::Every(env.config.death.scan_s) != 0) return;
	if (env.state.damaged.Size() == 0) return;
	const auto ascending = env.state.damaged.AscendingIds();
	const std::vector<int32_t> ids(ascending.begin(), ascending.end());   // units leave the index inside the loop
	for (const int32_t unit : ids)
		if (!DamagedUnit(env, unit)) ForgetDamaged(env.state, unit);
}

void ShowWrecks(const FxEnv& env) {
	std::erase_if(env.state.hidden, [&](const HiddenFeature& h) {
		if (env.step < h.show) return false;
		env.out.showFeatures.push_back(h.feature);   // the caller skips a feature that is gone
		return true;
	});
	std::erase_if(env.state.hideWaits, [&](const HideWait& w) { return env.step > w.ends; });
}

} // namespace

void OnUnitDamaged(const FxEnv& env, const UnitHitIn& in) {
	ConfirmHit(env, in);
	TrackDamage(env, in.unit);
}

void OnUnitDestroyed(const FxEnv& env, const UnitGoneIn& in) {
	if (in.unit >= 0 && in.unit < static_cast<int32_t>(env.state.lastConfirm.size())) env.state.lastConfirm[in.unit] = -1;
	RaiseDeath(env, in);
	RaiseFogDeath(env, in.now);
	ForgetBuilding(env.state, in.unit);
	ForgetDamaged(env.state, in.unit);
	QueueDeathSets(env, in);
	GiantDeath(env, in);
	CriteriaDeath(env, in);
}

void OnUnitCreated(const FxEnv& env, int32_t unit, int32_t defId, float build) {
	if (!env.config.build.enabled) return;
	const SynUnitDef* d = UnitDef(env, defId);
	if (d == nullptr || d->buildSlot < 1 || d->buildSlot > BUILD_SLOTS || !(build < 1)) return;
	if (env.defs.buildSlots[d->buildSlot].effects.empty()) return;
	const DenseIndex::Slot slot = env.state.building.Add(unit);
	if (slot != DenseIndex::NONE) env.state.slot[slot] = d->buildSlot;
}

void OnUnitFinished(const FxEnv& env, int32_t unit) { ForgetBuilding(env.state, unit); }

// gfx_criteria_fx: the newest waiting death near a unit wreck hides it until its main blast
void OnFeatureCreated(const FxEnv& env, int32_t feature, int32_t defId, const Vec3& pos) {
	if (!env.config.core.criteria_owns_hits) return;
	if (defId < 0 || defId >= static_cast<int32_t>(env.defs.features.size()) || !env.defs.features[defId].unitWreck) return;
	std::vector<HideWait>& waits = env.state.hideWaits;
	for (size_t i = waits.size(); i > 0; --i) {
		const HideWait w = waits[i - 1];
		if (!(std::abs(w.x - pos.x) + std::abs(w.z - pos.z) < w.reach)) continue;
		waits.erase(waits.begin() + static_cast<std::ptrdiff_t>(i - 1));
		if (w.show > env.step) {
			env.out.hideFeatures.push_back(feature);
			std::erase_if(env.state.hidden, [&](const HiddenFeature& h) { return h.feature == feature; });
			env.state.hidden.push_back({feature, w.show});
		}
		return;
	}
}

void OnFeatureDestroyed(const FxEnv& env, int32_t feature) { ForgetWreck(env.state.wrecks, feature, env.out); }

void StepFx(const FxEnv& env) {
	BuildSets(env);
	DueSets(env);
	DamageStates(env);
	ShowWrecks(env);
	if (env.config.wreck.enabled)
		StepWrecks(env.state.wrecks, env.config.wreck, env.defs.wreck, env.step, env.world, env.out);
}

} // namespace KT1::Core::Fx
