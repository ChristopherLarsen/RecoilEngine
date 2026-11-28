/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 ORDERS, step S12: the queue handling of system Orders, ported from ord_orders.lua as the Lua has it (LUA-FREE S12;
// port, then correct). Core in Core/Orders/QueueHandling.cpp.
//
//   CommandFallback  the waypoint step (CMD 39401) and the attack-move order (39404: kept while its task runs)
//   UnitCmdDone      a cut move gets a waypoint step (an event client of its own: the call-in needs the command)
//   FrameStart       PHASE_PRE_TURN + 10: the due waypoint steps are inserted in front, ascending unit id, after the
//                    tracking pass and the ferries (the Lua's GameFrame order)
//   Internal         Remove, InsertAhead, SetMoveGoal (OrdersInternal.h)
// Orders' LoadConfig calls RegisterQueueHandling (agreed with S05/S10, 2026-11-28). Check mode: the CommandFallback
// answer goes through CommandHooks; the cut point is reported as "waypoint_due" (unit, x, z); nothing is written.
// Not here: the transport gates, the ferry route and the self-destruct toggle (ORDERS C, OrdersRowActions.cpp), which use
// the same Core.

#include <cmath>
#include <memory>
#include <vector>

#include "OrdersInternal.h"

#include "Game/KT1/Rules/Cargo/CargoApi.h"

#include "Game/KT1/Core/Orders/QueueHandling.h"
#include "Game/KT1/Host/Check.h"
#include "Game/KT1/Host/CommandHooks.h"
#include "Game/KT1/Host/Phases.h"
#include "Game/KT1/Host/SimContext.h"
#include "Sim/Misc/GlobalSynced.h"
#include "Sim/MoveTypes/MoveType.h"
#include "Sim/Units/CommandAI/Command.h"
#include "Sim/Units/CommandAI/CommandAI.h"
#include "Sim/Units/Unit.h"
#include "Sim/Units/UnitDef.h"
#include "Sim/Units/UnitHandler.h"
#include "System/EventClient.h"
#include "System/EventHandler.h"

namespace KT1::Orders {

namespace CO = Core::Orders;

namespace {

constexpr const char* SYSTEM = "Orders";
constexpr int WAYPOINT_PHASE = PHASE_PRE_TURN + 10;   // DESIGN: after the tracking and ferry passes of PHASE_PRE_TURN
constexpr int FALLBACK_ORDER = 0;                     // DESIGN: the family's only CommandFallback
constexpr int CLIENT_ORDER = 19990;                   // DESIGN: after the KT1 event feed, before the Lua handles
constexpr int POINT_PARAMS = 3;                       // DESIGN (engine fact): x, y, z
constexpr int INDEX_Z = POINT_PARAMS - 1;             // DESIGN (engine fact): the z parameter

CO::WaypointParams waypoint;

// the advance radius of the unit's def, or the arrival radius; hasAdvance: the def has its own
float RadiusOf(const CO::State& s, int unit, bool& hasAdvance) {
	const CO::DefFacts* d = s.DefOf(unit);
	hasAdvance = d != nullptr && d->waypointAdvance > 0;
	return hasAdvance ? d->waypointAdvance : waypoint.arrive;
}

std::span<const CO::GoalCellRecord> GoalCells(const CO::State& s, int unit) {
	const auto slot = s.SlotOf(unit);
	if (slot == Core::DenseIndex::NONE) return {};
	return {s.goalCells[slot].data(), s.goalCells[slot].size()};
}

// Lua gadget:CommandFallback
Fallback OnFallback(const CUnit* u, const Command& c) {
	if (c.GetID() == CO::CUSTOM_AMOVE) return Fallback::Keep;
	if (c.GetID() != CO::CUSTOM_WAYPOINT || c.GetNumParams() < POINT_PARAMS) return Fallback::Pass;
	CO::State* s = Internal::State();
	if (s == nullptr) return Fallback::Pass;
	const auto slot = s->Track(u->id, u->unitDef->id);
	if (slot == Core::DenseIndex::NONE) return Fallback::Pass;
	CO::WaypointFacts f;
	f.ux = u->pos.x; f.uz = u->pos.z;
	f.vx = u->speed.x; f.vz = u->speed.z;
	f.x = c.GetParam(0); f.z = c.GetParam(INDEX_Z);
	f.orderedX = f.x; f.orderedZ = f.z;
	CO::GoalCellRecord rec;
	if (CO::FindGoalCell(GoalCells(*s, u->id), f.x, f.z, waypoint.same, rec)) {
		// Lua: off = math.sqrt((r[3] - gx) ^ 2 + (r[5] - gz) ^ 2) gx, gz = r[3], r[5]
		const float dx = rec.x - f.x, dz = rec.z - f.z;
		f.offset = std::sqrt(dx * dx + dz * dz);
		f.orderedX = rec.x; f.orderedZ = rec.z;
	}
	f.radius = RadiusOf(*s, u->id, f.hasAdvance);
	f.now = gs->frameNum;
	const CO::WaypointAnswer a = CO::RunWaypoint(s->waypointStep[slot], f, waypoint);
	if (a.setGoal) Internal::SetMoveGoal(u->id, c.GetParam(0), c.GetParam(1), c.GetParam(INDEX_Z), a.goalRadius);
	return a.finished ? Fallback::Done : Fallback::Keep;
}

// Lua MoreMoves(u, tag): another move among the first four entries
bool MoreMoves(const CUnit* u, uint32_t tag) {
	int n = 0;
	for (const Command& c : u->commandAI->commandQue) {
		if (n++ >= CO::MORE_MOVES_READ) break;
		if (c.GetID() == CMD_MOVE && c.GetTag() != tag) return true;
	}
	return false;
}

// Lua gadget:UnitCmdDone
void OnCmdDone(const CUnit* u, const Command& c) {
	CO::State* s = Internal::State();
	if (s == nullptr) return;
	const auto slot = s->SlotOf(u->id);
	if (slot == Core::DenseIndex::NONE) return;
	// (the Lua's first branch reads supcomRouteEndFrame, which this fork does not report: it never applies here)
	if (c.GetID() == CMD_MOVE && c.GetNumParams() >= POINT_PARAMS) {
		const CO::DefFacts* d = s->DefOf(u->id);
		CO::MoveDoneFacts f;
		f.internal = c.IsInternalOrder();
		f.ownMove = d != nullptr && d->Has(CO::DefFlag::OwnMove);
		f.moreMoves = f.ownMove && MoreMoves(u, c.GetTag());
		bool hasAdvance = false;
		f.radius = RadiusOf(*s, u->id, hasAdvance);
		f.toOrdered = hasAdvance || waypoint.stepAll;
		f.ux = u->pos.x; f.uz = u->pos.z;
		f.x = c.GetParam(0); f.y = c.GetParam(1); f.z = c.GetParam(INDEX_Z);
		CO::WaypointDue due;
		if (CO::PlanWaypointStep(f, GoalCells(*s, u->id), waypoint, due)) {
			if (Check::InCheck(SYSTEM)) {
				Check::Native(SYSTEM, "waypoint_due_x", u->id, 0, 0, due.x);
				Check::Native(SYSTEM, "waypoint_due_z", u->id, 0, 0, due.z);
			} else {
				s->waypointDue[slot] = due;
				s->waypointUnits.Insert(u->id);
			}
		}
	}
	// Lua: local o = ours[u]; if o and o.tag == tag then ours[u] = nil GG.Ord.guardRepair[u] = nil end
	if (!Check::InCheck(SYSTEM) && s->own[slot].tag == c.GetTag() && s->own[slot].kind != CO::OwnKind::None) {
		s->own[slot] = CO::OwnStep{};
		s->guardRepair[slot] = CO::NO_UNIT;
	}
}

class CmdDoneClient final : public CEventClient {
public:
	CmdDoneClient() : CEventClient("[KT1OrdersQueue]", CLIENT_ORDER, true) {}
	bool WantsEvent(const std::string& name) override { return name == "UnitCmdDone"; }
	int GetReadAllyTeam() const override { return AllAccessTeam; }
	void UnitCmdDone(const CUnit* u, const Command& c) override {
		const Check::Running running(SYSTEM, Check::InCheck(SYSTEM));
		OnCmdDone(u, c);
	}
};

// Not a static object: CEventClient's destructor calls the event handler, which may be gone by process exit.
CmdDoneClient* client = nullptr;

// Lua GameFrame: for u, p in SortedPairs(waypointDue) do ... INSERT { 0, CMD_WAYPOINT, OPT_INTERNAL, x, y, z }, OPT_ALT
void InsertDue(const SimContext& c) {
	CO::State* s = Internal::State();
	if (s == nullptr) return;
	const std::vector<int32_t> due(s->waypointUnits.Ids().begin(), s->waypointUnits.Ids().end());
	for (const int32_t u : due) {
		if (!c.ids.Contains(u)) continue;
		const auto slot = s->SlotOf(u);
		if (slot != Core::DenseIndex::NONE) {
			const CO::WaypointDue d = s->waypointDue[slot];
			s->waypointDue[slot] = CO::WaypointDue{};
			if (d.active && unitHandler.GetUnit(u) != nullptr) {
				const float p[] = {0, static_cast<float>(CO::CUSTOM_WAYPOINT), static_cast<float>(INTERNAL_ORDER), d.x, d.y, d.z};
				Internal::Give(u, CMD_INSERT, p, ALT_KEY);
			}
		}
		s->waypointUnits.Erase(u);
	}
}

} // namespace

namespace Internal {

// Lua Spring.GiveOrderToUnit(u, CMD.REMOVE, { tag }, 0)
bool Remove(int unit, std::uint32_t tag) {
	const float p[] = {static_cast<float>(tag)};
	return Give(unit, CMD_REMOVE, p, 0);
}

// Lua Insert(u, cmdID, params, kind, target)
void InsertAhead(int unit, int commandID, std::span<const float> params, CO::OwnKind kind, int target) {
	std::vector<float> p = {0, static_cast<float>(commandID), static_cast<float>(INTERNAL_ORDER)};
	p.insert(p.end(), params.begin(), params.end());
	if (!Give(unit, CMD_INSERT, p, ALT_KEY)) return;
	const CUnit* u = unitHandler.GetUnit(unit);
	CO::State* s = State();
	if (u == nullptr || s == nullptr || u->commandAI->commandQue.empty()) return;
	const Command& front = u->commandAI->commandQue.front();
	if (front.GetID() != commandID) return;
	const auto slot = s->Track(unit, u->unitDef->id);
	if (slot != Core::DenseIndex::NONE) s->own[slot] = CO::OwnStep{front.GetTag(), kind, target};
}

// Lua Spring.SetUnitMoveGoal(u, x, y, z, r)
void SetMoveGoal(int unit, float x, float y, float z, float radius) {
	if (!Check::MayWrite()) return;
	CUnit* u = unitHandler.GetUnit(unit);
	if (u == nullptr || u->moveType == nullptr) return;
	u->moveType->StartMoving(float3(x, y, z), radius, u->moveType->GetMaxSpeed());   // KT1 lint: allow direct-write called from CommandFallback, which has no SimContext; Check::MayWrite guards it above
}

// called from Orders' LoadConfig (game setup)
void RegisterQueueHandling() {
	waypoint = CO::MakeWaypointParams(Config());
	Cargo::Init();
	CommandHooks::AddFallback(SYSTEM, FALLBACK_ORDER, [](const CUnit* u, const Command& c, Faked) { return OnFallback(u, c); });
	Phases::Add(SYSTEM, Core::SimHost::Phase::FrameStart, WAYPOINT_PHASE, 0, [](const SimContext& c) { InsertDue(c); });
	if (client == nullptr) client = new CmdDoneClient();
	else eventHandler.RemoveClient(client);
	eventHandler.AddClient(client);
}

} // namespace Internal

} // namespace KT1::Orders
