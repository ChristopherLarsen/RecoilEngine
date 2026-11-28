/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 GroundShade: Spring.KT1.GroundShade for the Lua that still runs (replaces WG.groundShade; EFFECTS 4, step D08).
// Unsynced. While the system is not on, the Set functions do nothing and the readers answer 0.
//
//   SetLevel(level)                      0 off, 1 low, 2 full (frontend/screens/options.lua)
//   GetLevel()             -> level      asked for
//   GetEffective()         -> level      drawn (the frame rate guard holds 1)
//   Blobs()                -> n          scorch blobs queued
//   AddBlob(x, z, radius [, strength])   test hook
//   CostMs()               -> ms         mean per drawn frame since ResetCost
//   ResetCost()

#ifndef KT1_GROUND_SHADE_BRIDGE_H
#define KT1_GROUND_SHADE_BRIDGE_H

struct lua_State;

namespace KT1::GroundShade {

void PushGroundShadeBridge(lua_State* L);   // fills Spring.KT1.GroundShade (Rule::PushLua)

} // namespace KT1::GroundShade

#endif
