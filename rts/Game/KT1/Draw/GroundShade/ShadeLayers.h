/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 GroundShade: the layers shaded onto the terrain: wetness above the waterline and biome detail (the shader's, from
// the biome row), battle scorch (a texture painted with soft discs that fades) and the glow round the viewer's side's
// buildings (a second texture, redrawn on a period). Replaces LuaUI/Widgets/gfx_ground_shade.lua. Spec column texture =
// the two textures. Level 0 off, 1 wetness and detail, 2 all. Engine-free.

#ifndef KT1_SHADE_LAYERS_H
#define KT1_SHADE_LAYERS_H

#include <cstdint>
#include <string>
#include <vector>

namespace KT1::GroundShade {

struct BiomeShade {
	std::string biome;                // DESIGN: the map's biome; "default": any other
	float amp = 0.08f;                // DESIGN: detail strength
	float light[3] = {0.030f, 0.030f, 0.030f};   // DESIGN: detail tint
	float cell = 170.0f;              // DESIGN: detail size, elmos
	float fade = 3400.0f;             // DESIGN: detail fades out at this distance
	float wet = 1.0f;                 // DESIGN: wetness strength
	template<class S> void Describe(S& s) {
		s.Field("biome", biome);
		s.Field("amp", amp);
		s.Field("light", light);
		s.Field("cell", cell);
		s.Field("fade", fade);
		s.Field("wet", wet);
	}
};

struct ProfileStop {
	float radius = 0.0f;              // DESIGN: share of the disc's radius
	float value = 0.0f;               // DESIGN: share of its peak
	template<class S> void Describe(S& s) {
		s.Field("radius", radius);
		s.Field("value", value);
	}
};

struct ShadeConfig {                  // game/data/kt1/groundshade.json
	int level = 0;                    // DESIGN: until the cost is certified; setting and mod option override
	std::string setting = "KardashevGroundShade";   // DESIGN
	std::string mod_option = "ground_shade";        // DESIGN
	float cell = 64.0f;               // DESIGN: mesh cell, elmos
	float lift = 0.35f;               // DESIGN: elmos above the ground
	int scorch_texture = 1024;        // DESIGN: texels a side
	int glow_texture = 512;           // DESIGN
	float scorch_per_blob = 0.34f;    // DESIGN: added at a blob's centre by an average explosion
	float scorch_decay = 0.985f;      // DESIGN: the scorch is multiplied by this
	float decay_every_s = 20.0f;      // DESIGN
	float glow_every_s = 0.5f;        // DESIGN
	float glow_peak = 0.20f;          // DESIGN
	float wet_band = 28.0f;           // DESIGN: elmos above the waterline that read wet
	float wet_tint[3] = {0.004f, 0.010f, 0.016f};   // DESIGN
	float water_level = 0.0f;         // DESIGN
	float scorch_amp = 2.0f;          // DESIGN
	float scorch_amp_debug = 4.0f;    // DESIGN
	float min_area = 16.0f;           // DESIGN: smaller explosions leave no scorch, elmos
	int blobs_max = 200;              // DESIGN: blobs queued
	float blast_radius_share = 0.9f;  // DESIGN: blob radius over the area of effect
	float blast_radius_max = 220.0f;  // DESIGN
	float blast_area_ref = 120.0f;    // DESIGN: strength grows to full at this area
	float blast_strength_base = 0.3f; // DESIGN
	float death_building_radius = 1.4f;   // DESIGN: a building's radius times this
	float death_radius_share = 1.6f;  // DESIGN
	float death_radius_add = 12.0f;   // DESIGN
	float death_radius_max = 200.0f;  // DESIGN
	float death_building_strength = 1.1f; // DESIGN
	float death_unit_strength = 0.6f; // DESIGN
	float glow_footprint = 4.0f;      // DESIGN: elmos per footprint square, half width
	float glow_add = 70.0f;           // DESIGN: elmos beyond the footprint
	int disc_segments = 16;           // DESIGN
	float guard_poll_s = 0.5f;        // DESIGN: the frame rate guard is asked this often (it holds level 1)
	int max_zoom_level = 3;           // DESIGN: not drawn further out
	std::vector<ProfileStop> scorch_profile = {{0.0f, 1.0f}, {0.35f, 0.8f}, {0.7f, 0.3f}, {1.0f, 0.0f}};   // DESIGN
	std::vector<ProfileStop> glow_profile = {{0.0f, 0.35f}, {0.5f, 0.75f}, {0.72f, 1.0f}, {1.0f, 0.0f}};   // DESIGN
	std::vector<BiomeShade> biomes = {   // DESIGN: first pass, tuned by the look run
		{"lunar", 0.22f, {0.030f, 0.030f, 0.034f}, 190.0f, 4200.0f, 0.0f},
		{"tundra", 0.10f, {0.030f, 0.034f, 0.040f}, 160.0f, 3600.0f, 1.0f},
		{"desert", 0.12f, {0.040f, 0.034f, 0.022f}, 210.0f, 3600.0f, 0.6f},
		{"meadow", 0.10f, {0.022f, 0.034f, 0.018f}, 150.0f, 3400.0f, 1.0f},
		{"badlands", 0.12f, {0.040f, 0.026f, 0.018f}, 190.0f, 3600.0f, 0.6f},
		{"default", 0.08f, {0.030f, 0.030f, 0.030f}, 170.0f, 3400.0f, 1.0f},
	};
	std::string vertex_shader = "shaders/ground_shade.vert";     // DESIGN
	std::string fragment_shader = "shaders/ground_shade.frag";   // DESIGN
	template<class S> void Describe(S& s) {
		s.Field("level", level);
		s.Field("setting", setting);
		s.Field("mod_option", mod_option);
		s.Field("cell", cell);
		s.Field("lift", lift);
		s.Field("scorch_texture", scorch_texture);
		s.Field("glow_texture", glow_texture);
		s.Field("scorch_per_blob", scorch_per_blob);
		s.Field("scorch_decay", scorch_decay);
		s.Field("decay_every_s", decay_every_s);
		s.Field("glow_every_s", glow_every_s);
		s.Field("glow_peak", glow_peak);
		s.Field("wet_band", wet_band);
		s.Field("wet_tint", wet_tint);
		s.Field("water_level", water_level);
		s.Field("scorch_amp", scorch_amp);
		s.Field("scorch_amp_debug", scorch_amp_debug);
		s.Field("min_area", min_area);
		s.Field("blobs_max", blobs_max);
		s.Field("blast_radius_share", blast_radius_share);
		s.Field("blast_radius_max", blast_radius_max);
		s.Field("blast_area_ref", blast_area_ref);
		s.Field("blast_strength_base", blast_strength_base);
		s.Field("death_building_radius", death_building_radius);
		s.Field("death_radius_share", death_radius_share);
		s.Field("death_radius_add", death_radius_add);
		s.Field("death_radius_max", death_radius_max);
		s.Field("death_building_strength", death_building_strength);
		s.Field("death_unit_strength", death_unit_strength);
		s.Field("glow_footprint", glow_footprint);
		s.Field("glow_add", glow_add);
		s.Field("disc_segments", disc_segments);
		s.Field("guard_poll_s", guard_poll_s);
		s.Field("max_zoom_level", max_zoom_level);
		const auto stop = [](auto& o, ProfileStop& p) { p.Describe(o); };
		s.List("scorch_profile", scorch_profile, stop);
		s.List("glow_profile", glow_profile, stop);
		s.List("biomes", biomes, [](auto& o, BiomeShade& b) { b.Describe(o); });
		s.Field("vertex_shader", vertex_shader);
		s.Field("fragment_shader", fragment_shader);
	}
};

struct Blob { float x = 0.0f, z = 0.0f, radius = 0.0f, strength = 0.0f; };
struct GlowDisc { float x = 0.0f, z = 0.0f, radius = 0.0f, rgb[3] = {0.0f, 0.0f, 0.0f}; };

// one vertex of a disc's triangle strip, in map elmos, premultiplied colour
struct DiscVertex { float x = 0.0f, z = 0.0f; float rgba[4] = {0.0f, 0.0f, 0.0f, 0.0f}; };
static_assert(sizeof(DiscVertex) == 24);

// the shader's per frame values
struct ShadeUniforms {
	float eye[3] = {0.0f, 0.0f, 0.0f};
	float level = 0.0f, waterLevel = 0.0f, wetBand = 0.0f;
	float wetTint[3] = {0.0f, 0.0f, 0.0f};
	float detailAmp = 0.0f;
	float detailLight[3] = {0.0f, 0.0f, 0.0f};
	float detailScale = 0.0f, detailFade = 0.0f, scorchAmp = 0.0f;
};

struct ShadeState {
	int32_t level = 0;                // asked for
	bool limited = false;             // the guard holds level 1
	std::vector<Blob> blobs;          // queued scorch
	std::vector<GlowDisc> glows;      // of the last glow redraw
	std::vector<DiscVertex> vertices; // scratch
	float lastDecay = 0.0f, lastGlow = 0.0f;
	bool glowDirty = true;
	float costMs = 0.0f;
	uint32_t costFrames = 0;
};

int32_t EffectiveLevel(const ShadeState& s);
const BiomeShade& BiomeOf(const ShadeConfig& c, const std::string& biome);
// false: too small, or the queue is full
bool QueueBlast(ShadeState& s, const ShadeConfig& c, float x, float z, float areaOfEffect);
bool QueueDeath(ShadeState& s, const ShadeConfig& c, float x, float z, float unitRadius, bool building);
bool AddBlob(ShadeState& s, const ShadeConfig& c, const Blob& blob);
GlowDisc BuildingGlow(const ShadeConfig& c, float x, float z, int32_t xsize, int32_t zsize, const float (&teamRgb)[3]);
bool DecayDue(const ShadeState& s, const ShadeConfig& c, float now);
bool GlowDue(const ShadeState& s, const ShadeConfig& c, float now);
void DiscVertices(const ShadeConfig& c, float x, float z, float radius, float peak, const std::vector<ProfileStop>& profile,
	const float (&rgb)[3], std::vector<DiscVertex>& out);
ShadeUniforms UniformsOf(const ShadeConfig& c, const BiomeShade& biome, int32_t level, bool debug, const float (&eye)[3]);

} // namespace KT1::GroundShade

#endif
