/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 ShotFollower: the per-shot follower state and its steps (replaces LuaRules/Gadgets/gfx_shots.lua, fx_prim.lua,
// fx_projsnap.lua and the ribbon pools' use of them). Spec column tracks: per projectile id. The decisions are
// Core/ShotFollower (caps, setting, guard, bands, eligibility, cap order, cut-offs, cadence, puff budget, end point,
// seeded jitter). Shots come from the two steps of Snapshot<Core::Fx::ShotSample>; the shot's end from its ShotEnd event.
// Engine-free: the system passes plain values in.

#ifndef KT1_SHOT_TRACK_H
#define KT1_SHOT_TRACK_H

#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "Game/KT1/Core/Common/Column.h"
#include "Game/KT1/Core/Common/DenseIndex.h"
#include "Game/KT1/Core/FxEvents/FxEvent.h"
#include "Game/KT1/Core/FxEvents/FxSamples.h"
#include "Game/KT1/Core/ShotFollower/ShotFollowerCore.h"
#include "Game/KT1/Draw/FxCommon/FxRoster.h"
#include "Game/KT1/Draw/FxCommon/FxShapes.h"
#include "Game/KT1/Draw/ShotFollower/ShotRecipe.h"

namespace KT1::ShotFollower {

using Core::Fx::Vec3;
namespace Plan = Core::ShotFollower;

// The adapter's own tunables; the plan's are Plan::ShotFollowerConfig, described below.
struct TrackConfig {
	int scan_max = 4096;              // DESIGN: shots looked at per step
	int ribbons_max = 8192;           // DESIGN: segments alive per ribbon pool
	int bodies_max = 2048;            // DESIGN
	int smoke_puffs_max = 4096;       // DESIGN
	int glow_puffs_max = 2048;        // DESIGN
	int bubble_puffs_max = 2048;      // DESIGN
	int layer_pool_max = 1024;        // DESIGN: particles per pooled layer
	float check_every_s = 0.133f;     // DESIGN: sight and icon are asked again this often
	float under_depth = -2.0f;        // DESIGN: below this height a shot is under water
	float puff_back = 0.04f;          // DESIGN: a puff starts this long behind the shot
	float puff_drift = 0.6f;          // DESIGN
	float skip_renew_s = 10.0f;       // DESIGN
	float body_write_s = 0.067f;      // DESIGN: bodies are written this often
	float wake_every_s = 0.067f;      // DESIGN
	float wake_y = 0.8f;              // DESIGN: the wake lies this high
	int stains_max = 64;              // DESIGN
	float stain_tint[4] = {0.35f, 0.7f, 0.15f, 0.6f};   // DESIGN
	float stain_alpha = 0.9f;         // DESIGN
	float unit_hit_radius = 24.0f;    // DESIGN: an end this near a unit takes the unit impact
	float capsule_ahead = 0.35f;      // DESIGN: share of its length a capsule leads the shot
	std::string settings_shots = "KardashevFxShots";   // DESIGN: none, short, full
	std::string gate = "shot_follower";                // DESIGN: zoom gate
	template<class S> void Describe(S& s) {
		s.Field("scan_max", scan_max);
		s.Field("ribbons_max", ribbons_max);
		s.Field("bodies_max", bodies_max);
		s.Field("smoke_puffs_max", smoke_puffs_max);
		s.Field("glow_puffs_max", glow_puffs_max);
		s.Field("bubble_puffs_max", bubble_puffs_max);
		s.Field("layer_pool_max", layer_pool_max);
		s.Field("check_every_s", check_every_s);
		s.Field("under_depth", under_depth);
		s.Field("puff_back", puff_back);
		s.Field("puff_drift", puff_drift);
		s.Field("skip_renew_s", skip_renew_s);
		s.Field("body_write_s", body_write_s);
		s.Field("wake_every_s", wake_every_s);
		s.Field("wake_y", wake_y);
		s.Field("stains_max", stains_max);
		s.Field("stain_tint", stain_tint);
		s.Field("stain_alpha", stain_alpha);
		s.Field("unit_hit_radius", unit_hit_radius);
		s.Field("capsule_ahead", capsule_ahead);
		s.Field("settings_shots", settings_shots);
		s.Field("gate", gate);
	}
};

// the keys of Core/ShotFollower's config, whose header names none (data/kt1/shotfollower.json)
template<class S> void DescribePlan(S& s, Plan::ShotFollowerConfig& c) {
	s.Field("caps_count", c.capsCount);
	for (int i = 0; i < Plan::MAX_CAPS_ROWS; ++i) {
		static constexpr const char* NAMES[Plan::MAX_CAPS_ROWS] = {"caps_0", "caps_1", "caps_2", "caps_3"};
		Plan::CapsRow& row = c.caps[i];
		s.Object(NAMES[i], [&row](auto& o) {
			o.Field("scale", row.scale);
			o.Field("ribbons", row.ribbons);
			o.Field("points", row.points);
			o.Field("puffs_per_second", row.puffsPerSecond);
		});
	}
	s.Field("heavy_extras", c.heavyExtras);
	s.Field("heavy_life", c.heavyLife);
	s.Field("heavy_life_k", c.heavyLifeK);
	s.Field("default_priority", c.defaultPriority);
	s.Field("near_elmos", c.nearElmos);
	s.Field("mid_elmos", c.midElmos);
	s.Field("far_elmos", c.farElmos);
	s.Field("fade", c.fade);
	s.Field("short_life_s", c.shortLifeS);
	s.Field("guard_life_share", c.guardLifeShare);
	s.Field("under_fade_elmos", c.underFadeElmos);
	s.Field("depth_alpha_min", c.depthAlphaMin);
	s.Field("puff_window_s", c.puffWindowS);
	s.Field("path_puff_max", c.pathPuffMax);
	s.Field("path_puff_max_gap_s", c.pathPuffMaxGapS);
	s.Field("min_length_life_s", c.minLengthLifeS);
	s.Field("length_life_mul", c.lengthLifeMul);
	s.Field("min_speed_elmos_per_s", c.minSpeedElmosPerS);
	s.Field("min_segment_elmos", c.minSegmentElmos);
	s.Field("min_end_elmos", c.minEndElmos);
	s.Field("end_lift_impact", c.endLiftImpact);
	s.Field("end_lift_ribbon", c.endLiftRibbon);
	s.Field("muzzle_near_owner_elmos", c.muzzleNearOwnerElmos);
}

struct RibbonLookRow {                // a ribbon pool a recipe's parts name
	std::string name;                 // DATA
	KT1::FxDraw::RibbonPoolConfig pool;
	template<class S> void Describe(S& s) {
		s.Field("name", name);
		s.Object("pool", [&](auto& o) { pool.Describe(o); });
	}
};

struct FollowerConfig {               // game/data/kt1/shotfollower.json
	Plan::ShotFollowerConfig plan;
	TrackConfig track;
	std::vector<RibbonLookRow> ribbon_looks;   // DATA
	std::vector<ShotRecipe> recipes;  // DATA
	template<class S> void Describe(S& s) {
		s.Object("plan", [&](auto& o) { DescribePlan(o, plan); });
		s.Object("track", [&](auto& o) { track.Describe(o); });
		s.List("ribbon_looks", ribbon_looks, [](auto& o, RibbonLookRow& r) { r.Describe(o); });
		s.List("recipes", recipes, [](auto& o, ShotRecipe& r) { r.Describe(o); });
	}
};

namespace TrackFlag {
constexpr uint16_t Enemy = 1u << 0;
constexpr uint16_t New = 1u << 1;
constexpr uint16_t InLos = 1u << 2;
constexpr uint16_t OwnerIcon = 1u << 3;
constexpr uint16_t Collapsed = 1u << 4;
constexpr uint16_t Ribbon = 1u << 5;
constexpr uint16_t Started = 1u << 6;
constexpr uint16_t CoreOut = 1u << 7;
constexpr uint16_t Entered = 1u << 8;     // it has entered water
}

struct ShotTracks {                   // by projectile id
	Core::DenseIndex shots;
	Core::Column<int32_t> weaponDef, owner;
	Core::Column<Core::Fx::RosterKey> recipe;
	Core::Column<uint16_t> flags;
	Core::Column<Vec3> pos, vel;            // this step
	Core::Column<Vec3> prevPos;             // where the last ribbon point was laid
	Core::Column<float> prevTime;
	Core::Column<float> u;                  // ribbon length laid, elmos
	Core::Column<int32_t> phase;            // staggers its cadence
	Core::Column<int32_t> checkAt;          // step of the next sight and icon look
	Core::Column<float> depth, d2;
	Core::Column<uint8_t> band;             // Plan::Band
	Core::Column<float> life;               // its ribbon's
	Core::Column<int32_t> ribbonStep;       // step of the last point
	Core::Column<Plan::PathState> path;
	Core::Column<Vec3> wake;                // the wake's last point
	Core::Column<float> wakeTime, wakeU;
};

struct NamedRibbons { std::string look; KT1::FxDraw::RibbonPool core, head; };
struct LayerPool { std::string code; KT1::FxDraw::ParticlePool pool; };
struct Stain { Vec3 pos; float size = 0.0f, until = 0.0f; int32_t decal = -1; };
struct Glow { float x = 0.0f, y = 0.0f, z = 0.0f, radius = 0.0f, amount = 0.0f, r = 0.0f, g = 0.0f, b = 0.0f; };

struct FollowerStats {
	uint32_t shots = 0, ribbons = 0, capped = 0, segments = 0, puffs = 0, bodies = 0, puffsCapped = 0, enemy = 0, fogged = 0,
		collapsed = 0, enemySeen = 0, wake = 0, arcs = 0, waves = 0, implode = 0, impacts = 0, stains = 0;
};

struct FollowerState {
	ShotTracks tracks;
	std::vector<NamedRibbons> ribbons;            // one per look
	KT1::FxDraw::InstanceList<KT1::FxDraw::BodyInstance> bodies[3];   // by BodyLook
	KT1::FxDraw::ParticlePool puffs[3];           // smoke, glow, bubble
	std::vector<LayerPool> layers;
	std::vector<Stain> stains;
	std::vector<Glow> glows;                      // this step's glow sources
	Plan::PuffBudget puffBudget;
	std::vector<Plan::PlanShot> plan;             // scratch
	std::vector<int32_t> order;                   // scratch
	FollowerStats stats;
};

struct FollowIn {                     // one step
	const FollowerConfig& config;
	const KT1::FxDraw::RosterConfig& roster;
	int32_t step = 0;
	float simNow = 0.0f;
	float scale = 1.0f;
	Plan::Setting setting = Plan::DEFAULT_SETTING;
	bool limiting = false, fullView = false, overview = false;
	int32_t allyTeam = 0;
	float detailScale = 1.0f;         // the detail option's cut-off factor
	Vec3 camera, forward;
};

struct FollowWorld {                  // asked of the caller
	void* ctx = nullptr;
	float (*groundHeight)(void* ctx, float x, float z) = nullptr;
	bool (*ownerIcon)(void* ctx, int32_t unit) = nullptr;
	bool (*ownerNear)(void* ctx, int32_t unit, const Vec3& pos, float radius) = nullptr;
	bool (*unitNear)(void* ctx, const Vec3& pos, float radius) = nullptr;
};

struct ShotIn { int32_t projectile = -1; const Core::Fx::ShotSample* sample = nullptr; };

void InitFollower(FollowerState& s, const FollowerConfig& c, int32_t maxProjectiles);
// every shot of this step: adopts the new ones, plans the cap, lays ribbons, puffs, wakes, arcs and bodies
void FollowStep(FollowerState& s, const FollowIn& in, std::span<const ShotIn> shots, std::span<const Core::Fx::WeaponFx* const> weapons,
	const FollowWorld& world);
// a shot is gone: the closing stretch to its end point and, for a pooled look, its impact
void EndShot(FollowerState& s, const FollowIn& in, const Core::Fx::ShotEnd& end, const FollowWorld& world);
void ExpireStains(FollowerState& s, float now);

} // namespace KT1::ShotFollower

#endif
