/* This file is part of the Recoil engine fork of Kardashev Type One (GPL v2 or later). */
#include "SupComZoomLevel.h"

#include "Game/Camera.h"
#include "Game/CameraHandler.h"
#include "Game/Camera/CameraController.h"
#include "Map/Ground.h"
#include "Map/ReadMap.h"
#include "System/Config/ConfigHandler.h"
#include "System/EventHandler.h"

#include <cstdlib>

CONFIG(int, SupComZoomSkip).defaultValue(0).minimumValue(0).description(
	"Kardashev: draw passes skipped at zoom level L4 and out (bits: 1 units, 2 features, 4 projectiles, 8 particles, "
	"16 decals, 32 foliage, 64 sky). Presentation only.");

CONFIG(int, SupComShadowMaxLevel).defaultValue(7).minimumValue(-1).maximumValue(7).description(
	"Kardashev: the shadow-map pass runs only at zoom level L0..this value (7 = always, -1 = never). Above it the shadow "
	"map is cleared once and the pass is skipped, so nothing casts a shadow. Presentation only.");
CONFIG(int, SupComDecalMaxLevel).defaultValue(7).minimumValue(-1).maximumValue(7).description(
	"Kardashev: ground decals (scars, tracks, building plates, Lua decals) are drawn only at zoom level L0..this value "
	"(7 = always). Presentation only.");

namespace SupComZoomLevel {
	// split points L0/L1 .. L6/L7 in elmos; the same values as LuaUI/lib/zoomlevel.lua. Index 3 is icon_zoom (no hysteresis).
	static const float BOUNDS[MAX_LEVEL] = {450.0f, 820.0f, 930.0f, 1040.0f, 2000.0f, 4000.0f, 8000.0f};
	static constexpr int ICON_INDEX = 3;
	static constexpr float HYST = 0.05f;

	static int curLevel = 0;
	static float curDist = 0.0f;
	static unsigned skipMask = 0;
	static bool started = false;
	static int shadowMax = MAX_LEVEL;
	static int decalMax = MAX_LEVEL;

	void Init() { skipMask = unsigned(configHandler->GetInt("SupComZoomSkip")); started = false; curLevel = 0; }

	static int Raw(float d) {
		for (int i = 0; i < MAX_LEVEL; i++)
			if (d < BOUNDS[i]) return i;
		return MAX_LEVEL;
	}

	static bool WholeMapInView() {
		const float x = float(mapDims.mapx * SQUARE_SIZE), z = float(mapDims.mapy * SQUARE_SIZE);
		return camera->InView(float3(0, 0, 0)) && camera->InView(float3(x, 0, 0))
			&& camera->InView(float3(0, 0, z)) && camera->InView(float3(x, 0, z));
	}

	void Update() {
		shadowMax = configHandler->GetInt("SupComShadowMaxLevel");
		decalMax = configHandler->GetInt("SupComDecalMaxLevel");
		const float d = camHandler->GetCurrentController().GetSupComDist();
		const float3 cp = camera->GetPos();
		curDist = (d >= 0.0f) ? d : (cp.y - CGround::GetHeightReal(cp.x, cp.z, false));
		int lv = Raw(curDist);
		if (lv < MAX_LEVEL && curDist >= BOUNDS[MAX_LEVEL - 1] * 0.5f && WholeMapInView())
			lv = MAX_LEVEL;
		if (started && std::abs(lv - curLevel) == 1) {
			const int i = (lv > curLevel) ? lv - 1 : lv;    // the split point between the two levels
			if (i != ICON_INDEX) {
				if (lv > curLevel && curDist < BOUNDS[i] * (1.0f + HYST)) lv = curLevel;
				if (lv < curLevel && curDist >= BOUNDS[i] * (1.0f - HYST)) lv = curLevel;
			}
		}
		const int old = curLevel;
		curLevel = lv;
		if (started && lv != old)
			eventHandler.SupComZoomLevelChanged(lv, old);
		started = true;
	}

	int Get() { return curLevel; }
	float Dist() { return curDist; }
	bool Skip(Pass p) { return (skipMask & p) != 0 && curLevel >= SKIP_FROM; }
	bool ShadowsOff() { return curLevel > shadowMax; }
	bool DecalsOff() { return curLevel > decalMax; }
}
