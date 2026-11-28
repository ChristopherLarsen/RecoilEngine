/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 engine-free
// KT1 FxBuild: the beams and welds. Stub (LUA-FREE H01): step D15 writes the bodies.

#include "Game/KT1/Draw/FxBuild/BuildBeams.h"

namespace KT1::FxBuild {

BeamLook LookOfBeam(const BeamsConfig&, Core::Fx::BuildWork, bool, Core::Fx::Faction) { return {}; }
bool Welds(Core::Fx::BuildWork) { return false; }
Vec3 SurfaceEnd(const BeamsConfig&, const Vec3&, const Vec3&, float) { return {}; }
void BandOf(float, float, float& bandY, float& left) { bandY = 0.0f; left = 0.0f; }
Vec3 WeldPoint(const BeamsConfig&, const Core::Fx::BuildSample&, KT1::FxDraw::Random&) { return {}; }
Vec3 SparkAt(const BeamsConfig&, const Spark&, float) { return {}; }
void InitBeamsState(BeamsState&, const BeamsConfig&) {}
void StepBeams(BeamsState&, const BeamsConfig&, std::span<const BuilderIn>, bool, float) {}

} // namespace KT1::FxBuild
