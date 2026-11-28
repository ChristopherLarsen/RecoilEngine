/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 engine-free
// KT1 GroundShade: the layers' rules. Stub (LUA-FREE H01): step D08 writes the bodies.

#include "Game/KT1/Draw/GroundShade/ShadeLayers.h"

namespace KT1::GroundShade {

int32_t EffectiveLevel(const ShadeState&) { return 0; }
const BiomeShade& BiomeOf(const ShadeConfig&, const std::string&) {
	static const BiomeShade none;
	return none;
}
bool QueueBlast(ShadeState&, const ShadeConfig&, float, float, float) { return false; }
bool QueueDeath(ShadeState&, const ShadeConfig&, float, float, float, bool) { return false; }
bool AddBlob(ShadeState&, const ShadeConfig&, const Blob&) { return false; }
GlowDisc BuildingGlow(const ShadeConfig&, float, float, int32_t, int32_t, const float (&)[3]) { return {}; }
bool DecayDue(const ShadeState&, const ShadeConfig&, float) { return false; }
bool GlowDue(const ShadeState&, const ShadeConfig&, float) { return false; }
void DiscVertices(const ShadeConfig&, float, float, float, float, const std::vector<ProfileStop>&, const float (&)[3],
	std::vector<DiscVertex>&) {}
ShadeUniforms UniformsOf(const ShadeConfig&, const BiomeShade&, int32_t, bool, const float (&)[3]) { return {}; }

} // namespace KT1::GroundShade
