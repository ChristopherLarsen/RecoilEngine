/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 rule: Spring.KT1.Cues (Audio/Cues/CueBridge.h; LUA-FREE D10). The raising and fact calls act for synced Lua only;
// the zoom gate belongs to the viewer and acts for unsynced Lua only.

#include "Game/KT1/Audio/Cues/CueBridge.h"   // KT1 lint: allow sim-draw-include cue API

#include <string_view>

#include "CueSim.h"
#include "Game/KT1/Audio/Cues/CueApi.h"      // KT1 lint: allow sim-draw-include cue API
#include "Game/KT1/Audio/Cues/CueAudio.h"    // KT1 lint: allow sim-draw-include cue API
#include "Lua/LuaHandle.h"
#include "lib/lua/include/LuaInclude.h"

namespace KT1 {
namespace {

bool Synced(lua_State* L) { return CLuaHandle::GetHandleSynced(L); }

std::string_view Text(lua_State* L, int arg) {
	size_t n = 0;
	const char* s = lua_isstring(L, arg) ? lua_tolstring(L, arg, &n) : nullptr;
	return s == nullptr ? std::string_view() : std::string_view(s, n);
}

float3 Point(lua_State* L, int first) {
	return float3(luaL_optfloat(L, first, 0.0f), luaL_optfloat(L, first + 1, 0.0f), luaL_optfloat(L, first + 2, 0.0f));
}

int LRaise(lua_State* L) {
	lua_pushboolean(L, Synced(L) && Cues::Raise(Text(L, 2), luaL_optint(L, 1, -1), Text(L, 3)));   // DESIGN: Lua argument places
	return 1;
}

int LRaiseAt(lua_State* L) {
	const bool sent = Synced(L) && Cues::RaiseAt(Text(L, 1), Point(L, 2), luaL_optint(L, 5, 0), Text(L, 6));   // DESIGN: Lua argument places
	lua_pushboolean(L, sent);
	return 1;
}

int LAlert(lua_State* L) {
	if (Synced(L))
		Cues::AlertAt(Text(L, 1), Text(L, 2), luaL_optfloat(L, 3, 0.0f), luaL_optfloat(L, 4, 0.0f), luaL_optint(L, 5, 0));   // DESIGN: Lua argument places
	return 0;
}

int LLoop(lua_State* L) {
	Cues::CueSim* sim = Cues::ActiveSim();
	if (Synced(L) && sim != nullptr) sim->Loop(luaL_checkint(L, 1), lua_toboolean(L, 2) != 0);
	return 0;
}

int LShieldHit(lua_State* L) {
	if (Synced(L)) Cues::ShieldHit(luaL_checkint(L, 1), Point(L, 2));   // DESIGN: Lua argument places
	return 0;
}

int LShieldState(lua_State* L) {
	if (Synced(L)) Cues::ShieldState(luaL_checkint(L, 1), luaL_checkint(L, 2), Text(L, 3), luaL_optfloat(L, 4, 0.0f));   // DESIGN: Lua argument places
	return 0;
}

int LCaptureProgress(lua_State* L) {
	if (Synced(L)) Cues::CaptureProgress(luaL_checkint(L, 1), luaL_optint(L, 2, -1), luaL_optfloat(L, 3, 0.0f));
	return 0;
}

int LCaptureDone(lua_State* L) {
	if (Synced(L)) Cues::CaptureDone(luaL_checkint(L, 1), luaL_checkint(L, 2));
	return 0;
}

int LCounts(lua_State* L) {
	Cues::CueSim* sim = Cues::ActiveSim();
	lua_pushnumber(L, sim == nullptr ? 0 : sim->State().sent);
	lua_pushnumber(L, sim == nullptr ? 0 : sim->State().cut);
	return 2;
}

int LSetZoomCut(lua_State* L) {
	if (!Synced(L)) CueAudio::SetZoomCut(Text(L, 1), luaL_optfloat(L, 2, 0.0f));
	return 0;
}

int LKept(lua_State* L) {
	if (Synced(L)) return 0;
	lua_newtable(L);
	CueAudio::TakeKept([L](const std::string& cue, unsigned n) {
		lua_pushnumber(L, n);
		lua_setfield(L, -2, cue.c_str());
	});
	return 1;
}

} // namespace

void PushCuesBridge(lua_State* L) {
	const luaL_Reg functions[] = {
		{"Raise", LRaise}, {"RaiseAt", LRaiseAt}, {"Alert", LAlert}, {"Loop", LLoop}, {"ShieldHit", LShieldHit},
		{"ShieldState", LShieldState}, {"CaptureProgress", LCaptureProgress}, {"CaptureDone", LCaptureDone},
		{"Counts", LCounts}, {"SetZoomCut", LSetZoomCut}, {"Kept", LKept},
	};
	for (const luaL_Reg& f : functions) {
		lua_pushcfunction(L, f.func);
		lua_setfield(L, -2, f.name);
	}
}

} // namespace KT1
