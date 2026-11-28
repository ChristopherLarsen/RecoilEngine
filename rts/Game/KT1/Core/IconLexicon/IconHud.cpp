/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 core: stubs of IconHud.h (LUA-FREE H01). UNITDRAW step D01 writes the bodies.

#include "IconHud.h"

namespace KT1::Core::IconLexicon {

Badges BadgesFor(const IconStyleConfig&, const IconClass&, const IconUnitFacts&) { return {}; }
std::string HudRml(const IconConfig&, const IconClass&, int, const Paint&, const Badges&) { return {}; }

} // namespace KT1::Core::IconLexicon
