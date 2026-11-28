/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 core: the zoom level, the view rows (gates) of ViewSettings and its config (LUA-FREE H01, FRAME §4.2). No engine
// include. UNITDRAW, EFFECTS and FRAME share it. Bodies are FRAME step D07's, in Core/ViewSettings/*.cpp.
// Presentation only: synced code never reads any of it (PRF-002 R3/R6).
//
// A row is a gate of one of four units:
//   "level"  holds at zoom levels from..to (whole numbers, both included; to < 0: no end)
//   "elmos"  holds while the camera distance is in [from, to) elmos (to < 0: no end)
//   "depth"  as elmos, but asked per object with its own view depth (HoldsAtDepth): effect cut-offs
//   "scale"  no gate of its own: only its option factor is read
// The level-of-detail option's factor (one per option value: low, middle, high) multiplies from and to of a distance row;
// cap (> 0) limits the scaled end.

#ifndef KT1_CORE_ZOOM_LEVEL_H
#define KT1_CORE_ZOOM_LEVEL_H

#include <cstdint>
#include <string>
#include <vector>

#include "ViewCullConfig.h"
#include "ViewSceneConfig.h"

namespace KT1::Core::View {

constexpr int ZOOM_LEVELS = 8;       // L0 (closest) .. L7
constexpr int ZOOM_NONE = -1;        // no level yet
constexpr int DETAILS = 3;           // level-of-detail option values: 0 lowest, 1 middle, 2 highest
constexpr int MAX_GATES = 64;        // one bit each in a gate mask
constexpr float NO_END = -1.0f;      // a row's `to` or `cap` that is not set

// The level bounds and hysteresis.
struct ZoomBounds {
	float upper[ZOOM_LEVELS - 1] = {450.0f, 820.0f, 930.0f, 1040.0f, 2000.0f, 4000.0f, 8000.0f};   // DESIGN (render.zoom_levels): upper[n] ends level n, elmos; upper[3] is REQ CAM-003 v4 icon_zoom
	float hysteresis = 0.05f;        // DESIGN: the fraction a distance must pass a bound by to change level by one
	int no_hysteresis_split = 4;     // REQ CAM-003 v4: the icon switch is one step; counted from 1 (4: upper[3]); 0: every split has the band
	float whole_map_from = 0.5f;     // DESIGN: the whole-map check is made only from this share of the last bound
	template<class S> void Describe(S& s) {
		s.Field("upper", upper);
		s.Field("hysteresis", hysteresis);
		s.Field("no_hysteresis_split", no_hysteresis_split);
		s.Field("whole_map_from", whole_map_from);
	}
};

int RawLevel(const ZoomBounds& bounds, float distance);   // the level with no memory; a bound belongs to the farther level
// The level at this camera zoom distance; previous (ZOOM_NONE: none) holds a one-level move inside the hysteresis band;
// wholeMap (the whole map is in view) gives the last level.
int LevelFor(const ZoomBounds& bounds, float distance, int previous, bool wholeMap);

enum class RowUnit : uint8_t { Level, Elmos, Depth, Scale };

// One row of data/kt1/viewsettings.json. While it holds, its target (if any) is set to value.
struct ViewRow {
	std::string gate;                              // the id readers ask for (ZoomGates::Gate); empty: none
	std::string unit = "level";                    // "level", "elmos", "depth" or "scale"
	float from = 0.0f;                             // first level, or distance in elmos
	float to = NO_END;                             // last level, or distance in elmos; NO_END: none
	float cap = NO_END;                            // upper limit of the scaled end
	float detail[DETAILS] = {1.0f, 1.0f, 1.0f};    // the factor at each option value: low, middle, high
	std::string sweep;                             // when given: take `detail` from this DetailConfig sweep instead
	float hysteresis = 0.0f;                       // elmos rows: band around each end, a fraction of it
	std::string option;                            // a mod option that turns the row off when "0"; empty: none
	std::string action;                            // "", "set_engine_setting" or "set_draw_mask"
	std::string target;                            // the engine setting's or the draw mask's name
	float value = 0.0f;                            // written when the row starts to hold
	bool write_other = false;                      // also write other_value when it stops holding or its option is off
	float other_value = 0.0f;

