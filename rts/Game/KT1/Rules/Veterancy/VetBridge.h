/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 Veterancy: Spring.KT1.Veterancy for the Lua that still runs (LUA-FREE H01, step S26). GG.Vet becomes a thin table
// over it (its xp and level tables read Xp and Level; game_eco's capture writes them: Set). Functions that write do
// nothing outside synced Lua. The rules params vet_level, vet_xp, vet_prev, vet_next and vet_regen keep being written.
//
//   Level(unitID)                             -> level (0: none)
//   Xp(unitID)                                -> experience | nil
//   Bounds(unitID)                            -> prev, next | nothing
//   Credit(unitID, mass)
//   CreditCargo(transportID, team, attackerID, attackerTeam)
//   Set(unitID, xp | nil, level)                 a capture's carry-over (then SetBase)
//   SetBase(unitID, maxHealth)
//
// Calls into rules that are still Lua, registered once by the synced glue gadget in Initialize:
//   Hooks({ carrier = function(unitID) -> transportID | nil,        GG.CargoDeath.Carrier
//           stored = function(transportID) -> { unitID, ... } })    GG.SpcCargo.stored
//
// Test readers (dev builds):
//   Debug.UseFixture(name | nil)                 a fixture of data/kt1/veterancy.json (GG.Vet.UseFixture)
//   Debug.SetCost(unitID, mass)
//   Debug.Rebuild(unitID)
//   Debug.ClassFactor(class)                  -> factor | nil

#ifndef KT1_VET_BRIDGE_H
#define KT1_VET_BRIDGE_H

struct lua_State;

namespace KT1::Veterancy {

void PushVetBridge(lua_State* L);   // fills Spring.KT1.Veterancy (Rule::PushLua)

} // namespace KT1::Veterancy

#endif
