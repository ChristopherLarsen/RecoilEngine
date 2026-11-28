/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 EFFECTS draw: the GL renderers. Stub (LUA-FREE H01): steps D03 to D05 write the bodies.

#include "Game/KT1/Draw/FxCommon/FxRender.h"

#include "Game/KT1/Draw/Common/FrameFacts.h"

namespace KT1::FxDraw {

struct FxGl {};

ParticleRenderer::ParticleRenderer() = default;
ParticleRenderer::~ParticleRenderer() = default;
bool ParticleRenderer::Init(const char*, const ParticlePoolConfig&) { return false; }
void ParticleRenderer::Draw(ParticlePool&, const FrameFacts&, float) {}
void ParticleRenderer::Kill() {}

RibbonRenderer::RibbonRenderer() = default;
RibbonRenderer::~RibbonRenderer() = default;
bool RibbonRenderer::Init(const char*, const RibbonPoolConfig&) { return false; }
void RibbonRenderer::Draw(RibbonPool&, const FrameFacts&, float, float) {}
void RibbonRenderer::Kill() {}

BodyRenderer::BodyRenderer() = default;
BodyRenderer::~BodyRenderer() = default;
bool BodyRenderer::Init(const char*, BodyLook, const std::string&, int32_t) { return false; }
void BodyRenderer::Draw(const InstanceList<BodyInstance>&, const FrameFacts&, float, float, float) {}
void BodyRenderer::Kill() {}

BeamRenderer::BeamRenderer() = default;
BeamRenderer::~BeamRenderer() = default;
bool BeamRenderer::Init(const char*, int32_t) { return false; }
void BeamRenderer::Draw(const InstanceList<BeamInstance>&, const FrameFacts&, float) {}
void BeamRenderer::Kill() {}

GroundQuadRenderer::GroundQuadRenderer() = default;
GroundQuadRenderer::~GroundQuadRenderer() = default;
bool GroundQuadRenderer::Init(const char*, GroundLook, int32_t, float) { return false; }
void GroundQuadRenderer::Draw(const InstanceList<GroundQuad>&, const FrameFacts&, float) {}
void GroundQuadRenderer::Kill() {}

} // namespace KT1::FxDraw
