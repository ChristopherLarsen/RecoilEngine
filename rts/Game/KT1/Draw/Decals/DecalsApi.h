/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 FxDecals: what others ask of the decal system (replaces GG.Scorch, GG.Tracks and GG.TerrainDecals; EFFECTS 4, step
// D16). Draw side. While the system is not on, requests do nothing and ScorchDeath answers false.
//
// Lua (dev scenes only, unsynced), Spring.KT1.FxDecals (Spring.KT1.Decals is Draw/Rnd005's):
//   ScorchStats()   -> { created, merged, dropped, alive, scale, biome }
//   TrackStats()    -> { created, dropped, capped, prints, scuffs, washes, luaMs, frames, tracksOn, alive, scale, walkers,
//                        hovers, rotors, treaders, cap, perFrame, fade, fadeFar }       luaMs: the engine pass's ms
//   Steps(unitID)   -> { contacts per leg } | nil
//   TerrainStats()  -> { created, dropped, byType, biome, done }

#ifndef KT1_DECALS_API_H
#define KT1_DECALS_API_H

#include <cstdint>

struct lua_State;

namespace KT1::FxDecals {

struct DecalRequest;                  // DecalRequest.h

bool Active();
// another effect's own mark (a crater, a stain, a charged hit's burn): its handle, 0 when not placed
uint32_t Submit(const DecalRequest& request);
// a death mark of a diameter (elmos), life (s) and alpha (Draw/FxBuild's criteria); false: water, or marks are off
bool ScorchDeath(float x, float z, float size, float lifeS, float alpha);

void PushFxDecalsBridge(lua_State* L);   // fills Spring.KT1.FxDecals (Rule::PushLua)

} // namespace KT1::FxDecals

#endif
