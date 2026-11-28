/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 core: the config of the strategic and minimap icons (LUA-FREE H01, UNITDRAW §4.3). No engine include.
// Replaces the tunables of the widgets icon_lex and icon_minimap, LuaUI/lib/{icon_shape,icon_size,cam_switch}.lua,
// design/ui/icon_sizes.lua with its parity overlay gamedata/gen/overlay_icon.lua, and LuaUI/config/icon_atlas.lua.
// The converter writes the values of the data source in use (design or overlay) into the file; the defaults are the
// design file's. Sizes are icon pixels at scale one. The adapter reads the mod options icon_lex and icon_gl4.

#ifndef KT1_CORE_ICON_CONFIG_H
#define KT1_CORE_ICON_CONFIG_H

#include <string>
#include <vector>

namespace KT1::Core::IconLexicon {

// one size group: the filled shape { w, h } and the shape with its tier bars (0, 0: none measured, see IconSizeConfig)
struct IconGroupSize {
	std::string group;             // land_vehicle, structure, factory, commander, super_unit
	float body[2] = {0.0f, 0.0f};
	float extent[2] = {0.0f, 0.0f};
	template<class S> void Describe(S& s) {
		s.Field("group", group);
		s.Field("body", body);
		s.Field("extent", extent);
	}
};

struct IconSizeConfig {
	std::vector<IconGroupSize> groups = {
		{"land_vehicle", {11.0f, 11.0f}, {16.0f, 16.0f}},   // REQ UI-424 v4 R1 (design layer, owner review 2026-11-13)
		{"structure", {9.0f, 9.0f}, {0.0f, 0.0f}},          // REQ UI-424 v4 R1 (design)
		{"commander", {10.0f, 10.0f}, {0.0f, 0.0f}},        // REQ UI-424 v4 R1 (design)
		{"super_unit", {10.0f, 10.0f}, {0.0f, 0.0f}},       // REQ UI-424 v4 R1 (design)
		{"factory", {13.0f, 7.0f}, {0.0f, 0.0f}},           // REQ UI-424 v4 R1 (design)
	};
	std::string unmeasured = "land_vehicle";   // DESIGN: the group of a shape with no measured size; its bar strip serves groups without extent
	float steps[3] = {1.0f, 1.5f, 2.0f};       // REQ UI-424 v2 R4: the scale option's steps
	float selected_rim_px = 1.0f;              // REQ UI-424 v4 R2
	bool rotate = false;                       // REQ UI-424 R5: upright
	template<class S> void Describe(S& s) {
		s.List("groups", groups, [](auto& o, IconGroupSize& g) { g.Describe(o); });
		s.Field("unmeasured", unmeasured);
		s.Field("steps", steps);
		s.Field("selected_rim_px", selected_rim_px);
		s.Field("rotate", rotate);
	}
};

// colours and badges
struct IconStyleConfig {
	float relation_self[3] = {47.0f / 255.0f, 127.0f / 255.0f, 224.0f / 255.0f};    // DESIGN (ICON-02 section 8)
	float relation_ally[3] = {19.0f / 255.0f, 156.0f / 255.0f, 138.0f / 255.0f};    // DESIGN (ICON-02 section 8)
	float relation_enemy[3] = {1.0f, 154.0f / 255.0f, 118.0f / 255.0f};             // DESIGN (ICON-02 section 8)
	float contact_grey[3] = {128.0f / 255.0f, 128.0f / 255.0f, 128.0f / 255.0f};    // REQ UI-424 (owner review 2026-11-14): radar contact
	float team_contrast = 3.0f;            // DESIGN (section 8): white lines keep this contrast over the team fill
	float team_darken = 0.95f;             // DESIGN: factor per darkening step
	int team_darken_steps = 40;            // DESIGN: at most this many
	float low_health = 0.40f;              // DESIGN (ICON-02 section 6)
	float crit_health = 0.25f;             // DESIGN (ICON-02 section 6)
	float hover_lighten = 0.35f;           // DESIGN: share of white mixed into a hovered body
	float cloak_alpha = 0.35f;             // DESIGN: fill alpha of a cloaked unit
	float halo_px = 2.0f;                  // DESIGN (owner 2026-11-14): room around a selected icon
	float alt_min_px = 20.0f;              // DESIGN: with Alt every icon is at least this large
	float wall_min_px = 20.0f;             // DESIGN: walls hide below this size
	float lod_px[4] = {12.0f, 18.0f, 28.0f, 48.0f};   // DESIGN: pixel sizes from which detail levels 1..4 hold
	float bar_size[2] = {7.0f, 1.0f};      // DESIGN: one tier bar, px
	float bar_pitch = 2.0f;                // DESIGN: px between bar centres
	int bars = 3;                          // DESIGN: room for this many is always kept
	float mark_radius = 1.5f;              // DESIGN: the role mark
	template<class S> void Describe(S& s) {
		s.Field("relation_self", relation_self);
		s.Field("relation_ally", relation_ally);
		s.Field("relation_enemy", relation_enemy);
		s.Field("contact_grey", contact_grey);
		s.Field("team_contrast", team_contrast);
		s.Field("team_darken", team_darken);
		s.Field("team_darken_steps", team_darken_steps);
		s.Field("low_health", low_health);
		s.Field("crit_health", crit_health);
		s.Field("hover_lighten", hover_lighten);
		s.Field("cloak_alpha", cloak_alpha);
		s.Field("halo_px", halo_px);
		s.Field("alt_min_px", alt_min_px);
		s.Field("wall_min_px", wall_min_px);
		s.Field("lod_px", lod_px);
		s.Field("bar_size", bar_size);
		s.Field("bar_pitch", bar_pitch);
		s.Field("bars", bars);
		s.Field("mark_radius", mark_radius);
	}
};

// when the world view shows icons, and how the set is kept
struct IconViewConfig {
	float switch_distance = 1040.0f;          // REQ CAM-003 v3: camera-to-focus distance from which every unit is an icon, elmos
	std::string switch_sweep = "icons";       // REQ RND-006 v3 R6a: the level-of-detail sweep that scales it (ZoomGates::DetailFactor)
	bool mask_models = false;                 // DESIGN (Reference re-review 2026-11-14): models stay drawn under their icon
	float flicker_s = 0.15f;                  // DESIGN (GAME-DESIGN 1): a damaged unit's icon shows the flicker colour this long
	float flicker_colour[3] = {1.0f, 0.18f, 0.12f};   // DESIGN
	float contact_air_above = 40.0f;          // DESIGN: a contact this far above ground or water reads as air, elmos
	float contact_sub_below = -8.0f;          // DESIGN: a contact below this height reads as submerged
	int contact_spread_frames = 4;            // DESIGN: drawn frames over which every contact's shape is read once
	float heading_ahead = 64.0f;              // DESIGN: elmos ahead of a unit for its screen heading (rotate only)
	float hover_pad_px = 48.0f;               // DESIGN: reach of the near hover search beyond the largest box
	int hover_frames = 10;                    // DESIGN: drawn frames between hover reads under a still cursor
	int max_instances = 16384;                // DESIGN: icons drawn at most
	float click_slop_px = 4.0f;               // DESIGN: pointer movement below this is a click
	float double_click_px = 6.0f;             // DESIGN: the second click within this many pixels
	std::string engine_icon = "lex3_none";    // DESIGN: the transparent engine icon type put on every def while icons draw
	std::string engine_icon_texture = "icons/none.png";   // DESIGN
	std::string icon_texture = "LuaUI/icons/lex3_icons.png";    // DESIGN: the baked icon atlas
	std::string badge_texture = "LuaUI/icons/lex_badges.png";   // DESIGN
	template<class S> void Describe(S& s) {
		s.Field("switch_distance", switch_distance);
		s.Field("switch_sweep", switch_sweep);
		s.Field("mask_models", mask_models);
		s.Field("flicker_s", flicker_s);
		s.Field("flicker_colour", flicker_colour);
		s.Field("contact_air_above", contact_air_above);
		s.Field("contact_sub_below", contact_sub_below);
		s.Field("contact_spread_frames", contact_spread_frames);
		s.Field("heading_ahead", heading_ahead);
		s.Field("hover_pad_px", hover_pad_px);
		s.Field("hover_frames", hover_frames);
		s.Field("max_instances", max_instances);
		s.Field("click_slop_px", click_slop_px);
		s.Field("double_click_px", double_click_px);
		s.Field("engine_icon", engine_icon);
		s.Field("engine_icon_texture", engine_icon_texture);
		s.Field("icon_texture", icon_texture);
		s.Field("badge_texture", badge_texture);
	}
};

struct IconMinimapConfig {
	float size_px = 6.0f;                // REQ ICO-011: a unit's minimap icon
	float big_px = 11.0f;                // REQ ICO-011: commanders and experimentals
	int contact_spread_frames = 4;       // DESIGN: drawn frames over which every contact is read once
	int refresh_hz = 5;                  // DESIGN (owner 2026-11-21): MiniMapRefreshRate this session; 0: the engine's choice
	template<class S> void Describe(S& s) {
		s.Field("size_px", size_px);
		s.Field("big_px", big_px);
		s.Field("contact_spread_frames", contact_spread_frames);
		s.Field("refresh_hz", refresh_hz);
	}
};

// one atlas key "shape|glyph|n|dbl": per detail level 0..4 its square cell { x, y, side } in the icon atlas
struct IconAtlasEntry {
	std::string key;
	float l0[3] = {0.0f, 0.0f, 0.0f};
	float l1[3] = {0.0f, 0.0f, 0.0f};
	float l2[3] = {0.0f, 0.0f, 0.0f};
	float l3[3] = {0.0f, 0.0f, 0.0f};
	float l4[3] = {0.0f, 0.0f, 0.0f};
	template<class S> void Describe(S& s) {
		s.Field("key", key);
		s.Field("l0", l0);
		s.Field("l1", l1);
		s.Field("l2", l2);
		s.Field("l3", l3);
		s.Field("l4", l4);
	}
};

// a badge "b|name|level" or dot "d|name|1": { x, y, w, h, offset x, offset y, bake px }
struct IconBadgeEntry {
	std::string key;
	float cell[7] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
	template<class S> void Describe(S& s) {
		s.Field("key", key);
		s.Field("cell", cell);
	}
};

// the generated atlas (tools/ui/icons/build_atlas.py): no defaults, an empty atlas draws shapes only
struct IconAtlasConfig {
	float bake[5] = {10.0f, 14.0f, 20.0f, 32.0f, 64.0f};   // DESIGN (ICON-02): px of the 64-unit box at each detail level
	float icon_size[2] = {2048.0f, 1024.0f};               // DESIGN: the icon atlas, px
	float badge_size[2] = {512.0f, 128.0f};                // DESIGN: the badge atlas, px
	std::string fallback_key = "land|none|0|0";            // DESIGN: the key of a def whose own is not in the atlas
	std::vector<IconAtlasEntry> icons;
	std::vector<IconBadgeEntry> badges;
	template<class S> void Describe(S& s) {
		s.Field("bake", bake);
		s.Field("icon_size", icon_size);
		s.Field("badge_size", badge_size);
		s.Field("fallback_key", fallback_key);
		s.List("icons", icons, [](auto& o, IconAtlasEntry& e) { e.Describe(o); });
		s.List("badges", badges, [](auto& o, IconBadgeEntry& e) { e.Describe(o); });
	}
};

// data/kt1/icons.json (the atlas is its "atlas" object, generated from icon_atlas.lua)
struct IconConfig {
	IconSizeConfig sizes;
	IconStyleConfig style;
	IconViewConfig view;
	IconMinimapConfig minimap;
	IconAtlasConfig atlas;
	template<class S> void Describe(S& s) {
		s.Object("sizes", [this](auto& o) { sizes.Describe(o); });
		s.Object("style", [this](auto& o) { style.Describe(o); });
		s.Object("view", [this](auto& o) { view.Describe(o); });
		s.Object("minimap", [this](auto& o) { minimap.Describe(o); });
		s.Object("atlas", [this](auto& o) { atlas.Describe(o); });
	}
};

} // namespace KT1::Core::IconLexicon

#endif
