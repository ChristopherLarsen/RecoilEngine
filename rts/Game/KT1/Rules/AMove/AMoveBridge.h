/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 AMove: the table Spring.KT1.AMove (synced). From step S03 the engine runs the task's states and due steps while
// ord_orders.lua still picks targets and makes builder steps: it pulls the events once per step and answers them. Step
// S04 takes both native and removes Events, Pick and Stepping; State stays for the cases (dev_spc_accept.lua reads the
// task). With the system off every function returns nothing.
//
//   Events()                        -> { { unit=, kind="pick_due"|"arrived"|"front_lost"|"builder_step" }, ... }
//                                      this step's, ascending unit id
//   State(unitID)                   -> { tag=, mode="route"|"fight"|"halt"|"back", target=unitID|nil, gx=, gy=, gz=,
//                                        rx=nil|, rz=nil| } | nil
//   Pick(unitID, targetID|nil, primary, harmlessID|nil)  ->    the answer to a pick_due: fight the target, else halt for
//                                                              the harmless one, else go on
//   Stepping(unitID, on)            ->                         a builder step was put ahead of the order (on), or ended
//   Start(unitID, tag, x, y, z)     ->                         the order reached the front (while the unit turn is Lua)
//   End(unitID)                     ->
//   CapGroups()                     ->                         after the Lua unit turn

#ifndef KT1_RULES_AMOVE_BRIDGE_H
#define KT1_RULES_AMOVE_BRIDGE_H

struct lua_State;

namespace KT1 {

// fills the table on top of the stack (Spring.KT1.AMove); system AMove calls it from PushLua
void PushAMoveBridge(lua_State* L);

} // namespace KT1

#endif
