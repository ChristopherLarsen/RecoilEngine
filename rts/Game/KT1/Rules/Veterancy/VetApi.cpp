/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 Veterancy: VetApi.h before step S26 (LUA-FREE H01): the system is not live, every answer is neutral.

#include "VetApi.h"

namespace KT1::Veterancy {

bool Active() { return false; }

int32_t Level(int32_t) { return 0; }
bool Xp(int32_t, float&) { return false; }
bool Mark(int32_t, Core::Veterancy::VetMark&) { return false; }
bool Bounds(int32_t, float&, float&) { return false; }

void Credit(int32_t, float) {}
void CreditCargo(int32_t, int32_t, int32_t, int32_t) {}
void Carry(int32_t, int32_t) {}
void SetBase(int32_t, float) {}
void SetCost(int32_t, float) {}

} // namespace KT1::Veterancy
