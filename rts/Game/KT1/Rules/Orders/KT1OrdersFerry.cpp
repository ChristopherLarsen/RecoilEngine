/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 ORDERS, step S12: the self-destruct countdown pass and the ferry pass of ord_orders.lua's GameFrame, as the Lua has
// them (LUA-FREE S12; port, then correct). Orders' PHASE_PRE_TURN function calls them, in the Lua's order: dropped
// reclaims, RunSelfDestructs, move tracking, RunFerries (then the waypoint insert at PHASE_PRE_TURN + 10).
// Core: Core/Orders/QueueHandling.cpp. The route set-up and EndFerry are ORDERS C's (OrdersRowActions.cpp).
//
// Open (cross-family): the Lua asks GLUE's spc_cargo.lua (GG.SpcCargo.SelectLoad, Full, NoteRefused) when it is loaded.
// Native code cannot call it yet, so this pass takes the Lua's own fallback (one unit at a time, never "full", no note)
// until GLUE gives a native call. With spc_cargo loaded the ferry therefore differs from the Lua: see orders-s12-requirements.md.

#include <algorithm>
#include <vector>

#include "OrdersInternal.h"

#include "Game/KT1/Core/Orders/QueueHandling.h"
#include "Game/KT1/Host/Fakes.h"
#include "Game/KT1/Host/SimContext.h"
#include "Sim/Misc/QuadField.h"
#include "Sim/Units/CommandAI/Command.h"
#include "Sim/Units/CommandAI/CommandAI.h"
#include "Sim/Units/Unit.h"
#include "Sim/Units/UnitHandler.h"

