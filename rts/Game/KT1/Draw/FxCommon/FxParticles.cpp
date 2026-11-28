/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 engine-free
// KT1 EFFECTS draw: the particle pool. Stub (LUA-FREE H01): step D03 writes the bodies.

#include "Game/KT1/Draw/FxCommon/FxParticles.h"

namespace KT1::FxDraw {

ParticleLook LookOf(const std::string&) { return ParticleLook::Smoke; }
void InitPool(ParticlePool&, const ParticlePoolConfig&) {}
void AddParticle(ParticlePool&, const ParticleAdd&) {}
void Implode(ParticlePool&, const Vec3&, float, int32_t, float, float, float, float, const Rgba&, const Vec3&, uint32_t) {}
void ClearPool(ParticlePool&) {}

} // namespace KT1::FxDraw
