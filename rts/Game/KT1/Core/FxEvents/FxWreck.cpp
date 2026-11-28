/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 EFFECTS core: wreck physics. Stub (LUA-FREE H01): step S30 writes the bodies.

#include "Game/KT1/Core/FxEvents/FxWreck.h"

#include "Game/KT1/Core/FxEvents/FxState.h"

namespace KT1::Core::Fx {

WreckParams MakeWreckParams(const WreckConfig&) { return {}; }
void InitWrecks(WreckState&, int32_t) {}
WreckPushWeapon PushWeaponOf(const WreckConfig&, float, float) { return {}; }

void PlaceWreck(WreckState&, const WreckConfig&, int32_t, const WorldQuery&) {}
void ForgetWreck(WreckState&, int32_t, FxOutput&) {}
bool PushWreck(WreckState&, const WreckConfig&, int32_t, float, float, float, float) { return false; }
bool WreckMoving(const WreckState&, int32_t) { return false; }
void SetWreckPose(WreckState&, int32_t, const Vec3&) {}
void ClearWreckPose(WreckState&, int32_t) {}
void ExplodeNearWrecks(WreckState&, const WreckConfig&, const WreckParams&, const WreckPushWeapon&, int32_t, const Vec3&,
	std::vector<WreckNear>&, int32_t, const WorldQuery&, FxOutput&) {}
void StepWrecks(WreckState&, const WreckConfig&, const WreckParams&, int32_t, const WorldQuery&, FxOutput&) {}

} // namespace KT1::Core::Fx
