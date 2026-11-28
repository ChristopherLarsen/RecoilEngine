/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 core: reflections by zoom (zoomswitch.lua), the ground look (map_zoom_look.lua), the water modes
// (gfx_water_far, gfx_water_mode_guard) and the minimap rates (zoomrates.lua). LUA-FREE D07. Floats, as the engine's
// Lua computed.

#include "ViewSceneState.h"

#include <algorithm>
#include <cmath>

namespace KT1::Core::View {
namespace {

constexpr float PI_F = 3.14159265358979323846f;   // DESIGN: maths of the ported rule (smoothstep, half angle, percent, channel counts)
constexpr float HALF_TURN_DEG = 180.0f;   // DESIGN: maths of the ported rule (smoothstep, half angle, percent, channel counts)
constexpr float PERCENT = 100.0f;   // DESIGN: maths of the ported rule (smoothstep, half angle, percent, channel counts)

float Smooth(float t) {
	t = std::clamp(t, 0.0f, 1.0f);
	return t * t * (3 - 2 * t);   // DESIGN: maths of the ported rule (smoothstep, half angle, percent, channel counts)
}

float TanHalf(float fovDeg) { return std::tan(fovDeg * PI_F / HALF_TURN_DEG * 0.5f); }   // DESIGN: maths of the ported rule (smoothstep, half angle, percent, channel counts)

// where the map-object fade starts: at play zoom, never nearer than the closest view
float PropFadeClose(const ViewSettingsConfig& config) {
	return std::max(config.zoom_switch.close_elmos, config.props.fade_start_elmos);
}

} // namespace

float ZoomSwitchLevel(const ZoomSwitchConfig& config, float value) {
	if (value > 1) value /= PERCENT;
	for (const float level : config.levels)
		if (std::fabs(level - value) * PERCENT < 0.5f) return level;   // DESIGN: maths of the ported rule (smoothstep, half angle, percent, channel counts)
	return config.default_level;
}

float TankPixels(float distance, int viewH, float fovDeg, float length) {
	return length * viewH / (2 * distance * TanHalf(fovDeg));   // DESIGN: maths of the ported rule (smoothstep, half angle, percent, channel counts)
}

float DistanceForPixels(float pixels, int viewH, float fovDeg, float length) {
	return length * viewH / (2 * pixels * TanHalf(fovDeg));   // DESIGN: maths of the ported rule (smoothstep, half angle, percent, channel counts)
}

float ReflectionStrength(const ZoomSwitchConfig& config, float distance, float level, int viewH, float fovDeg) {
	const float close = config.close_elmos;
	const float offPx = config.reflection_off_px;
	float off = DistanceForPixels(offPx, viewH, fovDeg, config.tank_length);
	float full = DistanceForPixels(offPx + config.reflection_fade_span, viewH, fovDeg, config.tank_length);
	off = close + level * (off - close);
	full = close + level * (full - close);
	float s = 0;
	if (distance <= full) s = 1;
	else if (distance < off) s = Smooth((off - distance) / (off - full));
	return std::floor(s * config.strength_steps + 0.5f) / config.strength_steps;   // DESIGN: maths of the ported rule (smoothstep, half angle, percent, channel counts)
}

float PropFadeEnd(const ViewSettingsConfig& config, float size, float level) {
	const float f = config.props.small_fade_end +
	                std::clamp(size, 0.0f, 1.0f) * (config.props.large_fade_end - config.props.small_fade_end);
	const float close = PropFadeClose(config);
	return close + level * f * (config.zoom_switch.icon_elmos - close);
}

float PropFadeAlpha(const ViewSettingsConfig& config, float distance, float size, float level) {
	const float close = PropFadeClose(config);
	const float end = PropFadeEnd(config, size, level);
	if (distance <= close) return 1;
	if (distance >= end) return 0;
	return 1 - Smooth((distance - close) / (end - close));
}

float LookFar(const GroundLookConfig& config, float height) {
	return Smooth((height - config.near_height) / (config.far_height - config.near_height));
}

float LookNear(const GroundLookConfig& config, float height) {
	return 1 - Smooth((height - config.close_from) / (config.near_height - config.close_from));
}

// whether the factors are written: the first time, a step away, or arriving at an end
bool LookMoved(const SceneState& state, const GroundLookConfig& config, float far, float near) {
	if (state.lookFar < 0) return true;
	if (std::fabs(far - state.lookFar) >= config.apply_step || std::fabs(near - state.lookNear) >= config.apply_step) return true;
	return ((far == 0 || far == 1) && far != state.lookFar) || ((near == 0 || near == 1) && near != state.lookNear);
}

GroundLook ApplyLook(const GroundLookConfig& config, const GroundLook& base, float far, float near) {
	GroundLook out;
	const float splat = 1 - (1 - config.detail_floor) * far;
	for (int i = 0; i < 4; ++i) out.splat[i] = base.splat[i] * splat;   // DESIGN: maths of the ported rule (smoothstep, half angle, percent, channel counts)
	for (int i = 0; i < 3; ++i) {   // DESIGN: maths of the ported rule (smoothstep, half angle, percent, channel counts)
		out.ambient[i] = base.ambient[i] + config.fill_up * far;
		out.diffuse[i] = base.diffuse[i] * (1 - config.key_down * far) * (1 - config.near_dim * near);
	}
	return out;
}

int FarWaterMode(SceneState& state, const WaterConfig& config, int level, int currentMode) {
	const bool isFar = level >= config.far_level;
	if (isFar == state.waterFar) return NO_WATER_MODE;
	state.waterFar = isFar;
	if (isFar) {
		if (currentMode <= config.basic_mode) return NO_WATER_MODE;
		state.wantedWater = currentMode;
		return config.basic_mode;
	}
	// a mode someone else set meanwhile stays
	const int back = currentMode == config.basic_mode ? state.wantedWater : NO_WATER_MODE;
	state.wantedWater = NO_WATER_MODE;
	return back;
}

int GuardWaterMode(const WaterConfig& config, int currentMode) {
	return currentMode == config.dynamic_mode ? config.safe_mode : NO_WATER_MODE;
}

float MinimapGapSeconds(const MinimapRateConfig& config, int level) {
	return level >= config.far_from_level ? config.far_gap_s : config.near_gap_s;
}

bool MinimapLayersOff(const MinimapRateConfig& config, int level, bool wholeMap) {
	return level >= config.layers_off_level && wholeMap;
}

} // namespace KT1::Core::View
