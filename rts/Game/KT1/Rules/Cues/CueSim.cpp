/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 rule: CueSim.h. Movement is looked at one share of the units a step; build loops once per check period; shots and
// impacts come from the sim events. The build-beam rule is gamedata/fx_build_logic.lua BeamOn.

#include "CueSim.h"

#include <cmath>
#include <string>

#include "Game/KT1/Core/Cues/CueEvents.h"
#include "Game/KT1/Host/Check.h"
#include "Game/KT1/Host/DisplayEvents.h"
#include "Sim/Features/Feature.h"
#include "Sim/Features/FeatureHandler.h"
#include "Sim/Units/CommandAI/Command.h"
#include "Sim/Units/CommandAI/CommandAI.h"
#include "Sim/Units/Unit.h"
#include "Sim/Units/UnitDef.h"
#include "Sim/Units/UnitHandler.h"
#include "Sim/Units/UnitTypes/Builder.h"
#include "Sim/Units/UnitTypes/Factory.h"
#include "Sim/Weapons/WeaponDef.h"
#include "Sim/Weapons/WeaponDefHandler.h"

namespace KT1::Cues {
namespace {

using Core::SimHost::Event;
using Core::SimHost::EventType;
namespace Cue = Core::Cues;

constexpr float RANGE_SLACK = 24.0f;   // DESIGN: elmos beyond build distance plus target radius still at work (fx_build_logic.lua)

CueSim* active = nullptr;

std::string ClassOf(const UnitDef* def) {
	if (def == nullptr) return {};
	const auto it = def->customParams.find("unit_class");
	return it == def->customParams.end() ? std::string() : std::string(it->second.c_str());
}

// the target of a repair, reclaim or capture order; nullptr: none in the world
const CSolidObject* OrderTarget(const Command& c) {
	const int cmd = c.GetID();
	if (cmd != CMD_RECLAIM && cmd != CMD_REPAIR && cmd != CMD_CAPTURE) return nullptr;
	if (c.GetNumParams() != 1) return nullptr;   // an area order: no single target yet
	const int id = static_cast<int>(c.GetParam(0));
	const int maxUnits = static_cast<int>(unitHandler.MaxUnits());
	if (cmd == CMD_RECLAIM && id >= maxUnits) return featureHandler.GetFeature(id - maxUnits);
	return (id >= 0 && id < maxUnits) ? unitHandler.GetUnit(id) : nullptr;
}

bool BeamOn(const CUnit* u) {
	const CBuilder* builder = dynamic_cast<const CBuilder*>(u);
	if (builder != nullptr && builder->curBuild != nullptr) return true;
	const CFactory* factory = dynamic_cast<const CFactory*>(u);
	if (factory != nullptr && factory->curBuild != nullptr) return true;
	if (u->commandAI == nullptr || u->commandAI->commandQue.empty()) return false;
	const CSolidObject* target = OrderTarget(u->commandAI->commandQue.front());
	if (target == nullptr) return false;
	const float dx = target->pos.x - u->pos.x, dz = target->pos.z - u->pos.z;
	return std::sqrt(dx * dx + dz * dz) <= u->unitDef->buildDistance + target->radius + RANGE_SLACK;
}

} // namespace

CueSim* ActiveSim() { return active; }
void SetActiveSim(CueSim* sim) { active = sim; }

Subject Of(const CUnit* u) {
	Subject s;
	s.unit = u->id;
	s.def = u->unitDef->id;
	s.team = u->team;
	s.pos = u->pos;
	return s;
}

void CueSim::Init(const Cue::CueConfig& cfg, int maxUnits, int weaponDefs) {
	config = cfg;
	params = Cue::MakeParams(config);
	state.Init(maxUnits, weaponDefs, config);
	ids.reserve(maxUnits);
}

void CueSim::Step(const SimContext& c) {
	check = c.check;
	Track(c);
	state.log.clear();                 // the log holds one step
	Cue::BeginStep(state, params, c.frame);
	Moves(c.frame);
	if (c.frame % params.buildCheckSteps == 0) Loops();
}

void CueSim::Track(const SimContext& c) {
	c.events.ForEachRaised([this](const Event& e) {
		if (e.type == EventType::UnitDestroyed) {
			if (state.RemoveUnit(e.id)) Loop(e.id, false);
			return;
		}
		const CUnit* u = unitHandler.GetUnit(e.id);
		if (e.type != EventType::UnitCreated || u == nullptr) return;
		const UnitDef* d = u->unitDef;
		state.AddUnit(e.id, {d->speed > 0.0f && !d->IsBuildingUnit(), false, d->IsBuilderUnit()});
	});
}

void CueSim::Moves(int step) {   // KT1 hot
	const auto live = state.units.AscendingIds();
	ids.assign(live.begin(), live.end());
	for (const int id : ids) {
		if (!Cue::MoveDue(params, id, step) || state.mobile[state.units.SlotOf(id)] == 0) continue;
		const CUnit* u = unitHandler.GetUnit(id);
		if (u == nullptr) continue;
		const bool moving = u->speed.w > params.movingSpeed;
		if (!Cue::MoveChanged(state, id, moving)) continue;
		Trigger(moving ? "move_start" : "move_stop", Of(u), ClassOf(u->unitDef), {});
	}
}

void CueSim::Loops() {   // KT1 hot
	const auto live = state.units.AscendingIds();
	ids.assign(live.begin(), live.end());
	for (const int id : ids) {
		if (state.builder[state.units.SlotOf(id)] == 0) continue;
		const CUnit* u = unitHandler.GetUnit(id);
		if (u == nullptr) continue;
		const bool on = BeamOn(u);
		if (Cue::LoopChanged(state, id, on)) Loop(id, on);
	}
}

void CueSim::Events(const SimContext& c) {
	check = c.check;
	c.events.ForEachRaised([this](const Event& e) {
		if (e.type == EventType::ProjectileCreated) Fired(e);
		else if (e.type == EventType::Explosion) Hit(e);
	});
}

void CueSim::Fired(const Event& e) {   // KT1 hot
	if (e.def < 0 || e.other < 0) return;
	const CUnit* owner = unitHandler.GetUnit(e.other);
	if (owner == nullptr) return;
	const WeaponDef* wd = weaponDefHandler->GetWeaponDefByID(e.def);
	if (wd == nullptr) return;
	Subject s = Of(owner);
	s.weapon = e.def;
	s.pos = float3(e.pos[0], e.pos[1], e.pos[2]);
	Trigger("ProjectileCreated", s, ClassOf(owner->unitDef), wd->name);
}

void CueSim::Hit(const Event& e) {   // KT1 hot
	const WeaponDef* wd = e.def < 0 ? nullptr : weaponDefHandler->GetWeaponDefByID(e.def);
	if (wd == nullptr) return;
	if (config.beam_weapon_type == wd->type.c_str() && !Cue::BeamAdmit(state, params, e.def)) return;
	const CUnit* owner = e.other < 0 ? nullptr : unitHandler.GetUnit(e.other);
	Subject s;                          // an impact is at a point: no unit, no def
	s.team = owner == nullptr ? 0 : owner->team;
	s.weapon = e.def;
	s.pos = float3(e.pos[0], e.pos[1], e.pos[2]);
	Trigger("Explosion", s, {}, wd->name);
}

// A row filtered by the viewer's relation cannot be decided in the sim; such rows raise nothing (rule doc).
void CueSim::Trigger(std::string_view on, const Subject& subject, std::string_view unitClass, std::string_view weapon) {
	for (size_t i = 0; i < config.rows.size(); ++i) {
		const Cue::CueRow& row = config.rows[i];
		if (!Cue::RowMatches(row, on, unitClass, weapon, Core::TeamRelation::Any)) continue;
		if (!Cue::RowAdmit(state, config, static_cast<int32_t>(i))) continue;
		Subject s = subject;
		s.variant = row.variant;
		Send(row.cue, s);
	}
}

bool CueSim::Send(std::string_view cue, const Subject& subject) {
	if (!Cue::Admit(state)) return false;
	Emit(cue, subject);
	return true;
}

void CueSim::Emit(std::string_view cue, const Subject& s) {
	const float pos[3] = {s.pos.x, s.pos.y, s.pos.z};
	Cue::Log(state, s.unit, Cue::CueId(state, cue), pos);
	if (check) {
		Check::Native(NAME, std::string(cue).c_str(), s.unit, 0, 0, s.pos.x);
		return;
	}
	Cue::CueEvent e;
	e.unit = s.unit;
	e.def = s.def;
	e.team = s.team;
	e.weapon = s.weapon;
	for (int i = 0; i < 3; ++i) e.pos[i] = pos[i];   // DESIGN: x, y, z
	Cue::SetName(e.cue, cue);
	Cue::SetName(e.variant, s.variant);
	DisplayEvents::Send(e);
}

void CueSim::Loop(int unit, bool on) {
	if (check) {
		Check::Native(NAME, "loop", unit, 0, 0, on ? 1.0f : 0.0f);
		return;
	}
	Cue::CueLoopEvent e;
	e.unit = unit;
	e.on = on ? 1 : 0;
	DisplayEvents::Send(e);
}

} // namespace KT1::Cues
