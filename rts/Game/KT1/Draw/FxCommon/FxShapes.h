/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 EFFECTS draw: the other instanced shapes the effects share: ribbon segments, shot bodies, beams and ground quads
// (replaces the CPU halves of LuaRules/cfx_ribbons_gl4.lua and cfx_beams_gl4.lua and the ground quads of
// gfx_fx_lights.lua). The GL halves are FxRender.h. Engine-free.

#ifndef KT1_FX_SHAPES_H
#define KT1_FX_SHAPES_H

#include <cstdint>
#include <string>
#include <vector>

#include "Game/KT1/Draw/FxCommon/FxParticles.h"

namespace KT1::FxDraw {

// ---------------------------------------------------------------- GPU instances (16 floats unless said)

struct RibbonSegment {          // shader locations 1 .. 4
	float p0t[4];               // first point, its time (s)
	float p1t[4];               // second point, its time
	float shape[4];             // width at birth, width at the end of life (elmos), life (s), u at p0 (elmos)
	float color[4];
};
static_assert(sizeof(RibbonSegment) == 64);

struct BodyInstance {
	float posLen[4];            // position, length (elmos)
	float dirWid[4];            // direction, width
	float pulse[4];             // phase 0 .. 1, amplitude, rate per second (0: the pool's), speed (elmos per step)
	float color[4];
};
static_assert(sizeof(BodyInstance) == 64);

struct BeamInstance {
	float startW[4];            // start, width
	float endPhase[4];          // end, phase
	float color[4];
	float params[4];            // flow, orbs, form, unused
};
static_assert(sizeof(BeamInstance) == 64);

struct GroundQuad {             // 12 floats; the quad follows the ground (a grid of it)
	float posSize[4];           // x, z, half width x, half width z
	float color[4];
	float timing[4];            // birth, life (s), brightness, attack share
};
static_assert(sizeof(GroundQuad) == 48);

enum class RibbonLook : uint8_t { Hot, Smoke };
enum class BodyLook : uint8_t { Capsule, Glow, Bomb };
enum class GroundLook : uint8_t { Glow, Frame };

constexpr int32_t GROUND_QUAD_CELLS = 8;    // PARITY: cells along one side of a ground quad's mesh

struct RibbonPoolConfig {
	std::string look = "hot";   // DESIGN: hot, smoke
	std::string texture;        // DATA: empty: the analytic look
	float tile = 0.0f;          // DATA: elmos of one texture repeat
	float min_px = 1.0f;        // DESIGN: least width on screen, pixels
	float cap = 0.0f;           // DESIGN: above 0: a soft front end this long
	bool head = false;          // DESIGN: the pool holds each ribbon's last stretch to the shot
	int max = 8192;             // DESIGN: segments alive
	template<class S> void Describe(S& s) {
		s.Field("look", look);
		s.Field("texture", texture);
		s.Field("tile", tile);
		s.Field("min_px", min_px);
		s.Field("cap", cap);
		s.Field("head", head);
		s.Field("max", max);
	}
};

// a ring of segments that live in the shader from their two times
struct RibbonPool {
	RibbonPoolConfig config;
	std::vector<RibbonSegment> slots;
	int32_t next = 0, used = 0;
	int32_t dirtyLo = 0, dirtyHi = 0;       // [lo, hi) since the last upload
	float lastEnd = 0.0f;
};

// a list written afresh for the frames it changes in
template<class T> struct InstanceList {
	std::vector<T> items;       // reserved to `max` at Init
	int32_t max = 0;
};

struct RibbonPoint { Vec3 pos; float time = 0.0f; };
struct RibbonStyle { float width0 = 0.0f, width1 = 0.0f, life = 0.0f; Rgba color; };

void InitRibbons(RibbonPool& pool, const RibbonPoolConfig& config);
void AddSegment(RibbonPool& pool, const RibbonPoint& from, const RibbonPoint& to, const RibbonStyle& style, float u0);
void ClearRibbons(RibbonPool& pool);
void InitBodies(InstanceList<BodyInstance>& list, int32_t max);
bool AddBody(InstanceList<BodyInstance>& list, const BodyInstance& body);                 // false: full
void InitBeams(InstanceList<BeamInstance>& list, int32_t max);
bool AddBeam(InstanceList<BeamInstance>& list, const Vec3& from, const Vec3& to, float width, const Rgba& color,
	float flow, float orbs, float phase, float form);
void InitQuads(InstanceList<GroundQuad>& list, int32_t max);
bool AddQuad(InstanceList<GroundQuad>& list, const GroundQuad& quad);

} // namespace KT1::FxDraw

#endif
