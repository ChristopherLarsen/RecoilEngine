/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 OrderRules: the rows of AllowCommand (LUA-FREE S11 refusals, S14 rewritten orders). Each test is the replaced
// handler's, in Core/OrderRules/GuardLogic.h or Core/Orders; the comments name what differs.

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

#include "OrderRulesAdapter.h"
#include "Game/KT1/Core/Orders/QueueHandling.h"
#include "Game/KT1/Host/Check.h"
#include "Game/KT1/Rules/Orders/OrdersInternal.h"
#include "Lua/LuaRulesParams.h"
#include "Map/Ground.h"
#include "Map/ReadMap.h"
#include "Sim/Features/Feature.h"
#include "Sim/Features/FeatureDef.h"
#include "Sim/Misc/GlobalConstants.h"
#include "Sim/Misc/QuadField.h"
#include "Sim/Misc/TeamHandler.h"
#include "Sim/MoveTypes/SupComGroundMoveType.h"
#include "Sim/Units/CommandAI/Command.h"
#include "Sim/Units/Unit.h"
#include "Sim/Units/UnitDef.h"
#include "Sim/Units/UnitHandler.h"
#include "System/float3.h"

namespace KT1::OrderRules {
namespace {

using Core::Orders::CommandKind;
using Core::Orders::DefFlag;
using Core::Orders::FireState;
using Rows::CompiledRow;
using Rows::Condition;

namespace Internal = Orders::Internal;

constexpr size_t POINT = 3;          // DESIGN: parameters of a point order
constexpr size_t AREA = 4;           // DESIGN: parameters of an area order (point and radius)
constexpr size_t X = 0, Y = 1, Z = 2;   // DESIGN: the parameters of a point order

bool IsPoint(const ::Command& c) { return c.GetNumParams() >= POINT; }
bool Shifted(const ::Command& c) { return (c.GetOpts() & SHIFT_KEY) != 0; }

// the one unit an order names, or null
const CUnit* Target(const ::Command& c) {
	if (c.GetNumParams() != 1) return nullptr;
	const float p = c.GetParam(X);
	if (p < 0 || p >= unitHandler.MaxUnits()) return nullptr;
	return unitHandler.GetUnit(static_cast<unsigned>(p));
}

bool Allied(const CUnit& a, const CUnit& b) { return teamHandler.AlliedTeams(a.team, b.team); }
float Elmos(float mapUnits) { return mapUnits * Internal::Config().elmos_per_mu; }
float Ground(float x, float z) { return CGround::GetHeightReal(x, z, true); }

const Core::Orders::DefFacts* FactsOf(const CUnit& u) {
	const auto& defs = Internal::Defs();
	const int id = u.unitDef->id;
	return (id > 0 && id < static_cast<int>(defs.size())) ? &defs[id] : nullptr;
}

// the unit's slot in Orders' state, added when new; null while system Orders is off
Core::Orders::State* Tracked(const CUnit& u, Core::DenseIndex::Slot& slot) {
	Core::Orders::State* state = Internal::State();
	if (state != nullptr) slot = state->Track(u.id, u.unitDef->id);
	return state;
}

FireState CurrentFire(const CUnit& u) {
	FireState state = FireState::Free;
	const auto it = u.modParams.find("fire_state");
	if (it != u.modParams.end()) {
		if (const std::string* name = std::get_if<std::string>(&it->second.value)) Core::Orders::ParseFireState(*name, state);
	}
	return state;
}

// mov_air_unload_gap: a ground unit (not the transport, not its cargo) holds the spot
bool Held(const CUnit& transport, float x, float z, float query) {
	float cargoRadius = 0;
	for (const auto& t : transport.transportedUnits) cargoRadius = std::max(cargoRadius, t.unit->radius);
	QuadFieldQuery found;
	quadField.GetUnitsExact(found, float3(x - query, 0, z - query), float3(x + query, 0, z + query));
	const float3 spot(x, 0, z);
	for (const CUnit* u : *found.units) {
		if (u == &transport || u->GetTransporter() == &transport || u->unitDef->canfly) continue;
		if (u->midPos.SqDistance2D(spot) > query * query) continue;
		if (Rows::HoldsSpot(u->pos.x, u->pos.z, u->radius, x, z, cargoRadius)) return true;
	}
	return false;
}

bool UnloadGap(Table& table, const CompiledRow& row, const CommandAsk& ask) {
	const ::Command& c = ask.command;
	const float x = c.GetParam(X), z = c.GetParam(Z);
	if (!Held(ask.unit, x, z, table.config.values.unload_query_elmo)) return true;
	Rows::UnloadPoints points;
	if (!Rows::PlanUnloadPoints(ask.unit.pos.x, ask.unit.pos.z, x, z, ask.def.landShortGap, ask.def.cargoShortGap,
			table.config.values.unload_same_elmo, points))
		return true;
	std::array<float, Core::Orders::ISSUE_PARAMS> params{};
	const size_t n = std::min<size_t>(c.GetNumParams(), params.size());
	for (size_t i = 0; i < n; ++i) params[i] = c.GetParam(i);
	params[X] = points.landX;
	params[Y] = Ground(points.landX, points.landZ);
	params[Z] = points.landZ;
	const auto slot = table.state.Track(ask.unit.id);
	table.state.unload[slot] = {true, points.cargoX, points.cargoZ};
	Reissue(table, row, ask.unit.id, c.GetID(), std::span<const float>(params.data(), n), c.GetOpts());
	return false;
}

bool GoalCell(Table& table, const CompiledRow& row, const CommandAsk& ask) {
	const ::Command& c = ask.command;
	const Rows::GuardValues& values = table.config.values;
	const float x = c.GetParam(X), y = c.GetParam(Y), z = c.GetParam(Z);
	if (values.goal_cell_mu <= 0) return true;   // a data set without the value: no rule
	float fx = ask.unit.pos.x, fz = ask.unit.pos.z;
	if (Shifted(c)) {
		std::vector<Core::Orders::OrderView> queue;
		Internal::ReadQueue(ask.unit.id, -1, queue);
		for (auto it = queue.rbegin(); it != queue.rend(); ++it) {
			if (it->kind != CommandKind::Move) continue;
			fx = it->params[X];
			fz = it->params[Z];
			break;
		}
	}
	float rx = 0, rz = 0;
	Rows::RestPoint(x, z, fx, fz, Elmos(values.goal_cell_mu), Elmos(values.rest_short_mu), rx, rz);
	if (std::abs(rx - x) < values.goal_same_elmo && std::abs(rz - z) < values.goal_same_elmo) return true;
	Core::DenseIndex::Slot slot{};
	if (Core::Orders::State* state = Tracked(ask.unit, slot); state != nullptr)
		Core::Orders::AddGoalCell(state->goalCells[slot], {rx, rz, x, y, z}, values.goal_records);
	const std::array<float, POINT> point{rx, Ground(rx, rz), rz};
	Reissue(table, row, ask.unit.id, CMD_MOVE, point, c.GetOpts());
	return false;
}

// ord_edge R3: one reclaim for each wreck in the circle, nearest first
bool AreaReclaim(Table& table, const CompiledRow& row, const CommandAsk& ask) {
	const ::Command& c = ask.command;
	QuadFieldQuery found;
	quadField.GetFeaturesExact(found, float3(c.GetParam(X), 0, c.GetParam(Z)), c.GetParam(POINT), false);
	std::vector<Rows::AreaFeature> wrecks;
	for (const CFeature* f : *found.features) {
		if (!f->def->reclaimable) continue;
		const float dx = f->pos.x - ask.unit.pos.x, dz = f->pos.z - ask.unit.pos.z;
		wrecks.push_back({f->id, dx * dx + dz * dz});
	}
	Rows::SortAreaFeatures(wrecks);
	for (size_t i = 0; i < wrecks.size(); ++i) {
		const std::array<float, 1> param{static_cast<float>(wrecks[i].feature + unitHandler.MaxUnits())};
		const std::uint8_t options = (i == 0 && !Shifted(c)) ? 0 : SHIFT_KEY;
		Reissue(table, row, ask.unit.id, CMD_RECLAIM, param, options);
	}
	return false;
}

bool KnownTarget(Table& table, const CompiledRow& row, const CommandAsk& ask) {
	const ::Command& c = ask.command;
	const CUnit* target = Target(c);
	if (target == nullptr || !Rows::KnownNotSeen(target->losStatus[ask.unit.allyteam])) return true;
	const float dx = target->pos.x - ask.unit.pos.x, dz = target->pos.z - ask.unit.pos.z;
	if (!Rows::KnownTargetInRange(ask.unit.maxRange, dx * dx + dz * dz)) return true;
	const std::array<float, POINT> point{target->pos.x, target->pos.y, target->pos.z};
	Reissue(table, row, ask.unit.id, CMD_ATTACK, point, c.GetOpts());
	return false;
}

bool ShortOrder(Table& table, const CompiledRow& row, const CommandAsk& ask) {
	const ::Command& c = ask.command;
	const float min = Elmos(table.config.values.short_order_mu);
	if (!IsPoint(c) || Shifted(c) || min <= 0) return true;
	if (!Rows::ShortMove(c.GetParam(X) - ask.unit.pos.x, c.GetParam(Z) - ask.unit.pos.z, min)) return true;
	Reissue(table, row, ask.unit.id, CMD_STOP, {}, 0);
	return false;
}

bool ShiftKeep(Table& table, const CompiledRow& row, const CommandAsk& ask) {
	const ::Command& c = ask.command;
	if (!IsPoint(c) || !Shifted(c)) return true;
	std::vector<Core::Orders::OrderView> queue;
	Internal::ReadQueue(ask.unit.id, -1, queue);
	const float x = c.GetParam(X), z = c.GetParam(Z);
	if (!Rows::NearQueuedMove(queue, x, z, table.config.values.cancel_distance_elmo)) return true;
	// at the end of the queue (-1), where an insert cancels nothing
	const std::array<float, 6> params{-1.0f, static_cast<float>(CMD_MOVE), static_cast<float>(c.GetOpts()), x, c.GetParam(Y), z};
	Reissue(table, row, ask.unit.id, CMD_INSERT, params, ALT_KEY);
	return false;
}

// ---- the rows that only answer ---------------------------------------------------------------------------------------

bool Refuses(const CompiledRow& row, const CommandAsk& ask) {
	const ::Command& c = ask.command;
	const CUnit& u = ask.unit;
	const bool factoryRally = ask.def.Has(DefFlag::Factory) && ask.kind == CommandKind::Fight && IsPoint(c);
	switch (row.condition) {
		case Condition::Always: return true;
		case Condition::BeyondMap:
			return IsPoint(c) && Rows::BeyondMap(c.GetParam(X), c.GetParam(Z),
				mapDims.mapx * SQUARE_SIZE, mapDims.mapy * SQUARE_SIZE);
		case Condition::EnemyUnseen: {
			const CUnit* t = Target(c);
			return t != nullptr && Rows::EnemyUnseen(Allied(*t, u), true, t->losStatus[u.allyteam]);
		}
		case Condition::EnemyTarget: {
			const CUnit* t = Target(c);
			return t != nullptr && !Allied(*t, u);
		}
		case Condition::UnreclaimableTarget: {
			const CUnit* t = Target(c);
			const Core::Orders::DefFacts* facts = (t != nullptr) ? FactsOf(*t) : nullptr;
			return facts != nullptr && Rows::Unreclaimable(u.id, t->id, *facts);
		}
		case Condition::NotAccepted: return !Rows::PackAccepts(ask.def, ask.kind, factoryRally);
		case Condition::NotInDefAccepts: return !Rows::DefAccepts(ask.def, ask.kind);
		// ord_orders.lua tests the player number as "synced", so it never refuses; the correction waits for a ruling
		case Condition::MoveStateFromPlayer: return false;
		case Condition::DiveNotSubmarine: return !ask.def.Has(DefFlag::Submarine);
		case Condition::Carried: return u.GetTransporter() != nullptr;
		case Condition::FerryNotTransport:
			return !ask.def.Has(DefFlag::Transport) || c.GetNumParams() < Core::Orders::FERRY_PARAMS;
		default: return false;
	}
}

} // namespace

void Reissue(Table& table, const CompiledRow& row, int unit, int commandID, std::span<const float> params,
             std::uint8_t options) {
	if (Check::InCheck(NAME)) {
		const float first = params.empty() ? 0.0f : params[X];
		Check::Value v;
		v.v[0] = first;
		v.v[1] = (params.size() >= POINT) ? params[Z] : 0.0f;
		v.count = Check::Value::BOTH;
		// the first parameter tells the several orders of one call apart
		Check::Native(NAME, "reissued", unit, commandID, static_cast<int>(std::floor(first)), v);
	}
	table.state.Track(unit);
	table.state.SetReissuing(unit, row.source, true);
	Internal::Give(unit, commandID, params, options);
	table.state.SetReissuing(unit, row.source, false);
}

bool AskCommandRow(Table& table, const CompiledRow& row, const CommandAsk& ask) {
	const ::Command& c = ask.command;
	const CUnit& u = ask.unit;
	switch (row.condition) {
		case Condition::AttackMovePoint: {
			if (!IsPoint(c)) return true;
			const bool armed = Rows::AttackMoveRewrite(ask.def) == CommandKind::AMove;
			const std::array<float, POINT> point{c.GetParam(X), c.GetParam(Y), c.GetParam(Z)};
			Reissue(table, row, u.id, armed ? Core::Orders::CUSTOM_AMOVE : CMD_MOVE, point, c.GetOpts());
			return false;
		}
		case Condition::RecordGiven:
			Internal::NoteGiven(u.id, c);
			return true;
		case Condition::EngineFireState: {
			const Core::Orders::State* state = Internal::State();
			if (state != nullptr && state->settingFire) return true;
			const int value = (c.GetNumParams() > 0) ? static_cast<int>(c.GetParam(X)) : -1;
			Internal::SetFireState(u.id, Rows::FireFromEngine(value, Internal::Config().fire.engine_hold));
			return false;
		}
		case Condition::GameFireState: {
			const bool has = c.GetNumParams() > 0;
			Internal::SetFireState(u.id, Rows::NextFireState(CurrentFire(u), has, has ? c.GetParam(X) : 0.0f));
			return false;
		}
		case Condition::SelfDestruct:
			Internal::ToggleSelfDestruct(u.id, ask.frame);
			return false;
		case Condition::CancelsSelfDestruct:
			Internal::CancelSelfDestruct(u.id);
			return true;
		case Condition::DiveToggle: {
			const auto* sub = dynamic_cast<const CSupComGroundMoveType*>(u.moveType);
			const bool has = c.GetNumParams() > 0;
			Internal::SetDive(u.id, Rows::DiveWanted(has, has ? c.GetParam(X) : 0.0f,
				sub != nullptr && sub->IsSubmergedOrdered()));
			return false;
		}
		case Condition::FerryRoute: {
			std::array<float, Core::Orders::FERRY_PARAMS> params{};
			const size_t n = std::min<size_t>(c.GetNumParams(), params.size());
			for (size_t i = 0; i < n; ++i) params[i] = c.GetParam(i);
			Internal::SetFerryRoute(u.id, std::span<const float>(params.data(), n), u.team, ask.frame);
			return false;
		}
		case Condition::EndsFerry: {
			const Core::Orders::State* state = Internal::State();
			if (state == nullptr || state->givingFerryOrder) return true;
			const auto slot = state->SlotOf(u.id);
			if (slot != Core::DenseIndex::NONE && state->ferry[slot].active) Internal::EndFerry(u.id);
			return true;
		}
		case Condition::HeldIdleMode: {
			const auto slot = table.state.SlotOf(u.id);
			if (table.state.settingIdleMode || slot == Core::DenseIndex::NONE || !table.state.idleAir[slot].held) return true;
			if (c.GetNumParams() > 0) table.state.idleAir[slot].mode = static_cast<int>(c.GetParam(X));
			return false;
		}
		case Condition::OccupiedUnloadSpot: return !IsPoint(c) || UnloadGap(table, row, ask);
		case Condition::GoalCell: return !IsPoint(c) || GoalCell(table, row, ask);
		case Condition::AreaReclaim: return c.GetNumParams() != AREA || AreaReclaim(table, row, ask);
		case Condition::IndirectKnownTarget: return KnownTarget(table, row, ask);
		case Condition::ShortMove: return ShortOrder(table, row, ask);
		case Condition::NearQueuedMove: return ShiftKeep(table, row, ask);
		default: break;
	}
	if (!Refuses(row, ask)) return true;
	const std::string& reason = table.config.rows[row.source].reason;
	// a carried unit's own internal orders go silently
	const bool silent = reason.empty() || (row.condition == Condition::Carried && c.IsInternalOrder());
	if (!silent) Internal::Refuse(u.id, reason.c_str(), ask.frame);
	return false;
}

} // namespace KT1::OrderRules
