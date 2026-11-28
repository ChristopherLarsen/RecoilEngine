/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 engine-free
// KT1 FxBuild: the reclaim stream. Stub (LUA-FREE H01): step D15 writes the bodies.

#include "Game/KT1/Draw/FxBuild/ReclaimStream.h"

namespace KT1::FxBuild {

void StepReclaim(ReclaimState&, const ReclaimConfig&, int32_t, std::span<const ReclaimPair>, float) {}
Core::Fx::Vec3 PlaceParticle(const ReclaimVersion&, const ReclaimParticle&, const Core::Fx::Vec3&, float, float& share) {
	share = 0.0f;
	return {};
}
void BuildVertices(ReclaimState&, const ReclaimConfig&, int32_t, std::span<const ReclaimPair>, const Core::Fx::Vec3&,
	const Core::Fx::Vec3&) {}

} // namespace KT1::FxBuild
