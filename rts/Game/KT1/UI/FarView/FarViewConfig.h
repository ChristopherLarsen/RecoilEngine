/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 FarView: its config.
// Engine-free. Values: game-build game/data/kt1/ (labels there); the members hold the defaults.

#ifndef KT1_FARVIEW_CONFIG_H
#define KT1_FARVIEW_CONFIG_H

#include <string>

#include "Game/KT1/Data/Schema.h"

namespace KT1 {

struct FarViewConfig {
	float switchHeight = 4000.0f;   // camera height (or zoom) where the far view begins
	float hysteresis = 0.05f;       // band below the switch before the full view returns
	int grid = 128;                 // mesh cells per map side
	float refreshS = 0.5f;          // seconds between mesh height refreshes while far
	float waterLevel = 0.0f;        // the engine's water plane
	float waterColor[4] = {0.10f, 0.22f, 0.32f, 0.0f};   // tint below water; a = 0 keeps the image's water
	float brightness = 1.0f;        // multiplier on the baked image
	std::string texture;            // "" = the map's minimap image; else a game-archive image
	bool carto = true;              // the far view takes the cartographic look; false = the baked image
	bool autoSwitch = true;         // switch by zoom
	float rampDark[3] = {0.45f, 0.38f, 0.41f};
	float rampLight[3] = {0.80f, 0.80f, 0.68f};
	float contourStep = 0.10f;      // contour spacing as a share of the height range
	float edgeGain = 16.0f;         // sharpness of a contour line
	bool drawFx = true;             // projectiles, particles and ground flashes draw in the far view
	float fxCutoff = -1.0f;         // a value > 0 culls effects past this view depth
	float weaponCutoff = -1.0f;     // the same for weapon projectiles

	void Describe(Data::Schema& s) {
		s.Field("switch_height", switchHeight);
		s.Field("hysteresis", hysteresis, Data::Between(0, 0.5f));
		s.Field("grid", grid, Data::Between(8, 512));
		s.Field("refresh_s", refreshS, Data::AtLeast(0));
		s.Field("water_level", waterLevel);
		s.Field("water_color", waterColor);
		s.Field("brightness", brightness);
		s.Field("texture", texture);
		s.Field("cartographic", carto);
		s.Field("auto_switch", autoSwitch);
		s.Field("ramp_dark", rampDark);
		s.Field("ramp_light", rampLight);
		// the double nearest 0.001, not the engine's float literal: a file may say exactly 0.001
		s.Field("contour_step", contourStep, Data::AtLeast(static_cast<double>(1) / 1000));
		s.Field("contour_edge_gain", edgeGain);
		s.Field("draw_fx", drawFx);
		s.Field("fx_cutoff", fxCutoff);
		s.Field("weapon_cutoff", weaponCutoff);
	}
};

} // namespace KT1

#endif
