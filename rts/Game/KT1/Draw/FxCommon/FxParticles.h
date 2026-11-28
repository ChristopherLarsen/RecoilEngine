/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 EFFECTS draw: the particle pool every effect writes into (replaces the CPU half of LuaRules/cfx_particles_gl4.lua;
// the GL half is FxRender.h). A ring of fixed size: the oldest particle gives way. A particle moves in the shader from
// its birth values, so it is written once. Engine-free.

#ifndef KT1_FX_PARTICLES_H
#define KT1_FX_PARTICLES_H

#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "Game/KT1/Core/FxEvents/FxTypes.h"

namespace KT1::FxDraw {

using Core::Fx::Vec3;

struct Rgb { float r = 1.0f, g = 1.0f, b = 1.0f; };
struct Rgba { float r = 1.0f, g = 1.0f, b = 1.0f, a = 1.0f; };

// one instance, 16 floats (shader locations 1 .. 4)
struct ParticleInstance {
	float posBirth[4];          // x, y, z (elmos), birth (s of FxTime::now)
	float velLife[4];           // vx, vy, vz (elmos per s), life (s)
	float shape[4];             // size at birth, size at death (elmos), gravity (y per s squared), drag (per s)
	float color[4];             // r, g, b, a
};
static_assert(sizeof(ParticleInstance) == 64);

enum class ParticleLook : uint8_t { Smoke, Spark, Bubble, Glow };

constexpr int32_t RAMP_KEYS = 5;      // PARITY: pool colour ramps have keys at life 0, 0.25, 0.5, 0.75, 1

struct RampKey {
	float rgba[4] = {1.0f, 1.0f, 1.0f, 1.0f};   // DATA
	template<class S> void Describe(S& s) { s.Field("rgba", rgba); }
};
struct GradeKey {
	float rgb[3] = {1.0f, 1.0f, 1.0f};          // DATA
	template<class S> void Describe(S& s) { s.Field("rgb", rgb); }
};

struct ParticlePoolConfig {
	int max = 4096;                   // DESIGN: particles alive
	std::string look = "smoke";       // DESIGN: smoke, spark, bubble, glow
	bool depth = true;                // DESIGN: false: seen through smoke and terrain (a flash)
	bool flat = false;                // DESIGN: the squares lie on the ground
	float pull = 0.0f;                // DESIGN: each quad moves this many times its size towards the camera
	float lift = 0.0f;                // DESIGN: elmos
	std::string texture;              // DATA: empty: the look's analytic shader
	int frames = 1;                   // DATA: frames of the texture
	int grid_cols = 0;                // DATA: atlas columns; 0: a strip of `frames`
	int grid_rows = 0;                // DATA
	bool premul = false;              // DATA: premultiplied texture
	float energy = 0.0f;              // DESIGN: premultiplied additive: share of alpha that dims the background
	std::vector<RampKey> ramp;        // DATA: empty, or RAMP_KEYS multipliers over life
	bool ramp_only = false;           // DATA: the ramp alone is the life curve
	float soft = 0.0f;                // DESIGN: 0 .. 1 soft edge and noise erosion
	float spin = 0.0f;                // DESIGN: turns per life
	float elong = 0.0f;               // DESIGN: elmos each way along the motion a glow is drawn longer
	std::vector<GradeKey> grade;      // DATA: empty, or edge, middle and hot: colour by texel brightness
	template<class S> void Describe(S& s) {
		s.Field("max", max);
		s.Field("look", look);
		s.Field("depth", depth);
		s.Field("flat", flat);
		s.Field("pull", pull);
		s.Field("lift", lift);
		s.Field("texture", texture);
		s.Field("frames", frames);
		s.Field("grid_cols", grid_cols);
		s.Field("grid_rows", grid_rows);
		s.Field("premul", premul);
		s.Field("energy", energy);
		s.List("ramp", ramp, [](auto& o, RampKey& k) { k.Describe(o); });
		s.Field("ramp_only", ramp_only);
		s.Field("soft", soft);
		s.Field("spin", spin);
		s.Field("elong", elong);
		s.List("grade", grade, [](auto& o, GradeKey& k) { k.Describe(o); });
	}
};

struct ParticleAdd {
	Vec3 pos, vel;
	float birth = 0.0f, life = 0.0f;
	float size0 = 0.0f, size1 = 0.0f;
	float gravity = 0.0f, drag = 0.0f;
	Rgba color;
};

struct ParticlePool {
	ParticlePoolConfig config;
	std::vector<ParticleInstance> slots;      // sized to config.max at Init
	int32_t next = 0, used = 0;
	int32_t dirtyLo = 0, dirtyHi = 0;         // slots written since the last upload: [lo, hi)
	float lastEnd = 0.0f;                     // when the last particle written ends: nothing to draw after it
	uint32_t added = 0;
};

ParticleLook LookOf(const std::string& name);   // unknown: Smoke
void InitPool(ParticlePool& pool, const ParticlePoolConfig& config);
void AddParticle(ParticlePool& pool, const ParticleAdd& p);
// n particles on a sphere of `radius` round the centre, moving in to it over their life (plus the centre's own velocity)
void Implode(ParticlePool& pool, const Vec3& centre, float radius, int32_t n, float birth, float life, float size0,
	float size1, const Rgba& color, const Vec3& centreVel, uint32_t seed);
void ClearPool(ParticlePool& pool);

} // namespace KT1::FxDraw

#endif
