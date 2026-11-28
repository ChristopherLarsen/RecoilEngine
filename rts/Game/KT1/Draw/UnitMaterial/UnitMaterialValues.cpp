/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 draw: stubs of UnitMaterialValues.h (LUA-FREE H01). UNITDRAW step D06 writes the bodies.

#include "UnitMaterialValues.h"

namespace KT1::UnitMaterial {

void UnitMaterialState::Init(int32_t) {}
void UnitMaterialState::Remove(int32_t) {}
UnitMaterialValues ValuesFor(const UnitMaterialConfig&, const UnitMaterialFacts&, int) { return {}; }
bool Set(UnitMaterialState&, int32_t, const UnitMaterialValues&) { return false; }

const UnitMaterialState& State() {
	static const UnitMaterialState none;
	return none;
}

} // namespace KT1::UnitMaterial
