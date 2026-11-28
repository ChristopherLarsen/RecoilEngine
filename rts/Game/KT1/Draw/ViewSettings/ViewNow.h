/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 ViewSettings: what its parts read once per drawn frame (the camera) and once per game (the mod options).

#ifndef KT1_DRAW_VIEW_NOW_H
#define KT1_DRAW_VIEW_NOW_H

#include <string>

#include "System/float3.h"

namespace KT1::ViewSet {

struct ViewNow {
	float zoomDist = -1.0f;      // the camera controller's zoom distance, elmos; < 0: this camera mode has none
	float dist = 0.0f;           // zoomDist, or the camera's height above the ground
	float lookHeight = 0.0f;     // the camera state's height, else its distance, else its y
	float3 pos;
	float fov = 45.0f;           // DESIGN: degrees, until the camera is read
	int viewH = 1;
};

struct ViewOptions {
	bool frontend = false;
	bool cullModels = true, cullEarly = true, cullProps = true, wreckCull = true, zoomSwitch = true;
	bool shadowCap = true, shadowLog = false;
	bool gates = true, reflGate = true, deferred = true;
	int shadowMaxOverride = -1;  // dzg_shadow_max; < 0: none
	bool waterFar = false, waterGuard = true;
	float propScale = -1.0f;     // prop_lod_scale; < 0: the config's
};

ViewNow ReadCamera();
ViewOptions ReadOptions();
std::string ModOption(const char* name);

} // namespace KT1::ViewSet

#endif
