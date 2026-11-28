/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 AMove: stubs behind AMoveApi.h (LUA-FREE H01; step S03 ports them).

#include "AMoveApi.h"

namespace KT1::AMove {

const Core::AMove::State* State() { return nullptr; }
void Start(const SimContext&, int, std::uint32_t, float, float, float) {}
bool Running(int, std::uint32_t) { return false; }
void End(const SimContext&, int) {}
void UnitDestroyed(const SimContext&, int) {}
void FrontChanged(int) {}
void CapGroups(const SimContext&) {}

} // namespace KT1::AMove
