/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 ORDERS, step S05: the adapter of the busy-unit turn, for check mode and trace recording (game-build
// docs/engine/rules/orders.md "Busy-unit turn"). Core: Core/Orders/TurnStep.h and TurnTrace.h. System Orders owns it;
// with neither check nor recording nothing here runs. It also holds the family's state (Internal::State()).
//
// ord_orders.lua stays authoritative. From inside its walk it hands over, per unit, what only the Lua holds; the def, the
// queue and the queue length after the turn are read here from the engine at the same moment. Joins and leaves of its
// unit list come from the adapter's own event client. At the walk's end the step's TurnInput is recorded (trace input
// "turn") and, in check mode, Core runs it on the family's state and the three columns are reported. Nothing here gives an
// order or writes engine state.
//
// Lua (synced; each returns nothing when the adapter does not run):
//   Spring.KT1.Orders.TurnWatch()   -> "check" | "trace"   the gadget is to hand its walk over (and how it answers)
//   Spring.KT1.Orders.TurnBegin(step, netUnits|nil, guardCheckS, patrolCheckS, fightCheckS)
//       before the walk: the safety net's units in the order read (its steps only) and the intervals the Lua runs on
//   Spring.KT1.Orders.TurnRead(unitID, ownKind, ownTag, ownTarget, taskTag|nil, walkTags|nil, given|nil)
//       before the unit's turn: ours[u] (kind as Core OwnKind, 0 none), amove[u].tag, reachTag[u] as a list of tags,
//       given[u] as { cmdID, target, cmdID, target, ... }
//   Spring.KT1.Orders.TurnEnd()     after the walk

#ifndef KT1_RULES_ORDERS_TURN_H
#define KT1_RULES_ORDERS_TURN_H

struct lua_State;

namespace KT1::Orders::Turn {

// game setup (Orders' config load): check: the switch reads check; record: kt1_trace names Orders
void Start(bool check, bool record);
void Stop();                  // the rule's end: nothing more is collected and the state is freed
void PushLua(lua_State* L);   // into the table on top of the stack (Spring.KT1.Orders)

} // namespace KT1::Orders::Turn

#endif
