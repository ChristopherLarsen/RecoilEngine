/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 engine-free
// KT1 FxFields: the blast extras. Stub (LUA-FREE H01): step D14 writes the bodies.

#include "Game/KT1/Draw/FxFields/Blasts.h"

namespace KT1::FxFields {

Core::Fx::Medium MediumOf(const BlastConfig&, float, float) { return Core::Fx::Medium::Ground; }
BlastPlan PlanBlast(const BlastConfig&, Core::Fx::BlastClass, Core::Fx::BlastFamily, Core::Fx::Medium, const BlastView&) {
	return {};
}
int32_t Admit(const BlastConfig&, int32_t, int32_t) { return 0; }
void EmitBlast(BlastState&, const BlastConfig&, const Core::Fx::Blast&, const BlastPlan&, Core::Fx::Medium,
	const KT1::FxDraw::FactionLook&, float, uint32_t) {}

} // namespace KT1::FxFields
