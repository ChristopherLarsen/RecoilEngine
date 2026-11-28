/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 Targeting core: TargetState.h before step S19 (LUA-FREE H01): neutral stubs, no rule runs. No engine include.

#include "Game/KT1/Core/Targeting/TargetState.h"

namespace KT1::Core::Targeting {

const char* RefusalName(Refusal) { return ""; }

TargetingParams MakeTargetingParams(const TargetingConfig&, const CommandIds&, int32_t) { return {}; }

int32_t ScanSteps(const TargetingParams&, float) { return 0; }
int32_t FirstScan(const TargetingParams&, int32_t, int32_t, int32_t, int32_t) { return 0; }
int32_t ComputeRank(const WeaponTargets&, const UnitTargets&) { return 0; }
void BuildRanks(TargetingDefs&) {}
Refusal LayerCheck(const LayerReach&, bool, Layer, float, float) { return Refusal::None; }

void TargetState::Init(int32_t) {}

void TargetUnitCreated(TargetState&, const TargetingContext&, int32_t, int32_t, int32_t) {}
void TargetUnitDestroyed(TargetState&, int32_t) {}
void SetFocus(TargetState&, int32_t, int32_t) {}

bool AddShot(TargetState&, const TargetingContext&, int32_t, int32_t, int32_t, float, float&) { return false; }
bool RemoveShot(TargetState&, const TargetingContext&, int32_t, int32_t&, float&) { return false; }
float InFlight(const TargetState&, int32_t) { return 0; }

bool Contact(TargetingWorld&, int32_t, int32_t) { return false; }

TargetTest Acceptable(const TargetingContext&, int32_t, int32_t, int32_t, bool, TargetingWorld&) { return {}; }

Engagement Engage(const TargetingContext&, int32_t, int32_t, bool, int32_t, TargetingWorld&) { return {}; }
Engagement2 Engage2(const TargetingContext&, int32_t, int32_t, bool, TargetingWorld&) { return {}; }
bool BestKey(const TargetingContext&, int32_t, int32_t, int32_t&, int32_t&) { return false; }

Refusal OrderRefusal(const TargetingContext&, int32_t, int32_t, bool, TargetingWorld&) { return Refusal::None; }
bool AllowOrder(TargetState&, const TargetingContext&, const OrderView&, int32_t, TargetingWorld&) { return true; }
bool AllowTarget(TargetState&, const TargetingContext&, int32_t, int32_t, int32_t, int32_t, int32_t, TargetingWorld&) {
	return true;
}

void RunCommandGuns(TargetState&, const TargetingContext&, int32_t, TargetingWorld&) {}

} // namespace KT1::Core::Targeting
