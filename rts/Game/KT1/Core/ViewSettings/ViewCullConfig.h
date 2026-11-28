/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 core: ViewSettings' config for the level-of-detail option, model and wreck removal, the shadow cut and the draw
// passes (LUA-FREE H01, FRAME §4.2; part of data/kt1/viewsettings.json). No engine include.
// Replaces the tunables of LuaUI/lib/{detail_option,model_cull,shadow_zoom,draw_zoom_gates}.lua, the widgets gfx_model_cull,
// gfx_wreck_cull, gfx_shadow_zoom, gfx_draw_zoom_gates and their rows of gamedata/render_values.lua.
// "mu" is one map unit of the camera zoom (elmos_per_map_unit elmos). Mod options stay options: the adapter reads
// cull_models, model_cull_early, wreck_cull, shadow_zoom_cap, draw_zoom_gates, dzg_shadow_max, dzg_refl, dzg_deferred.

#ifndef KT1_CORE_VIEW_CULL_CONFIG_H
#define KT1_CORE_VIEW_CULL_CONFIG_H

#include <string>
#include <vector>

namespace KT1::Core::View {

// one kind of the level-of-detail sweep: {last seen, first gone} brackets in mu for the option's three values.
// first gone < 0: not reached; last seen 0: already gone at the first sampled zoom.
struct DetailSweep {
	std::string kind;
	float low[2] = {0.0f, 0.0f};
	float mid[2] = {0.0f, 0.0f};
	float high[2] = {0.0f, 0.0f};
	template<class S> void Describe(S& s) {
		s.Field("kind", kind);
		s.Field("low", low);
		s.Field("mid", mid);
		s.Field("high", high);
	}
};

struct DetailConfig {
	std::string setting = "GroundDetail";        // DESIGN: the engine config key of the level-of-detail option (RND-002 v3 R8)
	int setting_default = 60;                    // PARITY:G0-DRAWZOOM the middle option value
	int lowest_below = 31;                       // PARITY:G0-DRAWZOOM option values below this are lowest (the setting's step 30 and under)
	int highest_from = 120;                      // PARITY:G0-DRAWZOOM option values from this are highest
	std::string effects_kind = "models_small";   // DESIGN (RND-006 v4 R8): "effects" has no sweep, the smallest measured kind stands in
	std::vector<DetailSweep> sweeps = {
		{"models", {340.0f, 430.0f}, {430.0f, 540.0f}, {480.0f, 540.0f}},         // PARITY:G0-DRAWZOOM lod_option_sweep large_model_removal
		{"models_small", {160.0f, 180.0f}, {200.0f, 215.0f}, {215.0f, 230.0f}},   // PARITY:G0-DRAWZOOM lod_option_sweep_2 removal_tier1_tank
		{"models_lead", {200.0f, 215.0f}, {230.0f, 245.0f}, {260.0f, 280.0f}},    // PARITY:G0-DRAWZOOM lod_option_sweep_2 removal_commander
		{"bars", {160.0f, 180.0f}, {200.0f, 215.0f}, {215.0f, 230.0f}},           // PARITY:G0-DRAWZOOM lod_option_sweep health_bar_cut
		{"icons", {0.0f, 120.0f}, {120.0f, 140.0f}, {140.0f, 160.0f}},            // PARITY:G0-DRAWZOOM lod_option_sweep_2 icon_switch
		{"plants", {540.0f, 600.0f}, {600.0f, -1.0f}, {600.0f, 800.0f}},          // PARITY:G0-DRAWZOOM lod_option_sweep plant_end
	};
	template<class S> void Describe(S& s) {
		s.Field("setting", setting);
		s.Field("setting_default", setting_default);
		s.Field("lowest_below", lowest_below);
		s.Field("highest_from", highest_from);
		s.Field("effects_kind", effects_kind);
		s.List("sweeps", sweeps, [](auto& o, DetailSweep& w) { w.Describe(o); });
	}
};

// models and shadows removed by size class (CAM-003 v4 R3); wrecks follow their unit (RND-006 v3 R6a)
struct ModelCullConfig {
	float elmos_per_map_unit = 8.0f;             // REQ CAM-003 v4 R1
	float lead_zoom = 236.5f;                    // REQ CAM-003 v4 R3 model_cull_zoom_lead, mu
	float small_range[2] = {200.0f, 240.0f};     // REQ CAM-003 v4 R3 model_cull_zoom_small_range, mu (the class takes the middle: DESIGN)
	float large_range[2] = {440.0f, 480.0f};     // REQ CAM-003 v4 R3 model_cull_zoom_large_range, mu (the class takes the middle: DESIGN)
	int large_squares = 3;                       // DESIGN: footprint squares per side from which a unit is in the large class
	bool early = true;                           // DESIGN (owner 2026-11-23): every model is removed from early_zoom at the latest
	float early_zoom = 1500.0f;                  // DESIGN (owner 2026-11-23): elmos
	std::string command_name_part = "_cmd_";     // DESIGN: a def with the custom parameter icon_cmd or this in its name is the lead class
	int experimental_tier = 4;                   // DESIGN: eco_tier from which a unit's wreck is never cut
	float step_s = 0.1f;                         // DESIGN: seconds between zoom checks
	float resync_s = 2.0f;                       // DESIGN: seconds between full unit resyncs and option re-reads
	float wreck_step_s = 0.25f;                  // DESIGN: seconds between the wreck zoom checks
	float wreck_resync_s = 1.0f;                 // DESIGN: seconds between the wreck cull's option re-reads
	template<class S> void Describe(S& s) {
		s.Field("elmos_per_map_unit", elmos_per_map_unit);
		s.Field("lead_zoom", lead_zoom);
		s.Field("small_range", small_range);
		s.Field("large_range", large_range);
		s.Field("large_squares", large_squares);
		s.Field("early", early);
		s.Field("early_zoom", early_zoom);
		s.Field("command_name_part", command_name_part);
		s.Field("experimental_tier", experimental_tier);
		s.Field("step_s", step_s);
		s.Field("resync_s", resync_s);
		s.Field("wreck_step_s", wreck_step_s);
		s.Field("wreck_resync_s", wreck_resync_s);
	}
};

// the player's shadow distance (RND-002 v4 R8) and the owner's cap on shadows and decals; only the two shadow bits of
// a draw mask change
struct ShadowCutConfig {
	std::string setting = "KardashevShadowDistance";  // DESIGN: the engine config key, mu of camera zoom
	float elmos_per_map_unit = 8.0f;                  // PARITY:G0-RND units elmos_per_map_unit
	int setting_min = 200;                            // PARITY:G0-RND shadow_distance_range
	int setting_max = 440;                            // PARITY:G0-RND shadow_distance_range
	int setting_step = 20;                            // PARITY:G0-RND shadow_distance_range
	int setting_default = 260;                        // PARITY:G0-RND shadow_distance_range
	float hysteresis = 0.02f;                         // DESIGN: band around the cut so a resting zoom does not flip it
	int batch = 2000;                                 // DESIGN (coordinator 2026-11-25): masks written per drawn frame after a crossing
	int shadow_bits = 48;                             // DESIGN: engine draw flags SO_SHOPAQ_FLAG and SO_SHTRAN_FLAG (WorldObject.h)
	float unit_cap_elmos = 820.0f;                    // DESIGN (owner 2026-11-27): no unit or feature shadows from here
	float decal_cap_elmos = 820.0f;                   // DESIGN (owner 2026-11-27): the decal pass is skipped from here
	template<class S> void Describe(S& s) {
		s.Field("setting", setting);
		s.Field("elmos_per_map_unit", elmos_per_map_unit);
		s.Field("setting_min", setting_min);
		s.Field("setting_max", setting_max);
		s.Field("setting_step", setting_step);
		s.Field("setting_default", setting_default);
		s.Field("hysteresis", hysteresis);
		s.Field("batch", batch);
		s.Field("shadow_bits", shadow_bits);
		s.Field("unit_cap_elmos", unit_cap_elmos);
		s.Field("decal_cap_elmos", decal_cap_elmos);
	}
};

// engine passes by zoom level (DRAW-ZOOM-MAP) and the unused deferred model pass
struct DrawPassConfig {
	int shadow_max_level = 1;              // DESIGN (owner 2026-11-27): SupComShadowMaxLevel, shadow pass at L0-L1 only
	int reflection_max_level = 3;          // DESIGN (owner 2026-11-27): SupComReflTexMaxLevel, no reflection cubemap redraw from L4
	int always_level = 7;                  // DESIGN: the engine's "always drawn" value, written when the gates are off
	int zoom_skip_mask = 0;                // DESIGN (D-Z3 withdrawn 2026-11-26): SupComZoomSkip, nothing skipped
	float deferred_check_s = 1.0f;         // DESIGN: seconds between checks of the deferred model pass and the effect cull scale
	template<class S> void Describe(S& s) {
		s.Field("shadow_max_level", shadow_max_level);
		s.Field("reflection_max_level", reflection_max_level);
		s.Field("always_level", always_level);
		s.Field("zoom_skip_mask", zoom_skip_mask);
		s.Field("deferred_check_s", deferred_check_s);
	}
};

} // namespace KT1::Core::View

#endif
