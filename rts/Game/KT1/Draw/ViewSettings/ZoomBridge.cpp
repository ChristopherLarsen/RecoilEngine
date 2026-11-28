/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// Spring.KT1.Zoom (ZoomBridge.h). Every call that writes is refused to synced Lua; synced code must not read either
// (PRF-002 R3/R6), so synced callers get nothing.

#include "ZoomBridge.h"

#include "ViewShared.h"
#include "ZoomGatesApi.h"

#include "Lua/LuaHandle.h"
#include "lib/lua/include/LuaInclude.h"
#include "Lua/LuaUtils.h"
#include "System/Config/ConfigHandler.h"

namespace KT1 {
namespace {

namespace View = Core::View;
using ViewSet::Shared;

constexpr float PERCENT = 100.0f;   // DESIGN: fraction to percent

bool Synced(lua_State* L) { return CLuaHandle::GetHandleSynced(L); }

int SettingInt(const std::string& key, int fallback) {
	return configHandler->IsSet(key) ? configHandler->GetInt(key) : fallback;
}

float SwitchLevel() {
	const View::ZoomSwitchConfig& z = Shared().config.zoom_switch;
	const int fallback = static_cast<int>(z.default_level * PERCENT + 0.5f);   // DESIGN: fraction to percent
	return View::ZoomSwitchLevel(z, static_cast<float>(SettingInt(z.setting, fallback)));
}

int LLevel(lua_State* L) {
	if (Synced(L)) return 0;
	lua_pushinteger(L, Shared().view.level);
	lua_pushnumber(L, Shared().view.distance);
	return 2;
}

int LWholeMap(lua_State* L) {
	if (Synced(L)) return 0;
	lua_pushboolean(L, Shared().view.wholeMap);
	return 1;
}

int LBounds(lua_State* L) {
	if (Synced(L)) return 0;
	lua_createtable(L, View::ZOOM_LEVELS - 1, 0);
	for (int i = 0; i < View::ZOOM_LEVELS - 1; ++i) {
		lua_pushnumber(L, Shared().config.bounds.upper[i]);
		lua_rawseti(L, -2, i + 1);
	}
	lua_pushinteger(L, View::ZOOM_LEVELS - 1);
	return 2;
}

int LChanges(lua_State* L) {
	if (Synced(L)) return 0;
	lua_pushnumber(L, Shared().view.changes);
	return 1;
}

int LGate(lua_State* L) {
	if (Synced(L)) return 0;
	const ZoomGates::GateId g = ZoomGates::Gate(luaL_checkstring(L, 1));
	if (g == ZoomGates::NO_GATE) return 0;
	lua_pushboolean(L, ZoomGates::GateOpen(g));
	lua_pushnumber(L, ZoomGates::GateValue(g, 0.0f));
	lua_pushnumber(L, ZoomGates::GateStart(g, 0.0f));
	lua_pushnumber(L, ZoomGates::GateEnd(g, View::NO_END));
	return 4;
}

int LDetailFactor(lua_State* L) {
	if (Synced(L)) return 0;
	lua_pushnumber(L, ZoomGates::DetailFactor(luaL_checkstring(L, 1)));
	return 1;
}

int LMinimapGap(lua_State* L) {
	if (Synced(L)) return 0;
	lua_pushnumber(L, View::MinimapGapSeconds(Shared().config.minimap, Shared().view.level));
	return 1;
}

int LMinimapLayersOff(lua_State* L) {
	if (Synced(L)) return 0;
	lua_pushboolean(L, ZoomGates::MinimapLayersOff());
	return 1;
}

int LShadowDistance(lua_State* L) {
	if (Synced(L)) return 0;
	const View::ShadowCutConfig& c = Shared().config.shadow_cut;
	const int setting = View::ShadowClamp(c, static_cast<float>(SettingInt(c.setting, c.setting_default)));
	lua_pushinteger(L, setting);
	lua_pushboolean(L, Shared().shadow.far);
	lua_pushnumber(L, View::ShadowCutZoom(c, setting, true));
	return 3;
}

int LSetShadowDistance(lua_State* L) {
	if (Synced(L)) return 0;
	const View::ShadowCutConfig& c = Shared().config.shadow_cut;
	configHandler->Set(c.setting, View::ShadowClamp(c, luaL_checkfloat(L, 1)));
	return 0;
}

int LShadowFix(lua_State* L) {
	if (Synced(L)) return 0;
	const uint8_t mask = static_cast<uint8_t>(luaL_checkint(L, 1));
	lua_pushinteger(L, View::ShadowMask(Shared().config.shadow_cut, mask, Shared().shadow.far));
	return 1;
}

int LZoomSwitch(lua_State* L) {
	if (Synced(L)) return 0;
	lua_pushnumber(L, SwitchLevel());
	lua_pushnumber(L, Shared().scene.reflection);
	lua_pushboolean(L, true);
	return 3;
}

int LZoomSwitchLevels(lua_State* L) {
	if (Synced(L)) return 0;
	const auto& levels = Shared().config.zoom_switch.levels;
	lua_createtable(L, 3, 0);
	int i = 0;
	for (const float level : levels) {
		lua_pushnumber(L, level);
		lua_rawseti(L, -2, ++i);
	}
	return 1;
}

int LSetZoomSwitch(lua_State* L) {
	if (Synced(L)) return 0;
	float value = luaL_checkfloat(L, 1);
	if (value > 1) value /= PERCENT;
	const View::ZoomSwitchConfig& z = Shared().config.zoom_switch;
	const float level = View::ZoomSwitchLevel(z, value);
	const bool known = level == value;
	if (known) configHandler->Set(z.setting, static_cast<int>(level * PERCENT + 0.5f));   // DESIGN: fraction to percent
	lua_pushboolean(L, known);
	return 1;
}

int LSetEngineFog(lua_State* L) {
	if (Synced(L)) return 0;
	Shared().engineFog = lua_toboolean(L, 1) != 0;
	return 0;
}

int LReport(lua_State* L) {
	if (Synced(L)) return 0;
	Shared().view.log.Record(luaL_checkstring(L, 1), luaL_checkint(L, 2), luaL_checkfloat(L, 3));
	return 0;
}

} // namespace

void PushZoomBridge(lua_State* L) {
	const std::pair<const char*, lua_CFunction> calls[] = {
		{"Level", LLevel}, {"WholeMap", LWholeMap}, {"Bounds", LBounds}, {"Changes", LChanges}, {"Gate", LGate},
		{"DetailFactor", LDetailFactor}, {"MinimapGap", LMinimapGap}, {"MinimapLayersOff", LMinimapLayersOff},
		{"ShadowDistance", LShadowDistance}, {"SetShadowDistance", LSetShadowDistance}, {"ShadowFix", LShadowFix},
		{"ZoomSwitch", LZoomSwitch}, {"ZoomSwitchLevels", LZoomSwitchLevels}, {"SetZoomSwitch", LSetZoomSwitch},
		{"SetEngineFog", LSetEngineFog}, {"Report", LReport},
	};
	for (const auto& [name, fn] : calls) LuaPushRawNamedCFunc(L, name, fn);
}

} // namespace KT1
