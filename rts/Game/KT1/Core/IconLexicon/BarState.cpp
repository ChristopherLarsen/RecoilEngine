/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 core: stubs of BarState.h (LUA-FREE H01). UNITDRAW step D11 writes the bodies.

#include "BarState.h"

namespace KT1::Core::UnitBars {

void BarState::Init(int32_t) {}
void BarState::Begin() {}
float HideZoom(const BarConfig&, float) { return 0; }
bool BarsShown(const BarConfig&, const IconLexicon::HudOptions&, float, float) { return false; }
float SizeFactor(const BarConfig&, float) { return 1; }
HealthBand BandOf(const BarConfig&, float) { return HealthBand::Healthy; }
bool ShowsBars(const IconLexicon::HudOptions&, const BarUnitFacts&) { return false; }
bool UnderCap(const BarState&, const BarConfig&, const BarUnitFacts&) { return false; }
void AddUnitBars(BarState&, const BarConfig&, const IconLexicon::HudOptions&, const BarUnitFacts&) {}

} // namespace KT1::Core::UnitBars
