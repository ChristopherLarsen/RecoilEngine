/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 Targeting: TargetingApi.h before step S19 (LUA-FREE H01): the system is not live, every answer is neutral.

#include "TargetingApi.h"

namespace KT1::Targeting {

bool Active() { return false; }

int32_t Target(int32_t, int32_t) { return -1; }

Core::Targeting::Engagement Engage(int32_t, int32_t, bool, int32_t) { return {}; }
Core::Targeting::Engagement2 Engage2(int32_t, int32_t, bool) { return {}; }
bool BestKey(int32_t, int32_t, int32_t&, int32_t&) { return false; }

int32_t Focus(int32_t) { return -1; }
void SetFocus(int32_t, int32_t) {}

float InFlight(int32_t) { return 0; }
bool Contact(int32_t, int32_t) { return false; }
Core::Targeting::Refusal ManualFireRefusal(int32_t, int32_t) { return Core::Targeting::Refusal::None; }

int32_t ScanSteps(float) { return 0; }
float ScanIntervalDefaultSeconds() { return 0; }
int32_t ScanExtraSteps() { return 0; }

int32_t CommandGunTarget(int32_t) { return -1; }
int32_t CommandGunNextScan(int32_t) { return -1; }

} // namespace KT1::Targeting
