/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 engine-free
// KT1 EFFECTS draw: ribbons, bodies, beams and ground quads. Stub (LUA-FREE H01): steps D03 to D05 write the bodies.

#include "Game/KT1/Draw/FxCommon/FxShapes.h"

namespace KT1::FxDraw {

void InitRibbons(RibbonPool&, const RibbonPoolConfig&) {}
void AddSegment(RibbonPool&, const RibbonPoint&, const RibbonPoint&, const RibbonStyle&, float) {}
void ClearRibbons(RibbonPool&) {}
void InitBodies(InstanceList<BodyInstance>&, int32_t) {}
bool AddBody(InstanceList<BodyInstance>&, const BodyInstance&) { return false; }
void InitBeams(InstanceList<BeamInstance>&, int32_t) {}
bool AddBeam(InstanceList<BeamInstance>&, const Vec3&, const Vec3&, float, const Rgba&, float, float, float, float) {
	return false;
}
void InitQuads(InstanceList<GroundQuad>&, int32_t) {}
bool AddQuad(InstanceList<GroundQuad>&, const GroundQuad&) { return false; }

} // namespace KT1::FxDraw
