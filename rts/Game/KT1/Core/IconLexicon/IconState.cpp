/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 core: stubs of IconState.h (LUA-FREE H01). UNITDRAW steps D01 and D02 write the bodies.

#include "IconState.h"

namespace KT1::Core::IconLexicon {

Rgb FillColour(const Rgb& team, const IconStyleConfig&) { return team; }
Paint PaintFor(const IconConfig&, RelationMode, bool, Relation, const Rgb& teamFill) { return {teamFill, false}; }
IconKind KindOf(const IconClass*, const IconUnitFacts&, bool) { return IconKind::None; }
ContactLayer LayerOf(float, float, const IconViewConfig&) { return ContactLayer::Land; }
bool MakeInstance(const IconConfig&, const IconParams&, const IconClass*, const IconUnitFacts&, const Paint&, IconKind,
                  IconInstance&) { return false; }
int32_t HitRect(std::span<const IconRect>, float, float) { return -1; }
bool IsDoubleClick(float, float, float, bool, float, float) { return false; }

void IconState::Init(int32_t, int32_t) {}
void IconCreated(IconState&, int32_t) {}
void IconSeen(IconState&, int32_t) {}
void IconGone(IconState&, int32_t) {}
void IconMark(IconState&, int32_t) {}
void IconMarkAll(IconState&) {}
void RebuildIcons(IconState&, const IconConfig&, const IconParams&, std::span<const IconUnitFacts>,
                  std::span<const IconClass>, std::span<const Paint>) {}
bool ApplyIcon(IconState&, const IconConfig&, const IconParams&, const IconUnitFacts&, const IconClass*, const Paint&) {
	return false;
}
void FlickerStart(IconState&, const IconViewConfig&, int32_t, float) {}
void FlickerStep(IconState&, const IconViewConfig&, float) {}

} // namespace KT1::Core::IconLexicon
