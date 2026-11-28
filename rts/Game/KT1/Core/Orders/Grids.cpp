/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 Orders core: stubs behind Grids.h (LUA-FREE H01; step S10 ports them).

#include "Grids.h"

namespace KT1::Core::Orders {

void ChangeGrid::Init(float, float, float, float) {}
void ChangeGrid::Mark(float, float, float, Step) {}
bool ChangeGrid::Near(float, float, float, Step) const { return false; }

void StructureGrid::Init(float, float, float, int32_t) {}
void StructureGrid::Add(UnitId, float, float) {}
void StructureGrid::Remove(UnitId) {}
void StructureGrid::Candidates(float, float, float, std::vector<UnitId>&) const {}

} // namespace KT1::Core::Orders
