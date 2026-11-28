/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 core: ViewSettings' water, reflection, ground look and minimap rules and their state (LUA-FREE H01, FRAME §4;
// replaces gfx_zoomswitch, gfx_water_far, gfx_water_mode_guard, map_zoom_look and the libraries zoomswitch and
// zoomrates). No engine include.

#ifndef KT1_CORE_VIEW_SCENE_STATE_H
#define KT1_CORE_VIEW_SCENE_STATE_H

#include "ZoomLevel.h"

namespace KT1::Core::View {

constexpr int NO_WATER_MODE = -1;   // nothing to set

struct GroundLook {
	float splat[4] = {};            // the detail layer strengths
	float ambient[3] = {};
	float diffuse[3] = {};
};

struct SceneState {
	float switchLevel = 0.0f;       // the player's zoom switch setting, a fraction
	float reflection = 1.0f;        // the strength written last
	float lookFar = -1.0f;          // the look factors written last; below 0: none yet
	float lookNear = 0.0f;
	int wantedWater = NO_WATER_MODE;   // the player's mode to restore when close
	bool waterFar = false;
};

// the setting's step for a value in percent or as a fraction; the default when it is none of them
float ZoomSwitchLevel(const ZoomSwitchConfig& config, float value);
float TankPixels(float distance, int viewH, float fovDeg, float length);
float DistanceForPixels(float pixels, int viewH, float fovDeg, float length);
// 0..1, quantised to strength_steps
float ReflectionStrength(const ZoomSwitchConfig& config, float distance, float level, int viewH, float fovDeg);
// the distance at which a prop of size 0 (small) .. 1 (large) is gone, and its alpha 1..0 at `distance`
float PropFadeEnd(const ViewSettingsConfig& config, float size, float level);
float PropFadeAlpha(const ViewSettingsConfig& config, float distance, float size, float level);

float LookFar(const GroundLookConfig& config, float height);    // 0 play look .. 1 strategic
float LookNear(const GroundLookConfig& config, float height);   // 1 close-zoom dimming .. 0 none
bool LookMoved(const SceneState& state, const GroundLookConfig& config, float far, float near);
GroundLook ApplyLook(const GroundLookConfig& config, const GroundLook& base, float far, float near);

// the water mode to set on a level change (far_cut on), or NO_WATER_MODE
int FarWaterMode(SceneState& state, const WaterConfig& config, int level, int currentMode);
int GuardWaterMode(const WaterConfig& config, int currentMode);   // the safe mode for the dynamic one

float MinimapGapSeconds(const MinimapRateConfig& config, int level);
bool MinimapLayersOff(const MinimapRateConfig& config, int level, bool wholeMap);

} // namespace KT1::Core::View

#endif
