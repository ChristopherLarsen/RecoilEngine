/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 host: DrawView's reads of the viewer, teams, the ground and units (see DrawView.h, DrawReads.h). Each answers as
// the Spring.* call it replaces answers a widget (LuaSyncedRead, LuaUnsyncedRead).

#include "DrawView.h"

#include <iterator>
#include <utility>
#include <variant>

#include "ViewerRules.h"

#include "Game/Game.h"
#include "Lua/LuaRulesParams.h"
#include "Map/Ground.h"
#include "Rendering/Models/ModelsMemStorage.h"
#include "Sim/Misc/TeamHandler.h"
#include "Sim/Units/CommandAI/CommandAI.h"
#include "Sim/Units/CommandAI/FactoryCAI.h"
#include "Sim/Units/UnitHandler.h"
#include "Sim/Units/UnitTypes/Builder.h"
#include "Sim/Units/UnitTypes/Factory.h"

namespace KT1 {

namespace {

constexpr float COLOUR_BYTE_MAX = 255.0f;   // DESIGN: a team colour channel is a byte (the stock GetTeamColor)

// the unit, when the viewer sees it
const CUnit* SeenUnit(int id) {
	if (id < 0) return nullptr;
	const CUnit* u = unitHandler.GetUnit(id);
	return (u != nullptr && ViewerRules::Seen(*u)) ? u : nullptr;
}

void ReadLos(const CUnit& u, UnitStatus& out) {
	if (gu->spectatingFullView) {
		out.inLos = out.inRadar = out.seenBefore = true;
		return;
	}
	const unsigned short los = u.losStatus[gu->myAllyTeam];
	constexpr unsigned short prev = LOS_PREVLOS | LOS_CONTRADAR;
	out.inLos = (los & LOS_INLOS) != 0;
	out.inRadar = (los & LOS_INRADAR) != 0;
	out.seenBefore = (los & prev) == prev;
}

// GetUnitHealth, GetUnitIsBeingBuilt, GetUnitDirection
void ReadInSight(const CUnit& u, UnitStatus& out) {
	out.hasFront = true;
	out.front = u.frontdir;
	out.hasBuild = true;
	out.buildProgress = u.buildProgress;
	out.beingBuilt = u.beingBuilt;

	const UnitDef* ud = u.unitDef;
	if (ud->hideDamage && !out.readAsAlly) return;
	// an enemy decoy reports the health its decoy type would have
	const bool decoy = !out.readAsAlly && ud->decoyDef != nullptr;
	const float scale = decoy ? (ud->decoyDef->health / ud->health) : 1.0f;
	out.hasHealth = true;
	out.health = scale * u.health;
	out.maxHealth = scale * u.maxHealth;
}

// GetUnitIsCloaked, GetUnitIsBuilding, GetUnitCommands
void ReadOrders(const CUnit& u, UnitStatus& out) {
	out.hasOrders = true;
	out.cloaked = u.isCloaked;

	const CUnit* made = nullptr;
	if (const CBuilder* builder = dynamic_cast<const CBuilder*>(&u); builder != nullptr) made = builder->curBuild;
	if (made == nullptr) {
		if (const CFactory* factory = dynamic_cast<const CFactory*>(&u); factory != nullptr) made = factory->curBuild;
	}
	out.building = made != nullptr;
	if (made != nullptr) out.buildTarget = made->id;

	if (u.commandAI == nullptr) return;
	const CFactoryCAI* factoryAI = dynamic_cast<const CFactoryCAI*>(u.commandAI);
	const auto& queue = (factoryAI == nullptr) ? u.commandAI->commandQue : factoryAI->newUnitCommands;
	out.commandCount = static_cast<int>(queue.size());
	out.idle = u.buildProgress >= 1.0f && queue.empty();
}

// the param, when the viewer may read it (GetUnitRulesParamLosMask)
const LuaRulesParams::Param* ReadableParam(const CUnit& u, const std::string& name) {
	const auto it = u.modParams.find(name);
	if (it == u.modParams.end()) return nullptr;

	int mask = LuaRulesParams::RULESPARAMLOS_PUBLIC_MASK;
	if (ViewerRules::ReadsAsAlly(u) || (game != nullptr && game->IsGameOver())) {
		mask = LuaRulesParams::RULESPARAMLOS_PRIVATE_MASK;
	} else if (teamHandler.AlliedTeams(u.team, gu->myTeam)) {
		mask = LuaRulesParams::RULESPARAMLOS_ALLIED_MASK;
	} else {
		const unsigned short los = u.losStatus[gu->myAllyTeam];
		if (los & LOS_INLOS) mask = LuaRulesParams::RULESPARAMLOS_INLOS_MASK;
		else if (los & (LOS_PREVLOS | LOS_CONTRADAR)) mask = LuaRulesParams::RULESPARAMLOS_TYPED_MASK;
		else if (los & LOS_INRADAR) mask = LuaRulesParams::RULESPARAMLOS_INRADAR_MASK;
	}
	return (it->second.los & mask) ? &it->second : nullptr;
}

} // namespace

ViewerView DrawView::Viewer() const {
	ViewerView v;
	v.team = gu->myTeam;
	v.allyTeam = gu->myAllyTeam;
	v.spectating = gu->spectating;
	v.fullView = gu->spectatingFullView;
	v.fullSelect = gu->spectatingFullSelect;
	return v;
}

int DrawView::TeamCount() const { return teamHandler.ActiveTeams(); }

int DrawView::TeamAllyTeam(int team) const { return teamHandler.IsValidTeam(team) ? teamHandler.AllyTeam(team) : -1; }

bool DrawView::TeamColour(int team, float (&rgba)[4]) const {
	if (!teamHandler.IsValidTeam(team)) return false;
	const CTeam* t = teamHandler.Team(team);
	for (int i = 0; i < std::ssize(rgba); i++) rgba[i] = t->color[i] / COLOUR_BYTE_MAX;
	return true;
}

float DrawView::GroundHeight(float x, float z) const { return CGround::GetHeightReal(x, z, false); }

bool DrawView::Visible(int id) const { return SeenUnit(id) != nullptr; }

bool DrawView::Status(int id, UnitStatus& out) const {   // KT1 hot
	const CUnit* u = SeenUnit(id);
	if (u == nullptr) return false;

	out = UnitStatus{};
	out.id = id;
	out.defId = ViewerRules::DefId(*u);
	out.team = u->team;
	out.allyTeam = u->allyteam;
	out.uniformSlot = static_cast<int>(std::as_const(modelUniformsStorage).GetObjOffset(u));
	out.readAsAlly = ViewerRules::ReadsAsAlly(*u);
	ReadLos(*u, out);

	const float3 error = out.readAsAlly ? float3() : u->GetLuaErrorVector(gu->myAllyTeam, false);
	out.pos = u->pos + error;
	out.midPos = u->midPos + error;
	out.drawPos = u->drawPos + error;

	if (ViewerRules::InSight(*u)) ReadInSight(*u, out);
	if (out.readAsAlly) ReadOrders(*u, out);
	return true;
}

bool DrawView::UnitDrawState(int id, ObjectDrawState& out) const {
	const CUnit* u = SeenUnit(id);
	if (u == nullptr) return false;
	out.drawMask = u->engineDrawMask;
	out.drawFlag = u->drawFlag;
	out.noDraw = u->noDraw;
	out.inVoid = u->IsInVoid();
	out.icon = u->GetIsIcon();
	return true;
}

bool DrawView::UnitRulesParam(int id, const std::string& name, float& out) const {
	const CUnit* u = SeenUnit(id);
	if (u == nullptr) return false;
	const LuaRulesParams::Param* p = ReadableParam(*u, name);
	if (p == nullptr) return false;
	if (const float* number = std::get_if<float>(&p->value); number != nullptr) {
		out = *number;
		return true;
	}
	if (const bool* flag = std::get_if<bool>(&p->value); flag != nullptr) {
		out = *flag ? 1.0f : 0.0f;
		return true;
	}
	return false;
}

bool DrawView::UnitRulesParamText(int id, const std::string& name, std::string& out) const {
	const CUnit* u = SeenUnit(id);
	if (u == nullptr) return false;
	const LuaRulesParams::Param* p = ReadableParam(*u, name);
	if (p == nullptr) return false;
	const std::string* text = std::get_if<std::string>(&p->value);
	if (text == nullptr) return false;
	out = *text;
	return true;
}

} // namespace KT1
