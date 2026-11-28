/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 Targeting: Spring.KT1.Targeting for the Lua that still runs (LUA-FREE H01, COMBAT §4.5; step S19): ord_orders,
// the projectile gadgets, unit_cmd_shot. GG.Cmb and GG.CmdGun become thin tables over it (GG.Cmb.focus keeps its table
// form: __index reads Focus, __newindex calls SetFocus). Synced Lua only for the functions that read the sim; those
// that write do nothing elsewhere. Weapon numbers from 1; reaches in elmos.
//
//   Target(unitID, weapon)                    -> targetID | nil
//   Engage(unitID, targetID [, noLine [, only]]) -> reach, rank | nil
//   Engage2(unitID, targetID [, noLine])      -> primaryReach, primaryRank, bestReach, bestRank
//                                                (nils where no weapon takes it; nothing on hold fire)
//   BestKey(unitID, targetDefID)              -> kind (0 primary, 1 other), rank | nil
//   Focus(unitID)                             -> targetID | nil
//   SetFocus(unitID, targetID | nil)
//   InFlight(targetID)                        -> damage (0: none)
//   Contact(targetID, allyTeamID)             -> bool
//   ManualFireRefusal(unitID, targetID)       -> reason | nil
//   ScanFrames(seconds)                       -> frames
//   CmdGunTarget(unitID)                      -> targetID | nil
//   CmdGunNextScan(unitID)                    -> frame | nil
//
// The rules params cmb_refused and cmb_refused_frame keep being written.
//
// Test readers (dev builds):
//   Debug.SetRules(table)                        an overlay shaped as data/kt1/targeting.json; nil: the file's values
//                                                again (GG.Cmb.UseFixture)
//   Debug.Stats()                             -> scans, candidates, holds since the last call
//   Debug.SetProbe(on)                           logs every candidate of a scan (GG.Cmb.probeScan)

#ifndef KT1_TARGETING_BRIDGE_H
#define KT1_TARGETING_BRIDGE_H

struct lua_State;

namespace KT1::Targeting {

void PushTargetingBridge(lua_State* L);   // fills Spring.KT1.Targeting (Rule::PushLua)

} // namespace KT1::Targeting

#endif
