/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 core: stubs of IconMinimapState.h (LUA-FREE H01). UNITDRAW step D02 writes the bodies.

#include "IconMinimapState.h"

namespace KT1::Core::IconLexicon {

void MinimapState::Init(int32_t) {}
bool MakeMinimapInstance(const IconConfig&, const IconClass*, const IconUnitFacts&, const Paint&, IconKind, MinimapInstance&) {
	return false;
}
void MinimapPoint(const float (&)[3], float, float, float, float, float& outX, float& outY) {
	outX = 0;
	outY = 0;
}
void BuildMinimap(MinimapState&, const IconConfig&, const IconState&, std::span<const IconUnitFacts>,
                  std::span<const IconClass>, std::span<const Paint>) {}
bool ContactDue(const MinimapState&, const IconMinimapConfig&, int32_t) { return false; }

} // namespace KT1::Core::IconLexicon
