/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 template: the logic of a draw system. No engine include: tested engine-free (Test/_TEMPLATE/TemplateDrawTest.cpp).
// Copy to Core/<System>/<System>Core.h.
//
// The example: where and how strongly a dot is drawn, from the two published steps (LUA-FREE-PLAN 2.2 principle 8) and
// how far the drawn frame is into the step. Blending previous to current draws one step behind and stays smooth at any
// sim rate; extrapolating is the other choice (plan 9 D8).

#ifndef KT1_TEMPLATE_DRAW_CORE_H
#define KT1_TEMPLATE_DRAW_CORE_H

#include <cstdint>

#include "Game/KT1/Core/Common/DetMath.h"
#include "Game/KT1/Core/_TEMPLATE/TemplateSnapshot.h"

namespace KT1::Core::Template {

// `previous` is null for a unit that is new in the current step
// KT1 hot
inline Dot Blend(const Dot* previous, const Dot& current, float stepFraction) {
	if (previous == nullptr) return current;
	const float t = DetMath::Clamp(stepFraction, 0, 1);
	return {DetMath::Lerp(previous->x, current.x, t), DetMath::Lerp(previous->y, current.y, t),
	        DetMath::Lerp(previous->z, current.z, t), DetMath::Lerp(previous->charge, current.charge, t)};
}

// opacity 0 .. full for a charge 0 .. fullCharge
// KT1 hot
inline uint8_t Alpha(float charge, float fullCharge, uint8_t full) {
	if (fullCharge <= 0) return full;
	return static_cast<uint8_t>(DetMath::Clamp(charge / fullCharge, 0, 1) * full);
}

} // namespace KT1::Core::Template

#endif
