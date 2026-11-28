/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 FxDecals: the decals placed on the terrain at game start and their cut-off distances (replaces
// LuaRules/Gadgets/gfx_terrain_decals.lua and gamedata/decal_cutoff.lua; TER-008). The same map gives the same decals
// (a Wichmann-Hill generator seeded by the map). Engine-free.

#ifndef KT1_DECAL_DRESSING_H
#define KT1_DECAL_DRESSING_H

#include <cstdint>
#include <string>
#include <vector>

#include "Game/KT1/Draw/Decals/DecalRequest.h"

namespace KT1::FxDecals {

enum class DressingType : uint8_t { Dirt, Gravel, Erosion, Shore, Outcrop, Tracks, Count };

struct DressingTypeRow {
	std::string name;                 // DESIGN
	float weight = 0.0f;              // DESIGN
	float min_height = 3.0f;          // DESIGN: ground at least this high
	float max_height = 0.0f;          // DESIGN: and under this; 0: no upper end
	float slope_min = 0.0f;           // DESIGN: 1 minus the normal's upward part above this
	float slope_max = 1.0f;           // DESIGN: and under this
	float width[2] = {0.0f, 0.0f};    // DESIGN: least, most, elmos
	float length[2] = {0.0f, 0.0f};   // DESIGN: least, most; both 0: the width times 0.8 to 1.2
	template<class S> void Describe(S& s) {
		s.Field("name", name);
		s.Field("weight", weight);
		s.Field("min_height", min_height);
		s.Field("max_height", max_height);
		s.Field("slope_min", slope_min);
		s.Field("slope_max", slope_max);
		s.Field("width", width);
		s.Field("length", length);
	}
};

struct RateRow {
	int size_units = 0;               // DATA: map side, map units (gamedata/render_values.lua)
	float per_million = 0.0f;         // DATA: decals per million square map units
	template<class S> void Describe(S& s) {
		s.Field("size_units", size_units);
		s.Field("per_million", per_million);
	}
};

struct QuantileRow {
	float quantile = 0.0f;            // PARITY:G0-TERBUILD decal_cutoff (the ends DESIGN)
	float cutoff = 0.0f;              // PARITY:G0-TERBUILD camera zoom distance, map units (the ends DESIGN)
	template<class S> void Describe(S& s) {
		s.Field("quantile", quantile);
		s.Field("cutoff", cutoff);
	}
};

struct DressingConfig {
	std::vector<RateRow> rates;       // DATA: empty: rate_default
	float rate_default = 2197.0f;     // DESIGN: pack design_rate, coordinator 2026-11-19
	float map_unit = 8.0f;            // DESIGN: elmos per map unit
	int tries = 40;                   // DESIGN: spots tried per decal and kind
	int rounds = 3;                   // DESIGN: a kind without a spot is replaced by a new pick
	float margin = 96.0f;             // DESIGN: elmos from the map edge
	float alpha = 0.9f;               // DESIGN: mod option terlook_decalalpha and mapinfo decalAlpha override
	float scale_max = 1.5f;           // DESIGN: cap of the decal option
	bool cutoff_on = true;            // DESIGN
	float zoom_every_s = 0.2f;        // DESIGN: between zoom looks
	float water_probe = -1.0f;        // DESIGN: ground under this is water
	int water_grid = 32;              // DESIGN: probes a side that tell whether a map has water
	float shore_reach = 110.0f;       // DESIGN: water this near makes a shore spot
	float shore_step = 20.0f;         // DESIGN: the decal is walked back from the water in these steps
	float shore_back = 160.0f;        // DESIGN: at most this far
	float shore_dry = 0.5f;           // DESIGN: all round it at least this high
	float erosion_turn = 0.25f;       // DESIGN: radians of random turn on a runnel
	int first_texture = 20;           // DESIGN: mainscar index before the first dressing texture
	int per_biome = 12;               // DESIGN: textures per biome, 2 per type
	int lunar_biome = 6;              // DESIGN: its place after the five scorch biomes
	std::string texture_format = "mainscar_%d";   // DESIGN
	std::vector<DressingTypeRow> types = {   // DESIGN
		{"dirt", 24.0f, 3.0f, 0.0f, 0.0f, 0.10f, {80.0f, 180.0f}, {80.0f, 180.0f}},
		{"gravel", 17.0f, 3.0f, 0.0f, 0.0f, 0.18f, {60.0f, 140.0f}, {0.0f, 0.0f}},
		{"erosion", 14.0f, 3.0f, 0.0f, 0.03f, 0.40f, {90.0f, 160.0f}, {110.0f, 200.0f}},
		{"shore", 13.0f, 0.0f, 12.0f, 0.0f, 0.35f, {90.0f, 150.0f}, {80.0f, 130.0f}},
		{"outcrop", 16.0f, 3.0f, 0.0f, 0.04f, 1.0f, {50.0f, 110.0f}, {0.0f, 0.0f}},
		{"tracks", 16.0f, 3.0f, 0.0f, 0.0f, 0.15f, {40.0f, 60.0f}, {110.0f, 160.0f}},
	};
	std::vector<QuantileRow> cutoffs = {{0.0f, 50.0f}, {0.1f, 100.0f}, {0.5f, 250.0f}, {0.9f, 1000.0f}, {1.0f, 2000.0f}};   // PARITY:G0-TERBUILD
	template<class S> void Describe(S& s) {
		s.List("rates", rates, [](auto& o, RateRow& r) { r.Describe(o); });
		s.Field("rate_default", rate_default);
		s.Field("map_unit", map_unit);
		s.Field("tries", tries);
		s.Field("rounds", rounds);
		s.Field("margin", margin);
		s.Field("alpha", alpha);
		s.Field("scale_max", scale_max);
		s.Field("cutoff_on", cutoff_on);
		s.Field("zoom_every_s", zoom_every_s);
		s.Field("water_probe", water_probe);
		s.Field("water_grid", water_grid);
		s.Field("shore_reach", shore_reach);
		s.Field("shore_step", shore_step);
		s.Field("shore_back", shore_back);
		s.Field("shore_dry", shore_dry);
		s.Field("erosion_turn", erosion_turn);
		s.Field("first_texture", first_texture);
		s.Field("per_biome", per_biome);
		s.Field("lunar_biome", lunar_biome);
		s.Field("texture_format", texture_format);
		s.List("types", types, [](auto& o, DressingTypeRow& r) { r.Describe(o); });
		s.List("cutoffs", cutoffs, [](auto& o, QuantileRow& r) { r.Describe(o); });
	}
};

struct WichmannHill {
	int32_t s1 = 1, s2 = 1, s3 = 1;
};

struct CutDecal { float cutoff = 0.0f; uint32_t handle = 0; };

struct DressingState {
	std::vector<CutDecal> cuts;       // sorted by cut-off after placement
	int32_t hidden = 0;               // cuts[0 .. hidden) are not drawn
	uint32_t created = 0, dropped = 0, wanted = 0;
	uint32_t byType[static_cast<int>(DressingType::Count)] = {};
	bool done = false;
};

struct DressingWorld {                // asked of the caller
	void* ctx = nullptr;
	float (*groundHeight)(void* ctx, float x, float z) = nullptr;
	void (*groundNormal)(void* ctx, float x, float z, float (&normal)[3]) = nullptr;
	float mapSizeX = 0.0f, mapSizeZ = 0.0f;
};

struct DressingIn {
	int32_t mapNameLength = 0;
	int32_t biome = 0;                // 1 meadow .. 5 ember, 6 lunar; 0: no dressing textures
	float scale = 1.0f;               // the decal option
	float alpha = 0.9f;
};

void SeedDressing(WichmannHill& rng, const DressingIn& in, const DressingWorld& world);
float NextUnit(WichmannHill& rng);                              // [0, 1)
float CutoffElmos(const DressingConfig& c, float quantile);     // geometric interpolation between the rows
float CutoffQuantile(float x, float z);                         // a hash of the position's cell
int32_t DressingCount(const DressingConfig& c, const DressingWorld& world, float scale);
void PlaceDressing(DressingState& s, const DressingConfig& c, const DressingIn& in, const DressingWorld& world,
	DecalOutput& out);
// hides the decals whose cut-off the camera zoom has reached and shows the others again
void ApplyZoom(DressingState& s, float zoomElmos, float alpha, DecalOutput& out);

} // namespace KT1::FxDecals

#endif
