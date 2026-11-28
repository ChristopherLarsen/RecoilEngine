/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 ShotFollower: the rules of the far view's projectile dots (replaces LuaUI/fx/prjdot.lua and the values of
// gamedata/fx_prjdot.lua; WVFX-PRJDOT-001). The dot layer that draws them (gfx_prjdot, step D18) calls these. Defaults are
// the design values; the data file replaces them with the roster's. Engine-free.

#ifndef KT1_PRJ_DOT_H
#define KT1_PRJ_DOT_H

#include <string>
#include <string_view>
#include <vector>

namespace KT1::ShotFollower {

struct PrjDotWeapon {
	std::string weapon;           // DATA: weapon def name
	float size = 0.0f;            // DATA: its icon size field; 0: size_field
	std::string icon;             // DATA: its named icon; empty: the plain dot
	template<class S> void Describe(S& s) {
		s.Field("weapon", weapon);
		s.Field("size", size);
		s.Field("icon", icon);
	}
};

struct PrjDotIcon {
	std::string name;             // DATA
	int cell = 1;                 // DATA: the atlas cell
	template<class S> void Describe(S& s) {
		s.Field("name", name);
		s.Field("cell", cell);
	}
};

struct PrjDotConfig {             // game/data/kt1/prjdot.json
	float gate = 140.0f;          // DESIGN: shown from this camera zoom on, map units
	float map_unit = 8.0f;        // REQ OUT-0001: elmos per map unit
	float colour[4] = {1.0f, 0.85f, 0.2f, 1.0f};   // DESIGN
	bool force_yellow = true;     // DESIGN: else the owner's army colour
	float icon_scale = 1.0f;      // DESIGN
	float size_field = 2.0f;      // DESIGN: a weapon's own wins
	float icon_px = 16.0f;        // DESIGN: a named icon's side at scale 1
	bool icons_on = true;         // DESIGN
	float icon_tint[4] = {1.0f, 1.0f, 1.0f, 1.0f};   // DESIGN
	bool glow_on = true;          // DESIGN
	float glow_min = 0.02f;       // DESIGN
	float glow_max = 0.2f;        // DESIGN
	float glow_period_s = 2.5f;   // DESIGN
	std::string atlas = "bitmaps/fx_prjdot/prj_atlas.png";   // DESIGN
	std::vector<PrjDotIcon> icons;        // DATA
	std::vector<PrjDotWeapon> weapons;    // DATA
	template<class S> void Describe(S& s) {
		s.Field("gate", gate);
		s.Field("map_unit", map_unit);
		s.Field("colour", colour);
		s.Field("force_yellow", force_yellow);
		s.Field("icon_scale", icon_scale);
		s.Field("size_field", size_field);
		s.Field("icon_px", icon_px);
		s.Field("icons_on", icons_on);
		s.Field("icon_tint", icon_tint);
		s.Field("glow_on", glow_on);
		s.Field("glow_min", glow_min);
		s.Field("glow_max", glow_max);
		s.Field("glow_period_s", glow_period_s);
		s.Field("atlas", atlas);
		s.List("icons", icons, [](auto& o, PrjDotIcon& i) { i.Describe(o); });
		s.List("weapons", weapons, [](auto& o, PrjDotWeapon& w) { w.Describe(o); });
	}
};

bool PrjDotOn(const PrjDotConfig& c, float zoomUnits);
// camera-to-focus distance in map units
float PrjDotZoom(const PrjDotConfig& c, const float (&camera)[3], const float (&focus)[3]);
void PrjDotColour(const PrjDotConfig& c, const float* teamRgb, float (&out)[4]);   // teamRgb nullptr: the default
float PrjDotPixels(const PrjDotConfig& c, float scale, std::string_view weapon);
float PrjIconPixels(const PrjDotConfig& c, float scale);
float PrjDotGlow(const PrjDotConfig& c, float seconds);
const PrjDotIcon* PrjDotIconOf(const PrjDotConfig& c, std::string_view weapon);    // nullptr: the plain dot

} // namespace KT1::ShotFollower

#endif
