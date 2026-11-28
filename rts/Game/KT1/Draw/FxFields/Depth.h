/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 FxFields: depth charge and torpedo effects (replaces LuaRules/depth_fx.lua and the draw half of
// LuaRules/Gadgets/fx_depth.lua), fed by DepthHit, DepthIntercept and DepthLaunch; a launched charge is followed in
// Snapshot<Core::Fx::ShotSample> until it enters the water. Engine-free.

#ifndef KT1_FIELD_DEPTH_H
#define KT1_FIELD_DEPTH_H

#include <cstdint>
#include <vector>

#include "Game/KT1/Core/FxEvents/FxEvent.h"
#include "Game/KT1/Draw/FxCommon/FxParticles.h"
#include "Game/KT1/Draw/FxCommon/FxRandom.h"

namespace KT1::FxFields {

enum class DepthMedium : uint8_t { Under, Surface, Air, Ground };
enum class DepthPool : uint8_t { Glow, Bubble, Spark, Smoke, Count };

// one particle of a plan, with its start delayed
struct DelayedParticle {
	DepthPool pool = DepthPool::Glow;
	float delay = 0.0f;
	KT1::FxDraw::ParticleAdd add;       // birth: filled when it is written
};

struct Puff {                           // a kind of particle of the depth looks
	int count = 1;                      // DESIGN
	float size = 0.0f;                  // DESIGN
	float size1 = 0.0f;                 // DESIGN
	float life = 0.0f;                  // DESIGN
	float rise = 0.0f;                  // DESIGN
	float spread = 0.0f;                // DESIGN
	float speed = 0.0f;                 // DESIGN
	float drag = 0.0f;                  // DESIGN
	float radius = 0.0f;                // DESIGN
	float colour[4] = {1.0f, 1.0f, 1.0f, 1.0f};   // DESIGN
	template<class S> void Describe(S& s) {
		s.Field("count", count);
		s.Field("size", size);
		s.Field("size1", size1);
		s.Field("life", life);
		s.Field("rise", rise);
		s.Field("spread", spread);
		s.Field("speed", speed);
		s.Field("drag", drag);
		s.Field("radius", radius);
		s.Field("colour", colour);
	}
};

struct DepthConfig {                    // "depth"
	int glow_max = 512;                 // DESIGN: particles alive
	int bubble_max = 4096;              // DESIGN
	int spark_max = 1024;               // DESIGN
	int smoke_max = 2048;               // DESIGN
	int per_frame = 400;                // DESIGN
	float view_pad = 120.0f;            // DESIGN
	float surface = 3.0f;               // DESIGN: elmos round the sea surface that count as at it
	float air_above = 24.0f;            // DESIGN
	float tier_factor[4] = {1.0f, 1.4f, 1.9f, 2.6f};   // DESIGN: tiers S, M, L, XL
	Puff launch_smoke = {2, 2.5f, 7.0f, 1.1f, 6.0f, 0.0f, 0.0f, 0.0f, 0.0f, {0.78f, 0.78f, 0.76f, 0.45f}};   // DESIGN
	Puff hit_flash = {1, 10.0f, 22.0f, 0.22f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, {0.75f, 0.88f, 1.0f, 0.9f}};   // DESIGN
	Puff hit_bubbles = {22, 1.6f, 3.4f, 1.6f, 42.0f, 9.0f, 0.0f, 0.0f, 0.0f, {0.82f, 0.93f, 1.0f, 0.75f}};   // DESIGN
	Puff hit_ring = {16, 1.4f, 2.2f, 0.55f, 0.0f, 0.0f, 70.0f, 4.5f, 0.0f, {0.9f, 0.97f, 1.0f, 0.85f}};   // DESIGN
	Puff dome_centre = {1, 12.0f, 26.0f, 1.5f, 12.0f, 0.0f, 0.0f, 0.0f, 0.0f, {0.97f, 0.99f, 1.0f, 0.95f}};   // DESIGN
	Puff dome_ring = {10, 8.0f, 16.0f, 1.5f, 12.0f, 24.0f, 0.0f, 1.5f, 8.0f, {0.97f, 0.99f, 1.0f, 0.95f}};   // DESIGN
	float dome_delay = 0.3f;            // DESIGN
	float dome_max_depth = 160.0f;      // DESIGN
	Puff surface_patch = {6, 5.0f, 11.0f, 1.0f, 0.0f, 14.0f, 0.0f, 1.5f, 6.0f, {0.94f, 0.97f, 1.0f, 0.7f}};   // DESIGN
	float intercept_travel = 0.12f;     // DESIGN
	Puff intercept_shot = {3, 2.2f, 1.2f, 0.12f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, {1.0f, 1.0f, 1.0f, 1.0f}};   // DESIGN
	Puff intercept_burst = {1, 7.0f, 15.0f, 0.28f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, {0.7f, 0.86f, 1.0f, 1.0f}};   // DESIGN
	Puff intercept_sparks = {8, 1.6f, 0.6f, 0.22f, 0.0f, 0.0f, 55.0f, 3.0f, 0.0f, {0.8f, 0.92f, 1.0f, 1.0f}};   // DESIGN
	Puff intercept_bubbles = {8, 1.2f, 2.4f, 0.9f, 30.0f, 0.0f, 0.0f, 0.0f, 0.0f, {0.82f, 0.93f, 1.0f, 0.7f}};   // KT1 lint: allow frame-literal a rise in elmos per s; DESIGN
	Puff entry_ring = {10, 2.2f, 5.0f, 0.7f, 0.0f, 0.0f, 26.0f, 3.5f, 2.0f, {0.95f, 0.98f, 1.0f, 0.85f}};   // DESIGN
	Puff entry_drops = {5, 1.2f, 0.8f, 0.45f, 30.0f, 0.0f, 8.0f, 0.0f, 0.0f, {0.95f, 0.98f, 1.0f, 0.9f}};   // KT1 lint: allow frame-literal a speed in elmos per s; DESIGN
	float drop_gravity = -160.0f;       // DESIGN
	float entry_height = 0.5f;          // DESIGN: a followed charge at or under this has entered
	template<class S> void Describe(S& s) {
		s.Field("glow_max", glow_max);
		s.Field("bubble_max", bubble_max);
		s.Field("spark_max", spark_max);
		s.Field("smoke_max", smoke_max);
		s.Field("per_frame", per_frame);
		s.Field("view_pad", view_pad);
		s.Field("surface", surface);
		s.Field("air_above", air_above);
		s.Field("tier_factor", tier_factor);
		s.Object("launch_smoke", [&](auto& o) { launch_smoke.Describe(o); });
		s.Object("hit_flash", [&](auto& o) { hit_flash.Describe(o); });
		s.Object("hit_bubbles", [&](auto& o) { hit_bubbles.Describe(o); });
		s.Object("hit_ring", [&](auto& o) { hit_ring.Describe(o); });
		s.Object("dome_centre", [&](auto& o) { dome_centre.Describe(o); });
		s.Object("dome_ring", [&](auto& o) { dome_ring.Describe(o); });
		s.Field("dome_delay", dome_delay);
		s.Field("dome_max_depth", dome_max_depth);
		s.Object("surface_patch", [&](auto& o) { surface_patch.Describe(o); });
		s.Field("intercept_travel", intercept_travel);
		s.Object("intercept_shot", [&](auto& o) { intercept_shot.Describe(o); });
		s.Object("intercept_burst", [&](auto& o) { intercept_burst.Describe(o); });
		s.Object("intercept_sparks", [&](auto& o) { intercept_sparks.Describe(o); });
		s.Object("intercept_bubbles", [&](auto& o) { intercept_bubbles.Describe(o); });
		s.Object("entry_ring", [&](auto& o) { entry_ring.Describe(o); });
		s.Object("entry_drops", [&](auto& o) { entry_drops.Describe(o); });
		s.Field("drop_gravity", drop_gravity);
		s.Field("entry_height", entry_height);
	}
};

struct FollowedCharge { int32_t projectile = -1; Core::Fx::Vec3 last; };
struct DepthStats { uint32_t hits = 0, launches = 0, entries = 0, intercepts = 0, particles = 0, dome = 0, smokeUnder = 0, skipped = 0; };

struct DepthState {
	KT1::FxDraw::ParticlePool pools[static_cast<int>(DepthPool::Count)];
	std::vector<FollowedCharge> charges;
	std::vector<DelayedParticle> plan;       // scratch
	KT1::FxDraw::Random random;
	uint32_t frame = 0;
	int32_t used = 0;
	DepthStats stats;
};

DepthMedium DepthMediumOf(const DepthConfig& c, float y, float groundHeight);
DepthMedium HitPlan(const DepthConfig& c, const Core::Fx::Vec3& pos, float groundHeight, int32_t tier,
	KT1::FxDraw::Random& random, std::vector<DelayedParticle>& out);
DepthMedium InterceptPlan(const DepthConfig& c, const Core::Fx::Vec3* from, const Core::Fx::Vec3& pos, float groundHeight,
	KT1::FxDraw::Random& random, std::vector<DelayedParticle>& out);
void EntryPlan(const DepthConfig& c, float x, float z, KT1::FxDraw::Random& random, std::vector<DelayedParticle>& out);
void LaunchPlan(const DepthConfig& c, const Core::Fx::Vec3& pos, KT1::FxDraw::Random& random,
	std::vector<DelayedParticle>& out);
// writes the plan's particles, up to the frame's limit
void EmitDepth(DepthState& s, const DepthConfig& c, float now, uint32_t frame);

} // namespace KT1::FxFields

#endif
