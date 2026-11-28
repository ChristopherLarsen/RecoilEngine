/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 core: the zoom level and the view rows of ViewSettings (LUA-FREE H01, FRAME §4.2). No engine include.
// UNITDRAW, EFFECTS and FRAME share it. Bodies are FRAME step D07's, in Core/ViewSettings/*.cpp.

#ifndef KT1_CORE_ZOOM_LEVEL_H
#define KT1_CORE_ZOOM_LEVEL_H

#include <string>
#include <vector>

namespace KT1::Core::View {

constexpr int ZOOM_LEVELS = 8;       // L0 (closest) .. L7
constexpr int ZOOM_NONE = -1;        // no level yet

// The level bounds and hysteresis (the values and their source labels are in data/kt1/viewsettings.json).
struct ZoomBounds {
	float upper[ZOOM_LEVELS - 1] = {};   // upper[n]: the camera distance where level n ends, elmos
	float hysteresis = 0.0f;             // the fraction a distance must pass a bound by to change level back
	template<class S> void Describe(S& s) {
		s.Field("upper", upper);
		s.Field("hysteresis", hysteresis);
	}
};

// The level at this camera zoom distance; previous (ZOOM_NONE: none) holds it inside the hysteresis band; wholeMap
// puts a far camera in the last level.
int LevelFor(const ZoomBounds& bounds, float distance, int previous, bool wholeMap);

// One row: while its key holds, its target is set to value.
struct ViewRow {
	int level_from = 0;          // the first and last zoom level the row holds in
	int level_to = ZOOM_LEVELS - 1;
	std::string option;          // a detail option that scales or gates the row; empty: none
	std::string action;          // "set_engine_setting" or "set_draw_mask"
	std::string target;          // the engine setting's or the draw mask's name
	float value = 0.0f;
	float detail_factor = 0.0f;  // value is scaled by 1 + detail_factor * (the option's value - 1); 0: not scaled
	std::string gate;            // the gate id readers ask GateOpen for; empty: none

	template<class S> void Describe(S& s) {
		s.Field("level_from", level_from);
		s.Field("level_to", level_to);
		s.Field("option", option);
		s.Field("action", action);
		s.Field("target", target);
		s.Field("value", value);
		s.Field("detail_factor", detail_factor);
		s.Field("gate", gate);
	}
};

// data/kt1/viewsettings.json
struct ViewSettingsConfig {
	ZoomBounds bounds;
	std::vector<ViewRow> rows;
	template<class S> void Describe(S& s) {
		s.Object("bounds", [this](auto& b) { bounds.Describe(b); });
		s.List("rows", rows, [](auto& r, ViewRow& row) { row.Describe(r); });
	}
};

} // namespace KT1::Core::View

#endif
