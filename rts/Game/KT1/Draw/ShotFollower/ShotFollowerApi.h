/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 ShotFollower: what others ask of the shot follower (replaces GG.Shots, GG.ShotsHeavyStats and the call
// Script.LuaUI.FxGlow; EFFECTS 4, step D05). Draw side. While the system is not on every list is empty.
//
// Lua (unsynced), Spring.KT1.ShotFollower:
//   Glows()               -> { x, y, z, radius, amount, r, g, b, ... }   this step's glow sources, 8 numbers each
//                            (gfx_fx_post, while it is Lua: it reads them in place of the FxGlow calls)
//   Reload(why)           -> bool           the recipes are read again (chat "fxreload")
//   Mode()                -> { ribbons, keep, missilesOnly, lifeMul, lifeMax, puffs, pulse, flak }, setting, limiting
//   Stats()               -> { shots, ribbons, capped, segments, puffs, bodies, puffsCapped, enemy, fogged, collapsed,
//                              enemySeen, wake, arcs, waves, implode, impacts, stains }
//   Cost()                -> { plan, draw, draws, scan, ids, frames, band, follow, bodies, fetch, impact, kb }   ms totals
//   ResetCost()
//   HeavyStats()          -> { frames, heavy, maxShots }

#ifndef KT1_SHOT_FOLLOWER_API_H
#define KT1_SHOT_FOLLOWER_API_H

#include <span>

struct lua_State;

namespace KT1::ShotFollower {

struct Glow;                          // ShotTrack.h

bool Active();
std::span<const Glow> Glows();        // this step's (the native post pass)
bool Reload(const char* why);

void PushShotFollowerBridge(lua_State* L);   // fills Spring.KT1.ShotFollower (Rule::PushLua)

} // namespace KT1::ShotFollower

#endif
