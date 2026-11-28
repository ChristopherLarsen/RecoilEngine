/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 draw: system "unitbars" (LUA-FREE H01, UNITDRAW §4.6; step D11 writes the bodies). It draws after the icons, one
// instanced draw of rectangles and the countdown texts. The instance layout, config, state and rules are
// Core/IconLexicon/BarState.h (engine-free, so tested there). The HUD's options come from Icons::Options(), the zoom cut
// from ZoomGates::DetailFactor(config.hide_sweep) against FrameFacts::ZoomDist, veterancy from COMBAT's
// Snapshot<Core::Veterancy::VetMark> (rules param vet_level until then).

#ifndef KT1_DRAW_BAR_INSTANCE_H
#define KT1_DRAW_BAR_INSTANCE_H

#include "Game/KT1/Core/IconLexicon/BarState.h"

namespace KT1::UnitBars {

using BarInstance = Core::UnitBars::BarInstance;

const Core::UnitBars::BarState& State();   // the last drawn frame's (drawn and fuel bar counts for tests)

} // namespace KT1::UnitBars

#endif
