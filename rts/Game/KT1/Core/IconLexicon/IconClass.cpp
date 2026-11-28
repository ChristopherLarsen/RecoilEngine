/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 core: stubs of IconClass.h (LUA-FREE H01). UNITDRAW step D01 writes the bodies.

#include "IconClass.h"

namespace KT1::Core::IconLexicon {

IconClass ClassFor(const IconDefFacts&, const IconConfig&) { return {}; }
std::string MapKey(const IconDefFacts&) { return {}; }
Shape ShapeFor(std::string_view, std::string_view, int, bool) { return Shape::Diamond; }
int Lod(float, const IconStyleConfig&) { return 0; }
float NearestStep(const IconSizeConfig&, float) { return 1; }
IconBox BoxFor(Shape, float, const IconConfig&) { return {}; }

} // namespace KT1::Core::IconLexicon
