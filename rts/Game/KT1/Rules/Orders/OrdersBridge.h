/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 ORDERS: OrdersApi.h for the Lua that still runs, as Spring.KT1.Orders.* (synced; in an unsynced state every
// function but Config returns nothing, and with the system off every function but Config returns nothing). When
// ord_orders.lua stands down, GG.Ord becomes a thin table over these (set in KT1.StandDown). Ids are numbers; a missing
// unit, marker or target is nil.
//
// Kept from rule version 1 (ord_orders.lua's own walks):
//   Movers()                         -> { unitID, ... }    ground movers, not carried, a player's move in front; ascending
//   Busy()                           -> { unitID, ... }    units with a queue; ascending
// Values:
//   Config()                         -> { ord = { queue_min=, patrol_check_interval=, ... }, commands = { CMD_FERRY=,
//                                       CMD_WAYPOINT=, CMD_DIVE=, CMD_AMOVE=, CMD_FIRE_STATE= },
//                                       FIRE_NAMES = { "hold", "free", "ground" }, FERRY_LEAVE_DELAY = seconds }
//   UseFixture(name|nil [, values])  ->                    the named fixture's values over the config (nil: the config);
//                                                          `values`: a table { key = number } over them (OrdersConfig.h)
// Attack-move:
//   IsAMove(unitID)                  -> bool
//   AMoveTarget(unitID)              -> unitID|nil
//   AmTargetOf(targetID)             -> { unitID, ... }
// Guards and patrols:
//   Order(unitID)                    -> { id=, tag=, params={...}, options={internal=, shift=, coded=} } | nil
//   GuardTask(unitID)                -> kind|nil, targetID|featureID|nil, isFeature|nil, leadID|nil
//   GuardRepair(builderID)           -> unitID|nil
//   GuardShields()                   -> { [builderID] = unitID }
//   ClearGuardShield(builderID)      ->
//   Refused(unitID [, frame])        -> bool
// Fire state:
//   DefaultFireState(unitDefID)      -> "hold"|"free"|"ground"
//   FireState(unitID)                -> "hold"|"free"|"ground"
//   SetFireState(unitID, name)       ->
// Move tracking:
//   EngineEndsBlocked(unitID)        -> bool
//   EngineDropped(age, ux, uz, sx|nil, sz|nil, gx, gz)                                         -> bool
//   RedirectUnreachable(naval, reachable|nil, nx|nil, nz|nil, gx, gz, visited|nil, boxed, still|nil) -> bool
//   StaleFill(ux, uz, nx|nil, nz|nil, gx, gz, boxed)                                           -> bool
//   OrderFillBeyond(x, z, gx, gz)    -> bool
//   TieNear(px, pz, gx, gz, r)       -> bool
//   Failures()                       -> { { unit=, frame=, why= }, ... }                       test instrument
// Ferry:
//   HasFerry(transportID)            -> bool
//   FerryMarker(transportID)         -> unitID|nil
//   FerryEndMarker(transportID)      -> nil
//   FerryWaiting(transportID)        -> { unitID, ... }    in arrival order
// Goal cells (while mov_goal_cell and the waypoint step are on different sides):
//   NoteGoalCell(unitID, restX, restZ, x, y, z)   ->
//   GoalCells(unitID)                -> { { restX, restZ, x, y, z }, ... }

#ifndef KT1_RULES_ORDERS_BRIDGE_H
#define KT1_RULES_ORDERS_BRIDGE_H

struct lua_State;

namespace KT1 {

// fills the table on top of the stack (Spring.KT1.Orders); system Orders calls it from PushLua
void PushOrdersBridge(lua_State* L);

} // namespace KT1

#endif
