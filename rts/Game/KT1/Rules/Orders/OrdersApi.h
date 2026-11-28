/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 ORDERS: what other families call (today's GG.Ord readers: cmb_targeting, game_eco, ord_pickup, ord_transfer,
// snd_events, family GLUE's ferry and formation code, and the engine cases). Unit, team and feature ids and plain values
// only. The custom command ids are Core::Orders::CUSTOM_* (OrderTypes.h). While system Orders is off every read answers
// "nothing" (false, 0, -1, empty) and every write does nothing; Lua then still owns the state.
// Writes made while a system runs in check mode are skipped.

#ifndef KT1_RULES_ORDERS_API_H
#define KT1_RULES_ORDERS_API_H

#include <span>
#include <utility>
#include <vector>

#include "Game/KT1/Core/Orders/MoveTracking.h"
#include "Game/KT1/Core/Orders/OrderTypes.h"
#include "Game/KT1/Core/Orders/OrdersState.h"

namespace KT1 {

struct SimContext;

namespace Orders {

// ---- attack-move (asks system AMove) -------------------------------------------------------------------------------

bool IsAMove(const SimContext& context, int unit);                 // the unit has a running attack-move task
int AMoveTarget(const SimContext& context, int unit);              // rules param amove_target; -1: none
// GG.Ord.amTargetOf[target]: the units whose task fights or waits on it, ascending, appended to `units`
void AmTargetOf(const SimContext& context, int target, std::vector<int>& units);

// ---- guard and patrol work -----------------------------------------------------------------------------------------

// GG.Ord.GuardTask: the work the unit's guard chain reaches (kind None when it is not guarding a builder)
Core::Orders::Task GuardTask(const SimContext& context, int unit);
int GuardRepairOf(const SimContext& context, int builder);         // GG.Ord.guardRepair[builder]; -1: none
int GuardShieldOf(const SimContext& context, int builder);         // GG.Ord.guardShield[builder]; -1: none
// every (builder, guarded unit) of guardShield, ascending builder, appended
void GuardShields(const SimContext& context, std::vector<std::pair<int, int>>& out);
void ClearGuardShield(const SimContext& context, int builder);     // the economy drops a stale entry
// GG.Ord.Refused: the engine's patrol or attack-move work was removed from this unit this step or the one before
bool Refused(const SimContext& context, int unit);

// ---- fire state (ORD-010 R1) ---------------------------------------------------------------------------------------

Core::Orders::FireState DefaultFireState(const SimContext& context, int unitDefID);
Core::Orders::FireState FireStateOf(const SimContext& context, int unit);   // Free for a unit not tracked
// rules param fire_state, the engine's fire-state order and the unit's button
void SetFireState(const SimContext& context, int unit, Core::Orders::FireState state);

// ---- move tracking verdicts (pure; the cases ask them) -------------------------------------------------------------

bool EngineEndsBlocked(const SimContext& context, int unit);       // the fork's route follower ends a blocked move itself
bool EngineDropped(const SimContext& context, float ageSteps, float ux, float uz, bool hasStart, float sx, float sz,
                   float gx, float gz);
bool RedirectUnreachable(const SimContext& context, const Core::Orders::RedirectQuery& query, bool boxedIn);
bool StaleFill(const SimContext& context, float ux, float uz, bool hasNearest, float nx, float nz, float gx, float gz,
               bool boxedIn);
// GG.Ord.RefillDue: the schedule's answer for a move filled before, on the live change grid
bool RefillDue(const SimContext& context, const Core::Orders::MoveTrack& move, float x, float z, bool terrainDeforms);
bool OrderFillBeyond(const SimContext& context, float x, float z, float gx, float gz);
bool TieNear(const SimContext& context, float px, float pz, float gx, float gz, float r);
void MarkBlockChange(const SimContext& context, float x, float z, float radius);

// ---- ferry routes (ORD-007 R4, R5) ---------------------------------------------------------------------------------

bool HasFerry(const SimContext& context, int transport);
int FerryMarker(const SimContext& context, int transport);         // -1: no route
int FerryEndMarker(const SimContext& context, int transport);      // always -1: a route has one marker (UI-GAP-024 R4)
void FerryWaiting(const SimContext& context, int transport, std::vector<int>& units);   // in arrival order, appended

// ---- goal cells (mov_goal_cell's records; read by the waypoint step) -----------------------------------------------

void NoteGoalCell(const SimContext& context, int unit, const Core::Orders::GoalCellRecord& record);
std::span<const Core::Orders::GoalCellRecord> GoalCellRecords(const SimContext& context, int unit);

} // namespace Orders
} // namespace KT1

#endif
