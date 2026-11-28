/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 draw: Spring.KT1.Icons (see IconsBridge.h for the calls). Unsynced handles only.

#include <cstring>
#include <string>

#include "IconSet.h"
#include "IconsBridge.h"

#include "Game/KT1/Core/IconLexicon/IconClass.h"
#include "Lua/LuaHandle.h"
#include "LuaInclude.h"

namespace KT1 {
namespace {

namespace IL = Core::IconLexicon;

constexpr int VERSION = 3;        // DESIGN: WG.iconLex's version, which the HUD asks
constexpr int RECT_FIELDS = 5;    // DESIGN: unitID, x, y, w, h
constexpr int CHANNELS = 3;       // DESIGN: red, green, blue
constexpr int BADGES_ARG = 4;     // DESIGN: HudRml's fourth argument

int PushId(lua_State* L, int id) {
	if (id < 0) lua_pushnil(L);
	else lua_pushnumber(L, id);
	return 1;
}

int PushText(lua_State* L, const std::string& text) {
	lua_pushsstring(L, text);
	return 1;
}

int LVersion(lua_State* L) {
	lua_pushnumber(L, VERSION);
	return 1;
}

int LOk(lua_State* L) {
	const char* why = Icons::Failure();
	lua_pushboolean(L, why == nullptr);
	if (why == nullptr) return 1;
	lua_pushstring(L, why);
	return 2;
}

// the string at key `name` of the table at `index`; empty: unset
std::string Field(lua_State* L, int index, const char* name) {
	lua_getfield(L, index, name);
	const std::string value = lua_isstring(L, -1) ? lua_tostring(L, -1) : "";
	lua_pop(L, 1);
	return value;
}

void Flag(lua_State* L, int index, const char* name, bool& out) {
	lua_getfield(L, index, name);
	if (!lua_isnil(L, -1)) out = lua_toboolean(L, -1);
	lua_pop(L, 1);
}

void Number(lua_State* L, int index, const char* name, float& out) {
	lua_getfield(L, index, name);
	if (lua_israwnumber(L, -1)) out = lua_tofloat(L, -1);
	lua_pop(L, 1);
}

// the badges a caller names: { cond = "crit"|"dmg"|"fuel", act = "build"|"idle", st = bool, vet = level }
IL::Badges ReadBadges(lua_State* L, int index) {
	IL::Badges badges;
	if (!lua_istable(L, index)) return badges;
	const std::string cond = Field(L, index, "cond");
	if (cond == "crit") badges.condition = IL::Condition::Critical;
	else if (cond == "dmg") badges.condition = IL::Condition::Damaged;
	else if (cond == "fuel") badges.condition = IL::Condition::Fuel;
	const std::string act = Field(L, index, "act");
	if (act == "build") badges.activity = IL::Activity::Build;
	else if (act == "idle") badges.activity = IL::Activity::Idle;
	Flag(L, index, "st", badges.cloaked);
	float level = 0.0f;
	Number(L, index, "vet", level);
	badges.veterancy = static_cast<int>(level);
	return badges;
}

int LHudRml(lua_State* L) {
	const int defId = luaL_checkint(L, 1);
	const int px = luaL_checkint(L, 2);
	const int team = luaL_optint(L, 3, -1);
	return PushText(L, Icons::Markup(defId, px, team, ReadBadges(L, BADGES_ARG)));
}

int LUnitHudRml(lua_State* L) { return PushText(L, Icons::UnitMarkup(luaL_checkint(L, 1), luaL_checkint(L, 2))); }

int LEngineIconType(lua_State* L) {
	const std::string* type = Icons::EngineIconType(luaL_checkint(L, 1));
	if (type == nullptr) lua_pushnil(L);
	else lua_pushsstring(L, *type);
	return 1;
}

int LScale(lua_State* L) {
	lua_pushnumber(L, IL::NearestStep(Icons::Config().sizes, Icons::Options().iconScale));
	return 1;
}

int LSwitchDistance(lua_State* L) {
	const float distance = Icons::SwitchDistance();
	if (distance < 0.0f) lua_pushnil(L);
	else lua_pushnumber(L, distance);
	return 1;
}

int LPastSwitch(lua_State* L) {
	lua_pushboolean(L, Icons::PastSwitch());
	return 1;
}

int LHitAt(lua_State* L) { return PushId(L, Icons::HitAt(luaL_checkint(L, 1), luaL_checkint(L, 2))); }
int LHovered(lua_State* L) { return PushId(L, Icons::Hovered()); }

int LRects(lua_State* L) {
	const auto rects = Icons::Rects();
	lua_createtable(L, static_cast<int>(rects.size()), 0);
	int at = 0;
	for (const IL::IconRect& r: rects) {
		lua_createtable(L, RECT_FIELDS, 0);
		const float values[RECT_FIELDS] = {static_cast<float>(r.unit), r.x, r.y, r.w, r.h};
		for (int i = 0; i < RECT_FIELDS; ++i) {
			lua_pushnumber(L, values[i]);
			lua_rawseti(L, -2, i + 1);
		}
		lua_rawseti(L, -2, ++at);
	}
	lua_pushnumber(L, at);
	return 2;
}

int LKind(lua_State* L) {
	const char* kind = Icons::Kind(luaL_checkint(L, 1));
	if (kind == nullptr) lua_pushnil(L);
	else lua_pushstring(L, kind);
	return 1;
}

int LKindCount(lua_State* L) {
	lua_pushnumber(L, Icons::KindCount());
	return 1;
}

void Colour(lua_State* L, int index, const char* name, float (&out)[CHANNELS]) {
	lua_getfield(L, index, name);
	if (lua_istable(L, -1)) {
		for (int i = 0; i < CHANNELS; ++i) {
			lua_rawgeti(L, -1, i + 1);
			out[i] = lua_tofloat(L, -1);
			lua_pop(L, 1);
		}
	}
	lua_pop(L, 1);
}

// "palette": a table sets the three status colours, false takes them away, nil keeps what is set
void Palette(lua_State* L, int index, IL::HudOptions& options) {
	lua_getfield(L, index, "palette");
	if (lua_istable(L, -1)) {
		options.paletteChosen = true;
		const int palette = lua_gettop(L);
		Colour(L, palette, "healthy", options.healthy);
		Colour(L, palette, "damaged", options.damaged);
		Colour(L, palette, "critical", options.critical);
	} else if (!lua_isnil(L, -1)) {
		options.paletteChosen = false;
	}
	lua_pop(L, 1);
}

int LSetOptions(lua_State* L) {
	luaL_checktype(L, 1, LUA_TTABLE);
	IL::HudOptions options = Icons::Options();
	const std::string relation = Field(L, 1, "relation");
	if (relation == "off") options.relation = IL::RelationMode::Off;
	else if (relation == "minimap") options.relation = IL::RelationMode::Minimap;
	else if (relation == "everywhere") options.relation = IL::RelationMode::Everywhere;
	Number(L, 1, "scale", options.iconScale);
	Number(L, 1, "uiScale", options.uiScale);
	Flag(L, 1, "hidden", options.hidden);
	Flag(L, 1, "hideWithUI", options.iconsHideWithUi);
	Flag(L, 1, "minimapHidden", options.minimapHidden);
	Flag(L, 1, "bars", options.bars);
	Flag(L, 1, "enemybars", options.enemyBars);
	Palette(L, 1, options);
	Icons::SetOptions(options);
	return 0;
}

int LPause(lua_State* L) {
	Icons::SetPaused(lua_toboolean(L, 1));
	return 0;
}

int LFlickerTest(lua_State* L) {
	lua_pushboolean(L, Icons::FlickerTest(luaL_checkint(L, 1)));
	return 1;
}

} // namespace

void PushIconsBridge(lua_State* L) {
	if (CLuaHandle::GetHandleSynced(L)) return;
	const luaL_Reg calls[] = {{"Version", LVersion}, {"Ok", LOk}, {"HudRml", LHudRml}, {"UnitHudRml", LUnitHudRml},
	                          {"EngineIconType", LEngineIconType}, {"Scale", LScale}, {"SwitchDistance", LSwitchDistance},
	                          {"PastSwitch", LPastSwitch}, {"HitAt", LHitAt}, {"Rects", LRects}, {"Hovered", LHovered},
	                          {"Kind", LKind}, {"KindCount", LKindCount}, {"SetOptions", LSetOptions}, {"Pause", LPause},
	                          {"FlickerTest", LFlickerTest}};
	for (const luaL_Reg& call: calls) {
		lua_pushstring(L, call.name);
		lua_pushcfunction(L, call.func);
		lua_rawset(L, -3);
	}
}

} // namespace KT1
