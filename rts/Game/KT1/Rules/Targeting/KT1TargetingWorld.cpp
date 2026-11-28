/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 Targeting: the engine reads and writes of Core/Targeting (KT1TargetingWorld.h). Each function names the Spring.*
// call it stands for.

#include "KT1TargetingWorld.h"

#include "ExternalAI/SkirmishAIHandler.h"
#include "Game/KT1/Host/Check.h"
#include "Game/KT1/Host/Fakes.h"
#include "Lua/LuaRulesParams.h"
#include "Lua/SupComTargeting.h"
#include "Map/Ground.h"
#include "Sim/Misc/LosHandler.h"
#include "Sim/Misc/QuadField.h"
#include "Sim/Misc/TeamHandler.h"
#include "Sim/MoveTypes/SupComGroundMoveType.h"
#include "Sim/Units/CommandAI/Command.h"
#include "Sim/Units/CommandAI/CommandAI.h"
#include "Sim/Units/CommandAI/FactoryCAI.h"
#include "Sim/Units/Unit.h"
#include "Sim/Units/UnitDef.h"
#include "Sim/Units/UnitHandler.h"
#include "Sim/Projectiles/WeaponProjectiles/WeaponProjectile.h"
#include "Sim/Weapons/Weapon.h"

#include <cstring>

