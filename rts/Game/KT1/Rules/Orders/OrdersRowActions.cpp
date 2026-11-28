/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 ORDERS: what OrderRules' native rows do to Orders' state (LUA-FREE S11, S14; OrdersInternal.h): the fire state,
// the self-destruct countdown, the ferry route, the dive toggle, the reclaim record and the transport gates, as
// ord_orders.lua has them. State is written whenever system Orders holds it; the engine only when Check::MayWrite.
// Not carried over: the ferry order's acknowledgement mark (SendToUnsynced "order_ack", fx_strategic.lua), and the
// no-blast mark on a removed marker (death_blast.lua; the marker has no death blast).

#include <algorithm>
#include <array>
#include <string>
#include <utility>

#include "OrdersInternal.h"
#include "Game/KT1/Core/Orders/QueueHandling.h"
#include "Game/KT1/Core/Orders/QueueRules.h"
#include "Game/KT1/Host/Check.h"
#include "Lua/LuaRulesParams.h"
#include "Map/Ground.h"
#include "Sim/Misc/TeamHandler.h"
#include "Sim/MoveTypes/MoveType.h"
#include "Sim/Units/CommandAI/Command.h"
#include "Sim/Units/CommandAI/CommandAI.h"
#include "Sim/Units/CommandAI/CommandDescription.h"
#include "Sim/Units/Unit.h"
#include "Sim/Units/UnitDef.h"
#include "Sim/Units/UnitDefHandler.h"
#include "Sim/Units/UnitHandler.h"
#include "Sim/Units/UnitLoader.h"
#include "System/SpringHash.h"

