/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 TimedRules: Spring.KT1.TimedRules for the Lua that still runs (LUA-FREE H01, step S21): the Lua unit scripts'
// GG.Charge.Fire and Wind, and GG.Stun. Both become thin tables over it. Weapon numbers from 1. Functions that write do
// nothing outside synced Lua. The rules param stun_enabled keeps being written (SetStunEnabled).
//
//   ChargeFire(unitID, weapon)                -> bool: the shot may fire
//   ChargeWind(unitID, weapon, progress)
//   ChargeGrant(unitID, weapon, want, share)     the energyRequest hook's answer, from the economy's callback
//   Stunned(unitID)                           -> bool
//   SetStunEnabled(unitID, on)
//
// Calls into rules that are still Lua, registered once by the synced glue gadget in Initialize:
//   Hooks({ energyRequest = function(teamID, want, unitID, weapon) })   GG.Eco.Request(teamID, 0, want, callback)
//
// Test readers (dev builds):
//   Debug.Charge(unitID, weapon)              -> drawn, charging, started, cost | nil
//   Debug.SetCharge(unitID, weapon, drawn)       GG.Charge.Set
//   Debug.KeepShots(on); Debug.Shots(unitID)  -> { frame, ... } of the shots the gate let fire
//   Debug.StunEnd(unitID)                     -> frame | nil   (GG.Stun.state)
//   Debug.StunLog()                           -> { { frame, unit, seconds, weapon }, ... }

#ifndef KT1_TIMED_RULES_BRIDGE_H
#define KT1_TIMED_RULES_BRIDGE_H

struct lua_State;

namespace KT1::TimedRules {

void PushTimedRulesBridge(lua_State* L);   // fills Spring.KT1.TimedRules (Rule::PushLua)

} // namespace KT1::TimedRules

#endif
