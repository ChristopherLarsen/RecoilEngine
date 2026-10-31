/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */

#include "SupComMiniMapZoom.h"

#include "MiniMap.h"
#include "Lua/LuaUtils.h"
#include "Map/ReadMap.h"
#include "Rendering/GL/myGL.h"
#include "System/Config/ConfigHandler.h"
#include "System/Matrix44f.h"

#include <algorithm>

CONFIG(bool, SupComMiniMapZoom).defaultValue(true).description("supcom: the game may zoom the minimap into a region (Spring.SetMiniMapZoom); 0 ignores it, as stock Recoil.");

namespace {
	float centreX = 0.0f;
	float centreZ = 0.0f;
	float zoom = 1.0f;
	int setting = -1; // -1: not read yet

	bool SettingOn()
	{
		if (setting < 0)
			setting = configHandler->GetBool("SupComMiniMapZoom")? 1: 0;

		return (setting != 0);
	}

	float MapX() { return float(mapDims.mapx * SQUARE_SIZE); }
	float MapZ() { return float(mapDims.mapy * SQUARE_SIZE); }

	// the zoomed region's centre in normalized minimap coordinates (y up)
	float CentreGX() { return centreX / MapX(); }
	float CentreGY() { return 1.0f - centreZ / MapZ(); }
}

bool SupComMiniMapZoom::Active()
{
	if (zoom <= 1.0001f || !SettingOn())
		return false;

	return (minimap == nullptr || minimap->GetRotationOption() == CMiniMap::ROTATION_0);
}

float SupComMiniMapZoom::Zoom()
{
	return (Active()? zoom: 1.0f);
}

void SupComMiniMapZoom::ApplyGL()
{
	if (!Active())
		return;

	glTranslatef(0.5f, 0.5f, 0.0f);
	glScalef(zoom, zoom, 1.0f);
	glTranslatef(-CentreGX(), -CentreGY(), 0.0f);
}

void SupComMiniMapZoom::ApplyToViewMat(CMatrix44f& m)
{
	if (!Active())
		return;

	CMatrix44f z;
	z.Translate(0.5f, 0.5f, 0.0f);
	z.Scale({zoom, zoom, 1.0f});
	z.Translate(-CentreGX(), -CentreGY(), 0.0f);
	m = z * m;
}

void SupComMiniMapZoom::Unzoom(float& gx, float& gy)
{
	if (!Active())
		return;

	gx = (gx - 0.5f) / zoom + CentreGX();
	gy = (gy - 0.5f) / zoom + CentreGY();
}

void SupComMiniMapZoom::ZoomCoords(float& gx, float& gy)
{
	if (!Active())
		return;

	gx = (gx - CentreGX()) * zoom + 0.5f;
	gy = (gy - CentreGY()) * zoom + 0.5f;
}

static void SetZoom(float x, float z, float zm)
{
	zoom = std::clamp(zm, 1.0f, 4.0f);

	// the region stays on the map
	const float hx = MapX() * 0.5f / zoom;
	const float hz = MapZ() * 0.5f / zoom;
	centreX = std::clamp(x, hx, MapX() - hx);
	centreZ = std::clamp(z, hz, MapZ() - hz);

	// Lua's minimap view matrices follow (CMiniMap::UpdateGeometry)
	if (minimap != nullptr)
		minimap->UpdateGeometry();
}

static int SetMiniMapZoom(lua_State* L)
{
	SetZoom(luaL_checkfloat(L, 1), luaL_checkfloat(L, 2), luaL_optfloat(L, 3, 1.0f));
	return 0;
}

static int GetMiniMapZoom(lua_State* L)
{
	lua_pushnumber(L, (zoom > 1.0001f)? centreX: MapX() * 0.5f);
	lua_pushnumber(L, (zoom > 1.0001f)? centreZ: MapZ() * 0.5f);
	lua_pushnumber(L, SupComMiniMapZoom::Zoom());
	return 3;
}

static int MiniMapToWorld(lua_State* L)
{
	if (minimap == nullptr)
		return 0;

	const float sx = luaL_checkfloat(L, 1);
	const float sy = luaL_checkfloat(L, 2);
	float gx = (sx - minimap->GetPosX()) / std::max(1, minimap->GetSizeX());
	float gy = (sy - minimap->GetPosY()) / std::max(1, minimap->GetSizeY());

	if (gx < 0.0f || gx > 1.0f || gy < 0.0f || gy > 1.0f)
		return 0;

	SupComMiniMapZoom::Unzoom(gx, gy);
	lua_pushnumber(L, gx * MapX());
	lua_pushnumber(L, (1.0f - gy) * MapZ());
	return 2;
}

static int WorldToMiniMap(lua_State* L)
{
	if (minimap == nullptr)
		return 0;

	float gx = luaL_checkfloat(L, 1) / MapX();
	float gy = 1.0f - luaL_checkfloat(L, 2) / MapZ();
	SupComMiniMapZoom::ZoomCoords(gx, gy);

	if (gx < 0.0f || gx > 1.0f || gy < 0.0f || gy > 1.0f)
		return 0;

	lua_pushnumber(L, minimap->GetPosX() + gx * minimap->GetSizeX());
	lua_pushnumber(L, minimap->GetPosY() + gy * minimap->GetSizeY());
	return 2;
}

void SupComMiniMapZoom::PushUnsyncedCtrl(lua_State* L)
{
	LuaPushRawNamedCFunc(L, "SetMiniMapZoom", SetMiniMapZoom);
}

void SupComMiniMapZoom::PushUnsyncedRead(lua_State* L)
{
	LuaPushRawNamedCFunc(L, "GetMiniMapZoom", GetMiniMapZoom);
	LuaPushRawNamedCFunc(L, "MiniMapToWorld", MiniMapToWorld);
	LuaPushRawNamedCFunc(L, "WorldToMiniMap", WorldToMiniMap);
}
