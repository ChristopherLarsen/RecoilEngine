/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 FxEvents: Spring.KT1.Fx for the Lua that still runs (LUA-FREE H01, EFFECTS 4; step S30). GG.Fx, GG.FxEvents,
// GG.CombatFx, GG.WreckPhysics and GG.WreckPose become thin tables over it. Every function that writes does nothing
// outside synced Lua and while the system is not live. Frames and per frame speeds are the Lua callers' units.
//
//   Active()                                       -> bool
//
//   Emit(kind, ...)                                -> bool        one entry for every request; the kinds:
//       "spawn", name, x, y, z [, dx, dy, dz, priority, length]   GG.Fx.Spawn; priority "hit" (default), "death",
//                                                                  "shield", "trail", "state", "ambient"
//       "move", moveKind, size, x, y, z                           GG.Fx.Move
//       "intercept", x, y, z [, fromX, fromY, fromZ]              GG.Fx.Intercept
//       "teleport", "start"|"jump"|"cancel", unitID [, x0, z0, x1, z1, value]
//       "crash", unitID
//       "remnant", unitID, x, y, z
//       "sacrifice", engineerID, siteID, x, y, z, buildBefore, buildAfter
//       "blast", class, family, factionIndex, x, y, z [, weaponDefID]      GG.CombatFx.Forward (class and family names,
//                                                                  factionIndex 1 .. 4 as gamedata/fx_factions.lua)
//       "scorch", weaponDefID, x, y, z                            a scripted mark (death_blast)
//   Spawn, Move, Intercept(...)                    the same arguments and returns as Emit without the kind
//   Name(faction, class, scale, part)              -> string
//   Teleport, Crash, Remnant, Sacrifice(...)       the same arguments as the Emit kinds
//   ForwardBlast(class, family, factionIndex, x, y, z [, weaponDefID]) -> bool
//   ClassOf(weaponDefID)                           -> string | nil
//   FamilyOf(weaponDefID)                          -> string | nil
//   Scale()                                        -> number
//   Counts()                                       -> string      "hit <spawned>/<requested> death ..."
//   Stats()                                        -> { requested = {6}, spawned = {6}, dropped = {6} }
//   Intercepts()                                   -> number
//   CombatStats()                                  -> { blasts, cores, forwarded, capped, experimental }
//
// Wrecks (eco_wrecks, while it is Lua):
//   WreckPlaced(featureID)
//   WreckMoving(featureID)                         -> bool
//   WreckPush(featureID, vx, vz, up, spin)         -> bool        elmos and radians per frame
//   WreckPose(featureID, rx, ry, rz)                              GG.WreckPose[f] = { rx, ry, rz }; without angles: = nil
//   WreckInfo(featureID)                           -> seq, vx, vy, vz, rx, ry, rz, air | nil     (the tests' reads)
//   WreckList()                                    -> { featureID, ... } in motion, in order
//   WreckWeapon(weaponDefID)                       -> damage, radius, tumbles | nil
//
// The silo relay (spc_silo, while it is Lua), once a frame; records are the Lua's own tables with a `key` number:
//   Silo(missiles, interceptors, redirects)
//
// The registry gives every rule its own table, Spring.KT1.<Rule>, and this one is named "Fx". PushFxBridge is called from
// the FxEvents rule's PushLua, where Registry::PushLua has the rule's own table on top and the KT1 table at index -3: it
// sets KT1.Fx in that table and leaves the stack as it found it.

#ifndef KT1_FX_EVENTS_BRIDGE_H
#define KT1_FX_EVENTS_BRIDGE_H

struct lua_State;

namespace KT1::Fx {

void PushFxBridge(lua_State* L);

} // namespace KT1::Fx

#endif
