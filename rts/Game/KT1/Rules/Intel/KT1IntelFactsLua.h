/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 rule IntelFacts: Lua table helpers of its adapter (KT1IntelFacts.cpp only).

#ifndef KT1_INTEL_FACTS_LUA_H
#define KT1_INTEL_FACTS_LUA_H

#include <cstdint>
#include <vector>

#include "Lua/LuaUtils.h"
#include "Sim/Units/UnitDefHandler.h"

namespace KT1::IntelFactsLua {

inline float Num(lua_State* L, int t, const char* k, float def) {
	lua_getfield(L, t, k);
	const float v = lua_isnumber(L, -1) ? (float)lua_tonumber(L, -1) : def;
	lua_pop(L, 1);
	return v;
}
inline bool Bool(lua_State* L, int t, const char* k) {
	lua_getfield(L, t, k);
	const bool v = lua_toboolean(L, -1);
	lua_pop(L, 1);
	return v;
}
inline void DefFlags(lua_State* L, int t, const char* k, std::vector<uint8_t>& out) {
	out.assign(unitDefHandler->NumUnitDefs() + 1, 0);
	lua_getfield(L, t, k);
	if (lua_istable(L, -1))
		for (size_t d = 0; d < out.size(); ++d) { lua_rawgeti(L, -1, (int)d); out[d] = lua_toboolean(L, -1) ? 1 : 0; lua_pop(L, 1); }
	lua_pop(L, 1);
}

// DESIGN: ev holds (unit, code, value) triples and fin, drop (a, b) pairs, as SupComIntFactsRun's tables
inline void PushEv(lua_State* L, int EV, int& nEv, int u, float code, int v) {   // DESIGN: triples
	nEv += 1;
	lua_pushnumber(L, (float)u); lua_rawseti(L, EV, 3 * nEv - 2);   // DESIGN: triple
	lua_pushnumber(L, code); lua_rawseti(L, EV, 3 * nEv - 1);   // DESIGN: triple
	lua_pushnumber(L, (float)v); lua_rawseti(L, EV, 3 * nEv);   // DESIGN: triple
}
inline void Push2(lua_State* L, int t, int& n, float a, float b) {   // DESIGN: pairs
	n += 1;
	lua_pushnumber(L, a); lua_rawseti(L, t, 2 * n - 1);   // DESIGN: pair
	lua_pushnumber(L, b); lua_rawseti(L, t, 2 * n);   // DESIGN: pair
}

} // namespace KT1::IntelFactsLua

#endif
