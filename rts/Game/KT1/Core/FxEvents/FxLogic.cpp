/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 EFFECTS core: the synced effect logic, part 1: start, the budgeted spawn, explosions, projectiles and the relays
// (fx_core, fx_script_relay, fx_events and the synced halves of fx_blasts, gfx_scorch and fx_depth). Units, features and
// the step's end: FxLogicUnits.cpp. Order and float arithmetic are the Lua's. No engine include.
//
// Gadget order of one explosion (the Lua handler calls by layer): fx_core (-5), fx_blasts (10), fx_wreck_physics (11),
// gfx_scorch (100), fx_script_relay (120). The wreck push is the caller's (ExplodeNearWrecks needs the engine's feature
// sphere query, which WorldQuery does not have); it raises none of the events made here.

#include "Game/KT1/Core/FxEvents/FxLogic.h"

#include <algorithm>
#include <cmath>
#include <string_view>

#include "Game/KT1/Core/Common/DetMath.h"
#include "Game/KT1/Core/Common/Rate.h"

namespace KT1::Core::Fx {

// the event map: a row turns the pair (sim event, display event) on. Core knows no def names, so a row with a unit
// class or weapon filter turns nothing on. Also used by FxLogicUnits.cpp.
bool Mapped(const FxEventsConfig& config, std::string_view on, std::string_view display) {
	return std::any_of(config.events.begin(), config.events.end(), [&](const FxEventRow& row) {
		return row.on == on && row.display_event == display && row.unit_class.empty() && row.weapon.empty();
	});
}

namespace {

constexpr float PRUNE_BEAMS_S = 10;                  // DESIGN: fx_blasts.lua forgets old beam cells this often
constexpr int64_t CONTACT_KEY_UNIT = 4096;           // DESIGN: fx_core.lua contact key, attacker * this + weapon
constexpr int32_t BEAM_KEY_WEAPON = 1000003;         // DESIGN: fx_blasts.lua beam key factor of the weapon
constexpr int32_t BEAM_KEY_X = 1009;                 // DESIGN: fx_blasts.lua beam key factor of the cell's x
constexpr int32_t GATE_F = 73856093;                 // DESIGN: fx_core.lua Gate hash factor of the step
constexpr int32_t GATE_X = 19349663;                 // DESIGN: fx_core.lua Gate hash factor of x
constexpr int32_t GATE_Z = 83492791;                 // DESIGN: fx_core.lua Gate hash factor of z
constexpr int32_t GATE_MOD = 1000003;                // DESIGN: fx_core.lua Gate hash range
constexpr float LEAVE_X = 7;                         // DESIGN: fx_core.lua mark extras hash, factor of x
constexpr float LEAVE_Z = 13;                        // DESIGN: fx_core.lua mark extras hash, factor of z
constexpr float LEAVE_MOD = 101;                     // DESIGN: fx_core.lua mark extras hash range

float F(int32_t v) { return static_cast<float>(v); }

// the per step limits start again with the first request of a step
void FreshLimits(const FxEnv& env) {
	FxState& s = env.state;
	if (s.limitStep == env.step) return;
	s.limitStep = env.step;
	s.abilityUsed = 0;
	s.scriptUsed = 0;
	s.scorchUsed = 0;
	s.blastUsed = 0;
}

bool Take(const FxEnv& env, int32_t& used, int32_t limit) {
	FreshLimits(env);
	if (used >= limit) return false;
	++used;
	return true;
}

// the entry of `key`, in key order; found: it was there
std::vector<KeyStep>::iterator Find(std::vector<KeyStep>& list, int64_t key, bool& found) {
	const auto at = std::lower_bound(list.begin(), list.end(), key, [](const KeyStep& k, int64_t v) { return k.key < v; });
	found = at != list.end() && at->key == key;
	return at;
}

// false: the key was admitted under `gap` steps ago; else it is stamped with this step
bool Spaced(std::vector<KeyStep>& list, int64_t key, int32_t step, int32_t gap) {
	bool found = false;
	const auto at = Find(list, key, found);
	if (!found) {
		list.insert(at, {key, step});
		return true;
	}
	if (step - at->step < gap) return false;
	at->step = step;
	return true;
}

// fx_core: the weapon's hit set
void HitSet(const FxEnv& env, const SynWeapon& w, const ExplosionIn& in) {
	bool hit = w.hit.effect != NO_EFFECT || w.hit.pooled;
	if (hit && w.contactSteps > 0 && in.owner >= 0) {
		// the Lua key is a float: large unit ids share keys between neighbouring weapon ids (kept)
		const float key = F(in.owner) * static_cast<float>(CONTACT_KEY_UNIT) + F(in.weaponDef);
		hit = Spaced(env.state.contact, static_cast<int64_t>(key), env.step, w.contactSteps);
	}
	if (hit && w.unitHitTier >= 0 && env.world.unitsNear(env.world.ctx, in.pos, env.config.core.unit_hit_sphere)) hit = false;
	if (hit) Spawn(env, w.hit, in.pos, {}, Priority::Hit);
}

void ParityMark(const FxEnv& env, int32_t weaponDef, const Vec3& pos, float size, float lifeS) {
	if (!Mapped(env.config, "Explosion", "Scorch")) return;
	Scorch e;
	e.weaponDef = weaponDef;
	e.pos = {pos.x, 0, pos.z};           // the Lua message has no height
	e.size = size;
	e.lifeS = lifeS;
	e.source = ScorchSource::Parity;
	env.out.Raise(e);
}

// WVFX-*-001 R11: a hash of step and position; true also for the first mark and for one far from the last
bool Gate(const FxEnv& env, const GatedScorch& g, int32_t weaponDef, const Vec3& pos) {
	const float sum = F(env.step) * F(GATE_F) + std::floor(pos.x) * F(GATE_X) + std::floor(pos.z) * F(GATE_Z);
	const float h = DetMath::Mod(sum, F(GATE_MOD)) / F(GATE_MOD);
	const bool known = weaponDef < static_cast<int32_t>(env.state.hasGated.size());
	const bool last = known && env.state.hasGated[weaponDef] != 0;
	bool far = false;
	if (last) {
		const float dx = pos.x - env.state.lastGated[weaponDef].x;
		const float dz = pos.z - env.state.lastGated[weaponDef].z;
		far = dx * dx + dz * dz > g.radius * g.radius;
	}
	if (!(h < g.gate || far || !last)) return false;
	if (known) {
		env.state.lastGated[weaponDef] = pos;
		env.state.hasGated[weaponDef] = 1;
	}
	return true;
}

// fx_core: the ground puff and the pack's mark of a gated impact on land
void GatedMark(const FxEnv& env, const SynWeapon& w, const ExplosionIn& in) {
	const GatedScorch& g = w.gated;
	if (!g.on || !Gate(env, g, in.weaponDef, in.pos)) return;
	const float ground = env.world.groundHeight(env.world.ctx, in.pos.x, in.pos.z);
	if (!(ground > 0 && in.pos.y - ground < g.radius + env.config.core.gate_land_slack)) return;   // land, no air burst
	Spawn(env, g.puff, {in.pos.x, ground, in.pos.z}, {}, Priority::Hit);
	if (!g.leaves) return;
	const float k = DetMath::Mod(in.pos.x * LEAVE_X + in.pos.z * LEAVE_Z, LEAVE_MOD) / LEAVE_MOD;
	const float size = (g.size + g.sizePlus * k) * g.radius * env.config.core.gate_size_factor;
	ParityMark(env, in.weaponDef, in.pos, size, g.life + g.lifePlus * k);
}

void DepthMark(const FxEnv& env, const SynWeapon& w, const ExplosionIn& in) {
	if (w.depthTier == 0 || !Mapped(env.config, "Explosion", "DepthHit")) return;
	DepthHit e;
	e.pos = in.pos;
	e.tier = w.depthTier;
	e.seen = env.world.seen(env.world.ctx, in.pos);
	env.out.Raise(e);
}

// fx_blasts: the blast core and the classified blast; a beam at most once per weapon and cell in beam_gap_s
void BlastPart(const FxEnv& env, const SynWeapon& w, const ExplosionIn& in) {
	if (w.blastClass == BlastClass::None) return;
	if (w.beamBlast) {
		// the Lua key is a float: large weapon ids lose the low part of the cell (kept)
		const float cell = env.config.blast.beam_cell;
		const float key = F(in.weaponDef) * F(BEAM_KEY_WEAPON) + std::floor(in.pos.x / cell) * F(BEAM_KEY_X)
			+ std::floor(in.pos.z / cell);
		const int32_t gap = Rate::Every(env.config.blast.beam_gap_s);
		std::vector<KeyStep>& beams = env.state.beamBlast;
		if (!Spaced(beams, static_cast<int64_t>(key), env.step, gap)) return;
		if (env.step % Rate::Every(PRUNE_BEAMS_S) == 0)
			std::erase_if(beams, [&](const KeyStep& k) { return env.step - k.step > gap; });
	}
	Spawn(env, w.blastCore, in.pos, {}, Priority::Hit);
	if (Mapped(env.config, "Explosion", "Blast"))
		ForwardBlast(env, w.blastClass, w.blastFamily, w.faction, in.pos, in.weaponDef);
}

void ScorchAsk(const FxEnv& env, const SynWeapon& w, const ExplosionIn& in) {
	if (!w.scorchWatched || !Take(env, env.state.scorchUsed, env.config.relay.scorch_a_step)) return;
	if (!Mapped(env.config, "Explosion", "Scorch")) return;
	Scorch e;
	e.weaponDef = in.weaponDef;
	e.pos = in.pos;
	e.source = ScorchSource::Explosion;
	env.out.Raise(e);
}

void ScriptAsk(const FxEnv& env, const SynWeapon& w, const ExplosionIn& in) {
	if (w.script == ScriptTree::None || !Take(env, env.state.scriptUsed, env.config.relay.script_blasts_a_step)) return;
	if (!Mapped(env.config, "Explosion", "ScriptBlast")) return;
	ScriptBlast e;
	e.tree = w.script;
	e.weaponDef = in.weaponDef;
	e.pos = in.pos;
	env.out.Raise(e);
}

template<class T> void RaiseShot(const FxEnv& env, const ProjectileIn& in) {
	T e;
	e.projectile = in.projectile;
	e.weaponDef = in.weaponDef;
	e.owner = in.owner;
	e.pos = in.pos;
	e.vel = in.vel;
	e.seen = env.world.seen(env.world.ctx, in.pos);
	env.out.Raise(e);
}

float Ground(const FxEnv& env, float x, float z) { return env.world.groundHeight(env.world.ctx, x, z); }

} // namespace

void InitFx(FxState& s, const FxEventsConfig& config, const FxCapacity& capacity, float scale) {
	const size_t units = static_cast<size_t>(std::max(0, capacity.units));
	const size_t weapons = static_cast<size_t>(std::max(0, capacity.weaponDefs));
	s.budget.Init(config.budget, scale);
	s.lastConfirm.assign(units, -1);
	s.lastGated.assign(weapons, Vec3{});
	s.hasGated.assign(weapons, 0);
	s.contact.clear();
	s.beamBlast.clear();
	s.intercepts = 0;
	s.spawnedTotal = 0;
	s.limitStep = -1;
	s.abilityUsed = s.scriptUsed = s.scorchUsed = s.blastUsed = 0;
	s.building.Init(capacity.units);
	s.slot.Init(s.building);
	s.damaged.Init(capacity.units);
	for (Column<int32_t>& next : s.stateNext) next.Init(s.damaged, -1);
	s.pending.clear();
	s.hideWaits.clear();
	s.hidden.clear();
	s.wrecks = {};
	InitWrecks(s.wrecks, capacity.features);
	s.strategic = {};
}

void StartStep(const FxEnv& env) {
	env.state.budget.NewStep(env.step);
	FreshLimits(env);
	if (env.step % Rate::Every(env.config.core.scale_send_s) == 0) {
		Scale e;
		e.scale = env.state.budget.ScaleValue();
		env.out.Raise(e);
	}
}

// KT1 hot
bool Spawn(const FxEnv& env, const EffectRef& effect, const Vec3& pos, const Vec3& dir, Priority priority, float length) {
	if (effect.pooled || effect.effect == NO_EFFECT) return false;   // no budget request
	if (!env.state.budget.Admit(priority)) return false;
	const bool low = priority == Priority::Hit && env.state.budget.ScaleValue() < 1 && effect.lowEffect != NO_EFFECT;
	++env.state.spawnedTotal;
	env.out.spawns.push_back({low ? effect.lowEffect : effect.effect, pos, dir, length, priority});
	return true;
}

bool Intercept(const FxEnv& env, const Vec3& pos, const Vec3* from) {
	++env.state.intercepts;
	DepthIntercept e;
	e.pos = pos;
	if (from != nullptr) {
		e.from = *from;
		e.hasFrom = 1;
	}
	e.seen = env.world.seen(env.world.ctx, pos);
	env.out.Raise(e);
	return true;
}

void OnExplosion(const FxEnv& env, const ExplosionIn& in) {
	if (in.weaponDef < 0) return;
	if (in.weaponDef < static_cast<int32_t>(env.defs.weapons.size())) {
		const SynWeapon& w = env.defs.weapons[in.weaponDef];
		HitSet(env, w, in);
		if (w.decal) ParityMark(env, in.weaponDef, in.pos, w.decalSize, w.decalLife);
		GatedMark(env, w, in);
		DepthMark(env, w, in);
		BlastPart(env, w, in);
		ScorchAsk(env, w, in);
		ScriptAsk(env, w, in);
	}
	if (Mapped(env.config, "Explosion", "Impact")) {
		Impact e;
		e.weaponDef = in.weaponDef;
		e.owner = in.owner;
		e.pos = in.pos;
		e.seen = env.world.seen(env.world.ctx, in.pos);
		env.out.Raise(e);
	}
}

void OnProjectileCreated(const FxEnv& env, const ProjectileIn& in) {
	if (in.weaponDef < 0) return;
	if (Mapped(env.config, "ProjectileCreated", "ShotStart")) RaiseShot<ShotStart>(env, in);
	if (in.weaponDef >= static_cast<int32_t>(env.defs.weapons.size())) return;
	if (!env.defs.weapons[in.weaponDef].depthLaunch || !Mapped(env.config, "ProjectileCreated", "DepthLaunch")) return;
	DepthLaunch e;
	e.projectile = in.projectile;
	e.weaponDef = in.weaponDef;
	e.pos = in.pos;
	e.seen = env.world.seen(env.world.ctx, in.pos);
	env.out.Raise(e);
}

void OnProjectileDestroyed(const FxEnv& env, const ProjectileIn& in) {
	if (in.weaponDef >= 0 && Mapped(env.config, "ProjectileDestroyed", "ShotEnd")) RaiseShot<ShotEnd>(env, in);
}

bool ForwardBlast(const FxEnv& env, BlastClass cls, BlastFamily family, Faction faction, const Vec3& pos, int32_t weaponDef) {
	if (!Take(env, env.state.blastUsed, env.config.relay.blasts_a_step)) return false;
	Blast e;
	e.cls = cls;
	e.family = family;
	e.faction = faction;
	e.weaponDef = weaponDef;
	e.pos = pos;
	e.seen = env.world.seen(env.world.ctx, pos);
	env.out.Raise(e);
	return true;
}

bool RelayTeleport(const FxEnv& env, Teleport event) {
	if (event.kind < TeleportKind::Start || event.kind > TeleportKind::Cancel) return false;
	if (!Take(env, env.state.abilityUsed, env.config.relay.ability_events_a_step)) return false;
	event.from.y = Ground(env, event.from.x, event.from.z);
	event.to.y = Ground(env, event.to.x, event.to.z);
	event.seenFrom = env.world.seen(env.world.ctx, event.from);
	event.seenTo = env.world.seen(env.world.ctx, event.to);
	UnitNow now;
	if (event.allyTeam < 0 && env.world.unit(env.world.ctx, event.unit, now)) event.allyTeam = now.allyTeam;
	env.out.Raise(event);
	return true;
}

bool RelayCrash(const FxEnv& env, int32_t unit) {
	if (!Take(env, env.state.abilityUsed, env.config.relay.ability_events_a_step)) return false;
	Crash e;
	e.unit = unit;
	UnitNow now;
	if (env.world.unit(env.world.ctx, unit, now)) e.defId = now.defId;
	env.out.Raise(e);
	return true;
}

bool RelayRemnant(const FxEnv& env, int32_t unit, const Vec3& pos) {
	if (!Take(env, env.state.abilityUsed, env.config.relay.ability_events_a_step)) return false;
	Remnant e;
	e.unit = unit;
	e.pos = pos;
	e.seen = env.world.seen(env.world.ctx, pos);
	env.out.Raise(e);
	return true;
}

bool RelaySacrifice(const FxEnv& env, Sacrifice event) {
	if (!Take(env, env.state.abilityUsed, env.config.relay.ability_events_a_step)) return false;
	event.seen = env.world.seen(env.world.ctx, event.pos);
	UnitNow site;
	if (env.world.unit(env.world.ctx, event.site, site)) {
		event.sitePos = site.pos;
		event.siteRadius = site.radius;
		event.siteHeight = site.height;
		event.seen |= env.world.seen(env.world.ctx, site.pos);
	}
	env.out.Raise(event);
	return true;
}

} // namespace KT1::Core::Fx
