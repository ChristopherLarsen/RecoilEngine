/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 draw: stubs of MinimapLayer.h (LUA-FREE H01). UNITDRAW step D02 writes the bodies.

#include "MinimapLayer.h"

namespace KT1::MinimapUnits {

const Core::IconLexicon::MinimapState& State() {
	static const Core::IconLexicon::MinimapState none;
	return none;
}

void SetShown(bool) {}
bool Shown() { return false; }

} // namespace KT1::MinimapUnits
