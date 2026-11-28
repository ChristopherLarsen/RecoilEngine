/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 rule: Targeting (version 1). The game's target choice around the fork's engine step (REQ CMB-004 v2 and the rules in
// Core/Targeting/TargetState.h): it feeds the step its ranks, rules and guns, counts shots in flight, answers the order and
// weapon-target call-ins, and runs the command unit's gun. Ported from cmb_targeting.lua and unit_cmd_gun.lua as the Lua
// has them (LUA-FREE S19; port, then correct).
//
// Replaces:   LuaRules/Gadgets/cmb_targeting.lua, LuaRules/Gadgets/unit_cmd_gun.lua (they run when off or in check)
// Switch:     kt1_targeting = "" (the family decides) / 0 / 1 / check; family combat (kt1_fam_combat)
// Config:     gamedata/kt1/targeting.lua (Core/Targeting/TargetingConfig.h)
// Phases:     FrameStart  the command guns (unit_cmd_gun's GameFrame, layer 0)
//             FrameEnd    unit and projectile events of the step. The engine step ran before this step's FrameStart, so
//                         it sees them at its next run, as it saw the Lua's immediate calls.
// Allow:      AllowCommand (CMB-018 R4 refusals; a fire-state order wakes the unit's guns), AllowWeaponTarget
// State:      Core TargetState: command guns, focus, in-flight damage, answers; synced columns when on
// Writes:     the engine step (SupComTargeting::*), weapon targets, rules params cmb_refused and cmb_refused_frame; none
//             in check
// Check:      native "gun" (the command gun's target per unit per step) and "inflight" (target, on each change); the
//             Allow answers through AllowChain. The gadgets answer the same keys.
// Doc:        game-build docs/engine/rules/targeting.md

#include <algorithm>

#include "KT1TargetingDefs.h"
#include "KT1TargetingRule.h"
#include "KT1TargetingWorld.h"

#include "Game/KT1/Core/Common/Rate.h"
#include "Game/KT1/Host/AllowChain.h"
#include "Game/KT1/Host/Check.h"
#include "Game/KT1/Host/Phases.h"
#include "Game/KT1/Host/SimContext.h"
#include "Game/KT1/Host/SyncState.h"
#include "Game/KT1/KT1Register.h"
#include "Game/KT1/KT1Registry.h"
#include "Lua/LuaParser.h"
#include "TargetingBridge.h"
#include "Sim/Misc/GlobalSynced.h"
#include "Sim/Misc/GlobalConstants.h"
#include "Sim/Projectiles/ProjectileHandler.h"
#include "Sim/Projectiles/WeaponProjectiles/WeaponProjectile.h"
#include "Sim/Units/CommandAI/Command.h"
#include "Sim/Units/Unit.h"
#include "Sim/Units/UnitHandler.h"
#include "Sim/Weapons/WeaponDef.h"
#include "Sim/Weapons/WeaponDefHandler.h"

namespace KT1::Targeting {

namespace TG = Core::Targeting;
using Core::SimHost::Event;
using Core::SimHost::EventType;
using Core::SimHost::MaskOf;
using Core::SimHost::Phase;

namespace {

TargetingRule* live = nullptr;

void ReadGunConfig(const LuaTable& t, TG::CommandGunConfig& g) {
	g.weapon = t.GetString("weapon", g.weapon);
	g.scan_interval_s = t.GetFloat("scan_interval_s", g.scan_interval_s);
	g.engage_inset_mu = t.GetFloat("engage_inset_mu", g.engage_inset_mu);
	g.tracking_factor = t.GetFloat("tracking_factor", g.tracking_factor);
	g.range_min_mu = t.GetFloat("range_min_mu", g.range_min_mu);
	g.range_max_mu = t.GetFloat("range_max_mu", g.range_max_mu);
	const LuaTable list = t.SubTable("priorities");
	if (!list.IsValid()) return;
	g.priorities.clear();
	for (int i = 1; i <= list.GetLength(); ++i) g.priorities.push_back({list.GetString(i, "")});
}

} // namespace

TargetingRule::TargetingRule() : Rule({NAME, 1, true, "LuaRules/Gadgets/cmb_targeting.lua", false, true}) {}

TargetingRule* TargetingRule::Live() { return live; }

void TargetingRule::LoadConfig(const LuaTable& t) {
	TG::TargetingConfig& c = config;
	c.elmos_per_mu = t.GetFloat("elmos_per_mu", c.elmos_per_mu);
	c.ref_tick_s = t.GetFloat("ref_tick_s", c.ref_tick_s);
	c.engage_inset_mu = t.GetFloat("engage_inset_mu", c.engage_inset_mu);
	c.tracking_factor_default = t.GetFloat("tracking_factor_default", c.tracking_factor_default);
	c.scan_interval_default_s = t.GetFloat("scan_interval_default_s", c.scan_interval_default_s);
	c.scan_extra_ticks = t.GetInt("scan_extra_ticks", c.scan_extra_ticks);
	c.retarget_skip_s = t.GetFloat("retarget_skip_s", c.retarget_skip_s);
	c.overkill_inflight_factor = t.GetFloat("overkill_inflight_factor", c.overkill_inflight_factor);
	c.line_sample_step = t.GetFloat("line_sample_step", c.line_sample_step);
	c.line_clearance = t.GetFloat("line_clearance", c.line_clearance);
	c.line_friends = t.GetBool("line_friends", c.line_friends);
	c.line_friend_pad = t.GetFloat("line_friend_pad", c.line_friend_pad);
	c.inflight_epsilon = t.GetFloat("inflight_epsilon", c.inflight_epsilon);
	c.scan_phase_factor = t.GetInt("scan_phase_factor", c.scan_phase_factor);
	c.fire_state_cmd = t.GetInt("fire_state_cmd", c.fire_state_cmd);
	c.barrier_code_part = t.GetString("barrier_code_part", c.barrier_code_part);
	ReadGunConfig(t.SubTable("command_gun"), c.command_gun);

	TG::CommandIds ids;
	ids.attack = CMD_ATTACK;
	ids.manualFire = CMD_MANUALFIRE;
	ids.insert = CMD_INSERT;
	ids.internalOption = INTERNAL_ORDER;
	params = TG::MakeTargetingParams(c, ids, Core::Rate::StepsPerSecond());
	state.Init(MAX_UNITS);
	RegisterPhases();
	RegisterAllow();
	if (SwitchMode() != Mode::Check) RegisterSync();
	live = this;
}

void TargetingRule::ConfigValues(Data::Values& v) const {
	v.Add("engage_inset_mu", config.engage_inset_mu);
	v.Add("scan_extra_ticks", config.scan_extra_ticks);
	v.Add("retarget_skip_s", config.retarget_skip_s);
	v.Add("overkill_inflight_factor", config.overkill_inflight_factor);
}

void TargetingRule::Start() {
	ReadDefs(defs, config);
	if (SwitchMode() == Mode::On) FeedEngineStep(defs, params, config);
}

void TargetingRule::PushLua(lua_State* L) { PushTargetingBridge(L); }

void TargetingRule::RegisterPhases() {
	Phases::Add(NAME, Phase::FrameStart, 0, 0, [this](const SimContext& c) { CommandGuns(c); });
	const auto mask = MaskOf(EventType::UnitCreated) | MaskOf(EventType::UnitDestroyed)
		| MaskOf(EventType::ProjectileCreated) | MaskOf(EventType::ProjectileDestroyed);
	Phases::Add(NAME, Phase::FrameEnd, 0, mask, [this](const SimContext& c) { Events(c); });
}

void TargetingRule::RegisterSync() {
	SyncState::Add(NAME, "focus", state.units.index, state.focus);
	SyncState::Add(NAME, "in_flight", state.units.index, state.inFlight);
	SyncState::Add(NAME, "gun", [this](Core::SimHost::StateHash& h) {
		for (const int32_t id : state.gunners.Ids()) {
			const auto slot = state.units.index.SlotOf(id);
			if (slot == Core::DenseIndex::NONE) continue;
			h.Add(id);
			h.Add(state.gunner[slot].target);
			h.Add(state.gunner[slot].nextScan);
		}
	});
}

// KT1 hot
void TargetingRule::CommandGuns(const SimContext& c) {
	const TG::TargetingContext ctx{defs, params};
	TG::RunCommandGuns(state, ctx, c.frame, world);
	if (!c.check) return;
	for (const int32_t id : state.gunners.Ids()) {
		const auto slot = state.units.index.SlotOf(id);
		if (slot != Core::DenseIndex::NONE) Check::Native(NAME, "gun", id, 0, 0, static_cast<float>(state.gunner[slot].target));
	}
}

void TargetingRule::Events(const SimContext& c) {
	c.events.ForEachRaised([this, &c](const Event& e) {
		switch (e.type) {
			case EventType::UnitCreated: Created(c, e.id, e.def); break;
			case EventType::UnitDestroyed: Destroyed(c, e.id); break;
			case EventType::ProjectileCreated: ShotCreated(c, e.id, e.def); break;
			case EventType::ProjectileDestroyed: ShotGone(c, e.id); break;
			default: break;
		}
	});
}

// cmb_targeting.lua and unit_cmd_gun.lua UnitCreated
void TargetingRule::Created(const SimContext& c, int32_t id, int32_t def) {
	const TG::TargetingContext ctx{defs, params};
	TG::TargetUnitCreated(state, ctx, id, def, c.frame);
	if (c.check || def < 0 || static_cast<size_t>(def) >= defs.units.size()) return;
	const auto slot = state.units.index.SlotOf(id);
	if (slot == Core::DenseIndex::NONE) return;
	int32_t first = TG::NONE;
	for (int32_t n = 0; n < TG::MAX_UNIT_WEAPONS; ++n) {
		const int32_t due = state.guns[slot].scanDue[n];
		if (due == TG::NONE) continue;
		SupComTargeting::AddGun(id, n, defs.units[def].weapons[n], due);
		if (first == TG::NONE || due < first) first = due;
	}
	if (first != TG::NONE) SupComTargeting::WakeUnit(id, first);
}

// cmb_targeting.lua and unit_cmd_gun.lua UnitDestroyed
void TargetingRule::Destroyed(const SimContext& c, int32_t id) {
	TG::TargetUnitDestroyed(state, id);
	if (c.check) return;
	SupComTargeting::UnitGone(id, c.frame + 1);
	SupComTargeting::SetInflight(id, 0);
}

// cmb_targeting.lua ProjectileCreated: a scanned weapon's shot at a unit counts toward its kill
void TargetingRule::ShotCreated(const SimContext& c, int32_t projectile, int32_t weaponDef) {
	const CProjectile* p = projectileHandler.GetProjectileBySyncedID(projectile);
	if (p == nullptr || !p->weapon) return;
	const CWorldObject* to = static_cast<const CWeaponProjectile*>(p)->GetTargetObject();
	const CUnit* t = dynamic_cast<const CUnit*>(to);
	const WeaponDef* wd = weaponDefHandler->GetWeaponDefByID(weaponDef);
	if (t == nullptr || wd == nullptr) return;
	// Lua ShotDamage: dm[armour] or dm[0] or 0
	const int types = wd->damages.GetNumTypes();
	const int armour = t->unitDef->armorType;
	const float raw = (armour >= 0 && armour < types) ? wd->damages.Get(armour) : (types > 0 ? wd->damages.Get(0) : 0.0f);
	float total = 0;
	const TG::TargetingContext ctx{defs, params};
	if (!TG::AddShot(state, ctx, projectile, weaponDef, t->id, raw, total)) return;
	if (c.check) Check::Native(NAME, "inflight", t->id, 0, 0, total);
	else SupComTargeting::SetInflight(t->id, total);
}

// cmb_targeting.lua ProjectileDestroyed
void TargetingRule::ShotGone(const SimContext& c, int32_t projectile) {
	int32_t target = TG::NONE;
	float total = 0;
	const TG::TargetingContext ctx{defs, params};
	if (!TG::RemoveShot(state, ctx, projectile, target, total)) return;
	if (c.check) Check::Native(NAME, "inflight", target, 0, 0, total);
	else SupComTargeting::SetInflight(target, total);
}

void TargetingRule::RegisterAllow() {
	AllowChain::AddCommand(NAME, 0, [this](const CUnit* u, const Command& cmd, int, bool, bool, Faked) {
		return AllowCommand(u, cmd);
	});
	AllowChain::AddWeaponTarget(NAME, 0, [this](unsigned a, unsigned t, unsigned n, unsigned wd, float*, Faked) {
		const int32_t target = static_cast<int32_t>(t);
		const int32_t weapon = static_cast<int32_t>(n);
		if (target < 0 || weapon < 0) return true;   // the command AI's search question (-1, -1)
		const TG::TargetingContext ctx{defs, params};
		return TG::AllowTarget(state, ctx, static_cast<int32_t>(a), target, weapon + 1, static_cast<int32_t>(wd),
			CurrentFrame(), world);
	});
}

// cmb_targeting.lua AllowCommand, and its UnitCommand: a fire-state order makes the unit's guns look at once
bool TargetingRule::AllowCommand(const CUnit* u, const Command& cmd) {
	const int id = cmd.GetID();
	if ((id == CMD_FIRE_STATE || id == config.fire_state_cmd) && Check::MayWrite())
		SupComTargeting::FireStateChanged(u->id, CurrentFrame());
	TG::OrderView o;
	o.unit = u->id;
	o.unitDef = u->unitDef->id;
	o.team = u->team;
	o.command = id;
	o.nparams = static_cast<int32_t>(cmd.GetNumParams());
	for (int32_t i = 0; i < std::min<int32_t>(o.nparams, static_cast<int32_t>(o.params.size())); ++i) o.params[i] = cmd.GetParam(i);
	const TG::TargetingContext ctx{defs, params};
	return TG::AllowOrder(state, ctx, o, CurrentFrame(), world);
}

// the Lua's Spring.GetGameFrame() inside a call-in
int32_t TargetingRule::CurrentFrame() const { return gs->frameNum; }

} // namespace KT1::Targeting

namespace KT1 {
// ORDER: 2000, the first of phase "before move"
KT1_REGISTER_SYSTEM(Targeting, 2000, Targeting::TargetingRule)
} // namespace KT1
