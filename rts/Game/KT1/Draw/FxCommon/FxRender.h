/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 EFFECTS draw: the GL halves of the family's pools (FxParticles.h, FxShapes.h). One renderer per pool: Init once
// with a GL context, Draw in the system's draw phase, Kill when the game ends. Each uploads only what changed, sets the
// blending and depth state its look needs and puts the state back. `now` is FxTime::now.

#ifndef KT1_FX_RENDER_H
#define KT1_FX_RENDER_H

#include <memory>
#include <string>

#include "Game/KT1/Draw/FxCommon/FxShapes.h"

namespace KT1 {

class FrameFacts;

namespace FxDraw {

struct FxGl;                    // programs, buffers and the texture of one renderer (FxRender.cpp)

class ParticleRenderer {
public:
	ParticleRenderer();
	~ParticleRenderer();
	bool Init(const char* name, const ParticlePoolConfig& config);   // false: no GL, or the shader failed
	void Draw(ParticlePool& pool, const FrameFacts& facts, float now);
	void Kill();
private:
	std::unique_ptr<FxGl> gl;
};

class RibbonRenderer {
public:
	RibbonRenderer();
	~RibbonRenderer();
	bool Init(const char* name, const RibbonPoolConfig& config);
	// simNow: the current step's time; a segment laid in it is the newest
	void Draw(RibbonPool& pool, const FrameFacts& facts, float now, float simNow);
	void Kill();
private:
	std::unique_ptr<FxGl> gl;
};

class BodyRenderer {
public:
	BodyRenderer();
	~BodyRenderer();
	bool Init(const char* name, BodyLook look, const std::string& texture, int32_t max);
	// since: seconds since the bodies were written; each is carried along its velocity by it
	void Draw(const InstanceList<BodyInstance>& list, const FrameFacts& facts, float now, float pulseHz, float since);
	void Kill();
private:
	std::unique_ptr<FxGl> gl;
};

class BeamRenderer {
public:
	BeamRenderer();
	~BeamRenderer();
	bool Init(const char* name, int32_t max);
	void Draw(const InstanceList<BeamInstance>& list, const FrameFacts& facts, float now);
	void Kill();
private:
	std::unique_ptr<FxGl> gl;
};

class GroundQuadRenderer {
public:
	GroundQuadRenderer();
	~GroundQuadRenderer();
	bool Init(const char* name, GroundLook look, int32_t max, float lift);
	void Draw(const InstanceList<GroundQuad>& list, const FrameFacts& facts, float now);
	void Kill();
private:
	std::unique_ptr<FxGl> gl;
};

} // namespace FxDraw
} // namespace KT1

#endif
