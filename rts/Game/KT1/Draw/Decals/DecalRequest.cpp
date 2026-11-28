/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 engine-free
// KT1 FxDecals: the scorch marks. Stub (LUA-FREE H01): step D16 writes the bodies.

#include "Game/KT1/Draw/Decals/DecalRequest.h"

namespace KT1::FxDecals {

ScorchTier TierOf(const ScorchConfig&, const Core::Fx::WeaponFx&, bool) { return {}; }
void OnScorch(ScorchState&, const ScorchConfig&, const Core::Fx::Scorch&, bool, int32_t, float, const MarkWorld&,
	DecalOutput&) {}
bool DeathMark(ScorchState&, const ScorchConfig&, float, float, float, float, float, float, const MarkWorld&, DecalOutput&) {
	return false;
}
void ExpireScorch(ScorchState&, const ScorchConfig&, float, DecalOutput&) {}
void ClearScorch(ScorchState&, DecalOutput&) {}

} // namespace KT1::FxDecals
