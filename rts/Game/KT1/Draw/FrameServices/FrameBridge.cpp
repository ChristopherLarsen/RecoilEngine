/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// Spring.KT1.Frame (FrameBridge.h; LUA-FREE V01). Unsynced only: the selection, the pointer and the camera are one
// player's, so a synced handle gets no functions (PRF-002 R3/R6).

#include "FrameBridge.h"

#include "FrameRefresh.h"

#include "Lua/LuaHandle.h"
#include "lib/lua/include/LuaInclude.h"

namespace KT1 {
namespace {

namespace Frame = FrameServices;
using Core::View::FrameWhat;

// registry keys (their addresses): the shared tables of one Lua state and what they were filled from
char SELECTION_TABLE, SELECTION_STAMP, CAMERA_TABLE, CAMERA_STAMP, CAMERA_NAME;

void GetSlot(lua_State* L, const char* key) {
	lua_pushlightuserdata(L, const_cast<char*>(key));
	lua_rawget(L, LUA_REGISTRYINDEX);
}

// stores the value on top of the stack and pops it
void SetSlot(lua_State* L, const char* key) {
	lua_pushlightuserdata(L, const_cast<char*>(key));
	lua_insert(L, -2);
	lua_rawset(L, LUA_REGISTRYINDEX);
}

// true: the table under `table` was filled at this change count; it is left on the stack either way
bool Current(lua_State* L, const char* table, const char* stamp, uint32_t count) {
	GetSlot(L, stamp);
	const bool same = lua_isnumber(L, -1) && static_cast<uint32_t>(lua_tonumber(L, -1)) == count;
	lua_pop(L, 1);
	GetSlot(L, table);
	return same && lua_istable(L, -1);
}

void Stamp(lua_State* L, const char* stamp, uint32_t count) {
	lua_pushnumber(L, count);
	SetSlot(L, stamp);
}

int LSelection(lua_State* L) {
	const std::vector<int>& ids = Frame::Selection().selection;
	const uint32_t count = Frame::Changes(FrameWhat::Selection);
	const int n = static_cast<int>(ids.size());
	if (!Current(L, &SELECTION_TABLE, &SELECTION_STAMP, count)) {
		// a new table, so a reader that kept the old one still holds the old selection, as with Spring.GetSelectedUnits
		lua_pop(L, 1);
		lua_createtable(L, n, 0);
		for (int i = 0; i < n; ++i) {
			lua_pushnumber(L, ids[i]);
			lua_rawseti(L, -2, i + 1);
		}
		lua_pushvalue(L, -1);
		SetSlot(L, &SELECTION_TABLE);
		Stamp(L, &SELECTION_STAMP, count);
	}
	lua_pushinteger(L, n);
	lua_pushnumber(L, count);
	return 3;
}

int LIsSelected(lua_State* L) {
	lua_pushboolean(L, Frame::IsSelected(luaL_checkint(L, 1)));
	return 1;
}

void PushPoint(lua_State* L, const float3& p) {
	lua_pushnumber(L, p.x);
	lua_pushnumber(L, p.y);
	lua_pushnumber(L, p.z);
}

int LHover(lua_State* L) {
	const PointerFacts& p = Frame::Pointer().hover.pointer;
	const char* kind = nullptr;
	int id = -1;
	switch (p.hit.kind) {
		case RayHit::Unit: kind = "unit"; id = p.hit.unitId; break;
		case RayHit::Feature: kind = "feature"; id = p.hit.featureId; break;
		case RayHit::Ground: kind = "ground"; break;
		case RayHit::Sky: kind = "sky"; break;
		default: break;
	}
	if (kind != nullptr) lua_pushstring(L, kind); else lua_pushnil(L);
	if (id >= 0) lua_pushinteger(L, id); else lua_pushnil(L);
	if (p.hit.hasGround) {
		PushPoint(L, p.hit.ground);
	} else {
		for (int i = 0; i < 3; ++i) lua_pushnil(L);   // DESIGN: x, y, z
	}
	lua_pushboolean(L, p.overMiniMap);
	return 6;
}

int LHoverGround(lua_State* L) {
	const Frame::HoverState& h = Frame::Pointer().hover;
	if (h.onMiniMapGround) {
		PushPoint(L, h.miniMapGround);
		return 3;
	}
	if (!h.pointer.hit.hasGround) return 0;
	PushPoint(L, h.pointer.hit.ground);
	return 3;
}

int LViewCentre(lua_State* L) {
	const Frame::HoverState& h = Frame::Centre().hover;
	if (!h.centreOnGround) return 0;
	PushPoint(L, h.centreGround);
	return 3;
}

void SetField(lua_State* L, const char* name, float value) {
	lua_pushstring(L, name);
	lua_pushnumber(L, value);
	lua_rawset(L, -3);
}

// every value of Spring.GetCameraState, plus vsx and vsy
void FillCamera(lua_State* L, const Frame::CameraState& cam) {
	for (const auto& value : Frame::CameraValues()) SetField(L, value.first.c_str(), value.second);
	SetField(L, "vsx", static_cast<float>(cam.viewW));
	SetField(L, "vsy", static_cast<float>(cam.viewH));
	lua_pushliteral(L, "name");
	lua_pushstring(L, Frame::CameraName().c_str());
	lua_rawset(L, -3);
}

int LCamera(lua_State* L) {
	const Frame::CameraState& cam = Frame::Camera().cam;
	const uint32_t count = Frame::Changes(FrameWhat::Camera);
	if (Current(L, &CAMERA_TABLE, &CAMERA_STAMP, count)) return 1;

	GetSlot(L, &CAMERA_NAME);
	const bool sameController = lua_isstring(L, -1) && Frame::CameraName() == lua_tostring(L, -1);
	lua_pop(L, 1);
	if (!sameController || !lua_istable(L, -1)) {   // another controller has other keys: start a fresh table
		lua_pop(L, 1);
		lua_createtable(L, 0, static_cast<int>(Frame::CameraValues().size()) + 3);   // DESIGN: name, vsx, vsy
		lua_pushvalue(L, -1);
		SetSlot(L, &CAMERA_TABLE);
		lua_pushstring(L, Frame::CameraName().c_str());
		SetSlot(L, &CAMERA_NAME);
	}
	FillCamera(L, cam);
	Stamp(L, &CAMERA_STAMP, count);
	return 1;
}

int LMouse(lua_State* L) {
	const Frame::MouseState& m = Frame::Mouse().mouse;
	lua_pushnumber(L, m.x);
	lua_pushnumber(L, m.y);
	for (const bool b : {m.left, m.middle, m.right, m.offscreen, m.alt, m.ctrl, m.meta, m.shift}) lua_pushboolean(L, b);
	return 10;   // DESIGN: x, y and the eight flags
}

int LZoom(lua_State* L) {
	const Frame::FrameState& s = Frame::Zoom();
	lua_pushinteger(L, s.zoomLevel);
	lua_pushnumber(L, s.zoomDist);
	lua_pushboolean(L, s.wholeMap);
	return 3;
}

int LChanged(lua_State* L) {
	const FrameWhat what = Core::View::ParseFrameWhat(luaL_checkstring(L, 1));
	if (what == FrameWhat::Count) return luaL_error(L, "Spring.KT1.Frame.Changed: unknown part '%s'", lua_tostring(L, 1));
	lua_pushnumber(L, Frame::Changes(what));
	return 1;
}

} // namespace

void PushFrameBridge(lua_State* L) {
	if (CLuaHandle::GetHandleSynced(L)) return;
	const luaL_Reg functions[] = {
		{"Selection", LSelection}, {"IsSelected", LIsSelected}, {"Hover", LHover}, {"HoverGround", LHoverGround},
		{"ViewCentre", LViewCentre}, {"Camera", LCamera}, {"Mouse", LMouse}, {"Zoom", LZoom}, {"Changed", LChanged},
	};
	for (const luaL_Reg& f : functions) {
		lua_pushcfunction(L, f.func);
		lua_setfield(L, -2, f.name);
	}
}

} // namespace KT1
