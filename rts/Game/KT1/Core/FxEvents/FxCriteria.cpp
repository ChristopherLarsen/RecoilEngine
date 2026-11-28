/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 EFFECTS core: death blast timing. Stub (LUA-FREE H01): step S30 writes the bodies.

#include "Game/KT1/Core/FxEvents/FxCriteria.h"

namespace KT1::Core::Fx {

CriteriaRng::CriteriaRng(int32_t) {}
float CriteriaRng::Next() { return 0.0f; }

DeathChain PlanDeathChain(const CriteriaTimingConfig&, float, DeathMedium, bool, float, CriteriaRng&) { return {}; }
float MainBlastAfterS(const CriteriaTimingConfig&, float, DeathMedium, bool, CriteriaRng&) { return 0.0f; }
DeathMedium MediumOfDeath(const CriteriaTimingConfig&, float, float) { return DeathMedium::None; }

} // namespace KT1::Core::Fx
