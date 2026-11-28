/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 FxBuild: the stream of matter from a reclaim target to the builder's nozzle (replaces
// LuaRules/Gadgets/gfx_reclaim_particles.lua; the handoff GG.ReclaimFxPairs is inside the system now). Three versions
// (option reclaim_fx: 1 dust, 2 debris, 3 vortex; 0 the beam only). Config values are the Lua's before its spread, speed
// and size factors, which are applied on top. Engine-free.

#ifndef KT1_RECLAIM_STREAM_H
#define KT1_RECLAIM_STREAM_H

#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "Game/KT1/Core/FxEvents/FxTypes.h"
#include "Game/KT1/Draw/FxCommon/FxRandom.h"

namespace KT1::FxBuild {

constexpr int32_t RECLAIM_VERSIONS = 3;   // DESIGN

struct ReclaimVersion {
	float rate = 640.0f;              // DESIGN: particles per second
	float life[2] = {0.5f, 0.75f};    // DESIGN
	float size[2] = {0.6f, 1.3f};     // DESIGN
	float cloud = 1.0f;               // DESIGN: spawn radius over the target's
	float pull = 1.7f;                // DESIGN
	float bend = 0.0f;                // DESIGN
	bool cone = false;                // DESIGN: spawn on a disc across the path
	float spin = 0.0f;                // DESIGN: turns
	bool chunk = false;               // DESIGN: flakes, not motes
	float ember = 0.0f;               // DESIGN: share of ember particles
	float alpha = 0.8f;               // DESIGN
	template<class S> void Describe(S& s) {
		s.Field("rate", rate);
		s.Field("life", life);
		s.Field("size", size);
		s.Field("cloud", cloud);
		s.Field("pull", pull);
		s.Field("bend", bend);
		s.Field("cone", cone);
		s.Field("spin", spin);
		s.Field("chunk", chunk);
		s.Field("ember", ember);
		s.Field("alpha", alpha);
	}
};

struct ReclaimConfig {                // "reclaim"
	int version = 1;                  // DESIGN: 0 .. 3; mod option and setting override
	std::string mod_option = "reclaim_fx";        // DESIGN
	std::string setting = "KardashevReclaimFx";   // DESIGN
	int max_total = 1500;             // DESIGN: particles over all reclaimers
	int max_emitters = 8;             // DESIGN: nearest reclaimers that emit
	float spread = 0.35f;             // DESIGN: spawn area factor
	float speed = 4.0f;               // DESIGN: the life is divided by this
	float size = 0.35f;               // DESIGN: size factor
	float stretch = 3.0f;             // DESIGN: a mote's length over its width
	float moving_speed = 1.5f;        // DESIGN: elmos per s: a builder faster than this reclaims nothing
	float step_max = 0.1f;            // DESIGN: seconds, the longest step
	// DESIGN: black, dark brown, brown, grey, dark grey
	float palette[5][3] = {{0.05f, 0.045f, 0.04f}, {0.20f, 0.13f, 0.08f}, {0.32f, 0.22f, 0.13f}, {0.42f, 0.41f, 0.40f},
		{0.22f, 0.22f, 0.22f}};
	float ember_colour[3] = {0.62f, 0.30f, 0.10f};    // DESIGN
	ReclaimVersion versions[RECLAIM_VERSIONS] = {
		{640.0f, {0.5f, 0.75f}, {0.6f, 1.3f}, 1.0f, 1.7f, 0.0f, true, 0.0f, false, 0.0f, 0.80f},
		{70.0f, {0.55f, 0.85f}, {1.8f, 4.2f}, 1.0f, 2.2f, 0.18f, false, 0.0f, true, 0.22f, 0.95f},
		{240.0f, {1.2f, 1.7f}, {0.7f, 1.5f}, 1.15f, 1.5f, 0.10f, false, 3.2f, false, 0.06f, 0.85f},
	};
	template<class S> void Describe(S& s) {
		s.Field("version", version);
		s.Field("mod_option", mod_option);
		s.Field("setting", setting);
		s.Field("max_total", max_total);
		s.Field("max_emitters", max_emitters);
		s.Field("spread", spread);
		s.Field("speed", speed);
		s.Field("size", size);
		s.Field("stretch", stretch);
		s.Field("moving_speed", moving_speed);
		s.Field("step_max", step_max);
		static constexpr const char* PALETTE_KEYS[5] = {"palette_0", "palette_1", "palette_2", "palette_3", "palette_4"};
		for (int i = 0; i < 5; ++i) s.Field(PALETTE_KEYS[i], palette[i]);
		s.Field("ember_colour", ember_colour);
		static constexpr const char* VERSION_KEYS[RECLAIM_VERSIONS] = {"dust", "debris", "vortex"};
		for (int i = 0; i < RECLAIM_VERSIONS; ++i) s.Object(VERSION_KEYS[i], [&](auto& o) { versions[i].Describe(o); });
	}
};

struct ReclaimPair {                  // a reclaiming builder: from the target to the nozzle
	int32_t builder = -1;
	Core::Fx::Vec3 nozzle, target;
	float targetRadius = 0.0f;
	bool moving = false;
};

struct ReclaimParticle {
	int32_t builder = -1;
	Core::Fx::Vec3 from;
	float t = 0.0f, life = 0.0f, size = 0.0f, rot = 0.0f, rotSpeed = 0.0f, phase = 0.0f, side = 0.0f, lift = 0.0f, aspect = 1.0f;
	float rgb[3] = {0.0f, 0.0f, 0.0f};
};

// one corner of a particle's quad (drawn as triangles)
struct ReclaimVertex { float pos[3] = {0.0f, 0.0f, 0.0f}; float rgba[4] = {0.0f, 0.0f, 0.0f, 0.0f}; };
static_assert(sizeof(ReclaimVertex) == 28);

struct ReclaimStats { uint32_t parts = 0, emitters = 0, spawned = 0; };
struct ReclaimEmitter { int32_t builder = -1; float carry = 0.0f; };

struct ReclaimState {
	std::vector<ReclaimParticle> particles;
	std::vector<ReclaimEmitter> emitters;
	std::vector<ReclaimVertex> vertices;
	KT1::FxDraw::Random random;
	ReclaimStats stats;
};

// pairs: the reclaiming builders, nearest first
void StepReclaim(ReclaimState& s, const ReclaimConfig& c, int32_t version, std::span<const ReclaimPair> pairs, float delta);
// a particle's place now and its life share
Core::Fx::Vec3 PlaceParticle(const ReclaimVersion& v, const ReclaimParticle& p, const Core::Fx::Vec3& nozzle, float t,
	float& share);
void BuildVertices(ReclaimState& s, const ReclaimConfig& c, int32_t version, std::span<const ReclaimPair> pairs,
	const Core::Fx::Vec3& cameraRight, const Core::Fx::Vec3& cameraUp);

} // namespace KT1::FxBuild

#endif
