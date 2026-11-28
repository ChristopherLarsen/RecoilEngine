/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 FxBuild: what others ask of the build and criteria effects (replaces GG.CriteriaFx and GG.CombatBuildFx and the
// call Script.LuaUI.CriteriaShake, which stays inside this system; EFFECTS 4, step D15). Draw side. While the system is
// not on every call does nothing.
//
// Lua (unsynced, dev scenes), Spring.KT1.FxBuild:
//   Stats()          -> { beams, bands, welds, sparks, orbs, cost }
//   State()          -> { effects, on, limiting, guard }
//   CriteriaStats()  -> { hits, deaths, chains, debris, plumes, scorches, shakes }
//   ReclaimStats()   -> { parts, emitters, spawned }

#ifndef KT1_FX_BUILD_API_H
#define KT1_FX_BUILD_API_H

#include <cstdint>

struct lua_State;

namespace KT1::FxBuild {

bool Active();
// the criteria flash is on: unit effects leave a death's mark and char to it
bool CriteriaOn();
// a weapon hit's flash (Draw/FxFields' blast pass; the Lua's GG.CriteriaFx.Hit)
void Hit(float x, float y, float z, int32_t weaponDef);

void PushFxBuildBridge(lua_State* L);   // fills Spring.KT1.FxBuild (Rule::PushLua)

} // namespace KT1::FxBuild

#endif
