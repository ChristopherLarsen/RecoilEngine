/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 EFFECTS core: the effect budget. Stub (LUA-FREE H01): step S30 writes the bodies.

#include "Game/KT1/Core/FxEvents/FxBudget.h"

namespace KT1::Core::Fx {

float BudgetPerSecond(const BudgetConfig&, float) { return 0.0f; }
int32_t BudgetPerStep(const BudgetConfig&, float) { return 0; }

void Budget::Init(const BudgetConfig&, float) {}
void Budget::SetScale(float) {}
void Budget::NewStep(int32_t) {}
bool Budget::Admit(Priority, Refusal*) { return false; }

} // namespace KT1::Core::Fx