namespace KT1::Orders::Internal {

namespace CO = Core::Orders;

namespace {

constexpr const char* FERRY_MARKER = "ord_ferry_marker";   // DESIGN: the game's unit at a route's start
constexpr size_t END_X = 3, END_Y = 4, END_Z = 5;          // DESIGN: the route's end in a ferry order
constexpr size_t POINT_Z = 2;                              // DESIGN: a point order is x, y, z
constexpr char DIVE_KEY[] = "submerged";                   // DESIGN: the move type's member (engine fact)

using Slot = Core::DenseIndex::Slot;

// the unit's slot in Orders' state, added when new; false: no state, or no such unit
bool Tracked(int unit, CO::State*& state, Slot& slot) {
	state = State();
	const CUnit* u = unitHandler.GetUnit(unit);
	if (state == nullptr || u == nullptr) return false;
	slot = state->Track(unit, u->unitDef->id);
	return true;
}

void SetParam(CSolidObject& object, const char* name, float value, int los = LuaRulesParams::RULESPARAMLOS_PRIVATE) {
	LuaRulesParams::Param& p = object.modParams[name];   // as Spring.SetUnitRulesParam writes them
	p.los = los;
	p.value.emplace<float>(value);
}

// the first parameter of the unit's button with this command id: what Spring.EditUnitCmdDesc changes
void SetButton(CUnit& u, int commandID, const std::string& first) {
	const auto& buttons = u.commandAI->GetPossibleCommands();
	for (size_t i = 0; i < buttons.size(); ++i) {
		if (buttons[i]->id != commandID || buttons[i]->params.empty()) continue;
		SCommandDescription changed = *buttons[i];
		changed.params[0] = first;
		u.commandAI->UpdateCommandDescription(i, std::move(changed));   // KT1 lint: allow direct-write callers ask MayWrite
		return;
	}
}

int EngineFire(CO::FireState state) {
	const CO::FireConfig& fire = Config().fire;
	if (state == CO::FireState::Hold) return fire.engine_hold;
	return (state == CO::FireState::Ground) ? fire.engine_ground : fire.engine_free;
}

void Countdown(int unit, CO::State& state, Slot slot, CO::Step at) {
	state.selfDestruct[slot] = at;
	if (at == CO::NO_STEP) state.selfDestructUnits.Erase(unit);
	else state.selfDestructUnits.Insert(unit);
	CUnit* u = unitHandler.GetUnit(unit);
	if (u != nullptr && Check::MayWrite()) SetParam(*u, "selfd_frame", (at == CO::NO_STEP) ? 0.0f : static_cast<float>(at));
}

// the marker of a new route, or null at the team's unit limit
CUnit* MakeMarker(const float3& pos, int team) {
	const UnitDef* def = unitDefHandler->GetUnitDefByName(FERRY_MARKER);
	if (def == nullptr || !unitHandler.CanBuildUnit(def, team) || !Check::MayWrite()) return nullptr;
	UnitLoadParams params;
	params.unitDef = def;
	params.builder = nullptr;
	params.pos = pos;
	params.speed = ZeroVector;
	params.unitID = -1;
	params.teamID = team;
	params.facing = 0;   // DESIGN: south
	params.beingBuilt = false;
	params.flattenGround = true;
	return unitLoader->LoadUnit(params);   // KT1 lint: allow direct-write MayWrite is asked above
}

bool LaterAboard(const std::vector<CO::UnitId>& order, int transport, int cargo) {
	const auto at = std::find(order.begin(), order.end(), cargo);
	if (at == order.end()) return false;
	return std::any_of(at + 1, order.end(), [transport](CO::UnitId other) {
		const CUnit* u = unitHandler.GetUnit(other);
		return u != nullptr && u->GetTransporter() != nullptr && u->GetTransporter()->id == transport;
	});
}

// ORD-007 R2: the set-down point is too far from the point of a one-point unload order
bool OffDrop(const CUnit& transport, float x, float z, float accuracy) {
	const auto& queue = transport.commandAI->commandQue;
	if (queue.empty()) return false;
	const ::Command& c = queue.front();
	if (c.GetID() != CMD_UNLOAD_UNIT || c.GetNumParams() <= POINT_Z) return false;
	return CO::OffDropPoint(x, z, c.GetParam(0), c.GetParam(POINT_Z), accuracy);
}

} // namespace

void SetFireState(int unit, CO::FireState fire) {
	CO::State* state = nullptr;
	Slot slot{};
	const bool tracked = Tracked(unit, state, slot);
	if (tracked) state->fireState[slot] = static_cast<uint8_t>(fire);
	CUnit* u = unitHandler.GetUnit(unit);
	if (u == nullptr || !Check::MayWrite()) return;
	LuaRulesParams::Param& name = u->modParams["fire_state"];
	name.value.emplace<std::string>(CO::FireStateName(fire));
	const std::array<float, 1> value{static_cast<float>(EngineFire(fire))};
	if (tracked) state->settingFire = true;
	Give(unit, CMD_FIRE_STATE, value, INTERNAL_ORDER);
	if (tracked) state->settingFire = false;
	SetButton(*u, CO::CUSTOM_FIRE_STATE, std::to_string(static_cast<int>(fire)));
}

void NoteGiven(int unit, const ::Command& command) {
	CO::State* state = nullptr;
	Slot slot{};
	if (!Tracked(unit, state, slot)) return;
	std::array<float, CO::ISSUE_PARAMS> params{};
	const size_t n = std::min<size_t>(command.GetNumParams(), params.size());
	for (size_t i = 0; i < n; ++i) params[i] = command.GetParam(i);
	CO::RecordGiven(state->given[slot], KindOf(command.GetID()), std::span<const float>(params.data(), n),
		(command.GetOpts() & SHIFT_KEY) != 0);
}

void ToggleSelfDestruct(int unit, int frame) {
	CO::State* state = nullptr;
	Slot slot{};
	if (!Tracked(unit, state, slot)) return;
	const bool running = state->selfDestruct[slot] != CO::NO_STEP;
	Countdown(unit, *state, slot, running ? CO::NO_STEP : CO::SelfDestructAt(frame, Config().selfdestruct_delay_s));
}

void CancelSelfDestruct(int unit) {
	CO::State* state = nullptr;
	Slot slot{};
	if (Tracked(unit, state, slot) && state->selfDestruct[slot] != CO::NO_STEP) Countdown(unit, *state, slot, CO::NO_STEP);
}

void SetFerryRoute(int transport, std::span<const float> params, int team, int) {
	CO::State* state = nullptr;
	Slot slot{};
	if (params.size() < CO::FERRY_PARAMS || !Tracked(transport, state, slot)) return;
	const float3 start(params[0], CGround::GetHeightReal(params[0], params[POINT_Z], true), params[POINT_Z]);
	CUnit* marker = state->ferry[slot].active ? unitHandler.GetUnit(state->ferry[slot].marker) : nullptr;
	if (marker != nullptr && !marker->isDead) {
		if (Check::MayWrite()) marker->ForcedMove(start);   // KT1 lint: allow direct-write MayWrite is asked
	} else {
		EndFerry(transport);
		marker = MakeMarker(start, team);
	}
	if (marker == nullptr) return;
	CO::FerryRoute route;
	route.active = true;
	route.marker = marker->id;
	route.ex = params[END_X];
	route.ey = params[END_Y];
	route.ez = params[END_Z];
	state->ferry[slot] = std::move(route);
	state->ferryUnits.Insert(transport);
	if (!Check::MayWrite()) return;
	// the route for the order lines (hud_orderribbons.lua); drawing only
	constexpr int ALLIED = LuaRulesParams::RULESPARAMLOS_ALLIED;
	SetParam(*marker, "ferry_transport", static_cast<float>(transport), ALLIED);
	SetParam(*marker, "ferry_ex", params[END_X], ALLIED);
	SetParam(*marker, "ferry_ez", params[END_Z], ALLIED);
	state->givingFerryOrder = true;
	Give(transport, CMD_STOP, {}, 0);
	state->givingFerryOrder = false;
}

void EndFerry(int transport) {
	CO::State* state = State();
	if (state == nullptr) return;
	const Slot slot = state->SlotOf(transport);
	state->ferryUnits.Erase(transport);
	if (slot == Core::DenseIndex::NONE || !state->ferry[slot].active) return;
	CUnit* marker = unitHandler.GetUnit(state->ferry[slot].marker);
	state->ferry[slot] = CO::FerryRoute{};
	// removed, not destroyed: no wreck
	if (marker != nullptr && !marker->isDead && Check::MayWrite())
		marker->ForcedKillUnit(nullptr, false, true, -CSolidObject::DAMAGE_KILLED_LUA);   // KT1 lint: allow direct-write MayWrite is asked
}

void SetDive(int unit, bool submerged) {
	CUnit* u = unitHandler.GetUnit(unit);
	if (u == nullptr || !Check::MayWrite()) return;
	bool value = submerged;
	// as Spring.MoveCtrl.SetGroundMoveTypeData
	u->moveType->SetMemberValue(spring::LiteHash(DIVE_KEY, sizeof(DIVE_KEY) - 1, 0), &value);   // KT1 lint: allow direct-write MayWrite is asked above
	SetButton(*u, CO::CUSTOM_DIVE, submerged ? "1" : "0");
}

void NoteLivingReclaim(int builder, int target) {
	CO::State* state = nullptr;
	Slot slot{};
	if (!Tracked(builder, state, slot)) return;
	state->livingReclaim[slot] = target;
	state->reclaimUnits.Insert(builder);
}

bool AskTransportLoad(const CUnit& transport, const CUnit& cargo, bool engineAllows, int frame) {
	CO::State* state = nullptr;
	Slot slot{};
	if (!Tracked(transport.id, state, slot)) return engineAllows;
	const CO::DefFacts& facts = Defs()[transport.unitDef->id];
	// a transport with a load height waits until the engine says it is docked (MOV-015-A)
	if (!engineAllows && facts.Has(CO::DefFlag::AirLoadHeight)) {
		state->loading[slot] = CO::GateClock{};
		return false;
	}
	const CO::TransportParams p = CO::MakeTransportParams(Config());
	const CO::TransportTimes times = CO::TimesFor(Config(), facts);
	const CO::Step start = CO::OrderStart(state->orderClock[slot], CO::ClockKind::Load, frame, facts.Has(CO::DefFlag::UnloadAll));
	if (!CO::AskLoad(state->loading[slot], cargo.id, frame, start, times.loadS, p)) return false;
	state->orderClock[slot] = CO::OrderClock{};
	return true;
}

bool AskTransportUnload(const CUnit& transport, const CUnit& cargo, float x, float z, bool engineAllows, int frame) {
	CO::State* state = nullptr;
	Slot slot{};
	if (!Tracked(transport.id, state, slot)) return engineAllows;
	const CO::DefFacts& facts = Defs()[transport.unitDef->id];
	if (!engineAllows && facts.Has(CO::DefFlag::AirLoadHeight)) {
		state->unloading[slot] = CO::GateClock{};
		return false;
	}
	const CO::TransportParams p = CO::MakeTransportParams(Config());
	if (p.reverseOrder && LaterAboard(state->loadOrder[slot], transport.id, cargo.id)) {
		if (CO::Holding(state->reverseHold[slot], frame, p.reverseHold, p)) return false;
	} else {
		state->reverseHold[slot] = CO::HoldClock{};
	}
	const CO::TransportTimes times = CO::TimesFor(Config(), facts);
	if (OffDrop(transport, x, z, times.dropAccuracy)) {
		if (CO::Holding(state->dropHold[slot], frame, p.dropHold, p)) {
			state->unloading[slot] = CO::GateClock{};   // the set-down clock starts on the point
			return false;
		}
	} else {
		state->dropHold[slot] = CO::HoldClock{};
	}
	const CO::Step start = CO::OrderStart(state->orderClock[slot], CO::ClockKind::Unload, frame, facts.Has(CO::DefFlag::UnloadAll));
	if (!CO::AskUnload(state->unloading[slot], cargo.id, frame, start, times.unloadS, times.intervalS, p)) return false;
	if (times.intervalS < 0) state->orderClock[slot] = CO::OrderClock{};
	return true;
}

} // namespace KT1::Orders::Internal
