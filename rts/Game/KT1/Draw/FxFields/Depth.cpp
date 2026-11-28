/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 engine-free
// KT1 FxFields: the depth effects. Stub (LUA-FREE H01): step D14 writes the bodies.

#include "Game/KT1/Draw/FxFields/Depth.h"

namespace KT1::FxFields {

DepthMedium DepthMediumOf(const DepthConfig&, float, float) { return DepthMedium::Ground; }
DepthMedium HitPlan(const DepthConfig&, const Core::Fx::Vec3&, float, int32_t, KT1::FxDraw::Random&,
	std::vector<DelayedParticle>&) { return DepthMedium::Ground; }
DepthMedium InterceptPlan(const DepthConfig&, const Core::Fx::Vec3*, const Core::Fx::Vec3&, float, KT1::FxDraw::Random&,
	std::vector<DelayedParticle>&) { return DepthMedium::Ground; }
void EntryPlan(const DepthConfig&, float, float, KT1::FxDraw::Random&, std::vector<DelayedParticle>&) {}
void LaunchPlan(const DepthConfig&, const Core::Fx::Vec3&, KT1::FxDraw::Random&, std::vector<DelayedParticle>&) {}
void EmitDepth(DepthState&, const DepthConfig&, float, uint32_t) {}

} // namespace KT1::FxFields