	template<class S> void Describe(S& s) {
		s.Field("gate", gate);
		s.Field("unit", unit);
		s.Field("from", from);
		s.Field("to", to);
		s.Field("cap", cap);
		s.Field("detail", detail);
		s.Field("sweep", sweep);
		s.Field("hysteresis", hysteresis);
		s.Field("option", option);
		s.Field("action", action);
		s.Field("target", target);
		s.Field("value", value);
		s.Field("write_other", write_other);
		s.Field("other_value", other_value);
	}
};

// data/kt1/viewsettings.json
struct ViewSettingsConfig {
	ZoomBounds bounds;
	std::vector<ViewRow> rows;
	DetailConfig detail;
	ModelCullConfig model_cull;
	ShadowCutConfig shadow_cut;
	DrawPassConfig draw_passes;
	PropCullConfig props;
	ZoomSwitchConfig zoom_switch;
	GroundLookConfig ground_look;
	WaterConfig water;
	MinimapRateConfig minimap;
	template<class S> void Describe(S& s) {
		s.Object("bounds", [this](auto& o) { bounds.Describe(o); });
		s.List("rows", rows, [](auto& o, ViewRow& row) { row.Describe(o); });
		s.Object("detail", [this](auto& o) { detail.Describe(o); });
		s.Object("model_cull", [this](auto& o) { model_cull.Describe(o); });
		s.Object("shadow_cut", [this](auto& o) { shadow_cut.Describe(o); });
		s.Object("draw_passes", [this](auto& o) { draw_passes.Describe(o); });
		s.Object("props", [this](auto& o) { props.Describe(o); });
		s.Object("zoom_switch", [this](auto& o) { zoom_switch.Describe(o); });
		s.Object("ground_look", [this](auto& o) { ground_look.Describe(o); });
		s.Object("water", [this](auto& o) { water.Describe(o); });
		s.Object("minimap", [this](auto& o) { minimap.Describe(o); });
	}
};

// ---- the level-of-detail option -------------------------------------------------------------------------------------

// 0 lowest, 1 middle, 2 highest for the engine setting's value
int DetailIndex(const DetailConfig& config, int settingValue);
// the ratio of this option value's sweep bracket midpoint to the middle one's; 1 for the middle option, for an open
// middle bracket at the highest option, and for an unknown kind
float SweepFactor(const DetailSweep& sweep, int detail);
float DetailFactor(const DetailConfig& config, const std::string& kind, int detail);

// ---- the compiled table ---------------------------------------------------------------------------------------------

struct ViewGate {
	RowUnit unit = RowUnit::Level;
	float from = 0.0f;
	float to = NO_END;
	float cap = NO_END;
	float detail[DETAILS] = {1.0f, 1.0f, 1.0f};
	float hysteresis = 0.0f;
};

struct ViewTable {
	ViewGate gates[MAX_GATES];
	int count = 0;                     // gate i is rows[i] of the config
};

// Makes the table from the rows (resolving `sweep`). False when there are more than MAX_GATES rows or an unknown unit.
bool CompileRows(const ViewSettingsConfig& config, ViewTable& out);

float Factor(const ViewGate& gate, int detail);                   // an option value out of range takes the nearest
float Start(const ViewGate& gate, int detail);                    // the scaled start
float End(const ViewGate& gate, int detail);                      // the scaled end with the cap; NO_END when none
bool HoldsAtLevel(const ViewGate& gate, int level);
bool HoldsAtDistance(const ViewGate& gate, float distance, int detail, bool was);   // `was`: its last answer (the band)
bool HoldsAtDepth(const ViewGate& gate, float depth, int detail);                   // no memory
// One bit per row: level and elmos rows by the camera; depth and scale rows always set. previous: the last mask; first:
// there is none yet.
uint64_t Evaluate(const ViewTable& table, int level, float distance, int detail, uint64_t previous, bool first);

} // namespace KT1::Core::View

#endif
