/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */

#ifndef SUPCOM_MINIMAP_ZOOM_H
#define SUPCOM_MINIMAP_ZOOM_H

struct lua_State;
class CMatrix44f;

// supcom: springsetting SupComMiniMapZoom (default 1; 0 ignores the calls, as stock). The game can zoom the minimap
// into a region (unsynced Lua Spring.SetMiniMapZoom(centreX, centreZ, zoom), zoom 1 = the whole map, up to 4; the
// centre is clamped so the region stays on the map): terrain, unit icons (kept at their size), the camera frustum,
// selection and Lua's DrawInMiniMap (its 0..1 map coordinates go through the zoom) all show that region, and minimap
// clicks, drags and orders map through it. Spring.GetMiniMapZoom, Spring.MiniMapToWorld(sx, sy) and
// Spring.WorldToMiniMap(x, z) (screen pixels, bottom-left origin) respect it. Only with the minimap unrotated.
namespace SupComMiniMapZoom {
	/// a zoom above 1 is set, the setting is on and the minimap is unrotated
	bool Active();
	float Zoom();

	/// the zoom on the current GL modelview matrix, in normalized minimap coordinates (0..1, y up)
	void ApplyGL();
	/// the zoom applied after a world (or heightmap) to normalized minimap transform
	void ApplyToViewMat(CMatrix44f& m);
	/// normalized minimap coordinates (y up) of the zoomed view <-> of the whole map
	void Unzoom(float& gx, float& gy);
	void ZoomCoords(float& gx, float& gy);

	/// LuaUnsyncedCtrl: SetMiniMapZoom; LuaUnsyncedRead: GetMiniMapZoom, MiniMapToWorld, WorldToMiniMap
	void PushUnsyncedCtrl(lua_State* L);
	void PushUnsyncedRead(lua_State* L);
}

#endif