namespace KT1::Targeting {

namespace TG = Core::Targeting;

namespace {

constexpr int32_t LOS_LOW_BITS = 7;   // DESIGN (engine fact): sight, radar, seen before (the Lua's raw state % 8)

const CUnit* U(int32_t id) { return (id < 0) ? nullptr : unitHandler.GetUnit(id); }

// Spring.GetUnitsInCylinder / GetUnitsInRectangle for a synced gadget: the quad field's order, by mid position; intel's
// fakes left out as gamedata/fake_units.lua leaves them out of every Lua read
template<class In> void Query(const float3& mins, const float3& maxs, In inRegion, std::vector<int32_t>& out) {
	out.clear();
	QuadFieldQuery q;
	quadField.GetUnitsExact(q, mins, maxs);
	for (const CUnit* u : *q.units) {
		if (Fakes::Is(u) || !inRegion(u->midPos)) continue;
		out.push_back(u->id);
	}
}

} // namespace

// Spring.ValidUnitID
bool EngineWorld::ValidUnit(int32_t unit) { return U(unit) != nullptr; }
// Spring.GetUnitIsDead
bool EngineWorld::DeadUnit(int32_t unit) { const CUnit* u = U(unit); return u == nullptr || u->isDead; }
int32_t EngineWorld::UnitDef(int32_t unit) { const CUnit* u = U(unit); return u ? u->unitDef->id : TG::NONE; }
int32_t EngineWorld::UnitTeam(int32_t unit) { const CUnit* u = U(unit); return u ? u->team : TG::NONE; }
int32_t EngineWorld::AllyTeam(int32_t unit) { const CUnit* u = U(unit); return u ? u->allyteam : TG::NONE; }

// Spring.AreTeamsAllied: nil (false) for an invalid team
bool EngineWorld::Allied(int32_t a, int32_t b) {
	return teamHandler.IsValidTeam(a) && teamHandler.IsValidTeam(b) && teamHandler.AlliedTeams(a, b);
}

// (Spring.GetUnitRulesParam(t, "cmd_arriving") or 0) == 1
bool EngineWorld::Arriving(int32_t unit) {
	const CUnit* u = U(unit);
	if (u == nullptr) return false;
	const auto it = u->modParams.find("cmd_arriving");
	if (it == u->modParams.end()) return false;
	const float* v = std::get_if<float>(&it->second.value);
	return v != nullptr && *v == 1;
}

// Spring.GetTeamLuaAI(team) is a name that is not empty
bool EngineWorld::ScriptAI(int32_t team) {
	if (!teamHandler.IsValidTeam(team)) return false;
	for (const uint8_t id : skirmishAIHandler.GetSkirmishAIsInTeam(team)) {
		const SkirmishAIData* ai = skirmishAIHandler.GetSkirmishAI(id);
		if (ai != nullptr && ai->isLuaAI) return !ai->shortName.empty();
	}
	return false;
}

// Spring.GetUnitLosState(t, ally, true) % 8 (a synced gadget reads the raw state)
int32_t EngineWorld::LosBits(int32_t unit, int32_t allyTeam) {
	const CUnit* u = U(unit);
	if (u == nullptr || allyTeam < 0) return 0;
	return static_cast<int32_t>(u->losStatus[allyTeam]) & LOS_LOW_BITS;
}

// Spring.IsUnitInLos
bool EngineWorld::InLos(int32_t unit, int32_t allyTeam) {
	const CUnit* u = U(unit);
	return u != nullptr && allyTeam >= 0 && losHandler->InLos(u, allyTeam);
}

// Spring.GetUnitPosition(u)
bool EngineWorld::Position(int32_t unit, float (&pos)[3]) {
	const CUnit* u = U(unit);
	if (u == nullptr) return false;
	pos[0] = u->pos.x; pos[1] = u->pos.y; pos[2] = u->pos.z;
	return true;
}

// Spring.GetUnitPosition(u, true): the fourth to sixth values, the mid position
bool EngineWorld::MidPosition(int32_t unit, float (&pos)[3]) {
	const CUnit* u = U(unit);
	if (u == nullptr) return false;
	pos[0] = u->midPos.x; pos[1] = u->midPos.y; pos[2] = u->midPos.z;
	return true;
}

float EngineWorld::Height(int32_t unit) { const CUnit* u = U(unit); return u ? u->height : 0; }
float EngineWorld::Radius(int32_t unit) { const CUnit* u = U(unit); return u ? u->radius : 0; }

// Spring.GetUnitMoveTypeData(t).supcomLayer
TG::Layer EngineWorld::MovementLayer(int32_t unit) {
	const CUnit* u = U(unit);
	const auto* mt = (u != nullptr) ? dynamic_cast<const CSupComGroundMoveType*>(u->moveType) : nullptr;
	if (mt == nullptr) return TG::Layer::Unknown;
	const char* name = mt->GetLayerName();
	if (std::strcmp(name, "seabed") == 0) return TG::Layer::Seabed;
	if (std::strcmp(name, "water") == 0) return TG::Layer::Water;
	if (std::strcmp(name, "sub") == 0) return TG::Layer::Sub;
	return TG::Layer::Land;
}

// Spring.GetGroundHeight in synced code
float EngineWorld::GroundHeight(float x, float z) { return CGround::GetHeightReal(x, z, true); }

void EngineWorld::UnitsInCylinder(float x, float z, float radius, std::vector<int32_t>& out) {
	const float r2 = radius * radius;
	const float3 c(x, 0.0f, z);
	Query(float3(x - radius, 0.0f, z - radius), float3(x + radius, 0.0f, z + radius),
		[&](const float3& p) { return p.SqDistance2D(c) <= r2; }, out);
}

void EngineWorld::UnitsInRectangle(float x1, float z1, float x2, float z2, std::vector<int32_t>& out) {
	Query(float3(x1, 0.0f, z1), float3(x2, 0.0f, z2),
		[&](const float3& p) { return !(p.x < x1 || p.x > x2 || p.z < z1 || p.z > z2); }, out);
}

// Spring.GetUnitStates(u, false, false): the first value
int32_t EngineWorld::FireState(int32_t unit) { const CUnit* u = U(unit); return u ? u->fireState : 0; }

// Spring.GetUnitCurrentCommand(u, 1)
TG::CommandView EngineWorld::CurrentCommand(int32_t unit) {
	TG::CommandView v;
	const CUnit* u = U(unit);
	if (u == nullptr) return v;
	const CFactoryCAI* f = dynamic_cast<const CFactoryCAI*>(u->commandAI);
	const CCommandQueue& q = (f == nullptr) ? u->commandAI->commandQue : f->newUnitCommands;
	if (q.empty()) return v;
	const Command& c = q.front();
	v.command = c.GetID();
	v.options = c.GetOpts();
	v.nparams = static_cast<int32_t>(c.GetNumParams());
	v.p1 = (v.nparams >= 1) ? c.GetParam(0) : 0;
	return v;
}

// Spring.GetUnitWeaponState(u, n, "range")
bool EngineWorld::WeaponRange(int32_t unit, int32_t weapon, float& range) {
	const CUnit* u = U(unit);
	if (u == nullptr || weapon < 1 || static_cast<size_t>(weapon) > u->weapons.size()) return false;
	range = u->weapons[weapon - 1]->range;
	return true;
}

// the third value of Spring.GetUnitWeaponTarget(u, n) when it is an id (a unit or an intercepted projectile)
int32_t EngineWorld::WeaponTarget(int32_t unit, int32_t weapon) {
	const CUnit* u = U(unit);
	if (u == nullptr || weapon < 1 || static_cast<size_t>(weapon) > u->weapons.size()) return TG::NONE;
	const SWeaponTarget& t = u->weapons[weapon - 1]->GetCurrentTarget();
	if (t.type == Target_Unit && t.unit != nullptr) return t.unit->id;
	if (t.type == Target_Intercept && t.intercept != nullptr) return t.intercept->id;
	return TG::NONE;
}

// Spring.SupComTgtGet(u, n)
int32_t EngineWorld::ChosenTarget(int32_t unit, int32_t weapon) { return SupComTargeting::TargetOf(unit, weapon - 1); }

// Spring.SetUnitTarget(u, t, false, false, n)
void EngineWorld::SetWeaponTarget(int32_t unit, int32_t weapon, int32_t target) {
	if (!Check::MayWrite()) return;
	CUnit* u = unitHandler.GetUnit(unit);
	const CUnit* t = U(target);
	if (u == nullptr || t == nullptr || t == u || weapon < 1 || static_cast<size_t>(weapon) > u->weapons.size()) return;
	SWeaponTarget trg(t, false);
	trg.isManualFire = false;
	u->weapons[weapon - 1]->Attack(trg);
}

// Spring.SetUnitRulesParam(u, "cmb_refused", why); Spring.SetUnitRulesParam(u, "cmb_refused_frame", frame)
void EngineWorld::WriteRefusal(int32_t unit, TG::Refusal reason, int32_t step) {
	if (!Check::MayWrite()) return;
	CUnit* u = unitHandler.GetUnit(unit);
	if (u == nullptr) return;
	u->modParams["cmb_refused"].value.emplace<std::string>(TG::RefusalName(reason));
	u->modParams["cmb_refused_frame"].value.emplace<float>(static_cast<float>(step));
}

} // namespace KT1::Targeting
