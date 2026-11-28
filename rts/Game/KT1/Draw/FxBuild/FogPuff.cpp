/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 engine-free
// KT1 FxBuild: the fog death puff. Stub (LUA-FREE H01): step D15 writes the bodies.

#include "Game/KT1/Draw/FxBuild/FogPuff.h"

namespace KT1::FxBuild {

bool OnFogDeath(FogPuffState&, const FogPuffConfig&, const Core::Fx::FogDeath&, bool, bool, bool, float, float) {
	return false;
}
void ExpirePuffs(FogPuffState&, const FogPuffConfig&, float) {}

} // namespace KT1::FxBuild