namespace KT1::Orders::Internal {

namespace CO = Core::Orders;

namespace {

constexpr int GUARD_READ = 4;            // DESIGN (the Lua's Spring.GetUnitCommands(u, 4)): entries read for the guard order
constexpr size_t UNLOAD_ONE = 1;         // DESIGN (ORD-007 R5): a lone unit is set down at the end point
constexpr int POINT = 3;                 // DESIGN (engine fact): x, y, z

const CUnit* U(int id) { return (id < 0) ? nullptr : unitHandler.GetUnit(id); }

// Lua FerryOrder(t, cmdID, params, opts): an order the EndsFerry row lets through
void FerryOrder(CO::State& s, int t, int cmd, std::span<const float> params, std::uint8_t opts) {
	s.givingFerryOrder = true;
	Give(t, cmd, params, opts);
	s.givingFerryOrder = false;
}

// Lua Unload(t, fr, n)
void Unload(CO::State& s, int t, const CO::FerryRoute& fr, size_t n, const CO::FerryParams& p) {
	if (n == UNLOAD_ONE) {
		const float q[] = {fr.ex, fr.ey, fr.ez};
		FerryOrder(s, t, CMD_UNLOAD_UNIT, q, 0);
	} else {
		const float q[] = {fr.ex, fr.ey, fr.ez, p.dropRadius};
		FerryOrder(s, t, CMD_UNLOAD_UNITS, q, 0);
	}
}

// Lua: the guard order on the marker among the first four entries
bool GuardsMarker(const CUnit* u, int marker) {
	int n = 0;
	for (const Command& c : u->commandAI->commandQue) {
		if (n++ >= GUARD_READ) break;
		if (c.GetID() == CMD_GUARD && c.GetNumParams() >= 1 && static_cast<int>(c.GetParam(0)) == marker) return true;
	}
	return false;
}

// Lua: Spring.GetUnitsInCylinder(mx, mz, FERRY_GATHER, team) filtered to guards of the marker not carried, ascending
void Guarding(int team, int marker, float mx, float mz, float radius, std::vector<CO::UnitId>& out) {
	out.clear();
	const float r2 = radius * radius;
	const float3 c(mx, 0.0f, mz);
	QuadFieldQuery q;
	quadField.GetUnitsExact(q, float3(mx - radius, 0.0f, mz - radius), float3(mx + radius, 0.0f, mz + radius));
	for (const CUnit* u : *q.units) {
		if (u->team != team || Fakes::Is(u) || u->midPos.SqDistance2D(c) > r2) continue;
		if (GuardsMarker(u, marker) && u->GetTransporter() == nullptr) out.push_back(u->id);
	}
	std::sort(out.begin(), out.end());
}

// Lua Ferry(t, fr)
void Ferry(CO::State& s, int t, CO::FerryRoute& fr, int f, const CO::FerryParams& p) {
	const CUnit* tr = U(t);
	const CUnit* marker = U(fr.marker);
	if (tr == nullptr) return;
	if (marker == nullptr) { EndFerry(t); return; }
	const float mx = marker->pos.x, my = marker->pos.y, mz = marker->pos.z;
	std::vector<CO::UnitId> guarding;
	Guarding(tr->team, fr.marker, mx, mz, p.gather, guarding);
	CO::NoteWaiting(fr, guarding, f);
	const CCommandQueue& q = tr->commandAI->commandQue;
	std::vector<CO::UnitId> carried;
	for (const CUnit::TransportedUnit& tu : tr->transportedUnits) carried.push_back(tu.unit->id);
	switch (CO::CheckTrip(fr, q.empty(), static_cast<int32_t>(carried.size()))) {
		case CO::TripState::Busy: return;
		case CO::TripState::Unload: Unload(s, t, fr, carried.size(), p); return;
		case CO::TripState::Done:
			for (const CO::UnitId c : fr.trip) {
				const CUnit* cu = U(c);
				if (cu != nullptr && cu->GetTransporter() == nullptr) Give(c, CMD_STOP, {}, 0);
			}
			fr.trip.clear();
			fr.onTrip = false;
			break;
		case CO::TripState::NotOnTrip: break;
	}
	if (!q.empty() && q.front().GetID() == CMD_LOAD_UNITS) return;
	// the Lua's fallback without spc_cargo: one waiting unit, when the hold is empty
	if (carried.empty() && !fr.waiting.empty()) {
		const float u[] = {static_cast<float>(fr.waiting.front())};
		FerryOrder(s, t, CMD_LOAD_UNITS, u, 0);
		return;
	}
	if (!carried.empty() && CO::LeaveDue(fr, static_cast<int32_t>(carried.size()), false, f, p)) {
		CO::BeginTrip(fr, carried, f);
		Unload(s, t, fr, carried.size(), p);
		return;
	}
	if (!q.empty()) return;
	if (CO::ReturnNeeded(tr->pos.x, tr->pos.z, mx, mz, p)) {
		const float to[POINT] = {mx, my, mz};
		FerryOrder(s, t, CMD_MOVE, to, 0);
	}
}

} // namespace

// Lua: for u, at in SortedPairs(selfd) do if f >= at then selfd[u] = nil; DestroyUnit(u, true) (with its explosion)
void RunSelfDestructs(const SimContext& c) {
	CO::State* s = State();
	if (s == nullptr || c.check) return;
	const std::vector<int32_t> ids(s->selfDestructUnits.Ids().begin(), s->selfDestructUnits.Ids().end());
	for (const int32_t u : ids) {
		if (!c.ids.Contains(u)) continue;
		const auto slot = s->SlotOf(u);
		if (slot == Core::DenseIndex::NONE) { s->selfDestructUnits.Erase(u); continue; }
		const CO::Step at = s->selfDestruct[slot];
		if (at == CO::NO_STEP || c.frame < at) continue;
		s->selfDestruct[slot] = CO::NO_STEP;
		s->selfDestructUnits.Erase(u);
		CUnit* unit = unitHandler.GetUnit(u);
		if (unit != nullptr && !unit->isDead)
			c.Write([unit]() { unit->ForcedKillUnit(nullptr, true, false, -CSolidObject::DAMAGE_KILLED_LUA); });
	}
}

// Lua: if f % H.Every(0.5) == 0 then for t, fr in SortedPairs(ferries) do Ferry(t, fr) or EndFerry(t)
void RunFerries(const SimContext& c) {
	CO::State* s = State();
	if (s == nullptr || c.check) return;
	const CO::FerryParams p = CO::MakeFerryParams(Config());
	if (c.frame % p.period != 0) return;
	const std::vector<int32_t> ids(s->ferryUnits.Ids().begin(), s->ferryUnits.Ids().end());
	for (const int32_t t : ids) {
		if (!c.ids.Contains(t)) continue;
		const auto slot = s->SlotOf(t);
		if (unitHandler.GetUnit(t) == nullptr || slot == Core::DenseIndex::NONE || !s->ferry[slot].active) { EndFerry(t); continue; }
		Ferry(*s, t, s->ferry[slot], c.frame, p);
	}
}

} // namespace KT1::Orders::Internal
