/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 core: ViewSettings' config for the map objects, water, the ground look and the minimap (LUA-FREE H01, FRAME §4.2;
// part of data/kt1/viewsettings.json). No engine include.
// Replaces the tunables of the widgets gfx_prop_distance, gfx_zoomswitch, gfx_water_far, gfx_water_guard,
// gfx_water_mode_guard and map_zoom_look, LuaUI/lib/{proplod,zoomswitch,zoomrates}.lua, gamedata/zoomswitch_values.lua and
// the prop rows of gamedata/render_values.lua. The adapter reads the mod options cull_props, prop_lod_scale, zoomswitch,
// water_far and water_guard.

#ifndef KT1_CORE_VIEW_SCENE_CONFIG_H
#define KT1_CORE_VIEW_SCENE_CONFIG_H

#include <string>
#include <vector>

namespace KT1::Core::View {

// a prop class of the feature custom parameter prop_class that has a fixed size band; band 0: a plant
struct PropClassBand {
	std::string name;
	int band = 0;
	template<class S> void Describe(S& s) {
		s.Field("name", name);
		s.Field("band", band);
	}
};

// each prop kind's own last distance (RND-003 v4 R5) and the zoom gate of map objects (RND-004 v1)
struct PropCullConfig {
	float elmos_per_map_unit = 8.0f;                   // PARITY:G0-RND units elmos_per_map_unit
	float last_plant = 640.0f;                         // PARITY:G0-RND prop_zoom three_level_switch_distances (last), mu
	float size_edges[3] = {0.6f, 1.2f, 3.0f};          // PARITY:G0-RND prop_zoom collision-box side bands, mu
	float last_by_size[3] = {70.0f, 260.0f, 640.0f};   // PARITY:G0-RND prop_zoom last cut-off by size (median of each band), mu
	float last_longest = 1890.0f;                      // PARITY:G0-RND prop_zoom last_cutoff_longest, mu
	float lod_scale = 0.5f;                            // DESIGN (owner 2026-11-20): every distance is scaled by this (option prop_lod_scale)
	float fade_share = 1.0f;                           // REQ RND-002 v4 R2: FeatureFadeDistance as a share of FeatureDrawDistance (no fade)
	float cut_floor_elmos = 700.0f;                    // DESIGN (2026-11-22): the scale never shortens a kind's own distance below this
	float fade_start_elmos = 450.0f;                   // DESIGN (2026-11-22): the zoom gate's fade starts here (not below the closest view)
	float small_fade_end = 0.5f;                       // DESIGN (render.prop_small_fade_end): share of the way to icon zoom
	float large_fade_end = 0.95f;                      // DESIGN (render.prop_large_fade_end)
	float gone_below = 0.2f;                           // DESIGN: a size class is hidden below this fade alpha
	float back_above = 0.3f;                           // DESIGN: and comes back above this one
	float cell_elmos = 512.0f;                         // DESIGN: cell side
	float step_s = 0.1f;                               // DESIGN: seconds between passes
	float move_elmos = 16.0f;                          // DESIGN: camera moves under this skip the distance pass
	int full_mask = 255;                               // DESIGN: the engine's default draw mask, restored when the saved one was unusable
	std::vector<PropClassBand> classes = {
		{"knockable_single", 0},                       // DESIGN: our plant classes (three-level kinds in the pack)
		{"knockable_cluster", 0},                      // DESIGN
		{"boulder_small", 1},                          // DESIGN: rock classes to the pack's size bands
		{"boulder_medium", 2},                         // DESIGN
		{"boulder_large", 3},                          // DESIGN
	};
	template<class S> void Describe(S& s) {
		s.Field("elmos_per_map_unit", elmos_per_map_unit);
		s.Field("last_plant", last_plant);
		s.Field("size_edges", size_edges);
		s.Field("last_by_size", last_by_size);
		s.Field("last_longest", last_longest);
		s.Field("lod_scale", lod_scale);
		s.Field("fade_share", fade_share);
		s.Field("cut_floor_elmos", cut_floor_elmos);
		s.Field("fade_start_elmos", fade_start_elmos);
		s.Field("small_fade_end", small_fade_end);
		s.Field("large_fade_end", large_fade_end);
		s.Field("gone_below", gone_below);
		s.Field("back_above", back_above);
		s.Field("cell_elmos", cell_elmos);
		s.Field("step_s", step_s);
		s.Field("move_elmos", move_elmos);
		s.Field("full_mask", full_mask);
		s.List("classes", classes, [](auto& o, PropClassBand& c) { c.Describe(o); });
	}
};

// reflections fade out by zoom, with the player's setting (RND-004 v1)
struct ZoomSwitchConfig {
	std::string setting = "KardashevZoomSwitch";     // DESIGN: the engine config key, percent
	float levels[3] = {1.0f, 0.5f, 0.25f};           // DESIGN (render.zoomswitch_levels): Normal, 50 %, 25 %
	float default_level = 0.5f;                      // DESIGN (render.zoomswitch_default)
	float reflection_off_px = 12.0f;                 // DESIGN (render.reflection_off_px): tank length on screen at which reflections are off
	float reflection_fade_span = 6.0f;               // DESIGN (render.reflection_fade_span): width of the cross-fade, px
	float tank_length = 19.3f;                       // DESIGN: the typical tank, elmos
	float close_elmos = 100.0f;                      // DESIGN (config/camera.lua MIN_DIST): the closest view the setting scales from
	float icon_elmos = 1040.0f;                      // REQ CAM-003 v4: the icon switch, where the map-object fade has ended
	float fov_default = 45.0f;                       // DESIGN: degrees, when the camera gives none
	float step_s = 0.1f;                             // DESIGN: seconds between passes
	int strength_steps = 32;                         // DESIGN: reflection strength is written in this many steps
	template<class S> void Describe(S& s) {
		s.Field("setting", setting);
		s.Field("levels", levels);
		s.Field("default_level", default_level);
		s.Field("reflection_off_px", reflection_off_px);
		s.Field("reflection_fade_span", reflection_fade_span);
		s.Field("tank_length", tank_length);
		s.Field("close_elmos", close_elmos);
		s.Field("icon_elmos", icon_elmos);
		s.Field("fov_default", fov_default);
		s.Field("step_s", step_s);
		s.Field("strength_steps", strength_steps);
	}
};

// ground detail layers and light by camera height (map_zoom_look)
struct GroundLookConfig {
	std::string setting = "TerrainZoomLook";                    // DESIGN: the engine config key; 0 turns the look off
	float near_height = 1500.0f;                                // PLACEHOLDER (CAM-001 not released): play look up to here, elmos
	float far_height = 4800.0f;                                 // PLACEHOLDER: the full strategic look from here
	float detail_floor = 1.0f;                                  // REQ RND-006 v1 R12: full detail at every zoom
	float fill_up = 0.0f;                                       // REQ RND-006 v1 R12: no light flattening
	float key_down = 0.0f;                                      // REQ RND-006 v1 R12: no light flattening
	float close_from = 450.0f;                                  // DESIGN (owner 2026-11-07): full close-zoom dimming at and below
	float near_dim = 0.35f;                                     // DESIGN (owner 2026-11-07): key light lowered by this when close
	float apply_step = 0.01f;                                   // DESIGN: written again only after a factor moved this far
	float splat_default[4] = {0.4f, 0.4f, 0.4f, 0.4f};          // PLACEHOLDER: the generator's detail strengths when the map gives none
	template<class S> void Describe(S& s) {
		s.Field("setting", setting);
		s.Field("near_height", near_height);
		s.Field("far_height", far_height);
		s.Field("detail_floor", detail_floor);
		s.Field("fill_up", fill_up);
		s.Field("key_down", key_down);
		s.Field("close_from", close_from);
		s.Field("near_dim", near_dim);
		s.Field("apply_step", apply_step);
		s.Field("splat_default", splat_default);
	}
};

struct WaterConfig {
	bool far_cut = false;                                          // REQ RND-006 R11 (owner 2026-11-27): no water change with zoom; option water_far
	int far_level = 2;                                             // DESIGN: basic water from this zoom level when far_cut is on
	int basic_mode = 0;                                            // DESIGN: the engine's basic water renderer
	int dynamic_mode = 2;                                          // DESIGN: the engine's dynamic water, which can hang it from far out
	int safe_mode = 4;                                             // DESIGN: bump-mapped water, switched to instead
	std::string safe_setting = "KardashevFxWaterLevel";            // DESIGN: a remembered mode that is also corrected
	std::string base_normal = "bitmaps/waterbump.png";             // DESIGN: base textures swapped in for a missing map texture
	std::string base_foam = "bitmaps/foam.jpg";                    // DESIGN
	std::string base_ocean = "bitmaps/ocean.jpg";                  // DESIGN
	template<class S> void Describe(S& s) {
		s.Field("far_cut", far_cut);
		s.Field("far_level", far_level);
		s.Field("basic_mode", basic_mode);
		s.Field("dynamic_mode", dynamic_mode);
		s.Field("safe_mode", safe_mode);
		s.Field("safe_setting", safe_setting);
		s.Field("base_normal", base_normal);
		s.Field("base_foam", base_foam);
		s.Field("base_ocean", base_ocean);
	}
};

// per-level minimap rates the remaining minimap widgets read (zoomrates)
struct MinimapRateConfig {
	int far_from_level = 4;                // DESIGN (D-Z3, D-Z5): the far rate from this level
	float near_gap_s = 0.13333334f;        // DESIGN: seconds between minimap model redraws below it
	float far_gap_s = 0.26666668f;         // DESIGN: and from it
	int layers_off_level = 7;              // DESIGN (D-Z5): script minimap layers off from here with the whole map in view
	template<class S> void Describe(S& s) {
		s.Field("far_from_level", far_from_level);
		s.Field("near_gap_s", near_gap_s);
		s.Field("far_gap_s", far_gap_s);
		s.Field("layers_off_level", layers_off_level);
	}
};

} // namespace KT1::Core::View

#endif
