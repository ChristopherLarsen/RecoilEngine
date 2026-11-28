/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 draw: stubs of BarInstance.h (LUA-FREE H01). UNITDRAW step D11 writes the bodies.

#include "BarInstance.h"

namespace KT1::UnitBars {

const Core::UnitBars::BarState& State() {
	static const Core::UnitBars::BarState none;
	return none;
}

} // namespace KT1::UnitBars
