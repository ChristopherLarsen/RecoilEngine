/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 UnitFx: tunables and looks (game/data/kt1/unitfx.json; replaces gamedata/fx_unit_fx.lua and the numbers of
// gfx_unit_fx.lua). Looks only, none a gameplay value. The part lists are data: their defaults are empty. Lengths elmos,
// times seconds (the Lua's frame counts are seconds here). Engine-free.

#ifndef KT1_UNIT_FX_CONFIG_H
#define KT1_UNIT_FX_CONFIG_H

#include <string>
#include <vector>

#include "Game/KT1/Draw/FxCommon/FxParticles.h"

namespace KT1::UnitFx {

// one emitter of a recipe
struct EffectPart {
	std::string pool;             // DATA: smoke, spark, glow
	std::string sprite;           // DATA: the textured pool it draws in when that could be made; empty: none
	std::string role;             // DATA: palette role of its colour; empty: rgb
	float rgb[3] = {1.0f, 1.0f, 1.0f};   // DATA: used when role is empty
	float every_s = 0.0f;         // DATA: between emissions
	int count = 1;                // DATA
	float size0 = 0.0f;           // DATA
	float size1 = 0.0f;           // DATA
	float life = 0.0f;            // DATA
	float rise = 0.0f;            // DATA: elmos per s upward
	float spread = 0.0f;          // DATA: elmos per s sideways
	float drag = 0.0f;            // DATA
	float gravity = 0.0f;         // DATA
	float alpha = 1.0f;           // DATA
	float grey = 0.0f;            // DATA: random mix toward smoke grey, up to this share
	float dark = 0.0f;            // DATA: share the colour is darkened
	float ring = 0.0f;            // DATA: above 0: placed on a ring of this radius
	std::string at;               // DATA: top, base, ground; empty: the body
	bool on_body = false;         // DATA
	bool secondary = false;       // DATA: dropped at half detail
	bool smoky = false;           // DATA: only factions with trail smoke
	bool no_smoke = false;        // DATA: only factions without
	bool crown = false;           // DATA: from a plant's crown
	bool dust = false;            // DATA: coloured by the ground (dust or silt)
	template<class S> void Describe(S& s) {
		s.Field("pool", pool);
		s.Field("sprite", sprite);
		s.Field("role", role);
		s.Field("rgb", rgb);
		s.Field("every_s", every_s);
		s.Field("count", count);
		s.Field("size0", size0);
		s.Field("size1", size1);
		s.Field("life", life);
		s.Field("rise", rise);
		s.Field("spread", spread);
		s.Field("drag", drag);
		s.Field("gravity", gravity);
		s.Field("alpha", alpha);
		s.Field("grey", grey);
		s.Field("dark", dark);
		s.Field("ring", ring);
		s.Field("at", at);
		s.Field("on_body", on_body);
		s.Field("secondary", secondary);
		s.Field("smoky", smoky);
		s.Field("no_smoke", no_smoke);
		s.Field("crown", crown);
		s.Field("dust", dust);
	}
};

// a named list of parts: "damage.<faction>.1", "hit_burst.<faction>", "dot_tick", "crash_trail", "foot.dust",
// "ambient.power", "tree_fire", "tree_smoulder", "trail.piece" ...
struct RecipeRow {
	std::string name;             // DATA
	float period_s = 0.0f;        // DATA: a pulsing recipe's period; 0: none
	std::vector<EffectPart> parts;   // DATA
	template<class S> void Describe(S& s) {
		s.Field("name", name);
		s.Field("period_s", period_s);
		s.List("parts", parts, [](auto& o, EffectPart& p) { p.Describe(o); });
	}
};

struct NamedPool {
	std::string name;             // DATA: smoke, spark, glow, wreck, sprite_smoke, sprite_fire, sprite_spark, sprite_dust, sprite_flash
	KT1::FxDraw::ParticlePoolConfig pool;
	template<class S> void Describe(S& s) {
		s.Field("name", name);
		s.Object("pool", [&](auto& o) { pool.Describe(o); });
	}
};

struct CapPoint {
	float scale = 0.0f;           // DESIGN: the effect option's value
	int units = 0;                // DESIGN
	template<class S> void Describe(S& s) {
		s.Field("scale", scale);
		s.Field("units", units);
	}
};

struct BiomeColour {
	std::string biome;            // DATA
	float rgb[3] = {0.5f, 0.5f, 0.5f};   // DATA
	template<class S> void Describe(S& s) {
		s.Field("biome", biome);
		s.Field("rgb", rgb);
	}
};

struct TeleportLook {
	float charge_every_s = 0.1f;  // DESIGN
	int charge_motes = 5;         // DESIGN
	float charge_radius0 = 50.0f; // DESIGN
	float charge_radius1 = 14.0f; // DESIGN
	float charge_life = 0.45f;    // DESIGN
	float charge_size = 6.0f;     // DESIGN
	float charge_glow = 18.0f;    // DESIGN
	float charge_alpha = 0.85f;   // DESIGN
	int depart_motes = 36;        // DESIGN
	float depart_radius = 56.0f;  // DESIGN
	float depart_life = 0.4f;     // DESIGN
	float depart_size = 7.0f;     // DESIGN
	int column = 9;               // DESIGN: glows up the column of light
	float column_height = 110.0f; // DESIGN
	float column_size = 16.0f;    // DESIGN
	int arrive_ring = 40;         // DESIGN
	float arrive_life = 0.6f;     // DESIGN
	float arrive_size = 8.0f;     // DESIGN
	int arrive_sparks = 20;       // DESIGN
	float arrive_min_radius = 40.0f;   // DESIGN
	int line_dots = 18;           // DESIGN
	float line_life = 1.0f;       // DESIGN
	float line_size = 5.0f;       // DESIGN
	float line_alpha = 0.5f;      // DESIGN
	int max_charging = 8;         // DESIGN: portals charging at once
	template<class S> void Describe(S& s) {
		s.Field("charge_every_s", charge_every_s);
		s.Field("charge_motes", charge_motes);
		s.Field("charge_radius0", charge_radius0);
		s.Field("charge_radius1", charge_radius1);
		s.Field("charge_life", charge_life);
		s.Field("charge_size", charge_size);
		s.Field("charge_glow", charge_glow);
		s.Field("charge_alpha", charge_alpha);
		s.Field("depart_motes", depart_motes);
		s.Field("depart_radius", depart_radius);
		s.Field("depart_life", depart_life);
		s.Field("depart_size", depart_size);
		s.Field("column", column);
		s.Field("column_height", column_height);
		s.Field("column_size", column_size);
		s.Field("arrive_ring", arrive_ring);
		s.Field("arrive_life", arrive_life);
		s.Field("arrive_size", arrive_size);
		s.Field("arrive_sparks", arrive_sparks);
		s.Field("arrive_min_radius", arrive_min_radius);
		s.Field("line_dots", line_dots);
		s.Field("line_life", line_life);
		s.Field("line_size", line_size);
		s.Field("line_alpha", line_alpha);
		s.Field("max_charging", max_charging);
	}
};

struct MoteLook {                 // sacrifice and remnant
	int motes = 40;               // DESIGN
	float life = 1.0f;            // DESIGN
	float size = 6.0f;            // DESIGN
	float spread = 14.0f;         // DESIGN
	float rise = 0.0f;            // DESIGN
	int ring = 24;                // DESIGN
	float ring_life = 0.6f;       // DESIGN
	float ring_size = 7.0f;       // DESIGN
	float flash = 0.0f;           // DESIGN
	float alpha = 1.0f;           // DESIGN
	float rgb[3] = {1.0f, 1.0f, 1.0f};   // DESIGN
	int max = 6;                  // DESIGN: running at once
	template<class S> void Describe(S& s) {
		s.Field("motes", motes);
		s.Field("life", life);
		s.Field("size", size);
		s.Field("spread", spread);
		s.Field("rise", rise);
		s.Field("ring", ring);
		s.Field("ring_life", ring_life);
		s.Field("ring_size", ring_size);
		s.Field("flash", flash);
		s.Field("alpha", alpha);
		s.Field("rgb", rgb);
		s.Field("max", max);
	}
};

struct ChargeGlowLook {
	float size = 24.0f;           // DESIGN: at tier S and full charge
	float tier_factor[4] = {1.0f, 1.4f, 1.9f, 2.6f};   // DESIGN: tiers S, M, L, XL
	float size_at_zero = 0.15f;   // DESIGN: share of the size at no charge
	float flicker_from = 0.85f;   // DESIGN
	float flicker = 0.45f;        // DESIGN
	float stall_s = 0.5f;         // DESIGN: no new progress this long: the charge stopped
	float fade_s = 0.3f;          // DESIGN
	float collapse_size = 1.6f;   // DESIGN
	float collapse_life = 0.12f;  // DESIGN
	float alpha = 0.9f;           // DESIGN
	template<class S> void Describe(S& s) {
		s.Field("size", size);
		s.Field("tier_factor", tier_factor);
		s.Field("size_at_zero", size_at_zero);
		s.Field("flicker_from", flicker_from);
		s.Field("flicker", flicker);
		s.Field("stall_s", stall_s);
		s.Field("fade_s", fade_s);
		s.Field("collapse_size", collapse_size);
		s.Field("collapse_life", collapse_life);
		s.Field("alpha", alpha);
	}
};

struct HeavyShotLook {            // charged hits and XL beams
	float charged_flash = 40.0f;  // DESIGN
	float charged_ring = 70.0f;   // DESIGN
	int charged_ring_count = 28;  // DESIGN
	float charged_life = 0.35f;   // DESIGN
	float charged_scorch = 34.0f; // DESIGN
	float charged_scorch_life = 10.0f;   // DESIGN
	float beam_light = 26.0f;     // DESIGN
	float beam_light_life = 0.25f;       // DESIGN
	float beam_burn = 15.0f;      // DESIGN: half of the burn's width
	float beam_burn_size = 2.0f;  // DESIGN: times beam_burn
	float beam_burn_life = 4.0f;  // DESIGN
	template<class S> void Describe(S& s) {
		s.Field("charged_flash", charged_flash);
		s.Field("charged_ring", charged_ring);
		s.Field("charged_ring_count", charged_ring_count);
		s.Field("charged_life", charged_life);
		s.Field("charged_scorch", charged_scorch);
		s.Field("charged_scorch_life", charged_scorch_life);
		s.Field("beam_light", beam_light);
		s.Field("beam_light_life", beam_light_life);
		s.Field("beam_burn", beam_burn);
		s.Field("beam_burn_size", beam_burn_size);
		s.Field("beam_burn_life", beam_burn_life);
	}
};

struct TrailRules {
	float piece_every_s = 0.067f;     // DESIGN
	float wreck_every_s = 0.1f;       // DESIGN
	float wreck_min_air = 6.0f;       // DESIGN: a wreck this high leaves a trail
	float contrail_every_s = 0.1f;    // DESIGN
	float contrail_min_height = 90.0f;    // DESIGN
	float contrail_min_speed = 60.0f;     // DESIGN: elmos per s (2 a frame)
	int max_pieces = 64;              // DESIGN
	int max_wrecks = 32;              // DESIGN
	int max_air = 48;                 // DESIGN
	template<class S> void Describe(S& s) {
		s.Field("piece_every_s", piece_every_s);
		s.Field("wreck_every_s", wreck_every_s);
		s.Field("wreck_min_air", wreck_min_air);
		s.Field("contrail_every_s", contrail_every_s);
		s.Field("contrail_min_height", contrail_min_height);
		s.Field("contrail_min_speed", contrail_min_speed);
		s.Field("max_pieces", max_pieces);
		s.Field("max_wrecks", max_wrecks);
		s.Field("max_air", max_air);
	}
};

struct CraterRow {
	std::string blast_class;      // DATA: artillery, building_death, experimental_death
	float size = 0.0f;            // DATA: half extent
	float life = 0.0f;            // DATA
	float ignite_reach = 0.0f;    // DATA: plants this near catch fire; 0: none
	template<class S> void Describe(S& s) {
		s.Field("blast_class", blast_class);
		s.Field("size", size);
		s.Field("life", life);
		s.Field("ignite_reach", ignite_reach);
	}
};

struct UnitFxConfig {
	float near_dist = 1400.0f;        // DESIGN: full detail within this camera distance
	float mid_dist = 3200.0f;         // DESIGN: half within this; beyond it the icon band
	std::string gate_full = "unit_fx_full";   // DESIGN: zoom gate for full detail (data/kt1/viewsettings.json)
	std::string gate_any = "unit_fx";         // DESIGN: zoom gate for any detail
	float light_share = 0.6667f;      // DESIGN: owner 2026-10-30: smoke below this health share
	float heavy_share = 0.3333f;      // DESIGN: coordinator ruling 2026-11-10: fire below this
	float poll_s = 0.5f;              // DESIGN: health looks
	float ambient_poll_s = 1.0f;      // DESIGN
	float size_ref = 18.0f;           // DESIGN: unit radius at which recipe sizes hold
	float size_min = 0.6f;            // DESIGN
	float size_max = 3.0f;            // DESIGN
	float structure_size = 1.4f;      // DESIGN: structures emit larger
	float icon_smoke = 0.5f;          // DESIGN: puff size at icon distance
	float smoke_grey = 0.78f;         // DESIGN
	std::vector<CapPoint> damage_caps = {{0.0f, 0}, {0.5f, 0}, {1.0f, 60}, {2.0f, 200}};     // DESIGN
	std::vector<CapPoint> walker_caps = {{0.0f, 0}, {0.5f, 0}, {1.0f, 150}, {2.0f, 400}};    // DESIGN
	std::vector<CapPoint> ambient_caps = {{0.0f, 0}, {0.5f, 0}, {1.0f, 80}, {2.0f, 200}};    // DESIGN
	std::vector<CapPoint> crater_caps = {{0.0f, 0}, {0.5f, 0}, {1.0f, 40}, {2.0f, 90}};      // DESIGN
	std::vector<CapPoint> tree_caps = {{0.0f, 0}, {0.5f, 0}, {1.0f, 24}, {2.0f, 60}};        // DESIGN
	int lingering_max = 64;           // DESIGN: smoke left by units that died burning
	float lingering_s = 20.0f;        // DESIGN
	float lingering_every_s = 0.267f; // DESIGN
	int dot_area_points = 3;          // DESIGN: points of an area tick
	int dot_ticks_a_frame = 24;       // DESIGN
	float wreck_tint[3] = {0.36f, 0.33f, 0.30f};   // DESIGN: a unit wreck's burnt colour
	float char_size = 2.2f;           // DESIGN: a burnt plant's ring, times its radius
	float char_alpha = 0.85f;         // DESIGN
	float char_life = 150.0f;         // DESIGN
	float foot_size[4] = {0.6f, 1.0f, 1.7f, 2.8f};   // DESIGN: size buckets s, m, l, x
	float foot_surface = 4.0f;        // DESIGN: a foot this close under the water line splashes
	int foot_thin_legs = 6;           // DESIGN: this many legs and more: every second contact
	float build_rgb[3] = {0.45f, 1.0f, 0.70f};   // DESIGN
	float burn_s[2] = {8.0f, 15.0f};  // DESIGN: a plant burns this long
	float smoulder_s = 10.0f;         // DESIGN
	float crater_air = 24.0f;         // DESIGN: higher above the ground leaves no crater
	std::string crater_texture = "fx_crater_%s_%d";   // DESIGN: biome, variant
	TeleportLook teleport;
	MoteLook sacrifice = {56, 0.9f, 6.0f, 14.0f, 0.0f, 32, 0.6f, 7.0f, 40.0f, 1.0f, {0.45f, 1.0f, 0.70f}, 6};   // DESIGN
	MoteLook remnant = {40, 1.2f, 7.0f, 18.0f, 40.0f, 24, 0.8f, 8.0f, 0.0f, 1.0f, {0.55f, 0.85f, 1.0f}, 6};     // DESIGN
	ChargeGlowLook charge;
	HeavyShotLook heavy;
	TrailRules trails;
	std::vector<CraterRow> craters;       // DATA
	std::vector<BiomeColour> biome_dust;  // DATA
	std::vector<NamedPool> pools;         // DATA
	std::vector<RecipeRow> recipes;       // DATA
	template<class S> void Describe(S& s) {
		s.Field("near_dist", near_dist);
		s.Field("mid_dist", mid_dist);
		s.Field("gate_full", gate_full);
		s.Field("gate_any", gate_any);
		s.Field("light_share", light_share);
		s.Field("heavy_share", heavy_share);
		s.Field("poll_s", poll_s);
		s.Field("ambient_poll_s", ambient_poll_s);
		s.Field("size_ref", size_ref);
		s.Field("size_min", size_min);
		s.Field("size_max", size_max);
		s.Field("structure_size", structure_size);
		s.Field("icon_smoke", icon_smoke);
		s.Field("smoke_grey", smoke_grey);
		const auto cap = [](auto& o, CapPoint& p) { p.Describe(o); };
		s.List("damage_caps", damage_caps, cap);
		s.List("walker_caps", walker_caps, cap);
		s.List("ambient_caps", ambient_caps, cap);
		s.List("crater_caps", crater_caps, cap);
		s.List("tree_caps", tree_caps, cap);
		s.Field("lingering_max", lingering_max);
		s.Field("lingering_s", lingering_s);
		s.Field("lingering_every_s", lingering_every_s);
		s.Field("dot_area_points", dot_area_points);
		s.Field("dot_ticks_a_frame", dot_ticks_a_frame);
		s.Field("wreck_tint", wreck_tint);
		s.Field("char_size", char_size);
		s.Field("char_alpha", char_alpha);
		s.Field("char_life", char_life);
		s.Field("foot_size", foot_size);
		s.Field("foot_surface", foot_surface);
		s.Field("foot_thin_legs", foot_thin_legs);
		s.Field("build_rgb", build_rgb);
		s.Field("burn_s", burn_s);
		s.Field("smoulder_s", smoulder_s);
		s.Field("crater_air", crater_air);
		s.Field("crater_texture", crater_texture);
		s.Object("teleport", [&](auto& o) { teleport.Describe(o); });
		s.Object("sacrifice", [&](auto& o) { sacrifice.Describe(o); });
		s.Object("remnant", [&](auto& o) { remnant.Describe(o); });
		s.Object("charge", [&](auto& o) { charge.Describe(o); });
		s.Object("heavy", [&](auto& o) { heavy.Describe(o); });
		s.Object("trails", [&](auto& o) { trails.Describe(o); });
		s.List("craters", craters, [](auto& o, CraterRow& r) { r.Describe(o); });
		s.List("biome_dust", biome_dust, [](auto& o, BiomeColour& r) { r.Describe(o); });
		s.List("pools", pools, [](auto& o, NamedPool& p) { p.Describe(o); });
		s.List("recipes", recipes, [](auto& o, RecipeRow& r) { r.Describe(o); });
	}
};

} // namespace KT1::UnitFx

#endif
