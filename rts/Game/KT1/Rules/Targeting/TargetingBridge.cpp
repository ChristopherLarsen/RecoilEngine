/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 Targeting: Spring.KT1.Targeting (TargetingBridge.h) over TargetingApi.h, for the Lua that still runs (LUA-FREE S19).
// Reads answer in synced Lua only; writes do nothing elsewhere.

#include "TargetingBridge.h"

#include "TargetingApi.h"
#include "Lua/LuaHandle.h"
#include "Lua/LuaUtils.h"
#include "Lua/SupComTargeting.h"

namespace KT1::Targeting {

namespace TG = Core::Targeting;

namespace {

bool Synced(lua_State* L) { return CLuaHandle::GetHandleSynced(L) && Active(); }

int PushId(lua_State* L, int32_t id) {
	if (id < 0) return 0;
	lua_pushnumber(L, id);
	return 1;
}

int LTarget(lua_State* L) {
	if (!Synced(L)) return 0;
	return PushId(L, Target(luaL_checkint(L, 1), luaL_checkint(L, 2)));
}

// Lua: return bestReach, bestR (nothing when no weapon takes it)
int LEngage(lua_State* L) {
	if (!Synced(L)) return 0;
	const TG::Engagement e = Engage(luaL_checkint(L, 1), luaL_checkint(L, 2), luaL_optboolean(L, 3, false), luaL_optint(L, 4, 0));
	if (!e.found) return 0;
	lua_pushnumber(L, e.reach);
	lua_pushnumber(L, e.rank);
	return 2;
}

// Lua: return pReach, pR, bestReach, bestR (nils where no weapon takes it; nothing at all for no guns or hold fire)
int LEngage2(lua_State* L) {
	if (!Synced(L)) return 0;
	const TG::Engagement2 e = Engage2(luaL_checkint(L, 1), luaL_checkint(L, 2), luaL_optboolean(L, 3, false));
	if (!e.primary.found && !e.best.found) return 0;
	if (e.primary.found) { lua_pushnumber(L, e.primary.reach); lua_pushnumber(L, e.primary.rank); }
	else { lua_pushnil(L); lua_pushnil(L); }
	if (e.best.found) { lua_pushnumber(L, e.best.reach); lua_pushnumber(L, e.best.rank); }
	else { lua_pushnil(L); lua_pushnil(L); }
	return 4;
}

int LBestKey(lua_State* L) {
	if (!Synced(L)) return 0;
	int32_t kind = 0, rank = 0;
	if (!BestKey(luaL_checkint(L, 1), luaL_checkint(L, 2), kind, rank)) return 0;
	lua_pushnumber(L, kind);
	lua_pushnumber(L, rank);
	return 2;
}

int LFocus(lua_State* L) {
	if (!Synced(L)) return 0;
	return PushId(L, Focus(luaL_checkint(L, 1)));
}

int LSetFocus(lua_State* L) {
	if (!Synced(L)) return 0;
	SetFocus(luaL_checkint(L, 1), lua_isnumber(L, 2) ? lua_toint(L, 2) : TG::NONE);
	return 0;
}

int LInFlight(lua_State* L) {
	if (!Synced(L)) return 0;
	lua_pushnumber(L, InFlight(luaL_checkint(L, 1)));
	return 1;
}

int LContact(lua_State* L) {
	if (!Synced(L)) return 0;
	lua_pushboolean(L, Contact(luaL_checkint(L, 1), luaL_checkint(L, 2)));
	return 1;
}

int LManualFireRefusal(lua_State* L) {
	if (!Synced(L)) return 0;
	const TG::Refusal r = ManualFireRefusal(luaL_checkint(L, 1), luaL_checkint(L, 2));
	if (r == TG::Refusal::None) return 0;
	lua_pushstring(L, TG::RefusalName(r));
	return 1;
}

int LScanFrames(lua_State* L) {
	if (!Synced(L)) return 0;
	lua_pushnumber(L, ScanSteps(luaL_checkfloat(L, 1)));
	return 1;
}

int LCmdGunTarget(lua_State* L) {
	if (!Synced(L)) return 0;
	return PushId(L, CommandGunTarget(luaL_checkint(L, 1)));
}

int LCmdGunNextScan(lua_State* L) {
	if (!Synced(L)) return 0;
	return PushId(L, CommandGunNextScan(luaL_checkint(L, 1)));
}

int LStats(lua_State* L) {
	if (!Synced(L)) return 0;
	int scans = 0, candidates = 0, holds = 0;
	SupComTargeting::Stats(scans, candidates, holds);
	lua_pushnumber(L, scans);
	lua_pushnumber(L, candidates);
	lua_pushnumber(L, holds);
	return 3;
}

} // namespace

void PushTargetingBridge(lua_State* L) {
	LuaPushRawNamedCFunc(L, "Target", LTarget);
	LuaPushRawNamedCFunc(L, "Engage", LEngage);
	LuaPushRawNamedCFunc(L, "Engage2", LEngage2);
	LuaPushRawNamedCFunc(L, "BestKey", LBestKey);
	LuaPushRawNamedCFunc(L, "Focus", LFocus);
	LuaPushRawNamedCFunc(L, "SetFocus", LSetFocus);
	LuaPushRawNamedCFunc(L, "InFlight", LInFlight);
	LuaPushRawNamedCFunc(L, "Contact", LContact);
	LuaPushRawNamedCFunc(L, "ManualFireRefusal", LManualFireRefusal);
	LuaPushRawNamedCFunc(L, "ScanFrames", LScanFrames);
	LuaPushRawNamedCFunc(L, "CmdGunTarget", LCmdGunTarget);
	LuaPushRawNamedCFunc(L, "CmdGunNextScan", LCmdGunNextScan);
	lua_pushstring(L, "Debug");
	lua_createtable(L, 0, 1);
	LuaPushRawNamedCFunc(L, "Stats", LStats);
	lua_rawset(L, -3);
}

} // namespace KT1::Targeting
