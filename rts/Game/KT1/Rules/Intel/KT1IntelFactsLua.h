/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 rule IntelFacts: helpers of its adapter (KT1IntelFacts.cpp only): Lua tables, a unit's batch flags and layer.

#ifndef KT1_INTEL_FACTS_LUA_H
#define KT1_INTEL_FACTS_LUA_H

#include <cstring>
#include <cstdint>
#include <vector>

#include <string>

#include "Lua/LuaUtils.h"
#include "Sim/MoveTypes/SupComGroundMoveType.h"
#include "Sim/Units/Unit.h"
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

// DESIGN: the batch flags fl (SupComIntFactsRun's fin and Lua's F.SrcLua read them): carried, active, being built, cloaked, sub
constexpr int FL_CARRIED = 1, FL_ACTIVE = 2, FL_BUILDING = 4, FL_CLOAKED = 8, FL_SUB = 16;   // DESIGN

// the unit's move layer (CSupComGroundMoveType): one cast and no string per read
constexpr int LAYER_NONE = 0, LAYER_SUB = 1, LAYER_WATER = 2;   // DESIGN
inline int Layer(const CUnit* u) {
	const auto* gm = dynamic_cast<const CSupComGroundMoveType*>(u->moveType);
	const char* n = gm != nullptr ? gm->GetLayerName() : nullptr;
	if (n == nullptr) return LAYER_NONE;
	if (std::strcmp(n, "sub") == 0) return LAYER_SUB;
	return std::strcmp(n, "water") == 0 ? LAYER_WATER : LAYER_NONE;
}

inline int Flags(const CUnit* u, int layer) {
	int fl = 0;
	if (u->GetTransporter() != nullptr) fl |= FL_CARRIED;
	if (u->activated) fl |= FL_ACTIVE;
	if (u->beingBuilt) fl |= FL_BUILDING;
	if (u->isCloaked) fl |= FL_CLOAKED;
	if (layer == LAYER_SUB) fl |= FL_SUB;
	return fl;
}

inline bool Submerged(const CUnit* unit, float y, int layer) {   // int_coverage's facts loop
	if (!(y < 0.0f)) return false;
	if (layer == LAYER_SUB) return true;
	if (layer == LAYER_WATER) return false;
	return y + unit->height < 0.0f;
}

// Fact(u, "row"), check mode: the rest of the row WriteRow writes (py, vx, vz, vkey, skey, fk, ck, fb, cb), and whether pcloak is written false
template <typename Row>
inline int PushRow(lua_State* L, const Row& o, bool cloakFalse) {
	lua_pushnumber(L, o.py); lua_pushnumber(L, o.px); lua_pushnumber(L, o.pz);
	lua_pushnumber(L, o.vkey); lua_pushnumber(L, o.skey); lua_pushnumber(L, o.fk); lua_pushnumber(L, o.ck);
	lua_pushnumber(L, o.fb); lua_pushnumber(L, o.cb);
	lua_pushboolean(L, cloakFalse);
	return 10;   // DESIGN: the values above
}

} // namespace KT1::IntelFactsLua

#endif
