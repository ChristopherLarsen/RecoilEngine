/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 FxDecals: the requests the decal logic makes to the engine's ground decal system, and the scorch marks (replaces
// LuaRules/Gadgets/gfx_scorch.lua). Spec column requests: scorch, track and dressing. The logic names a mark by its own
// handle; the system keeps the engine decal of each handle. Tracks: TrackMarks.h. Dressing: Dressing.h. Engine-free.
// Folder Draw/Decals; the namespace and the system are "FxDecals" because Draw/Rnd005 already registers "Decals".

#ifndef KT1_DECAL_REQUEST_H
#define KT1_DECAL_REQUEST_H

#include <cstdint>
#include <string>
#include <vector>

#include "Game/KT1/Core/FxEvents/FxEvent.h"
#include "Game/KT1/Core/FxEvents/FxSamples.h"
#include "Game/KT1/Draw/FxCommon/FxRandom.h"

namespace KT1::FxDecals {

enum class MarkKind : uint8_t { Scorch, Death, Weapon, Print, Scuff, Wash, Dressing, Crater };
enum class DecalOp : uint8_t { Create, Alpha, Remove };

constexpr int32_t TEXTURE_NAME = 48;  // DESIGN: longest decal texture name, with its end

struct DecalRequest {
	DecalOp op = DecalOp::Create;
	MarkKind kind = MarkKind::Scorch;
	uint32_t handle = 0;              // the logic's name for the mark
	char texture[TEXTURE_NAME] = {0}; // Create
	uint8_t normalMap = 0;            // Create: also <texture>_normal
	float x = 0.0f, z = 0.0f;
	float halfX = 0.0f, halfZ = 0.0f; // the engine's decal size values
	float rotation = 0.0f;            // radians
	float alpha = 1.0f;               // Create, Alpha
	float alphaFalloff = 0.0f;        // Create: alpha lost per second by the engine; 0: none
	float tint[4] = {0.5f, 0.5f, 0.5f, 0.5f};
	uint8_t zoomFade = 0;             // fades at strategic zoom (decal user data)
	float cutoff = 0.0f;              // camera zoom distance, elmos, at which it is no longer drawn; 0: none
};

struct DecalOutput {
	std::vector<DecalRequest> requests;   // spec column requests, in the order made
	uint32_t nextHandle = 1;
};

// ---------------------------------------------------------------- scorch

struct ScorchTierRow {
	float area_squares = 0.0f;        // DESIGN: tier when area / 16 reaches this; 0: not by area
	float damage = 0.0f;              // DESIGN: or the damage reaches this
	float size_min = 0.0f;            // DESIGN: diameter at least this, elmos
	template<class S> void Describe(S& s) {
		s.Field("area_squares", area_squares);
		s.Field("damage", damage);
		s.Field("size_min", size_min);
	}
};

struct ScorchConfig {
	std::vector<ScorchTierRow> tiers = {{2.0f, 300.0f, 48.0f}, {0.0f, 60.0f, 30.0f}, {0.0f, 0.0f, 18.0f}};   // KT1 lint: allow frame-literal DESIGN: a mark size in elmos; DESIGN: largest first
	float area_unit = 16.0f;          // DESIGN: elmos of area of effect per area_squares
	float size_per_area = 1.3f;       // DESIGN
	float death_factor = 1.5f;        // DESIGN: a death blast's mark is wider
	float size_max = 150.0f;          // DESIGN
	float weapon_size = 0.7f;         // DESIGN: a weapon hit's mark: share of the tier's size
	float weapon_alpha = 0.5f;        // DESIGN
	float alpha = 0.92f;              // DESIGN
	float life_s = 140.0f;            // DESIGN
	float life_spread = 0.2f;         // DESIGN: plus or minus
	float death_life_s = 200.0f;      // DESIGN: a death mark: this
	float death_life_random_s = 150.0f;   // DESIGN: plus up to this
	float fade_s = 25.0f;             // DESIGN
	float alpha_step = 0.04f;         // DESIGN: the alpha is written when it moved this much
	int cap = 400;                    // DESIGN: marks at scale 1
	float per_step = 4.0f;            // DESIGN: new marks per step at scale 1
	float cell = 48.0f;               // DESIGN: merge grid, elmos
	float merge_share = 0.5f;         // DESIGN: a hit within this share of a mark's size renews it
	float min_ground = 3.0f;          // DESIGN: no mark lower than this above the water
	float air_share = 0.45f;          // DESIGN: higher above the ground than size x this + air_add is an air burst
	float air_add = 14.0f;            // DESIGN
	float edge = 32.0f;               // DESIGN: no mark this near the map edge
	float parity_size_min = 8.0f;     // DESIGN: a look's own mark
	float parity_alpha = 0.5f;        // DESIGN
	int scan_max = 600;               // DESIGN: marks looked at per expiry pass
	float expire_every_s = 1.0f;      // DESIGN
	std::string death_prefix = "sp_death";     // DESIGN: weapons whose blast is a death's
	std::string texture_format = "mainscar_%d";   // DESIGN: (biome - 1) x 4 + 1 .. 4
	std::string setting = "KardashevDecalScale";  // DESIGN
	std::string setting_alias = "SplinterDecalScale";   // DESIGN
	template<class S> void Describe(S& s) {
		s.List("tiers", tiers, [](auto& o, ScorchTierRow& r) { r.Describe(o); });
		s.Field("area_unit", area_unit);
		s.Field("size_per_area", size_per_area);
		s.Field("death_factor", death_factor);
		s.Field("size_max", size_max);
		s.Field("weapon_size", weapon_size);
		s.Field("weapon_alpha", weapon_alpha);
		s.Field("alpha", alpha);
		s.Field("life_s", life_s);
		s.Field("life_spread", life_spread);
		s.Field("death_life_s", death_life_s);
		s.Field("death_life_random_s", death_life_random_s);
		s.Field("fade_s", fade_s);
		s.Field("alpha_step", alpha_step);
		s.Field("cap", cap);
		s.Field("per_step", per_step);
		s.Field("cell", cell);
		s.Field("merge_share", merge_share);
		s.Field("min_ground", min_ground);
		s.Field("air_share", air_share);
		s.Field("air_add", air_add);
		s.Field("edge", edge);
		s.Field("parity_size_min", parity_size_min);
		s.Field("parity_alpha", parity_alpha);
		s.Field("scan_max", scan_max);
		s.Field("expire_every_s", expire_every_s);
		s.Field("death_prefix", death_prefix);
		s.Field("texture_format", texture_format);
		s.Field("setting", setting);
		s.Field("setting_alias", setting_alias);
	}
};

struct ScorchTier { float size = 0.0f; bool death = false; };

struct ScorchMark {
	uint32_t handle = 0;
	float x = 0.0f, z = 0.0f, size = 0.0f, alpha = 0.0f, alpha0 = 0.0f, born = 0.0f, life = 0.0f;
	int64_t cell = 0;
};
struct CellMark { int64_t cell = 0; uint32_t handle = 0; };

struct ScorchStats { uint32_t created = 0, merged = 0, dropped = 0, alive = 0; };

struct ScorchState {
	std::vector<ScorchTier> tiers;            // by weapon def id
	std::vector<uint8_t> parityWeapon;        // by weapon def id: its mark comes as a Parity scorch
	std::vector<ScorchMark> marks;            // in creation order
	std::vector<CellMark> grid;               // sorted by cell
	std::vector<std::string> textures;        // of the map's biome
	KT1::FxDraw::Random random;
	int32_t step = -1, made = 0;
	float scale = 1.0f, nextExpire = 0.0f;
	ScorchStats stats;
};

struct MarkWorld {                            // asked of the caller
	void* ctx = nullptr;
	float (*groundHeight)(void* ctx, float x, float z) = nullptr;
	float mapSizeX = 0.0f, mapSizeZ = 0.0f;
};

ScorchTier TierOf(const ScorchConfig& c, const Core::Fx::WeaponFx& weapon, bool death);
// criteriaDeaths: a unit's death mark is the criteria effects' (Draw/FxBuild asks for it), so a death tier is skipped
void OnScorch(ScorchState& s, const ScorchConfig& c, const Core::Fx::Scorch& e, bool criteriaDeaths, int32_t step, float now,
	const MarkWorld& world, DecalOutput& out);
// a death mark of a given diameter, life and alpha (GG.Scorch.Death); false: on water, or marks are off
bool DeathMark(ScorchState& s, const ScorchConfig& c, float x, float z, float size, float lifeS, float alpha, float now,
	const MarkWorld& world, DecalOutput& out);
void ExpireScorch(ScorchState& s, const ScorchConfig& c, float now, DecalOutput& out);
void ClearScorch(ScorchState& s, DecalOutput& out);

} // namespace KT1::FxDecals

#endif
