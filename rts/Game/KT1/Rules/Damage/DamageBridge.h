/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 Damage: Spring.KT1.Damage for the Lua that still runs (LUA-FREE H01, step S21). GG.Cmb, GG.CmbPatterns, GG.SpcExp
// and GG.RemoveWithoutBlast become thin tables over it. Functions that write do nothing outside synced Lua.
//
//   LastKill(unitID)                      -> amount, healthBefore, maxHealth | nil
//   MarkNoBlast(unitID)                      GG.RemoveWithoutBlast[unitID] = true
//   NoBlast(unitID)                       -> bool
//   Crashing(unitID)                      -> frame of the killing hit | nil
//   CrashKill(unitID)                     -> attackerID, attackerTeam, frame | nil
//   CrashContact(unitID)                  -> frame, x, y, z, fallFrom | nil
//   Effect(unitDefID)                     -> { damage, radius, dtype, kind, crash, fall } | nil
//   Impact(weaponDefID, x, y, z [, attackerID]) -> bool
//   ArmourMult(unitDefID [, damageType])  -> number
//   Values()                              -> { death_threshold, death_delay_s (nil: unset), land_death_to_wreck_s,
//                                              ship_death_to_wreck_s, death_blast_beat_s }
//
// Calls into rules that are still Lua, registered once by the synced glue gadget in Initialize:
//   Hooks({ truce = function(attackerTeam, team) -> bool,                 GG.Options.Truce
//           cargoNoBlast = function(unitID, weaponDefID) -> bool,         GG.CargoDeath.NoBlast
//           cargoFall = function(unitID, team, attackerID, attackerTeam), GG.CargoDeath.Fall
//           shieldBlast = function(x, y, z, radius, amount, damageType, sourceUnitID) -> handle (integer) | nil,
//           shelter = function(handle, unitID, amount) -> amount,         GG.Shields.Blast and Shelter
//           vetCredit = function(attackerID, mass),                       GG.Vet.Credit, while veterancy is Lua
//           flightAim = function(projectileID, unitID | nil, x, y, z),    GG.CmbProjectiles.flights: the target
//           flightWeave = function(projectileID, elmos, everyFrames) -> bool (false: no flight),
//           hitEffect = function(weaponDefID, x, y, z),                   look: GG.Fx.Spawn of the weapon's hit
//           crashTrail = function(unitID),                                look: GG.FxEvents.Crash
//           crashBurst = function(x, y, z) })                             look: GG.CombatFx.Forward
//   A hook that is absent answers false or nothing.
//
// Test readers (dev builds):
//   Debug.SetValues(table)                   an overlay shaped as data/kt1/damage.json; nil: the file's values again
//                                            (GG.Cmb.UseFixture, GG.CmbPatterns.UseValues)
//   Debug.Pending()                       -> { { at, x, y, z, radius, amount, attacker, w }, ... } pattern pulses
//   Debug.Children()                      -> { [projectileID] = { born, w } }
//   Debug.SeedChild(projectileID, weaponDefID, bornFrame)
//   Debug.Splits()                        -> number
//   Debug.KeepLog(on); Debug.Log()        -> { { frame, unit, def, team, kind, x, y, z, radius, damage, hits }, ... }
//   Debug.Work()                          -> steps that had worked-out falls

#ifndef KT1_DAMAGE_BRIDGE_H
#define KT1_DAMAGE_BRIDGE_H

struct lua_State;

namespace KT1::Damage {

void PushDamageBridge(lua_State* L);   // fills Spring.KT1.Damage (Rule::PushLua)

} // namespace KT1::Damage

#endif
