/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 FxBuild: the criteria effects: the white flash of every weapon hit, a unit's death (chain, main flash, debris,
// arcs, smoke plumes, the dying model until the main blast, its scorch) and the camera shake they ask for (replaces the
// draw half of LuaRules/Gadgets/gfx_criteria_fx.lua, gamedata/fx_criteria.lua's looks and
// LuaUI/Widgets/gfx_criteria_shake.lua). Radii in map units (U). Fed by CritDeath; hits by the blast pass
// (FxBuildApi.h Hit). The timing is Core/FxEvents/FxCriteria.h. Engine-free.

#ifndef KT1_CRITERIA_FX_H
#define KT1_CRITERIA_FX_H

#include <cstdint>
#include <string>
#include <vector>

#include "Game/KT1/Core/FxEvents/FxCriteria.h"
#include "Game/KT1/Core/FxEvents/FxEvent.h"
#include "Game/KT1/Core/FxEvents/FxSamples.h"
#include "Game/KT1/Draw/FxCommon/FxParticles.h"
#include "Game/KT1/Draw/FxCommon/FxRandom.h"

namespace KT1::FxBuild {

enum class CriteriaPool : uint8_t { Flash, Core, Spark, Smoke, Count };

struct CriteriaConfig {               // "criteria"
	KT1::FxDraw::ParticlePoolConfig flash_pool, core_pool, spark_pool, smoke_pool;   // DATA
	int debris_per_frame = 400;       // DESIGN
	float chain_radius = 1.5f;        // DESIGN: a chain blast's flash radius, U
	float hit_colour[3] = {1.0f, 1.0f, 0.95f};    // DESIGN
	float foam_colour[3] = {0.86f, 0.90f, 0.90f}; // DESIGN
	float fire_colour[3] = {0.95f, 0.52f, 0.22f}; // DESIGN
	float rim_colour[3] = {1.0f, 0.78f, 0.45f};   // DESIGN
	float arc_colour[3] = {0.70f, 0.85f, 1.0f};   // DESIGN
	float cloud_colour[3] = {0.5f, 0.5f, 0.5f};   // DESIGN
	float smoke_colour[3] = {0.62f, 0.57f, 0.50f};    // DESIGN
	float scorch_alpha = 0.3f;        // DESIGN
	float scorch_size = 2.0f;         // DESIGN: the death mark's diameter over its radius in elmos
	float scorch_life = 200.0f;       // DESIGN: seconds, plus up to scorch_life_random
	float scorch_life_random = 150.0f;    // DESIGN
	float min_per_elmo = 0.05f;       // DESIGN
	float death_core = 0.35f;         // DESIGN
	float death_gain = 0.6f;          // DESIGN
	float plume_every_s = 0.12f;      // DESIGN
	float arc_every_s = 0.06f;        // DESIGN
	float ghost_extra_s = 0.067f;     // DESIGN: the dying model stays this long past the main blast (2 frames)
	float ghost_tint = 0.55f;         // DESIGN: toward the burnt colour
	float burnt_colour[3] = {0.36f, 0.33f, 0.30f};    // DESIGN
	float hit_radius_min = 3.0f;      // DESIGN: U
	float hit_radius_max = 5.0f;      // DESIGN
	float hit_life_min = 1.0f;        // DESIGN: seconds
	float hit_life_max = 1.5f;        // DESIGN
	float hit_scale_radius = 0.25f;   // DESIGN: missiles and shells
	float hit_scale_life = 0.3f;      // DESIGN
	float death_flash_min = 2.0f;     // DESIGN: times the radius
	float death_flash_random = 1.0f;  // DESIGN
	float death_flash_life = 1.05f;   // DESIGN
	float death_flash_life_random = 0.4f; // DESIGN
	float debris_lo = 6.0f;           // DESIGN: 1 + this x bound radius pieces at least
	float debris_hi = 15.0f;          // DESIGN
	int debris_cap = 100;             // DESIGN
	float cloud_fire_s = 1.0f;        // DESIGN
	float cloud_life = 5.0f;          // DESIGN: plus up to cloud_life_random
	float cloud_life_random = 3.0f;   // DESIGN
	float plume_scale_min = 0.5f;     // DESIGN
	float plume_scale_max = 1.25f;    // DESIGN
	float plume_emit_min = 4.0f;      // DESIGN: seconds
	float plume_emit_max = 11.5f;     // DESIGN
	float plume_per_size = 5.0f;      // DESIGN: plumes: size over this, plus 1
	int plume_parts = 6;              // DESIGN: when the model's part count is unknown
	float arc_share = 0.85f;          // DESIGN
	float air_small = 1.1f;           // DESIGN: radius bands, U
	float air_large = 7.0f;           // DESIGN
	float water_small = 2.0f;         // DESIGN
	float large = 3.75f;              // DESIGN
	std::string setting = "KardashevFxBlasts";    // DESIGN
	std::string mod_option_off = "crit_fx_off";   // DESIGN
	template<class S> void Describe(S& s) {
		s.Object("flash_pool", [&](auto& o) { flash_pool.Describe(o); });
		s.Object("core_pool", [&](auto& o) { core_pool.Describe(o); });
		s.Object("spark_pool", [&](auto& o) { spark_pool.Describe(o); });
		s.Object("smoke_pool", [&](auto& o) { smoke_pool.Describe(o); });
		s.Field("debris_per_frame", debris_per_frame);
		s.Field("chain_radius", chain_radius);
		s.Field("hit_colour", hit_colour);
		s.Field("foam_colour", foam_colour);
		s.Field("fire_colour", fire_colour);
		s.Field("rim_colour", rim_colour);
		s.Field("arc_colour", arc_colour);
		s.Field("cloud_colour", cloud_colour);
		s.Field("smoke_colour", smoke_colour);
		s.Field("scorch_alpha", scorch_alpha);
		s.Field("scorch_size", scorch_size);
		s.Field("scorch_life", scorch_life);
		s.Field("scorch_life_random", scorch_life_random);
		s.Field("min_per_elmo", min_per_elmo);
		s.Field("death_core", death_core);
		s.Field("death_gain", death_gain);
		s.Field("plume_every_s", plume_every_s);
		s.Field("arc_every_s", arc_every_s);
		s.Field("ghost_extra_s", ghost_extra_s);
		s.Field("ghost_tint", ghost_tint);
		s.Field("burnt_colour", burnt_colour);
		s.Field("hit_radius_min", hit_radius_min);
		s.Field("hit_radius_max", hit_radius_max);
		s.Field("hit_life_min", hit_life_min);
		s.Field("hit_life_max", hit_life_max);
		s.Field("hit_scale_radius", hit_scale_radius);
		s.Field("hit_scale_life", hit_scale_life);
		s.Field("death_flash_min", death_flash_min);
		s.Field("death_flash_random", death_flash_random);
		s.Field("death_flash_life", death_flash_life);
		s.Field("death_flash_life_random", death_flash_life_random);
		s.Field("debris_lo", debris_lo);
		s.Field("debris_hi", debris_hi);
		s.Field("debris_cap", debris_cap);
		s.Field("cloud_fire_s", cloud_fire_s);
		s.Field("cloud_life", cloud_life);
		s.Field("cloud_life_random", cloud_life_random);
		s.Field("plume_scale_min", plume_scale_min);
		s.Field("plume_scale_max", plume_scale_max);
		s.Field("plume_emit_min", plume_emit_min);
		s.Field("plume_emit_max", plume_emit_max);
		s.Field("plume_per_size", plume_per_size);
		s.Field("plume_parts", plume_parts);
		s.Field("arc_share", arc_share);
		s.Field("air_small", air_small);
		s.Field("air_large", air_large);
		s.Field("water_small", water_small);
		s.Field("large", large);
		s.Field("setting", setting);
		s.Field("mod_option_off", mod_option_off);
	}
};

struct ShakeConfig {                  // "shake"
	std::string setting = "KardashevFxShake";   // DESIGN
	float map_unit = 8.0f;            // DESIGN: elmos per U
	float strength_elmos = 4.0f;      // DESIGN: camera offset per U of strength
	float death_reach = 30.0f;        // KT1 lint: allow frame-literal DESIGN: a death's reach per U of radius
	float death_time = 0.5f;          // DESIGN
	float chain_reach = 30.0f;        // KT1 lint: allow frame-literal DESIGN: a chain blast's reach, U
	float chain_strength = 1.0f;      // DESIGN
	float chain_time = 0.45f;         // DESIGN
	float gauss_reach = 20.0f;        // DESIGN
	float gauss_strength = 1.0f;      // DESIGN
	float gauss_time = 1.0f;          // DESIGN
	float wobble[3] = {1.7f, 2.3f, 1.3f};   // DESIGN: the three axes' rates
	float threshold = 0.05f;          // DESIGN: weaker offsets are not set, elmos
	template<class S> void Describe(S& s) {
		s.Field("setting", setting);
		s.Field("map_unit", map_unit);
		s.Field("strength_elmos", strength_elmos);
		s.Field("death_reach", death_reach);
		s.Field("death_time", death_time);
		s.Field("chain_reach", chain_reach);
		s.Field("chain_strength", chain_strength);
		s.Field("chain_time", chain_time);
		s.Field("gauss_reach", gauss_reach);
		s.Field("gauss_strength", gauss_strength);
		s.Field("gauss_time", gauss_time);
		s.Field("wobble", wobble);
		s.Field("threshold", threshold);
	}
};

struct Shake { Core::Fx::Vec3 pos; float reach = 0.0f, strength = 0.0f, time = 0.0f, born = 0.0f; };   // reach, strength: U
struct ShakeState { std::vector<Shake> shakes; };

enum class DeathBand : uint8_t { Small, Medium, Large, AirSmall, AirMedium, AirLarge, WaterSmall, WaterMedium };

enum class Timed : uint8_t { Main, Chain, Debris, Cloud, Scorch };
struct TimedEvent { float at = 0.0f; Timed kind = Timed::Main; int32_t death = -1; Core::Fx::Vec3 pos; float radius = 0.0f; };
struct Dying {                        // the model drawn until the main blast
	int32_t unit = -1, defId = -1, team = 0, heading = 0;
	Core::Fx::Vec3 pos;
	float radius = 0.0f, from = 0.0f, until = 0.0f;
};
struct Plume { Core::Fx::Vec3 pos; float scale = 1.0f, until = 0.0f, next = 0.0f; };

struct CriteriaStats { uint32_t hits = 0, deaths = 0, chains = 0, debris = 0, plumes = 0, scorches = 0, shakes = 0; };

struct CriteriaState {
	KT1::FxDraw::ParticlePool pools[static_cast<int>(CriteriaPool::Count)];
	std::vector<TimedEvent> queue;
	std::vector<Dying> dying;
	std::vector<Plume> plumes;
	ShakeState shake;
	KT1::FxDraw::Random random;
	uint32_t frame = 0;
	int32_t frameDebris = 0;
	CriteriaStats stats;
};

struct DeathMarkAsk { float x = 0.0f, z = 0.0f, size = 0.0f, life = 0.0f, alpha = 0.0f; };

DeathBand BandOfDeath(const CriteriaConfig& c, float radius, Core::Fx::DeathMedium medium);
void DebrisRange(const CriteriaConfig& c, float bound, int32_t& least, int32_t& most);
// a weapon hit's flash; gauss weapons add a shake
void OnHit(CriteriaState& s, const CriteriaConfig& c, const ShakeConfig& shake, const Core::Fx::Vec3& pos,
	const Core::Fx::WeaponFx* weapon, bool water, float now);
void OnDeath(CriteriaState& s, const CriteriaConfig& c, const Core::Fx::CriteriaTimingConfig& timing,
	const Core::Fx::CritDeath& e, const Core::Fx::UnitDefFx& def, float now);
// due events, plumes and arcs; death marks asked of the decals (Draw/Decals ScorchDeath) are appended to `marks`
void StepCriteria(CriteriaState& s, const CriteriaConfig& c, const ShakeConfig& shake, float now, uint32_t frame,
	std::vector<DeathMarkAsk>& marks);
float ShakeAmp(const ShakeConfig& c, const Shake& shake, float distance, float age);       // elmos
// the camera offset of all live shakes; false: none above the threshold
bool CameraOffset(ShakeState& s, const ShakeConfig& c, const Core::Fx::Vec3& camera, float now, Core::Fx::Vec3& offset);

} // namespace KT1::FxBuild

#endif
